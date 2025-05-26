#include "types.h"
#include "vfs.h"
#include "ext4_errno.h"
#include "vfs.h"
#include "blockdev.h"
#include "ext4.h"
#include "proc.h"
#include "ext4fs.h"
#include "list.h"
#include "debug.h"
#include "ext4_inode.h"
#include "ext4_fs.h"
#include "ext4.h"
#include "dirent.h"
#include "stat.h"
#include "iovec.h"

void ext4_ilock(struct inode *ip);
int ext4_vfread(struct file *fp, int user_dst, uint64 dst, uint off, uint size, int *rcnt);
int ext4_vfopen(struct file *fp, const char *path, int flags);
int ext4_vwrite(struct file *fp, int user_src, uint64 src, uint off, uint size, int *wcnt);
int ext4_vmknod(const char *pathname, mode_t mode, dev_t dev);
int ext4_vcleansf(struct file *fp);
int ext4_vmkdir(const char *pathname, mode_t mode);
int ext4_vgetdents(struct file *fp, struct linux_dirent64 *dirp, int count);

struct {
    struct ext4_mfile fpool[NFILE];
    spinlock_t lock;
} ext4_fpool;

struct {
    struct ext4_minode ipool[NINODE];
    spinlock_t lock;
} ext4_ipool;

struct inode_ops ext4_inode_ops = {
    .ilock = ext4_ilock,
    .iunlock = generic_iunlock,
    .stati = generic_stati,

};

struct file_ops ext4_file_ops = {
    .read = ext4_vfread,
    .write = ext4_vwrite,
    .open = ext4_vfopen,
    .close = ext4_vfclose,
    .cleansf = ext4_vcleansf,
    .getdents = ext4_vgetdents,
    .writev = ext4_vwritev,
};

struct fs_ops ext4_fs_ops = {
    .mknod = ext4_vmknod,
    .mkdir = ext4_vmkdir,
    .fstat = ext4_vstat,
    .isdir = ext4_visdir,
};

struct vfs_filesystem ext4_fs = {
    .name = "ext4",
    .type = VFS_TYPE_EXT4,
    .iops = &ext4_inode_ops,
    .fops = &ext4_file_ops,
    .fsops = &ext4_fs_ops,
};


void init_ext4_fpool() {
    memset(&ext4_fpool, 0, sizeof(ext4_fpool));
    initlock(&ext4_fpool.lock, "ext4_fpool");
}

void init_ext4_ipool() {
    memset(&ext4_ipool, 0, sizeof(ext4_ipool));
    initlock(&ext4_ipool.lock, "ext4_ipool");
}

int ext4_init()
{
    const char *mount_point = "/";
    int r;

    struct ext4_blockdev *blockdev = ext4_blockdev_get();
    r = ext4_device_register(blockdev, "ext4_bd");
    if (r != EOK)
    {
        printf("[ext4] device register error! r=%d\n", r);
        return -1;
    }

    r = ext4_mount("ext4_bd", mount_point, false);
    if (r != EOK)
    {
        printf("[ext4] mount error! r=%d\n", r);
        return -1;
    }

    r = ext4_recover(mount_point);
    if (r != EOK && r != ENOTSUP)
    {
        printf("[ext4] recover error! r=%d\n", r);
        return -1;
    }

    r = ext4_journal_start(mount_point);
    if (r != EOK)
    {
        printf("[ext4] journal start error! r=%d\n", r);
        return -1;
    }

    r = ext4_cache_write_back(mount_point, true);
    if (r != EOK)
    {
        printf("[ext4] cache write back error! r=%d\n", r);
        return -1;
    }

    // printf("[ext4] ext4 filesystem initialized successfully!\n");
    return 0;
}

int init_ext4fs() {
    return register_fs(&ext4_fs);
}

void ext4_ipool_init() {
    memset(&ext4_ipool, 0, sizeof(ext4_ipool));
    initlock(&ext4_ipool.lock, "ext4_ipool");
}

