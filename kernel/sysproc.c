#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "timer.h"
#include "debug.h"
#include "rc.h"

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

/**
 * @brief This system call terminates all threads in the calling process's
       thread group.
 * 
 * @return this function will not return
 */
uint64 sys_exit_group(void) {
  int n;
  argint(0, &n);
  struct tcb *t = mythread();
  if (t->tg->group_leader == t) {
    free_allother_threads_group(t);
    thread_exit(n);
  } else {
    thread_exit(n);
  }
  return 0; // not reached
}
uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64 sys_clone(void) {
  int flags;
  uint64 stack, tls, ctid;
  pid_t ptid;
  argint(0, &flags);
  argaddr(1, &stack);
  argint(2, &ptid);
  argaddr(3, &tls);
  argaddr(4, &ctid);
  return do_clone(flags, stack, ptid, tls, (pid_t *) ctid);
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

uint64 sys_brk(void) {
  uint64 addr;
  uint64 oldsz;

  argaddr(0, &addr);
  #ifdef __DEBUG_BRK
  Log("[sys_brk] %p", addr);
  Log("[sys_brk] oldsize %p", myproc()->sz);
  #endif
  
  if(addr >= MAXVA || addr >= BRKTOP) {
    Warn("brk %p failed\n", addr);
    return -1;
  }
  oldsz = myproc()->sz;

  if(addr == 0) {
    return oldsz;
  }
  if(addr > oldsz) {
    if (growproc(addr - oldsz) < 0)
      return -1;
    else
      return myproc()->sz;
  } 
  return oldsz;
  
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
  int sig;

  argint(0, &pid);
  argint(1, &sig);
  
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


uint64 sys_set_tid_address(void) {
  // printf("sys_set_tid_address\n");
  uint64 addr;
  argaddr(0, &addr);
  struct tcb *t = mythread();
  t->set_child_tid = addr;
  return (uint64)t->tid;
} 

extern struct proc proc[NPROC];
// TODO: implement this
uint64 sys_set_robust_list(void) {
  return 0;
}

static inline int rlim64_is_infinity(__u64 rlim64) { return rlim64 == RLIM64_INFINITY; }

static void rlim64_to_rlim(const struct rlimit64 *rlim64, struct rlimit *rlim) {
  if (rlim64_is_infinity(rlim64->rlim_cur))
      rlim->rlim_cur = RLIM_INFINITY;
  else
      rlim->rlim_cur = (unsigned long) rlim64->rlim_cur;
  if (rlim64_is_infinity(rlim64->rlim_max))
      rlim->rlim_max = RLIM_INFINITY;
  else
      rlim->rlim_max = (unsigned long) rlim64->rlim_max;
}

static void rlim_to_rlim64(const struct rlimit *rlim, struct rlimit64 *rlim64) {  
  if (rlim->rlim_cur == RLIM_INFINITY)
      rlim64->rlim_cur = RLIM64_INFINITY;
  else
      rlim64->rlim_cur = rlim->rlim_cur;
  if (rlim->rlim_max == RLIM_INFINITY)
      rlim64->rlim_max = RLIM64_INFINITY;
  else
      rlim64->rlim_max = rlim->rlim_max;
}

// RLIMIT_NOFILE and RLIMIT_STACK
static int do_prlimit(struct proc *p, uint32 resource, struct rlimit *new_rlim, struct rlimit *old_rlim) {
  int retval = 0;

  if (resource >= RLIM_NLIMITS)
      return -EINVAL;

  struct rlimit *rlim = p->rlim + resource;
  if (!retval) {
      if (old_rlim)
          *old_rlim = *rlim;
      if (new_rlim) {
          *rlim = *new_rlim;
      }
  }
  return retval;
}

// int prlimit(pid_t pid, int resource, const struct rlimit *new_limit, struct rlimit *old_limit);
uint64 sys_prlimit64(void) {
  pid_t pid;
  int resource;
  uint64 new_limit_addr;
  uint64 old_limit_addr;
  struct rlimit new_limit;
  struct rlimit old_limit;
  struct rlimit64 new_limit64;
  struct rlimit64 old_limit64;
  struct proc *p = NULL;
  int ret = 0;

  argint(0, &pid);
  argint(1, &resource);
  argaddr(2, &new_limit_addr);
  argaddr(3, &old_limit_addr);
  // printf("[sys_prlimit64] pid: %d, resource: %d, new_limit_addr: %p, old_limit_addr: %p\n", pid, resource, new_limit_addr, old_limit_addr);

  if(new_limit_addr) {
    if(copyin(myproc()->mm.pagetable, (char *)&new_limit64, new_limit_addr, sizeof(struct rlimit)) < 0) {
      printf("[sys_prlimit64] copyin new_limit failed\n");
      return -1;
    }
    rlim64_to_rlim(&new_limit64, &new_limit);
  }

  if (pid) {
    for (int i = 0; i < NPROC; i++) {
        if (proc[i].pid == pid) {
            p = &proc[i];
            break;
        }
    }
  } else {
    p = myproc();
  }

  if (!p) {
    panic("proc get error\n");
  }
  acquire(&p->lock);

  ret = do_prlimit(p, resource, new_limit_addr ? &new_limit : NULL, old_limit_addr ? &old_limit : NULL);

  if (!ret && old_limit_addr) {
    rlim_to_rlim64(&old_limit, &old_limit64);
    if (copyout(myproc()->mm.pagetable, old_limit_addr, (char *) &old_limit64, sizeof(old_limit64)) < 0) {
        ret = -EFAULT;
    }
  }

  release(&p->lock);
  return ret;

}

uint64 sys_gettid(void) {
  struct tcb *t = mythread();
  if (t == NULL) {
    return -1; // No thread found
  }
  return t->tid;
}

/**
 * @brief getpgid — get the process group ID for a process
 * 
 * @property pid_t getpgid(pid_t pid);
 * @return  Upon successful completion, getpgid() shall return a process group
       ID. Otherwise, it shall return (pid_t)-1 and set errno to indicate
       the error.
 */
uint64 sys_getpgid(void) {
  pid_t pid,pgid;
  struct proc *p = NULL;
  argint(0, &pid); // Get the process ID from the syscall argument
  
  p = &proc[pid]; // Find the process by its ID
  if (pid < 0 || pid >= NPROC || p == NULL) {
    return -1; // Invalid process ID or process not found
  }
  if (p->state == UNUSED) {
    return -1; // Process is not in use
  }
  acquire(&p->lock); // Acquire the process lock to ensure thread safety
  pgid = p->pgid; // Get the process group ID
  release(&p->lock); // Release the process lock
  return pgid; // Return the process group ID
}

/**
 * @brief  setpgid() sets the PGID of the process specified by pid to pgid.
       If pid is zero, then the process ID of the calling process is
       used.  If pgid is zero, then the PGID of the process specified by
       pid is made the same as its process ID. 
 * 
  * @property int setpgid(pid_t pid, pid_t pgid);
 * @return On success, setpgid() and setpgrp() return zero.  On error, -1 is
       returned, and errno is set to indicate the error.
 */
uint64 sys_setpgid(void) {
  pid_t pid, pgid;
  struct proc *p = NULL;

  argint(0, &pid); // Get the process ID from the syscall argument
  argint(1, &pgid); // Get the process group ID from the syscall argument

  if (pid < 0 || pid >= NPROCID) {
    return -1; // Invalid process ID
  }
  if(pgid < 0 || pgid >= NPROC_GROUP) {
    return -1; // Invalid process group ID
  }
  if(pid == 0) {
    p = myproc(); // If pid is 0, use the current process
  } else {
    p = &proc[pid]; // Otherwise, find the process by its ID
  }
  if (p == NULL || p->state == UNUSED) {
    return -1; // Process not found or not in use
  }
  acquire(&p->lock); // Acquire the process lock to ensure thread safety
  if (pgid == 0) {
    pgid = p->pid; // If pgid is 0, set it to the process ID
  }
  acquire(&p->lock);
  p->pgid = pgid; // Set the process group ID
  release(&p->lock); // Release the process lock

  return 0; // Return success  
}