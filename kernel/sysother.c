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

