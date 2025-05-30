#include "types.h"
#include "ioctl.h"
#include "defs.h"
#include "file.h"
#include "proc.h"
#include "debug.h"

struct termios kernel_termios = {
    .c_iflag = ICRNL,
    .c_oflag = OPOST,
    .c_cflag = 0,
    .c_lflag = ECHO | ICANON,
    .c_line = 0,
    .c_cc = {0},
};

struct winsize kernel_winsize;

int do_ioctl(struct file *f, uint64 op, uint64 arg) {
    switch (op) {
        case TCGETS:
            // printf("TCGETS\n");
            // return 0;
            // buggy after copyout,why??
            if (arg == 0) {
                return -1;
            }
            if (copyout(myproc()->mm.pagetable, arg, (char *)&kernel_termios, sizeof(kernel_termios)) < 0) {
                return -1;
            }
            /* return 1 ... see glibc src

            ISATTY(3)                                     Linux Programmer's Manual                                    ISATTY(3)

NAME
       isatty - test whether a file descriptor refers to a terminal

SYNOPSIS
       #include <unistd.h>

       int isatty(int fd);

DESCRIPTION
       The isatty() function tests whether fd is an open file descriptor referring to a terminal.

RETURN VALUE
       isatty() returns 1 if fd is an open file descriptor referring to a terminal; otherwise 0 is returned, and er‐
       rno is set to indicate the error.
       
            int
            isatty (int fd)
            {
            struct termios term;
            return __tcgetattr (fd, &term) != -1;
            }
            */
            return 1;
            break;
        case TCSETS:
            // printf("TCSETS\n");
            if (arg == 0) {
                return -1;
            }
            if (copyin(myproc()->mm.pagetable, (char *)&kernel_termios, arg, sizeof(kernel_termios)) < 0) {
                return -1;
            }
            break;
        case TIOCGWINSZ:
            // printf("TIOCGWINSZ\n");
            if (arg == 0) {
                return -1;
            }
            if (copyout(myproc()->mm.pagetable, arg, (char *)&kernel_winsize, sizeof(kernel_winsize)) < 0) {
                return -1;
            }
            break;
        // case TIOCGPGRP:
        //     // printf("TIOCGPGRP\n");
        //     if (arg == 0) {
        //         return -1;
        //     }
        //     if (copyout(myproc()->mm.pagetable, arg, (char *)&myproc()->pgid, sizeof(myproc()->pgid)) < 0) {
        //         return -1;
        //     }
            break;
        default:
            Log("UNKNOW IOCTL %d!", op);
            return -1;
    }
    // printf("do_ioctl: %d\n", op);
    return 0;
}