struct ext4_file *ext4_falloc() {
    struct ext4_file *fp = NULL;
    acquire(&ext4_fpool.lock);
    for (int i = 0; i < NFILE; i++) {
        if (ext4_fpool.fpool[i].ref == 0) {
            ext4_fpool.fpool[i].ref++;
            fp = &ext4_fpool.fpool[i].ext4_fentry;
            break;
        }
    }
    release(&ext4_fpool.lock);
    return fp;
}

inline struct ext4_mfile *parent_mfile(struct ext4_file *efp) {
    return container_of(efp, struct ext4_mfile, ext4_fentry);
}
/**
 * @brief recycle ext4 file. check null pointer inside
 * 
 * @attention manage references of ext4 file, meaning that will recycle the fp * only when ref == 0
 * 
 * if ref > 0, just set ref--, and don't recycle the fp
 * @param efp pointer to ext4 file
 * @return return 0 on success, -1 on error
 */
int recycle_efile(struct ext4_file *efp) {
    if(!efp) 
        return 0;
    acquire(&ext4_fpool.lock);
    struct ext4_mfile *fp = parent_mfile(efp);
    if (fp->ref > 0) {
        fp->ref--;
    }
    if (fp->ref == 0) {
        memset(fp, 0, sizeof(struct ext4_mfile));
    }
    release(&ext4_fpool.lock);
    return 0;
}

struct ext4_inode *ext4_ialloc() {
    struct ext4_inode *ip = NULL;
    acquire(&ext4_ipool.lock);
    for (int i = 0; i < NINODE; i++) {
        if (ext4_ipool.ipool[i].ref == 0) {
            ext4_ipool.ipool[i].ref++;
            ip = &ext4_ipool.ipool[i].ext4_ientry;
            break;
        }
    }
    release(&ext4_ipool.lock);
    return ip;
}

inline struct ext4_minode *parent_minode(struct ext4_inode *eip) {
    return container_of(eip, struct ext4_minode, ext4_ientry);
}

int recycle_einode(struct ext4_inode *eip) {
    acquire(&ext4_ipool.lock);
    struct ext4_minode *ip = parent_minode(eip);
    if (ip->ref > 0) {
        ip->ref--;
    }
    if (ip->ref == 0) {
        memset(ip, 0, sizeof(struct ext4_minode));
    }
    release(&ext4_ipool.lock);
    return 0;
}

int ext4_vcleansf(struct file *fp) {
    struct ext4_file *efp = (ext4_file*) fp->private_data;
    return recycle_efile(efp);
}

struct inode *ext4_namei(char *rel_path) {
    struct inode *inode = NULL;
    struct ext4_inode *ext4_i = NULL;
    uint32_t ino;
    char abs_path[MAXPATH];
    char *cwd = myproc()->cinfo.path;
    get_absolute_path(rel_path, cwd, abs_path);
    if (abs_path == NULL) {
        printf("[ext4] get_absolute_path error!\n");
        return NULL;
    }
    if((ext4_i = ext4_ialloc()) == NULL) {
        printf("[ext4] ext4_ialloc error!\n");
        return NULL;
    }
    int r = ext4_raw_inode_fill(abs_path, &ino, ext4_i);
    if (r != EOK) {
        printf("[ext4] namei error! r=%d\n", r);
        if(recycle_einode(ext4_i) != 0) {
            printf("[ext4] recycle_einode error!\n");
        }
        return NULL;
    }
    inode = ifind_fempty(ext4_i);
    inode->i_ino = ino;

    return inode;

}

void ext4_ilock(struct inode *ip) {
    if(ip == 0 || ip->ref < 1)
        panic("ilock");
    acquiresleep(&ip->lock);
}

/**
 * @brief read from a file
 * 
 * @param fp file pointer
 * @param user_dst is 1 if dst is in user space, 0 if dst is in kernel space
 * @param dst destination address in user space or kernel space
 * @param off offset in the file to read from
 * @param size size to read
 * @param rcnt count of bytes read
 * @attention rcnt shouldn't be NULL !!!!!, it will be set to the number of bytes read
 * @return int 
 */
