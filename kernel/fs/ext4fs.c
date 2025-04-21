#include "types.h"
#include "vfs.h"
#include "ext4_errno.h"
#include "vfs.h"
#include "blockdev.h"
#include "ext4.h"

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