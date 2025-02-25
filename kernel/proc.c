#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "sched.h"
#include "trap.h"
#include "debug.h"
#include "thread.h"

void thread_forkret(void);

extern queue_t unused_p_q, used_p_q, zombie_p_q;
extern queue_t *g_pcb_queues[PROC_STATEMAX];
extern char trampoline[], uservec[], userret[];

atomic_t nextpid;

static int inline allocpid(){ return atomic_inc_return(&nextpid);}

// cpu table
struct cpu cpus[NCPU];

// process table
struct proc proc[NPROC];

extern tcb_t tcb_pool[NTHREADS];

struct proc *initproc;



// struct spinlock pid_lock;

extern void forkret(void);
static void freeproc(struct proc *p);

extern char trampoline[]; // trampoline.S

// helps ensure that wakeups of wait()ing
// parents are not lost. helps obey the
// memory model when using p->parent.
// must be acquired before any p->lock.
struct spinlock wait_lock;

// initialize the proc table.
void
procinit(void)
{
  struct proc *p;
  atomic_set(&nextpid, 1);
  // initlock(&pid_lock, "nextpid");
  initlock(&wait_lock, "wait_lock");

  PCB_Q_ALL_INIT();

  for(p = proc; p < &proc[NPROC]; p++) {
      initlock(&p->lock, "proc");
      p->ktime = 0;
      p->utime = 0;
      p->state = UNUSED;
      p->kstack = KSTACK((int) (p - proc));
      queue_push_back_atomic(&unused_p_q, p);
  }


  Info("========= Information of proc table and tcb table ==========\n");
  Info("number of proc : %d\n", NPROC);
  Info("proc table init [ok]\n");
  
  return;
}

/// @brief delete all child process from its list
/// @param parent parent process
/// @param child child process
void delete_child(struct proc *parent, struct proc *child) {
    if (nosibling(child)) {
        parent->first_child = NULL;
    } else {
        struct proc *firstchild = firstchild(parent);
        if (child == firstchild) {
            parent->first_child = nextsibling(firstchild);
        }
        list_del_reinit(&child->sibling_list);
    }
}

/// @brief add child process to parent's child list
/// @param parent parent process 
/// @param child child process
/// @attention remember to hold the parent's lock
void append_child(struct proc *parent, struct proc *child) {
    if (nochildren(parent)) {
        parent->first_child = child;
    } else {
        list_add_tail(&child->sibling_list, &(firstchild(parent)->sibling_list));
    }
}