int ext4_vfread(struct file *fp, int user_dst, uint64 dst, uint off, uint size, int *rcnt) {
    int r = EOK;
    struct ext4_file *efp = fp->private_data;
    char *kbuf = NULL;
    if(!efp) {
        printf("[ext4] efp is NULL!\n");
        return EINVAL;
    }
    if((r = ext4_fseek(efp, off, SEEK_SET)) != EOK) {
        printf("[ext4] ext4_fseek error! r=%d\n", r);
        return r;
    }
    fp->fpos = efp->fpos;
    if(user_dst) {
        kbuf = (char *)kmalloc(size);
        if(!kbuf) {
            printf("[ext4] kmalloc error!\n");
            return ENOMEM;
        }
    } else {
        kbuf = (char *)dst;
    }
    #ifdef __DEBUG_EXT4_VFREAD
    Log("[ext4] ext4_vfread: kbuf = %p, dst = %p, size = %d", kbuf, dst, size);
    #endif
    if((r = ext4_fread(efp, kbuf, (size_t)size, (size_t*) rcnt)) != EOK) {
        printf("[ext4] ext4_fread error! r=%d\n", r);
        if(user_dst) {
            kfree(kbuf);
        }
        return r;
    }
    #ifdef __DEBUG_EXT4_VFREAD
    Log("[ext4] ext4_vfread: finish, kbuf = %p, dst = %p, size = %d", kbuf, dst, size);
    #endif
    if(user_dst) {
        if(copyout(myproc()->mm.pagetable, dst, kbuf, *rcnt) != EOK) {
            printf("[ext4] copyout error! r=%d\n", r);
            kfree(kbuf);
            return -1;
        }
        kfree(kbuf);
    }
    #ifdef __DEBUG_EXT4_VFREAD
    Log("[ext4] ext4_vfread: copyout finish, kbuf = %p, dst = %p, size = %d", kbuf, dst, size);
    #endif
    return r;
}

/**
 * @brief write to a file using iovec
 * 
 * @param fp file pointer
 * @param user_src if 1, address in iovec is in user space
 * @param iovec kernel space iovecs
 * @param iovcnt count of iovecs
 * @param wcnt write count
 * @attention iovec should in kernel space
 * @return 0 on success, -1 on error
 */
int ext4_vwritev(struct file *fp, int user_src, __kernel_space uint64 iovec, int iovcnt, int *wcnt) {
    int r = EOK;
    struct ext4_file *efp = fp->private_data;
    char *kvecs = (char*) iovec;
    int wc = 0;
    if(!efp) {
        printf("[ext4] efp is NULL!\n");
        return EINVAL;
    }
    // if(user_src) {
    //     kvecs = (char *)kmalloc(iovcnt * sizeof(struct iovec));
    //     if(!kvecs) {
    //         printf("[ext4] kmalloc error!\n");
    //         return ENOMEM;
    //     }
    //     if((r = copyin(myproc()->mm.pagetable, kvecs, iovec, iovcnt * sizeof(struct iovec))) != EOK) {
    //         printf("[ext4] copyin error! r=%d\n", r);
    //         kfree(kvecs);
    //         return r;
    //     }
    // } else {
    //     kvecs = (char *)iovec;
    // }
    struct iovec *vecs = (struct iovec *)kvecs;
    for (int i = 0; i < iovcnt; i++) {
        if (vecs[i].iov_len == 0) {
            continue;
        }
        if ((r = ext4_vwrite(fp, user_src, (uint64)vecs[i].iov_base, fp->fpos, vecs[i].iov_len, &wc)) != EOK) {
            printf("[ext4] ext4_vwrite error! r=%d\n", r);
            if(user_src) {
                kfree(kvecs);
            }
            return -1;
        }
        *wcnt += wc;
        vecs++;
    }
    return r;
    
}
int ext4_vwrite(struct file *fp, int user_src, uint64 src, uint off, uint size, int *wcnt) {
    int r = EOK;
    struct ext4_file *efp = fp->private_data;
    char *kbuf = NULL;
    if(!efp) {
        printf("[ext4] efp is NULL!\n");
        return EINVAL;
    }
    if((r = ext4_fseek(efp, off, SEEK_SET)) != EOK) {
        printf("[ext4] ext4_fseek error! r=%d\n", r);
        return r;
    }
    fp->fpos = efp->fpos;
    if(user_src) {
        kbuf = (char *)kmalloc(size);
        if(!kbuf) {
            printf("[ext4] kmalloc error!\n");
            return ENOMEM;
        }
        if((r = copyin(myproc()->mm.pagetable, kbuf, src, size)) != EOK) {
            printf("[ext4] copyin error! r=%d\n", r);
            kfree(kbuf);
            return r;
        }
    } else {
        kbuf = (char *)src;
    }
    if((r = ext4_fwrite(efp, kbuf, size, (size_t*)wcnt)) != EOK) {
        printf("[ext4] ext4_fwrite error! r=%d\n", r);
        if(user_src) {
            kfree(kbuf);
        }
        return r;
    }
    fp->fpos = efp->fpos;
    if(user_src) {
        kfree(kbuf);
    }
    return r;
}

