#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "arch.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "../include/elf.h"
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

#ifdef __ARCH_RISCV
int flags2perm(int flags)
{
    int perm = PTE_A;
    if(flags & 0x1)
      perm = PTE_X;
    if(flags & 0x2)
      perm |= PTE_W;
    return perm;
}
#else
unsigned long int flags2perm(int flags) {
  unsigned long int perm = 0;
  if(!(flags & 0x1)) 
    perm = PTE_NX;
  if(flags & 0x2)
    perm |= PTE_W;
  return perm;
}

#endif
int execve(char *path, char **argv, char **envp)
{
  
  char *s, *last;
  int i, off;
  int r = 0, index = 0;
  size_t rcnt = 0;
  uint64 argc, envc, sz = 0, sp, ustack[MAXARG], estack[MAXENV + 1], stackbase;
  struct elfhdr elf;
  struct proghdr ph;
  pagetable_t pagetable = 0, oldpagetable;
  
  struct proc *p = myproc();
  struct tcb *t = mythread();
  char abs_path[MAXPATH];
  struct file *f;
  int need_dynamic = 0;
  uint64 prog_entry = 0, interp_base = 0;
  uint64 progh_base = 0;
  int first = 0;

  if((f = filealloc()) == 0)
    return -1;
  get_absolute_path(path, myproc()->cinfo.path, abs_path);
#ifdef __DEBUG_EXECVE
  Log("execve abs_path: %s, path %s, cinfo.path %s", abs_path, path, myproc()->cinfo.path);
#endif
  if((r = ext4_vfopen(f, abs_path, O_RDONLY)) != EOK) {
#ifdef __DEBUG_EXECVE
    Warn("ext4_fopen2 failed %d", r);
#endif
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

    if(ph.type == ELF_PROG_INTERP) {
#ifdef __DEBUG_EXECVE
      Warn("ELF_PROG_INTERP!");
#endif
      need_dynamic = 1;
    }
    if(ph.type != ELF_PROG_LOAD) {
#ifdef __DEBUG_EXECVE
      Log("ph.type == 0x%x", ph.type);
#endif
      continue;
    }

    if(ph.memsz < ph.filesz)
      goto bad;
    if(ph.vaddr + ph.memsz < ph.vaddr)
      goto bad;
    // if(ph.vaddr % PGSIZE != 0)
    //   goto bad;
    uint64 sz1;
#ifdef __DEBUG_EXECVE
    Log("ph.vaddr = %p, ph.memsz = 0x%x, ph.filesz = 0x%x, ph.off = %p, ph.flags = 0x%x", 
        ph.vaddr, ph.memsz, ph.filesz, ph.off, ph.flags);
#endif
    // allocate and map memory for the segment into process' pagetable
    if((sz1 = uvmalloc(pagetable, sz, ph.vaddr + ph.memsz, flags2perm(ph.flags) | PTE_W)) == 0)
      goto bad;
    sz = sz1;
#ifdef __DEBUG_EXECVE
    printf_green("ph.off = %p, PGROUNDDOWN(ph.off) = %p, ph.vaddr = %p, PGROUNDDOWN(ph.vaddr) = %p, ph.filesz = %d\n", 
            ph.off, PGROUNDDOWN(ph.off), ph.vaddr, PGROUNDDOWN(ph.vaddr), ph.filesz);
#endif
    if(floadseg(pagetable, f, PGROUNDDOWN(ph.vaddr), PGROUNDDOWN(ph.off), ph.filesz + (ph.vaddr - PGROUNDDOWN(ph.vaddr))) < 0)
      goto bad;
    if(first == 0) {
      progh_base = PGROUNDDOWN(ph.vaddr);
      first = 1; 
    }
  }

  ext4_vfclose(f);

  if (need_dynamic) {
      struct file* interp;
      struct elfhdr interp_elf;
      struct proghdr interp_ph;
      const char* interp_path = "/musl/lib/libc.so";
      // const char* interp_path = "/lib/ld-musl-riscv64.so.1";

      interp_base = PGROUNDUP(sz);
      if((interp = filealloc()) == 0) {
          Warn("filealloc failed for dynamic linker");
          goto bad;
      }
      // printf("[execve] dynamic link start, interp_base: %p\n", interp_base);

      if((r = ext4_vfopen(interp, interp_path, O_RDONLY)) != EOK) {
          Warn("ext4_vfopen2 failed %d", r);
          goto bad;
      }

      r = ext4_vfread(interp, 0, (uint64)&interp_elf, 0, sizeof(struct elfhdr), &rcnt);
      if (r != EOK || rcnt != sizeof(struct elfhdr))
          goto bad;
      if (interp_elf.magic != ELF_MAGIC) {
        Warn("ELF magic not match for dynamic linker");
        ext4_vfclose(interp);
        interp = NULL;
        goto bad;
      }
      for (i = 0, off = interp_elf.phoff; i < interp_elf.phnum; i++, off += sizeof(struct proghdr))
      {
          r = ext4_vfread(interp, 0, (uint64)&interp_ph, off, sizeof(struct proghdr), &rcnt);
          if (r != EOK || rcnt != sizeof(struct proghdr))
              goto bad;
          if (interp_ph.type != ELF_PROG_LOAD)
              continue;
          if (interp_ph.memsz < interp_ph.filesz)
              goto bad;
          if (interp_ph.vaddr + interp_ph.memsz < interp_ph.vaddr)
              goto bad;
#ifdef __DEBUG_EXECVE
          Log("interp_ph.vaddr = %p, interp_ph.memsz = 0x%x, interp_ph.filesz = 0x%x, interp_ph.off = %p, interp_ph.flags = 0x%x", 
              interp_ph.vaddr, interp_ph.memsz, interp_ph.filesz, interp_ph.off, interp_ph.flags);
#endif
          uint64 sz1;
#ifdef __ARCH_RISCV
          if ((sz1 = uvmalloc(pagetable, sz, interp_base + interp_ph.vaddr + interp_ph.memsz, PTE_W | PTE_X)) == 0)
              goto bad;
#else
          if ((sz1 = uvmalloc(pagetable, sz, interp_base + interp_ph.vaddr + interp_ph.memsz, PTE_W)) == 0)
            goto bad;
#endif
          sz = sz1;

          uint margin_size = 0;
          if ((interp_ph.vaddr % PGSIZE) != 0)
          {
              margin_size = interp_ph.vaddr % PGSIZE;
          }
          if (floadseg(pagetable, interp, interp_base + PGROUNDDOWN(interp_ph.vaddr), PGROUNDDOWN(interp_ph.off), interp_ph.filesz + margin_size) < 0)
              goto bad;
      }
      ext4_vfclose(interp);
      prog_entry = interp_base + interp_elf.entry;
#ifdef __DEBUG_EXECVE
      Warn("dynamic linker base = %p, intep_elf.entry = %p", interp_base, interp_elf.entry);
#endif
  } else
      prog_entry = elf.entry;
#ifdef __DEBUG_EXECVE
  Warn("execve: prog_entry = %p, elf_entry = %p, need_dynamic = %d", prog_entry, elf.entry, need_dynamic);
#endif
  // fix size, and allocate ustack and stack guard page
  p = myproc();
  uint64 oldsz = p->sz;

  // Allocate two pages at the next page boundary.
  // Make the first inaccessible as a stack guard.
  // Use the second as the user stack.

  uint64 sz1;
  sz = PGROUNDUP(sz);

  // Log("uvmalloc sz = %p, sz + 64 * PGSIZE = %p", sz, sz + 64 * PGSIZE);
  if((sz1 = uvmalloc(pagetable, sz, sz + 64 * PGSIZE, PTE_W)) == 0)
    goto bad;
  sz = sz1;
  uvmclear(pagetable, sz - 64 * PGSIZE);
  sp = sz;
  stackbase = sp - 63 * PGSIZE;
  
  // if((sz1 = map_ustack(pagetable, sz, 63)) == 0)
  //     goto bad;
  // sp = sz1;
  // stackbase = sp - 63 * PGSIZE;

  // Log("execve: sz = %p, sp = %p, stackbase = %p", sz, sp, stackbase);
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
  // int fd = generic_open(abs_path, O_RDONLY, 0);
  // if(fd < 0) {
  //   Log("execve: generic_open failed for %s, fd = %d", abs_path, fd);
  //   goto bad;
  // } else {
  //   Log("execve: generic_open success for %s, fd = %d", abs_path, fd);
  // }
  // ADD_AUXV(AT_HWCAP, 0);
  ADD_AUXV(AT_PAGESZ, PGSIZE);
  ADD_AUXV(AT_PHDR, elf.phoff + progh_base);
  ADD_AUXV(AT_PHENT, elf.phentsize);
  ADD_AUXV(AT_PHNUM, elf.phnum);
  ADD_AUXV(AT_BASE, need_dynamic ? interp_base : 0);
  ADD_AUXV(AT_ENTRY, elf.entry);
  ADD_AUXV(AT_UID, 0);
  ADD_AUXV(AT_EUID, 0);
  ADD_AUXV(AT_GID, 0);
  ADD_AUXV(AT_EGID, 0);
  ADD_AUXV(AT_SECURE, 0);
  ADD_AUXV(AT_RANDOM, sp);
  // ADD_AUXV(AT_EXECFN, fd);
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
#ifdef __ARCH_RISCV
  t->trapframe->epc = prog_entry;
#else
  t->trapframe->era = prog_entry;
#endif
  t->trapframe->sp = sp; // initial stack pointer
// #ifdef __DEBUG_EXEC
//   Log("check elf.entry = %p", elf.entry);
//   walk_va(pagetable, elf.entry);
// #endif
  proc_freepagetable(oldpagetable, oldsz, 1);

  // close the file with flag O_CLOEXEC
  for(i = 0; i < NOFILE; i++) {
    if (p->ofile[i] && p->ofile[i]->flags & O_CLOEXEC) {
      fileclose(p->ofile[i], 1);
      p->ofile[i] = 0;
    }
  }

  // return argc; // this ends up in a0, the first argument to main(argc, argv)
  
  // start.S in glibc expects a0 to store the address of the dynamic linker destructor
  // NULL now 
  return 0; 

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
  int r = 0;
  size_t rcnt = 0;
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