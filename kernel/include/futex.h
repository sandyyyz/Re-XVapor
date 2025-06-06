#ifndef __FUTEX_H
#define __FUTEX_H

#include "queue.h"
#include "timer.h"

#define FUTEX_NUM 32

/* Second argument to futex syscall */
#define FUTEX_WAIT		0
#define FUTEX_WAKE		1
#define FUTEX_FD		2
#define FUTEX_REQUEUE		3
#define FUTEX_CMP_REQUEUE	4
#define FUTEX_WAKE_OP		5
#define FUTEX_LOCK_PI		6
#define FUTEX_UNLOCK_PI		7
#define FUTEX_TRYLOCK_PI	8
#define FUTEX_WAIT_BITSET	9
#define FUTEX_WAKE_BITSET	10
#define FUTEX_WAIT_REQUEUE_PI	11
#define FUTEX_CMP_REQUEUE_PI	12
#define FUTEX_LOCK_PI2		13

#define FUTEX_PRIVATE_FLAG	128
#define FUTEX_CLOCK_REALTIME	256
#define FUTEX_CMD_MASK		~(FUTEX_PRIVATE_FLAG | FUTEX_CLOCK_REALTIME)

#define FUTEX_WAIT_PRIVATE	(FUTEX_WAIT | FUTEX_PRIVATE_FLAG)
#define FUTEX_WAKE_PRIVATE	(FUTEX_WAKE | FUTEX_PRIVATE_FLAG)
#define FUTEX_REQUEUE_PRIVATE	(FUTEX_REQUEUE | FUTEX_PRIVATE_FLAG)
#define FUTEX_CMP_REQUEUE_PRIVATE (FUTEX_CMP_REQUEUE | FUTEX_PRIVATE_FLAG)
#define FUTEX_WAKE_OP_PRIVATE	(FUTEX_WAKE_OP | FUTEX_PRIVATE_FLAG)
#define FUTEX_LOCK_PI_PRIVATE	(FUTEX_LOCK_PI | FUTEX_PRIVATE_FLAG)
#define FUTEX_LOCK_PI2_PRIVATE	(FUTEX_LOCK_PI2 | FUTEX_PRIVATE_FLAG)
#define FUTEX_UNLOCK_PI_PRIVATE	(FUTEX_UNLOCK_PI | FUTEX_PRIVATE_FLAG)
#define FUTEX_TRYLOCK_PI_PRIVATE (FUTEX_TRYLOCK_PI | FUTEX_PRIVATE_FLAG)
#define FUTEX_WAIT_BITSET_PRIVATE	(FUTEX_WAIT_BITSET | FUTEX_PRIVATE_FLAG)
#define FUTEX_WAKE_BITSET_PRIVATE	(FUTEX_WAKE_BITSET | FUTEX_PRIVATE_FLAG)
#define FUTEX_WAIT_REQUEUE_PI_PRIVATE	(FUTEX_WAIT_REQUEUE_PI | \
					 FUTEX_PRIVATE_FLAG)
#define FUTEX_CMP_REQUEUE_PI_PRIVATE	(FUTEX_CMP_REQUEUE_PI | \
					 FUTEX_PRIVATE_FLAG)


struct futex {
    struct spinlock lock; // uncessary, using p->lock is ok
    struct queue waiting_queue;
};

struct robust_list {
    struct robust_list *next;
};

struct robust_list_head {
    /*
     * The head of the list. Points back to itself if empty:
     */
    struct robust_list list;
    /*
     * This relative offset is set by user-space, it gives the kernel
     * the relative position of the futex field to examine. This way
     * we keep userspace flexible, to freely shape its data-structure,
     * without hardcoding any particular offset into the kernel:
     */
    long futex_offset;
    /*
     * The death of the thread may race with userspace setting
     * up a lock's links. So to handle this race, userspace first
     * sets this field to the address of the to-be-taken lock,
     * then does the lock acquire, and then adds itself to the
     * list, and then clears this field. Hence the kernel will
     * always have full knowledge of all locks that the thread
     * _might_ have taken. We check the owner TID in any case,
     * so only truly owned locks will be handled.
     */
    struct robust_list *list_op_pending;
};

struct futex_hash_node {
    uint64 uaddr;
    struct futex *fp;
    struct list_head futex_hash_list_node;
};

int futex_need_timeout(int futex_op);
int futex_wake(uint64 uaddr, int nr_wake);
int do_futex(uint64 uaddr, int futex_op, uint32 val, __nullable __kernel_space const struct timespec *timeout, uint32 val2, uint32 uaddr2, uint32 val3);
void futex_hash_init();

#endif