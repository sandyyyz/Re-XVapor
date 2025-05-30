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
#include "debug.h"
#include "iovec.h"

static void set_omode(struct file *f, int omode);

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

static int fdalloc_spec(struct file *f, int spec_fd) {
  struct proc *p = myproc();
  if (spec_fd < 0 || spec_fd >= NOFILE) {
    return -1;
  }
  if(p->ofile[spec_fd] != 0) {
    fileclose(p->ofile[spec_fd]);
    p->ofile[spec_fd] = 0;
  }
  p->ofile[spec_fd] = f;
  return spec_fd;
}

static void get_abpath_from_dirfd(__nullable const char* path, int dirfd, char* abs_path) {
  struct proc *p = myproc();
  const char *dirpath = dirfd == AT_FDCWD ? p->cinfo.path : p->ofile[dirfd]->info.path;
  get_absolute_path(path, dirpath, abs_path);
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

/**
 * @brief dup3() duplicates the file descriptor oldfd to newfd, closing newfd first if it was open.
       If flags is specified, it is used to set the close-on-exec flag for newfd.
 * 
 * @property int dup3(int oldfd, int newfd, int flags);
 * @return uint64 
 */
uint64 sys_dup3(void) {
  int oldfd, newfd, flags;
  struct file *f;
  if(argfd(0, &oldfd, &f) < 0)
    return -1;
  argint(1, &newfd);
  argint(2, &flags);
  if(oldfd < 0 || oldfd >= NOFILE || newfd < 0 || newfd >= NOFILE)
    return -1;
#ifdef __DEBUG_SYS_DUP3
  Log("sys_dup3: oldfd=%d, newfd=%d, flags=%d", oldfd, newfd, flags);
#endif
  if(fdalloc_spec(f, newfd) < 0)
    return -1;
  if (flags & O_CLOEXEC) {
    f->flags |= O_CLOEXEC; // set the close-on-exec flag
  } else {
    f->flags &= ~O_CLOEXEC; // clear the close-on-exec flag
  }
  f->ref++;
  return newfd;
}
/**
 * @brief ssize_t read(int fd, void buf[.count], size_t count);  
 * 
 * read() attempts to read up to count bytes from file descriptor fd
       into the buffer starting at buf.
 * 
 * @return  On success, the number of bytes read is returned (zero indicates
       end of file), and the file position is advanced by this number.
       It is not an error if this number is smaller than the number of
       bytes requested; this may happen for example because fewer bytes
       are actually available right now (maybe because we were close to
       end-of-file, or because we are reading from a pipe, or from a
       terminal), or because read() was interrupted by a signal.  See
       also NOTES.

       On error, -1 is returned, and errno is set to indicate the error.
       In this case, it is left unspecified whether the file position (if
       any) changes.
 */
uint64 sys_read(void)
{
  struct file *f;
  int n;
  uint64 p;
  int fd;
  argaddr(1, &p);
  argint(2, &n);
  if(argfd(0, &fd, &f) < 0)
    return -1;
#ifdef __DEBUG_SYS_READ
  Log("sys_read: fd=%d, p=%p, n=%d, f->pos %d", fd, p, n, f->fpos);
#endif
  return fileread(f, p, n);
}

uint64
sys_write(void)
{
  struct file *f;
  int n;
  uint64 p;
  int fd;
  argaddr(1, &p);
  argint(2, &n);
  if(argfd(0, &fd, &f) < 0)
    return -1;
#ifdef __DEBUG_SYS_WRITE
  Log("sys_write: fd=%d, p=%p, n=%d, f->pos %d", fd, p, n, f->fpos);
#endif
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
#ifdef __DEBUG_CLOSE
  Log("sys_close: fd=%d, f=%p, ref after close %d", fd, f, f->ref);
#endif
  return 0;
}

static int generic_link(char *oldpath, char *newpath, int flags) {
  struct vfs_filesystem *fs = vfs_resolve_fs(oldpath);
  if (fs == NULL) {
    printf("sys_link: vfs_resolve_fs failed\n");
    return -1;
  }
  if(fs->fsops->link == NULL) {
    printf("sys_link: fsops->link is NULL\n");
    return -1;
  }
  if (fs->fsops->link(oldpath, newpath, flags) < 0) {
    printf("sys_link: fsops->link failed\n");
    return -1;
  }
  return 0;
}

/**
 * @brief link() creates a new link (also known as a hard link) to an
       existing file.

       If newpath exists, it will not be overwritten.

       This new name may be used exactly as the old one for any
       operation; both names refer to the same file (and so have the same
       permissions and ownership) and it is impossible to tell which name
       was the "original".
 * 
  * @property int link(const char *oldpath, const char *newpath);
 * @return  On success, zero is returned.  On error, -1 is returned, and errno
       is set to indicate the error.
 */
uint64 sys_link(void)
{
  char oldpath[MAXPATH], newpath[MAXPATH];
  char oldpath_abs[MAXPATH], newpath_abs[MAXPATH];

  argstr(0, oldpath, MAXPATH);
  argstr(1, newpath, MAXPATH);

  get_abpath_from_dirfd(oldpath, AT_FDCWD, oldpath_abs);
  get_abpath_from_dirfd(newpath, AT_FDCWD, newpath_abs);

  return generic_link(oldpath_abs, newpath_abs, 0);
}

uint64 sys_linkat(void) {
  int olddirfd, newdirfd;
  char oldpath[MAXPATH], newpath[MAXPATH];
  char abs_oldpath[MAXPATH], abs_newpath[MAXPATH];
  int flags;

  argint(0, &olddirfd);
  argstr(1, oldpath, MAXPATH);
  argint(2, &newdirfd);
  argstr(3, newpath, MAXPATH);
  argint(4, &flags);

  get_abpath_from_dirfd(oldpath, olddirfd, abs_oldpath);
  get_abpath_from_dirfd(newpath, newdirfd, abs_newpath);

  return generic_link(abs_oldpath, abs_newpath, flags);
}


static int generic_unlink(char *path, int flags) {
  struct vfs_filesystem *fs = vfs_resolve_fs(path);
  if (fs == NULL) {
    printf("sys_unlink: vfs_resolve_fs failed\n");
    return -1;
  }
  if(fs->fsops->unlink == NULL) {
    printf("sys_unlink: fsops->unlink is NULL\n");
    return -1;
  }
  if (fs->fsops->unlink(path, flags) < 0) {
    printf("sys_unlink: fsops->unlink failed\n");
    return -1;
  }
  return 0;
}

/**
 * @brief delete a name and possibly the file it refers
 * 
 * @property int unlink(const char *pathname);
 * @return On success, zero is returned.  On error, -1 is returned
 */
uint64 sys_unlink(void)
{
  char path[MAXPATH];
  char abs_path[MAXPATH];

  if(argstr(0, path, MAXPATH) < 0) {
    printf("sys_unlink: argstr failed\n");
    return -1;
  }
  get_abpath_from_dirfd(path, AT_FDCWD, abs_path);

  return generic_unlink(abs_path, 0);
}

/**
 * @brief delete a name and possibly the file it refers to.
       
 * @property int unlinkat(int dirfd, const char *pathname, int flags);
 * @return uint64 
 */
uint64 sys_unlinkat(void) {
  int dirfd;
  char path[MAXPATH];
  char abs_path[MAXPATH];
  int flags;

  argint(0, &dirfd);
  if(argstr(1, path, MAXPATH) < 0) {
    printf("sys_unlinkat: argstr failed\n");
    return -1;
  }
  argint(2, &flags);
  
  get_abpath_from_dirfd(path, dirfd, abs_path);
  
  return generic_unlink(abs_path, flags);
}

static int generic_mkdir(char *path, mode_t mode) {
  struct vfs_filesystem *fs = vfs_resolve_fs(path);
  if (fs == NULL) {
    printf("sys_mkdir: vfs_resolve_fs failed\n");
    return -1;
  }
  if(fs->fsops->mkdir == NULL) {
    printf("sys_mkdir: fsops->mkdir is NULL\n");
    return -1;
  }
  if (fs->fsops->mkdir(path, mode) != 0) {
    printf("sys_mkdir: fsops->mkdir failed\n");
    return -1;
  }
#ifdef __DEBUG_GENERIC_MKDIR
  Log("sys_mkdir : path %s successfully created", path);
#endif
  return 0;
}
/**
 * @brief make a directory. The argument mode specifies the mode for the new directory (see
       inode(7)).  It is modified by the process's umask in the usual
       way: in the absence of a default ACL, the mode of the created
       directory is (mode & ~umask & 0777)
  
  @property int mkdir(const char *pathname, mode_t mode);
 * 
 * @return return 0 on success, -1 on error
 */
uint64 sys_mkdir(void)
{
  char path[MAXPATH];
  mode_t mode;

  if(argstr(0, path, MAXPATH) < 0) {
    printf("sys_mkdir: argstr failed\n");
    return -1;
  }
  arguint32(1, &mode);
  return generic_mkdir(path, mode);
}

/**
 * @brief create a directory
 * 
 * @property int mkdirat(int dirfd, const char *pathname, mode_t mode);
 * @return return 0 on success, -1 on error
 */
uint64 sys_mkdirat(void) {
  char path[MAXPATH];
  char abs_path[MAXPATH];
  int dirfd;
  mode_t mode;

  if(argstr(1, path, MAXPATH) < 0) {
    printf("sys_mkdirat: argstr failed\n");
    return -1;
  }
  argint(0, &dirfd);
  arguint32(2, &mode);
  get_abpath_from_dirfd(path, dirfd, abs_path);
  printf("sys_mkdirat: abs_path = %s\n", abs_path);
  return generic_mkdir(abs_path, mode);
}


/**
 * @brief generic_mknod - create a filesystem node
 * 
 * @param path absolute path to the file to be created
 * @param mode the file type and permissions
 * @param dev device number, including major and minor numbers
 * @return 0 on success, -1 on error
 */
static uint64 generic_mknod(char *path, mode_t mode, dev_t dev) {
  struct vfs_filesystem *fs = vfs_resolve_fs(path);
  if (fs == NULL) {
    printf("sys_mknod: vfs_resolve_fs failed\n");
    return -1;
  }
  if(fs->fsops->mknod == NULL) {
    printf("sys_mknod: fsops->mknod is NULL\n");
    return -1;
  }
  if (fs->fsops->mknod(path, mode, dev) < 0) {
    printf("sys_mknod: fsops->mknod failed\n");
    return -1;
  }
  Log("sys_mknod : path %s successfully created", path);
  return 0;
}
/**
 * @brief  The system call mknod() creates a filesystem node (file, device
       special file, or named pipe) named pathname, with attributes
       specified by mode and dev.
 * 
 * @property int mknod(const char *pathname, mode_t mode, dev_t dev);
 * @param pathname the path to the file to be created
 * @param mode the file type and permissions
 * @param dev the device number, including major and minor numbers
 * @return mknod() and mknodat() return zero on success.  On error, -1 is
       returned and errno is set to indicate the error.
 */
uint64 sys_mknod(void)
{

  mode_t mode;
  dev_t dev;
  char path[MAXPATH];
  arguint32(1, &mode);
  arguint32(2, &dev);
  if(argstr(0, path, MAXPATH) < 0) {
    printf("sys_mknod: argstr failed\n");
    return -1;
  }
  return generic_mknod(path, mode, dev);
}

/**
 * @brief create a filesystem node
 * 
 * @property int mknodat(int dirfd, const char *pathname, mode_t mode, dev_t dev);
 * @return return 0 on success, -1 on error
 */
uint64 sys_mknodat(void) {
  int dirfd;
  char path[MAXPATH];
  mode_t mode;
  dev_t dev;
  char abs_path[MAXPATH];

  argint(0, &dirfd);
  arguint32(2, &mode);
  arguint32(3, &dev);
  if(argstr(1, path, MAXPATH) < 0) {
    printf("sys_mknodat: argstr failed\n");
    return -1;
  }
  get_abpath_from_dirfd(path, dirfd, abs_path);
  printf("sys_mknodat: abs_path = %s\n", abs_path);

  return generic_mknod(abs_path, mode, dev);
}

/**
 * @brief chdir() changes the current working directory of the calling
       process to the directory specified in path.

 * @property int chdir(const char *path);
 * @return  On success, zero is returned.  On error, -1 is returned, and errno
       is set to indicate the error.
 */
uint64 sys_chdir(void)
{
  char path[MAXPATH];
  struct proc *p = myproc();
  char *cwd = p->cinfo.path;
  char abs_path[MAXPATH];
  struct vfs_filesystem *fs = NULL;
  if(argstr(0, path, MAXPATH) < 0)
    return -1;
  get_absolute_path(path, cwd, abs_path);
  fs = vfs_resolve_fs(abs_path);

  if(strlen(abs_path) >= MAXPATH) {
    printf("sys_chdir: path too long\n");
    return -1;
  }
  if (fs == NULL) {
    printf("sys_chdir: vfs_resolve_fs failed\n");
    return -1;
  }
  if(fs->fsops->isdir == NULL) {
    printf("sys_chdir: fsops->isdir is NULL\n");
    return -1;
  }
  if (!fs->fsops->isdir(abs_path)) {
    printf("sys_chdir: fsops->isdir failed\n");
    return -1;
  }

  // check over
  strncpy(cwd, abs_path, MAXPATH);
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

  //  char devf[MAXPATH];
  //  char path[MAXPATH];
  //  char fstype[MAXPATH];
  //  struct inode *ip, *devi;
 
  //  if (argstr(0, devf, MAXPATH) < 0 || argstr(1, path, MAXPATH) < 0 || argstr(2, fstype, MAXPATH) < 0) {
  //    return -1;
  //  }
 
  //  if ((ip = namei(path)) == 0 || (devi = namei(devf)) == 0) {
  //    return -1;
  //  }
 
  //  struct vfs_filesystem *fs = getfs(fstype);
 
  //  if (fs == 0) {
  //    printf("FS type not found\n");
  //    return -1;
  //  }
 
  //  ip->iops->ilock(ip);
  //  devi->iops->ilock(devi);
  //  // we only can mount points over directories nodes
  //  if (ip->type != T_DIR && ip->ref > 1) {
  //    ip->iops->iunlock(ip);
  //    devi->iops->iunlock(devi);
  //    return -1;
  //  }
 
  //  // The device inode should be T_DEV
  //  if (devi->type != T_DEVICE) {
  //    ip->iops->iunlock(ip);
  //    devi->iops->iunlock(devi);
  //    return -1;
  //  }
 
  //  if (bdev_open(devi) != 0) {
  //    ip->iops->iunlock(ip);
  //    devi->iops->iunlock(devi);
  //    return -1;
  //  }
 
  //  if (devi->minor == 0 || devi->minor == ROOTDEV) {
  //    ip->iops->iunlock(ip);
  //    devi->iops->iunlock(devi);
  //    return -1;
  //  }
 
  //  // Add this to a list to retrieve the Filesystem type to current device
  //  if (put_vfs_on_list(devi->major, devi->minor, fs) == -1) {
  //    ip->iops->iunlock(ip);
  //    devi->iops->iunlock(devi);
  //    return -1;
  //  }
 
  //  int mounted = fs->fsops->mount(devi, ip);
 
  //  if (mounted != 0) {
  //    ip->iops->iunlock(ip);
  //    devi->iops->iunlock(devi);
  //    return -1;
  //  }
 
  //  ip->type = T_MOUNT;
 
  //  ip->iops->iunlock(ip);
  //  devi->iops->iunlock(devi);
 
   return 0;
}

// TODO
uint64 sys_umount2() {
  return 0;
}

static void set_omode(struct file *f, int omode) {
  if(!(omode & O_WRONLY) )
  SET_READABLE(f->flags);
  if((omode & O_WRONLY) || (omode & O_RDWR))
  SET_WRITABLE(f->flags);
};

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
    set_omode(f, omode);
    myproc()->ofile[fd] = f;
    return fd;
}


