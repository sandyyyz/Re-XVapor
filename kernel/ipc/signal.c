#include "types.h"
#include "defs.h"
#include "../include/signal.h"
#include "proc.h"
#include "thread.h"
#include "debug.h"
#include "riscv.h"
#include "memlayout.h"
#include "trap.h"
#include "errno.h"

static void signal_default(struct tcb *t, int sig_no);
static int do_handle_signal(struct tcb *t, int sig_no, struct sigaction *sig_act);
static int setup_rt_frame(struct sigaction *sig, sig_t signo, sigset_t *set, struct trapframe *tf);
static void *get_sigframe(struct sigaction *sig, struct trapframe *tf, size_t framesize);

int do_sigaction(int signum, __nullable struct sigaction *act, __nullable struct sigaction *oldact) {
    struct tcb *t = mythread();
    struct sigaction *sa;

    if(!valid_signal(signum))
        return -1;
    acquire(&t->sigs->siglock);
    sa = &sig_action(t, signum);
    if(oldact) {
        *oldact = *sa;
    }
    if (act) {
        // should never block or ignore SIGKILL and SIGSTOP when handle the sigaction
        sig_del_set_mask(act->sa_mask, sig_gen_mask(SIGKILL) | sig_gen_mask(SIGSTOP));
        *sa = *act;
    }
    release(&t->sigs->siglock);
    return 0;
}

int do_sigprocmask(int how, __nullable const sigset_t *set, __nullable sigset_t *oldset) {
    struct tcb *t = mythread();
    sigset_t oldmask;

    acquire(&t->sigs->siglock);
    if (set) {
#ifdef __DEBUG_DO_SIGPROCMASK
        Log("do_sigprocmask: thread %d, how: %d, set: %p", t->tid, how, set->sig);
#endif
        switch (how) {
            case SIG_BLOCK:
                t->blocked.sig |= set->sig;
                break;
            case SIG_UNBLOCK:
                t->blocked.sig &= ~set->sig;
                break;
            case SIG_SETMASK:
                t->blocked.sig = set->sig;
                break;
            default:
                release(&t->sigs->siglock);
                Warn("do_sigprocmask: invalid how value: %d", how);
                return -1; // Invalid how value
        }
    }
    /*
     If set is NULL, then the signal mask is unchanged (i.e., how is
       ignored), but the current value of the signal mask is nevertheless
       returned in oldset (if it is not NULL). 
     */
    if (oldset) {
        oldmask = t->blocked;
        *oldset = oldmask;
    }
    sig_del_set_mask(t->blocked, sig_gen_mask(SIGKILL) | sig_gen_mask(SIGSTOP));
    release(&t->sigs->siglock);
    return 0;
}


/**
 * @brief default action for a signal when no specific handler is set
 * 
 * @param t given thread 
 * @param sig_no signal number
 */
static void signal_default(struct tcb *t, int sig_no) {
    switch (sig_no) {
        case SIGKILL:
            // Terminate the thread
            thread_setkilled(t);
            break;
        case SIGCHLD:
            // Default action for SIGCHLD is to ignore it
            // This is usually handled by the kernel, so we can just ignore it here
            break;
        default:
            // For other signals, we can just ignore them or log a warning
            Warn("Default action for signal %d not implemented", sig_no);
            break;
    }
}


/**
 * @brief handle one or more pending signal (specially delete signal ignored from list) for the given thread
 * 
 * @param t thread
 * @param sig signal number to handle, if == 0, not specified
 * @param retinfo if not NULL, will be filled with the siginfo of the handled signal
 * @return 0 if success, -1 if error
 */
