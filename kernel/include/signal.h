#ifndef __SIGNAL_H
#define __SIGNAL_H

#include "list.h"
#include "trapframe.h"
#include "atomic.h"

/* Bits in `sa_flags'.  */
#define	SA_NOCLDSTOP  1		 /* Don't send SIGCHLD when children stop.  */
#define SA_NOCLDWAIT  2		 /* Don't create zombie on child death.  */
#define SA_SIGINFO    4		 /* Invoke signal-catching function with
				    three arguments instead of one.  */
#define SI_USER		0		/* sent by kill, sigsend, raise */
#define SI_KERNEL	0x80		/* sent by the kernel from somewhere */

#define SIGHUP		 1
#define SIGINT		 2
#define SIGQUIT		 3
#define SIGILL		 4
#define SIGTRAP		 5
#define SIGABRT		 6
#define SIGIOT		 6
#define SIGBUS		 7
#define SIGFPE		 8
#define SIGKILL		 9
#define SIGUSR1		10
#define SIGSEGV		11
#define SIGUSR2		12
#define SIGPIPE		13
#define SIGALRM		14
#define SIGTERM		15
#define SIGSTKFLT	16
#define SIGCHLD		17
#define SIGCONT		18
#define SIGSTOP		19
#define SIGTSTP		20
#define SIGTTIN		21
#define SIGTTOU		22
#define SIGURG		23
#define SIGXCPU		24
#define SIGXFSZ		25
#define SIGVTALRM	26
#define SIGPROF		27
#define SIGWINCH	28
#define SIGIO		29
#define SIGPOLL		SIGIO
/*
#define SIGLOST		29
*/
#define SIGPWR		30
#define SIGSYS		31
#define	SIGUNUSED	31

/* These should not be considered constants from userland.  */
#define SIGRTMIN	32


#define _NSIG		64
#define valid_signal(sig) (((sig) <= _NSIG && (sig) >= 1) ? 1 : 0)

// just support 64 signals at most right now
typedef struct {
    uint64 sig;
} sigset_t;

typedef void __signalfn_t(int);
typedef __signalfn_t *__sighandler_t;

typedef void __restorefn_t(void);
typedef __restorefn_t *__sigrestore_t;

// Flags for sigprocmask
/**
 * @brief The set of blocked signals is the union of the current set
              and the set argument.
 * 
 */
#define SIG_BLOCK 0

/**
 * @brief The signals in set are removed from the current set of
              blocked signals.  It is permissible to attempt to unblock a
              signal which is not blocked.
 * 
 */
#define SIG_UNBLOCK 1

/**
 * @brief The set of blocked signals is set to the argument set.
 * 
 */
#define SIG_SETMASK 2

#define SIG_DFL	((__sighandler_t)0)	/* default signal handling */
#define SIG_IGN	((__sighandler_t)1)	/* ignore signal */
#define SIG_ERR	((__sighandler_t)-1)	/* error return from signal */

#define SI_MAX_SIZE 128
#define __SIGINFO 			\
struct {				\
	int si_signo;			\
	int si_code;			\
	int si_errno;			\
}
//  +	union __sifields _sifields;	

typedef struct siginfo {
	union {
		__SIGINFO;
		int _si_pad[SI_MAX_SIZE/sizeof(int)];
	};
} siginfo_t;


// siganal action, to discribe the behavior when receive a given signal
struct sigaction {
    union {
        __sighandler_t sa_handler;	/* signal handler */
        void (*sa_sigaction)(int, siginfo_t*, void *); /* signal handler with extra info */
    };
    sigset_t sa_mask; /* Additional set of signals to be blocked during execution of signal-catching function. 1 means block*/
    int sa_flags;	/* special flags */
    void (*sa_restorer)(void); /* used by kernel to restore context */
};

// struct that contain all information a thread need to handle signals, one per thread
struct sighand {
    spinlock_t siglock;
    atomic_t ref;
    struct sigaction actions[_NSIG]; // sigaction for each given signal
};
// pending signal queue head of proc
struct sigpending {
    struct list_head list;
    sigset_t signal; /* pending signal set */
};

// signal queue struct
struct sigqueue {
    struct list_head list;
    int flags;
    siginfo_t info;
};

/* Structure describing a signal stack.  */
typedef struct
  {
    void *ss_sp;
    int ss_flags;
    size_t ss_size;
  } stack_t;


  struct __riscv_mc_f_ext_state
  {
    unsigned int __f[32];
    unsigned int __fcsr;
  };

struct __riscv_mc_d_ext_state
  {
    unsigned long long int __f[32];
    unsigned int __fcsr;
  };

struct __riscv_mc_q_ext_state
  {
    unsigned long long int __f[64] __attribute__ ((__aligned__ (16)));
    unsigned int __fcsr;
    /* Reserved for expansion of sigcontext structure.  Currently zeroed
       upon signal, and must be zero upon sigreturn.  */
    unsigned int __glibc_reserved[3];
  };

