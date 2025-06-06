#include "types.h"
#include "defs.h"
#include "debug.h"
#include "futex.h"
#include "proc.h"
#include "hash.h"
#include "spinlock.h"
#include "sched.h"

extern uint ticks;
extern struct spinlock tickslock;
extern struct spinlock timeout_lock;

static uint64 __futex_hash_key(void *key);
static void *__futex_hash_lookup(struct hash_table *table, void *key);
static int __futex_hash_insert(struct hash_table *table, void *fhnp);
static void __futex_hash_delete(struct hash_table *table, void *uaddr);
static int futex_wait(uint64 uaddr, uint32 val, __nullable __kernel_space const struct timespec *timeout);

struct hash_table futex_hashtable = {
    .size = FUTEX_NUM,
};

static struct hash_table_operation futex_hash_table_op = {
    .hash_key = __futex_hash_key,
    .hash_lookup = __futex_hash_lookup,
    .hash_insert = __futex_hash_insert,
    .hash_delete = __futex_hash_delete,
};

void futex_hash_init() {
    initlock(&futex_hashtable.lock, "futex hashtable");
    hash_table_entry_init(&futex_hashtable);
    futex_hashtable.op = &futex_hash_table_op;
}

static uint64 __futex_hash_key(void *key){
    return (uint64) key; 
}

/**
 * @brief lookup a futex hash node in the hash table.
 * 
 * @param table hash table
 * @param key key to lookup, here is the user address of futex
 * @return pointer to the struct futex (in the struct futex node)if found, otherwise NULL.
 * @attention call with holding the lock of hash table. and it will return with holding the lock of hash table.
 */
static void *__futex_hash_lookup(struct hash_table *table, void *key) {
    uint64 uaddr = (uint64) key;
    struct hash_entry *entry = hash_get_entry(table, key);
    if (entry == NULL) {
        return NULL;
    }

    struct futex_hash_node *pos, *n;
    list_for_each_entry_safe(pos, n, &entry->list, futex_hash_list_node) {
        if (pos->uaddr == uaddr) {
            return pos->fp;
        }
    }
    return NULL;
}

/**
 * @brief insert a futex hash node into the hash table.
 * 
 * @param table hash table 
 * @param fhnp futex hash node pointer, which contains the user address and futex pointer
 * @attention remember to hold the hash table lock before calling this function.And it willreturn with holding the lock of hash table.
 * @return 0 on success, -1 on failure (if the futex hash node already exists).
 */
static int __futex_hash_insert(struct hash_table *table, void *fhnp) {
    struct futex_hash_node *fhn = (struct futex_hash_node *) fhnp;
    uint64 uaddr = fhn->uaddr;
    struct futex *fp_old = (struct futex *) __futex_hash_lookup(table, (void *) uaddr);
    if (fp_old != NULL) {
        // same futex hash node exists
        return -1;
    }
    // here we can be sure that we're holding the lock
    struct hash_entry *entry = hash_get_entry(table, (void *) uaddr);
    if (entry == NULL) {
        return -1;
    }
    INIT_LIST_HEAD(&fhn->futex_hash_list_node);
    list_add_tail(&fhn->futex_hash_list_node, &entry->list);

    return 0;
}

/**
 * @brief delete a futex hash node from the hash table.
 * 
 * @param table hash table
 * @param uaddr user address of futex
 * @attention call with holding the lock of hash table.And return with hash table lock held.
 */
static void __futex_hash_delete(struct hash_table *table, void *uaddr) {
    struct futex_hash_node *fhn =
            (struct futex_hash_node *) __futex_hash_lookup(table, uaddr); 
    if (fhn == NULL) {
        return;
    }
    // Should I free futex here?
    // Note that fp's lock is redundant, so it is NEVER acquired!
    struct futex *fp = fhn->fp;
    kfree(fp);
    list_del(&fhn->futex_hash_list_node);
    kfree(fhn);
    return;
}

static uint get_timeout_ticks(const struct timespec *ts) {
    acquire(&tickslock);
    uint ticks0 = ticks;
    release(&tickslock);
    uint rticks = TIMESPEC2TICKS(*ts);
    return rticks + ticks0;
}  
/**
 * @brief free the futex structure and its hash node with the user address addr.
 * 
 * @param uaddr user address of futex
 */
static void futex_free(uint64 uaddr) {
    acquire(&futex_hashtable.lock);
    futex_hashtable.op->hash_delete(&futex_hashtable, (void *) uaddr);
    release(&futex_hashtable.lock);
}

static void futex_init(struct futex *fp, char *name) {
    initlock(&fp->lock, name);
    queue_init(&fp->waiting_queue, name, TCB_WAIT_QUEUE);
}

/**
 * @brief Get the futex object
 * 
 * @param uaddr user address of futex
 * @param assert if 1, return NULL if the futex does not exist, otherwise create a new futex(including the futex hash node)
 * @return pointer to the futex structure if found or created, otherwise NULL.
 * @attention will deal with hash table lock inside, and return without holding the hash table lock.So needn't to acquire the hash table lock before calling this function.
 */
static struct futex *get_futex(uint64 uaddr, int assert) {
    acquire(&futex_hashtable.lock);
    struct futex *fp = NULL;
    struct futex_hash_node *fhn = NULL;

    fp = futex_hashtable.op->hash_lookup(&futex_hashtable, (void *) uaddr);
    if (fp) {
        /* find futex */
        goto bad;
    }
    /* not found */

    if (assert == 1) {
        goto bad;
    }
    /* create futex */
    fp = (struct futex *) kmalloc(sizeof(struct futex));
    if (fp == NULL) {
        Warn("get_futex: no free space for futex\n");
        goto bad;
    }
    futex_init(fp, "futex");

