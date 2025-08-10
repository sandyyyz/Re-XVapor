	# qemu -kernel loads the kernel at 0x0000000
        # and causes each CPU to jump there.
        # kernel.ld causes the following code to
        # be placed at 0x9000000000000000.
#define		__ASSEMBLY__
#include "memlayout.h"
.section .text
.global _entry
_entry:
	# set up a stack for C.
        # stack0 is declared in start.c,
        # with a 4096-byte stack per CPU.
        # sp = stack0 + (hartid * 4096)

	# This writes 0xa8 to control register 0, setting the privilege level to 0, disabling interrupts, and enabling paging.
	li.w	$t0, 0xb0		# PLV=0, IE=0, PG=1, DATF = DATM = 01
	csrwr	$t0, 0x0		# CRMD

	# This writes 0x04 to control register 1, keeping the privilege level at 0, enabling interrupts, and disabling write protection.
	li.w	$t0, 0x04		# PLV=0, PIE=1, PWE=0
	csrwr	$t0, 0x1		# PRMD

	# This writes 0x00 to control register 2, disabling various exceptions.
	li.w	$t0, 0x00		# FPE=0, SXE=0, ASXE=0, BTE=0
	csrwr	$t0, 0x2		# EUEN

	# 使用映射窗口映射整个物理内存
	li.d	$t0, CSR_DMW0_INIT	# UC, PLV0, 0x8000 xxxx xxxx xxxx
	csrwr	$t0, LOONGARCH_CSR_DMWIN0
	li.d	$t0, CSR_DMW1_INIT
	csrwr	$t0, LOONGARCH_CSR_DMWIN1
	
	# 由于已经在虚拟地址空间执行，直接设置栈
	la.abs	$sp, stack0+4096
	csrrd	$t0, 0x20		    # CPUID
	andi	$t0, $t0, 0x3ff
	slli.d	$t0, $t0, 12
	add.d	$sp, $sp, $t0
    # 实现每个CPU拥有独立的栈空间。
    # 通过使用CPUID作为偏移量，可以为每个CPU分配一个唯一的栈空间，避免不同CPU之间的栈数据互相干扰。

    bl start				# call start in start.c
spin:
	b spin