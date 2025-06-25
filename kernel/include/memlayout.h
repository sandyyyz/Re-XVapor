#ifndef __MEMLAYOUT_H
#define __MEMLAYOUT_H
#include "param.h"
// Physical memory layout

// qemu -machine virt is set up like this,
// based on qemu's hw/riscv/virt.c:
//
// 00001000 -- boot ROM, provided by qemu
// 02000000 -- CLINT
// 0C000000 -- PLIC
// 10000000 -- uart0 
// 10001000 -- virtio disk 
// 80000000 -- boot ROM jumps here in machine mode
//             -kernel loads the kernel here
// unused RAM after 80000000.

// the kernel uses physical memory thus:
// 80000000 -- entry.S, then kernel text and data
// end -- start of kernel page allocation area
// PHYSTOP -- end RAM used by the kernel

// shutdown ??
#define FINISHER_BASE 0x100000L

// qemu puts UART registers here in physical memory.
#define UART0 0x10000000L
#define UART0_IRQ 10

// virtio mmio interface
#define VIRTIO0 0x10001000
#define VIRTIO0_IRQ 1

// core local interruptor (CLINT), which contains the timer.
#define CLINT 0x2000000L
#define CLINT_MTIMECMP(hartid) (CLINT + 0x4000 + 8*(hartid))
#define CLINT_MTIME (CLINT + 0xBFF8) // cycles since boot.

// qemu puts platform-level interrupt controller (PLIC) here.
#define PLIC 0x0c000000L
#define PLIC_PRIORITY (PLIC + 0x0)
#define PLIC_PENDING (PLIC + 0x1000)
#define PLIC_MENABLE(hart) (PLIC + 0x2000 + (hart)*0x100)
#define PLIC_SENABLE(hart) (PLIC + 0x2080 + (hart)*0x100)
#define PLIC_MPRIORITY(hart) (PLIC + 0x200000 + (hart)*0x2000)
#define PLIC_SPRIORITY(hart) (PLIC + 0x201000 + (hart)*0x2000)
#define PLIC_MCLAIM(hart) (PLIC + 0x200004 + (hart)*0x2000)
#define PLIC_SCLAIM(hart) (PLIC + 0x201004 + (hart)*0x2000)

// for opensbi
#define MBASE 0x80000000L

#define KERNBASE 0x80200000L
#define PHYSTOP (KERNBASE + 512 * 1024 * 1024)

// map the trampoline page to the highest address,
// in both user and kernel space.
#define TRAMPOLINE (MAXVA - PGSIZE)

// the kernel stack grows down from KSTACKTOP.
#define KSTACK_PAGE 64
#define MAX_THREAD NTHREADS_PER_PROC
// now every single thread has its own kernel stack
// map kernel stacks beneath the trampoline,
// each surrounded by invalid guard pages.'
// KSTACK means KSTACK_BASE actrually 
#define KSTACK(t) (TRAMPOLINE - ((t) + 1) * (KSTACK_PAGE + 1) * PGSIZE)
#define BRKTOP KSTACK(NTHREADS + 1) // bottom of the kernel stack

// user space

#define SIGRETURN (TRAMPOLINE - PGSIZE) // trampoline to call sigreturn syscall
#define TRAPFRAME (SIGRETURN - PGSIZE)
// thread-exclusive
#define THREAD_TRAPFRAME(idx) (TRAPFRAME - (idx) * PGSIZE)
// #define KSTACK(t) (THREAD_TRAPFRAME(MAX_THREAD - 1 - (t)) - KSTACK_PAGE * PGSIZE)


#endif