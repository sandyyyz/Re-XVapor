#include "types.h"
#include "param.h"
#include "defs.h"
#include "spinlock.h"
#include "vfs.h"
#include "file.h"
#include "vfs_mount.h"

/**
 * @brief get the mounted root inode for the given inode
 * @param given inode
 * @return mounted root inode
 */
struct inode * mnt_rtinode(struct inode * ip)
{
  struct inode *rtinode;
  struct mntentry *mp;

  acquire(&mtable.lock);
  for (mp = &mtable.mpoint[0]; mp < &mtable.mpoint[MOUNTSIZE]; mp++) {
    if (mp->m_inode->dev == ip->dev && mp->m_inode->inum == ip->inum) {
      rtinode = mp->m_rtinode;
      release(&mtable.lock);

      return rtinode;
    }
  }
  release(&mtable.lock);

  return 0;
}

/**
 * @brief get the mounted inode for the given inode
 * @param given inode
 * @return mounted inode
 */
struct inode * mnt_inode(struct inode * ip)
{
  struct inode *mntinode;
  struct mntentry *mp;

  acquire(&mtable.lock);
  for (mp = &mtable.mpoint[0]; mp < &mtable.mpoint[MOUNTSIZE]; mp++) {
    if (mp->m_rtinode->dev == ip->dev && mp->m_rtinode->inum == ip->inum) {
      mntinode = mp->m_inode;
      release(&mtable.lock);

      return mntinode;
    }
  }
  release(&mtable.lock);

  return 0;
}

/**
 * @brief is the given inode a mounted root inode
 * 
 * @param ip 
 * @return 1 if it is a mounted root inode, 0 otherwise
 */
int is_rtinode(struct inode* ip)
{
  struct mntentry *mp;

  acquire(&mtable.lock);
  for (mp = &mtable.mpoint[0]; mp < &mtable.mpoint[MOUNTSIZE]; mp++) {
    if (mp->m_rtinode->dev == ip->dev && mp->m_rtinode->inum == ip->inum) {
      release(&mtable.lock);
      return 1;
    }
  }
  release(&mtable.lock);

  return 0;
}

void mntinit(void)
{
  initlock(&mtable.lock, "mtable");
}
