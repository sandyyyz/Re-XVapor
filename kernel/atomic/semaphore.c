#include "cond.h"
#include "semaphore.h"

/// @brief init a semaphore
/// @param s semaphore
/// @param value semaphore's value 
/// @param name semaphore's name
void sema_init(sem *s, int value, char *name) {
    s->value = value;
    s->wakeup = 0;
    initlock(&s->sem_lock, name);
    cond_init(&s->sem_cond, name);
}

/// @brief wait a semaphore (P operation)
/// @param s semaphore waitting on
void sema_wait(sem *s) {
    acquire(&s->sem_lock);
    s->value--;
    if (s->value < 0) {
        do {
            cond_wait(&s->sem_cond, &s->sem_lock);
        } while (s->wakeup == 0);
        s->wakeup--;
    }
    release(&s->sem_lock);
}


/// @brief signal a semaphore (V operation)
/// @param s  semaphore signaling
void sema_signal(sem *s) {
    acquire(&s->sem_lock);
    s->value++;
    if (s->value <= 0) {
        s->wakeup++;
        cond_signal(&s->sem_cond);
    }
    release(&s->sem_lock);
}