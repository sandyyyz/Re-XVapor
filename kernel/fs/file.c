//
// Support functions for system calls that involve file descriptors.
//

#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "xv6fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "stat.h"
#include "proc.h"
#include "debug.h"
#include "thread.h"
#include "proc.h"

struct devsw devsw[NDEV];
struct {
  struct spinlock lock;
  struct file file[NFILE];
} ftable;

void
fileinit(void)
{
  initlock(&ftable.lock, "ftable");
}

/**
 * @brief allocate a file structure, not including the underlay filesystem-specific file structure
 * 
 * @return struct file* 
 */
struct file* filealloc(void)
{
  struct file *f;
  struct vfs_filesystem *fs;
  acquire(&ftable.lock);
  for(f = ftable.file; f < ftable.file + NFILE; f++){
    if(f->ref == 0){

      // TODO: maybe change to dynamicly get fs 
      fs = getfs("ext4");

      if(!fs) {
        release(&ftable.lock);
        panic("filealloc: no fs");
        return 0;
      }
      f->ref = 1;
      f->fops = fs->fops;
      f->private_data = 0;
      f->flags = 0;
      f->omode = 0;
      f->info.fs = fs;
      f->fpos = 0;
      f->istmp = 0;
      release(&ftable.lock);
      return f;
    }
  }
  release(&ftable.lock);
  return 0;
}

// Increment ref count for file f.
struct file*
filedup(struct file *f)
{
  acquire(&ftable.lock);
  if(f->ref < 1){
    printf("filedup: ref count < 1\n");
    printf("filedup: file info: type=%d, flags=%d, omode=%d, fpos= %d ref = %d\n", 
           f->type, f->flags, f->omode, f->fpos, f->ref);
    printf("path %s\n", f->info.path);
    panic("filedup");
  }
  f->ref++;
  release(&ftable.lock);
  return f;
}

/**
 * @brief close a file
 * 
 * @param f file pointer
 * @param drop_ofile_cnt if set to 1, will drop the ofile_cnt of the process, otherwise will not drop it. 
 * This parameter is nassary because it may be called before a call to fdalloc, or after a failed fdalloc, which will not increase the ofile_cnt. 
 */
void fileclose(struct file *f, int drop_ofile_cnt)
{
  struct file ff;
  struct proc *p = myproc();
  if(drop_ofile_cnt) {
    p->ofile_cnt--;
    // Log("--ofile_cnt %d", p->ofile_cnt);;
  }
  if(p->ofile_cnt < 0)
    panic("fileclose: ofile_cnt < 0");
#ifdef __DEBUG_FILE_CLOSE
  Log("fileclose : f %p ref %d", f, f->ref);
#endif
  acquire(&ftable.lock);
  if(f->ref < 1)
    panic("fileclose");
  if(--f->ref > 0){
    release(&ftable.lock);
    return;
  }
  if(f->private_data) {
    f->fops->close(f);
    if(f->istmp)
      f->info.fs->fsops->unlink(f->info.path, 0);
    f->fops->cleansf(f);
  }
  ff = *f;
  f->ref = 0;
  f->flags = 0;
  f->omode = 0;
  f->type = FD_NONE;
  f->fops = 0;
  f->fpos = 0;
  f->private_data = 0;
  memset(&f->info, 0, sizeof(f->info));
  memset(&f->dir, 0, sizeof(f->dir));
  release(&ftable.lock);

  if(ff.type == FD_PIPE){
    pipeclose(ff.pipe, IS_WRITABLE(ff.flags));
  } else if(ff.type == FD_INODE || ff.type == FD_DEVICE){ 
  }
}

// Get metadata about file f.
// addr is a user virtual address, pointing to a struct stat.
int
filestat(struct file *f, uint64 addr)
{
  struct proc *p = myproc();
  struct stat st;
  
  if(f->type == FD_INODE || f->type == FD_DEVICE){
    f->ip->iops->ilock(f->ip);
    f->ip->iops->stati(f->ip, &st);
    f->ip->iops->iunlock(f->ip);
    if(copyout(p->mm.pagetable, addr, (char *)&st, sizeof(st)) < 0)
      return -1;
    return 0;
  }
  return -1;
}


int fileread(struct file *f, int user_dst, uint64 addr, int n, int off)
{
  int r = 0;

  if(!IS_READABLE(f->flags))
    return -1;

  if(f->type == FD_PIPE){
    r = piperead(f->pipe, user_dst, addr, n);
  } else if(f->type == FD_DEVICE){
    if(f->major < 0 || f->major >= NDEV || !devsw[f->major].read)
      return -1;
    r = devsw[f->major].read(1, addr, n);
  } else if(f->type == FD_INODE){
    f->fops->read(f, user_dst, addr, off, n, &r);
  } else {
    Log("file %p type = %d", f, f->type);
    panic("fileread");
  }

  return r;
}


/**
 * @brief write a file
 * 
 * @param f file pointer 
 * @param addr virtual address
 * @param n length
 * @param off offset in the file to write to. if you don't want to write to a specific offset, set it to fp->fpos. 
 * only used for FD_INODE type file
 * @return int return the number of bytes written, -1 if failed 
 */
int filewrite(struct file *f, int user_src, uint64 addr, int n, int off)
{
  int r, ret = 0;

  // Log("file path = %s", f->info.path);
  // Log("file type = %d", f->type);
  if(!(IS_WRITABLE(f->flags)))
    return -1;

  if(f->type == FD_PIPE){
    ret = pipewrite(f->pipe, user_src, addr, n);
  } else if(f->type == FD_DEVICE){
    if(f->major < 0 || f->major >= NDEV || !devsw[f->major].write)
      return -1;
    ret = devsw[f->major].write(user_src, addr, n);
  } else if(f->type == FD_INODE){
    f->fops->write(f, user_src, addr, off, n, &r);
    if(r > 0)
      ret = r;
    else
      ret = -1;
  } else {
    panic("filewrite");
  }
  return ret;
  
}

int is_exc_rcfile(struct proc *p) {
  return p->ofile_cnt >= p->rlim[RLIMIT_NOFILE].rlim_cur || p->ofile_cnt >= NOFILE;
}