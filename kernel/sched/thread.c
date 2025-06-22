#include "types.h"
#include "thread.h"
#include "queue.h"
#include "param.h"
#include "debug.h"
#include "riscv.h"
#include "memlayout.h"
#include "sched.h"
#include "debug.h"
#include "vm.h"
#include "proc.h"
#include "vfs.h"
#include "signal.h"
#include "futex.h"
#include "trap.h"

queue_t unused_t_queue, used_t_queue, runnable_t_queue, sleeping_t_queue;

atomic_t next_tid = ATOMIC_INIT(1); // tid

static inline tid_t alloctid() {return atomic_inc_return(&next_tid);}


void TCB_Q_ALL_INIT() {
    queue_init(&unused_t_queue, "TCB_UNUSED", TCB_STATE_QUEUE);
    queue_init(&used_t_queue, "TCB_USED", TCB_STATE_QUEUE);
    queue_init(&runnable_t_queue, "TCB_RUNNABLE", TCB_STATE_QUEUE);
    queue_init(&sleeping_t_queue, "TCB_SLEEPING", TCB_STATE_QUEUE);
}

queue_t *g_tcb_queues[TCB_MAX_STATE] = {
    [TCB_UNUSED] &unused_t_queue,
    [TCB_USED] &used_t_queue,
    [TCB_RUNNABLE] &runnable_t_queue,
    [TCB_SLEEPING] &sleeping_t_queue,
};

tcb_t tcb_pool[NTHREADS];


// tcb init
void tcb_init(void) {
    struct tcb *t;
    TCB_Q_ALL_INIT();
    for (int i = 0; i < NTHREADS; i++) {
        t = tcb_pool + i;
        initlock(&t->lock, "tcb_lock"); // init its spinlock
        t->state = TCB_UNUSED;
        t->kstack = KSTACK((int)(t - tcb_pool));
#ifdef __DEBUG_TCB_INIT
        Log("thread %d alloc with kstack %p", i + 1, t->kstack);
#endif
        queue_push_back_atomic(g_tcb_queues[TCB_UNUSED], t);
    }
    Info("thread table init [ok]\n");
    return;
}

tcb_t* mythread(void) {
    push_off();
    struct cpu *c = mycpu();
    struct tcb *thread = c->thread;
    pop_off();
    return thread;
}


void thread_forkret(void)
{
#ifdef __DEBUG_FORKRET
    // printf_blue("into forkret!\n");
    Log("thread %d come in forkret!\n", mythread()->tid); 

#endif

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


  usertrapret();
}

/// @brief allocate a new thread with new context, but the trapframe is not set yet
/// @param callback callback of thread
/// @return return the new thread with the lock held
struct tcb *alloc_thread(thread_callback callback) {
    struct tcb *t = NULL;

    t = (struct tcb *)queue_pop_atomic(g_tcb_queues[TCB_UNUSED], 1);

    if (t == NULL)
        goto no_unused;

    acquire(&t->lock);  

    // spinlock and threads list head
    INIT_LIST_HEAD(&t->threads);

    t->tid = alloctid();

    memset(&t->context, 0, sizeof(t->context));
    t->context.ra = (uint64)callback;
    t->context.sp = t->kstack + KSTACK_PAGE * PGSIZE;
#ifdef __DEBUG_ALLOCATE_THREAD
    Warn("thread %d alloc with kstack base %p, kstack top %p", t->tid, t->context.sp, t->kstack);
#endif
    sig_empty_set(&t->blocked);
    // sig_fill_set(&t->blocked);
    sigpending_init(&t->sig_pending);
    // chage state of TCB
    tcb_q_change_state(t, TCB_USED);

    t->set_child_tid = 0;
    t->clear_child_tid = 0;
    t->killed = 0;
    t->chan = NULL;
    t->timeout = 0; // for futex
    t->wait_chan_entry = NULL;
    
#ifdef __DEBUG_ALLOCATE_THREAD
    Log("allocate thread %d\n", t->tid);
#endif
no_unused:
    return t;
}



/// @brief create a runnable thread
/// @param p process it belongs to
/// @param t thread
/// @param name name of thread
/// @param callback callback of thread
void create_thread(struct proc *p, struct tcb *t, char *name, thread_callback callback) {
    ASSERT(p != NULL);

    if ((t = alloc_thread(callback)) == NULL) {
        panic("no free thread\n");
    }

    proc_join_thread(p, t, name);

#ifdef __DEBUG_CREATE_THREAD
    Log("create thread %d\n", t->tid);
#endif
    tcb_q_change_state(t, TCB_RUNNABLE);
    release(&t->lock);

}

