        # qemu -kernel loads the kernel at 0x80000000
        # and causes each hart (i.e. CPU) to jump there.
        # kernel.ld causes the following code to
        # be placed at 0x80000000.
.section .text
.global _entry
.section .text
.global _entry
_entry:
    # set up a stack for C.
    # stack0 is declared in start.c,
    # with a 4096-byte stack per CPU.
    # sp = stack0 + (hartid * 4096)

    # // entry.S needs one stack per CPU.
    # __attribute__ ((aligned (16))) char stack0[4096 * NCPU];
    # 每个CPU分配一个独立的栈区，1page
    
    la sp, stack0       # 将栈指针 (sp) 设置为 stack0 的地址
    li a0, 1024*4       # 将 a0 寄存器设置为 1024 * 4 (4096)，表示每个 CPU 的栈大小
    csrr a1, mhartid    # 读取 mhartid 寄存器的值，即当前 CPU 的硬件线程 ID
    addi a1, a1, 1      # 将 mhartid 加 1，这是因为 mhartid 从 0 开始
    mul a0, a0, a1      # 计算 a0 = a0 * a1，即每个 CPU 的栈大小
    add sp, sp, a0      # 计算栈指针 sp = sp + a0，即设置每个 CPU 的栈起始地址
    # jump to start() in start.c
    call start          # 调用 start 函数

spin:
    j spin              # 进入一个无限循环，使程序持续处于自旋状态