int ext4_vfopen(struct file *fp, const char *path, int flags) {
    int r = EOK;
    struct ext4_file *efp = fp->private_data;
    struct ext4_inode inode;
    struct ext4_sblock *sb = NULL;
    uint32_t ino = 0;
    int eftype = 0;
    if((r = ext4_dir_open(&fp->dir, path)) == EOK) {
        goto isdir;
    }
    if(efp) {
        printf("[ext4] efp is not NULL!\n");
        return -EINVAL;
    }
    if((efp = ext4_falloc()) == NULL) {
        printf("[ext4] ext4_falloc error!\n");
        return -ENOMEM;
    }
    if((r = ext4_fopen2(efp, path, flags)) != EOK) {
        printf("[ext4] ext4_fopen2 error! r=%d\n", r);
        recycle_efile(efp);
        return -r;
    }
    fp->private_data = efp;

isdir:
    // to get the file type
    if(ext4_raw_inode_fill(path, &ino, &inode) != EOK) {
        printf("[ext4] ext4_raw_inode_fill error!\n");
        recycle_efile(efp);
        return -EINVAL;
    }

    if(ext4_get_sblock(path, &sb) != EOK) {
        printf("[ext4] ext4_get_sblock error!\n");
        recycle_efile(efp);
        return -EINVAL;
    }
    eftype = ext4_inode_type(sb, &inode);
    /*
    #define EXT4_INODE_MODE_FIFO 0x1000
    #define EXT4_INODE_MODE_CHARDEV 0x2000
    #define EXT4_INODE_MODE_DIRECTORY 0x4000
    #define EXT4_INODE_MODE_BLOCKDEV 0x6000
    #define EXT4_INODE_MODE_FILE 0x8000
    #define EXT4_INODE_MODE_SOFTLINK 0xA000
    #define EXT4_INODE_MODE_SOCKET 0xC000
    #define EXT4_INODE_MODE_TYPE_MASK 0xF000
    */
    switch(eftype) {
        case EXT4_INODE_MODE_FIFO:
            fp->type = FD_PIPE;
            break;
        case EXT4_INODE_MODE_CHARDEV:
            fp->type = FD_DEVICE;
            fp->major = ext4_inode_get_dev(&inode);
            break;
        case EXT4_INODE_MODE_DIRECTORY:
            fp->type = FD_DIR;
            break;
        case EXT4_INODE_MODE_BLOCKDEV:
            fp->type = FD_DEVICE;
            break;
        case EXT4_INODE_MODE_FILE:
            fp->type = FD_INODE;
            break;
        case EXT4_INODE_MODE_SOFTLINK:
            fp->type = FD_SOFTLINK;
            break;
        case EXT4_INODE_MODE_SOCKET:
            fp->type = FD_SOCKET;
            break;
        default:
            printf("[ext4] unknown file type! eftype=%d\n", eftype);
            recycle_efile(efp);
            return -EINVAL;
    }
        
    return r;
}

