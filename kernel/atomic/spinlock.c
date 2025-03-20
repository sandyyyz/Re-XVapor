// Mutual exclusion spin locks.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "proc.h"
#include "defs.h"
#include "debug.h"

extern int init_finished;

void
initlock(struct spinlock *lk, char *name)
{
  lk->name = name;
  lk->locked = 0; // 0表示未锁定
  lk->cpu = 0;
}

// Acquire the lock.
// Loops (spins) until the lock is acquired.
// 尝试获取锁
void
acquire(struct spinlock *lk)
{

  push_off(); // disable interrupts to avoid deadlock.
  if(holding(lk))
    panic("acquire");

  // On RISC-V, sync_lock_test_and_set turns into an atomic swap:
  //   a5 = 1
  //   s1 = &lk->locked
  //   amoswap.w.aq a5, a5, (s1)
  // 原子交换。类似于x86的CAS?
  // 一直尝试获取锁
  while(__sync_lock_test_and_set(&lk->locked, 1) != 0)
    ;

  // Tell the C compiler and the processor to not move loads or stores
  // past this point, to ensure that the critical section's memory
  // references happen strictly after the lock is acquired.
  // On RISC-V, this emits a fence instruction.
  // __sync_synchronize 
  // 确保在获取锁之后的内存操作在语义上发生在获取锁之后
  // 也就是为了保证锁变量更新之前无法进行别的内存操作
  // 保证锁变量更新对别的CPU的可见性
  __sync_synchronize();

  // Record info about lock acquisition for holding() and debugging.
  lk->cpu = mycpu();
  #ifdef __DEBUG_SPINLOCK
  if(init_finished)
    Info("acquire %s: cpu %d\n", lk->name, cpuid());
  #endif
}

// Release the lock.
void
release(struct spinlock *lk)
{
  if(!holding(lk))
    panic("release");

  lk->cpu = 0;

  // Tell the C compiler and the CPU to not move loads or stores
  // past this point, to ensure that all the stores in the critical
  // section are visible to other CPUs before the lock is released,
  // and that loads in the critical section occur strictly before
  // the lock is released.
  // On RISC-V, this emits a fence instruction.
  __sync_synchronize();

  // Release the lock, equivalent to lk->locked = 0.
  // This code doesn't use a C assignment, since the C standard
  // implies that an assignment might be implemented with
  // multiple store instructions.
  // On RISC-V, sync_lock_release turns into an atomic swap:
  //   s1 = &lk->locked
  //   amoswap.w zero, zero, (s1)
  __sync_lock_release(&lk->locked);

  #ifdef __DEBUG_SPINLOCK
  if(init_finished)
    Info("release %s: cpu %d\n", lk->name, cpuid());
  #endif
  pop_off();
}

// Check whether this cpu is holding the lock.
// Interrupts must be off.
int
holding(struct spinlock *lk)
{
  int r;
  if(!lk) {
    panic("holding NULL");
  } 
  r = (lk->locked && lk->cpu == mycpu());
  return r;
}

// push_off/pop_off are like intr_off()/intr_on() except that they are matched:
// it takes two pop_off()s to undo two push_off()s.  Also, if interrupts
// are initially off, then push_off, pop_off leaves them off.

void
push_off(void)
{
  int old = intr_get();

  intr_off(); //禁止设备中断
  // 如果当前CPU的中断深度为0，证明在增加中断深度以前
  // 也就是调用push_off以前中断是启用的
  // 所以需要保存旧的中断状态到 intena里
  // 以便我们退出临界区时恢复中断状态
  if(mycpu()->noff == 0)
    mycpu()->intena = old;

  mycpu()->noff += 1;


}

void
pop_off(void)
{
  struct cpu *c = mycpu();
  // 启用中断前，中断已经启用 ,panic
  if(intr_get())
    panic("pop_off - interruptible");
  if(c->noff < 1)
    panic("pop_off");
  c->noff -= 1;
  // 禁用中断深度为0， 且之前中断启用，则恢复中断
  if(c->noff == 0 && c->intena)
    intr_on();
}