// 总共64个进程
// Allocate a page for each process's kernel stack.
// Map it high in memory, followed by an invalid
// guard page.
void
proc_mapstacks(pagetable_t kpgtbl)
{
  struct proc *p;
  
  for(p = proc; p < &proc[NPROC]; p++) {
    char *pa = kalloc();
    if(pa == 0)
      panic("kalloc");
    uint64 va = KSTACK((int) (p - proc));
    kvmmap(kpgtbl, va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
  }
}


// Must be called with interrupts disabled,
// to prevent race with process being moved
// to a different CPU.
int
cpuid()
{
  int id = r_tp();
  return id;
}

// Return this CPU's cpu struct.
// Interrupts must be disabled.
struct cpu*
mycpu(void)
{
  int id = cpuid();
  struct cpu *c = &cpus[id];
  return c;
}

// Return the current struct proc *, or zero if none.
struct proc*
myproc(void)
{
  push_off();
  struct cpu *c = mycpu();
  struct proc *p = c->thread->p;
  pop_off();
  return p;
}



// Look in the process table for an UNUSED proc.
// If found, initialize state required to run in the kernel,
// and return with p->lock held.
// If there are no free procs, or a memory allocation fails, return 0.
/// @brief allocate a proccess , without a thread join to it
/// @brief the state has changed to used
static struct proc*
allocproc(void)
{
  struct proc *p;

  // for(p = proc; p < &proc[NPROC]; p++) {
  //   acquire(&p->lock);
  //   if(p->state == UNUSED) {
  //     goto found;
  //   } else {
  //     release(&p->lock);
  //   }
  // }
  // return 0;

  if((p = (struct proc*) queue_pop_atomic(&unused_p_q, 1)) == NULL)
    return NULL;
  
  acquire(&p->lock);

// found:
  p->pid = allocpid();

  // process family tree
  p->first_child = NULL;
  INIT_LIST_HEAD(&p->sibling_list);

  // thread group
  tginit(&p->tg);

  // timer
  p->utime = 0;
  p->ktime = 0;

  // state
  pcb_q_change_state(p, USED);

/// we don't need a trapframe in process anymorem,caz we use thread to replace process 

  // Allocate a trapframe page.
  // if((p->trapframe = (struct trapframe *)kalloc()) == 0){
  //   freeproc(p);
  //   release(&p->lock);
  //   return 0;
  // }

  // An empty user page table.
  // p->pagetable = proc_pagetable(p);
  // if(p->pagetable == 0){
  //   freeproc(p);
  //   release(&p->lock);
  //   return 0;
  // }

  // Set up new context to start executing at forkret,
  // which returns to user space.
  // memset(&p->context, 0, sizeof(p->context));
  // p->context.ra = (uint64)forkret;
  // p->context.sp = p->kstack + PGSIZE;

  // mm_struct
  initlock(&p->mm.lock, "mm_lock");
  p->mm.pagetable = proc_pagetable(p);
  INIT_LIST_HEAD(&p->mm.vma_list);

  return p;
}

/// @brief create a process with a group leader thread joined 
/// @return return the process pointer if success, NULL if failed
/// @attention return with the process'lock held, but the thread's lock is released!!
struct proc *create_proc() {
    struct tcb *t = NULL;
    struct proc *p = NULL;

    if ((p = allocproc()) == 0) {
        return 0;
    }

    if ((t = alloc_thread(thread_forkret)) == 0) {
        freeproc(p);
        return 0;
    }

    proc_join_thread(p, t, NULL);

    release(&t->lock);

    return p;
}

void thread_forkret(void)
{
  static int thread_first = 1;

  // Still holding p->lock from scheduler.
  release(&mythread()->lock);

  if (thread_first) {
    // File system initialization must be run in the context of a
    // regular process (e.g., because it calls sleep), and thus cannot
    // be run from main().
    thread_first = 0;
    fsinit(ROOTDEV);
  }

  thread_usertrapret();
}



// free a proc structure and the data hanging from it,
// including user pages.
// p->lock must be held.
// make sure child processes are already freed outside
// TODO: how should i deal with the therads in the process?
static void
freeproc(struct proc *p)
{

// no trapframe anymore in a process
  // if(p->trapframe)
  //   kfree((void*)p->trapframe);
  // p->trapframe = 0;
  if(p->mm.pagetable)
    proc_freepagetable(p->mm.pagetable, p->sz);
  // the list??
  p->mm.pagetable = 0;
  p->sz = 0;
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  // p->chan = 0;
  p->killed = 0;
  p->xstate = 0;
  p->utime = 0;
  p->ktime = 0;
  // p->state = UNUSED;
  // change to UNUSED state
  pcb_q_change_state(p, UNUSED);

}

// Create a user page table for a given process, with no user memory,
// but with trampoline and trapframe pages.
// update : the process do not have a trapframe anymore. so needn't to map the trapframe page
pagetable_t
proc_pagetable(struct proc *p)
{
  pagetable_t pagetable;

  // An empty page table.
  pagetable = uvmcreate();
  if(pagetable == 0)
    return 0;

  // map the trampoline code (for system call return)
  // at the highest user virtual address.
  // only the supervisor uses it, on the way
  // to/from user space, so not PTE_U.
  if(mappages(pagetable, TRAMPOLINE, PGSIZE,
              (uint64)trampoline, PTE_R | PTE_X) < 0){
    uvmfree(pagetable, 0);
    return 0;
  }

  // map the trapframe page just below the trampoline page, for
  // trampoline.S.
  // if(mappages(pagetable, TRAPFRAME, PGSIZE,
  //             (uint64)(p->trapframe), PTE_R | PTE_W) < 0){
  //   uvmunmap(pagetable, TRAMPOLINE, 1, 0);
  //   uvmfree(pagetable, 0);
  //   return 0;
  // }

  return pagetable;
}

// Free a process's page table, and free the
// physical memory it refers to.
void
proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
  int thread_cnt = atomic_read(&myproc()->tg.thread_cnt);
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);
  // uvmunmap(pagetable, TRAPFRAME, 1, 0);

  // and unmap the thread's trapframe
  for (int i = 0; i < thread_cnt; i++) {
    uvmunmap(pagetable, THREAD_TRAPFRAME(i), 1, 0);
  }
  uvmfree(pagetable, sz);
}

