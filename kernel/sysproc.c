#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "timer.h"


uint64
sys_nanosleep(void)
{
    uint rticks;
    uint ticks0;
    uint64 addr0, addr1;
    struct proc *p = myproc();
    argaddr(0, &addr0);
    argaddr(1, &addr1);
    struct timespec req, rem;
    if (copyin(p->mm.pagetable, (char *)&req, addr0, sizeof(struct timespec)) < 0)
        return -1;
    if (copyin(p->mm.pagetable, (char *)&rem, addr1, sizeof(struct timespec)) < 0)
        return -1;

    rticks = req.tv_sec * TICKS_PER_SECOND + req.tv_nsec * TICKS_PER_SECOND / 1000000000;
    acquire(&tickslock);
    ticks0 = ticks;

    while (ticks - ticks0 < rticks)
    {
        if (killed(p))
        {
            rem.tv_sec = (ticks - ticks0) / TICKS_PER_SECOND;
            rem.tv_nsec = ((ticks - ticks0) % TICKS_PER_SECOND) * 1000000000 / TICKS_PER_SECOND;
            copyout(p->mm.pagetable, addr1, (char *)&rem, sizeof(rem));
            release(&tickslock);
            return -1;
        }
        thread_sleep(&ticks, &tickslock);
    }
    release(&tickslock);
    return 0;
}


uint64
sys_getppid(void) {
  struct proc *p = myproc();
  if(p->parent) return p->parent->pid;
  else return -1;
}
uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  thread_exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64 sys_clone(void) {
  //TODO
  return fork();
}
uint64
sys_fork(void)
{
  return fork();
}

uint64 sys_wait4(void) {
  pid_t pid;
  uint64 pstatus;
  int options;
  argint(0, &pid);
  argaddr(1, &pstatus);
  argint(2, &options);
 
  return waitpid(pid, pstatus, options);
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait_one(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  struct proc *p = myproc();
  struct tcb *t = mythread();

  argint(0, &n);
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < 10 * n){
    if(killed(p) || thread_killed(t)){
      release(&tickslock);
      return -1;
    }
    thread_sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