/**
 * @brief free a thread， and move it to the unused queue
 * 
 * @param t thread
 * @attention must hold the t->lock, and remenber to remove this thread of the thread group outside
 * @details return with the lock held
 */
void free_thread(struct tcb *t) {

#ifdef __DEBUG_FREE_THREAD
    Log("thread %d free thread %d\n", mythread()->tid, t->tid);
#endif
    // free & unmap tramframe
    acquire(&t->p->mm.lock);
    if (t->trapframe)
        uvmunmap(t->p->mm.pagetable, THREAD_TRAPFRAME(t->tidx), 1, 1);
    else
        uvmunmap(t->p->mm.pagetable, THREAD_TRAPFRAME(t->tidx), 1, 0);
    release(&t->p->mm.lock);



    t->tid = 0; 
    t->tidx = 0;
    t->trapframe = 0;
    t->name[0] = 0;
    t->p = 0;
    memset(&t->context, 0, sizeof(t->context));
    t->killed = 0;  
    t->pending_cnt = 0;
    t->sig_processing = 0;
    t->timeout = 0;
    signal_queue_flush(&t->sig_pending);
    t->chan = NULL;
    t->wait_chan_entry = NULL;
    if(t->sigs) {
        int ref = atomic_dec_return(&t->sigs->ref);
        if(ref == 1) {
            // last thread using this sighand, free it
            kfree(t->sigs);
        }
        t->sigs = NULL;
    }
    tcb_q_change_state(t, TCB_UNUSED);
}


/// @brief thread join to process's thread group
/// @param p process
/// @param t thread
/// @param name thread's name
/// @return return 0 if success, -1 if map thread trapframe failed. return with thread's lock held
/// @attention call with the thread lock held
/// @details now allocate and map a trapframe for the thread
int proc_join_thread(struct proc *p, struct tcb *t, char *name) {
    struct thread_group *tg = &(p->tg);
    t->tg = tg;
    atomic_inc_return(&tg->thread_cnt);
    
    acquire(&tg->lock);
    if (tg->group_leader == NULL) {
        tg->group_leader = t;
    }
    list_add_tail(&t->threads, &p->tg.threads);
    tg->tgid = p->pid;
    t->tidx = tg->thread_idx++;
    t->p = p;
    release(&tg->lock);

    acquire(&t->p->mm.lock);
    // Log("thread idx is %d, within group %d", t->tidx, p->pid);
    if ((t->trapframe = uvm_thread_trapframe(p->mm.pagetable, t->tidx)) == 0) {   
        release(&t->p->mm.lock);
        return -1;
    }

    release(&t->p->mm.lock);

    // vmprint(p->mm->pagetable, 0, 0, MAXVA - 512 * PGSIZE, 0);
    if (name == NULL) {
        // char name_tmp[20];
        // snprintf(name_tmp, 20, "%s-%d", p->name, t->tidx);
        strncpy(t->name, "testname", 20);
    } else {
        strncpy(t->name, name, 20);
    }


    return 0;
}

/// @brief set thread's killed = 1
/// @param t thread
void thread_setkilled(struct tcb *t) {
    acquire(&t->lock);
    t->killed = 1;
    release(&t->lock);
}


/// @brief is thread killed?
/// @param t 
/// @return t->killed
int thread_killed(struct tcb *t) {
    int k;

    acquire(&t->lock);
    k = t->killed;
    release(&t->lock);
    return k;
}

