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
#include "debug.h"

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
void arglong(int n, long *lip) {
  *lip = (long) argraw(n); 
}
void argulong(int n, unsigned long *lip) {
  *lip = (unsigned long) argraw(n); 
}

void arguint32(int n, uint32 *lip) {
  *lip = (uint32) argraw(n); 
}
void arguint64(int n, uint64 *lip) {
  *lip = (uint64) argraw(n); 
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

#include "sysdecl.h"

static uint64 (*syscalls[])(void) = {
#include "sysfunc.h"
};

#include "sysname.h"

void
syscall(void)
{
  int num;
  // struct proc *p = myproc();
  struct tcb *t = mythread();

  num = t->trapframe->a7; // call number

    if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
#ifdef __DEBUG_SYSCALL
      printf("thread %d syscall %d: %s\n", t->tid, num, syscall_name[num]);
#endif
    // Use num to lookup the system call function for num, call it,
    // and store its return value in p->trapframe->a0
    t->trapframe->a0 = syscalls[num]();
    // printf("%d %s: syscall %d\n", p->pid, p->name, num);
  } else {
    t->trapframe->a0 = -1;
    Warn("thread %d syscall %d: unknown", t->tid, num);
  }
}
