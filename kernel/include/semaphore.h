#ifndef __SEMAPHORE_H
#define __SEMAPHORE_H

#include "types.h"
#include "spinlock.h"
#include "cond.h"

// semaphore
struct semaphore {
    volatile int value;
    volatile int wakeup;
    spinlock_t sem_lock;
    struct cond sem_cond;
};

typedef struct semaphore sem;

void sema_init(sem *S, int value, char *name);

void sema_wait(sem *S);

void sema_signal(sem *S);

#endif