union __riscv_mc_fp_state
  {
    struct __riscv_mc_f_ext_state __f;
    struct __riscv_mc_d_ext_state __d;
    struct __riscv_mc_q_ext_state __q;
  };

typedef unsigned long int __riscv_mc_gp_state[32];

typedef struct mcontext_t
  {
    __riscv_mc_gp_state __gregs;
    union  __riscv_mc_fp_state __fpregs;
  } mcontext_t;

/* Userlevel context.  */
typedef struct ucontext_t
  {
    unsigned long int  __uc_flags;
    struct ucontext_t *uc_link;
    stack_t            uc_stack;
    sigset_t           uc_sigmask;
    /* There's some padding here to allow sigset_t to be expanded in the
       future.  Though this is unlikely, other architectures put uc_sigmask
       at the end of this structure and explicitly state it can be
       expanded, so we didn't want to box ourselves in here.  */
    char               __glibc_reserved[1024 / 8 - sizeof (sigset_t)];
    /* We can't put uc_sigmask at the end of this structure because we need
       to be able to expand sigcontext in the future.  For example, the
       vector ISA extension will almost certainly add ISA state.  We want
       to ensure all user-visible ISA state can be saved and restored via a
       ucontext, so we're putting this at the end in order to allow for
       infinite extensibility.  Since we know this will be extended and we
       assume sigset_t won't be extended an extreme amount, we're
       prioritizing this.  */
    mcontext_t uc_mcontext;
  } ucontext_t;

  struct sigcontext {
    struct trapframe tf;
    // struct user_regs_struct sc_regs;
    // union __riscv_fp_state sc_fpregs;
};

struct ucontext {
    // uint64 uc_flags;
    // struct ucontext *uc_link;
    // stack_t uc_stack;
    struct sigcontext uc_mcontext;
    sigset_t uc_sigmask; /* mask last for extensibility */
    sig_t sigprocessing;
};


struct rt_sigframe {
    struct siginfo info;
    ucontext_t uc_riscv;
    struct ucontext uc;
};

/**
 * @brief empty given sigset
 * 
 */
#define sig_empty_set(set) (memset(set, 0, sizeof(sigset_t)))

/**
 * @brief fill all bits of a sigset with 1
 * 
 */
#define sig_fill_set(set) (memset(set, -1, sizeof(sigset_t)))

/**
 * @brief add signal to sigset
 * 
 */
#define sig_add_set(set, sig) (set.sig |= 1UL << (sig - 1))

/**
 * @brief delete given signal from sigset
 * 
 */
#define sig_del_set(set, sig) (set.sig &= ~(1UL << (sig - 1)))

/**
 * @brief add mask to given sigset
 * 
 */
#define sig_add_set_mask(set, mask) (set.sig |= (mask))

/**
 * @brief delete mask from given sigset
 * 
 */
#define sig_del_set_mask(set, mask) (set.sig &= (~mask))

/**
 * @brief is signal in sigset?
 * 
 */
#define sig_is_member(set, n_sig) (1 & (set.sig >> (n_sig - 1)))

/**
 * @brief generate mask for a given signal
 * 
 */
#define sig_gen_mask(sig) (1UL << (sig - 1))

/**
 * @brief return x | y
 * 
 */
#define sig_or(x, y) ((x) | (y))

/**
 * @brief return x & y
 * 
 */
#define sig_and(x, y) ((x) & (y))

/**
 * @brief is mask in sigset?
 * 
 */
#define sig_test_mask(set, mask) ((set.sig & mask) != 0)
/**
 * @brief pending of given thread
 * 
 */
#define sig_pending(t) (t.sig_pending)

/**
 * @brief is signal ignored by the thread?
 * 
 */
#define sig_ignored(t, sig) (sig_is_member(t->blocked, sig))

/**
 * @brief is signal will be applied by the thread?
 * 
 */
#define sig_existed(t, sig) (sig_is_member(t->sig_pending.signal, sig))

/**
 * @brief sigaction of signo in given thread
 * 
 */
#define sig_action(t, signo) (t->sigs.actions[signo - 1])

int do_sigaction(int signum, __nullable struct sigaction *act, __nullable struct sigaction *oldact);
int do_sigprocmask(int how, __nullable const sigset_t *set, __nullable sigset_t *oldset);
int signal_frame_restore(struct tcb *t, struct rt_sigframe *rtf);
void signal_info_init(sig_t sig, siginfo_t *info, int opt);
int signal_queue_delete(uint64 mask, struct sigpending *pending);
int signal_queue_flush(struct sigpending *pending);
void sigpending_init(struct sigpending *sig);
int signal_handle(struct tcb *t);
int signal_send(siginfo_t *info, struct tcb *t);
int signal_frame_setup(sigset_t *set, struct trapframe *tf, struct rt_sigframe *rtf, sig_t signo);

#endif