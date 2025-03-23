#ifndef __MMAP_H
#define __MMAP_H

#include "types.h"
#include "memlayout.h"
#include "list.h"
#include "file.h"

// prot
#define PROT_READ	0x1		/* page can be read */
#define PROT_WRITE	0x2		/* page can be written */
#define PROT_EXEC	0x4		/* page can be executed */
#define PROT_NONE	0x0		/* page can not be accessed */

// flags
#define MAP_SHARED	0x01		/* Share changes */
#define MAP_PRIVATE	0x02		/* Changes are private */
#define MAP_SHARED_VALIDATE 0x03	/* share + validate extension flags */
/* compatibility flags */
#define MAP_FILE	0
/* flags for mmap */
#ifndef MAP_FAILED
#define MAP_FAILED ((void *)-1)
#endif

#define MMAP_MAX_ADDR_START THREAD_TRAPFRAME(MAX_THREAD) /* max vitural start-address mmap can use, from here go downside*/
#define MMAP_END_ADDRESS MMAP_MAX_ADDR_START - 1024 * 1024 * 1024 /* 1GB */
#define PROT2PTE_FLAGS(prot) ((prot & PROT_READ ? PTE_R : 0) | (prot & PROT_WRITE ? PTE_W : 0) | (prot & PROT_EXEC ? PTE_X : 0))
uint64 sys_mmap(void);
uint64 sys_munmap(void);
uint64 do_mmap(uint64 addr, uint64 len, uint64 prot, uint64 flags, uint64 fd, struct file *fp, uint64 offset);
int do_munmap(uint64 addr, int len);
struct vma_struct *find_vma(struct proc *p, uint64 addr);
void freeprocvm(struct proc *p);
int proc_copy_vma(struct proc *p, struct proc *np);
int mmap_writeback_unmapf(pagetable_t pgtable, struct vma_struct *vma);

struct vma_struct {
    int valid; // 0: invalid, 1: valid
    uint64 vm_start; // start address
    uint64 vm_end; // end address
    uint64 offset; // offset in file
    int flags; // flags
    int prot; // protection
    int fd; // file descriptor
    struct file* file; // file
    struct list_head vma_list; // list
};

#endif