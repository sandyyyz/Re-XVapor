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
        queue_push_back(g_tcb_queues[TCB_UNUSED], t);
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


/// @brief allocate a new thread with new context, but the trapframe is not set yet
/// @param callback callback of thread
/// @return return the new thread
struct tcb *alloc_thread(thread_callback callback) {
    struct tcb *t;

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

    // chage state of TCB
    tcb_q_change_state(t, TCB_USED);

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

    tcb_q_change_state(t, TCB_RUNNABLE);
    release(&t->lock);

}

// free a thread
void free_thread(struct tcb *t) {
    // free & unmap tramframe
    acquire(&t->p->mm.lock);
    if (t->trapframe)
        uvmunmap(t->p->mm.pagetable, THREAD_TRAPFRAME(t->tidx), 1, 1);
    else
        uvmunmap(t->p->mm.pagetable, THREAD_TRAPFRAME(t->tidx), 1, 0);
    release(&t->p->mm.lock);

    // // bug!
    // if (t->wait_chan_entry != NULL) {
    //     // Queue_remove_atomic(thread->wait_chan_entry, (void *)thread);
    //     ASSERT(thread->state == TCB_SLEEPING);
    //     thread->wait_chan_entry = NULL;
    // }
    // // bug!
    // if (t->sig) {
    //     // !!! for shared
    //     int ref = atomic_dec_return(&t->sig->ref) - 1;
    //     if (ref == 0) {
    //         kfree((void *)t->sig);
    //     }
    //     t->sig = NULL;
    // }

    // delete <tid, t>
    // hash_delete(&tid_map, (void *)&t->tid, 0, 1); // not holding lock, release lock

    // cnt_tid_dec;

    t->tid = 0;
    t->tidx = 0;
    t->trapframe = 0;
    t->name[0] = 0;
    // t->exit_status = 0;
    t->p = 0;
    // t->sig_pending_cnt = 0;
    // t->sig_ing = 0;
    memset(&t->context, 0, sizeof(t->context));

    // signal_queue_flush(&t->pending); // !!!
    t->killed = 0;                   // !!! bug qwq

    tcb_q_change_state(t, TCB_UNUSED);
}


/// @brief thread join to process's thread group
/// @param p process
/// @param t thread
/// @param name thread's name
/// @return return 0 if success, -1 if map thread trapframe failed
int proc_join_thread(struct proc *p, struct tcb *t, char *name) {
    struct thread_group *tg = &(p->tg);

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

// // send signal to all threads of proc p
// void proc_sendsignal_all_thread(struct proc *p, sig_t signo, int opt) {
//     struct tcb *t_cur = NULL;
//     struct tcb *t_tmp = NULL;
//     siginfo_t info;
//     acquire(&p->tg->lock);
//     list_for_each_entry_safe(t_cur, t_tmp, &p->tg->threads, threads) {

//         signal_info_init(signo, &info, opt);

//         acquire(&t_cur->lock);
//         thread_send_signal(t_cur, &info);
//         release(&t_cur->lock);
//     }
//     release(&p->tg->lock);
//     if (signo == SIGKILL || signo == SIGSTOP) {
//         proc_setkilled(p);
//     }
// }


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
thread_sleep(void *chan, struct spinlock *lk)
{
//   struct proc *p = myproc();
  struct tcb *t  = mythread();
  // Must acquire p->lock in order to
  // change p->state and then call sched.
  // Once we hold p->lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup locks p->lock),
  // so it's okay to release lk.

  // acquire(&p->lock);  //DOC: sleeplock1
  acquire(&t->lock);
  release(lk);

  // Go to sleep.
  t->chan = chan;

  // p->state = SLEEPING;
  tcb_q_change_state(t, TCB_SLEEPING);

  // sched();
  thread_sched();

  // Tidy up.
  // p->chan = 0;
  t->chan = 0;
  
  // Reacquire original lock.
  release(&t->lock);
  acquire(lk);
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

    struct tcb *t, *tt;
    struct tcb *cur_threads = mythread();
    queue_for_each_entry_safe(t, tt, g_tcb_queues[TCB_SLEEPING], wait_list) {
        if(t != cur_threads) {
            acquire(&t->lock);
            if(t->chan == chan) {
                tcb_q_change_state(t, TCB_RUNNABLE);
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

    ASSERT(t->wait_chan_entry != NULL);
    queue_remove_atomic(t->wait_chan_entry, (void *)t);
    ASSERT(t->state == TCB_SLEEPING);
    t->wait_chan_entry = NULL;
    tcb_q_change_state(t, TCB_RUNNABLE);

    release(&t->lock);
}


/// @brief wake up a given thread, call and return with thread's lock held 
/// @param t given thread
void thread_wakeup_specific(struct tcb *t) {
    ASSERT(t->wait_chan_entry != NULL);
    queue_remove_atomic(t->wait_chan_entry, (void *)t);
    ASSERT(t->state == TCB_SLEEPING);
    t->wait_chan_entry = NULL;
    tcb_q_change_state(t, TCB_RUNNABLE);
}


