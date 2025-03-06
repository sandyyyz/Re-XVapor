#include "types.h"
#include "queue.h"
#include "proc.h"
#include "thread.h"
#include "sched.h"
#include "debug.h"
#include "riscv.h"

queue_t unused_p_q, used_p_q, zombie_p_q;
queue_t *g_pcb_queues[PROC_STATEMAX] = {
    [UNUSED] & unused_p_q,
    [USED] & used_p_q,
    [ZOMBIE] & zombie_p_q};

extern queue_t *g_tcb_queues[TCB_MAX_STATE];
extern struct proc proc[NPROC];
extern struct tcb thread[NTHREADS];

void PCB_Q_ALL_INIT() {
    queue_init(&unused_p_q, "PCB_UNUSED", PCB_STATE_QUEUE);
    queue_init(&used_p_q, "PCB_USED", PCB_STATE_QUEUE);
    queue_init(&zombie_p_q, "PCB_ZOMBIE", PCB_STATE_QUEUE);
}



/// @brief change the process's state
/// @param p given process
/// @param  state_new the new state
/// @attention must hold the p->lock. return with the process's lock held
void pcb_q_change_state(struct proc *p, enum procstate state_new) {

    queue_t *pcb_q_new = g_pcb_queues[state_new];

    // acquire(&p->lock);
    // maybe inconsistent here??
    queue_t *pcb_q_old = g_pcb_queues[p->state];

    queue_remove_atomic(pcb_q_old, (void *)p);
    queue_push_back_atomic(pcb_q_new, (void *)p);
    p->state = state_new;

    // release(&p->lock);
    return;
}

/// @brief change the given tcb's state except for turning to tcb_runnnig!!
/// @details Special distinction is made for the tcb_runnning queue
/// @param t tcb
/// @param  state_new the new state
/// @attention must hold the t->lock. return with the thread's lock held. cannot use for turning to tcb_runnnig!!
void tcb_q_change_state(struct tcb *t, enum thread_state state_new) {
    queue_t *tcb_q_new = g_tcb_queues[state_new];

    // acquire(&t->lock);
    queue_t *tcb_q_old = g_tcb_queues[t->state];

    if (t->state != TCB_RUNNING) {
        queue_remove_atomic(tcb_q_old, (void *)t);
    } else {
        queue_remove((void *)t, TCB_STATE_QUEUE);
    }
    queue_push_back_atomic(tcb_q_new, (void *)t);


    t->state = state_new;
    return;
}

/**
 * @brief change the tcb's state to tcb_running 
 * @attention must hold the t->lock, return with the thread's lock held. make sure the t has been removed from it's old state_queue
 * @param t given tcb
 */
void inline tcb_change2_running(struct tcb *t) {
    t->state = TCB_RUNNING;
}


void thread_yield(void) {
    struct tcb *t = mythread();
    acquire(&t->lock);

    tcb_q_change_state(t, TCB_RUNNABLE);

    thread_sched();

    release(&t->lock);
}


void thread_sched(void) {
    int intena;
    struct tcb *t = mythread();

    if(!holding(&t->lock))
    panic("sched t->lock");
    if(mycpu()->noff != 1)
    panic("sched t locks");
    if(t->state == TCB_RUNNING)
    panic("sched t running");
    if(intr_get())
    panic("sched t interruptible");


    intena = mycpu()->intena;
    swtch(&t->context, &mycpu()->context);
    mycpu()->intena = intena;

}

void thread_scheduler(void) {
    struct tcb *t;
    struct  cpu *c = mycpu();

    c->thread = 0;
    for (;;) {
        // Avoid deadlock by ensuring that devices can interrupt.
        intr_on();

        t = (struct tcb *)queue_pop_atomic(g_tcb_queues[TCB_RUNNABLE], 1); // remove it
        if (t == NULL)
            continue;
        else 
#ifdef __DEBUG_SCHED
            printf("thread %d is running\n", t->tid);

        printf("thread %d try to get the lock\n",t->tid);
#endif
        acquire(&t->lock);

#ifdef __DEBUG_SCHED
        printf("thread %d get the lock\n",t->tid);
#endif
        // t->state = TCB_RUNNING;
        // tcb_q_change_state(t, TCB_RUNNING); // buggy
        tcb_change2_running(t);
        c->thread = t;
        swtch(&c->context, &t->context);
        c->thread = 0;
#ifdef __DEBUG_SCHED
        printf("thread %d try to release the lock\n",t->tid);
#endif        
        release(&t->lock);
#ifdef __DEBUG_SCHED
        printf("thread %d release the lock\n",t->tid);
#endif
    }
}