int ext4_vfclose(struct file *fp) {
    #ifdef __DEBUG_EXT4_VFCLOSE
    Log("[ext4] ext4_vfclose: fp = %p", fp);
    #endif
    int r = EOK;
    struct ext4_file *efp = fp->private_data;
    if(!efp) {
        printf("[ext4] efp is NULL!\n");
        return EINVAL;
    }
    if((r = ext4_fclose(efp)) != EOK) {
        printf("[ext4] ext4_fclose error! r=%d\n", r);
        return r;
    }
    recycle_efile(efp);
    fp->private_data = NULL;
    return r;
}

int ext4_vstat(char *path, struct kstat *st) {
    int r;
    char *stat_path;
    struct ext4_inode inode;
    uint32_t ino = 0;

    stat_path = path;

    /* Don't open file or dir, just get info from inode */
    r = ext4_raw_inode_fill(stat_path, &ino, &inode);
    if (r != EOK) {
        return r;
    }

    struct ext4_sblock *sb = NULL;
    r = ext4_get_sblock(stat_path, &sb);
    if (r != EOK) {
        return -r;
    }

    st->st_dev = ext4_inode_get_dev(&inode);
    st->st_ino = ino;
    st->st_mode = ext4_inode_get_mode(sb, &inode);
    st->st_nlink = ext4_inode_get_links_cnt(&inode);
    st->st_uid = ext4_inode_get_uid(&inode);
    st->st_gid = ext4_inode_get_gid(&inode);
    st->st_rdev = 0;
    st->st_size = (off_t) inode.size_lo;
    st->st_atime_sec = 0;
    st->st_atime_nsec = 0;
    st->st_mtime_sec = 0;
    st->st_mtime_nsec = 0;
    st->st_ctime_sec = 0;
    st->st_ctime_nsec = 0;

    if (r == 0) {
        struct ext4_mount_stats s;
        r = ext4_mount_point_stats(stat_path, &s);
        if (r == 0) {
            st->st_blksize = s.block_size;
            st->st_blocks = (st->st_size + s.block_size) / s.block_size;
        }
    }

    return r;
}

int ext4_vfstat(struct file *f, struct kstat *st) {
    struct ext4_file *file = (struct ext4_file *) f->private_data;
    if (file == NULL) {
        panic("can't get file");
    }
    struct ext4_inode_ref ref;

    int r = ext4_fs_get_inode_ref(&file->mp->fs, file->inode, &ref);
    if (r != EOK) {
        return -r;
    }

    st->st_dev = 0;
    st->st_ino = ref.index;
    st->st_mode = ref.inode->mode;
    st->st_nlink = ref.inode->size_lo;
    st->st_uid = 0;
    st->st_gid = 0;
    st->st_rdev = 0;
    st->st_size = ref.inode->size_lo;
    st->st_blksize = ref.inode->size_lo / ref.inode->blocks_count_lo;
    st->st_blocks = (uint64) ref.inode->blocks_count_lo;

    st->st_atime_sec = ext4_inode_get_access_time(ref.inode);
    st->st_ctime_sec = ext4_inode_get_change_inode_time(ref.inode);
    st->st_mtime_sec = ext4_inode_get_modif_time(ref.inode);


    return EOK;
}

/**
 * @brief Convert mode to ext4 type.
 * 
 * @param mode given mode
 * @return ext4 filetype
 * @note  *  ext4 Directory entry types.
 *  enum { EXT4_DE_UNKNOWN = 0, 
 *  EXT4_DE_REG_FILE,
 *  EXT4_DE_DIR,
 *  EXT4_DE_CHRDEV,
 *  EXT4_DE_BLKDEV,
 *  EXT4_DE_FIFO,
 *  EXT4_DE_SOCK,
 *  EXT4_DE_SYMLINK };
 */
static int mode2ext4type(mode_t mode) {

    int filetype = EXT4_DE_UNKNOWN;
    if (S_ISREG(mode)) {
        filetype = EXT4_DE_REG_FILE;
    } else if (S_ISDIR(mode)) {
        filetype = EXT4_DE_DIR;
    } else if (S_ISCHR(mode)) {
        filetype = EXT4_DE_CHRDEV;
    } else if (S_ISBLK(mode)) {
        filetype = EXT4_DE_BLKDEV;
    } else if (S_ISFIFO(mode)) {
        filetype = EXT4_DE_FIFO;
    } else if (S_ISSOCK(mode)) {
        filetype = EXT4_DE_SOCK;
    } else if (S_ISLNK(mode)) {
        filetype = EXT4_DE_SYMLINK;
    }
    return filetype;
}

