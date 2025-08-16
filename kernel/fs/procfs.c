#include "types.h"
#include "defs.h"
#include "vfs.h"
#include "procfs.h"
#include "debug.h"
#include "snprintf.h"
#include "proc.h"

static int procfs_read(struct file *fp, int user_dst, uint64 dst, int64_t off, size_t size, size_t *rcnt);
static int procfs_write(struct file *fp, int user_src, uint64 src, int64_t off, size_t size, size_t *wcnt);
static int procfs_open(struct file *fp, const char *path, int flags);
static int procfs_rename(const char *oldpath, const char *newpath);
static int procfs_close(struct file *fp);
static int procfs_unlink(const char* path, int flags);
static int proc_interrupts_rename(const char *oldpath, const char *newpath);
static int proc_interrupts_read(struct file *fp, int user_dst, uint64 dst, int64_t off, size_t size, size_t *rcnt);
static int proc_interrupts_write(struct file *fp, int user_src, uint64 src, int64_t off, size_t size, size_t *wcnt);
static int proc_interrupts_open(struct file *fp, const char *path, int flags);
static int proc_interrupts_close(struct file *fp);
static int proc_interrupts_unlink(const char* path, int flags);

void record_intr(uint64 intr);

volatile uint64 intrcnt[MAXINTR] = {0};

struct file_ops procfs_file_ops = {
    .read = procfs_read,
    .write = procfs_write,
    .open = procfs_open,
    .close = procfs_close,
};

struct fs_ops procfs_fs_ops = {
    .rename = procfs_rename,
    .unlink = procfs_unlink,
};

struct vfs_filesystem procfs = {
    .name = "procfs",
    .type = VFS_TYPE_PROCFS,
    .fops = &procfs_file_ops,
    .fsops = &procfs_fs_ops,
    .path = "/proc"
};

int init_procfs() {
    return register_fs(&procfs);
}

static int procfs_unlink(const char *path, int flags){
    if(strcmp(path, "/proc/interrupts") == 0) {
        return proc_interrupts_unlink(path, flags);
    }
    return -1;
}
int procfs_rename(const char *oldpath, const char *newpath) {
    if(strcmp(oldpath, "/proc/interrupts") == 0) {
        return proc_interrupts_rename(oldpath, newpath);
    }
    return -1;
}
static int procfs_read(struct file *fp, int user_dst, uint64 dst, int64_t off, size_t size, size_t *rcnt) {
    char *path = fp->info.path;
    if(strcmp(path, "/proc/interrupts") == 0) {
        return proc_interrupts_read(fp, user_dst, dst, off, size, rcnt);
    }
    return -1;
}


static int procfs_write(struct file *fp, int user_src, uint64 src, int64_t off, size_t size, size_t *wcnt) {
    char *path = fp->info.path;
    if(strcmp(path, "/proc/interrupts") == 0) {
        return proc_interrupts_write(fp, user_src, src, off, size, wcnt);
    }
    return -1;
}

static int procfs_open(struct file *fp, const char *path, int flags) {
    if(strcmp(path, "/proc/interrupts") == 0) {
        return proc_interrupts_open(fp, path, flags);
    }
    return -1;
}

static int procfs_close(struct file *fp) {
    char *path = fp->info.path;
    if(strcmp(path, "/proc/interrupts") == 0) {
        return proc_interrupts_close(fp);
    }
    return -1;
}


static int proc_interrupts_read(struct file *fp, int user_dst, uint64 dst, int64_t off, size_t size, size_t *rcnt) {
    void *buf;

    int la = 0;
    for(int i = 0; i < MAXINTR; i++) 
        if(intrcnt[i] != 0 && i > la)
            la = i;
    if(la == fp->fpos) {
        fp->fpos = 0;
        *rcnt = 0;
        return 0;
    }

    if(user_dst) {
        if((buf = kmalloc(PGSIZE)) == NULL) {
            Warn("kzalloc failed");
            return -1;
        }
    } else {
        buf = (void*)dst;
    }
    char *p = buf;
    size_t len = 0;
#ifdef __DEBUG_PROC_INTR_READ
    Log("user_dst = %d, dst = %p, buf = %p, size = %d", user_dst, dst, buf, size);
#endif
    for(int i = 0; i < MAXINTR; i++) {
        if(intrcnt[i] != 0) {
            size_t r = sprintf(p, "%d:      %d\n", i, intrcnt[i]);
#ifdef __DEBUG_PROC_INTR_READ
            Log("sprintf %d:      %d", i, intrcnt[i]);
#endif
            len += r;
            p += r;
        }
    }
    if(user_dst && copyout(myproc()->mm.pagetable, dst, buf, len) < 0) {
        Warn("copyout failed!");
        kfree(buf);
        return -1;
    }
    *rcnt = len;
    fp->fpos = la;
    if(user_dst) {
        kfree(buf);
    }
    return 0;
}

static int proc_interrupts_write(struct file *fp, int user_src, uint64 src, int64_t off, size_t size, size_t *wcnt) {
    // cannot write
    return -1;
}

static int proc_interrupts_open(struct file *fp, const char *path, int flags) {
    fp->type = FD_SPEC;
    return 0;
}

static int proc_interrupts_close(struct file *fp) {
    return 0;
}

static int proc_interrupts_rename(const char *oldpath, const char *newpath) {
    // cannot rename
    return -1;
}

static int proc_interrupts_unlink(const char* path, int flags) {
    // cannot unlink
    return -1;
}
void record_intr(uint64 intr) {
    if(intr >= MAXINTR) {
        return;
    }
    intrcnt[intr]++;
}