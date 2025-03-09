#include "debug.h"
#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "thread.h"
#include "riscv.h"
#include "vm.h"

struct spinlock tickslock;
uint ticks;

extern char trampoline[], uservec[], userret[];
extern char debug_uservec[];
// in kernelvec.S, calls kerneltrap().
void kernelvec();

extern int devintr();

void
trapinit(void)
{
  initlock(&tickslock, "time");
}

// set up to take exceptions and traps while in the kernel.
void
trapinithart(void)
{
  w_stvec((uint64)kernelvec);
}

// trampoline已经换栈和页表
//
// handle an interrupt, exception, or system call from user space.
// called from trampoline.S
//
void
usertrap(void)
{
  int which_dev = 0;
#ifdef __DEBUG_UTRAP
  // walk_va(myproc()->mm.pagetable, THREAD_TRAPFRAME(mythread()->tidx));
  // walk_va(myproc()->mm.pagetable, TRAMPOLINE);
  // walk_va(myproc()->mm.pagetable, mythread()->trapframe->sp);
  printf_green("thread %d usertrap!\n", mythread()->tid);
#endif
// check SPP bit in sstatus
  if((r_sstatus() & SSTATUS_SPP) != 0) {
    printf("\nprocess %d, thread %d\n", myproc()->pid, mythread()->tid);
    printf("sepc=%p stval=%p\n", r_sepc(), r_stval());
    printf("sstatus : 0x%x\n",r_sstatus());
    printf("scause: 0x%x\n", r_scause());
    panic("usertrap: not from user mode");
  }

  // send interrupts and exceptions to kerneltrap(),
  // since we're now in the kernel.
  w_stvec((uint64)kernelvec);

  struct proc *p = myproc();
  // change to thread 
  struct tcb *t = mythread();
  
  // save user program counter.
  // p->trapframe->epc = r_sepc();
  t->trapframe->epc = r_sepc();
  
  if(r_scause() == 8){
    // system call

    if(killed(p) || thread_killed(t))
      exit(-1);

    // sepc points to the ecall instruction,
    // but we want to return to the next instruction.
    // p->trapframe->epc += 4;
    t->trapframe->epc += 4;

    // an interrupt will change sepc, scause, and sstatus,
    // so enable only now that we're done with those registers.
    intr_on();

    syscall();
  } else if((which_dev = devintr()) != 0){
    // ok
  } else {
    printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
    printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());

    list_for_each_entry(t, &p->tg.threads, threads) {
      thread_setkilled(t);
    }
    setkilled(p);
  }

  if(killed(p) || thread_killed(t))
    exit(-1);

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2) {
    p->utime++;
    thread_yield();
  }

  // each of the syscall, interrupt, exceptions from userspace will return from here
  // because we have set the t->tramframe->kernel_trap = (uint64)usertrap
  // and set the stvec to uservec before returned to userspace last time
  // next time's trap: uservec->usertrap->usertrapret->userret 
  usertrapret();
}

extern int g_first_exec;
int inline dodebug() { return 1;}

//
// return to user space
//