int signal_handle(struct tcb *t, int sig, __nullable siginfo_t *retinfo) {
    struct sigqueue *sig_cur = NULL;
    struct sigqueue *sig_tmp = NULL;
    struct sigaction sig_act;
    int sig_no;

    if(t->pending_cnt == 0) {
        return 0; // No pending signals
    }
#ifdef __DEBUG_SIGNAL_HANDLE
    Log("signal_handle: thread %d has %d pending signals", t->tid, t->pending_cnt);
#endif
    acquire(&t->sig_pending.siglock);
    list_for_each_entry_safe(sig_cur, sig_tmp, &t->sig_pending.list, list) {
        sig_no = sig_cur->info.si_signo;
        if (sig > 0 && sig_no != sig) {
            // If a specific signal is requested, skip others
            continue;
        }
        if (!valid_signal(sig_no)) {
            Warn("Invalid signal number: %d", sig_no);
            panic("Invalid signal number in signal_handle");
        }
        if (retinfo) {
            *retinfo = sig_cur->info; // Fill the retinfo with the siginfo
        }
        sig_act = sig_action(t, sig_no);
        if (sig_ignored(t, sig_no) || sig_act.sa_handler == SIG_IGN) {
            // Ignore the signal
            list_del_reinit(&sig_cur->list);
            t->pending_cnt--;
            kfree(sig_cur); // Free the signal queue
            continue;
        } else if (sig_act.sa_handler == SIG_DFL) {
            // Default action
            signal_default(t, sig_no);
            t->pending_cnt--;
            list_del_reinit(&sig_cur->list);
            kfree(sig_cur); // Free the signal queue
        } else {
            // Custom handler
            if(do_handle_signal(t, sig_no, &sig_act) != 0) {
                Warn("Failed to handle signal %d for thread %d", sig_no, t->tid);
                release(&t->sig_pending.siglock);
                return -1; // Error handling the signal
            }
            // After handling, we can remove the signal from the pending list
            t->pending_cnt--;
            t->sig_processing = sig_no; // Set the signal being processed
            list_del_reinit(&sig_cur->list);
            kfree(sig_cur); // Free the signal queue
            break;
        }
    }
    release(&t->sig_pending.siglock);
    return 0; // Successfully handled signals
}

static int do_handle_signal(struct tcb *t, int sig_no, struct sigaction *sig_act) {
    sigset_t *oldset = &t->blocked;
 
    return setup_rt_frame(sig_act, sig_no, oldset, t->trapframe);
}

/**
 * @brief setup the signal frame for the given signal action
 * 
 * @param sig signal action
 * @param signo signal number
 * @param set signal mask to be applied
 * @param tf trapframe to be used
 * @return 0 if success, -1 if error
 */
static int setup_rt_frame(struct sigaction *sig, sig_t signo, sigset_t *set, struct trapframe *tf) {
    struct rt_sigframe *frame;
    frame = get_sigframe(sig, tf, sizeof(*frame));
    if (signal_frame_setup(set, tf, frame, signo) < 0) {
        return -1;
    }

    tf->ra = (uint64)SIGRETURN; // trampoline to call sigreturn syscall
    tf->sp = (uint64)frame;

    if (sig->sa_flags & SA_SIGINFO) {
        tf->epc = (uint64)sig->sa_sigaction;
        tf->a0 = (uint64)signo; /* a0: signal number */
        tf->a1 = (uint64)&frame->info;  // tf->a1  = (uint64)(&frame->info); 
        tf->a2 = (uint64)&frame->uc;  // tf->a2 = (uint64)(&frame->uc);  // TODO or uc_riscv?
    } else {
        tf->epc = (uint64)sig->sa_handler;
        tf->a0 = (uint64)signo;
        tf->a1 = 0;
        tf->a2 = 0;
    }

    return 0;
}


/**
 * @brief Get position of the signal frame in the stack.
 * 
 * @param sig signal action
 * @param tf thread's trapframe
 * @param framesize signal frame size
 * @details put the signal frame on the top of the stack right now , size == framesize
 * @return void* pointer to the signal frame in the stack, or a bogus address if it would overflow the alternate signal stack
 */
