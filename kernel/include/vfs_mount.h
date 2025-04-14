// This file implements the Mount table and utilities functions
#ifndef xv6fs_VFSMOUNT_H_
#define xv6fs_VFSMOUNT_H_
#include "param.h"
#include "vfs.h"
#include "file.h"
#include "spinlock.h"


#define M_USED 0x1
#define MOUNTSIZE     2   // size of mounted devices

// Mount Table Entry
struct mntentry {
  struct inode *m_inode;
  struct inode *m_rtinode; // Root inode for device
  void *pdata;             // Private date of mountentry. Almost is a superblock
  int dev;                 // Mounted device
  int flags;                // Flags
};

// Mount Table Structure
struct {
    struct spinlock lock;
    struct mntentry mpoint[MOUNTSIZE];
  } mtable;
  
// Utility functions

struct inode* mnt_rtinode(struct inode * ip);
struct inode* mnt_inode(struct inode * ip);
int is_rtinode(struct inode* ip);
void mntinit(void);

#endif /* xv6fs_VFSMOUNT_H_ */


