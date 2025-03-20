// Sleeping locks

#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "sleeplock.h"
#include "debug.h"

void
initsleeplock(struct sleeplock *lk, char *name)
{
  initlock(&lk->lk, "sleep lock");
  lk->name = name;
  lk->locked = 0;
  lk->pid = 0;
}

void
acquiresleep(struct sleeplock *lk)
{
  acquire(&lk->lk);

  // sleep时 lk->lk已经被释放
  // 多线程对sleeplock不会因为spinlock而阻塞
  while (lk->locked) {
    // sleep 等待获取锁lk
#ifdef __DEBUG_SLEEPLOCK
  int iswaken = 1;
    if(iswaken)
      Info("acquiresleep %p\n", lk);
      iswaken = 0;
#endif

    thread_sleep(lk, &lk->lk);
  }
#ifdef __DEBUG_SLEEPLOCK
  Info("acuiresleep wakeup %p\n", lk);
  iswaken = 1;
#endif

  // sleeplock 被spinlock保护
  lk->locked = 1;
  lk->pid = myproc()->pid;
  release(&lk->lk);
}

void
releasesleep(struct sleeplock *lk)
{
  acquire(&lk->lk);
  lk->locked = 0;
  lk->pid = 0;
#ifdef __DEBUG_SLEEPLOCK
  Info("releasesleep %p\n", lk);
#endif
  thread_wakeup_chan(lk);
  release(&lk->lk);
}

// check 当前 进程是否持有lk
int
holdingsleep(struct sleeplock *lk)
{
  int r;
  
  acquire(&lk->lk);
  r = lk->locked && (lk->pid == myproc()->pid);
  release(&lk->lk);
  return r;
}



