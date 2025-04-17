// File system implementation.  Five layers:
//   + Blocks: allocator for raw disk blocks.
//   + Log: crash recovery for multi-step updates.
//   + Files: inode allocator, reading, writing, metadata.
//   + Directories: inode with special contents (list of other inodes!)
//   + Names: paths like /usr/rtm/xv6fs/fs.c for convenient naming.
//
// This file contains the low-level file system manipulation
// routines.  The (higher-level) system call implementations
// are in sysfile.c.

#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "spinlock.h"
#include "proc.h"
#include "sleeplock.h"
#include "xv6fs.h"
#include "buf.h"
#include "file.h"
#include "debug.h"
#include "vfs.h"
#include "vfs_mount.h"

extern struct mt mtable;

int xv6fs_init(void);
struct xv6fs_superblock* alloc_xv6fs_sb();
struct xv6fs_inode* alloc_xv6fs_inode();
int xv6fs_mount(struct inode *devi, struct inode *ip);
int xv6fs_unmount(struct inode *devi);
struct inode *xv6fs_getroot(int major, int minor);
static void xv6fs_readsb(int dev, struct superblock *sb);
struct inode* xv6fs_ialloc(uint dev, short type);
uint xv6fs_balloc(uint dev);
void xv6fs_bzero(int dev, int bno);
void xv6fs_bfree(int dev, uint b);
int xv6fs_namecmp(const char *s, const char *t);
struct inode* xv6fs_dirlookup(struct inode *dp, char *name, uint *poff);
void xv6fs_iupdate(struct inode *ip);
void xv6fs_itrunc(struct inode *ip);
void xv6fs_cleanup(struct inode *ip);
uint xv6fs_bmap(struct inode *ip, uint bn);
void xv6fs_ilock(struct inode *ip);
int xv6fs_readi(struct inode *ip, int user_dst, uint64 dst, uint off, uint n);
int xv6fs_writei(struct inode *ip, int user_src, uint64 src, uint off, uint n);
int xv6fs_unlink(struct inode *dp, uint off);
int xv6fs_isdirempty(struct inode *dp);
struct inode* xv6fs_iget(uint dev, uint inum);

struct fs_ops xv6fs_fsops = {
  .fs_init = &xv6fs_init,
  .mount   = &xv6fs_mount,
  .unmount = &xv6fs_unmount,
  .getroot = &xv6fs_getroot,
  .readsb  = &xv6fs_readsb,
  .ialloc  = &xv6fs_ialloc,
  .balloc  = &xv6fs_balloc,
  .bzero   = &xv6fs_bzero,
  .bfree   = &xv6fs_bfree,
  .brelse  = &brelse,
  .bwrite  = &bwrite,
  .bread   = &bread,
  .namecmp = &xv6fs_namecmp
};

struct inode_ops xv6fs_iops = {
  .dirlookup  = &xv6fs_dirlookup,
  .iupdate    = &xv6fs_iupdate,
  .itrunc     = &xv6fs_itrunc,
  .cleanup    = &xv6fs_cleanup,
  .bmap       = &xv6fs_bmap,
  .ilock      = &xv6fs_ilock,
  .iunlock    = &generic_iunlock,
  .stati      = &generic_stati,
  .readi      = &xv6fs_readi,
  .writei     = &xv6fs_writei,
  .dirlink    = &generic_dirlink,
  .unlink     = &xv6fs_unlink,
  .isdirempty = &xv6fs_isdirempty
};

struct vfs_filesystem xv6fs = {
  .type = VFS_TYPE_XV6FS,
  .name = "xv6fs",
  .fsops = &xv6fs_fsops,
  .iops = &xv6fs_iops
};

#define min(a, b) ((a) < (b) ? (a) : (b))
// there should be one xv6fs_superblock per disk device, but we run with
// only one device
// struct superblock sb;
extern struct superblock sb[NDEV];
extern struct icache_t icache;

static struct {
  struct spinlock lock;
  struct xv6fs_superblock sb[MAXVFSSIZE];
} xv6fs_sb_pool; // It is a Pool of xv6fs Superblock Filesystems

static struct {
  struct spinlock lock;
  struct xv6fs_inode xv6fs_i_entry[NINODE];
} xv6fs_inode_pool;

struct xv6fs_superblock*
alloc_xv6fs_sb()
{
  struct xv6fs_superblock *xsb;

  acquire(&xv6fs_sb_pool.lock);
  for (xsb = &xv6fs_sb_pool.sb[0]; xsb < &xv6fs_sb_pool.sb[MAXVFSSIZE]; xsb++) {
    if (xsb->flags == XV6FS_SB_FREE) {
      xsb->flags |= XV6FS_SB_USED;
      release(&xv6fs_sb_pool.lock);

      return xsb;
    }
  }
  release(&xv6fs_sb_pool.lock);

  return 0;
}

