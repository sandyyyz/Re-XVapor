#include "types.h"
#include "defs.h"
#include "fcntl.h" 
#include "file.h"
#include "proc.h"

int getfd(struct file *f) {
    int fd;
    struct proc *p = myproc();
    for (fd = 0; fd < NOFILE; fd++) {
        if (p->ofile[fd] == f) {
            return fd;
        }
    }
    return -1;
}
int setfd(struct file *f, int fd) {
    struct proc *p = myproc();
    if (fd < 0 || fd >= NOFILE) {
        return -1;
    }
    if (p->ofile[fd] != 0) {
        return -1;
    }
    p->ofile[fd] = f;
    return 0;
}

/**
 * @brief allocate a file descriptor larger than or equal to the dmd for a file
 * 
 * @param f file 
 * @param dmd demanded file descriptor
 * @return file descriptor, -1 if failed
 */
int fdalloc_lteq(struct file *f, int dmd) {
    int fd;
    struct proc *p = myproc();
    if(dmd < 0 || dmd >= NOFILE) {
        return -1;
    }
    for (fd = dmd; fd < NOFILE; fd++) {
        if (p->ofile[fd] == 0) {
            p->ofile[fd] = f;
            return fd;
        }
    }
    return -1;
}

int do_fcntl(struct file *f, int cmd, uint64 arg) {
    int ret = 0;
    switch (cmd) {
        case F_DUPFD:
            if((ret = fdalloc_lteq(f, arg)) < 0)
                return -1;
            filedup(f);
            break;
        case F_GETFD:
            ret = getfd(f);
            break;
        case F_SETFD:
            if(setfd(f, arg) < 0) {
                ret = -1;
            }
            else {
                ret = 0;
            }
            break;
        case F_GETFL:
            ret = f->flags;
            break;
        case F_SETFL:
            f->flags = arg;
            break;
        default:
            ret = -1;
    }
    return ret;
}