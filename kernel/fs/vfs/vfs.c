#include "types.h"
#include "vfs.h"
#include "debug.h"
#include "defs.h"
#include "spinlock.h"
#include "vfs_xv6fs.h"
#include "vfs_ext4.h"
#include "param.h"
#include "proc.h"
#include "stat.h"
#include "list.h"
#include "device.h"
#include "vfs_mount.h"
#include "ext4_errno.h"
#include "blockdev.h"
#include "ext4fs.h"

#define min(a, b) ((a) < (b) ? (a) : (b))

extern struct file_ops ext4_fops;
extern struct file_ops xv6fs_fops;

static struct inode* namex(char *path, int nameiparent, char *name);


struct superblock sb[NDEV];

/*
 * This is te representation of mounted lists.
 * It is defferent from the vfssw, because it is mapping the mounted
 * on filesystem per (major, minor)
 */
struct {
    struct spinlock lock;
    struct list_head fs_list;
  } vfsmlist;
  
struct vfs *rootfs; // It is the golbal pointer to root fs entry

struct icache_t icache;
struct {
    struct spinlock lock;
    struct list_head fs_list;
  } vfssw;
  

struct vfs_filesystem *vfs_fs[VFS_MAXFS];

struct file_ops *vfs_fops[VFS_MAXFS] = {
    [VFS_TYPE_UNKNOWN] = NULL,
    [VFS_TYPE_EXT4] = &ext4_fops,
    [VFS_TYPE_XV6FS] = &xv6fs_fops,
};

struct {
    struct spinlock lock;
    struct mount_point mount_points[MAX_MOUNTS];
} vfs_mount_table;

static struct {
  struct spinlock lock;
  struct vfs vfsentry[MAXVFSSIZE];
} vfspool;

struct vfs*
allocvfs()
{
  struct vfs *vfs;

  acquire(&vfspool.lock);
  for (vfs = &vfspool.vfsentry[0]; vfs < &vfspool.vfsentry[MAXVFSSIZE]; vfs++) {
    if (vfs->flag == VFS_FREE) {
      vfs->flag |= VFS_USED;
      release(&vfspool.lock);

      return vfs;
    }
  }
  release(&vfspool.lock);

  return 0;
}

void init_vfs_mtable() {
  memset(&vfs_mount_table, 0, sizeof(vfs_mount_table));
  initlock(&vfs_mount_table.lock, "vfs_mount_table");
  for (int i = 0; i < MAX_MOUNTS; i++) {
    vfs_mount_table.mount_points[i].mp = NULL;
    vfs_mount_table.mount_points[i].dev = -1;
    vfs_mount_table.mount_points[i].type = VFS_TYPE_UNKNOWN;
  }
}


// Add rootvfs on the list
void
install_rootfs(void)
{
  if ((rootfs = allocvfs()) == 0) {
    panic("Failed on rootfs allocation");
  }

  rootfs->major = BLOCKMAJOR;
  rootfs->minor = ROOTDEV;

  struct vfs_filesystem *fst = getfs(ROOTFSTYPE);
  // printf("fst = %s\n", fst->name);
  // printf("fst = %d\n", fst->type);
  if (fst == 0) {
    panic("The root fs type is not supported");
  }

  rootfs->fs_t = fst;

  acquire(&vfsmlist.lock);
  list_add_tail(&(rootfs->fs_next), &(vfsmlist.fs_list));
  release(&vfsmlist.lock);
  
  // mount the rootfs to the root directory
  if(vfs_mount(fst, "/") < 0) {
    panic("Failed to mount rootfs");
  }
  // add the rootfs to the vfs table
  if(addfs(fst) < 0) {
    panic("Failed to add rootfs to the vfs table");
  }
}

void
init_vfsmlist(void)
{
  initlock(&vfsmlist.lock, "vfsmlist");
  initlock(&vfspool.lock, "vfspol");
  INIT_LIST_HEAD(&(vfsmlist.fs_list));
}