/**
 * @brief Create a file with the given mode and device.
 * 
 * @param pathname path to the file
 * @param mode file mode
 * @param dev device id
 * @return int 0 on success, error code on error
 */
int ext4_vmknod(const char *pathname, mode_t mode, dev_t dev) {
    int filetype;
    filetype = mode2ext4type(mode);
    if(filetype == EXT4_DE_UNKNOWN) {
        printf("[ext4] ext4_vmknod error! filetype=%d\n", filetype);
        return -EINVAL;
    }
    if(ext4_mknod(pathname, filetype, dev) != EOK) {
        printf("[ext4] ext4_mknod error!\n");
        return -EINVAL;
    }
    return EOK;
}

int ext4_vmkdir(const char *pathname, mode_t mode) {
    int r = ext4_dir_mk(pathname);
    if (r != EOK) {
        printf("[ext4] ext4_vmkdir error! r=%d\n", r);
        return r;
    }
    r = ext4_mode_set(pathname, mode);
    if (r != EOK) {
        printf("[ext4] ext4_mode_set error! r=%d\n", r);
        return r;
    }
    return r;
}


/**
 * @brief temporary function to get directory entries when cannot allocator a large kernel buffer
 * 
 * @attention copy data to user buffer directly, reuse a kernel buffer
 * @param fp file pointer
 * @param u_dirp pointer to the user space buffer
 * @param count size of the buffer
 * @return read bytes on success when quit in the loop, -1 on error, 0 on no more entries
 */
int ext4_temp_vgentdents(struct file *fp, __user_space struct linux_dirent64 *u_dirp, int count) {
    struct ext4_dir *dir = &fp->dir;
    struct linux_dirent64 *d;
    struct linux_dirent64 *dirp;
    uint64 off = 0;
    const ext4_direntry *de;
    int totlen = 0;
    int reclen = 0;
    if(count <= 0) {
        printf("[ext4] count <= 0!\n");
        return 0;
    }
    if(fp->type != FD_DIR) {
        printf("[ext4] fp->type != FD_DIR!, == %d\n", fp->type);
        return 0;
    }
    dirp = kalloc();
    if(dirp == NULL) {
        printf("[ext4] dirp is NULL!\n");
        return 0;
    }

    d = dirp;
    while(1) {
        de = ext4_dir_entry_next(dir);
        if(de == NULL) {
            break;
        }
        if(de->entry_length <= 0) {
            printf("[ext4] de->rec_len <= 0!\n");
            return -1;
        }
        reclen = sizeof(struct linux_dirent64) + de->name_length + 1;
        if(totlen + reclen >= count) {
            break;
        }
        strncpy(d->d_name, (const char*)de->name, de->name_length);
        d->d_name[de->name_length] = '\0';
        d->d_reclen = reclen;
        d->d_ino = de->inode;
        d->d_off = off; //
        off = dir->next_off;
        /**@brief   Directory entry types. */
        /*
        enum { EXT4_DE_UNKNOWN = 0,
            EXT4_DE_REG_FILE,
            EXT4_DE_DIR,
            EXT4_DE_CHRDEV,
            EXT4_DE_BLKDEV,
            EXT4_DE_FIFO,
            EXT4_DE_SOCK,
            EXT4_DE_SYMLINK };
        */        
        switch(de->inode_type) {
            case EXT4_DE_UNKNOWN:
                d->d_type = T_UNKNOWN;
                break;
            case EXT4_DE_REG_FILE:
                d->d_type = T_REG;
                break;
            case EXT4_DE_DIR:
                d->d_type = T_DIR;
                break;
            case EXT4_DE_CHRDEV:
                d->d_type = T_CHR;
                break;
            case EXT4_DE_BLKDEV:
                d->d_type = T_BLK;
                break;
            case EXT4_DE_FIFO:
                d->d_type = T_FIFO;
                break;
            case EXT4_DE_SOCK:
                d->d_type = T_SOCK;
                break;
            case EXT4_DE_SYMLINK:
                d->d_type = T_LNK;
                break;
            default:
                d->d_type = T_UNKNOWN;
                break;
        }
        if(copyout(myproc()->mm.pagetable, (uint64)u_dirp, (char*)dirp, reclen) != EOK) {
            printf("[ext4] copyout error!\n");
            kfree(dirp);
            return totlen;
        }
        totlen += reclen;
        u_dirp = (struct linux_dirent64 *)((char *)u_dirp + reclen);
    }
    kfree(dirp);
#ifdef __DEBUG_EXT4_VGENTDENTS
    Log("[ext4] ext4_temp_vgentdents: totlen=%d, path = %s\n", totlen, fp->info.path);
#endif
    return totlen;
}
/**
 * @brief get directory entries
 * 
 * @param fp file pointer
 * @param dirp buffer to store directory entries
 * @param count buffer size
 * @return number of bytes read on success, -1 on error
 */
