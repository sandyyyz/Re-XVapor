#ifndef __VFS_H
#define __VFS_H

#include "types.h"

#ifndef VFS_MAXFS
#define VFS_MAXFS 4
#endif

typedef enum vfs_type {
    VFS_TYPE_UNKNOWN = 0,
    VFS_TYPE_EXT4,
    VFS_TYPE_XV6FS,
} vfs_type_t;

struct vfs_filesystem {
    int dev; // Device number
    vfs_type_t type; // Filesystem type
    struct file_ops *fops; // File operations
    char *mp; // Mount point
    void *fs_data; // Filesystem-specific data, e.g. superblock
};

struct vfs_inode {
    struct inode_ops *fops; // File operations
    void *private_data; // Private data for the inode, may point to a filesystem-specific structure like inode
    int ref; // Reference count for the inode
    int mode; // Mode of the inode (e.g., file, directory)
    int flags; // Flags for the inode (e.g., dirty, locked)
    off_t size; // Size of the inode
};

struct vfs_file {
    enum { FD_NONE, FD_PIPE, FD_INODE, FD_DEVICE } type; // Type of file
    struct pipe *pipe; // Pointer to the pipe structure (if applicable)
    struct vinode *vip; // Pointer to the inode structure
    struct file_ops *fops; // File operations
    int ref; // Reference count for the file
    int flags; // Flags for the file (e.g., read, write)
    off_t offset; // Current offset in the file
    short major; // Major device number
};

struct inode_ops {

};

struct file_ops {
    int (*open)(const char *path, int flags);
    int (*close)(int fd);
    int (*read)(int fd, void *buf, int count);
    int (*write)(int fd, const void *buf, int count);
};

#endif