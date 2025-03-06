#ifndef __QUEUE_H
#define __QUEUE_H

// #include "spinlock.h"
#include "list.h"


struct tcb;
struct proc;

enum queue_type { TCB_STATE_QUEUE,
                  PCB_STATE_QUEUE,
                  TCB_WAIT_QUEUE,
                  INODE_FREE_QUEUE };

struct queue {
    struct spinlock lock; // lock to protect the queue
    struct list_head list;
    char name[30]; // the name of queue
    enum queue_type type;
};

typedef struct queue queue_t;

// init
void queue_init(queue_t *q, char *name, enum queue_type type);

// is empty?
int queue_isempty(queue_t *q);

// is empty (atomic)?
int queue_isempty_atomic(queue_t *q);

// acquire the list entry of node
struct list_head *queue_entry(void *node, enum queue_type type);

// acquire the first node of queue given type of queue
void *queue_first_node(queue_t *q);

// push back
void queue_push_back(queue_t *q, void *node);

// push back (atomic)
void queue_push_back_atomic(queue_t *q, void *node);

// move it from its old Queue
void queue_remove(void *node, enum queue_type type);

// move it from its old Queue (atomic)
void queue_remove_atomic(queue_t *q, void *node);

// pop the queue
void *queue_pop(queue_t *q, int remove);

// provide the first one of the queue (atomic)
void *queue_pop_atomic(queue_t *q, int remove);

/**
 * @brief traverse the queue safely
 * @param pos: the type* of the target structure to traverse
 * @param tmp: another type* of the structure for temporary storage
 * @param q: the queue* to traverse
 * @param member: the name of the list_head within the struct
 * @details not elegant right now, just used before using the condition variable for sleep-wakeup...
 */
#define queue_for_each_entry_safe(pos, tmp, q, member) \
    list_for_each_entry_safe(pos, tmp, &(q->list), member)// buggy
    


#endif  // __QUEUE_H