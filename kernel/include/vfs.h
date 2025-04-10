#ifndef __VFS_H
#define __VFS_H

#include "types.h"
#include "sleeplock.h"
#include "file.h"

#ifndef VFS_MAXFS
#define VFS_MAXFS 4
#endif

#define VFS_INODE_MAX 60
#define MAX_MOUNTS 4

typedef enum vfs_type {
    VFS_TYPE_UNKNOWN = 0,
    VFS_TYPE_EXT4,
    VFS_TYPE_XV6FS,
} vfs_type_t;

struct mount_point {
    char *mp; // Mount point
    int dev; // Device number
    vfs_type_t type; // Filesystem type
};

struct vfs_filesystem {
    int dev; // Device number
    vfs_type_t type; // Filesystem type
    char *name; // Filesystem name
    struct fs_ops *fsops; // Filesystem operations
    void *fs_data; // Filesystem-specific data, e.g. superblock
};

// struct inode {
//     struct inode_ops *iops; // Inode operations
//     void *private_data; // Private data for the inode, may point to a filesystem-specific structure like inode
//     int ref; // Reference count for the inode
//     int mode; // Mode of the inode (e.g., file, directory)
//     int flags; // Flags for the inode (e.g., dirty, locked)
//     off_t size; // Size of the inode
//     struct sleeplock lock; // Lock for the inode
// };

// struct file {
//     enum { FD_NONE, FD_PIPE, FD_INODE, FD_DEVICE } type; // Type of file
//     struct pipe *pipe; // Pointer to the pipe structure (if applicable)
//     struct inode *ip; // Pointer to the inode structure
//     struct file_ops *fops; // File operations
//     int ref; // Reference count for the file
//     int flags; // Flags for the file (e.g., read, write)
//     off_t offset; // Current offset in the file
//     short major; // Major device number
// };

struct inode_ops {
    int (*readi)(struct inode *ip, int user_dst, uint64 dst, uint off, uint n);
    int (*writei)(struct inode *ip, int user_src, uint64 src, uint off, uint n);
    void (*ilock)(struct inode *ip);
    void (*iupdate)(struct inode *ip);
    void (*iput)(struct inode *ip);
    void (*iunlockput)(struct inode *ip);
    struct inode* (*idup)(struct inode *ip); 
    struct inode* (*iget)(int dev, int inum);
};

struct file_ops {
    struct file* (*dup)(struct file *f);
    int (*open)(const char *path, int flags);
    void (*close)(struct file *f);
    int (*read)(struct file *f, void *buf, int count);
    int (*write)(struct file *f, const void *buf, int count);
    int (*filestat)(struct file *f, uint64 addr);

};

struct fs_ops {
    int (*mount)(const char *path, const char *fs_type, const char *options);
    int (*unmount)(const char *path);
    // int (*statfs)(const char *path, struct statfs *buf);
    // int (*sync)(void);
    // should be here?..
    struct vfs_node* (*vfs_namei)(char *path);
};

struct vfs_filesystem *vfs_getfs_bytype(vfs_type_t type);
struct vfs_filesystem *vfs_getfs_bydev(int dev);
struct vfs_filesystem *vfs_getfs_byname(const char *name);
struct vfs_filesystem *vfs_getfs_bypath(const char *path);
struct inode* vfs_namei(char *path);

struct file* vfile_alloc(void);

#endif