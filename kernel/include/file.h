#ifndef __FILE_H
#define __FILE_H

#include "types.h"
#include "sleeplock.h"
#include "xv6fs.h"
#include "param.h"
#include "ext4.h"
#include "fcntl.h"

#define O_ACCMODE 03

// 低两位用于访问模式：00 只读，01 只写，10 读写，11 保留
#define IS_READABLE(flags)  (((flags) & O_ACCMODE) != O_WRONLY)
#define IS_WRITABLE(flags)  (((flags) & O_ACCMODE) != O_RDONLY)

// 设置为可读（保持原写标志）
#define SET_READABLE(flags) \
    do { \
        int mode = (flags) & O_ACCMODE; \
        if (mode == O_WRONLY) \
            (flags) = ((flags) & ~O_ACCMODE) | O_RDWR; \
        else if (mode == 0) \
            (flags) = ((flags) & ~O_ACCMODE) | O_RDONLY; \
    } while (0)

// 设置为可写（保持原读标志）
#define SET_WRITABLE(flags) \
    do { \
        int mode = (flags) & O_ACCMODE; \
        if (mode == O_RDONLY) \
            (flags) = ((flags) & ~O_ACCMODE) | O_RDWR; \
        else if (mode == 0) \
            (flags) = ((flags) & ~O_ACCMODE) | O_WRONLY; \
    } while (0)

// 清除可读（保留写标志）
#define UNSET_READABLE(flags) \
    do { \
        int mode = (flags) & O_ACCMODE; \
        if (mode == O_RDONLY) \
            (flags) = ((flags) & ~O_ACCMODE); /* 变成无访问权限 */ \
        else if (mode == O_RDWR) \
            (flags) = ((flags) & ~O_ACCMODE) | O_WRONLY; \
    } while (0)

// 清除可写（保留读标志）
#define UNSET_WRITABLE(flags) \
    do { \
        int mode = (flags) & O_ACCMODE; \
        if (mode == O_WRONLY) \
            (flags) = ((flags) & ~O_ACCMODE); /* 变成无访问权限 */ \
        else if (mode == O_RDWR) \
            (flags) = ((flags) & ~O_ACCMODE) | O_RDONLY; \
    } while (0)
    
struct file_info {
  char path[MAXPATH]; // full path of the file
  struct vfs_filesystem *fs; // file system
  void *data; // file-specific data
};

struct file {
  enum { FD_NONE, FD_PIPE, FD_INODE, FD_DEVICE, FD_SOFTLINK, FD_SOCKET, FD_DIR} type;
  int ref; // reference count
  // char readable;
  // char writable;
  int flags;
  int omode;
  struct pipe *pipe; // FD_PIPE
  struct inode *ip;  // FD_INODE and FD_DEVICE
  uint off;          // FD_INODE
  short major;       // FD_DEVICE

  // support vfs
  struct file_ops *fops; // file operations
  void *private_data; // filesystem-specific data; just used for ext4 right now. maybe a ext4_file or ext4_dir
  struct file_info info;
  uint64 fpos; // file position
  ext4_dir dir; // directory entry. just used for ext4, and don't use a pointer because of lacking a small memory allocator in kernel right now
  int removed; // if the file is removed, this will be set to 1, and the file will be removed when the reference count reaches 0
  
};

#define major(dev)  ((dev) >> 16 & 0xFFFF)
#define minor(dev)  ((dev) & 0xFFFF)
#define	mkdev(m,n)  ((uint)((m)<<16| (n)))

// in-memory copy of an inode
struct inode {
  uint dev;           // Device number
  uint inum;          // Inode number
  int ref;            // Reference count
  struct sleeplock lock; // protects everything below here
  int valid;          // inode has been read from disk?

  short type;         // copy of disk inode
  short major;
  short minor;
  short nlink;
  uint size;

  // support vfs
  struct vfs_filesystem *fs; // Filesystem
  struct inode_ops *iops; // Inode operations
  void *i_private; // private data for the inode

  int i_ino; // inode number of ext4 inode 
};


struct superblock {
  int major;        // Driver major number from it superblocks is stored in.
  int minor;        // Driver major number from it superblocks is stored in.
  uint blocksize;  // Block size of this superblock
  void *fs_info;    // Filesystem-specific info
  unsigned char s_blocksize_bits;

  int flags;       // Superblock Falgs to map its usage
};

// map major device number to device functions.
struct devsw {
  int (*read)(int user_dst, uint64 dst, int n);
  int (*write)(int user_src, uint64 src, int n);
};

extern struct devsw devsw[];

#define CONSOLE 1

#endif