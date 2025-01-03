#include "types.h"
#include "thread.h"
#include "queue.h"
#include "param.h"
#include "debug.h"
#include "riscv.h"
#include "memlayout.h"
#include "sched.h"

queue_t unused_t_queue, used_t_queue, runnable_t_queue, sleeping_t_queue;

atomic_t next_tid = ATOMIC_INIT(1); // tid

static inline tid_t allocpid() {return atomic_inc_return(&next_tid);}


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

// allocate a new kernel thread
struct tcb *alloc_thread(thread_callback callback) {
    struct tcb *t;

    t = (struct tcb *)queue_pop_atomic(g_tcb_queues[TCB_UNUSED], 1);

    if (t == NULL)
        return 0;

    acquire(&t->lock);

    // spinlock and threads list head
    INIT_LIST_HEAD(&t->threads);

    t->tid = allocpid();



    memset(&t->context, 0, sizeof(t->context));
    t->context.ra = (uint64)callback;
    t->context.sp = t->kstack + KSTACK_PAGE * PGSIZE;

    // chage state of TCB
    tcb_q_change_state(t, TCB_USED);

    return t;
}