void thread_exit(int status) {
    struct tcb *t = mythread();
    struct proc *p = t->p;
    struct thread_group *tg = &(p->tg);
    int thread_cnt = 0;

    if( t->state == TCB_SLEEPING) 
        thread_wakeup_specific(t);
    
    if(t->set_child_tid) {
        thread_wakeup_chan((void*)t->set_child_tid);
    }
    if(t->clear_child_tid) {
        tid_t clear = 0;
        // thread_wakeup_chan((void*)t->clear_child_tid);
        copyout(p->mm.pagetable, t->clear_child_tid, (char *)&clear, sizeof(t->tid));
        futex_wake(t->clear_child_tid, 1);
    }

    if((thread_cnt = atomic_dec_return(&tg->thread_cnt))== 1) {

        proc_exit(status);
        release(&p->lock);
    }

    acquire(&p->tg.lock);
    list_del_reinit(&t->threads);
    if(p->tg.group_leader == t) {
        p->tg.group_leader = list_first_entry(&p->tg.threads, struct tcb, threads);
    }
    release(&p->tg.lock);
    
    acquire(&t->lock);
    free_thread(t);
    // tcb_q_change_state(t, TCB_UNUSED);
    if(thread_cnt == 1)
        release(&p->lth_exitlock);

    thread_sched();
    

    return;
}
/// @brief initialize thread group
/// @param tg thread group
void tginit(struct thread_group *tg) {
    initlock(&tg->lock, "thread group lock");
    tg->group_leader = NULL;
    atomic_set(&tg->thread_cnt, 0);
    tg->thread_idx = 0;
    INIT_LIST_HEAD(&tg->threads);
}

// 传入chan来标记sleep原因？
// 即wait channel
// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
thread_sleep(void *chan, struct spinlock *lk, __nullable const struct timespec *timeout)
{
//   struct proc *p = myproc();
  struct tcb *t  = mythread();
  // Must acquire p->lock in order to
  // change p->state and then call sched.
  // Once we hold p->lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup locks p->lock),
  // so it's okay to release lk.
#ifdef __DEBUG_TSLEEP
    // Info("noff when enter tsleep: %d\n", mycpu()->noff);
#endif
  // acquire(&p->lock);  //DOC: sleeplock1
  acquire(&t->lock);
  release(lk);

  // Go to sleep.
  t->chan = chan;
  if(timeout)
    t->timeout = get_timeout_ticks(timeout);
  // p->state = SLEEPING;
  tcb_q_change_state(t, TCB_SLEEPING);
  // sched();
#ifdef __DEBUG_TSLEEP
    if(t->timeout)
        Log("thread %d sleep on chan %p, timeout %d", t->tid, chan, t->timeout);
#endif
  thread_sched();
#ifdef __DEBUG_TSLEEP
    if(t->timeout)
        Log("thread %d wakeup on chan %p, timeout %d", t->tid, chan, t->timeout);
#endif
  // Tidy up.
  // p->chan = 0;
  t->chan = 0;
  t->timeout = 0; // reset timeout
  
  // Reacquire original lock.
  release(&t->lock);
  acquire(lk);
}
void thread_wakeup_timeout(uint ticks_now)
 {
    struct tcb *t;
    struct tcb *cur_thread = mythread();
    for(t = tcb_pool; t < &tcb_pool[NTHREADS]; t++) {
        if(t != cur_thread) {
            acquire(&t->lock);
            if(t->state == TCB_SLEEPING && t->timeout == ticks_now) {
#ifdef __DEBUG_WAKEUP_TIMEOUT
                Log("thread_wakeup_timeout: thread %d wakeup on timeout %d", t->tid, t->timeout);
#endif
                tcb_q_change_state(t, TCB_RUNNABLE);
                // t->timeout = UINT64_MAX; // reset timeout
                release(&t->lock);
                break;
            }
            release(&t->lock);
        }
    }
    return;
}

void thread_wakeup_chan_timeout(void *chan, uint ticks_now)
{
    struct tcb *t;
    struct tcb *cur_thread = mythread();
    for(t = tcb_pool; t < &tcb_pool[NTHREADS]; t++) {
        if(t != cur_thread) {
            acquire(&t->lock);
            if(t->state == TCB_SLEEPING && t->chan == chan && t->timeout == ticks_now) {
#ifdef __DEBUG_WAKEUP_CHAN_TIMEOUT
                Log("thread_wakeup_chan_timeout: thread %d wakeup on chan %p, timeout %d", t->tid, chan, t->timeout);
#endif
                tcb_q_change_state(t, TCB_RUNNABLE);
                t->timeout = 0; // reset timeout
                release(&t->lock);
                break;
            }
            release(&t->lock);
        }
    }
}

/**
 * @brief // Wake up all threads sleeping on chan. Must be called without any p->lock.
 * 
 * @param chan sleeping channel
 */
