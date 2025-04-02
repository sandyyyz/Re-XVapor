#ifndef __FILE_H
#define __FILE_H

#include "types.h"
#include "sleeplock.h"
#include "fs.h"

#define READABLE 0X1
#define WRITABLE 0X2
#define IS_READABLE(x) (((x) & READABLE) ? 1 : 0)
#define IS_WRITABLE(x) (((x) & WRITABLE) ? 1 : 0)
#define IS_READABLE_WRITABLE(x) (((x) & (READABLE | WRITABLE)) ? 1 : 0)

#define SET_READABLE(x) ((x) |= READABLE)
#define SET_WRITABLE(x) ((x) |= WRITABLE)
#define SET_READABLE_WRITABLE(x) ((x) |= (READABLE | WRITABLE))
#define UNSET_READABLE(x) ((x) &= ~READABLE)
#define UNSET_WRITABLE(x) ((x) &= ~WRITABLE)
#define UNSET_READABLE_WRITABLE(x) ((x) &= ~(READABLE | WRITABLE))
struct file {
  enum { FD_NONE, FD_PIPE, FD_INODE, FD_DEVICE } type;
  int ref; // reference count
  // char readable;
  // char writable;
  int flags;
  struct pipe *pipe; // FD_PIPE
  struct inode *ip;  // FD_INODE and FD_DEVICE
  uint off;          // FD_INODE
  short major;       // FD_DEVICE
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
  uint addrs[NDIRECT+1];
};

// map major device number to device functions.
struct devsw {
  int (*read)(int, uint64, int);
  int (*write)(int, uint64, int);
};

extern struct devsw devsw[];

#define CONSOLE 1

#endif