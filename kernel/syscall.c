#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "syscall.h"
#include "defs.h"
#include "thread.h"
#include "mmap.h"

struct timespec;

// ip-pa  （因为内核页表直接映射？）
// addr-va
// Fetch the uint64 at addr from the current process.
int
fetchaddr(uint64 addr, uint64 *ip)
{
  struct proc *p = myproc();
  if(addr >= p->sz || addr+sizeof(uint64) > p->sz) // both tests needed, in case of overflow
    return -1;
  if(copyin(p->mm.pagetable, (char *)ip, addr, sizeof(*ip)) != 0)
    return -1;
  return 0;
}


// buf -- pa
// addr --va
// Fetch the nul-terminated string at addr from the current process.
// Returns length of string, not including nul, or -1 for error.
int
fetchstr(uint64 addr, char *buf, int max)
{
  struct proc *p = myproc();
  if(copyinstr(p->mm.pagetable, buf, addr, max) < 0)
    return -1;
  return strlen(buf);
}

// return p->trapfram->an,n (- [0,5]
static uint64
argraw(int n)
{
  // struct proc *p = myproc();
  struct tcb *t = mythread();
  switch (n) {
  case 0:
    return t->trapframe->a0;
  case 1:
    return t->trapframe->a1;
  case 2:
    return t->trapframe->a2;
  case 3:
    return t->trapframe->a3;
  case 4:
    return t->trapframe->a4;
  case 5:
    return t->trapframe->a5;
  }
  panic("argraw");
  return -1;
}

// Fetch the nth 32-bit system call argument.
void
argint(int n, int *ip)
{
  *ip = argraw(n);
}

// Retrieve an argument as a pointer.
// Doesn't check for legality, since
// copyin/copyout will do that.
void
argaddr(int n, uint64 *ip)
{
  *ip = argraw(n);
}

// Fetch the nth word-sized system call argument as a null-terminated string.
// Copies into buf, at most max.
// Returns string length if OK (including nul), -1 if error.
int
argstr(int n, char *buf, int max)
{
  uint64 addr;
  argaddr(n, &addr);
  return fetchstr(addr, buf, max);
}

// Prototypes for the functions that handle system calls.
extern uint64 sys_fork(void);
extern uint64 sys_exit(void);
extern uint64 sys_wait(void);
extern uint64 sys_pipe(void);
extern uint64 sys_read(void);
extern uint64 sys_kill(void);
extern uint64 sys_exec(void);
extern uint64 sys_fstat(void);
extern uint64 sys_chdir(void);
extern uint64 sys_dup(void);
extern uint64 sys_getpid(void);
extern uint64 sys_sbrk(void);
extern uint64 sys_brk(void);
extern uint64 sys_sleep(void);
extern uint64 sys_uptime(void);
extern uint64 sys_open(void);
extern uint64 sys_write(void);
extern uint64 sys_mknod(void);
extern uint64 sys_unlink(void);
extern uint64 sys_link(void);
extern uint64 sys_mkdir(void);
extern uint64 sys_close(void);
extern uint64 sys_times(void);
extern uint64 sys_gettimeofday(void);
extern uint64 sys_uname(void);
extern uint64 sys_sched_yield(void);
extern uint64 sys_nanosleep(void);
extern uint64 sys_clone(void);
extern uint64 sys_getppid(void);
extern uint64 sys_wait4(void);
extern uint64 sys_execve(void);
// 函数指针数组
// An array mapping syscall numbers from syscall.h
// to the function that handles the system call.
static uint64 (*syscalls[])(void) = {
[SYS_fork]    sys_fork,
[SYS_exit]    sys_exit,
[SYS_wait]    sys_wait,
[SYS_pipe]    sys_pipe,
[SYS_read]    sys_read,
[SYS_kill]    sys_kill,
[SYS_exec]    sys_exec,
[SYS_fstat]   sys_fstat,
[SYS_chdir]   sys_chdir,
[SYS_dup]     sys_dup,
[SYS_getpid]  sys_getpid,
[SYS_sbrk]    sys_sbrk,
[SYS_brk]     sys_brk,
[SYS_sleep]   sys_sleep,
[SYS_uptime]  sys_uptime,
[SYS_open]    sys_open,
[SYS_write]   sys_write,
[SYS_mknod]   sys_mknod,
[SYS_unlink]  sys_unlink,
[SYS_link]    sys_link,
[SYS_mkdir]   sys_mkdir,
[SYS_close]   sys_close,
[SYS_nanosleep] sys_nanosleep,
[SYS_clone]   sys_clone,
[SYS_wait4]   sys_wait4,
[SYS_execve]  sys_execve,

//other syscalls
[SYS_times]   sys_times,
[SYS_gettimeofday] sys_gettimeofday,
[SYS_uname]   sys_uname,
[SYS_sched_yield] sys_sched_yield,
[SYS_getppid] sys_getppid,
[SYS_mmap]   sys_mmap,
[SYS_munmap] sys_munmap,
};

void
syscall(void)
{
  int num;
  // struct proc *p = myproc();
  struct tcb *t = mythread();

  num = t->trapframe->a7; // call number
  if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    // Use num to lookup the system call function for num, call it,
    // and store its return value in p->trapframe->a0
    t->trapframe->a0 = syscalls[num]();
    // printf("%d %s: syscall %d\n", p->pid, p->name, num);
  } else {
    printf("%d %s: unknown sys call %d\n",
            t->tid, t->name, num);
    t->trapframe->a0 = -1;
  }
}
