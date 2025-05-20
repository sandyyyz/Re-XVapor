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
#ifdef __USE_XV6FS
      fs = getfs("xv6fs");
#else
      fs = getfs("ext4");
#endif
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
  if(f->ref < 1)
    panic("filedup");
  f->ref++;
  release(&ftable.lock);
  return f;
}

// Close file f.  (Decrement ref count, close when reaches 0.)
void
fileclose(struct file *f)
{
  struct file ff;

  acquire(&ftable.lock);
  if(f->ref < 1)
    panic("fileclose");
  if(--f->ref > 0){
    release(&ftable.lock);
    return;
  }
  ff = *f;
  f->ref = 0;
  f->flags = 0;
  f->omode = 0;
  f->type = FD_NONE;
  f->fops = 0;
  f->fpos = 0;
  if(f->private_data) {
    f->fops->close(f);
    f->fops->cleansf(f);
  }
  f->private_data = 0;
  memset(&f->info, 0, sizeof(f->info));
  memset(&f->dir, 0, sizeof(f->dir));
  release(&ftable.lock);

  if(ff.type == FD_PIPE){
    pipeclose(ff.pipe, IS_WRITABLE(ff.flags));
  } else if(ff.type == FD_INODE || ff.type == FD_DEVICE){
    begin_op();
    iput(ff.ip);
    end_op();
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

// Read from file f.
// addr is a user virtual address.
int
fileread(struct file *f, uint64 addr, int n)
{
  int r = 0;

  if(!IS_READABLE(f->flags))
    return -1;

  if(f->type == FD_PIPE){
    r = piperead(f->pipe, addr, n);
  } else if(f->type == FD_DEVICE){
    if(f->major < 0 || f->major >= NDEV || !devsw[f->major].read)
      return -1;
    r = devsw[f->major].read(1, addr, n);
  } else if(f->type == FD_INODE){
    f->fops->read(f, 1, addr, f->fpos, n, &r);
  } else {
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
 * @return int return the number of bytes written, -1 if failed 
 */
int filewrite(struct file *f, uint64 addr, int n)
{
  int r, ret = 0;

  // Log("file path = %s", f->info.path);
  // Log("file type = %d", f->type);
  if(!(IS_WRITABLE(f->flags)))
    return -1;

  if(f->type == FD_PIPE){
    ret = pipewrite(f->pipe, addr, n);
  } else if(f->type == FD_DEVICE){
    if(f->major < 0 || f->major >= NDEV || !devsw[f->major].write)
      return -1;
    ret = devsw[f->major].write(1, addr, n);
  } else if(f->type == FD_INODE){
    f->fops->write(f, 1, addr, f->fpos, n, &r);
    if(r > 0)
      ret = r;
    else
      ret = -1;
  } else {
    panic("filewrite");
  }
  return ret;
}

