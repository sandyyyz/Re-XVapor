#ifndef __XV6FS_H
#define __XV6FS_H

// On-disk file system format.
// Both the kernel and user programs use this header file.

#include "types.h"

#define ROOTINO  1   // root i-number
#define BSIZE 1024  // block size

// Disk layout:
// [ boot block | super block | log | inode blocks |
//                                          free bit map | data blocks]
//
// mkfs computes the super block and builds an initial file system. The
// super block describes the disk layout:
struct xv6fs_superblock {
  uint magic;        // Must be FSMAGIC
  uint size;         // Size of file system image (blocks)
  uint nblocks;      // Number of data blocks
  uint ninodes;      // Number of inodes.
  uint nlog;         // Number of log blocks
  uint logstart;     // Block number of first log block
  uint inodestart;   // Block number of first inode block
  uint bmapstart;    // Block number of first free map block

  int flags;
};

#define XV6FS_SB_FREE 0
#define XV6FS_SB_USED 1

#define FSMAGIC 0x10203040
 
#define NDIRECT 12
#define NINDIRECT (BSIZE / sizeof(uint))
#define MAXFILE (NDIRECT + NINDIRECT)

// On-disk inode structure
struct xv6fs_dinode {
  short type;           // File type
  short major;          // Major device number (T_DEVICE only)
  short minor;          // Minor device number (T_DEVICE only)
  short nlink;          // Number of links to inode in file system
  uint size;            // Size of file (bytes)
  uint addrs[NDIRECT+1];   // Data block addresses
};

// In-memory copy of an xv6fs_dinode
struct xv6fs_inode {
  short type;
  short major;
  short minor;
  short nlink;
  uint size;
  int flags;
  uint addrs[NDIRECT+1];
};

#define XV6FS_INODE_FREE 0
#define XV6FS_INODE_USED 1

// Inodes per block.
#define IPB           (BSIZE / sizeof(struct xv6fs_dinode))

// Block containing inode i
// 将inode号转化为磁盘块号
// 1. i/IPB得到inode所在的inode块号
// 2. +sb.inodestart得到所在磁盘块号
#define IBLOCK(i, sb)     ((i) / IPB + sb.inodestart)

// Bitmap bits per block
// 1byte = 8bits, so 8*BSIZE
#define BPB           (BSIZE*8)

// Block of free map containing bit for block b
#define BBLOCK(b, sb) ((b)/BPB + sb.bmapstart)

// Directory is a file containing a sequence of dirent structures.
#define DIRSIZ 14

#endif