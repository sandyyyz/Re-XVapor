#include "types.h"
#include "mmap.h"
#include "defs.h"
#include "proc.h" 
#include "arch.h"
#include "debug.h"

static uint64 allocvma() {
    struct vma_struct *vma = (struct vma_struct *)kalloc();
    if (vma == 0) {
        return 0;
    }
    memset(vma, 0, sizeof(struct vma_struct));
    return (uint64)vma;
}

/**
 * @brief find a given address in the vma list of a process
 * 
 * @param p process 
 * @param addr virtual address
 * @return pointer to the vma struct, 0 if not found 
 * @attention call with mm lock held
 */
struct vma_struct *find_vma(struct proc *p, uint64 addr) {
    struct vma_struct *vma;
    list_for_each_entry(vma, &p->mm.vma_list, vma_list) {
        if (vma->vm_start <= addr && vma->vm_end > addr) {
            return vma;
        }
    }
    return 0;
}

/**
 * @brief copy the vma list of a process to another process
 * 
 * @param p process
 * @param np new process
 * @return return 0 if success, -1 if failed
 * @attention call with two processes' mm lock held
 */
int proc_copy_vma(struct proc *p, struct proc *np) {
    struct vma_struct *vma, *nvma;
    list_for_each_entry(vma, &p->mm.vma_list, vma_list) {
        if((nvma = (struct vma_struct *)allocvma()) == 0) {
            return -1;
        }
        *nvma = *vma;
        if(nvma->file)
            nvma->file->ref++;
        list_add_tail(&nvma->vma_list, &np->mm.vma_list);
    }
    return 0;
}
/**
 * @brief free process vm
 * 
 * @param p process
 * @attention call without mm lock held
 */
void freeprocvm(struct proc *p) {
    struct vma_struct *vma, *next;
    list_for_each_entry_safe(vma, next, &p->mm.vma_list, vma_list) {
        mmap_writeback_unmapf(p->mm.pagetable, vma, vma->vm_end - vma->vm_start);
        acquire(&p->mm.lock);
        list_del(&vma->vma_list);
        release(&p->mm.lock);
        kfree(vma);
        vma = 0;
    }
}
uint64 sys_mmap(void) {
    /* come in like :
     * void *mmap(void addr, size_t length, int prot, int flags, int fd, off_t offset);*/
    uint64 addr;
    int length;
    int prot;
    int flags;
    int fd;
    int offset;
    struct file *fp;


    argaddr(0, &addr);
    argint(1, &length);
    argint(2, &prot);
    argint(3, &flags);
    if(flags & MAP_ANONYMOUS) {
        fd = -1;
        fp = NULL;
    } else {
        if(argfd(4, &fd, &fp) < 0) {
            Warn("sys_mmap: argfd failed");
            return -1;
        }
    }
    argint(5, &offset);

#ifdef __DEBUG_SYS_MMAP
    Log("sys_mmap: addr %p, length %d, prot %d, flags %d, fd %d, offset %d", addr, length, prot, flags, fd, offset);
#endif
    return do_mmap(addr, length, prot, flags, fd, fp, offset);


}
uint64 sys_munmap(void) {
    /**
     * come in like :
     * munmap(uint64 addr, int len)
     */

    uint64 addr;
    int len;

    argaddr(0, &addr);
    argint(1, &len);
#ifdef __DEBUG_SYS_MUNMAP
    Log("sys_munmap: addr %p, len %d", addr, len);
#endif
    return do_munmap(addr, len);
    
}
/**
 * @brief map a file to the virtual memory of a process
 * 
 * @param addr given address
 * @param length length of the file
 * @param prot protection of the memory
 * @param flags mapping flags, use to determine the behavior of the mapping
 * @param fd file descriptor, if -1, the mapping is anonymous
 * @param fp file pointer, NUll if fd is -1
 * @param offset offset in the file
 * @return return the start address of the mapping, -1 if failed
 */
uint64 do_mmap(uint64 addr, uint64 length, uint64 prot, uint64 flags, uint64 fd, __nullable struct file *fp, uint64 offset) {

    struct proc *p = myproc();
    struct vma_struct *vma;
    // printf("fp->writeable: %d\n", fp->writable);
    // printf("fp->flags: %d\n", fp->flags);

    if(fp && !IS_WRITABLE(fp->flags) && ((prot & PROT_WRITE) && flags & MAP_SHARED)) {
        return -1;
    }

    if((vma = (struct vma_struct *)allocvma()) == 0) {
        return -1;
    }
    acquire(&p->mm.lock);
    vma->vm_end = PGROUNDDOWN(p->mm.max_vma);
    vma->vm_start = PGROUNDDOWN(p->mm.max_vma - length);
    vma->valid = 1;
    vma->prot = prot;
    vma->flags = flags;
    vma->fd = fd;
    vma->file = fp;
    vma->offset = offset;
    p->mm.max_vma = vma->vm_start;
    if(flags & MAP_ANONYMOUS) {
        // fp = NULL. fd = -1 or 0. just ignore
        vma->type = VMA_ANONYMOUS;
    } else {
        vma->type = VMA_FILE;
        vma->file->ref++;
    }
    // TODO: here has a problem that the max_vma is always going down for a given process...
    // just make a check right now...
    if(p->mm.max_vma < MMAP_END_ADDRESS) {
        release(&p->mm.lock);
        Warn("mmap: out of memory\n");
        kfree((void *)vma);
        return -1;
    }
    list_add_tail(&vma->vma_list, &p->mm.vma_list);
    release(&p->mm.lock);
#ifdef __DEBUG_DO_MMAP
    Log("process %d do mmap: vma_start %p, vma_end %p, fd %d, offset %d\n", p->pid, vma->vm_start, vma->vm_end, vma->fd, vma->offset);
#endif
    return vma->vm_start;
}
/**
 * @brief write back the dirty pages of a vma to the file, unmap and free the pages
 * 
 * @param vma vma 
 * @param pgtable pagetable
 * @param len length of the vma want to unmap
 * @return return 0 if success, -1 if failed
 */
