#include "types.h"
#include "proc.h"
#include "thread.h"
#include "debug.h"
#include "sched.h"

extern struct proc proc[NPROC];
extern struct tcb thread[NTHREADS];

extern queue_t unused_p_q, used_p_q, zombie_p_q;
extern queue_t unused_t_q, runnable_t_q, sleeping_t_q;

// init
void cond_init(struct cond *cond, char *name) {
    queue_init(&cond->waiting_queue, name, TCB_WAIT_QUEUE);
}

// wait
int cond_wait(struct cond *cond, struct spinlock *mutex) {
    struct tcb *t = mythread();

    acquire(&t->lock);

    tcb_q_change_state(t, TCB_SLEEPING);

    queue_push_back(&cond->waiting_queue, (void *)t);

    t->wait_chan_entry = &cond->waiting_queue; // !!!

    // TODO : modify it to futex(ref to linux)
    release(mutex);

    thread_sched();
    // ==========special for signal==============
    int killed = t->killed;
    release(&t->lock);
    if (killed) {
        thread_exit(-1);
        panic("thread has exit!!");
    }
    // ==========special for signal ==============
    // Reacquire original lock.
    acquire(mutex);

    return 0;
}

// just signal a object!!!
void cond_signal(struct cond *cond) {
    struct tcb *t;

    if (!queue_isempty_atomic(&cond->waiting_queue)) {
        t = (struct tcb *)queue_pop_atomic(&cond->waiting_queue, 1); // remove it
        acquire(&t->lock);
        if (t == NULL)
            panic("cond signal : this cond has no object waiting queue");
        if (t->state != TCB_SLEEPING) {
            printf("%s\n", t->state);
            panic("cond signal : this thread is not sleeping");
        }
        ASSERT(t->wait_chan_entry != NULL);
        t->wait_chan_entry = NULL;
        tcb_q_change_state(t, TCB_RUNNABLE);
        release(&t->lock);
    }
}

// signal all object!!!
void cond_broadcast(struct cond *cond) {
    struct tcb *t;
    while (!queue_isempty_atomic(&cond->waiting_queue)) {
        t = (struct tcb *)queue_pop_atomic(&cond->waiting_queue, 1); // remove it

        acquire(&t->lock);
        if (t == NULL)
            panic("cond signal : this cond has no object waiting queue");
        if (t->state != TCB_SLEEPING) {
            printf("%s\n", cond->waiting_queue.name);
            printf("%s\n", t->state);
            panic("cond broadcast : this thread is not sleeping");
        }
        ASSERT(t->wait_chan_entry != NULL);
        t->wait_chan_entry = NULL;
        tcb_q_change_state(t, TCB_RUNNABLE);
        release(&t->lock);
    }
}