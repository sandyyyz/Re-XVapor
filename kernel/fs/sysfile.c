//
// File-system system calls.
// Mostly argument checking, since we don't trust
// user code, and calls into file.c and fs.c.
//

#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "spinlock.h"
#include "proc.h"
#include "xv6fs.h"
#include "sleeplock.h"
#include "file.h"
#include "fcntl.h"
#include "debug.h"
#include "vm.h"
#include "memlayout.h"
#include "device.h"
#include "ext4fs.h"
#include "vfs.h"
#include "ioctl.h"
#include "ext4_errno.h"

// for debug
int g_first_exec = 0;
// Fetch the nth word-sized system call argument as a file descriptor
// and return both the descriptor and the corresponding struct file.
int argfd(int n, int *pfd, struct file **pf)
{
  int fd;
  struct file *f;

   argint(n, &fd);
  if(fd < 0 || fd >= NOFILE || (f=myproc()->ofile[fd]) == 0)
    return -1;
  if(pfd)
    *pfd = fd;
  if(pf)
    *pf = f;
  return 0;
}

// Allocate a file descriptor for the given file.
// Takes over file reference from caller on success.
static int
fdalloc(struct file *f)
{
  int fd;
  struct proc *p = myproc();

  for(fd = 0; fd < NOFILE; fd++){
    if(p->ofile[fd] == 0){
      p->ofile[fd] = f;
      return fd;
    }
  }
  return -1;
}

uint64
sys_dup(void)
{
  struct file *f;
  int fd;

  if(argfd(0, 0, &f) < 0)
    return -1;
  if((fd=fdalloc(f)) < 0)
    return -1;
  filedup(f);
  return fd;
}

uint64
sys_read(void)
{
  struct file *f;
  int n;
  uint64 p;

  argaddr(1, &p);
  argint(2, &n);
  if(argfd(0, 0, &f) < 0)
    return -1;
  return fileread(f, p, n);
}

uint64
sys_write(void)
{
  struct file *f;
  int n;
  uint64 p;
  
  argaddr(1, &p);
  argint(2, &n);
  if(argfd(0, 0, &f) < 0)
    return -1;

  return filewrite(f, p, n);
}

uint64
sys_close(void)
{
  int fd;
  struct file *f;

  if(argfd(0, &fd, &f) < 0)
    return -1;
  myproc()->ofile[fd] = 0;
  fileclose(f);
  return 0;
}

uint64
sys_fstat(void)
{
  struct file *f;
  uint64 st; // user pointer to struct stat

  argaddr(1, &st);
  if(argfd(0, 0, &f) < 0)
    return -1;
  return filestat(f, st);
}

// Create the path new as a link to the same inode as old.
uint64
sys_link(void)
{
  char name[DIRSIZ], new[MAXPATH], old[MAXPATH];
  struct inode *dp, *ip;

  if(argstr(0, old, MAXPATH) < 0 || argstr(1, new, MAXPATH) < 0)
    return -1;

  begin_op();
  if((ip = namei(old)) == 0){
    end_op();
    return -1;
  }

  ip->iops->ilock(ip);
  if(ip->type == T_DIR){
    iunlockput(ip);
    end_op();
    return -1;
  }

  ip->nlink++;
  ip->iops->iupdate(ip);
  ip->iops->iunlock(ip);

  if((dp = nameiparent(new, name)) == 0)
    goto bad;
  ip->iops->ilock(dp);
  if(dp->dev != ip->dev || dp->iops->dirlink(dp, name, ip->inum) < 0){
    iunlockput(dp);
    goto bad;
  }
  iunlockput(dp);
  iput(ip);

  end_op();

  return 0;

bad:
  ip->iops->ilock(ip);
  ip->nlink--;
  ip->iops->iupdate(ip);
  iunlockput(ip);
  end_op();
  return -1;
}