struct vfs*
get_vfs_entry(int major, int minor)
{
  struct vfs *vfs;

  list_for_each_entry(vfs, &(vfsmlist.fs_list), fs_next) {
    if (vfs->major == major && vfs->minor == minor) {
      return vfs;
    }
  }

  return 0;
}

int put_vfs_on_list(int major, int minor, struct vfs_filesystem *fs_t)
{
  struct vfs* nvfs;

  if ((nvfs = allocvfs()) == 0) {
    return -1;
  }

  nvfs->major = major;
  nvfs->minor = minor;
  nvfs->fs_t  = fs_t;

  acquire(&vfsmlist.lock);
  list_add_tail(&(nvfs->fs_next), &(vfsmlist.fs_list));
  release(&vfsmlist.lock);

  return 0;
}

void
init_vfssw(void)
{
  initlock(&vfssw.lock, "vfssw");
  INIT_LIST_HEAD(&(vfssw.fs_list));
}

int
register_fs(struct vfs_filesystem *fs)
{
  acquire(&vfssw.lock);
  list_add(&(fs->fs_list), &(vfssw.fs_list));
  release(&vfssw.lock);

  return 0;
}

struct vfs_filesystem* getfs(const char *fs_name)
{
  struct vfs_filesystem *fs;

  list_for_each_entry(fs, &(vfssw.fs_list), fs_list) {
    if (strcmp(fs_name, fs->name) == 0) {
      return fs;
    }
  }
  return 0;
}
/**
 * @brief get fs by type
 * 
 * @param type fs_type
 * @return struct vfs_filesystem* pointer to the filesystem structure , null if not found
 */
struct vfs_filesystem *vfs_getfs_bytype(vfs_type_t type) {
  if(type == VFS_TYPE_UNKNOWN) {
    return NULL;
  }
    for (int i = 0; i < VFS_MAXFS; i++) {
        if (vfs_fs[i] && vfs_fs[i]->type == type) {
            return vfs_fs[i];
        }
    }
    return NULL;
}

struct vfs_filesystem *vfs_getfs_bydev(int dev) {
    for (int i = 0; i < VFS_MAXFS; i++) {
        if (vfs_fs[i] && vfs_fs[i]->dev == dev) {
            return vfs_fs[i];
        }
    }
    return NULL;
}
struct vfs_filesystem *vfs_getfs_byname(const char *name) {
    for (int i = 0; i < VFS_MAXFS; i++) {
        if (vfs_fs[i] && strcmp(vfs_fs[i]->name, name) == 0) {
            return vfs_fs[i];
        }
    }
    return NULL;
}

/**
 * @brief Resolve the filesystem instance of a given path by longest mount point prefix match
 * 
 * @param path path to resolve
 * @return struct vfs_filesystem* pointer to the filesystem instance
 */
struct vfs_filesystem * vfs_resolve_fs(const char* path) {
  struct vfs_filesystem *selected_fs = NULL;
  int longest_match_len = -1;
  char abs_path[MAXPATH] = {0};

  get_absolute_path(path, "/", abs_path);
  // printf("vfs_resolve_fs: abs_path = %s\n", abs_path);

  acquire(&vfs_mount_table.lock);
  for (int i = 0; i < MAX_MOUNTS; i++) {
      const char* mp = vfs_mount_table.mount_points[i].mp;
      struct vfs_filesystem* fs = vfs_getfs_bytype(vfs_mount_table.mount_points[i].type);
      if (!mp || !fs) continue;

      int len = strlen(mp);

      if (substr_cmp(mp, abs_path) == 0) {
          if (len > longest_match_len) {
              longest_match_len = len;
              selected_fs = fs;
          }
      }
  }
  release(&vfs_mount_table.lock);

  return selected_fs;
}


/**
 * @brief Get the absolute path of a file
 * 
 * @param path path to the file
 * @param cwd current working directory
 * @param absolute_path pointer to the buffer to store the absolute path
 */
