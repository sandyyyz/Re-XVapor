#ifndef __DEFS_H__
#define __DEFS_H__

struct buf;
struct context;
struct inode;
struct pipe;
struct proc;
struct spinlock;
struct sleeplock;
struct stat;
struct xv6fs_superblock;
struct tcb;
struct file;

// bio.c
void            binit(void);
struct buf*     bread(uint, uint);
void            brelse(struct buf*);
void            bwrite(struct buf*);
void            bpin(struct buf*);
void            bunpin(struct buf*);
struct buf* bget(uint dev, uint blockno);

// console.c
void            consoleinit(void);
void            consoleintr(int);
void            consputc(int);

// exec.c
int             exec(char*, char**);
int execve(char *path, char **argv, char **envp);
// file.c
struct file*    filealloc(void);
void            fileclose(struct file*);
struct file*    filedup(struct file*);
void            fileinit(void);
int fileread(struct file *f, int user_dst, uint64 addr, int n, int off);
int             filestat(struct file*, uint64 addr);
int filewrite(struct file *f, int user_src, uint64 addr, int n, int off);

// fs.c
struct inode*   idup(struct inode*);
struct inode*   namei(char*);
struct inode*   nameiparent(char*, char*);

// ramdisk.c
void            ramdiskinit(void);
void            ramdiskintr(void);
void            ramdiskrw(struct buf*);

// kalloc.c
void*           kalloc(void);
void            kfree(void *);
void            kinit(void);
void*           kzalloc(void);
void*           kmalloc(uint size);
void*           kcalloc(int n, uint size);
uint64          freemem_pages(void);
uint64          freemem_bytes(void);
uint64          totalram_bytes(void);
// log.c
void            initlog(int, struct xv6fs_superblock*);
void            log_write(struct buf*);
void            begin_op(void);
void            end_op(void);

// pipe.c
int             pipealloc(struct file**, struct file**);
void            pipeclose(struct pipe*, int);
int piperead(struct pipe *pi, int user_dst, uint64 addr, int n);
int pipewrite(struct pipe *pi, int user_src, uint64 addr, int n);

// printf.c
void            printf(const char*, ...);
void            panic(char*) __attribute__((noreturn));
void            printfinit(void);

// proc.c
int             cpuid(void);
void            proc_exit(int);
int             fork(void);
int             growproc(int);
void            thread_mapstacks(pagetable_t);
pagetable_t     proc_pagetable(struct proc *);
void            proc_freepagetable(pagetable_t pagetable, uint64 sz, int unmmap_ttf);
int             kill(int);
int             killed(struct proc*);
void            setkilled(struct proc*);
struct cpu*     mycpu(void);
struct cpu*     getmycpu(void);
struct proc*    myproc();
void            procinit(void);
int do_clone(int flags, uint64 stack, pid_t ptid, uint64 tls, pid_t *ctid);
// void            scheduler(void) __attribute__((noreturn));
// void            sched(void);
void            thread_scheduler(void) __attribute__((noreturn));
void            thread_sched(void);
int thread_killed(struct tcb *t);

// void            sleep(void*, struct spinlock*);
void            userinit(void);
int             wait_one(uint64);
// void            wakeup(void*);
// void            yield(void);
int             either_copyout(int user_dst, uint64 dst, void *src, uint64 len);
int             either_copyin(void *dst, int user_src, uint64 src, uint64 len);
void            procdump(void);
int wait4(pid_t pid, uint64 pstatus, int options);
pid_t waitpid(pid_t pid, uint64 wstatus, int options);

void thread_sleep(void*, struct spinlock*);
void thread_wakeup_chan(void *chan);
void thread_yield(void);

// swtch.S
// push a1, push a0
void            swtch(struct context*, struct context*);

// spinlock.c
void            acquire(struct spinlock*);
int             holding(struct spinlock*);
void            initlock(struct spinlock*, char*);
void            release(struct spinlock*);
void            push_off(void);
void            pop_off(void);

// sleeplock.c
void            acquiresleep(struct sleeplock*);
void            releasesleep(struct sleeplock*);
int             holdingsleep(struct sleeplock*);
void            initsleeplock(struct sleeplock*, char*);

// string.c
void* memcpy(void *dst, const void *src, uint n);
int             memcmp(const void*, const void*, uint);
void*           memmove(void*, const void*, uint);
void*           memset(void*, int, uint);
char*           safestrcpy(char*, const char*, int);
int             strlen(const char*);
int             strncmp(const char*, const char*, uint);
char*           strncpy(char*, const char*, int);
int strcmp(const char *p, const char *q);
char * strcpy(char *s, const char *t);
int substr_cmp(const char *p, const char *q);
char *strcat(char *dest, const char *src);

// syscall.c
void            arglong(int, uint64*);
void            argint(int, int*);
int             argstr(int, char*, int);
void arguint32(int n, uint32 *lip);
void arguint64(int n, uint64 *lip);
void            argaddr(int, uint64 *);
int             fetchstr(uint64, char*, int);
int             fetchaddr(uint64, uint64*);
void            syscall();

int argfd(int n, int *pfd, struct file **pf);
// trap.c
extern uint     ticks;
void            trapinit(void);
void            trapinithart(void);
extern struct spinlock tickslock;
void            usertrapret(void);

// uart.c
void            uartinit(void);
void            uartintr(void);
void            uartputc(int);
void            uartputc_sync(int);
int             uartgetc(void);

// vm.c
void            kvminit(void);
void            kvminithart(void);
void            kvmmap(pagetable_t, uint64, uint64, uint64, int);
int mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm);
pagetable_t     uvmcreate(void);
void            uvmfirst(pagetable_t, uchar *, uint);
uint64          uvmalloc(pagetable_t, uint64, uint64, int);
uint64          uvmdealloc(pagetable_t, uint64, uint64);
int             uvmcopy(pagetable_t, pagetable_t, uint64);
void            uvmfree(pagetable_t, uint64);
void            uvmunmap(pagetable_t, uint64, uint64, int);
void            uvmclear(pagetable_t, uint64);
pte_t *         walk(pagetable_t, uint64, int);
uint64          walkaddr(pagetable_t, uint64);
int             copyout(pagetable_t, uint64, char *, uint64);
int             copyin(pagetable_t, char *, uint64, uint64);
int             copyinstr(pagetable_t, char *, uint64, uint64);
void            vmprint(pagetable_t pgtbl);
// plic.c
void            plicinit(void);
void            plicinithart(void);
int             plic_claim(void);
void            plic_complete(int);

// virtio_disk.c
void            virtio_disk_init(void);
void            virtio_disk_rw(struct buf *, int);
void            virtio_disk_intr(void);

// number of elements in fixed-size array
#define NELEM(x) (sizeof(x)/sizeof((x)[0]))
#define N2ADDR(x) ((void*)(x))
#define ADDR2N(x) ((uint64)(x))

#endif // __DEFS_H__