#include "types.h"
#include "arch.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "proc.h"
#include "xv6fs.h"
#include "sleeplock.h"
#include "file.h"
#include "debug.h"

#define PIPESIZE 512

struct pipe {
  struct spinlock lock;
  char data[PIPESIZE];
  uint nread;     // number of bytes read
  uint nwrite;    // number of bytes written
  int readopen;   // read fd is still open
  int writeopen;  // write fd is still open
};

int
pipealloc(struct file **f0, struct file **f1)
{
  struct pipe *pi;

  pi = 0;
  *f0 = *f1 = 0;
  if((*f0 = filealloc()) == 0 || (*f1 = filealloc()) == 0)
    goto bad;
  if((pi = (struct pipe*)kalloc()) == 0)
    goto bad;
  pi->readopen = 1;
  pi->writeopen = 1;
  pi->nwrite = 0;
  pi->nread = 0;
  initlock(&pi->lock, "pipe");
  (*f0)->type = FD_PIPE;
  // (*f0)->readable = 1;
  // (*f0)->writable = 0;
  SET_READABLE((*f0)->flags);
  UNSET_WRITABLE((*f0)->flags);
  (*f0)->pipe = pi;
  (*f1)->type = FD_PIPE;
  // (*f1)->readable = 0;
  // (*f1)->writable = 1;
  UNSET_READABLE((*f1)->flags);
  SET_WRITABLE((*f1)->flags);
  (*f1)->pipe = pi;
  return 0;

 bad:
  if(pi)
    kfree((char*)pi);
  if(*f0)
    fileclose(*f0, 0);
  if(*f1)
    fileclose(*f1, 0);
  return -1;
}

void
pipeclose(struct pipe *pi, int writable)
{
  acquire(&pi->lock);
  if(writable){
#ifdef __DEBUG_PIPE_CLOSE
    Log("pipeclose: pi %p -> writeopen = 0", pi);
#endif
    pi->writeopen = 0;
    thread_wakeup_chan(&pi->nread);
  } else {
#ifdef __DEBUG_PIPE_CLOSE
    Log("pipeclose: pi %p -> readopen = 0", pi);
#endif
    pi->readopen = 0;
    thread_wakeup_chan(&pi->nwrite);
  }
  if(pi->readopen == 0 && pi->writeopen == 0){
#ifdef __DEBUG_PIPE_CLOSE
    Log("pipeclose: pi %p -> both readopen and writeopen are 0, releasing pipe");
#endif
    release(&pi->lock);
    kfree((char*)pi);
  } else
    release(&pi->lock);
}

int
pipewrite(struct pipe *pi, int user_src, uint64 addr, int n)
{
  int i = 0;
  struct proc *pr = myproc();

  acquire(&pi->lock);
  while(i < n){
    if(pi->readopen == 0 || proc_killed(pr)){
      release(&pi->lock);
      return -1;
    }
    if(pi->nwrite == pi->nread + PIPESIZE){ //DOC: pipewrite-full
      thread_wakeup_chan(&pi->nread);
      thread_sleep(&pi->nwrite, &pi->lock, NULL);
    } else {
      char ch;
      if(either_copyin(&ch, user_src, addr + i, 1) == -1)
        break;
      pi->data[pi->nwrite++ % PIPESIZE] = ch;
      i++;
    }
  }
  thread_wakeup_chan(&pi->nread);
  release(&pi->lock);

  return i;
}

int
piperead(struct pipe *pi, int user_dst, uint64 addr, int n)
{
  int i;
  struct proc *pr = myproc();
  char ch;

  acquire(&pi->lock);
  while(pi->nread == pi->nwrite && pi->writeopen){  //DOC: pipe-empty
    if(proc_killed(pr)){
      release(&pi->lock);
      return -1;
    }
    thread_sleep(&pi->nread, &pi->lock, NULL); //DOC: piperead-sleep
  }
  for(i = 0; i < n; i++){  //DOC: piperead-copy
    if(pi->nread == pi->nwrite)
      break;
    ch = pi->data[pi->nread++ % PIPESIZE];
    if(either_copyout(user_dst, addr + i, &ch, 1) == -1)
      break;
  }
  thread_wakeup_chan(&pi->nwrite);  //DOC: piperead-wakeup
  release(&pi->lock);
  return i;
}
