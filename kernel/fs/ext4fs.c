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
void ext4_ilock(struct inode *ip);
int ext4_vfread(struct file *fp, int user_dst, uint64 dst, uint off, uint size, int *rcnt);
int ext4_vfopen(struct file *fp, const char *path, uint32_t flags);

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

};

struct fs_ops ext4_fs_ops = {
};

struct vfs_filesystem ext4_fs = {
    .name = "ext4",
    .type = VFS_TYPE_EXT4,
    .iops = &ext4_inode_ops,
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

    printf("[ext4] ext4 filesystem initialized successfully!\n");
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
int recycle_efile(struct ext4_file *efp) {
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
    if(user_dst) {
        kbuf = (char *)kmalloc(size);
        if(!kbuf) {
            printf("[ext4] kmalloc error!\n");
            return ENOMEM;
        }
    } else {
        kbuf = (char *)dst;
    }
    if((r = ext4_fread(efp, kbuf, size, (size_t*) rcnt)) != EOK) {
        printf("[ext4] ext4_fread error! r=%d\n", r);
        if(user_dst) {
            kfree(kbuf);
        }
        return r;
    }
    if(user_dst) {
        if((r = copyout(myproc()->mm.pagetable, dst, kbuf, *rcnt)) != EOK) {
            printf("[ext4] copyout error! r=%d\n", r);
            kfree(kbuf);
            return r;
        }
        kfree(kbuf);
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
    if(user_src) {
        kfree(kbuf);
    }
    return r;
}

int ext4_vfopen(struct file *fp, const char *path, uint32_t flags) {
    int r = EOK;
    struct ext4_file *efp = fp->private_data;
    if(efp) {
        printf("[ext4] efp is not NULL!\n");
        return EINVAL;
    }
    if((efp = ext4_falloc()) == NULL) {
        printf("[ext4] ext4_falloc error!\n");
        return ENOMEM;
    }
    if((r = ext4_fopen2(efp, path, flags)) != EOK) {
        printf("[ext4] ext4_fopen2 error! r=%d\n", r);
        recycle_efile(efp);
        return r;
    }
    fp->private_data = efp;
    return r;
}

int ext4_vfclose(struct file *fp) {
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
        return -r;
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

    return -r;
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