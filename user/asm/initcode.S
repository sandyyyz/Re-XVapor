# Initial process that execs /glibc/busybox sh
# This code runs in user space.

#include "syscall.h"

.globl start
start:
        la a0, busybox_path    # const char *path = "/glibc/busybox"
        la a1, argv            # char *argv[] = { "sh", 0 }
        li a7, SYS_exec
        ecall

exit:
        li a7, SYS_exit
        ecall
        jal exit

# char busybox_path[] = "/glibc/busybox\0";
busybox_path:
  .string "/glibc/busybox\0"

# char arg0[] = "sh\0";
shstr:
  .string "sh\0"

# char *argv[] = { arg0, 0 };
.p2align 2
argv:
  .long shstr
  .long 0
