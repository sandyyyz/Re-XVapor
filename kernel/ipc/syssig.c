#include "types.h"
#include "../include/signal.h"
#include "debug.h"
#include "defs.h"
#include "proc.h"
#include "signal.h"

/**
 * @brief    The sigaction() system call is used to change the action taken by
       a process on receipt of a specific signal. (examine and change a signal action)

       signum specifies the signal and can be any valid signal except
       SIGKILL and SIGSTOP.

       If act is non-NULL, the new action for signal signum is installed
       from act.  If oldact is non-NULL, the previous action is saved in
       oldact.

 * 
 * @property int sigaction(int signum,
                     const struct sigaction *_Nullable restrict act,
                     struct sigaction *_Nullable restrict oldact);
 * @return sigaction() returns 0 on success; on error, -1 is returned, and
       errno is set to indicate the error.
 */
uint64 sys_sigaction() {
    int signum, ret;
    uint64 act_addr, oldact_addr;
    struct sigaction act, oldact;
    struct proc *p = myproc();

    argint(0, &signum);
    argaddr(1, &act_addr);
    argaddr(2, &oldact_addr);
#ifdef __DEBUG_SYS_SIGACTION
    Log("[sys_sigaction] signum: %d, act_addr: %p, oldact_addr: %p", signum, act_addr, oldact_addr);
#endif
    if(act_addr) {
        if(copyin(p->mm.pagetable, (char*) &act, act_addr, sizeof(act)) < 0) {
            return -1;
        }
    }
    ret = do_sigaction(signum, act_addr ? &act : NULL, oldact_addr ? &oldact : NULL);
    if(!ret && oldact_addr) {
        if(copyout(p->mm.pagetable, oldact_addr, (char*) &oldact_addr, sizeof(oldact)) < 0) {
            return -1;
        }
    }
    return ret;
}

/**
 * @brief sigprocmask() is used to fetch and/or change the signal mask of
       the calling thread.
 * @property int sigprocmask(int how, const sigset_t *_Nullable restrict set,
                                  sigset_t *_Nullable restrict oldset);
 * @return  sigprocmask() returns 0 on success.  On failure, -1 is returned
       and errno is set to indicate the error.
 */
uint64 sys_sigprocmask() {
    int how, ret;
    uint64 set_addr, oldset_addr;
    sigset_t set, oldset;

    argint(0, &how);
    argaddr(1, &set_addr);
    argaddr(2, &oldset_addr);
#ifdef __DEBUG_SYS_SIGPROCMASK
    Log("[sys_sigprocmask] how: %d, set_addr: %p, oldset_addr: %p", how, set_addr, oldset_addr);
#endif
    if(set_addr) {
        if(copyin(myproc()->mm.pagetable, (char*) &set, set_addr, sizeof(set)) < 0) {
            return -1;
        }
    }
    ret = do_sigprocmask(how, set_addr ? &set : NULL, oldset_addr ? &oldset : NULL);
    if(!ret && oldset_addr) {
        if(copyout(myproc()->mm.pagetable, oldset_addr, (char*) &oldset, sizeof(oldset)) < 0) {
            return -1;
        }
    }
    return ret;
}

uint64 sys_sigreturn() {
    struct tcb *t = mythread();
    struct rt_sigframe *rtf = (struct rt_sigframe *)t->trapframe->sp;

    signal_frame_restore(t, rtf);
    sig_del_set_mask(t->sig_pending.signal, sig_gen_mask(t->sig_processing));

    return t->trapframe->a0; 
}