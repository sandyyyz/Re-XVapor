#include "types.h"
#include "vfs.h"

extern struct file_ops ext4_fops;
extern struct file_ops xv6fs_fops;

struct vfs_filesystem *vfs_fs[VFS_MAXFS];
struct file_ops *vfs_fops[VFS_MAXFS] = {
    [VFS_TYPE_UNKNOWN] = NULL,
    [VFS_TYPE_EXT4] = &ext4_fops,
    [VFS_TYPE_XV6FS] = &xv6fs_fops,
};