void
thread_wakeup_chan(void *chan)
{
//   struct proc *p;

    struct tcb *t;
    struct tcb *cur_threads = mythread();
//     acquire(&g_tcb_queues[TCB_SLEEPING]->lock);
//     queue_for_each_entry_safe(t, tt, g_tcb_queues[TCB_SLEEPING], state_list) {
// #ifdef __DEBUG_WAKEUP_CHAN
//         // if(!t) Log("wakeup tid: %d, with state %d", t->tid, t->state);
// #endif
//         if(t != cur_threads) {
// #ifdef __DEBUG_WAKEUP_CHAN
//             Log("thread_wakeup_chan get thread %d with state = %d", t->tid, t->state);
// #endif
//             acquire(&t->lock);
//             if(t->chan == chan) {
//                 // tcb_q_change_state(t, TCB_RUNNABLE);
//                 queue_t *tcb_q_new = g_tcb_queues[TCB_RUNNABLE];
//                 // queue_t *tcb_q_old = g_tcb_queues[TCB_SLEEPING];
//                 queue_remove(t, TCB_STATE_QUEUE);
//                 queue_push_back_atomic(tcb_q_new, t);
//                 t->chan = 0;
//                 t->state = TCB_RUNNABLE;

// #ifdef __DEBUG_WAKEUP_CHAN
//                 Log("thread_wakeup_chan %d at chan %p", t->tid, chan);
// #endif
//             }

//             release(&t->lock);
//         }
//     }
//     release(&g_tcb_queues[TCB_SLEEPING]->lock);

    for(t = tcb_pool; t < &tcb_pool[NTHREADS]; t++) {
        if(t!= cur_threads) {
            acquire(&t->lock);
            if(t->chan == chan && t->state == TCB_SLEEPING) {
#ifdef __DEBUG_WAKEUP_CHAN
                if(chan == &ticks)
                    Log("thread_wakeup_chan %d at chan %p", t->tid, chan);
#endif
                tcb_q_change_state(t, TCB_RUNNABLE);
                // queue_t *tcb_q_new = g_tcb_queues[TCB_RUNNABLE];
                // queue_t *tcb_q_old = g_tcb_queues[TCB_SLEEPING];
                // queue_remove(t, TCB_STATE_QUEUE);
                // queue_push_back_atomic(tcb_q_new, t);
                // t->chan = 0;
                // t->state = TCB_RUNNABLE;
            }
            release(&t->lock);
        }
    }
}
/// @brief wake up a given thread atomic, meaning that we needn't hold thread's lock in advance,
/// @brief we acquire the lock first, and release it at the end
/// 
/// @param t given thread
void thread_wakeup_specific_atomic(struct tcb *t) {

    acquire(&t->lock);

    // ASSERT(t->wait_chan_entry != NULL);
    // queue_remove_atomic(t->wait_chan_entry, (void *)t);
    ASSERT(t->state == TCB_SLEEPING);
    // t->wait_chan_entry = NULL;
    ASSERT(t->chan != NULL || t->wait_chan_entry != NULL);
    t->chan = NULL; // clear the chan, so that we can wake it up
    tcb_q_change_state(t, TCB_RUNNABLE);

    release(&t->lock);
}


/// @brief wake up a given thread, call and return with thread's lock held 
/// @param t given thread
void thread_wakeup_specific(struct tcb *t) {
    // ASSERT(t->wait_chan_entry != NULL);
    // queue_remove_atomic(t->wait_chan_entry, (void *)t);
    ASSERT(t->state == TCB_SLEEPING);
    // t->wait_chan_entry = NULL;
    ASSERT(t->chan != NULL || t->wait_chan_entry != NULL);
    t->chan = NULL; // clear the chan, so that we can wake it up
    tcb_q_change_state(t, TCB_RUNNABLE);
}


void print_trapframe(struct trapframe *tf) {
    printf("trapframe at %p\n", tf);
    printf("  kernel_satp %p\n", tf->kernel_satp);
    printf("  kernel_sp %p\n", tf->kernel_sp);
    printf("  kernel_trap %p\n", tf->kernel_trap);
    printf("  kernel_hartid %p\n", tf->kernel_hartid);
    printf("  epc %p\n", tf->epc);
    printf("  ra %p\n", tf->ra);
    printf("  sp %p\n", tf->sp);
    printf("  gp %p\n", tf->gp);
    printf("  tp %p\n", tf->tp);
    printf("  t0 %p\n", tf->t0);
}

