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

#ifdef __ARCH_LOONGARCH

#ifdef __ASSEMBLY__
#define _CONST64_(x)    x
#else
#define _CONST64_(x)    x ## L
#endif

#define DMW_PABITS	48

/* Direct Map windows registers */
#define LOONGARCH_CSR_DMWIN0		0x180	/* 64 direct map win0: MEM & IF */
#define LOONGARCH_CSR_DMWIN1		0x181	/* 64 direct map win1: MEM & IF */
#define LOONGARCH_CSR_DMWIN2		0x182	/* 64 direct map win2: MEM */
#define LOONGARCH_CSR_DMWIN3		0x183	/* 64 direct map win3: MEM */

/* Direct Map window 0/1 */
#define CSR_DMW0_PLV0		_CONST64_(1 << 0)
#define CSR_DMW0_VSEG		_CONST64_(0x8000)
#define CSR_DMW0_BASE		(CSR_DMW0_VSEG << DMW_PABITS)
#define CSR_DMW0_INIT		(CSR_DMW0_BASE | CSR_DMW0_PLV0)

#define CSR_DMW1_PLV0		_CONST64_(1 << 0)
#define CSR_DMW1_MAT		_CONST64_(1 << 4)
#define CSR_DMW1_VSEG		_CONST64_(0x9000)
#define CSR_DMW1_BASE		(CSR_DMW1_VSEG << DMW_PABITS)
#define CSR_DMW1_INIT		(CSR_DMW1_BASE | CSR_DMW1_MAT | CSR_DMW1_PLV0)

#define KERNBASE CSR_DMW1_BASE
#define V2P(v)		(v-KERNBASE)
#define P2V(p)		(p+KERNBASE)
#define RAMBASE (0x90000000UL | KERNBASE)
// #define PHYSTOP		0x98000000L
#define PHYSTOP (256 * 1024 * 1024)


/* ============== LS7A registers =============== */
#define LS7A_PCH_REG_BASE		(0x10000000UL | KERNBASE)

#define LS7A_INT_MASK_REG		LS7A_PCH_REG_BASE + 0x020
#define LS7A_INT_EDGE_REG		LS7A_PCH_REG_BASE + 0x060
#define LS7A_INT_CLEAR_REG		LS7A_PCH_REG_BASE + 0x080
#define LS7A_INT_HTMSI_VEC_REG		LS7A_PCH_REG_BASE + 0x200
#define LS7A_INT_STATUS_REG		LS7A_PCH_REG_BASE + 0x3a0
#define LS7A_INT_POL_REG		LS7A_PCH_REG_BASE + 0x3e0

#define UART0 (0x1fe001e0UL | KERNBASE)
#define UART0_IRQ 2

#else  // __ARCH_RISCV

// qemu puts UART registers here in physical memory.
#define UART0 0x10000000L
#define UART0_IRQ 10

#define PHYSTOP (KERNBASE + 512 * 1024 * 1024)
#define KERNBASE 0x80200000L
// for opensbi
#define MBASE 0x80000000L

#endif

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