static void *get_sigframe(struct sigaction *sig, struct trapframe *tf, size_t framesize) {
    uint64 sp;
    /* Default to using normal stack */
    sp = tf->sp;
    /*
     * If we are on the alternate signal stack and would overflow it, don't.
     * Return an always-bogus address instead so we will die with SIGSEGV.
     */
    // if (on_sig_stack(sp) && !likely(on_sig_stack(sp - framesize)))
    // 	return (void __user __force *)(-1UL);

    /* This is the X/Open sanctioned signal stack switching. */
    // sp = sigsp(sp, ksig) - framesize;
    sp -= framesize;

    /* Align the stack frame. */
    sp &= ~0xfUL;

    return (void *)sp;
}

/**
 * @brief setup the signal frame on stack, first setup myproc's ucontext, then setup the ucontext_riscv
 * 
 * @param set signal set to be applied
 * @param tf trapframe to be used
 * @param rtf signal frame to be used
 * @param signo sigal number
 * @details just set the uc_riscv to zero right now, we can extend it later. we have saved the trapframe on the stack in this function
 * @return return 0 if success, -1 if error.
 */
int signal_frame_setup(sigset_t *set, struct trapframe *tf, struct rt_sigframe *rtf, sig_t signo) {
    struct ucontext uc;
    struct proc *p = myproc();

    uc.uc_sigmask = *set;
    uc.uc_mcontext.tf = *tf; // save the trapframe to ucontext
    uc.sigprocessing = signo;
    if (copyout(p->mm.pagetable, (uint64)&rtf->uc, (char *)&uc, sizeof(struct ucontext)))
        return -1;

    ucontext_t uc_riscv;
    memset((void *)&uc_riscv, 0, sizeof(uc_riscv));
    if (copyout(p->mm.pagetable, (uint64)&rtf->uc_riscv, (char *)&uc_riscv, sizeof(ucontext_t))) {
        return -1;
    }
    return 0;
}

/**
 * @brief restore thread's trapframe and signal mask from the signal frame
 * 
 * @param t thread
 * @param rtf rt_sigframe pointer
 * @return 0 if success, -1 if error
 */
int signal_frame_restore(struct tcb *t, struct rt_sigframe *rtf) {
    struct ucontext uc;
    struct proc *p  = myproc();
    if (copyin(p->mm.pagetable, (char *)&uc, (uint64)&rtf->uc, sizeof(struct ucontext)) != 0)
        return -1;
    t->blocked = uc.uc_sigmask;
    *(t->trapframe) = uc.uc_mcontext.tf;
    t->sig_processing = uc.sigprocessing;

    ucontext_t uc_riscv;
    if (copyin(p->mm.pagetable, (char *)&uc_riscv, (uint64)&rtf->uc_riscv, sizeof(ucontext_t)) != 0)
        return -1;
    uint64 MC_PC = uc_riscv.uc_mcontext.__gregs[0];// for libc-test (pthread_cancel)
    if(MC_PC) {
        t->trapframe->epc = MC_PC; 
    }
    return 0;
}

void sigpending_init(struct sigpending *sig) {
    sig_empty_set(&sig->signal);
    initlock(&sig->siglock, "siglock");
    INIT_LIST_HEAD(&sig->list);
}

/**
 * @brief delete the signal with the given mask from the pending signal queue
 * 
 * @param mask mask of signals to be deleted
 * @param pending pending signal queue
 * @return 0 if success, -1 if error
 */
int signal_queue_delete(uint64 mask, struct sigpending *pending) {
    ASSERT(pending != NULL);
    struct sigqueue *sig_cur;
    struct sigqueue *sig_tmp;

    if (!sig_test_mask(pending->signal, mask)) {
        Warn("this signal is invalid\n");
        return -1;
    }

    sig_del_set_mask(pending->signal, mask);
    list_for_each_entry_safe(sig_cur, sig_tmp, &pending->list, list) {
        if (valid_signal(sig_cur->info.si_signo) && (mask & sig_gen_mask(sig_cur->info.si_signo))) {
            list_del_reinit(&sig_cur->list);
            kfree(sig_cur);
        }
    }
    return 0;
}

/**
 * @brief delete all pending signals in the given pending signal queue
 * 
 * @param pending pending signal queue
 * @return 0 if success, -1 if error
 */
