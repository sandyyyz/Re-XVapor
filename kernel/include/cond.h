#ifndef __COND_H
#define __COND_H

#include "queue.h"

struct cond {
    queue_t waiting_queue;
};

void cond_init(struct cond *cond, char *name);

int cond_wait(struct cond *cond, struct spinlock *mutex);

void cond_signal(struct cond *cond);

void cond_broadcast(struct cond *cond);

#endif