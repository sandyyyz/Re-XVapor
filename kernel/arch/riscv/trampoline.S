        #
        # low-level code to handle traps from user space into
        # the kernel, and returns from kernel to user.
        #
        # the kernel maps the page holding this code
        # at the same virtual address (TRAMPOLINE)
        # in user and kernel space so that it continues
        # to work when it switches page tables.
        # kernel.ld causes this code to start at 
        # a page boundary.
        #

#include "arch.h"
#include "memlayout.h"

.section trampsec
.globl trampoline
trampoline:
.align 4
.globl uservec
uservec:    
	#
        # trap.c sets stvec to point here, so
        # traps from user space start here,
        # in supervisor mode, but with a
        # user page table.
        #
        
        # sscratch saves THREAD_TRAPFRAME now 
        
        # swap sscratch and a0
        # a0 can be used to get at TRAPFRAME.
        csrrw a0, sscratch, a0
        

        # save the user registers in TRAPFRAME
        sd ra, 40(a0)
        sd sp, 48(a0)
        sd gp, 56(a0)
        sd tp, 64(a0)
        sd t0, 72(a0)
        sd t1, 80(a0)
        sd t2, 88(a0)
        sd s0, 96(a0)
        sd s1, 104(a0)
        sd a1, 120(a0)
        sd a2, 128(a0)
        sd a3, 136(a0)
        sd a4, 144(a0)
        sd a5, 152(a0)
        sd a6, 160(a0)
        sd a7, 168(a0)
        sd s2, 176(a0)
        sd s3, 184(a0)
        sd s4, 192(a0)
        sd s5, 200(a0)
        sd s6, 208(a0)
        sd s7, 216(a0)
        sd s8, 224(a0)
        sd s9, 232(a0)
        sd s10, 240(a0)
        sd s11, 248(a0)
        sd t3, 256(a0)
        sd t4, 264(a0)
        sd t5, 272(a0)
        sd t6, 280(a0)

	# save the user a0 in p->trapframe->a0
        csrr t0, sscratch
        sd t0, 112(a0)

        # initialize kernel stack pointer, from p->trapframe->kernel_sp
        ld sp, 8(a0)

        # make tp hold the current hartid, from p->trapframe->kernel_hartid
        ld tp, 32(a0)

        # load the address of usertrap(), from p->trapframe->kernel_trap
        ld t0, 16(a0)


        # fetch the kernel page table address, from p->trapframe->kernel_satp.
        ld t1, 0(a0)

        # wait for any previous memory operations to complete, so that
        # they use the user page table.
        sfence.vma zero, zero

        # install the kernel page table.
        csrw satp, t1

        # flush now-stale user entries from the TLB.
        sfence.vma zero, zero

        # jump to usertrap(), which does not return
        jr t0


.globl userret
userret:

        # usertrapret call this function
        # ((void (*)(uint64, uint64))trampoline_userret)(satp, THREAD_TRAPFRAME(t->tidx));
        # a0: user page table, for satp.
        # a1: THREAD_TRAPFRAME(t->tidx)

        # switch to the user page table.
        sfence.vma zero, zero
        csrw satp, a0
        sfence.vma zero, zero

        # restore all but a1 from TRAPFRAME
        ld ra, 40(a1)
        ld sp, 48(a1)
        ld gp, 56(a1)
        ld tp, 64(a1)
        ld t0, 72(a1)
        ld t1, 80(a1)
        ld t2, 88(a1)
        ld s0, 96(a1)
        ld s1, 104(a1)
        ld a0, 112(a1)
        ld a2, 128(a1)
        ld a3, 136(a1)
        ld a4, 144(a1)
        ld a5, 152(a1)
        ld a6, 160(a1)
        ld a7, 168(a1)
        ld s2, 176(a1)
        ld s3, 184(a1)
        ld s4, 192(a1)
        ld s5, 200(a1)
        ld s6, 208(a1)
        ld s7, 216(a1)
        ld s8, 224(a1)
        ld s9, 232(a1)
        ld s10, 240(a1)
        ld s11, 248(a1)
        ld t3, 256(a1)
        ld t4, 264(a1)
        ld t5, 272(a1)
        ld t6, 280(a1)

        # save THREAD_TRAPFRAME in sscratch
        csrw sscratch, a1

        # restore a1
        ld a1, 120(a1)

        sret
        