    fhn = (struct futex_hash_node *) kmalloc(sizeof(struct futex_hash_node));
    if (fhn == NULL) {
        Warn("get_futex: no free space for futex hash node\n");
        goto bad;
    }
    fhn->uaddr = uaddr;
    fhn->fp = fp;

    futex_hashtable.op->hash_insert(&futex_hashtable, (void *) fhn);
    release(&futex_hashtable.lock);

    return fp;
bad:
    release(&futex_hashtable.lock);
    if(fp)
        kfree(fp);
    if(fhn)
        kfree(fhn);
    return NULL;
}

/**
 * @brief is the futex operation a wait operation?
 * 
 * @param futex_op futex operation code
 * @return if the futex operation is a wait operation, need a timeout
 */
int futex_need_timeout(int futex_op) {
    return (futex_op == FUTEX_WAIT || futex_op == FUTEX_WAIT_BITSET ||
            futex_op == FUTEX_WAIT_REQUEUE_PI || futex_op == FUTEX_LOCK_PI ||
            futex_op == FUTEX_LOCK_PI2);
}

int do_futex(uint64 uaddr, int futex_op, uint32 val, __nullable __kernel_space const struct timespec *timeout, uint32 val2, uint32 uaddr2, uint32 val3) {
    int ret = -1;

    switch(futex_op) {
        case FUTEX_WAIT:
        // case FUTEX_WAIT_BITSET:
        // case FUTEX_WAIT_REQUEUE_PI:
            ret = futex_wait(uaddr, val, timeout);
            break;
        case FUTEX_WAKE:
            ret = futex_wake(uaddr, val);
            break;
        // case FUTEX_REQUEUE:
        //     ret = futex_requeue(uaddr, val, uaddr2, val2);
        //     break;
        // case FUTEX_CMP_REQUEUE:
        //     ret = futex_cmp_requeue(uaddr, val, uaddr2, val2, val3);
        //     break;
        // case FUTEX_WAKE_OP:
        //     ret = futex_wake_op(uaddr, val, uaddr2, val2, val3);
        //     break;
        // case FUTEX_LOCK_PI:
        //     ret = futex_lock_pi(uaddr);
        //     break;
        // case FUTEX_UNLOCK_PI:
        //     ret = futex_unlock_pi(uaddr);
        //     break;
        // case FUTEX_TRYLOCK_PI:
        //     ret = futex_trylock_pi(uaddr);
        //     break;
        default:
            Warn("sys_futex: unknown futex operation %d\n", futex_op);
    }
    if (ret < 0) {
        Warn("sys_futex: futex operation %d failed on address %p with value %d\n", futex_op, (void *)uaddr, val);
    }
    return ret;
}

/**
 * @brief wait on a futex at the user address uaddr until the value at uaddr is equal to val.
 * 
 * @param uaddr user address
 * @param val val to wait for
 * @param timeout timeout of the wait operation, if NULL, wait indefinitely
 * @return return 0 on success, -1 on failure. 
 */
static int futex_wait(uint64 uaddr, uint32 val, __nullable __kernel_space const struct timespec *timeout) {
    uint32 uval = 0;
    struct proc* p = myproc();
    struct tcb *t = mythread();
    struct futex *fp = NULL;

    if(copyin(p->mm.pagetable, (char*)uaddr, (uint64)&uval, sizeof(uval)) < 0) {
        Warn("futex_wait: failed to read value from user address %p\n", (void *)uaddr);
        return -1;
    }
    if(uval != val) {
        // the value has changed, no need to wait
        return 0;
    }

    /* futex wait */
    fp = get_futex(uaddr, 0);
    if(fp == NULL) {
        Warn("futex_wait: failed to get futex for address %p\n", (void *)uaddr);
        return -1;
    }
    acquire(&timeout_lock);
    acquire(&t->lock);
    release(&timeout_lock);
    tcb_q_change_state(t, TCB_SLEEPING);
    queue_push_back(&fp->waiting_queue, t);
    t->wait_chan_entry = &fp->waiting_queue;
    if(timeout) {
        t->timeout = get_timeout_ticks(timeout);
    }
    thread_sched();
    release(&t->lock);

    return 0;
}

/**
 * @brief wakes at most val of the waiters that are waiting on the futex at the user address uaddr.
 * 
 * @param uaddr user address of futex
 * @param nr_wake number of waiters to wake up
 * @return the number of waiters that were woken up, or 0 if no waiters were waiting.
 */
int futex_wake(uint64 uaddr, int nr_wake) {
    struct tcb *t = NULL;
    struct futex *fp = NULL;
    int ret = 0;

    fp = get_futex(uaddr, 1);
    if (fp == NULL) {
        Warn("futex_wake: futex not found for address %p\n", (void *)uaddr);
        return 0;
    }
    while (!queue_isempty_atomic(&fp->waiting_queue) && ret < nr_wake) {
        t = (struct tcb *)queue_pop_atomic(&fp->waiting_queue, 1);
        if (t == NULL)
            panic("futex wakeup : no waiting queue");
        if (t->state != TCB_SLEEPING) {
            printf("%s\n", fp->waiting_queue.name);
            printf("%s\n", t->state);
            panic("futex wakeup : this thread is not sleeping");
        }
        acquire(&t->lock);
        ASSERT(t->wait_chan_entry != NULL);
        t->wait_chan_entry = NULL;
        tcb_q_change_state(t, TCB_RUNNABLE);
        release(&t->lock);
        ret++;
    }

    if (queue_isempty_atomic(&fp->waiting_queue)) {
        futex_free(uaddr);
    }
    return ret;
}