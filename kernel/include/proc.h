#ifndef __PROC_H
#define __PROC_H

#include "spinlock.h"
#include "list.h"
#include "param.h"
#include "semaphore.h" 
#include "mm.h"
#include "thread.h"
#include "vfs.h"
#include "ext4.h"
#include "rc.h"

struct thread_group;
struct tcb;

// Per-CPU state.
struct cpu {
  // struct proc *proc;          // The process running on this cpu, or null.
  struct context context;     // swtch() here to enter scheduler().
  int noff;                   // Depth of push_off() nesting.
  int intena;                 // Were interrupts enabled before push_off()?

  struct tcb *thread;       // The thread running on this cpu, or null.
};

extern struct cpu cpus[NCPU];


// enum procstate { UNUSED, USED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE, PROC_STATEMAX };
// --> UNUSED, USED, ZOMBIE
enum procstate {UNUSED, USED, ZOMBIE, PROC_STATEMAX };

// Per-process state
struct proc {
  struct spinlock lock;

  struct spinlock lth_exitlock; // last thread exit lock, only use in racing between freeproc() and thread_exit()
  // p->lock must be held when using these:
  enum procstate state;        // Process state
  // void *chan;                  // If non-zero, sleeping on chan
  int killed;                  // If non-zero, have been killed
  int xstate;                  // Exit status to be returned to parent's wait
  int pid;                     // Process ID
  int pgid;                   // Process group ID

  // these are private to the process, so p->lock need not be held.
  uint64 kstack;               // Virtual address of kernel stack
  uint64 sz;                   // Size of process memory (bytes)
  // pagetable_t pagetable;       // User page table(put in mm_struct now)

  // struct trapframe *trapframe; // data page for trampoline.S
  // struct context context;      // swtch() here to run process
  int ofile_cnt;            // Number of open files
  struct file *ofile[NOFILE];  // Open files
  struct inode *cwd;           // Current directory

  struct cwdinfo cinfo;

  char name[16];               // Process name (debugging)

  // used for sys_times
  _clock_t ktime;               // kernel time 
  _clock_t utime;              //  user time
  
  list_head_t state_list;

  // proc children and siblings
  // wait_lock must be held when using this:
  struct proc *parent;         // Parent process

  struct proc *first_child;      // its first child
  struct list_head sibling_list; // its sibling  
// thread group
  struct thread_group tg;
  // for clone
  pid_t ctid;

  // thread lock
  struct semaphore tlock;

  struct mm_struct mm;
  
  struct ext4_dir dir; // for ext4

  struct rlimit rlim[RLIM_NLIMITS];

};


// ======================= process family tree =====================
#define nochildren(p) (p->first_child == NULL)
#define nosibling(p) (list_empty(&p->sibling_list))
#define firstchild(p) (p->first_child)
#define nextsibling(p) (list_first_entry(&(p->sibling_list), struct proc, sibling_list))

void freeproc(struct proc *p);
struct proc* myproc(void);
int procs_cnt(void);
void proc_sendsignal_all_thread(struct proc *p, sig_t signo, int opt);

#endif