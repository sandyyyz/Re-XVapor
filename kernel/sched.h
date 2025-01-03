#ifndef __SCHED_H
#define __SCHED_H
#include "thread.h"
#include "proc.h"
struct proc;
struct tcb;

void PCB_Q_ALL_INIT(void);
void pcb_q_change_state(struct proc *, enum procstate);

void TCB_Q_ALL_INIT(void);
void tcb_q_change_state(struct tcb *t, enum thread_state state_new);

void thread_wakeup_atomic(void *t);
void thread_wakeup(struct tcb *t);
void thread_yield(void);

void thread_sched(void);
void thread_scheduler(void) __attribute__((noreturn));

// switch to context of scheduler
void swtch(struct context *, struct context *);

#endif