int ext4_vgetdents(struct file *fp, __kernel_space struct linux_dirent64 *dirp, int count) {
    struct ext4_dir dir = fp->dir;
    struct linux_dirent64 *d;
    const ext4_direntry *de;
    int totlen = 0;
    int reclen = 0;
    int index = 1;
    if(count <= 0) {
        printf("[ext4] count <= 0!\n");
        return -1;
    }
    if(dirp == NULL) {
        printf("[ext4] dirp is NULL!\n");
        return -1;
    }

    d = dirp;
    while(1) {
        de = ext4_dir_entry_next(&dir);
        if(de->inode == 0) {
            continue;
        }
        if(de == NULL) {
            break;
        }
        if(de->entry_length <= 0) {
            printf("[ext4] de->rec_len <= 0!\n");
            return -1;
        }
        reclen = sizeof(struct linux_dirent64) + de->name_length + 1;
        if(totlen + reclen > count) {
            break;
        }
        strncpy(d->d_name, (const char*)de->name, de->name_length);
        d->d_name[de->name_length] = '\0';
        d->d_reclen = reclen;
        d->d_ino = de->inode;
        d->d_off = index++;
        /**@brief   Directory entry types. */
        /*
        enum { EXT4_DE_UNKNOWN = 0,
            EXT4_DE_REG_FILE,
            EXT4_DE_DIR,
            EXT4_DE_CHRDEV,
            EXT4_DE_BLKDEV,
            EXT4_DE_FIFO,
            EXT4_DE_SOCK,
            EXT4_DE_SYMLINK };
        */        
        switch(de->inode_type) {
            case EXT4_DE_UNKNOWN:
                d->d_type = T_UNKNOWN;
                break;
            case EXT4_DE_REG_FILE:
                d->d_type = T_REG;
                break;
            case EXT4_DE_DIR:
                d->d_type = T_DIR;
                break;
            case EXT4_DE_CHRDEV:
                d->d_type = T_CHR;
                break;
            case EXT4_DE_BLKDEV:
                d->d_type = T_BLK;
                break;
            case EXT4_DE_FIFO:
                d->d_type = T_FIFO;
                break;
            case EXT4_DE_SOCK:
                d->d_type = T_SOCK;
                break;
            case EXT4_DE_SYMLINK:
                d->d_type = T_LNK;
                break;
            default:
                d->d_type = T_UNKNOWN;
                break;
        }
        totlen += reclen;
        d = (struct linux_dirent64 *)((char *)d + reclen);
    }
    return totlen;
}

/**
 * @brief if the path is a directory?
 * 
 * @param path given path
 * @return 1 if it is a directory, 0 if not
 */
int ext4_visdir(const char *path) {
    struct ext4_dir dir;
    int r = EOK;

    if((r = ext4_dir_open(&dir, path)) != EOK) {
        printf("[ext4] ext4_visdir error! r=%d\n", r);
        return 0;
    }
    if((r = ext4_dir_close(&dir)) != EOK) {
        printf("[ext4] ext4_dir_close error! r=%d\n", r);
        return 0;
    }
    return 1;
}