void
usertrapret(void)
{


  struct proc *p = myproc();
  struct tcb *t = mythread();
#ifdef __DEBUG_UTRAPRET
  printf_green("thread %d ready to userret\n", t->tid);
#endif
  // we're about to switch the destination of traps from
  // kerneltrap() to usertrap(), so turn off interrupts until
  // we're back in user space, where usertrap() is correct.
  intr_off();

  // send syscalls, interrupts, and exceptions to uservec in trampoline.S
  uint64 trampoline_uservec = TRAMPOLINE + (uservec - trampoline);
  // uint64 trampoline_debug_uservec = TRAMPOLINE + (debug_uservec - trampoline);

  // if(t->tid != 1) {
    w_stvec(trampoline_uservec);
  // } else {
  //   w_stvec(trampoline_debug_uservec);
  // }
  // set up trapframe values that uservec will need when
  // the process next traps into the kernel.
  // p->trapframe->kernel_satp = r_satp();         // kernel page table
  // p->trapframe->kernel_sp = p->kstack + PGSIZE; // process's kernel stack
  // p->trapframe->kernel_trap = (uint64)usertrap;
  // p->trapframe->kernel_hartid = r_tp();         // hartid for cpuid()

  t->trapframe->kernel_satp = r_satp();         // kernel page table
  t->trapframe->kernel_sp = t->kstack + KSTACK_PAGE * PGSIZE; // thread's kernel stack
  t->trapframe->kernel_trap = (uint64)usertrap;
  t->trapframe->kernel_hartid = r_tp(); // hartid for cpuid()

  // set up the registers that trampoline.S's sret will use
  // to get to user space.
  
  // set S Previous Privilege mode to User.
  unsigned long x = r_sstatus();
  x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
  x |= SSTATUS_SPIE; // enable interrupts in user mode
  w_sstatus(x);

  // set S Exception Program Counter to the saved user pc.
  // w_sepc(p->trapframe->epc);
  w_sepc(t->trapframe->epc);

  // tell trampoline.S the user page table to switch to.
  // because the threads shared the same pagetable
  uint64 satp = MAKE_SATP(p->mm.pagetable);

  // write tidx to sscratch for uservec in the future 
  w_sscratch(t->tidx);

  // jump to userret in trampoline.S at the top of memory, which 
  // switches to the user page table, restores user registers,
  // and switches to user mode with sret.
  // 定位到userret (trampoline.s)
  uint64 trampoline_userret = TRAMPOLINE + (userret - trampoline);
  #ifdef __DEBUG_UTRAPRET
  // static int startd = 0;
  if(dodebug())
  { 
    // startd += 1;
    // Log("TRAMPOLINE = %p, (userret-trampoline) = %p", TRAMPOLINE, userret - trampoline);
    // Log("usertrapret: trampoline_userret = %p\n", trampoline_userret);
    // Log("trapframe->ra = %p, epc = %p, tid = %d", t->trapframe->ra, t->trapframe->epc, t->tid);
    // printf_blue("  startd = %d \n", startd);
    print_trapframe(t->trapframe);
    vmprint(p->mm.pagetable);
    // walk_va(p->mm.pagetable, (uint64)(t->trapframe));
    // walk_va(p->mm.pagetable, (uint64)(THREAD_TRAPFRAME(t->tidx)));
    walk_va(p->mm.pagetable, t->trapframe->sp);
  }
  #endif

  ((void (*)(uint64))trampoline_userret)(satp);
}

// interrupts and exceptions from kernel code go here via kernelvec,
// on whatever the current kernel stack is.
void 
kerneltrap()
{
  int which_dev = 0;
  uint64 sepc = r_sepc();
  uint64 sstatus = r_sstatus();
  uint64 scause = r_scause();
  
#ifdef __DEBUG_TRAP
  printf("kerneltrap: sepc=%p stval=%p scause=%p\n", sepc, r_stval(), scause);
#endif

  // struct proc *p = myproc();


  if((sstatus & SSTATUS_SPP) == 0)
    panic("kerneltrap: not from supervisor mode");
  if(intr_get() != 0)
    panic("kerneltrap: interrupts enabled");

  if((which_dev = devintr()) == 0){
    printf("unknow devintr()\n");
    printf("scause %p\n", scause);
    printf("sepc=%p stval=%p\n", r_sepc(), r_stval());
    panic("kerneltrap");
  }

  // give up the CPU if this is a timer interrupt.
  // and if there is a thread running on cpu
  if(which_dev == 2 && mythread() != 0 && mythread()->state == TCB_RUNNING) {
    myproc()->ktime++;  
    thread_yield();
  }


  // the yield() may have caused some traps to occur,
  // so restore trap registers for use by kernelvec.S's sepc instruction.
  w_sepc(sepc);
  w_sstatus(sstatus);
}

void
clockintr()
{
  acquire(&tickslock);
  ticks++;
  thread_wakeup_chan(&ticks);
  release(&tickslock);
}