uint64
sys_unlink(void)
{
  struct inode *ip, *dp;
  struct dirent de;
  char name[DIRSIZ], path[MAXPATH];
  uint off;

  if(argstr(0, path, MAXPATH) < 0)
    return -1;

  begin_op();
  if((dp = nameiparent(path, name)) == 0){
    end_op();
    return -1;
  }

  dp->iops->ilock(dp);

  // Cannot unlink "." or "..".
  if(dp->fs->fsops->namecmp(name, ".") == 0 || dp->fs->fsops->namecmp(name, "..") == 0)
    goto bad;

  if((ip = dp->iops->dirlookup(dp, name, &off)) == 0)
    goto bad;
  ip->iops->ilock(ip);

  if(ip->nlink < 1)
    panic("unlink: nlink < 1");
  if(ip->type == T_DIR && !ip->iops->isdirempty(ip)){
    iunlockput(ip);
    goto bad;
  }

  memset(&de, 0, sizeof(de));
  if(dp->iops->writei(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
    panic("unlink: writei");
  if(ip->type == T_DIR){
    dp->nlink--;
    dp->iops->iupdate(dp);
  }
  iunlockput(dp);

  ip->nlink--;
  ip->iops->iupdate(ip);
  iunlockput(ip);

  end_op();

  return 0;

bad:
  iunlockput(dp);
  end_op();
  return -1;
}

static struct inode*
create(char *path, short type, short major, short minor)
{
  struct inode *ip, *dp;
  char name[DIRSIZ];

  if((dp = nameiparent(path, name)) == 0)
    return 0;

  dp->iops->ilock(dp);

  if((ip = dp->iops->dirlookup(dp, name, 0)) != 0){
    iunlockput(dp);
    ip->iops->ilock(ip);
    if(type == T_FILE && (ip->type == T_FILE || ip->type == T_DEVICE))
      return ip;
    iunlockput(ip);
    return 0;
  }

  if((ip = dp->fs->fsops->ialloc(dp->dev, type)) == 0){
    iunlockput(dp);
    return 0;
  }

  ip->iops->ilock(ip);
  ip->major = major;
  ip->minor = minor;
  ip->nlink = 1;
  ip->iops->iupdate(ip);

  if(type == T_DIR){  // Create . and .. entries.
    // No ip->nlink++ for ".": avoid cyclic ref count.
    if(dp->iops->dirlink(ip, ".", ip->inum) < 0 || dp->iops->dirlink(ip, "..", dp->inum) < 0)
      goto fail;
  }

  if(dp->iops->dirlink(dp, name, ip->inum) < 0)
    goto fail;

  if(type == T_DIR){
    // now that success is guaranteed:
    dp->nlink++;  // for ".."
    dp->iops->iupdate(dp);
  }

  iunlockput(dp);

  return ip;

 fail:
  // something went wrong. de-allocate ip.
  ip->nlink = 0;
  ip->iops->iupdate(ip);
  iunlockput(ip);
  iunlockput(dp);
  return 0;
}

uint64
sys_open(void)
{
  char path[MAXPATH];
  int fd, omode;
  struct file *f;
  struct inode *ip;
  int n;
#ifdef __DEBUG_SYS_OPEN
  Log("do sys_open");
  // while(1);
#endif
  argint(1, &omode);
  if((n = argstr(0, path, MAXPATH)) < 0)
    return -1;

  begin_op();

  if(omode & O_CREATE){
    ip = create(path, T_FILE, 0, 0);
    if(ip == 0){
      end_op();
      return -1;
    }
  } else {
    if((ip = namei(path)) == 0){
      end_op();
      return -1;
    }
    ip->iops->ilock(ip);
    if(ip->type == T_DIR && omode != O_RDONLY){
      iunlockput(ip);
      end_op();
      return -1;
    }
  }

  if(ip->type == T_DEVICE && (ip->major < 0 || ip->major >= NDEV)){
    iunlockput(ip);
    end_op();
    return -1;
  }

  if((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0){
    if(f)
      fileclose(f);
    iunlockput(ip);
    end_op();
    return -1;
  }

  if(ip->type == T_DEVICE){
    f->type = FD_DEVICE;
    f->major = ip->major;
  } else {
    f->type = FD_INODE;
    f->off = 0;
  }
  f->ip = ip;
  // f->readable = !(omode & O_WRONLY);
  // printf("flags %d\n", f->flags);
  if(!(omode & O_WRONLY) )
    SET_READABLE(f->flags);
  // f->writable = (omode & O_WRONLY) || (omode & O_RDWR);
  if((omode & O_WRONLY) || (omode & O_RDWR))
    SET_WRITABLE(f->flags);

  if((omode & O_TRUNC) && ip->type == T_FILE){
    ip->iops->itrunc(ip);
  }


  ip->iops->iunlock(ip);
  end_op();

  return fd;
}

uint64
sys_mkdir(void)
{
  char path[MAXPATH];
  struct inode *ip;

  begin_op();
  if(argstr(0, path, MAXPATH) < 0 || (ip = create(path, T_DIR, 0, 0)) == 0){
    end_op();
    return -1;
  }
  iunlockput(ip);
  end_op();
  return 0;
}

uint64
sys_mknod(void)
{
  struct inode *ip;
  char path[MAXPATH];
  int major, minor;

  begin_op();
  argint(1, &major);
  argint(2, &minor);
  if((argstr(0, path, MAXPATH)) < 0 ||
     (ip = create(path, T_DEVICE, major, minor)) == 0){
    end_op();
    return -1;
  }
  iunlockput(ip);
  end_op();
  return 0;
}

uint64
sys_chdir(void)
{
  char path[MAXPATH];
  struct inode *ip;
  struct proc *p = myproc();
  
  begin_op();
  if(argstr(0, path, MAXPATH) < 0 || (ip = namei(path)) == 0){
    end_op();
    return -1;
  }
  ip->iops->ilock(ip);
  if(ip->type != T_DIR){
    iunlockput(ip);
    end_op();
    return -1;
  }
  ip->iops->iunlock(ip);
  iput(p->cwd);
  end_op();
  p->cwd = ip;
  return 0;
}
uint64 sys_execve(void){
// come in like:
// # execve(path, argv, envp)
// where path is stored in a0, argv in a1 and envp in a2
char path[MAXPATH], *argv[MAXARG], *envp[MAXENV];
int i;
uint64 uargv, uarg, uenvp, uenv = 0;

// copy path and argv from user space to kernel space
argaddr(1, &uargv);
if(argstr(0, path, MAXPATH) < 0) {
  return -1;
}
argaddr(2, &uenvp);
memset(envp, 0, sizeof(envp));
for(i=0;; i++){
  if(i >= NELEM(envp)){
    goto badenv;
  }
  if(uenvp)
    if(fetchaddr(uenvp+sizeof(uint64)*i, (uint64*)&uenv) < 0){
      goto badenv;
    }
  if(uenv == 0){
    envp[i] = 0;
    break;
  }
  envp[i] = kalloc();
  if(envp[i] == 0)
    goto badenv;
  if(fetchstr(uenv, envp[i], PGSIZE) < 0)
    goto badenv;
}

memset(argv, 0, sizeof(argv));

for(i=0;; i++){
  if(i >= NELEM(argv)){
    goto bad;
  }
  if(fetchaddr(uargv+sizeof(uint64)*i, (uint64*)&uarg) < 0){
    goto bad;
  }
  if(uarg == 0){
    argv[i] = 0;
    break;
  }
  argv[i] = kalloc();
  if(argv[i] == 0)
    goto bad;
  if(fetchstr(uarg, argv[i], PGSIZE) < 0) 
    goto bad;
}

// now path and argv holds the user's args
int ret = execve(path, argv, envp);

// free the temporary memory then return
for(i = 0; i < NELEM(argv) && argv[i] != 0; i++) {
  kfree(argv[i]);
}

for(i = 0; i < NELEM(envp) && envp[i] != 0; i++) {
  kfree(envp[i]);
}

return ret;

bad:
for(i = 0; i < NELEM(argv) && argv[i] != 0; i++)
  kfree(argv[i]);
return -1;
badenv:
for(i = 0; i < NELEM(envp) && envp[i] != 0; i++)
  kfree(envp[i]);
return -1;
}

uint64
sys_exec(void)
{

  // come in like:
  // # exec(path, argv)
  // where path is stored in a0, argv in a1 
  char path[MAXPATH], *argv[MAXARG];
  int i;
  uint64 uargv, uarg;

  // copy path and argv from user space to kernel space
  argaddr(1, &uargv);
  if(argstr(0, path, MAXPATH) < 0) {
    return -1;
  }

#ifdef __DEBUG_SYS_EXEC
  Log("do sys_exec");
  Log("path = %s", path);
#endif
  memset(argv, 0, sizeof(argv));
  for(i=0;; i++){
    if(i >= NELEM(argv)){
      goto bad;
    }
    if(fetchaddr(uargv+sizeof(uint64)*i, (uint64*)&uarg) < 0){
      goto bad;
    }
	if(uarg == 0){
      argv[i] = 0;
      break;
    }
    argv[i] = kalloc();
    if(argv[i] == 0)
      goto bad;
    if(fetchstr(uarg, argv[i], PGSIZE) < 0)
      goto bad;
  }

  // now path and argv holds the user's args
  int ret = exec(path, argv);

  // free the temporary memory then return
  for(i = 0; i < NELEM(argv) && argv[i] != 0; i++)
    kfree(argv[i]);
#ifdef __DEBUG_SYS_EXEC
  Log("sys_exec done"); 
#endif

  return ret;

 bad:
  for(i = 0; i < NELEM(argv) && argv[i] != 0; i++)
    kfree(argv[i]);
  return -1;
}

uint64
sys_pipe(void)
{
  uint64 fdarray; // user pointer to array of two integers
  struct file *rf, *wf;
  int fd0, fd1;
  struct proc *p = myproc();

  argaddr(0, &fdarray);
  if(pipealloc(&rf, &wf) < 0)
    return -1;
  fd0 = -1;
  if((fd0 = fdalloc(rf)) < 0 || (fd1 = fdalloc(wf)) < 0){
    if(fd0 >= 0)
      p->ofile[fd0] = 0;
    fileclose(rf);
    fileclose(wf);
    return -1;
  }
  if(copyout(p->mm.pagetable, fdarray, (char*)&fd0, sizeof(fd0)) < 0 ||
     copyout(p->mm.pagetable, fdarray+sizeof(fd0), (char *)&fd1, sizeof(fd1)) < 0){
    p->ofile[fd0] = 0;
    p->ofile[fd1] = 0;
    fileclose(rf);
    fileclose(wf);
    return -1;
  }
  return 0;
}


uint64 sys_mount() {
  /**
   * * 功能：挂载文件系统；
* 输入：
    - special: 挂载设备；
    - dir: 挂载点；
    - fstype: 挂载的文件系统类型；
    - flags: 挂载参数；
    - data: 传递给文件系统的字符串参数，可为NULL；
* 返回值：成功返回0，失败返回-1；
```
const char *special, const char *dir, const char *fstype, unsigned long flags, const void *data;
int ret = syscall(SYS_mount, special, dir, fstype, flags, data);
```
   * 
   */

   char devf[MAXPATH];
   char path[MAXPATH];
   char fstype[MAXPATH];
   struct inode *ip, *devi;
 
   if (argstr(0, devf, MAXPATH) < 0 || argstr(1, path, MAXPATH) < 0 || argstr(2, fstype, MAXPATH) < 0) {
     return -1;
   }
 
   if ((ip = namei(path)) == 0 || (devi = namei(devf)) == 0) {
     return -1;
   }
 
   struct vfs_filesystem *fs = getfs(fstype);
 
   if (fs == 0) {
     printf("FS type not found\n");
     return -1;
   }
 
   ip->iops->ilock(ip);
   devi->iops->ilock(devi);
   // we only can mount points over directories nodes
   if (ip->type != T_DIR && ip->ref > 1) {
     ip->iops->iunlock(ip);
     devi->iops->iunlock(devi);
     return -1;
   }
 
   // The device inode should be T_DEV
   if (devi->type != T_DEVICE) {
     ip->iops->iunlock(ip);
     devi->iops->iunlock(devi);
     return -1;
   }
 
   if (bdev_open(devi) != 0) {
     ip->iops->iunlock(ip);
     devi->iops->iunlock(devi);
     return -1;
   }
 
   if (devi->minor == 0 || devi->minor == ROOTDEV) {
     ip->iops->iunlock(ip);
     devi->iops->iunlock(devi);
     return -1;
   }
 
   // Add this to a list to retrieve the Filesystem type to current device
   if (put_vfs_on_list(devi->major, devi->minor, fs) == -1) {
     ip->iops->iunlock(ip);
     devi->iops->iunlock(devi);
     return -1;
   }
 
   int mounted = fs->fsops->mount(devi, ip);
 
   if (mounted != 0) {
     ip->iops->iunlock(ip);
     devi->iops->iunlock(devi);
     return -1;
   }
 
   ip->type = T_MOUNT;
 
   ip->iops->iunlock(ip);
   devi->iops->iunlock(devi);
 
   return 0;
}

// To open console device.
uint64
sys_dev(void)
{
    int fd, omode;
    int major, minor;
    struct file *f;

    argint(0, &omode);
    argint(1, &major);
    argint(2, &minor);

    if (omode & O_CREAT)
    {
      panic("dev file on FAT");
    }

    if (major < 0 || major >= NDEV)
        return -1;

    if ((f = filealloc()) == NULL || (fd = fdalloc(f)) < 0)
    {
        if (f)
            fileclose(f);
        return -1;
    }

    f->type = FD_DEVICE;
    f->off = 0;
    f->major = major;
    if(!(omode & O_WRONLY) )
    SET_READABLE(f->flags);
    if((omode & O_WRONLY) || (omode & O_RDWR))
    SET_WRITABLE(f->flags);
    myproc()->ofile[fd] = f;
    return fd;
}


uint64 sys_openat(void) {
// int openat(int dirfd, const char *pathname, int flags, mode_t mode);
  char path[MAXPATH];
  int dirfd, flags, omode;
  struct file *f;
  struct proc *p = myproc();
  int fd;
  
  argint(0, &dirfd);
  if(argstr(1, path, MAXPATH) < 0)
    return -1;
  argint(2, &flags);
  argint(3, &omode);

  printf("[sys_openat] dirfd = %d, path = %s, flags = %d, omode = %d\n", dirfd, path, flags, omode);

  struct vfs_filesystem *fs = getfs(ROOTFSTYPE);
  if (fs == NULL) {
    printf("FS type not found\n");
    return -1;
  }
  
  if(fs->type == VFS_TYPE_EXT4) {
    const char *dirpath = dirfd == AT_FDCWD ? p->cinfo.path : p->ofile[dirfd]->info.path;
    char abs_path[MAXPATH] = {0};
    get_absolute_path(path, dirpath, abs_path);
    printf("[sys_openat] abs_path = %s\n", abs_path);
    if((f = filealloc()) == NULL || (fd = fdalloc(f)) < 0) {
      if(f)
        fileclose(f);
      return -1;
    }
    f->flags |= flags;
    f->omode = omode;
    strcpy(f->info.path, path);
  
    if(ext4_vfopen(f, path, flags) != EOK) {
      fileclose(f);
      return -1;
    }
    return fd;
  }
  return -1;
}



/**
 * 
 * @brief  places the contents of the symbolic link path in the buffer buf, which has size bufsiz. TODO!!
 * 
 * @return On success, it returns the number of bytes placed in buf. On error, -1 is returned and errno is set to indicate the error.
 */
uint64 sys_readlinkat() {
//  int readlinkat(int dirfd, const char *pathname, char *buf, size_t bufsiz);
  return 0;
}

/**
 * @brief These functions return information about a file.
 * @param dirfd The file descriptor of the directory in which the file is located.
 * @param pathname The name of the file.
 * @param buf A pointer to a stat structure where the information will be stored.
 * @param flags The flags that control the behavior of the function. The only flag that is currently defined is AT_SYMLINK_NOFOLLOW, which prevents following symbolic links.
 * @attention If the pathname given in pathname is relative, then it is interpreted relative to the directory referred to by the file descriptor dirfd (rather than relative to the current working directory of the calling process
 * 
 * 
 * @return On success, fstatat() returns 0. On error, -1 is returned and errno is set to indicate the error. 
 */
uint64 sys_fstatat() {
  // int fstatat(int dirfd, const char *pathname, struct stat *buf, int flags);

  char pathname[MAXPATH];
  int dirfd, flags;
  uint64 statbuf;
  argint(0, &dirfd);
  if (argstr(1, pathname, MAXPATH) < 0) {
      return -1;
  }
  argaddr(2, &statbuf);
  argint(3, &flags);

  struct vfs_filesystem *fs = vfs_resolve_fs(pathname);
  printf("pathname = %s\n", pathname);
  if (fs == NULL) {
      printf("FS type not found\n");
      return ENOENT;
  }

  if(fs->type == VFS_TYPE_EXT4) {
    struct kstat kstat;
    char * dirfd_path = dirfd == AT_FDCWD ? myproc()->cinfo.path : myproc()->ofile[dirfd]->info.path;
    char abs_path[MAXPATH] = {0};
    get_absolute_path(pathname, dirfd_path, abs_path);
    // printf("[sys_fstatat] abs_path = %s\n", abs_path);
    if (ext4_vstat(abs_path, &kstat) < 0) {
      return -1;
    }
    if (copyout(myproc()->mm.pagetable, statbuf, (char *)&kstat, sizeof(kstat)) < 0) {
      return -1;
    }
    return 0;
  }
  printf("sys_fstatat: fs type not supported, type: %d\n", fs->type);
  return -1;
}

/**
 * @brief getcwd - get current working directory
 * 
 * @return        On success, these functions return a pointer to a string
       containing the pathname of the current working directory.  In the
       case of getcwd() and getwd() this is the same value as buf.

       On failure, these functions return NULL, and errno is set to
       indicate the error.  The contents of the array pointed to by buf
       are undefined on error.
 */
uint64 sys_getcwd() {
  // char *getcwd(char *buf, size_t size);
  uint64 addr;
  int size;
  argaddr(0, &addr);
  argint(1, &size);
  // printf("[sys_getcwd] cinfo.path = %s\n", myproc()->cinfo.path);
  if(size < 0 || size > MAXPATH || size < strlen(myproc()->cinfo.path)) {
    return 0;
  }
  if (copyout(myproc()->mm.pagetable, addr, myproc()->cinfo.path, MAXPATH) < 0) {
    return 0;
  }
  return addr;
}

/**
 * @brief    The ioctl() system call manipulates the underlying device
       parameters of special files.  In particular, many operating
       characteristics of character special files (e.g., terminals) may
       be controlled with ioctl() operations.  The argument fd must be an
       open file descriptor.

       The second argument is a device-dependent operation code.  The
       third argument is an untyped pointer to memory.  It's
       traditionally char *argp (from the days before void * was valid
       C), and will be so named for this discussion.

       An ioctl() op has encoded in it whether the argument is an in
       parameter or out parameter, and the size of the argument argp in
       bytes.  Macros and defines used in specifying an ioctl() op are
       located in the file <sys/ioctl.h>. 
 * 
 * @return  Usually, on success zero is returned.  A few ioctl() operations
       use the return value as an output parameter and return a
       nonnegative value on success.  On error, -1 is returned, and errno
       is set to indicate the error.
 */
uint64 sys_ioctl(void) {
  // int ioctl(int fd, unsigned long op, ...);  /* glibc, BSD */
  // int ioctl(int fd, int op, ...);            /* musl, other UNIX */
  struct file *f;
  int fd;
  uint64 op;
  uint64 arg;
  if (argfd(0, &fd, &f) < 0) {
      return -1;
  }
  arglong(1, &op);
  arglong(2, &arg);
  printf("[sys_ioctl] fd = %d, op = 0x%x, arg = 0x%x\n", fd, op, arg);
  return do_ioctl(f, op, arg);
  // return -1;
}

uint64 sys_fcntl(void) {
  // int fcntl(int fd, int cmd, ... /* arg */ );
  struct file *f;
  int fd;
  uint64 cmd;
  uint64 arg;
  if (argfd(0, &fd, &f) < 0) {
      return -1;
  }
  arglong(1, &cmd);
  arglong(2, &arg);
  printf("[sys_fcntl] fd = %d, cmd = %d, arg = %d\n", fd, cmd, arg);
  return do_fcntl(f, cmd, arg);
}