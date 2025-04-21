#include "types.h"
#include "vfs.h"
#include "ext4_errno.h"
#include "vfs.h"
#include "blockdev.h"
#include "ext4.h"
#include "proc.h"
#include "ext4fs.h"
#include "list.h"

struct {
    struct ext4_minode ipool[NINODE];
    spinlock_t lock;
} ext4_ipool;

struct vfs_filesystem ext4_fs = {
    .name = "ext4",
    .type = VFS_TYPE_EXT4,
};



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

struct ext4_minode inline *parent_minode(struct ext4_inode *eip) {
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