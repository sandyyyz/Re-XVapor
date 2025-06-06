#ifndef __THREAD_H
#define __THREAD_H

#include "types.h"
#include "list.h"
#include "atomic.h"
// #include "proc.h"
#include "signal.h"
#include "trapframe.h"

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
  uint64 s5;
  uint64 s6;
  uint64 s7;
  uint64 s8;
  uint64 s9;
  uint64 s10;
  uint64 s11;
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

    // waiting queue entry, just used for futex right now
    struct queue *wait_chan_entry;  
  // waiting queue list, used to hang the threads into the same waiting queue  
    struct list_head wait_list; 

    void  *chan; // if not zero sleeping on chan,temporary used before using the condition variable 

    uint64 timeout; // timeout for futex, used in futex_wait. ticks

    // exit state
    int xstate;

    /* CLONE_CHILD_SETTID: */
    uint64 set_child_tid;
    /* CLONE_CHILD_CLEARTID: */
    uint64 clear_child_tid;   
    
    struct sighand sigs; // store all signal action
    sigset_t blocked; // the blocked signal
    struct sigpending sig_pending; // the queue of signals waiting thread to process
    int pending_cnt; // the count of pending signals
    sig_t sig_processing; // the signal thread processing on
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
void thread_send_signal(struct tcb *t_given, siginfo_t *info);
int thread_kill(int tid, int sig);
int thread_group_kill(int tgid, int tid, int sig);
void thread_wakeup_timeout(uint ticks_now);

#endif