int signal_queue_flush(struct sigpending *pending) {
    ASSERT(pending != NULL);
    struct sigqueue *sig_cur;
    struct sigqueue *sig_tmp;
    sig_empty_set(&pending->signal);
    list_for_each_entry_safe(sig_cur, sig_tmp, &pending->list, list) {
        list_del_reinit(&sig_cur->list);
        kfree(sig_cur);
    }
    return 0;
}

/**
 * @brief send a signal to the given thread
 * 
 * @param info siginfo_t pointer containing signal information
 * @param t thread to which the signal is sent
 * @attention call with the thread's lock held
 * @return return 0 if success, -1 if error
 */
int signal_send(siginfo_t *info, struct tcb *t) {
    ASSERT(t != NULL);
    ASSERT(info != NULL);

    // signo
    sig_t sig = info->si_signo;

    if(!valid_signal(sig)) {
        Warn("signal_send : invalid signal %d", sig);
        return -1;
    }
    if (sig_existed(t, sig)) {
        Warn("signal_send : signal %d already exists in pending queue", sig);
        return -1;
    }
#ifdef __DEBUG_SIGNAL_SEND
    Log("signal_send: thread %d send signal %d to thread %d", mythread()->tid, sig, t->tid);
#endif
    // be killed immediately !!!
    if (sig == SIGKILL || sig == SIGSTOP || sig == SIGTERM) {
        t->killed = 1;
    }

    struct sigqueue *q;
    if ((q = (struct sigqueue *)kalloc()) == NULL) {
        Warn("signal_send : no space in heap\n");
        return -1;
    }

    q->info = *info;
    INIT_LIST_HEAD(&q->list);
    acquire(&t->sig_pending.siglock);
    list_add_tail(&q->list, &t->sig_pending.list);
    sig_add_set(t->sig_pending.signal, sig);
    release(&t->sig_pending.siglock);
    t->pending_cnt++;

    return 0;
}

/**
 * @brief initialize signal info 
 * 
 * @param sig signo
 * @param info pointer to siginfo
 * @param opt 0 means SI_USER, 1 for SI_KERNEL
 */
void signal_info_init(sig_t sig, siginfo_t *info, int opt) {
    // USER
    if (opt == 0) {
        info->si_signo = sig;
        info->si_code = SI_USER;
        // KERNEL
    } else if (opt == 1) {
        info->si_signo = sig;
        info->si_code = SI_KERNEL;
    } else {
        panic("signal info : error\n");
    }
}

/**
 * @brief do sigtimedwait common function
 * 
 * @param set set of signals to wait for
 * @param info if not NULL, will be filled with the siginfo of the handled signal
 * @param timeout sleeping timeout, if NULL, will wait indefinitely
 * @return return the signal number if success, -1 if error or timeout
 */
int do_sigtimedwait(__kernel_space sigset_t *set,  __nullable __kernel_space siginfo_t *info, __nullable __kernel_space struct timespec *timeout) {

    struct tcb *t = mythread();
    sig_t sig_no;
    siginfo_t siginfo;

    if (sig_test_empty(*set)) {
        Warn("sigtimewait_common: set is empty");
        return -1;
    }
    sig_no = sig_first_member(*set);
    acquire(&t->sig_pending.siglock);
    if (!sig_existed(t, sig_no)) {
        // No pending signal in the set
        t->timeout = INT_MAX; // Set a large timeout value
        // sleep on the signal pending queue
        while(t->timeout != 0) {
            thread_sleep((void *) sig_no, &t->sig_pending.siglock, timeout);
        }
        release(&t->sig_pending.siglock);
    }
    if(!sig_existed(t, sig_no)) 
        return -EAGAIN ; // timeout
    if(signal_handle(t, sig_no, &siginfo) < 0) {
        Warn("sigtimewait_common: signal_handle failed for signal %d", sig_no);
        return -1;
    }

    if(info)
        *info = siginfo; // Copy the siginfo to the output info
    return info->si_signo; // Return the signal number
}