void get_absolute_path(const char *path, const char *cwd, char *absolute_path) {
    if (path == NULL) {
        strncpy(absolute_path, cwd, strlen(cwd));
    } else if (path[0] == '/') {
        strcpy(absolute_path, path);
    } else {
        strcpy(absolute_path, cwd);
        strcat(absolute_path, "/");
        strcat(absolute_path, path);
    }
    // handle ./ and ../
    char *p = absolute_path;
    while (*p != '\0') {
        if (*p == '.' && *(p + 1) == '/') {
            strcpy(p, p + 2);
        } else if (*p == '.' && *(p + 1) == '.' && *(p + 2) == '/') {
            char *q = p - 2;
            while (q >= absolute_path && *q != '/') {
                q--;
            }
            if (q >= absolute_path) {
                strcpy(q + 1, p + 3);
                p = q;
            } else {
                strcpy(absolute_path, p + 3);
                p = absolute_path;
            }
        } else {
            p++;
        }
    }

    /* handle trailing . and .. */
    p = absolute_path + strlen(absolute_path) - 2;
    if ((*p == '/' || *p == '.') && *(p + 1) == '.' && *(p + 2) == '\0') {
        *p = *(p + 1) = *(p + 2) = '\0';
    }
    p--;
    if (*p == '/' && *(p + 1) == '.' && *(p + 2) == '.' && *(p + 3) == '\0') {
        if (p == absolute_path) {
            *(p + 1) = *(p + 2) = *(p + 3) = '\0';
            return;
        }
        char *q = p - 1;
        while (q >= absolute_path && *q != '/') {
            q--;
        }
        if (q >= absolute_path) {
            *q = '\0';
            p = q;
        } else {
            strcpy(absolute_path, p + 4);
            p = absolute_path;
        }
    }


    while (absolute_path[0] == '/' && absolute_path[1] == '/') {
        strcpy(absolute_path, absolute_path + 1);
    }
    size_t len = strlen(absolute_path);
    if (absolute_path[len - 1] == '/') {
        absolute_path[len - 1] = '\0';
        --len;
    }
    if (strlen(absolute_path) == 0) {
        strcpy(absolute_path, "/");
    } else if (absolute_path[0] != '/') {
        size_t len2 = strlen(absolute_path);
        char *x = absolute_path + len2;
        *(x + 1) = 0;
        while (x > absolute_path) {
            *x = *(x - 1);
            x--;
        }
        *x = '/';
    }
}

int sb_set_blocksize(struct superblock *sb, int size)
{
  /* If we get here, we know size is power of two
   * and it's value is between 512 and PAGE_SIZE */
  sb->blocksize = size;
  sb->s_blocksize_bits = blksize_bits(size);
  return sb->blocksize;
}

// Common idiom: unlock, then put.
void iunlockput(struct inode *ip)
{
  ip->iops->iunlock(ip);
  iput(ip);
}

void generic_iunlock(struct inode *ip)
{
  if(ip == 0 || !holdingsleep(&ip->lock) || ip->ref < 1)
    panic("iunlock");

  releasesleep(&ip->lock);
}

void generic_stati(struct inode *ip, struct stat *st)
{
  return;
}

