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
#include "list.h"
#include "wait.h"
#include "mmap.h"
#include "vfs.h"
#include "../include/sched.h"

void thread_forkret(void);

extern queue_t unused_p_q, used_p_q, zombie_p_q;
extern queue_t *g_pcb_queues[PROC_STATEMAX];
extern char trampoline[], uservec[], userret[], __user_rt_sigreturn[];

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
void freeproc(struct proc *p);

extern char trampoline[]; // trampoline.S

// helps ensure that wakeups of wait()ing
// parents are not lost. helps obey the
// memory model when using p->parent.
// must be acquired before any p->lock.
struct spinlock wait_lock;

// initialize the proc table.
// static void mm_init(struct mm_struct *mm) {
//     mm->max_vma = MMAP_MAX_ADDR_START;
//     INIT_LIST_HEAD(&mm->vma_list);
//     initlock(&mm->lock, "mm_lock");
//     mm->pagetable = 0;
// }
void
procinit(void)
{
  struct proc *p;
  PCB_Q_ALL_INIT();
  atomic_set(&nextpid, 1);
  // initlock(&pid_lock, "nextpid");
  initlock(&wait_lock, "wait_lock");


  for(p = proc; p < &proc[NPROC]; p++) {
      initlock(&p->lock, "proc");
      initlock(&p->lth_exitlock, "lth_exitlock");
      p->ktime = 0;
      p->utime = 0;
      p->state = UNUSED;
      p->kstack = KSTACK((int) (p - proc));
      INIT_LIST_HEAD(&p->state_list);
      INIT_LIST_HEAD(&p->sibling_list);
      
      queue_push_back_atomic(&unused_p_q, p);

      // mm_init(&p->mm);
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

// Allocate a page for each thread's kernel stack.
// Map it high in memory, followed by an invalid
// guard page.
void thread_mapstacks(pagetable_t kpgtbl)
{
  struct tcb *t;
  
  for(t = tcb_pool;  t < &tcb_pool[NTHREADS]; t++) {
    char *pa = kalloc();
    if(pa == 0)
      panic("kalloc");
    uint64 va = KSTACK((int) (t - tcb_pool));
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
  // if(!(c->thread)) panic("cpu's thread = null!!\n");
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
/// @attention the pgtbl of the proc only contain TRAMPOLINE right now 
static struct proc*
allocproc(void)
{
  struct proc *p;

  if((p = (struct proc*)queue_pop_atomic(&unused_p_q, 1)) == NULL)
    return NULL;

  acquire(&p->lock);

// found:
  p->pid = allocpid();
  p->pgid = p->pid; // process group id, equals to pid
#ifdef __DEBUG_ALLOCATE_PROC
  Info("proc %d allocated, pcb %p\n", p->pid, p);
#endif
  pcb_q_change_state(p, USED);
  // process family tree
  p->first_child = NULL;
  INIT_LIST_HEAD(&p->sibling_list);

  // thread group
  tginit(&p->tg);

  // timer
  p->utime = 0;
  p->ktime = 0;
  
  strcpy(p->cinfo.path, "/");
  // state
  // mm_struct
  initlock(&p->mm.lock, "mm_lock");
  p->mm.pagetable = proc_pagetable(p);
#ifdef __DEBUG_ALLOCATE_PROC
  Info("proc %d's pagetable = %p\n", p->pid, p->mm.pagetable);
#endif
  INIT_LIST_HEAD(&p->mm.vma_list);
  p->mm.max_vma = MMAP_MAX_ADDR_START;

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




/**
 * @brief free the given process and all it's threads
 * 
 * @param p process
 * @attention must hold the p->lock.make sure all children process have exit before call this function
 */
void freeproc(struct proc *p)
{

// no trapframe anymore in a process
  // if(p->trapframe)
  //   kfree((void*)p->trapframe);
  // p->trapframe = 0;


#ifdef __DEBUG_FREEPROC
  Log("thread %d freeproc %d", mythread()->tid, cur_proc->pid);
#endif

  // struct tcb  *cur_thread = mythread();
  struct proc *cur_proc = myproc();
  /**
   * acturally, we needn't to free the threads here,
   * because we have freed them in thread_exit
   * and the only entry to proc_exit() is last thread's thread_exit()
   * so we needn't to free them again if we have freed them in thread_exit
   * we just need to wait for the last thread to exit before free the process
   * 
   */
  acquire(&p->lth_exitlock);
  struct tcb *t, *tt;
  acquire(&cur_proc->tg.lock);

  // sometimes it is not from the last thread's thread_exit
  list_for_each_entry_safe(t, tt, &(p->tg.threads), threads) {
    // TODO: is it right?
    if(t)
      list_del_reinit(&t->threads);

    if(!atomic_dec_return(&cur_proc->tg.thread_cnt)) {
      panic("thread count error\n");
    }
    free_thread(t);
  }
  release(&cur_proc->tg.lock);

  // have unmmaped threads' trapframe in free_thread
  // so needn't free them again
  if(p->mm.pagetable)
    proc_freepagetable(p->mm.pagetable, p->sz, 0);
  // the list??
  p->tg.group_leader = NULL;
  p->mm.pagetable = 0;
  p->sz = 0;
  p->pid = 0;
  p->pgid = 0; // process group id, equals to pid
  
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
  release(&p->lth_exitlock);
#ifdef __DEBUG_FREEPROC
  Log("thread %d freeproc %d end", mythread()->tid, cur_proc->pid);
#endif
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

  if(mappages(pagetable, SIGRETURN, PGSIZE, (uint64) __user_rt_sigreturn, PTE_R | PTE_X | PTE_U) < 0){
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
proc_freepagetable(pagetable_t pagetable, uint64 sz, int unmmap_ttf)
{
  int thread_cnt = atomic_read(&myproc()->tg.thread_cnt);
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);
  uvmunmap(pagetable, SIGRETURN, 1, 0);
  // uvmunmap(pagetable, TRAPFRAME, 1, 0);

  // and unmap the thread's trapframe
  if(unmmap_ttf)
    for (int i = 0; i < thread_cnt; i++) {
      uvmunmap(pagetable, THREAD_TRAPFRAME(i), 1, 0);
    }
  uvmfree(pagetable, sz);
}

#ifdef __USE_XV6FS
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

unsigned int initcode_len = sizeof(initcode);

#else 
#include "../../build/user/initcode.h"
#endif

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
  uvmfirst(p->mm.pagetable, initcode, initcode_len);
  p->sz = PGSIZE; // TODO: how large?? maybe bug here

  // prepare for the very first "return" from kernel to user.

  t->trapframe->epc = 0;      // user program counter
  t->trapframe->sp = PGSIZE;  // user stack pointer
  // Log("userinit trapframe: %p", t->trapframe);
  safestrcpy(p->name, "initcode", sizeof(p->name));
  safestrcpy(p->tg.group_leader->name, "/init-0", 10);

  // p->state = RUNNABLE; 
  tcb_q_change_state(t, TCB_RUNNABLE);

#ifdef __DEBUG_PROC

#endif //__DEBUG_PROC 
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
  // struct tcb  *t = mythread();
  // Allocate process.
  // return with np->lock held
  // if((np = allocproc()) == 0){
  //   return -1;
  // }
  // if((t = alloc_thread(thread_forkret)) == 0) {
  //   freeproc(np);
  //   return -1;
  // }

  // // make the allocated thread be the group leader
  // proc_join_thread(np, t, NULL);
  if((np = create_proc()) == 0) {
    return -1;
  }
  if(uvmcopy(p->mm.pagetable, np->mm.pagetable, p->sz) < 0){
    freeproc(np);
    release(&np->lock);
    return -1;
  }
  np->sz = p->sz;
  
  // copy vma
  acquire(&p->mm.lock);
  acquire(&np->mm.lock);
  proc_copy_vma(p, np);
  release(&np->mm.lock);
  release(&p->mm.lock);

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

  safestrcpy(np->name, p->name, sizeof(p->name));

  pid = np->pid;
  strncpy(np->cinfo.path, p->cinfo.path, MAXPATH);
  // release(&np->tg.group_leader->lock);
  release(&np->lock);

  acquire(&wait_lock);
  np->parent = p;
  release(&wait_lock);

  acquire(&p->lock);
  append_child(p, np);
  release(&p->lock);
  
  // acquire(&np->lock);
  // np->state = RUNNABLE;
  // change the leader thread's state
  // acquire(&np->tg.lock);
  acquire(&np->tg.group_leader->lock);
  tcb_q_change_state(np->tg.group_leader, TCB_RUNNABLE);
  release(&np->tg.group_leader->lock);
  // release(&np->tg.lock);
  // release(&np->lock);
  
  return pid;
}

/**
 * @brief clone a new process, copying the parent.
 * 
 * @property  * @property int clone(typeof(int (void *_Nullable)) *fn,
                 void *stack,
                 int flags,
                 pid_t *_Nullable parent_tid,
                    void *_Nullable tls,
                    pid_t *_Nullable child_tid 

 * @param flags flags for clone, see sched.h
 * @param stack stack address for the new process
 * @param ptid ptid address for the new process, used for CLONE_PARENT_SETTID
 * @param tls tls address for the new process, used for CLONE_SETTLS
 * The TLS (Thread Local Storage) descriptor is set to tls.

              The interpretation of tls and the resulting effect is
              architecture dependent.  On x86, tls is interpreted as a
              struct user_desc * (see set_thread_area(2)).  On x86-64 it
              is the new value to be set for the %fs base register (see
              the ARCH_SET_FS argument to arch_prctl(2)).  On
              architectures with a dedicated TLS register, it is the new
              value of that register. tp in arch risc-v
 * @param ctid ctid address for the new process, used for CLONE_CHILD_CLEARTID and CLONE_CHILD_SETTID
 * @return return the pid of the new process if success, -1 if failed
 */
int do_clone(int flags, uint64 stack, uint64 ptid, uint64 tls, uint64 ctid)
{
  int i, pid;
  struct proc *np = NULL;
  struct proc *p = myproc();
  struct tcb *t = NULL;

  if(flags & CLONE_THREAD) {
    // if CLONE_THREAD, we just create a new thread in the same process
    if((t = alloc_thread(thread_forkret)) == 0)
      return -1; 
    if(proc_join_thread(p, t, NULL) < 0) {
      free_thread(t);
      return -1;
    }
  } else {
    // allocate a process
    if((np = create_proc()) == 0) {
      return -1;
    }
    t = np->tg.group_leader;
  }
  // TODO: let's just copy the leader thread right now
  // copy saved user registers.
  *(t->trapframe) = *(p->tg.group_leader->trapframe);
  // Cause fork to return 0 in the child.
  t->trapframe->a0 = 0;

  if(flags & CLONE_SETTLS) {
    printf("CLONE_SETTLS\n");
    t->trapframe->tp = tls; // set the tp register
  }
  if(flags & CLONE_CHILD_SETTID) {
    printf("CLONE_CHILD_SETTID\n");
    if(copyout(p->mm.pagetable, ctid, (char *)&t->tid, sizeof(t->tid)) < 0) {
      goto bad;
    }
    t->set_child_tid = ctid; // for debug
  }
  if(flags & CLONE_CHILD_CLEARTID) {
    printf("CLONE_CHILD_CLEARTID\n");
    // clear when exit
    t->clear_child_tid = ctid;
  }
  if(stack) {
    printf("set stack to %p\n", stack);
    t->trapframe->sp = stack;
  }
  if(flags & CLONE_SIGHAND) {
    // not support yet
    printf("CLONE_SIGHAND\n");
  }
  if(flags & CLONE_PARENT_SETTID) {
    printf("CLONE_PARENT_SETTID\n");
    // set the parent tid
    if(copyout(p->mm.pagetable, ptid, (char *)&t->tid, sizeof(t->tid)) < 0) {
      goto bad;
    }
  }
  if(np == NULL) {
    // if np is NULL, we are just creating a thread
    tcb_q_change_state(t, TCB_RUNNABLE);
    release(&t->lock);
    return t->tid; // return the thread id
  }

  /*
    create a process
  */
  if(flags & CLONE_VM) {
    printf("CLONE_VM\n");
    np->mm.pagetable = p->mm.pagetable; // share the same pagetable
  } else {
    if(uvmcopy(p->mm.pagetable, np->mm.pagetable, p->sz) < 0){
      freeproc(np);
      release(&np->lock);
      return -1;
    }
  }
  // just not support right now
  if(flags & CLONE_FS) {
    printf("CLONE_FS\n");
  }
  if(flags & CLONE_FILES) {
    printf("CLONE_FILES\n");
  }
  if(proc_copy_vma(p, np) < 0) {
    freeproc(np);
    release(&np->lock);
    return -1;
  }
  np->sz = p->sz;
  // increment reference counts on open file descriptors.
  // child process "open" the files
  for(i = 0; i < NOFILE; i++)
    if(p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);

  safestrcpy(np->name, p->name, sizeof(p->name));

  pid = np->pid;
  strncpy(np->cinfo.path, p->cinfo.path, MAXPATH);

  release(&np->lock);

  acquire(&wait_lock);
  np->parent = p;
  release(&wait_lock);

  acquire(&p->lock);
  append_child(p, np);
  release(&p->lock);
  
  acquire(&t->lock);
  tcb_q_change_state(t, TCB_RUNNABLE);
  release(&t->lock);

  return pid;

bad:
  printf("bad \n");
  if(np) {
    freeproc(np);
  }
  if(t) {
    free_thread(t);
  }
  return -1;
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

/**
 * @brief Exit the current process.  Does not return.
 * @brief An exited process remains in the zombie state
 * @brief until its parent calls wait().
 * 
 * @param status exit status 
 * @attention never call this function with thread's lock held!!!!!, also with no other lock held!!!! (noff must == 0)
 * @details this function is called after every thread in the process has exited
 */
void proc_exit(int status)
{
  struct proc *p = myproc();
  // struct tcb *t = mythread();
  if(p == initproc)
    panic("init exiting");

  // Close all open files.
  for(int fd = 0; fd < NOFILE; fd++){
    if(p->ofile[fd]){
      struct file *f = p->ofile[fd];
      if(f->ref > 0)
        fileclose(f);
      p->ofile[fd] = 0;
    }
  }
  freeprocvm(p);

  memset(&p->cinfo, 0, sizeof(p->cinfo));

  acquire(&wait_lock);
  
  acquire(&p->lth_exitlock);
  // Give any children to init.
  reparent(p);

  // Parent might be sleeping in wait().
  thread_wakeup_chan(p->parent);
  
  acquire(&p->lock);

  p->xstate = status;
  p->xstate <<= 8; // shift to high byte
  pcb_q_change_state(p, ZOMBIE);
  release(&wait_lock);

  // Jump into the scheduler, never to return.
  // thread_sched();
  // panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait_one(uint64 addr)
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
    if(!havekids || proc_killed(p)){
      release(&wait_lock);
      return -1;
    }

#ifdef __DEBUG_WAIT
    Info("thread %d wait, ready to sleep\n", mythread()->tid);
#endif
    // Wait for a child to exit.
    thread_sleep(p, &wait_lock);  //DOC: wait-sleep
#ifdef __DEBUG_WAIT
    Info("thread %d wait, waked up\n", mythread()->tid);
#endif
  }
}

/**
 * @brief wait for a specific child process to exit
 * 
 * @param pid child pid
 * @param pstatus child process's status
 * @param options options to specify the behavior of wait4, see wait.h
 * @return child process's pid
 */
pid_t wait4(pid_t pid, uint64 pstatus, int options) {
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
    if(!havekids || proc_killed(p)){
      release(&wait_lock);
      return -1;
    }

    // WNOHANG: return immediately if no child has exited.
    if(options & WNOHANG) {
      release(&wait_lock);
      return 0;
    }
    // Wait for a child to exit.
    // TODO: how to wait for a specific child process to exit??
    // TODO: bug here: any child process will wake up this parent process
    thread_sleep(p, &wait_lock);  //DOC: wait-sleep
  }
  
}

/**
 * @brief entrance for waiting for a specific child process to exit
 * 
 * @param pid -1 means wait for any child process, any value greater than 0 means wait for the specific child process
 * @param wstatus the status of the exited child process
 * @param options specify the behavior of waitpid, in wait.h
 * @return child process's pid
 */
pid_t waitpid(pid_t pid, uint64 wstatus, int options) {
  if(pid < -1 || pid == 0) {
    panic("waitpid: not support pid < -1 || pid == 0 right now!, for no process group!");
  }
  if(pid == -1 && options == 0) {
    return wait_one(wstatus);
  } else {
    return wait4(pid, wstatus, options);
  }
}



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
#ifdef __DEBUG_KILL
      Info("kill: pid %d, name %s", p->pid, p->name);
#endif //__DEBUG_KILL
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
proc_setkilled(struct proc *p)
{
  acquire(&p->lock);
  p->killed = 1;
  release(&p->lock);
}

int
proc_killed(struct proc *p)
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

int procs_cnt(void) {
  int cnt = 0;

  cnt += get_queue_count(&used_p_q);
  cnt += get_queue_count(&zombie_p_q);

  return cnt;
}

/**
 * @brief send a signal to all threads in the process
 * 
 * @param p process
 * @param signo signal number
 * @param opt options for the signal, see signal.h
 */
void proc_sendsignal_all_thread(struct proc *p, sig_t signo, int opt) {
  struct tcb *t_cur = NULL;
  struct tcb *t_tmp = NULL;
  siginfo_t info;
  acquire(&p->tg.lock);
  list_for_each_entry_safe(t_cur, t_tmp, &p->tg.threads, threads) {
      signal_info_init(signo, &info, opt);

      acquire(&t_cur->lock);
      thread_send_signal(t_cur, &info);
      release(&t_cur->lock);
  }
  release(&p->tg.lock);
  if (signo == SIGKILL || signo == SIGSTOP) {
      proc_setkilled(p);
  }
}