/**
 * @brief generic_open - open a file in the filesystem
 * 
 * @param path absolute path to the file to be opened
 * @param flags flags for opening the file
 * @param omode open mode
 * @return file descriptor on success, -1 on error
 */
static uint64 generic_open(char *path, int flags, int omode) {
  struct vfs_filesystem *fs = vfs_resolve_fs(path);
  struct file *f;
  int fd;
  int r = -1;
  if((f = filealloc()) == NULL || (fd = fdalloc(f)) < 0) {
    if(f)
      fileclose(f);
    return -1;
  }
  r = fd;
  if (fs == NULL) {
    printf("FS type not found\n");
    return -1;
  }
  if(fs->fops->open == NULL) {
    printf("fsops->open is NULL\n");
    return -1;
  }
  
  f->flags |= flags;
  f->fops = fs->fops;
  set_omode(f, omode);
  strcpy(f->info.path, path);
  if (fs->fops->open(f, path, flags) < 0) {
    fileclose(f);
    myproc()->ofile[fd] = 0;
    printf("fsops->open failed, path %s\n", path);
    return -1;
  }
  #ifdef __DEBUG_GOPEN
  Log("generic_open : path %s successfully opened, type = %d", path, f->type);
  #endif

  return r;
}
uint64 sys_openat(void) {
// int openat(int dirfd, const char *pathname, int flags, mode_t mode);
  char path[MAXPATH];
  char abs_path[MAXPATH];
  int dirfd, flags, omode;
  int r = 0;
  argint(0, &dirfd);
  if(argstr(1, path, MAXPATH) < 0)
    return -1;
  argint(2, &flags);
  argint(3, &omode);

  // printf("[sys_openat] dirfd = %d, path = %s, flags = %d, omode = %d\n", dirfd, path, flags, omode);
  get_abpath_from_dirfd(path, dirfd, abs_path);
#ifdef __DEBUG_SYS_OPENAT
  printf("[sys_openat] abs_path = %s\n", abs_path);
#endif
  if((r = generic_open(abs_path, flags, omode)) < 0) {
    printf("[sys_openat] generic_open failed, abs_path = %s\n", abs_path);
    return -1;
  }

#ifdef __DEBUG_SYS_OPENAT
  Log("[sys_openat] generic_open success, fd = %d, path = %s", r, abs_path);
#endif
  return r;
}