int generic_dirlink(struct inode *dp, char *name, uint inum)
{
  int off;
  struct dirent de;
  struct inode *ip;

  // Check that name is not present.
  if((ip = dp->iops->dirlookup(dp, name, 0)) != 0){
    iput(ip);
    return -1;
  }

  // Look for an empty dirent.
  for(off = 0; off < dp->size; off += sizeof(de)){
    if(dp->iops->readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
      panic("dirlink read");
    if(de.inum == 0)
      break;
  }

  strncpy(de.name, name, DIRSIZ);
  de.inum = inum;
  if(dp->iops->writei(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
    panic("dirlink");

  return 0;
}

int generic_readi(struct inode *ip, char *dst, uint off, uint n)
{
  uint tot, m;
  struct buf *bp;

  if(ip->type == T_DEVICE){
    if(ip->major < 0 || ip->major >= NDEV || !devsw[ip->major].read)
      return -1;
    return devsw[ip->major].read(0, (uint64)dst, n);
  }

  if(off > ip->size || off + n < off)
    return -1;
  if(off + n > ip->size)
    n = ip->size - off;

  for(tot=0; tot<n; tot+=m, off+=m, dst+=m){
    bp = ip->fs->fsops->bread(ip->dev, ip->iops->bmap(ip, off/sb[ip->dev].blocksize));
    m = min(n - tot, sb[ip->dev].blocksize - off % sb[ip->dev].blocksize);
    memmove(dst, bp->data + off % sb[ip->dev].blocksize, m);
    ip->fs->fsops->brelse(bp);
  }

  return n;
}
  
void iinit() {
  int i = 0;

  initlock(&icache.lock, "icache");
  for (i = 0; i < NINODE; i++) {
    initsleeplock(&icache.inode[i].lock, "inode");
  }
}


int ext4_init();

// static void print_xsb(struct xv6fs_superblock *xsp) {
//   printf("xv6fs: magic %x, size %d, nblocks %d, ninodes %d, nlog %d, logstart %d, inodestart %d, bmapstart %d\n",
//           xsp->magic, xsp->size, xsp->nblocks, xsp->ninodes, xsp->nlog, xsp->logstart, xsp->inodestart, xsp->bmapstart);
// }
void fsinit(int dev) {
  
  #ifdef __USE_XV6FS
  rootfs->fs_t->fsops->readsb(dev, &sb[dev]);
  // TODO: how about other kind of fs?
  if(rootfs->fs_t->type == VFS_TYPE_XV6FS) {
    struct xv6fs_superblock *xsp = (struct xv6fs_superblock*)sb[dev].fs_info;
    if(xsp->magic != FSMAGIC) {
      panic("xv6fs: bad super block");
    }
    initlog(dev,xsp);
  }
  #else
  ext4_init();
  #endif
}

/**
 * @brief find the inode with the given private data, and fill a potential empty inode 
 * 
 * @param pdata i_private
 * @return struct inode* 
 */
struct inode *ifind_fempty(void *pdata) {
  struct inode *ip = NULL;
  struct inode *empty = NULL;
  struct vfs_filesystem *fs = NULL;
  acquire(&icache.lock);
  for(ip = &icache.inode[0]; ip < &icache.inode[NINODE]; ip++) {
    if (ip->ref > 0 && ip->i_private == pdata) {
      ip->ref++;
      release(&icache.lock);
      return ip;
    }
    if(empty == 0 && ip->ref == 0) { // Remember empty slot.
      empty = ip;
    }
  }
  if(empty == 0){
    panic("ifind_wempty: no inodes");
  }

    fs = get_vfs_entry(BLOCKMAJOR, ROOTDEV)->fs_t;

    ip = empty;
    ip->dev = ROOTDEV;
    ip->inum = 999999; // TODO
    ip->ref = 1;
    ip->valid = 0;
    ip->fs = fs;
    ip->iops = fs->iops;
    ip->i_private = pdata;
    release(&icache.lock);
    return ip;

}
// Find the inode with number inum on device dev
// and return the in-memory copy. Does not lock
// the inode and does not read it from disk.
struct inode* iget(uint dev, uint inum, int (*fill_inode)(struct inode *))
{
  struct inode *ip, *empty;
  struct vfs_filesystem *fs;

  acquire(&icache.lock);

  // Is the inode already cached?
  empty = 0;
  for(ip = &icache.inode[0]; ip < &icache.inode[NINODE]; ip++){
    if(ip->ref > 0 && ip->dev == dev && ip->inum == inum){

      // If the current inode is an mount point
      if (ip->type == T_MOUNT) {
        struct inode *rinode = mnt_rtinode(ip);
        if (rinode == 0) {
          panic("Invalid Inode on Mount Table");
        }
        rinode->ref++;
        release(&icache.lock);
        return rinode;
      }

      ip->ref++;
      release(&icache.lock);
      return ip;
    }
    if(empty == 0 && ip->ref == 0)    // Remember empty slot.
      empty = ip;
  }

  // Recycle an inode cache entry.
  if(empty == 0)
    panic("iget: no inodes");

  fs = get_vfs_entry(BLOCKMAJOR, dev)->fs_t;

  ip = empty;
  ip->dev = dev;
  ip->inum = inum;
  ip->ref = 1;
  ip->valid = 0;
  ip->fs = fs;
  ip->iops = fs->iops;

  release(&icache.lock);

  if (!fill_inode(ip)) {
    panic("Error on fill inode");
  }

  return ip;
}
  // Increment reference count for ip.
  // Returns ip to enable ip = idup(ip1) idiom.
  struct inode *idup(struct inode *ip) {
    acquire(&icache.lock);
    ip->ref++;
    release(&icache.lock);
    return ip;
  }
  
  
// Drop a reference to an in-memory inode.
// If that was the last reference, the inode table entry can
// be recycled.
// If that was the last reference and the inode has no links
// to it, free the inode (and its content) on disk.
// All calls to iput() must be inside a transaction in
// case it has to free the inode.
void iput(struct inode *ip) {
  acquire(&icache.lock);

  if (ip->ref == 1 && ip->valid && ip->nlink == 0) {
    // inode has no links and no other references: truncate and free.

    // ip->ref == 1 means no other process can have ip locked,
    // so this acquiresleep() won't block (or deadlock).
    acquiresleep(&ip->lock);

    release(&icache.lock);

    ip->iops->itrunc(ip);
    ip->type = 0;
    ip->iops->iupdate(ip);
    ip->valid = 0;
    releasesleep(&ip->lock);
    acquire(&icache.lock);
  }

  ip->ref--;
  if (ip->ref == 0 ) {
    ip->iops->cleanup(ip);
  }
  release(&icache.lock);
}


  struct inode *namei(char *path) {
    #ifdef __USE_XV6FS
    char name[DIRSIZ];
    return namex(path, 0, name);
    #else
    return ext4_namei(path);
    #endif
  }
  

  
  
// Look up and return the inode for a path name.
// If parent != 0, return the inode for the parent and copy the final
// path element into name, which must have room for DIRSIZ bytes.
// Must be called inside a transaction since it calls iput().
// static struct inode*
// namex(char *path, int nameiparent, char *name)
// {
//   struct inode *ip, *next, *ir;

//   if(*path == '/')
//     ip = rootfs->fs_t->fsops->getroot(BLOCKMAJOR, ROOTDEV);
//   else
//     ip = idup(myproc()->cwd);

//   while((path = skipelem(path, name)) != 0){
//     ip->iops->ilock(ip);
//     if(ip->type != T_DIR){
//       iunlockput(ip);
//       return 0;
//     }
//     if(nameiparent && *path == '\0'){
//       // Stop one level early.
//       ip->iops->iunlock(ip);
//       return ip;
//     }

//     component_search:
//     if((next = ip->iops->dirlookup(ip, name, 0)) == 0){
//       iunlockput(ip);
//       return 0;
//     }

//     ir = next->fs->fsops->getroot(BLOCKMAJOR, next->dev);

//     if (next->inum == ir->inum  && is_rtinode(ip) && (strncmp(name, "..", 2) == 0)) {
//       struct inode *mntinode = mnt_inode(ip);
//       iunlockput(ip);
//       ip = mntinode;
//       ip->iops->ilock(ip);
//       ip->ref++;
//       goto component_search;
//     }

//     iunlockput(ip);
//     ip = next;
//   }
//   if(nameiparent){
//     iput(ip);
//     return 0;
//   }

//   #ifdef __DEBUG_NAMEX
//   struct xv6fs_inode *xip = ip->i_private;
//   Log("ip->dev = %d", ip->dev);
//   Log("ip->inum = %d", ip->inum);
//   Log("xip->type = %d", xip->type);
//   Log("xip->size = %d", xip->size);
//   Log("xip->nlink = %d", xip->nlink);
//   Log("xip->major = %d", xip->major);
//   Log("xip->minor = %d", xip->minor);
//   Log("xip->addrs[0] = %d", xip->addrs[0]);
// #endif  
//   return ip;
// }

  // Paths
  
  // Copy the next path element from path into name.
  // Return a pointer to the element following the copied one.
  // The returned path has no leading slashes,
  // so the caller can check *path=='\0' to see if the name is the last one.
  // If no name to remove, return 0.
  //
  // Examples:
  //   skipelem("a/bb/c", name) = "bb/c", setting name = "a"
  //   skipelem("///a//bb", name) = "bb", setting name = "a"
  //   skipelem("a", name) = "", setting name = "a"
  //   skipelem("", name) = skipelem("////", name) = 0
  //
  static char *skipelem(char *path, char *name) {
    char *s;
    int len;
  
    while (*path == '/')
      path++;
    if (*path == 0)
      return 0;
    s = path;
    while (*path != '/' && *path != 0)
      path++;
    len = path - s;
    if (len >= DIRSIZ)
      memmove(name, s, DIRSIZ);
    else {
      memmove(name, s, len);
      name[len] = 0;
    }
    while (*path == '/')
      path++;
    return path;
  }
  struct inode *nameiparent(char *path, char *name) {
    return namex(path, 1, name);
  }
// Look up and return the inode for a path name.
// If parent != 0, return the inode for the parent and copy the final
// path element into name, which must have room for DIRSIZ bytes.
// Must be called inside a transaction since it calls iput().

static struct inode *namex(char *path, int nameiparent, char *name) {
  struct inode *ip, *next;

  if (*path == '/')
    ip = rootfs->fs_t->fsops->getroot(BLOCKMAJOR, ROOTDEV);
  else
    ip = idup(myproc()->cwd);

  while ((path = skipelem(path, name)) != 0) {
    ip->iops->ilock(ip);
    if (ip->type != T_DIR) {
      iunlockput(ip);
      return 0;
    }
    if (nameiparent && *path == '\0') {
      // Stop one level early.
      ip->iops->iunlock(ip);
      return ip;
    }
    if ((next = ip->iops->dirlookup(ip, name, 0)) == 0) {
      iunlockput(ip);
      return 0;
    }
    iunlockput(ip);
    ip = next;
  }
  if (nameiparent) {
    iput(ip); 
    return 0;
  }
  #ifdef __DEBUG_NAMEX
  Log("ip->type = %d", ip->type);
  Log("ip->inum = %d", ip->inum);
  Log("ip->size = %d", ip->size);
  Log("ip->dev = %d", ip->dev);
  Log("ip->nlink = %d", ip->nlink);
  Log("ip->major = %d", ip->major);
  Log("ip->minor = %d", ip->minor);
  Log("ip->addrs[0] = %d", ip->addrs[0]);
#endif  
  return ip;
}


int vfs_mount(struct vfs_filesystem *fs, char *path) {
  for(int i = 0; i < MAX_MOUNTS; i++) {
    if (vfs_mount_table.mount_points[i].mp == NULL) {
      vfs_mount_table.mount_points[i].mp = path;
      vfs_mount_table.mount_points[i].type = fs->type;
      return 0;
    }
  }
  return -1;
}

int addfs(struct vfs_filesystem *fs) {
  for (int i = 0; i < VFS_MAXFS; i++) {
    if (vfs_fs[i] == NULL) {
      vfs_fs[i] = fs;
      return 0;
    }
  }
  return -1;
}