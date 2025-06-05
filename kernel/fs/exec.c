#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "elf.h"
#include "thread.h"
#include "debug.h"
#include "vm.h"
#include "ext4fs.h"
#include "fcntl.h"

//动态链接器所需要的一些辅助信息数组（Auxiliary Vector)
// type: 类型 val:类型对应的值
#define ADD_AUXV(type, val) \
    aux[index++] = type;    \
    aux[index++] = val;

static int floadseg(pagetable_t pagetable, struct file *f, uint64 va, uint offset, uint sz);
#ifdef __USE_XV6FS
static int loadseg(pde_t *, uint64, struct inode *, uint, uint);
#endif

int flags2perm(int flags)
{
    int perm = 0;
    if(flags & 0x1)
      perm = PTE_X;
    if(flags & 0x2)
      perm |= PTE_W;
    return perm;
}
/**
 * @brief execute a program
 * 
 * @param path the program path
 * @param argv arguments
 * @return argc of the main(argc, argv), -1 if failed
 * @details create a new pagetable and load the program into memory
 * @details create a new stack for the program
 * @details set trapframe->sp to ustack,and epc to 
 * @attention TODO: how to manage other threads' memory in pgtable? just kill them all right now , and the current thread turn to be the group leader
 */
int exec(char *path, char **argv)
{
#ifdef __DEBUG_EXEC
  Log("exec path: %s", path);
#endif

  char *s, *last;
  int i, off, index = 0;
  uint64 argc, sz = 0, sp, ustack[MAXARG], stackbase;
  struct elfhdr elf;
  int r = 0, rcnt = 0;
  struct proghdr ph;
  pagetable_t pagetable = 0, oldpagetable;
  
  struct proc *p = myproc();
  struct tcb *t = mythread();

  char abs_path[MAXPATH];
  struct file *f;
  if((f = filealloc()) == 0)
    return -1;
  get_absolute_path(path, myproc()->cinfo.path, abs_path);
  if((r = ext4_vfopen(f, abs_path, O_RDONLY)) != EOK) {
    Log("ext4_fopen2 failed %d", r);
    return -1;
  }

  if(((r = ext4_vfread(f, 0, (uint64) &elf, 0, sizeof(elf), &rcnt)) != EOK) || 
     (rcnt != sizeof(elf))) {
    Log("ext4_fread failed %d", r);
    ext4_vfclose(f);
    return -1;
  }

  if(elf.magic != ELF_MAGIC)
    goto bad;

  if((pagetable = proc_pagetable(p)) == 0)
    goto bad;

  
  
  // Load program into memory.
  for(i=0, off=elf.phoff; i<elf.phnum; i++, off+=sizeof(ph)){
    if((r = ext4_vfread(f, 0, (uint64)&ph, off, sizeof(ph), &rcnt)) != EOK || rcnt != sizeof(ph))
      goto bad;

    if(ph.type == ELF_PROG_INTERP)
      printf("ELF_PROG_INTERP not supported\n");
    if(ph.type != ELF_PROG_LOAD) {
      printf("ph.type == 0x%x\n", ph.type);
      continue;
    }
    if(ph.memsz < ph.filesz)
      goto bad;
    if(ph.vaddr + ph.memsz < ph.vaddr)
      goto bad;
    // if(ph.vaddr % PGSIZE != 0)
    //   goto bad;
    uint64 sz1;
    // allocate and map memory for the segment into process' pagetable
    if((sz1 = uvmalloc(pagetable, sz, ph.vaddr + ph.memsz, flags2perm(ph.flags))) == 0)
      goto bad;
    sz = sz1;
    #ifdef __USE_XV6FS
    // now load segment into mapped memory in pgtble
    if(loadseg(pagetable, ph.vaddr, ip, ph.off, ph.filesz) < 0)
      goto bad;
    #else 
    if(floadseg(pagetable, f, PGROUNDDOWN(ph.vaddr), PGROUNDDOWN(ph.off), ph.filesz + (ph.vaddr - PGROUNDDOWN(ph.vaddr))) < 0)
      goto bad;
    #endif
    
  } 

  // fix size, and allocate ustack and stack guard page
  p = myproc();
  uint64 oldsz = p->sz;

  // Allocate two pages at the next page boundary.
  // Make the first inaccessible as a stack guard.
  // Use the second as the user stack.
  sz = PGROUNDUP(sz);
  uint64 sz1;
  if((sz1 = uvmalloc(pagetable, sz, sz + 32 * PGSIZE, PTE_W)) == 0)
    goto bad;
  sz = sz1;
  uvmclear(pagetable, sz - 32 * PGSIZE);
  sp = sz;
  stackbase = sp - 31 * PGSIZE;
  
  // Push argument strings, prepare rest of stack in ustack.
  for(argc = 0; argv[argc]; argc++) {
    if(argc >= MAXARG)
      goto bad;
    sp -= strlen(argv[argc]) + 1;
    sp -= sp % 16; // riscv sp must be 16-byte aligned
    if(sp < stackbase)
      goto bad;
    if(copyout(pagetable, sp, argv[argc], strlen(argv[argc]) + 1) < 0)
      goto bad;
    ustack[argc] = sp;
  }
  ustack[argc] = 0;

  // Load AUX vectors
  sp -= 16;
  uint64 aux[MAX_AT * 2];

  ADD_AUXV(AT_HWCAP, 0);
  ADD_AUXV(AT_PAGESZ, PGSIZE);
  ADD_AUXV(AT_PHDR, elf.phoff);
  ADD_AUXV(AT_PHENT, elf.phentsize);
  ADD_AUXV(AT_PHNUM, elf.phnum);
  ADD_AUXV(AT_BASE, 0);
  ADD_AUXV(AT_ENTRY, elf.entry);
  ADD_AUXV(AT_UID, 0);
  ADD_AUXV(AT_EUID, 0);
  ADD_AUXV(AT_GID, 0);
  ADD_AUXV(AT_EGID, 0);
  ADD_AUXV(AT_SECURE, 0);
  ADD_AUXV(AT_RANDOM, sp);
  ADD_AUXV(AT_NULL, 0);

  //三个向量的压栈顺序：AUX -> envp -> argv,argc
  // 1. AUX vector, 已经16字节对齐
  sp -= sizeof(aux);
  if (copyout(pagetable, sp, (char *)aux, sizeof(aux)) < 0)
      goto bad;

  // push the array of argv[] pointers.
  sp -= (argc+1) * sizeof(uint64);
  sp -= sp % 16;
  if(sp < stackbase)
    goto bad;
  if(copyout(pagetable, sp, (char *)ustack, (argc+1)*sizeof(uint64)) < 0)
    goto bad;
  
  // now free other threads
  free_allother_threads_group(t);
  // and transfer trapframe
  transfer_trapframe(t, pagetable,0);

  // arguments to user main(argc, argv)
  // argc is returned via the system call return
  // value, which goes in a0.
  t->trapframe->a1 = sp;

  // Save program name for debugging.
  for(last=s=path; *s; s++)
    if(*s == '/')
      last = s+1;
  safestrcpy(p->name, last, sizeof(p->name));
    
  // Commit to the user image.
  oldpagetable = p->mm.pagetable;
  p->mm.pagetable = pagetable;
  p->sz = sz;
  t->trapframe->epc = elf.entry;  // initial program counter = main
  t->trapframe->sp = sp; // initial stack pointer
// #ifdef __DEBUG_EXEC
//   Log("check elf.entry = %p", elf.entry);
//   walk_va(pagetable, elf.entry);
// #endif
  proc_freepagetable(oldpagetable, oldsz, 1);

  return argc; // this ends up in a0, the first argument to main(argc, argv)

 bad:
  if(pagetable)
    proc_freepagetable(pagetable, sz, 1);

  if(f)
    ext4_vfclose(f);
  return -1;

}