uint64
sys_open(void)
{
  char path[MAXPATH];
  int flags, omode;
  int r;

  if(argstr(0, path, MAXPATH) < 0)
    return -1;
  argint(1, &flags);
  argint(2, &omode);
  // printf("[sys_open] path = %s, flags = %d, omode = %d\n", path, flags, omode);
  if((r = generic_open(path, flags, omode)) < 0) {
    return -1;
  }
  return r;
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


uint64 generic_fstat(char *path, __kernel_space struct kstat *buf) {
  struct vfs_filesystem *fs = vfs_resolve_fs(path);
  int r = 0;
  #ifdef __DEBUG_GENERIC_FSTAT
  printf("[generic_fstat] pathname = %s\n", path);
  #endif
  if (fs == NULL) {
      printf("FS type not found\n");
      return -1;
  }
  if(fs->fsops->fstat == NULL) {
      printf("fsops->fstat is NULL\n");
      return -1;
  }
  if ((r = fs->fsops->fstat(path, buf)) != 0) {
      printf("fsops->fstat failed, r = %d\n", r);
      return -1;
  }
  #ifdef __DEBUG_GENERIC_FSTAT
  Log("sys_fstat : path %s successfully fstat", path);
  #endif
  return r;
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
  char abs_path[MAXPATH] = {0};

  argint(0, &dirfd);
  if (argstr(1, pathname, MAXPATH) < 0) {
      return -1;
  }
  argaddr(2, &statbuf);
  argint(3, &flags);

  get_abpath_from_dirfd(pathname, dirfd, abs_path);
#ifdef __DEBUG_SYS_FSTATAT
  printf("[sys_fstatat] abs_path = %s\n", abs_path);
#endif
  struct kstat kbuf;
  if(generic_fstat(abs_path, &kbuf) != 0) {
      printf("[sys_fstatat] generic_fstat failed\n");
      return -1;
  }
  if (copyout(myproc()->mm.pagetable, statbuf, (char *)&kbuf, sizeof(struct stat)) < 0) {
      printf("[sys_fstatat] copyout failed\n");
      return -1;
  }
  return 0;
}

uint64
sys_fstat(void)
{
  struct file *f;
  uint64 st; // user pointer to struct stat
  struct kstat kbuf;
  argaddr(1, &st);
  if(argfd(0, 0, &f) < 0)
    return -1;
  if(generic_fstat(f->info.path, &kbuf) != 0) {
    printf("[sys_fstat] generic_fstat failed\n");
    return -1;
  }
  if (copyout(myproc()->mm.pagetable, st, (char *)&kbuf, sizeof(struct stat)) < 0) {
    printf("[sys_fstat] copyout failed\n");
    return -1;
  }
  return 0;
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
#ifdef __DEBUG_SYS_GETCWD
  Log("[sys_getcwd] addr : %p, size %d, cinfo.path = %s", addr, size, myproc()->cinfo.path);
#endif
  if(size < 0 || size > MAXPATH || size < strlen(myproc()->cinfo.path)) {
    return 0;
  }
  if (copyout(myproc()->mm.pagetable, addr, myproc()->cinfo.path, size) < 0) {
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
  // printf("[sys_ioctl] fd = %d, op = 0x%x, arg = 0x%x\n", fd, op, arg);
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
#ifdef __DEBUG_SYS_FCNTL
  printf("[sys_fcntl] fd = %d, cmd = %d, arg = %d\n", fd, cmd, arg);
#endif
  return do_fcntl(f, cmd, arg);
}

/**
 * @brief getdents - read several directory entries from a directory
 * 
 * @property int getdents(unsigned int fd, struct linux_dirent *dirp,unsigned int count);
 * @return 
 *     On success, the number of bytes read is returned (zero indicates
       end of file), and the file position is advanced by this number.
       It is not an error if this number is smaller than the number of
       bytes requested; this may happen for example because fewer bytes
       are actually available right now (maybe because we were close to
       end-of-file, or because we are reading from a pipe, or from a
       terminal), or because read() was interrupted by a signal.  See
       also NOTES.

       On success, the number of bytes read is returned. On end of directory, 0 is returned. On error, -1 is returned, and errno is set appropriately.
       On error, -1 is returned, and errno is set to indicate the error.
 */
uint64 sys_getdents64(void) {
  // int getdents(unsigned int fd, struct linux_dirent *dirp,unsigned int count);  
  struct file *f;
  uint64 ubuf;
  uint64 count;

  argaddr(1, &ubuf);
  arguint64(2, &count);
  if(argfd(0, 0, &f) < 0)
    return -1;
  
  /*

  here are the real implementations
  because of lacking a memmory allocator that can offer a buffer larger than a page,
  so the `kbuf` is not always allocated successfully.
  so we reuse a page buffer to temporarily store the data right now.
  However, this approach will make the interface non-standard, so it is listed separately.

  real implementation:
  
  int r;
  char *kbuf;
  if(f->fops->getdents == NULL) {
    printf("sys_getdents64: fops->getdents is NULL\n");
    return -1;
  }
  if((kbuf = kmalloc(count)) == NULL) {
    printf("sys_getdents64: kmalloc failed, count %d\n", count);
    return -1;
  }
  if((r = f->fops->getdents(f, (struct linux_dirent64 *)kbuf, count)) < 0) {
    kfree(kbuf);
    printf("sys_getdents64: fops->getdents failed\n");
    return -1;
  }
  if(copyout(myproc()->mm.pagetable, ubuf, kbuf, r) < 0) {
    kfree(kbuf);
    printf("sys_getdents64: copyout failed\n");
    return -1;
  }
  kfree(kbuf);
  return r;

  real implement after realize a buddy system (or allocator that can offer a buffer lager than a page */


  /* temporary impelementation */


  return ext4_temp_vgentdents(f, (struct linux_dirent64*)ubuf, count);
  
}

int generic_writev(struct file *f, uint64 iov, int iovcnt) {
  int wcnt = 0;
  char *kvecs = NULL;
  if((kvecs = kmalloc(iovcnt * sizeof(struct iovec))) == NULL) {
    printf("generic_writev: kmalloc failed\n");
    return -1;
  }
  if(either_copyin(kvecs, 1, iov, iovcnt * sizeof(struct iovec)) < 0) {
    printf("generic_writev: either_copyin failed\n");
    goto bad;
  }
  if(f->type == FD_DIR) {
    printf("generic_writev: f->type is T_DIR\n");
    goto bad;
  } else if(f->type == FD_DEVICE || f->type == FD_PIPE) {
    struct iovec *kvec = (struct iovec *)kvecs;
    int wc = 0;
    for(int i = 0; i < iovcnt; i++)  {
#ifdef __DEBUG_GENERIC_WRITEV
      Log("generic_writev: i = %d, kvec[i].iov_base = %p, kvec[i].iov_len = %d", i, kvec[i].iov_base, kvec[i].iov_len);
#endif
      if(kvec[i].iov_len <= 0 || kvec[i].iov_base == 0) {
        continue;
      }
      if((wc = filewrite(f, (uint64)kvec[i].iov_base, kvec[i].iov_len)) < 0) {
        printf("generic_writev: filewrite failed\n");
        goto bad;
      }
      wcnt += wc;
      kvec++;
    }
  goto out;
  }

  // inode-file writev
  if(f->fops == NULL) {
    printf("generic_writev: fops is NULL\n");
    goto bad;
  }
  if(f->fops->writev == NULL) {
    printf("generic_writev: fops->writev is NULL\n");
    goto bad;
  }
  if(f->fops->writev(f, 1, (uint64)kvecs, iovcnt, &wcnt) != EOK) {
    printf("generic_writev: fops->writev failed\n");
    goto bad;
  }
  if(wcnt < 0) {
    printf("generic_writev: wcnt < 0\n");
    goto bad;
  }
out:
  kfree(kvecs);
  return wcnt;
bad:
  kfree(kvecs);
  return -1;
}
/**
 * @brief The writev() system call writes iovcnt buffers of data described by iov to the file associated with the file descriptor fd ("gather output").
 * 
 * 
 * @return On success, writev() and pwritev() return the number of bytes written. On error, -1 is returned, and errno is set appropriately. 
 */
uint64 sys_writev(void) {
  // ssize_t writev(int fd, const struct iovec *iov, int iovcnt);
  struct file *f;
  uint64 iov;
  int iovcnt;
  int r = 0;
  int fd;
  argaddr(1, &iov);
  argint(2, &iovcnt);
  if(argfd(0, &fd, &f) < 0)
    return -1;
#ifdef __DEBUG_SYS_WRITEV
  Log("sys_writev : fd %d, f %p, iov %p, iovcnt %d", fd, f, (void *)iov, iovcnt);
#endif
  if((r = generic_writev(f, iov, iovcnt)) < 0) {
    printf("sys_writev: generic_writev failed\n");
    return -1;
  }
#ifdef __DEBUG_SYS_WRITEV
  Log("sys_writev : f %p successfully writev, r = %d", f, r);
#endif
  return r;
}


/**
 * @brief        faccessat — determine accessibility of a file relative to
       directory file descriptor
 * @property int faccessat(int fd, const char *path, int amode, int flag);
 * @return    Upon successful completion, these functions shall return 0.
       Otherwise, these functions shall return -1 and set errno to
       indicate the error.
 */
uint64 sys_faccessat(void) {
  int dirfd;
  int amode, flag;
  char path[MAXPATH], abs_path[MAXPATH];
  struct vfs_filesystem *fs = NULL;
  argint(0, &dirfd);
  if(argstr(1, path, MAXPATH) < 0) {
    printf("sys_faccessat: argstr failed\n");
    return -1;
  }
  argint(2, &amode);
  argint(3, &flag);
  get_abpath_from_dirfd(path, dirfd, abs_path);
#ifdef __DEBUG_SYS_FACCESSAT
  printf("sys_faccessat: abs_path = %s, amode = %d, flag = %d\n", abs_path, amode, flag);
#endif
  fs = vfs_resolve_fs(abs_path);
  if (fs == NULL) {
    printf("sys_faccessat: vfs_resolve_fs failed\n");
    return -1;
  }
  if(fs->fsops->faccess == NULL) {
    printf("sys_faccessat: fsops->faccessat is NULL\n");
    return -1;
  }
  if (fs->fsops->faccess(abs_path, amode, flag) < 0) {
    printf("sys_faccessat: fsops->faccessat failed\n");
    return -1;
  }
#ifdef __DEBUG_SYS_FACCESSAT
  Log("sys_faccessat: abs_path %s successfully accessed", abs_path);
#endif
  return 0;
}

static int file_exist(const char *path) {
  struct vfs_filesystem *fs = vfs_resolve_fs(path);
  if (fs == NULL) {
    printf("file_exsit: vfs_resolve_fs failed\n");
    return 0;
  }
  if(fs->fsops->file_exist == NULL) {
    printf("file_exsit: fsops->file_exsit is NULL\n");
    return 0;
  }
  return fs->fsops->file_exist(path);
}

int generic_utimensat(int dirfd, __nullable char *pathname, __nullable struct timespec *times, int flags) {
  char abs_path[MAXPATH];
  struct vfs_filesystem *fs = NULL;

  if(flags & ~AT_SYMLINK_NOFOLLOW) {
    printf("generic_utimensat: flags & ~AT_SYMLINK_NOFOLLOW is not supported\n");
    return -1;
  }
  get_abpath_from_dirfd(pathname, dirfd, abs_path);
  
#ifdef __DEBUG_GENERIC_UTIMENSAT
  printf("[generic_utimensat] dirfd = %d, pathname = %s, abs_path = %s, flags = %d\n", dirfd, pathname ? pathname : "NULL", abs_path, flags);
#endif
  if(!file_exist(abs_path)) {
    // printf("generic_utimensat: file %s does not exist\n", abs_path);
    // return -1;

    // or create a new file if it does not exist?
    int flags = O_CREAT | O_RDWR;
    int omode = 0644; // default mode
    int fd = generic_open(abs_path, flags, omode);
    if(fd < 0) {
      printf("generic_utimensat: generic_open failed, abs_path = %s\n", abs_path);
      return -1;
    }
    // close the file descriptor
    struct file *f = myproc()->ofile[fd];
    fileclose(f);
    myproc()->ofile[fd] = NULL; // clear the file descriptor
    // now we can set the timestamps
  }

  fs = vfs_resolve_fs(abs_path);
  if (fs == NULL) {
    printf("generic_utimensat: vfs_resolve_fs failed\n");
    return -1;
  }
  if(fs->fsops->utimens == NULL) {
    printf("generic_utimensat: fsops->utimensat is NULL\n");
    return -1;
  }
  if (fs->fsops->utimens(abs_path, times) < 0) {
    printf("generic_utimensat: fsops->utimensat failed\n");
    return -1;
  }
#ifdef __DEBUG_GENERIC_UTIMENSAT
  Log("generic_utimensat: abs_path %s successfully utimensat", abs_path);
#endif
  return 0;
}
/**
 * @brief  utimensat() and futimens() update the timestamps of a file with
       nanosecond precision.
    *
       times: times[0] specifies the new "last access time" (atime);
       times[1] specifies the new "last modification time" (mtime). 
    *
    If the tv_nsec field of one of the timespec structures has the
       special value UTIME_NOW, then the corresponding file timestamp is
       set to the current time.  If the tv_nsec field of one of the
       timespec structures has the special value UTIME_OMIT, then the
       corresponding file timestamp is left unchanged.  In both of these
       cases, the value of the corresponding tv_sec field is ignored.

       The flags argument is a bit mask created by ORing together zero or
       more of the following values defined in <fcntl.h>:

       AT_EMPTY_PATH (since Linux 5.8)
              If pathname is an empty string, operate on the file
              referred to by dirfd (which may have been obtained using
              the open(2) O_PATH flag).  In this case, dirfd can refer to
              any type of file, not just a directory.  If dirfd is
              AT_FDCWD, the call operates on the current working
              directory.  This flag is Linux-specific; define _GNU_SOURCE
              to obtain its definition.

       AT_SYMLINK_NOFOLLOW
              If pathname specifies a symbolic link, then update the
              timestamps of the link, rather than the file to which it
              refers.
 * 
 * 
 * @property        int utimensat(int dirfd, const char *pathname,
                     const struct timespec times[_Nullable 2], int flags);
 * @return  On success, utimensat() and futimens() return 0.  On error, -1 is
       returned and errno is set to indicate the error.
 */
uint64 sys_utimensat(void) {
  int dirfd;
  char path[MAXPATH];
  uint64 path_addr;
  uint64 times_addr;
  struct timespec times[2];
  int flags;

  argint(0, &dirfd);
  argaddr(1, &path_addr);
  if(path_addr == 0 || argstr(1, path, MAXPATH) < 0) {
      path[0] = '\0'; // empty path
  }
  argaddr(2, &times_addr);
  argint(3, &flags);
  
  if(times_addr) {
    if(copyin(myproc()->mm.pagetable, (char *)&times, times_addr, sizeof(times)) < 0) {
      printf("sys_utimensat: copyin failed\n");
      return -1;
    }
    if(times[0].tv_nsec == UTIME_OMIT && times[1].tv_nsec == UTIME_OMIT) {
      // both times are omitted, so we don't need to update the timestamps
      return 0;
    }
  }
  return generic_utimensat(dirfd, path[0] ? path : NULL , times_addr ? times : NULL, flags);
}

/**
 * @brief  sendfile() copies data between one file descriptor and another.
       Because this copying is done within the kernel, sendfile() is more
       efficient than the combination of read(2) and write(2), which
       would require transferring data to and from user space.


       If offset is not NULL, then it points to a variable holding the
       file offset from which sendfile() will start reading data from
       in_fd.  When sendfile() returns, this variable will be set to the
       offset of the byte following the last byte that was read.  If
       offset is not NULL, then sendfile() does not modify the file
       offset of in_fd; otherwise the file offset is adjusted to reflect
       the number of bytes read from in_fd.

       If offset is NULL, then data will be read from in_fd starting at
       the file offset, and the file offset will be updated by the call.

       count is the number of bytes to copy between the file descriptors.
 * 
 * @property ssize_t sendfile(int out_fd, int in_fd, off_t *_Nullable offset,
                        size_t count);
 * @return If the transfer was successful, the number of bytes written to
       out_fd is returned.  Note that a successful call to sendfile() may
       write fewer bytes than requested; the caller should be prepared to
       retry the call if there were unsent bytes.  See also NOTES.

       On error, -1 is returned, and errno is set to indicate the error.
 */
uint64 sys_sendfile(void) {
  int out_fd, in_fd;
  uint64 offset_addr;
  uint64 count;

  /**
   *    in_fd should be a file descriptor opened for reading and out_fd
        should be a descriptor opened for writing.
   */

  argint(0, &out_fd);
  argint(1, &in_fd);
  argaddr(2, &offset_addr);
  arguint64(3, &count);
#ifdef __DEBUG_SYS_SENDFILE
  printf("[sys_sendfile] out_fd = %d, in_fd = %d, offset_addr = %p, count = %d\n", out_fd, in_fd, (void *)offset_addr, count);
#endif
  if(out_fd < 0 || in_fd < 0 || count <= 0) {
    printf("[sys_sendfile] invalid arguments, out_fd = %d, in_fd = %d, count = %d\n", out_fd, in_fd, count);
    return -1;
  }
  
  struct file *out_f = myproc()->ofile[out_fd];
  struct file *in_f = myproc()->ofile[in_fd];
  if(out_f == NULL || in_f == NULL) {
    printf("[sys_sendfile] out_f or in_f is NULL\n");
    return -1;
  }
  
  // return do_sendfile(out_f, in_f, offset_addr, count);
  return -1;
}