// a user program that calls exec("/init")
// assembled from ../user/initcode.S
// od -t xC ../user/initcode
uchar initcode[] = {
  0x17, 0x05, 0x00, 0x00, 0x13, 0x05, 0x45, 0x02,
  0x97, 0x05, 0x00, 0x00, 0x93, 0x85, 0x35, 0x02,
  0x93, 0x08, 0x70, 0x00, 0x73, 0x00, 0x00, 0x00,
  0x93, 0x08, 0x20, 0x00, 0x73, 0x00, 0x00, 0x00,
  0xef, 0xf0, 0x9f, 0xff, 0x2f, 0x69, 0x6e, 0x69,
  0x74, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
};

// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  struct tcb *t;
  p = create_proc(); // with a group leader thread
  t = p->tg.group_leader;

  if(p == 0)
    panic("userinit: create_proc failed");
  
  initproc = p;
  // struct tcb* t = p->tg.group_leader;

  // allocate one user page and copy initcode's instructions
  // and data into it.
  uvmfirst(p->mm.pagetable, initcode, sizeof(initcode));
  p->sz = PGSIZE; // TODO: how large?? maybe bug here

  // prepare for the very first "return" from kernel to user.

  t->trapframe->epc = 0;      // user program counter
  t->trapframe->sp = PGSIZE;  // user stack pointer

  safestrcpy(p->name, "initcode", sizeof(p->name));
  safestrcpy(p->tg.group_leader->name, "/init-0", 10);

  p->cwd = namei("/"); 

  // p->state = RUNNABLE; 
  tcb_q_change_state(t, TCB_RUNNABLE);
  
  release(&p->lock);
}

// Grow or shrink user memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint64 sz;
  struct proc *p = myproc();

  sz = p->sz;
  if(n > 0){
    if((sz = uvmalloc(p->mm.pagetable, sz, sz + n, PTE_W)) == 0) {
      return -1;
    }
  } else if(n < 0){
    sz = uvmdealloc(p->mm.pagetable, sz, sz + n);
  }
  p->sz = sz;
  return 0;
}

// Create a new process, copying the parent.
// Sets up child kernel stack to return as if from fork() system call.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *p = myproc();
  struct tcb  *t = mythread();
   
  // Allocate process.
  // return with np->lock held
  if((np = allocproc()) == 0){
    return -1;
  }

  if((t = alloc_thread(thread_forkret)) == 0) {
    freeproc(np);
    return -1;
  }

  // make the allocated thread be the group leader
  proc_join_thread(np, t, NULL);


  // proc_join_thread(np, t, NULL);?

  // Copy user memory from parent to child.
  if(uvmcopy(p->mm.pagetable, np->mm.pagetable, p->sz) < 0){
    freeproc(np);
    release(&np->lock);
    return -1;
  }
  np->sz = p->sz;

  // TODO: let's just copy the leader thread right now
  // copy saved user registers.
  *(np->tg.group_leader->trapframe) = *(p->tg.group_leader->trapframe);

  // Cause fork to return 0 in the child.
  np->tg.group_leader->trapframe->a0 = 0;

  // increment reference counts on open file descriptors.
  // child process "open" the files
  for(i = 0; i < NOFILE; i++)
    if(p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);
  np->cwd = idup(p->cwd);

  safestrcpy(np->name, p->name, sizeof(p->name));

  pid = np->pid;

  release(&np->lock);

  acquire(&wait_lock);
  np->parent = p;
  release(&wait_lock);

  acquire(&p->lock);
  append_child(p, np);
  release(&p->lock);
  
  acquire(&np->lock);
  // np->state = RUNNABLE;
  // change the leader thread's state
  tcb_q_change_state(np->tg.group_leader, TCB_RUNNABLE);
  release(&np->lock);

  return pid;
}

