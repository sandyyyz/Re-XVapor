#include "types.h"
#include "arch.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "timer.h"
#include "debug.h"
#include "rc.h"
#include "futex.h"

/**
 * @brief clock_nanosleep() allows the calling thread to
       sleep for an interval specified with nanosecond precision.  It
       differs in allowing the caller to select the clock against which
       the sleep interval is to be measured, and in allowing the sleep
       interval to be specified as either an absolute or a relative
       value.
 * @property int clock_nanosleep(clockid_t clock_id,
                            int flags,
                            const struct timespec *request,
                            struct timespec *_Nullable remain);

 * @return   On successfully sleeping for the requested interval,
       clock_nanosleep() returns 0.  If the call is interrupted by a
       signal handler or encounters an error, then it returns one of the
       positive error number listed in ERRORS. 
 */
uint64 sys_clock_nanosleep(void) {
  int clock_id, flags;
  uint64 addr0, addr1;
  struct timespec req, rem;
  uint rticks;
  uint ticks0;
  int is_invalid = 0;
  struct proc *p = myproc();
  argint(0, &clock_id);
  argint(1, &flags);
  argaddr(2, &addr0);
  argaddr(3, &addr1);

  if (copyin(myproc()->mm.pagetable, (char *)&req, addr0, sizeof(struct timespec)) < 0)
    return -1;

  rticks = TIMESPEC2TICKS(req);
  if(rticks < 0) {
    Warn("sys_clock_nanosleep: invalid request time %d.%d\n", req.tv_sec, req.tv_nsec);
    return -1;
  }
  acquire(&tickslock);
  ticks0 = ticks;

#ifdef __DEBUG_SYS_CLOCK_NANOSLEEP
  Log("[sys_clock_nanosleep] clock_id: %d, flags: %d, addr0: %p, addr1: %p", clock_id, flags, (void *)addr0, (void *)addr1);
  Log("req_sec: %d, req_nsec %d, rticks: %d", req.tv_sec, req.tv_nsec, rticks);
#endif
  if(req.tv_sec == INT_MAX){
    // Warn("INTMAX sec in sys_clock_nanosleep, set to 1 sec\n");
    req.tv_sec = 1;
    rticks = TIMESPEC2TICKS(req);
    is_invalid = 1;
    // release(&tickslock);
    // thread_exit(0);
  }
  // release(&tickslock);
  // return EINVAL;
  while (ticks - ticks0 < rticks)
  {
      if (proc_killed(p))
      {   
          if(addr1) {
            // rem.tv_sec = (ticks - ticks0) / TICKS_PER_SECOND;
            // rem.tv_nsec = ((ticks - ticks0) % TICKS_PER_SECOND) * 1000000000 / TICKS_PER_SECOND;
            TICKS2TIMESPEC(ticks - ticks0, rem);
            copyout(p->mm.pagetable, addr1, (char *)&rem, sizeof(rem));
          }
          release(&tickslock);
          return -1;
      }
      thread_sleep(&ticks, &tickslock, NULL);
  }
  if(addr1) {
    rem.tv_sec = 0;
    rem.tv_nsec = 0;
    if (copyout(p->mm.pagetable, addr1, (char *)&rem, sizeof(struct timespec)) < 0) {
      Warn("sys_clock_nanosleep: copyout failed\n");
      release(&tickslock);
      return -1;
    }
#ifdef __DEBUG_SYS_CLOCK_NANOSLEEP
    Log("[sys_clock_nanosleep] copyout rem: sec: %d, nsec: %d", rem.tv_sec, rem.tv_nsec);
#endif
  }
  release(&tickslock);
  if(is_invalid)
    thread_exit(0);
  return 0;
}
/**
 * @brief  nanosleep() suspends the execution of the calling thread until
       either at least the time specified in *duration has elapsed, or
       the delivery of a signal that triggers the invocation of a handler
       in the calling thread or that terminates the process.

 * @property int nanosleep(const struct timespec *duration,
                     struct timespec *_Nullable rem);

 * @return  On successfully sleeping for the requested duration, nanosleep()
       returns 0.  If the call is interrupted by a signal handler or
       encounters an error, then it returns -1, with errno set to
       indicate the error.
 */
