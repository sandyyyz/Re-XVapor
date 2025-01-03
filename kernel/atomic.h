#ifndef __ATOMIC_H
#define __ATOMIC_H

#include "types.h"

typedef struct {
    volatile int counter;
} atomic_t;

// 初始化
#define ATOMIC_INITIALIZER \
    { 0 }
#define ATOMIC_INIT(i) \
    { (i) }

// 原子读取
/* #define WRITE_ONCE(var, val) \
     (*((volatile typeof(val) *)(&(var))) = (val)) */
#define WRITE_ONCE(var, val)          \
    do {                              \
        union {                       \
            volatile typeof(val) tmp; \
            typeof(var) result;       \
        } u = {.tmp = (val)};         \
        (var) = u.result;             \
    } while (0)
#define READ_ONCE(var) (*((volatile typeof(var) *)(&(var))))

// 原子性的读和设置
#define atomic_read(v) READ_ONCE((v)->counter)
#define atomic_set(v, i) WRITE_ONCE(((v)->counter), (i))
// #define atomic_set(v, i)

// 自增
static inline int atomic_inc_return(atomic_t *v) {
    return __sync_fetch_and_add(&v->counter, 1);
}

// 自减
static inline int atomic_dec_return(atomic_t *v) {
    return __sync_fetch_and_sub(&v->counter, 1);
}

// 增加
static inline int atomic_add_return(atomic_t *v, int i) {
    return __sync_fetch_and_add(&v->counter, i);
}

// 减少
static inline int atomic_sub_return(atomic_t *v, int i) {
    return __sync_fetch_and_sub(&v->counter, i);
}

#endif