/// @brief transfer thread's trapframe to a newpgtble
/// @param t thread
/// @param newpgtble target pgtble
/// @param unmmap_old unmmap trapframe in the old pgtable if == 1
void transfer_trapframe(struct tcb* t, pagetable_t newpgtble, int unmmap_old) {
    pagetable_t oldpgtble = t->p->mm.pagetable;
    struct trapframe *tf = t->trapframe;
    uint64 tfva = THREAD_TRAPFRAME(t->tidx);

    if(unmmap_old){
    acquire(&t->p->mm.lock);
    uvmunmap(oldpgtble, tfva, 1, 0); 
    release(&t->p->mm.lock);
    }

    if(mappages(newpgtble, tfva, PGSIZE, (uint64)tf, PTE_R | PTE_W) < 0) {
        panic("transfer trapframe failed\n");
    }
}


/**
 * @brief free all other threads in the same group, except the given thread
 * 
 * @param t thread
 * @return return 0 if success
 */
int free_allother_threads_group(struct tcb *t) {
    struct tcb *tt, *ntt;
    struct thread_group *tg = t->tg;

    acquire(&tg->lock);
    list_for_each_entry_safe(tt, ntt,&(tg->threads), threads) {
        acquire(&tt->lock);
        if(tt != t) {
            free_thread(tt);
            if(t->tg->group_leader == tt) {
                t->tg->group_leader = mythread();
            }
            list_del_reinit(&tt->threads);
        }
        release(&tt->lock);
    }
    release(&tg->lock);

    return 0;
}

void sighandinit(struct tcb *t) {
    if ((t->sigs = kmalloc(sizeof(struct sighand))) == NULL) {
        panic("sighandinit: kmalloc failed\n");
    }
    initlock(&t->sigs->siglock, "sighand lock");
    atomic_set(&t->sigs->ref, 1);
}

/**
 * @brief send a signal to the given thread
 * 
 * @attention should call with the thread's lock held
 * @param t_given given thread to send signal
 * @param info siginfo_t pointer containing signal information
 * @details this function will send a signal to the given thread, and wake it up if it is sleeping.
 */
void thread_send_signal(struct tcb *t_given, siginfo_t *info) {
    signal_send(info, t_given);

    if (t_given->state == TCB_SLEEPING) {
        thread_wakeup_specific(t_given);
    }

    switch(info->si_signo) {
        case SIGKILL:  
        case SIGSTOP:
        t_given->killed = 1;
            break;
        default:
            break;
    }
    
    return;
}

int thread_kill(int tid, sig_t sig) {
    struct tcb *t = NULL;
    siginfo_t info;
    if(tid < 0 || tid >= NTHREADS) {
        Warn("thread_kill: invalid tid %d", tid);
        return -1;
    }
    for(t = tcb_pool; t < &tcb_pool[NTHREADS]; t++) {
        acquire(&t->lock);
        if(t->tid == tid) {
            signal_info_init(sig, &info, 0);
            thread_send_signal(t, &info);
            release(&t->lock);
            // one thread may wait for the sig
            acquire(&t->sig_pending.siglock);
            thread_wakeup_chan_timeout((void*) sig, get_ticksnow());
            release(&t->sig_pending.siglock);
            return 0;
        }
        release(&t->lock);
    }
    Warn("thread_kill: thread %d not found", tid);
    return -1;
}

int thread_group_kill(int tgid, int tid, sig_t sig) {
    struct tcb *t = NULL;
    siginfo_t info;
    if(tid < 0 || tid >= NTHREADS) {
        Warn("thread_group_kill: invalid tid %d", tid);
        return -1;
    }
    for(t = tcb_pool; t < &tcb_pool[NTHREADS]; t++) {
        acquire(&t->lock);
        if(t->tg->tgid == tgid && t->tid == tid) {
            signal_info_init(sig, &info, 0);
            thread_send_signal(t, &info);
            release(&t->lock);

            // one thread may wait for the sig
            acquire(&t->sig_pending.siglock);
            thread_wakeup_chan_timeout((void*) sig, get_ticksnow());
            release(&t->sig_pending.siglock);
            return 0;
        }
        release(&t->lock);
    }
    Warn("thread_group_kill: thread %d not found in group %d", tid, tgid);
    return -1;
}

uint get_timeout_ticks(const struct timespec *ts) {
    acquire(&tickslock);
    uint ticks0 = ticks;
    release(&tickslock);
    uint rticks = TIMESPEC2TICKS(*ts);
    return rticks + ticks0;
}  