// initproc接管即将退出的父进程的所有子进程
// 将p所有子进程的父进程修改为initproc,并wakeup(initproc)
// Pass p's abandoned children to init.
// Caller must hold wait_lock.
void
reparent(struct proc *p)
{
  struct proc *pp;

  for(pp = proc; pp < &proc[NPROC]; pp++){
    if(pp->parent == p){
      pp->parent = initproc;
      thread_wakeup_chan(initproc);
    }
  }
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait().
void
exit(int status)
{
  struct proc *p = myproc();

  if(p == initproc)
    panic("init exiting");

  // Close all open files.
  for(int fd = 0; fd < NOFILE; fd++){
    if(p->ofile[fd]){
      struct file *f = p->ofile[fd];
      fileclose(f);
      p->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(p->cwd);
  end_op();
  p->cwd = 0;

  acquire(&wait_lock);

  // Give any children to init.
  reparent(p);

  // Parent might be sleeping in wait().
  thread_wakeup_chan(p->parent);
  
  acquire(&p->lock);

  p->xstate = status;
  // p->state = ZOMBIE;
  pcb_q_change_state(p, ZOMBIE);
  release(&wait_lock);

  // Jump into the scheduler, never to return.
  // sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(uint64 addr)
{
  struct proc *pp;
  int havekids, pid;
  struct proc *p = myproc();
  // struct tcb *t = mythread();

  acquire(&wait_lock);

  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(pp = proc; pp < &proc[NPROC]; pp++){
      if(pp->parent == p){
        // make sure the child isn't still in exit() or swtch().
        acquire(&pp->lock);

        havekids = 1;
        if(pp->state == ZOMBIE){
          // Found one.
          pid = pp->pid;
          if(addr != 0 && copyout(p->mm.pagetable, addr, (char *)&pp->xstate,
                                  sizeof(pp->xstate)) < 0) {
            release(&pp->lock);
            release(&wait_lock);
            return -1;
          }
          freeproc(pp);
          release(&pp->lock);
          release(&wait_lock);
          return pid;
        }
        release(&pp->lock);
      }
    }

    // No point waiting if we don't have any children. 
    if(!havekids || killed(p)){
      release(&wait_lock);
      return -1;
    }
    
    // Wait for a child to exit.
    thread_sleep(p, &wait_lock);  //DOC: wait-sleep
  }
}

int wait4(pid_t pid, uint64 pstatus, int options) {
  struct proc *pp;
  int havekids;
  struct proc *p = myproc();

  acquire(&wait_lock);

  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(pp = proc; pp < &proc[NPROC]; pp++){
      if(pp->parent == p && pid == pp->pid){
        // make sure the child isn't still in exit() or swtch().
        acquire(&pp->lock);

        havekids = 1;
        if(pp->state == ZOMBIE){
          // Found target child process.
          if(pstatus != 0 && copyout(p->mm.pagetable, pstatus, (char *)&pp->xstate,
                                  sizeof(pp->xstate)) < 0) {
            release(&pp->lock);
            release(&wait_lock);
            return -1;
          }
          freeproc(pp);
          release(&pp->lock);
          release(&wait_lock);
          return pid;
        }
        release(&pp->lock);
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || killed(p)){
      release(&wait_lock);
      return -1;
    }
    
    // Wait for a child to exit.
    // TODO: how to wait for a specific child process to exit??
    // TODO: bug here: any child process will wake up this parent process
    thread_sleep(p, &wait_lock);  //DOC: wait-sleep
  }
  

}
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run.
//  - swtch to start running that process.
//  - eventually that process transfers control
//    via swtch back to the scheduler.
// void
// scheduler(void)
// {
//   struct proc *p;
//   struct cpu *c = mycpu();
  
//   c->proc = 0;
//   for(;;){
//     // Avoid deadlock by ensuring that devices can interrupt.
//     intr_on();

//     for(p = proc; p < &proc[NPROC]; p++) {
//       acquire(&p->lock);
//       if(p->state == RUNNABLE) {
//         // Switch to chosen process.  It is the process's job
//         // to release its lock and then reacquire it
//         // before jumping back to us.
//         p->state = RUNNING;
//         c->proc = p;
//         swtch(&c->context, &p->context);

//         // Process is done running for now.
//         // It should have changed its p->state before coming back.
//         c->proc = 0;
//       }
//       release(&p->lock);
//     }
//   }
// }

// // p->scheduler
// Switch to scheduler.  Must hold only p->lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->noff, but that would
// break in the few places where a lock is held but
// there's no process.
// void
// sched(void)
// {
//   int intena;
//   struct proc *p = myproc();

//   if(!holding(&p->lock))
//     panic("sched p->lock");
//   if(mycpu()->noff != 1)
//     panic("sched locks");
//   if(p->state == RUNNING)
//     panic("sched running");
//   if(intr_get())
//     panic("sched interruptible");

//   intena = mycpu()->intena;
//   swtch(&p->context, &mycpu()->context);
//   mycpu()->intena = intena;
// }

// Give up the CPU for one scheduling round.
// void
// yield(void)
// {
//   struct proc *p = myproc();
//   acquire(&p->lock);
//   p->state = RUNNABLE;
//   sched();
//   release(&p->lock);
// }

// A fork child's very first scheduling by scheduler()
// will swtch to forkret.
// void
// forkret(void)
// {
//   static int first = 1;

//   // Still holding p->lock from scheduler.
//   release(&myproc()->lock);

//   if (first) {
//     // File system initialization must be run in the context of a
//     // regular process (e.g., because it calls sleep), and thus cannot
//     // be run from main().
//     first = 0;
//     fsinit(ROOTDEV);
//   }

//   usertrapret();
// }





// Kill the process with the given pid.
// The victim won't exit until it tries to return
// to user space (see usertrap() in trap.c).
int
kill(int pid)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++){
    acquire(&p->lock);
    if(p->pid == pid){
      p->killed = 1;
      // if(p->state == SLEEPING){
      //   // Wake process from sleep().
      //   p->state = RUNNABLE;
      // }
      
      // kill all threads then
      
      release(&p->lock);
      return 0;
    }
    release(&p->lock);
  }
  return -1;
}

void
setkilled(struct proc *p)
{
  acquire(&p->lock);
  p->killed = 1;
  release(&p->lock);
}

int
killed(struct proc *p)
{
  int k;
  
  acquire(&p->lock);
  k = p->killed;
  release(&p->lock);
  return k;
}

// Copy to either a user address, or kernel address,
// depending on usr_dst.
// Returns 0 on success, -1 on error.
int
either_copyout(int user_dst, uint64 dst, void *src, uint64 len)
{
  struct proc *p = myproc();
  if(user_dst){
    return copyout(p->mm.pagetable, dst, src, len);
  } else {
    memmove((char *)dst, src, len);
    return 0;
  }
}

// Copy from either a user address, or kernel address,
// depending on usr_src.
// Returns 0 on success, -1 on error.
int
either_copyin(void *dst, int user_src, uint64 src, uint64 len)
{
  struct proc *p = myproc();
  if(user_src){
    return copyin(p->mm.pagetable, dst, src, len);
  } else {
    memmove(dst, (char*)src, len);
    return 0;
  }
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused", 
  [USED]      "used",
  // [SLEEPING]  "sleep ",
  // [RUNNABLE]  "runble",
  // [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  struct proc *p;
  char *state;

  printf("\n");
  for(p = proc; p < &proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    printf("%d %s %s", p->pid, state, p->name);
    printf("\n");
  }
}
