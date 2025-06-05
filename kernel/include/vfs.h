#ifndef __VFS_H
#define __VFS_H

#include "types.h"
#include "sleeplock.h"
#include "file.h"
#include "buf.h"
#include "stat.h"
#include "param.h"
#include "list.h"
#include "types.h"
#include "dirent.h"

#ifndef VFS_MAXFS
#define VFS_MAXFS 4
#endif

#define VFS_INODE_MAX 60
#define MAX_MOUNTS 4

#define I_BUSY 0x1
#define I_VALID 0x2

#define SB_LOADED 1
#define SB_UNLOADED 0

struct cwdinfo {
    char path[MAXPATH]; // current working directory
    struct vfs_filesystem *fs;
    void *pdata; // private data for the filesystem
};

struct icache_t {
    // synchronize access for multiple processes
    struct spinlock lock;
    // active inodes
    struct inode inode[NINODE];
};

typedef enum vfs_type {
    VFS_TYPE_UNKNOWN = 0,
    VFS_TYPE_EXT4,
    VFS_TYPE_XV6FS,
} vfs_type_t;

/*
 * This is struct is the map block device and its filesystem.
 * Its main job is return the filesystem type of current (major, minor)
 * mounted device. It is used when it is not possible retrive the
 * vfs_filesystem from the inode.
 */
struct vfs {
    int major;
    int minor;
    int flag;
    struct vfs_filesystem *fs_t;
    struct list_head fs_next; // Next mounted on vfs
  };
#define VFS_FREE 0
#define VFS_USED 1

  
struct mount_point {
    char *mp; // Mount point 
    int dev; // Device number
    vfs_type_t type; // Filesystem type
};

struct  vfs_filesystem {
    int dev; // Device number
    char *name; // Filesystem name
    vfs_type_t type; // Filesystem type

    struct inode_ops *iops; // Inode operations
    struct file_ops *fops; // File operations
    struct fs_ops *fsops; // Filesystem operations

    void *fs_data; // Filesystem-specific data, e.g. xv6fs_superblock
    struct list_head fs_list; // List of mounted filesystems
};

struct dirent {
    ushort inum;
    char name[DIRSIZ];
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
    struct inode*       (*dirlookup)(struct inode *dp, char *name, uint *off);
    void                (*iupdate)(struct inode *ip);
    void                (*itrunc)(struct inode *ip);
    void                (*cleanup)(struct inode *ip);
    uint                (*bmap)(struct inode *ip, uint bn);
    void                (*ilock)(struct inode* ip);
    void                (*iunlock)(struct inode* ip);
    void                (*stati)(struct inode *ip, struct stat *st);
    int                 (*readi)(struct inode *ip, int user_dst, uint64 dst, uint off, uint n);
    int                 (*writei)(struct inode *ip,int user_src, uint64 src, uint off, uint n);
    int                 (*dirlink)(struct inode *dp, char *name, uint inum);
    int                 (*unlink)(struct inode *dp, uint off);
    int                 (*isdirempty)(struct inode *dp);
};

struct file_ops {

    int             (*open)(struct file *f, const char *path, int flags);
    int             (*close)(struct file *f);
    int             (*read)(struct file *fp, int user_dst, uint64 dst, uint off, uint size, int *rcnt);
    int             (*write)(struct file *fp, int user_src, uint64 src, uint off, uint size, int *wcnt);
    int             (*filestat)(struct file *f, uint64 addr);
    int             (*cleansf)(struct file* f);
    int (*getdents)(struct file *fp, struct linux_dirent64 *dirp, int count);
    int (*writev)(struct file *fp, int user_src, uint64 iovec, int iovcnt, int *wcnt);
    int (*lseek)(struct file *fp, off_t offset, int whence);
};

struct fs_ops {
    int             (*fs_init) (void);
    int             (*mount) (struct inode *devi, struct inode *ip);
    int             (*unmount) (struct inode *devi);
    struct inode*   (*getroot)(int major, int minor);
    void            (*readsb)(int dev, struct superblock *sb);
    struct inode*   (*ialloc)(uint dev, short type);
    uint            (*balloc)(uint dev);
    void            (*bzero)(int dev, int bno);
    void            (*bfree)(int dev, uint b);
    void            (*brelse)(struct buf *b);
    void            (*bwrite)(struct buf *b);
    struct buf*     (*bread)(uint dev, uint blockno);
    int             (*namecmp)(const char *s, const char *t);
    int             (*mknod)(const char *pathname, mode_t mode, dev_t dev);
    int             (*mkdir)(const char *pathname, mode_t mode);
    int (*fstat)(char *path, struct kstat *kst);
    int (*isdir)(const char *path);
    int (*link) (const char *oldpath, const char *newpath, int flags);
    int (*unlink)(const char *path, int flags);
    int (*faccess)(char *path, int amode, int flags);
    int (*utimens)(const char *path, const struct timespec times[2]);
    int (*file_exist)(const char *path);
};

struct vfs_filesystem *vfs_getfs_bytype(vfs_type_t type);
struct vfs_filesystem *vfs_getfs_bydev(int dev);
struct vfs_filesystem *vfs_getfs_byname(const char *name);
struct vfs_filesystem * vfs_resolve_fs(const char* path);

void generic_iunlock(struct inode *ip);
void generic_stati(struct inode *ip, struct stat *st);
int generic_dirlink(struct inode *dp, char *name, uint inum);
int generic_readi(struct inode *ip, char *dst, uint off, uint n);
struct vfs_filesystem* getfs(const char *fs_name);
int register_fs(struct vfs_filesystem *fs);
struct inode* iget(uint dev, uint inum, int (*fill_inode)(struct inode *));
int put_vfs_on_list(int major, int minor, struct vfs_filesystem *fs_t);
int sb_set_blocksize(struct superblock *sb, int size);
void fsinit(int dev);
void iput(struct inode *ip);
void iunlockput(struct inode *ip);
void iinit();
void install_rootfs(void);
void init_vfssw(void);
void init_vfsmlist(void);

void get_absolute_path(const char *path, const char *cwd, char *absolute_path);
struct inode *ifind_fempty(void *pdata);
void init_vfs_mtable();
int vfs_mount(struct vfs_filesystem *fs, char *path);
int addfs(struct vfs_filesystem *fs);

#endif