// check if it's an external interrupt or software interrupt,
// and handle it.
// returns 2 if timer interrupt,
// 1 if other device,
// 0 if not recognized.
int
devintr()
{
  uint64 scause = r_scause();

  if((scause & 0x8000000000000000L) &&
     (scause & 0xff) == 9){
    // this is a supervisor external interrupt, via PLIC.

    // irq indicates which device interrupted.
    int irq = plic_claim();

    if(irq == UART0_IRQ){
      uartintr();
    } else if(irq == VIRTIO0_IRQ){
      virtio_disk_intr();
    } else if(irq){
      printf("unexpected interrupt irq=%d\n", irq);
    }

    // the PLIC allows each device to raise at most one
    // interrupt at a time; tell the PLIC the device is
    // now allowed to interrupt again.
    if(irq)
      plic_complete(irq);

    return 1;
  } else if(scause == 0x8000000000000001L){
    // software interrupt from a machine-mode timer interrupt,
    // forwarded by timervec in kernelvec.S.

    // only the fisrt cpu to deal with the clockintr
    if(cpuid() == 0){
      clockintr();
    }
    
    // acknowledge the software interrupt by clearing
    // the SSIP bit in sip.
    w_sip(r_sip() & ~2);

    return 2;
  } else {
    return 0;
  }
}

/*  thread trap function (just modify original procedure is ok)

// trampoline已经换栈和页表
//
// handle an interrupt, exception, or system call from user space.
// called from trampoline.S
//
void thread_usertrap(void)
{
  int which_dev = 0;
// check SPP bit in sstatus
  if((r_sstatus() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  // send interrupts and exceptions to kerneltrap(),
  // since we're now in the kernel.
  w_stvec((uint64)kernelvec);

  struct proc *p = myproc();
  struct tcb* t = mythread();

  // save user program counter.
  // p->trapframe->epc = r_sepc();
  t->trapframe->epc = r_sepc();

  if(r_scause() == 8){
    // system call

    if(killed(p) || thread_killed(t))
      exit(-1);

    // sepc points to the ecall instruction,
    // but we want to return to the next instruction.
    t->trapframe->epc += 4;

    // an interrupt will change sepc, scause, and sstatus,
    // so enable only now that we're done with those registers.
    intr_on();

    syscall();
  } else if((which_dev = devintr()) != 0){
    // ok
  } else {
    printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
    printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());

    list_for_each_entry(t, &p->tg.threads, threads) {
      thread_setkilled(t);
    }
    setkilled(p);
  }

  if(killed(p) || thread_killed(t))
    exit(-1);

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2) {
    p->utime++;
    thread_yield();
  }
}
/// @brief thread return to user space, the diffrences between usertrap() and 
/// thread_usertrap() is that thread_usertrap() will store register in trapframe in advance, and store tid in ssratch
void thread_usertrapret() {
    struct proc *p = myproc();
    struct tcb *t = mythread();
    // we're about to switch the destination of traps from
    // kerneltrap() to usertrap(), so turn off interrupts until
    // we're back in user space, where usertrap() is correct.
    intr_off();
    // p->stub_time = rdtime();

    // p->last_out = rdtime();
    // p->stime += rdtime() - p->last_in;

    // send syscalls, interrupts, and exceptions to uservec in trampoline.S
    uint64 trampoline_uservec = TRAMPOLINE + (uservec - trampoline);
    w_stvec(trampoline_uservec);

    // set up trapframe values that uservec will need when
    // the process next traps into the kernel.

    t->trapframe->kernel_satp = r_satp();         // kernel page table
    t->trapframe->kernel_sp = t->kstack + KSTACK_PAGE * PGSIZE; // process's kernel stack
    t->trapframe->kernel_trap = (uint64)thread_usertrap;
    t->trapframe->kernel_hartid = r_tp(); // hartid for cpuid()

    // trapframe_print(t->trapframe);// debug

    // if (print_tf_flag) {
    //     printf("%d\n", p->pid);
    //     trapframe_print(t->trapframe);
    //     print_tf_flag = 0;
    // }

    // set up the registers that trampoline.S's sret will use
    // to get to user space.

    // set S Previous Privilege mode to User.
    unsigned long x = r_sstatus();
    x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
    x |= SSTATUS_SPIE; // enable interrupts in user mode
    w_sstatus(x);
    
    // tf_flrestore(t->trapframe);

    // set S Exception Program Counter to the saved user pc.
    w_sepc(t->trapframe->epc);

    // tell trampoline.S the user page table to switch to.
    uint64 satp = MAKE_SATP(p->mm.pagetable);

    // write thread idx into sscratch
    w_sscratch(t->tidx);

    // jump to userret in trampoline.S at the top of memory, which
    // switches to the user page table, restores user registers,
    // and switches to user mode with sret.
    uint64 trampoline_userret = TRAMPOLINE + (userret - trampoline);
    ((void (*)(uint64))trampoline_userret)(satp);
}

*/