struct xv6fs_inode*
alloc_xv6fs_inode()
{
  struct xv6fs_inode *ip;

  acquire(&xv6fs_inode_pool.lock);
  for (ip = &xv6fs_inode_pool.xv6fs_i_entry[0]; ip < &xv6fs_inode_pool.xv6fs_i_entry[NINODE]; ip++) {
    if (ip->flags == XV6FS_INODE_FREE) {
      ip->flags |= XV6FS_INODE_USED;
      release(&xv6fs_inode_pool.lock);
      return ip;
    }
  }
  release(&xv6fs_inode_pool.lock);
  return 0;
}

// Init fs
// void xv6fs_init(int dev) {
//   readsb(dev, &sb);
//   if (sb.magic != FSMAGIC)
//     panic("invalid file system");
//   initlog(dev, &sb);
// }

int
init_xv6fs(void)
{
  initlock(&xv6fs_sb_pool.lock, "xv6fs_sb_pool");
  initlock(&xv6fs_inode_pool.lock, "xv6fs_inode_pool");
  return register_fs(&xv6fs);
}

int
xv6fs_init(void)
{
  return 0;
}

int
xv6fs_mount(struct inode *devi, struct inode *ip)
{
  struct mntentry *mp;

  // Read the Superblock
  xv6fs_fsops.readsb(devi->minor, &sb[devi->minor]);

  // Read the root device
  struct inode *devrtip = xv6fs_fsops.getroot(devi->major, devi->minor);

  acquire(&mtable.lock);
  for (mp = &mtable.mpoint[0]; mp < &mtable.mpoint[MOUNTSIZE]; mp++) {
    // This slot is available
    if (mp->flags == 0) {
found_slot:
      mp->dev = devi->minor;
      mp->m_inode = ip;
      mp->pdata = &sb[devi->minor];
      mp->flags |= M_USED;
      mp->m_rtinode = devrtip;

      release(&mtable.lock);

      initlog(devi->minor, sb[devi->minor].fs_info);
      return 0;
    } else {
      // The disk is already mounted
      if (mp->dev == devi->minor) {
        release(&mtable.lock);
        return -1;
      }

      if (ip->dev == mp->m_inode->dev && ip->inum == mp->m_inode->inum)
        goto found_slot;
    }
  }
  release(&mtable.lock);

  return -1;
}


int
xv6fs_unmount(struct inode *devi)
{
  return 0;
}

struct inode *
xv6fs_getroot(int major, int minor)
{
  return xv6fs_iget(minor, ROOTINO);
}

void
xv6fs_readsb(int dev, struct superblock *sb)
{
  struct buf *bp;
  struct xv6fs_superblock *xsb;

  if(!(sb->flags & SB_LOADED)) {
    xsb = alloc_xv6fs_sb(); // Allocate a new S5 sb struct to the superblock.
  } else{
    xsb = sb->fs_info;
  }

  // These sets are needed because of bread
  sb->major = BLOCKMAJOR;
  sb->minor = dev;
  sb->blocksize = BSIZE;

  bp = xv6fs_fsops.bread(dev, 1);
  memmove(xsb, bp->data, sizeof(*xsb) - sizeof(xsb->flags));
  xv6fs_fsops.brelse(bp);

  sb->fs_info = xsb;
  sb->flags |= SB_LOADED;
}

struct inode*
xv6fs_ialloc(uint dev, short type)
{
  int inum;
  struct buf *bp;
  struct xv6fs_dinode *dip;
  struct xv6fs_superblock *xsb;

  xsb = sb[dev].fs_info;

  for(inum = 1; inum < xsb->ninodes; inum++){
    bp = xv6fs_fsops.bread(dev, IBLOCK(inum, (*xsb)));
    dip = (struct xv6fs_dinode*)bp->data + inum % IPB;
    if(dip->type == 0){  // a free inode
      memset(dip, 0, sizeof(*dip));
      dip->type = type;
      log_write(bp);   // mark it allocated on the disk
      xv6fs_fsops.brelse(bp);
      return xv6fs_iget(dev, inum);
    }
    xv6fs_fsops.brelse(bp);
  }
  panic("ialloc: no inodes");
}

