#include "types.h"
#include "riscv.h"
#include "timer.h"
#include "param.h"
#include "proc.h"
#include "defs.h"
#include "uname.h"


extern struct proc proc[NPROC];

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

    strncpy(uts.sysname, "rexvapor", 3);
    strncpy(uts.nodename, "none", 4);
    strncpy(uts.release, "5.0", 3);
    strncpy(uts.version, "0.1", 3);
    strncpy(uts.machine, "QEMU", 4);
    strncpy(uts.domainname, "none", 4);

    if (copyout(myproc()->mm.pagetable, addr, (char *)&uts, sizeof(uts)) < 0)
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

uint64 sys_sigaction() {
    return 0;
}

uint64 sys_geteuid() {
    return 0;
}

uint64 sys_ppoll() {
    return 0;
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