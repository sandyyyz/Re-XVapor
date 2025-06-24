#include "types.h"
#include "riscv.h"
#include "timer.h"
#include "param.h"
#include "proc.h"
#include "defs.h"
#include "uname.h"
#include "debug.h"
#include "sysinfo.h"
#include "signal.h"
#include "memlayout.h"
#include "sbi.h"

extern struct proc proc[NPROC];

struct utsname g_uts;

uint64 sys_times(void){ 
    struct tms ptms;
    uint64 utms;
    argaddr(0, &utms);
    struct proc *curr_p = myproc();
    ptms.tms_utime = curr_p->utime;
    ptms.tms_stime = curr_p->ktime;
    ptms.tms_cstime = 0;
    ptms.tms_cutime = 0;

    struct proc *p;
    for (p = proc; p < proc + NPROC; p++)
    {
        acquire(&p->lock);
        if (p->parent == curr_p)
        {
            ptms.tms_cutime += p->utime;
            ptms.tms_cstime += p->ktime;
        }
        release(&p->lock);
    }
    if (copyout(curr_p->mm.pagetable, utms, (char *)&ptms, sizeof(ptms)) < 0)
        return -1;
    return ptms.tms_utime + ptms.tms_stime;
}

uint64
sys_uname(void)
{
    uint64 addr;
    argaddr(0, &addr);

    if (copyout(myproc()->mm.pagetable, addr, (char *)&g_uts, sizeof(g_uts)) < 0)
        return -1;
    return 0;
}

uint64
sys_sched_yield(void)
{
    thread_yield();
    return 0;
}


uint64
sys_gettimeofday(void)
{
    uint64 addr;
    argaddr(0, &addr);
    uint64 t = r_time();
    struct timeval ts;
    ts.tv_sec = t / CLK_FREQ;
    ts.tv_usec = (t % CLK_FREQ) * 1000000 / CLK_FREQ;
    printf("%d  %d\n",ts.tv_sec,ts.tv_usec);
    if (copyout(myproc()->mm.pagetable, addr, (char *)&ts, sizeof(struct timeval)) < 0)
        return -1;
    return 0;
}


static // from FreeBSD.
int
do_rand_kernel(unsigned long *ctx)
{
/*
 * Compute x = (7^5 * x) mod (2^31 - 1)
 * without overflowing 31 bits:
 *      (2^31 - 1) = 127773 * (7^5) + 2836
 * From "Random number generators: good ones are hard to find",
 * Park and Miller, Communications of the ACM, vol. 31, no. 10,
 * October 1988, p. 1195.
 */
    long hi, lo, x;

    /* Transform to [1, 0x7ffffffe] range. */
    x = (*ctx % 0x7ffffffe) + 1;
    hi = x / 127773;
    lo = x % 127773;
    x = 16807 * lo - 2836 * hi;
    if (x < 0)
        x += 0x7fffffff;
    /* Transform to [0, 0x7ffffffd] range. */
    x--;
    *ctx = x;
    return (x);
}


unsigned long rand_next_kernel = 1;

static int randkernel(void)
{
    return (do_rand_kernel(&rand_next_kernel));
}

/**
 * @brief getrandom() - get random bytes
 * 
 * @return  On success, getrandom() returns the number of bytes that were
       copied to the buffer buf.  This may be less than the number of
       bytes requested via buflen if either GRND_RANDOM was specified in
       flags and insufficient entropy was present in the random source or
       the system call was interrupted by a signal.

       On error, -1 is returned, and errno is set to indicate the error.
 */
uint64 sys_getrandom(void)
{
//        ssize_t getrandom(void buf[.buflen], size_t buflen, unsigned int flags);
    uint64 addr;
    uint64 buflen;
    argaddr(0, &addr);
    argaddr(1, &buflen);
    if (buflen > 4096)
        return -1;
    char *buf = (char *)kmalloc(buflen);
    if (buf == NULL)
        return -1;
    for (uint64 i = 0; i < buflen; i++)
        buf[i] = randkernel();
    if (copyout(myproc()->mm.pagetable, addr, buf, buflen) < 0)
        return -1;
    kfree(buf);
    return buflen;
}

//TODO

uint64 sys_getuid() {
    return 0;
}

uint64 sys_setuid() {
    return 0; 
}

uint64 sys_getgid() {
    return 0;
}

uint64 sys_setgid() {
    return 0;
}

uint64 sys_geteuid() {
    return 0;
}

/**
 * @brief ppoll() - wait for events on file descriptors
 * @property int ppoll(struct pollfd *fds, nfds_t nfds,
        const struct timespec *timeout_ts, const sigset_t *sigmask);
 * @return On success, ppoll() returns the number of file descriptors
       ready for reading or writing, or 0 if the timeout expired.  On
       error, -1 is returned, and errno is set to indicate the error.
 */
uint64 sys_ppoll() {
    uint64 fds_addr;
    uint64 nfds;
    uint64 timeout_ts_addr;
    uint64 sigmask_addr;
    argaddr(0, &fds_addr);
    argaddr(1, &nfds);
    argaddr(2, &timeout_ts_addr);
    argaddr(3, &sigmask_addr);
    return nfds;
}


uint64 sys_clock_gettime() {
    uint64 clkid;
    uint64 tp;
    arguint64(0, &clkid);
    argaddr(1, &tp);

    volatile struct timespec ts;
    ts = TIME2TIMESPEC(rdtime());
    if(copyout(myproc()->mm.pagetable, tp, (char *)&ts, sizeof(ts)) < 0)
        return -1;
    return 0;
}

uint64 sys_syslog(void) {
    int priority;
    uint64 addr;
    argint(0, &priority);
    argaddr(1, &addr);

    char buf[128];
    if (copyin(myproc()->mm.pagetable, buf, addr, sizeof(buf)) < 0) {
        return -1;
    }

    Log("%s", buf);
    return 0;
}

uint64 sys_sysinfo(void) {
    uint64 addr;
    argaddr(0, &addr);

    struct sysinfo info;
    info.uptime = TIME2SEC(rdtime());
    info.totalram = totalram_bytes();
    info.freeram = freemem_bytes();
    info.sharedram = 0; // not supported
    info.bufferram = 0; // not supported
    info.totalswap = 0; // not supported
    info.freeswap = 0; // not supported
    info.procs = procs_cnt();
    
    if (copyout(myproc()->mm.pagetable, addr, (char *)&info, sizeof(info)) < 0) {
        return -1;
    }
    
    return 0;
}

// static int qemu_raw_poweroff() {
//     volatile uint32_t *poweroff = (uint32_t *)FINISHER_BASE;
//     *poweroff = 0x5555; 
//     // while(1);
//     return 0;
// }

static int opensbi_poweroff(int sysfail) {
    sbi_shutdown(sysfail);
    return 0;
}
uint64 sys_poweroff(void) {
    int sysfail;
    argint(0, &sysfail);
    opensbi_poweroff(sysfail);
    return 0;
}