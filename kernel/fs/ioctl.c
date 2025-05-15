#include "types.h"
#include "ioctl.h"
#include "defs.h"
#include "file.h"
#include "proc.h"

struct termios kernel_termios;
struct winsize kernel_winsize;

int do_ioctl(struct file *f, uint64 op, uint64 arg) {
    switch (op) {
        case TCGETS:
            printf("TCGETS\n");
            return 0;
            // buggy after copyout,why??
            if (arg == 0) {
                return -1;
            }
            if (copyout(myproc()->mm.pagetable, arg, (char *)&kernel_termios, sizeof(kernel_termios)) < 0) {
                return -1;
            }
            break;
        case TCSETS:
            printf("TCSETS\n");
            if (arg == 0) {
                return -1;
            }
            if (copyin(myproc()->mm.pagetable, (char *)&kernel_termios, arg, sizeof(kernel_termios)) < 0) {
                return -1;
            }
            break;
        case TIOCGWINSZ:
            printf("TIOCGWINSZ\n");
            if (arg == 0) {
                return -1;
            }
            if (copyout(myproc()->mm.pagetable, arg, (char *)&kernel_winsize, sizeof(kernel_winsize)) < 0) {
                return -1;
            }
            break;
        default:
            return -1;
    }
    // printf("do_ioctl: %d\n", op);
    return 0;
}