int execve(char *path, char **argv, char **envp)
{
  
  char *s, *last;
  int i, off;
  int r = 0, rcnt = 0, index = 0;
  uint64 argc, envc, sz = 0, sp, ustack[MAXARG], estack[MAXENV + 1], stackbase;
  struct elfhdr elf;
  struct proghdr ph;
  pagetable_t pagetable = 0, oldpagetable;
  
  struct proc *p = myproc();
  struct tcb *t = mythread();
  char abs_path[MAXPATH];
  struct file *f;

  if((f = filealloc()) == 0)
    return -1;
  get_absolute_path(path, myproc()->cinfo.path, abs_path);
#ifdef __DEBUG_EXECVE
  Log("execve abs_path: %s, path %s, cinfo.path %s", abs_path, path, myproc()->cinfo.path);
#endif
  if((r = ext4_vfopen(f, abs_path, O_RDONLY)) != EOK) {
    Log("ext4_fopen2 failed %d", r);
    return -1;
  }
  if(((r = ext4_vfread(f, 0, (uint64) &elf, 0, sizeof(elf), &rcnt)) != EOK) || 
     (rcnt != sizeof(elf))) {
    Log("ext4_fread failed %d, rcnt = %d, size of elf == %d", r, rcnt, sizeof(elf));
    ext4_vfclose(f);
    return -1;
  }
  if(elf.magic != ELF_MAGIC)
    goto bad;
  if((pagetable = proc_pagetable(p)) == 0)
    goto bad;
  
  // Load program into memory.
  for(i=0, off=elf.phoff; i<elf.phnum; i++, off+=sizeof(ph)){

    if((r = ext4_vfread(f, 0, (uint64)&ph, off, sizeof(ph), &rcnt)) != EOK
                        || rcnt != sizeof(ph))
      goto bad;

    // if(ph.type == ELF_PROG_INTERP)
    //   printf("ELF_PROG_INTERP not supported\n");
    if(ph.type != ELF_PROG_LOAD) {
      // printf("ph.type == 0x%x\n", ph.type);
      continue;
    }

    if(ph.memsz < ph.filesz)
      goto bad;
    if(ph.vaddr + ph.memsz < ph.vaddr)
      goto bad;
    // if(ph.vaddr % PGSIZE != 0)
    //   goto bad;
    uint64 sz1;
    // allocate and map memory for the segment into process' pagetable
    if((sz1 = uvmalloc(pagetable, sz, ph.vaddr + ph.memsz, flags2perm(ph.flags))) == 0)
      goto bad;
    sz = sz1;

    if(floadseg(pagetable, f, PGROUNDDOWN(ph.vaddr), PGROUNDDOWN(ph.off), ph.filesz + (ph.vaddr - PGROUNDDOWN(ph.vaddr))) < 0)
      goto bad;
  }

  ext4_vfclose(f);
  // fix size, and allocate ustack and stack guard page
  p = myproc();
  uint64 oldsz = p->sz;

  // Allocate two pages at the next page boundary.
  // Make the first inaccessible as a stack guard.
  // Use the second as the user stack.
  sz = PGROUNDUP(sz);
  uint64 sz1;
  if((sz1 = uvmalloc(pagetable, sz, sz + 32 * PGSIZE, PTE_W)) == 0)
    goto bad;
  sz = sz1;
  uvmclear(pagetable, sz - 32 * PGSIZE);
  sp = sz;
  stackbase = sp - 31 * PGSIZE;
  
  sp -= 16;
  // push environment strings
  for (envc = 0; envp[envc]; envc++)
  {
      if (envc >= MAXENV)
          goto bad;
      sp -= strlen(envp[envc]) + 1;
      sp -= sp % 16; // riscv sp must be 16-byte aligned
      if (sp < stackbase)
          goto bad;
      if (copyout(pagetable, sp, envp[envc], strlen(envp[envc]) + 1) < 0)
          goto bad;
      estack[envc] = sp;
  }
  
  estack[envc] = 0;

  // Push argument strings, prepare rest of stack in ustack.
  for(argc = 0; argv[argc]; argc++) {
    if(argc >= MAXARG)
      goto bad;
    sp -= strlen(argv[argc]) + 1;
    sp -= sp % 16; // riscv sp must be 16-byte aligned
    if(sp < stackbase)
      goto bad;
    if(copyout(pagetable, sp, argv[argc], strlen(argv[argc]) + 1) < 0)
      goto bad;
    ustack[argc] = sp;
  }
  ustack[argc] = 0;

  // Load AUX vectors
  sp -= 16;
  uint64 aux[MAX_AT * 2];

  ADD_AUXV(AT_HWCAP, 0);
  ADD_AUXV(AT_PAGESZ, PGSIZE);
  ADD_AUXV(AT_PHDR, elf.phoff);
  ADD_AUXV(AT_PHENT, elf.phentsize);
  ADD_AUXV(AT_PHNUM, elf.phnum);
  ADD_AUXV(AT_BASE, 0);
  ADD_AUXV(AT_ENTRY, elf.entry);
  ADD_AUXV(AT_UID, 0);
  ADD_AUXV(AT_EUID, 0);
  ADD_AUXV(AT_GID, 0);
  ADD_AUXV(AT_EGID, 0);
  ADD_AUXV(AT_SECURE, 0);
  ADD_AUXV(AT_RANDOM, sp);
  ADD_AUXV(AT_NULL, 0);


  //三个向量的压栈顺序：AUX -> envp -> argv,argc
  // 1. AUX vector, 已经16字节对齐
  sp -= sizeof(aux);
  if (copyout(pagetable, sp, (char *)aux, sizeof(aux)) < 0)
      goto bad;

// push the array of envp[] pointers.
  sp -= (envc + 1) * sizeof(uint64);
  // sp -= sp % 16;
  // t->trapframe->a2 = sp;
  if (sp < stackbase)
      goto bad;
  if (copyout(pagetable, sp, (char *)estack, (envc + 1) * sizeof(uint64)) < 0)
      goto bad;

  // push the array of argv[] pointers.
  sp -= (argc+1) * sizeof(uint64);
  // sp -= sp % 16;
  if(sp < stackbase)
    goto bad;
  if(copyout(pagetable, sp, (char *)ustack, (argc+1)*sizeof(uint64)) < 0)
    goto bad;
  
  // now free other threads
  free_allother_threads_group(t);
  // and transfer trapframe
  transfer_trapframe(t, pagetable,0);

  // arguments to user main(argc, argv)
  // argc is returned via the system call return
  // value, which goes in a0.
  t->trapframe->a1 = sp;

  // push the argc.
  sp -= sizeof(uint64);
  if (sp < stackbase)
      goto bad;
  if (copyout(pagetable, sp, (char *)&argc, sizeof(uint64)) < 0)
      goto bad;
      
  // Save program name for debugging.
  for(last=s=path; *s; s++)
    if(*s == '/')
      last = s+1;
  safestrcpy(p->name, last, sizeof(p->name));
    
  // Commit to the user image.
  oldpagetable = p->mm.pagetable;
  p->mm.pagetable = pagetable;
  p->sz = sz;
  // Log("exec: %s, sz = %p", p->name, sz);
  t->trapframe->epc = elf.entry;  // initial program counter = main
  t->trapframe->sp = sp; // initial stack pointer
// #ifdef __DEBUG_EXEC
//   Log("check elf.entry = %p", elf.entry);
//   walk_va(pagetable, elf.entry);
// #endif
  proc_freepagetable(oldpagetable, oldsz, 1);

  // close the file with flag O_CLOEXEC
  for(i = 0; i < NOFILE; i++) {
    if (p->ofile[i] && p->ofile[i]->flags & O_CLOEXEC) {
      fileclose(p->ofile[i]);
      p->ofile[i] = 0;
    }
  }
  return argc; // this ends up in a0, the first argument to main(argc, argv)

  bad:
  if(pagetable)
    proc_freepagetable(pagetable, sz, 1);
  if(f)
    ext4_vfclose(f);
  return -1;

}

static int floadseg(pagetable_t pagetable, struct file *f, uint64 va, uint offset, uint sz){
  #ifdef __DEBUG_FLOADSEG
  Log("enter floadseg: va %p, offset %p, sz 0x%x", va, offset, sz);
  #endif
  uint i, n;
  int r = 0, rcnt = 0;
  uint64 pa;
  if ((va % PGSIZE) != 0)
    panic("loadseg: va must be page aligned");

  for (i = 0; i < sz; i += PGSIZE)
  {
    pa = walkaddr(pagetable, va + i);
    if (pa == 0)
        panic("loadseg: address should exist");
    if (sz - i < PGSIZE)
        n = sz - i;
    else
        n = PGSIZE;
    r = ext4_vfread(f, 0, (uint64) pa, offset + i, n, &rcnt);
    if (r != EOK || rcnt != n)
        return -1;
  }

#ifdef __DEBUG_FLOADSEG
  Log("floadseg: va %p, offset %p, sz 0x%x", va, offset, sz);
#endif
  return 0;

} 