uint
xv6fs_balloc(uint dev)
{
  int b, bi, m;
  struct buf *bp;
  struct xv6fs_superblock *xsb;

  xsb = sb[dev].fs_info;
  bp = 0;
  for (b = 0; b < xsb->size; b += BPB) {
    bp = xv6fs_fsops.bread(dev, BBLOCK(b, (*xsb)));
    for (bi = 0; bi < BPB && b + bi < xsb->size; bi++) {
      m = 1 << (bi % 8);
      if ((bp->data[bi/8] & m) == 0) {  // Is block free?
        bp->data[bi/8] |= m;  // Mark block in use.
        log_write(bp);
        xv6fs_fsops.brelse(bp);
        xv6fs_fsops.bzero(dev, b + bi);
        return b + bi;
      }
    }
    xv6fs_fsops.brelse(bp);
  }
  panic("balloc: out of blocks");
}

void
xv6fs_bzero(int dev, int bno)
{
  struct buf *bp;

  bp = xv6fs_fsops.bread(dev, bno);
  memset(bp->data, 0, BSIZE);
  log_write(bp);
  xv6fs_fsops.brelse(bp);
}

void
xv6fs_bfree(int dev, uint b)
{
  struct buf *bp;
  int bi, m;
  struct xv6fs_superblock *xsb;

  xsb = sb[dev].fs_info;
  xv6fs_fsops.readsb(dev, &sb[dev]);
  bp = xv6fs_fsops.bread(dev, BBLOCK(b, (*xsb)));
  bi = b % BPB;
  m = 1 << (bi % 8);
  if((bp->data[bi/8] & m) == 0)
    panic("freeing free block");
  bp->data[bi/8] &= ~m;
  log_write(bp);
  xv6fs_fsops.brelse(bp);
}