int mmap_writeback_unmapf(pagetable_t pgtable, struct vma_struct *vma, int len) {
    pte_t *pte;
    uint64 va;
    struct file *fp = vma->file;
    for(va = PGROUNDDOWN(vma->vm_start); va < vma->vm_start + len  && va < vma->vm_end; va += PGSIZE) {
        pte = walk(myproc()->mm.pagetable, va, 0);
        if(pte == 0) {
            panic("mmap_writeback: walk");
        }
        // the page could not be mapped and allocated because never access by the thread, then never reach mmap pgfault handler
        if(!(*pte) || !(*pte & PTE_V)) {
            continue;
        }
        if((*pte & PTE_D) && (vma->flags & MAP_SHARED) && (vma->type == VMA_FILE)) {
        // dirty page, write back to file
        // write back 
        filewrite(fp, 1, va, PGSIZE, fp->fpos);
        }
#ifdef __DEBUG_MMAP_WRITEBACK
        Log("mmap_writeback_unmapf: va %p, pte %p, pa %p\n", va, *pte, PTE2PA(*pte));
#endif
        uvmunmap(pgtable, va, 1, 1);
        *pte = 0;
    }
    acquire(&myproc()->mm.lock);
    vma->vm_start += len;
    release(&myproc()->mm.lock);

    return 0;
}

int do_munmap(uint64 addr, int len) {
    struct proc *p = myproc();
    struct vma_struct *vma;

    // TODO: 
    // if we don't acquire &p->mm.lock, how can we protect the vma?...
    // but if we acquire it, here is a potential deadlock that another thread is holding the file inode lock
    // and waiting for the mm lock, and we are holding the mm lock and waiting for the file inode lock...
    // when we sleep and waiting file inode lock..
    // but is that possible?... maybe holding a spinlock when sleep is not always a bad thing...
    // just never hold a spinlock when sleep right now in thread_sched()...

    acquire(&p->mm.lock);
    vma = find_vma(p, addr);
    release(&p->mm.lock);

    if(vma == 0) {
        // release(&p->mm.lock);
        return -1;
    }
    mmap_writeback_unmapf(p->mm.pagetable, vma, len);
    // free vma if it's empty
    acquire(&p->mm.lock);
    if(vma->vm_start >= vma->vm_end) {
        if(vma->type == VMA_FILE)
            vma->file->ref--;
        list_del(&vma->vma_list);
        kfree((void *)vma);
    }
    release(&p->mm.lock);
    return 0;
}

static int setperm(pagetable_t pagetable, uint64 va, uint64 perm)
{
    pte_t *pte;

    if (va >= MAXVA)
        return -1;

    pte = walk(pagetable, va, 0);
    if (pte == 0 || (*pte & PTE_V) == 0)
        return 0;
    if ((*pte & PTE_U) == 0)
        return -1;
    *pte |= perm;
    return 0;
}

int do_mprotect(uint64 addr, int size, int prot) {
    struct proc *p = myproc();
    struct vma_struct *vma;
    uint64 perm;
    int pgnum;
    uint64 va = addr;

    acquire(&p->mm.lock);
    vma = find_vma(p, addr);
    if(vma == 0 || vma->vm_start > addr || vma->vm_end < addr + size) {
        release(&p->mm.lock);
        // return -1; // not found or not in the range
        goto notbymmap; // not by mmap, just set the permission
    }
    vma->prot = prot;
    release(&p->mm.lock);

notbymmap:
    pgnum = PGROUNDDOWN(size) / PGSIZE;
    perm = PROT2PTE_FLAGS(prot);
    perm |= PTE_U;

#ifdef __DEBUG_DO_MPROTECT
    Log("do_mprotect: addr %p, size %d, prot %p, perm %p, pgnum %d", addr, size, prot, perm, pgnum);
#endif
    for(int i = 0; i < pgnum; i++) {
        if (setperm(p->mm.pagetable, va, perm) != 0)
            panic("out of maxva or not user page");
        va += PGSIZE;
    }
    return 0;
}
/**
 * @brief mprotect() changes the access protections for the calling
       process's memory pages containing any part of the address range in
       the interval [addr, addr+size-1].  addr must be aligned to a page
       boundary.
 * @property int mprotect(void addr[.size], size_t size, int prot);
 * @return On success, mprotect() return zero.  On error,
       these system calls return -1, and errno is set to indicate the
       error.
 */
uint64 sys_mprotect(void) {
    uint64 addr;
    int size;
    int prot;

    argaddr(0, &addr);
    argint(1, &size);
    argint(2, &prot);
#ifdef __DEBUG_SYS_MPROTECT
    Log("sys_mprotect: addr %p, size %d, prot %d", addr, size, prot);
#endif
    return do_mprotect(addr, size, prot);
}
