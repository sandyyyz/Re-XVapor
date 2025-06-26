        # qemu -kernel loads the kernel at 0x80000000
        # and causes each hart (i.e. CPU) to jump there.
        # kernel.ld causes the following code to
        # be placed at 0x80000000.
.section .text
.global _entry
_entry:
    # set up a stack for C.
    # stack0 is declared in start.c,
    # with a 4096-byte stack per CPU.
    # sp = stack0 + ((hartid+1) * 4096)
    # sbi把hart id 放在了 a0
    mv tp, a0
    la sp, stack0
    # 内核栈是否需要加大
    li t0, 1024*4
    addi a0, a0, 1
    mul t0, t0, a0
    add sp, sp, t0
    # jump to start() in start.c
    call start

spin:
    j spin              # 进入一个无限循环，使程序持续处于自旋状态