struct inode*
xv6fs_dirlookup(struct inode *dp, char *name, uint *poff)
{
  uint off, inum;
  struct dirent de;

  if(dp->type == T_FILE || dp->type == T_DEVICE)
    panic("dirlookup not DIR");

  for(off = 0; off < dp->size; off += sizeof(de)){
    if(xv6fs_iops.readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
      panic("dirlink read");
    if(de.inum == 0)
      continue;
    if(xv6fs_fsops.namecmp(name, de.name) == 0){
      // entry matches path element
      if(poff)
        *poff = off;
      inum = de.inum;
      return xv6fs_iget(dp->dev, inum);
    }
  }

  return 0;
}

void
xv6fs_iupdate(struct inode *ip)
{
  struct buf *bp;
  struct xv6fs_dinode *dip;
  struct xv6fs_superblock *xsb;
  struct xv6fs_inode *xip;

  xip = ip->i_private;
  xsb = sb[ip->dev].fs_info;
  bp = xv6fs_fsops.bread(ip->dev, IBLOCK(ip->inum, (*xsb)));
  dip = (struct xv6fs_dinode*)bp->data + ip->inum%IPB;
  dip->type = ip->type;
  dip->major = ip->major;
  dip->minor = ip->minor;
  dip->nlink = ip->nlink;
  dip->size = ip->size;
  memmove(dip->addrs, xip->addrs, sizeof(xip->addrs));
  log_write(bp);
  xv6fs_fsops.brelse(bp);
}

void
xv6fs_itrunc(struct inode *ip)
{
  int i, j;
  struct buf *bp;
  uint *a;
  struct xv6fs_inode *xip;

  xip = ip->i_private;

  for(i = 0; i < NDIRECT; i++){
    if(xip->addrs[i]){
      xv6fs_fsops.bfree(ip->dev, xip->addrs[i]);
      xip->addrs[i] = 0;
    }
  }

  if(xip->addrs[NDIRECT]){
    bp = xv6fs_fsops.bread(ip->dev, xip->addrs[NDIRECT]);
    a = (uint*)bp->data;
    for (j = 0; j < NINDIRECT; j++) {
      if (a[j])
        xv6fs_fsops.bfree(ip->dev, a[j]);
    }
    xv6fs_fsops.brelse(bp);
    xv6fs_fsops.bfree(ip->dev, xip->addrs[NDIRECT]);
    xip->addrs[NDIRECT] = 0;
  }

  ip->size = 0;
  xv6fs_iops.iupdate(ip);
}

void
xv6fs_cleanup(struct inode *ip)
{
  memset(ip->i_private, 0, sizeof(struct xv6fs_inode));
}

uint
xv6fs_bmap(struct inode *ip, uint bn)
{
  uint addr, *a;
  struct buf *bp;
  struct xv6fs_inode *xip;

  xip = ip->i_private;

  if(bn < NDIRECT){
    if((addr = xip->addrs[bn]) == 0)
      xip->addrs[bn] = addr = xv6fs_fsops.balloc(ip->dev);
    return addr;
  }
  bn -= NDIRECT;

  if(bn < NINDIRECT){
    // Load indirect block, allocating if necessary.
    if((addr = xip->addrs[NDIRECT]) == 0) {
      addr = xv6fs_fsops.balloc(ip->dev);
      if(addr == 0)
        return 0;
      xip->addrs[NDIRECT] = addr;
    }
    bp = xv6fs_fsops.bread(ip->dev, addr);
    a = (uint*)bp->data;
    if((addr = a[bn]) == 0){
      if(addr) {
        a[bn] = addr = xv6fs_fsops.balloc(ip->dev);
        log_write(bp);
      }
    }
    xv6fs_fsops.brelse(bp);
    return addr;
  }

  panic("bmap: out of range");
}

// Lock the given inode.
// Reads the inode from disk if necessary.
void xv6fs_ilock(struct inode *ip)
{
  struct buf *bp;
  struct xv6fs_dinode *dip;
  struct xv6fs_superblock *xsb;
  struct xv6fs_inode *xip;

  xip = ip->i_private;

  xsb = sb[ip->dev].fs_info;

  if(ip == 0 || ip->ref < 1)
    panic("ilock");

  acquiresleep(&ip->lock);

  if(ip->valid == 0){
    bp = bread(ip->dev, IBLOCK(ip->inum, (*xsb)));
    dip = (struct xv6fs_dinode*)bp->data + ip->inum%IPB;
    ip->type = dip->type;
    ip->major = dip->major;
    ip->minor = dip->minor;
    ip->nlink = dip->nlink;
    ip->size = dip->size;
    memmove(xip->addrs, dip->addrs, sizeof(xip->addrs));
    xv6fs_fsops.brelse(bp);
    ip->valid = 1;
    if(ip->type == 0)
      panic("ilock: no type");
  }
}

int
xv6fs_readi(struct inode *ip, int user_dst, uint64 dst, uint off, uint n)
{
  uint tot, m;
  struct buf *bp;

  if(off > ip->size || off + n < off)
    return 0;
  if(off + n > ip->size)
    n = ip->size - off;

  for(tot=0; tot<n; tot+=m, off+=m, dst+=m){
    uint addr = ip->iops->bmap(ip, off/BSIZE);
    if(addr == 0)
      break;
    bp = ip->fs->fsops->bread(ip->dev, addr);
    m = min(n - tot, BSIZE - off%BSIZE);
    if(either_copyout(user_dst, dst, bp->data + (off % BSIZE), m) == -1) {
      ip->fs->fsops->brelse(bp);
      tot = -1;
      break;
    }
    ip->fs->fsops->brelse(bp);
  }
  return tot;
}

int
xv6fs_writei(struct inode *ip, int user_src, uint64 src, uint off, uint n)
{
  uint tot, m;
  struct buf *bp;

  if(ip->type == T_DEVICE){
    if(ip->major < 0 || ip->major >= NDEV || !devsw[ip->major].write)
      return -1;
    return devsw[ip->major].write(0, src, n);
  }

  if(off > ip->size || off + n < off)
    return -1;
  if(off + n > MAXFILE*BSIZE)
    return -1;

  for(tot=0; tot<n; tot+=m, off+=m, src+=m){
    uint addr = ip->iops->bmap(ip, off/BSIZE);
    if(addr == 0)
      break;
    bp = ip->fs->fsops->bread(ip->dev, addr);
    m = min(n - tot, BSIZE - off%BSIZE);
    if(either_copyin(bp->data + (off % BSIZE), user_src, src, m) == -1) {
      ip->fs->fsops->brelse(bp);
      break;
    }
    log_write(bp);
    brelse(bp);
  }

  if(off > ip->size)
    ip->size = off;

  // write the i-node back to disk even if the size didn't change
  // because the loop above might have called bmap() and added a new
  // block to ip->addrs[].
  ip->iops->iupdate(ip);

  return tot;
}

int
xv6fs_isdirempty(struct inode *dp)
{
  int off;
  struct dirent de;

  for(off=2*sizeof(de); off<dp->size; off+=sizeof(de)){
    if(xv6fs_iops.readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
      panic("isdirempty: readi");
    if(de.inum != 0)
      return 0;
  }
  return 1;
}

int
xv6fs_unlink(struct inode *dp, uint off)
{
  struct dirent de;

  memset(&de, 0, sizeof(de));
  if(dp->iops->writei(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
    return -1;

  return 0;
}

int
xv6fs_namecmp(const char *s, const char *t)
{
  return strncmp(s, t, DIRSIZ);
}

int
xv6fs_fill_inode(struct inode *ip) {
  struct xv6fs_inode *xip;

  xip = alloc_xv6fs_inode();
  if (!xip) {
    panic("No xv6fs inode available");
  }

  ip->i_private = xip;

  return 1;
}

struct inode*
xv6fs_iget(uint dev, uint inum)
{
  return iget(dev, inum, &xv6fs_fill_inode);
}

