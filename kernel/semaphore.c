#include "semaphore.h"


/// @brief init a semaphore
/// @param S semaphore
/// @param value semaphore's value 
/// @param name semaphore's name
void sema_init(sem *S, int value, char *name) {
    S->value = value;
    S->wakeup = 0;
    initlock(&S->sem_lock, name);
    cond_init(&S->sem_cond, name);
}

/// @brief wait a semaphore (P operation)
/// @param S semaphore waitting on
void sema_wait(sem *S) {
    acquire(&S->sem_lock);
    S->value--;
    if (S->value < 0) {
        do {
            cond_wait(&S->sem_cond, &S->sem_lock);
        } while (S->wakeup == 0);
        S->wakeup--;
    }
    release(&S->sem_lock);
}


/// @brief signal a semaphore (V operation)
/// @param S  semaphore signaling
void sema_signal(sem *S) {
    acquire(&S->sem_lock);
    S->value++;
    if (S->value <= 0) {
        S->wakeup++;
        cond_signal(&S->sem_cond);
    }
    release(&S->sem_lock);
}