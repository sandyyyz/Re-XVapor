#ifndef __THREAD_H
#define __THREAD_H

#include "types.h"
#include "list.h"
#include "atomic.h"
// #include "proc.h"

struct context;
// callback for the first scheduled of thread
typedef void (*thread_callback)(void);

// thread state
enum thread_state { TCB_UNUSED,
                    TCB_USED,
                    TCB_RUNNABLE,
                    TCB_RUNNING,
                    TCB_SLEEPING,
                    TCB_MAX_STATE,
};

typedef enum thread_state thread_state_t;

// Saved registers for kernel context switches.
struct context {
  // return address
  uint64 ra;
  uint64 sp;

  // callee-saved
  uint64 s0;
  uint64 s1;
  uint64 s2;
  uint64 s3;
  uint64 s4;
  uint64 xv6fs;
  uint64 s6;
  uint64 s7;
  uint64 s8;
  uint64 s9;
  uint64 s10;
  uint64 s11;
};

// per-process data for the trap handling code in trampoline.S.
// sits in a page by itself just under the trampoline page in the
// user page table. not specially mapped in the kernel page table.
// uservec in trampoline.S saves user registers in the trapframe,
// then initializes registers from the trapframe's
// kernel_sp, kernel_hartid, kernel_satp, and jumps to kernel_trap.
// usertrapret() and userret in trampoline.S set up
// the trapframe's kernel_*, restore user registers from the
// trapframe, switch to the user page table, and enter user space.
// the trapframe includes callee-saved user registers like s0-s11 because the
// return-to-user path via usertrapret() doesn't return through
// the entire kernel call stack.
struct trapframe {
  /*   0 */ uint64 kernel_satp;   // kernel page table
  /*   8 */ uint64 kernel_sp;     // top of process's kernel stack
  /*  16 */ uint64 kernel_trap;   // usertrap()
  /*  24 */ uint64 epc;           // saved user  program counter
  /*  32 */ uint64 kernel_hartid; // saved kernel tp
  /*  40 */ uint64 ra;
  /*  48 */ uint64 sp;
  /*  56 */ uint64 gp;
  /*  64 */ uint64 tp;
  /*  72 */ uint64 t0;
  /*  80 */ uint64 t1;
  /*  88 */ uint64 t2;
  /*  96 */ uint64 s0;
  /* 104 */ uint64 s1;
  /* 112 */ uint64 a0;
  /* 120 */ uint64 a1;
  /* 128 */ uint64 a2;
  /* 136 */ uint64 a3;
  /* 144 */ uint64 a4;
  /* 152 */ uint64 a5;
  /* 160 */ uint64 a6;
  /* 168 */ uint64 a7;
  /* 176 */ uint64 s2;
  /* 184 */ uint64 s3;
  /* 192 */ uint64 s4;
  /* 200 */ uint64 xv6fs;
  /* 208 */ uint64 s6;
  /* 216 */ uint64 s7;
  /* 224 */ uint64 s8;
  /* 232 */ uint64 s9;
  /* 240 */ uint64 s10;
  /* 248 */ uint64 s11;
  /* 256 */ uint64 t3;
  /* 264 */ uint64 t4;
  /* 272 */ uint64 t5;
  /* 280 */ uint64 t6;
};

// thread group
struct thread_group {
    // spinlock
    spinlock_t lock;
    // thread group id, equals to pid
    tgid_t tgid;
    // thread index within the group, start from 0
    int thread_idx;
    // count of threads, start from 1
    atomic_t thread_cnt;
    // for list
    struct list_head threads;
    // group leader : main thread
    struct tcb *group_leader;

};

// thread control block
struct tcb {
    // spinlock
    spinlock_t lock;
    // thread name (debugging)
    char name[20];
    // the state of thread
    thread_state_t state;
    // the proc pointer it belongs to
    struct proc *p;
    // thread id, global
    tid_t tid;
    // offset, local
    int tidx;
    // thread : killed ?
    int killed;
    // tcb state queue
    struct list_head state_list;

    // kernel stack, trapframe and context
    uint64 kstack;               // kernel stack , needn't to be freed, fixed allocation
    uint64 ustack;               // user stack

    struct trapframe *trapframe; // data page for trampoline.S
    struct context context;      // swtch() here to run thread
    
    // thread list
    struct list_head threads;
    // thread group
    struct thread_group *tg;

    // waiting queue entry
    struct queue *wait_chan_entry;  
  // waiting queue list, used to hang the threads into the same waiting queue  
    struct list_head wait_list; 

    void  *chan; // if not zero sleeping on chan,temporary used before using the condistion variable 

    // exit state
    int xstate;
};
typedef struct tcb tcb_t;

int proc_join_thread(struct proc *p, struct tcb *t, char *name);
void thread_setkilled(struct tcb *t);
void tginit(struct thread_group *tg);
void free_thread(struct tcb *t); 
void create_thread(struct proc *p, struct tcb *t, char *name, thread_callback callback);
struct tcb *alloc_thread(thread_callback callback);
tcb_t* mythread(void);
void tcb_init(void);
void thread_sleep(void *chan, struct spinlock *lk);
int thread_killed(struct tcb *t);
void thread_wakeup_chan(void *chan);
void thread_wakeup_specific_atomic(struct tcb *t);
void thread_wakeup_specific(struct tcb *t);
void thread_forkret();
void print_trapframe(struct trapframe *tf);
void thread_exit(int status);
void transfer_trapframe(struct tcb* t, pagetable_t newpgtble, int unmmap_old);
int free_allother_threads_group(struct tcb *t);

#endif