uint64 sys_nanosleep(void)
{
    uint rticks;
    uint ticks0;
    uint64 addr0, addr1;
    struct proc *p = myproc();
    struct timespec req, rem;
    argaddr(0, &addr0);
    argaddr(1, &addr1);
    if (copyin(p->mm.pagetable, (char *)&req, addr0, sizeof(struct timespec)) < 0)
        return -1;

    rticks = TIMESPEC2TICKS(req);
    acquire(&tickslock);
    ticks0 = ticks;
    while (ticks - ticks0 < rticks)
    {
        if (proc_killed(p))
        {   
            if(addr1) {
              // rem.tv_sec = (ticks - ticks0) / TICKS_PER_SECOND;
              // rem.tv_nsec = ((ticks - ticks0) % TICKS_PER_SECOND) * 1000000000 / TICKS_PER_SECOND;
              TICKS2TIMESPEC(ticks - ticks0, rem);
              copyout(p->mm.pagetable, addr1, (char *)&rem, sizeof(rem));
            }
            release(&tickslock);
            return -1;
        }
#ifdef __DEBUG_SYS_NANOSLEEP
        Log("[sys_nanosleep] ticks: %d, ticks0: %d, rticks: %d", ticks, ticks0, rticks);
#endif
        thread_sleep(&ticks, &tickslock, NULL);
    }
    if(addr1) {
      rem.tv_sec = 0;
      rem.tv_nsec = 0;
      if (copyout(p->mm.pagetable, addr1, (char *)&rem, sizeof(struct timespec)) < 0) {
        Warn("sys_nanosleep: copyout failed\n");
        release(&tickslock);
        return -1;
      }
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


/**
 * @brief These system calls create a new ("child") process, in a manner
       similar to fork.
  * @param parent_tid If non-NULL, may be used to store the thread ID of the child process in the parent memory it pointed to 
  * @param tls may be used to set the thread local storage (TLS) descriptor
  * @param child_tid If non-NULL, may be used to store the thread ID of the child process in the child memory it pointed to
 * @property int clone(typeof(int (void *_Nullable)) *fn,
                 void *stack,
                 int flags,
                 pid_t *_Nullable parent_tid,
                    void *_Nullable tls,
                    pid_t *_Nullable child_tid 

 * @return On success, the thread ID of the child process is returned in the
       caller's thread of execution.  On failure, -1 is returned in the
       caller's context, no child process is created, and errno is set to
       indicate the error.
 */
uint64 sys_clone(void) {
  int flags;
  uint64 stack, tls, ctid, ptid;
  argint(0, &flags);
  argaddr(1, &stack);
  argaddr(2, &ptid);
  argaddr(3, &tls);
  argaddr(4, &ctid);
#ifdef __DEBUG_SYS_CLONE
  Log("[sys_clone] flags: 0x%x, stack: %p, ptid: %d, tls: %p, ctid: %p", flags, stack, ptid, tls, ctid);
#endif
  return do_clone(flags, stack, ptid, tls, ctid);
}

uint64
sys_fork(void)
{
  return do_clone(0, 0, 0, 0, 0);
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
    if(proc_killed(p) || thread_killed(t)){
      release(&tickslock);
      return -1;
    }
    thread_sleep(&ticks, &tickslock, NULL);
  }
  release(&tickslock);
  return 0;
}

/**
 * @brief The kill() system call can be used to send any signal to any
       process group or process.

 *     If pid is positive, then signal sig is sent to the process with
       the ID specified by pid.

       If pid equals 0, then sig is sent to every process in the process
       group of the calling process.

       If pid equals -1, then sig is sent to every process for which the
       calling process has permission to send signals, except for process
       1 (init), but see below.

       If pid is less than -1, then sig is sent to every process in the
       process group whose ID is -pid.

       If sig is 0, then no signal is sent, but existence and permission
       checks are still performed; this can be used to check for the
       existence of a process ID or process group ID that the caller is
       permitted to signal.

   @property int kill(pid_t pid, int sig);

 * @return On success (at least one signal was sent), zero is returned.  On
       error, -1 is returned, and errno is set to indicate the error.
 */
uint64 sys_kill(void)
{
  int pid;
  sig_t sig;

  argint(0, &pid);
  arguint64(1, &sig);
  
  return proc_kill(pid, sig);
}

uint64 sys_tkill(void) {
  int tid;
  sig_t sig;

  argint(0, &tid);
  arguint64(1, &sig);
#ifdef __DEBUG_SYS_TKILL
  Log("[sys_tkill] tid: %d, sig: %d", tid, sig);
#endif
  return thread_kill(tid, sig);
}
/**
 * @brief tgkill() sends the signal sig to the thread with the thread ID tid
       in the thread group tgid.
 * @property int tgkill(pid_t tgid, pid_t tid, int sig);
 * @return   On success, zero is returned.  On error, -1 is returned, and errno
       is set to indicate the error.
 */
uint64 sys_tgkill(void) {
  pid_t tgid, tid;
  int sig;
  argint(0, &tgid);
  argint(1, &tid);
  argint(2, &sig);
#ifdef __DEBUG_SYS_TGKILL
  Log("[sys_tgkill] tgid: %d, tid: %d, sig: %d", tgid, tid, sig);
#endif
  return thread_group_kill(tgid, tid, sig);
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


/**
 * @brief   The system call set_tid_address() sets the clear_child_tid value
       for the calling thread to tidptr.
 * @property pid_t syscall(SYS_set_tid_address, int *tidptr);
 * @return  set_tid_address() always returns the caller's thread ID.
 */
uint64 sys_set_tid_address(void) {
  // printf("sys_set_tid_address\n");
  uint64 addr;
  argaddr(0, &addr);
  struct tcb *t = mythread();
#ifdef __DEBUG_SYS_STID_ADDRESS
  Log("[sys_set_tid_address] tid: %d, addr: %p", t->tid, addr);
#endif
  t->clear_child_tid = addr;
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

#ifdef __DEBUG_DO_PRLIMIT
  Log("[do_prlimit] resource: %d, new_rlim: %p, old_rlim: %p", resource, new_rlim, old_rlim);
  if(new_rlim) {
      Log("[do_prlimit] new_rlim->rlim_cur: %d, new_rlim->rlim_max: %d", new_rlim->rlim_cur, new_rlim->rlim_max);
  }
  if(old_rlim) {
      Log("[do_prlimit] old_rlim->rlim_cur: %d, old_rlim->rlim_max: %d", old_rlim->rlim_cur, old_rlim->rlim_max);
  } 
#endif
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

/**
 * @brief  The futex() system call provides a method for waiting until a
       certain condition becomes true.
 * @property   long syscall(SYS_futex, uint32_t *uaddr, int futex_op, uint32_t val,
                    const struct timespec *timeout,   or: uint32_t val2 
                    uint32_t *uaddr2, uint32_t val3);
 * @return depends on the futex operation:
 */
uint64 sys_futex(void) {
  int futex_op;
  uint32_t val, val2, val3;
  uint64 timeout_addr, uaddr, uaddr2;
  struct timespec timeout;

  argaddr(0, &uaddr);
  argint(1, &futex_op);
  arguint32(2, &val);
  argaddr(3, &timeout_addr);
  arguint32(3, &val2);
  argaddr(4, &uaddr2);
  arguint32(5, &val3);

#ifdef __DEBUG_SYS_FUTEX
  Log("[sys_futex] uaddr: %p, futex_op: %d, val: %d, timeout_addr: %p, val2: %d, uaddr2: %p, val3: %d", 
      uaddr, futex_op, val, timeout_addr, val2, uaddr2, val3);
#endif
  if(futex_need_timeout(futex_op) && timeout_addr) {
      if (copyin(myproc()->mm.pagetable, (char *)&timeout, timeout_addr, sizeof(struct timespec)) < 0) {
          Warn("sys_futex: copyin timeout failed\n");
          return -1;
      }
  }
  return do_futex(uaddr, futex_op, val, timeout_addr ? &timeout : NULL, 
                  uaddr2, val2, val3);

}