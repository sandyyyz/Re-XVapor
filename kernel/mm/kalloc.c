// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

/*
  Use an array to record the number of references to each physical page
  Because the address under KERNBASE is I/O device, so the size of the
  array is (PHYSTOP - KERNBASE) / PGSIZE
  @ref: because max process number is 64, so use uint8
*/

struct {
  uint8 ref;
  struct spinlock lock;
} memory_refs[(PHYSTOP - KERNBASE) / PGSIZE];

// increase the reference count of physical address
void increase_ref(uint64 pa) {
  if (pa < KERNBASE || pa > PHYSTOP)
    panic("increase_ref");
  pa = (pa - KERNBASE) / PGSIZE;
  acquire(&memory_refs[pa].lock);
  memory_refs[pa].ref++;
  release(&memory_refs[pa].lock);
}

// decrease the reference count of physical address
uint decrease_ref(uint64 pa) {
  if (pa < KERNBASE || pa > PHYSTOP)
    return 0;
  pa = (pa - KERNBASE) / PGSIZE;
  acquire(&memory_refs[pa].lock);
  memory_refs[pa].ref--;
  release(&memory_refs[pa].lock);
  return memory_refs[pa].ref;
}

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

void kinit() {
  initlock(&kmem.lock, "kmem");
  freerange(end, (void *)PHYSTOP);
}

void freerange(void *pa_start, void *pa_end) {
  char *p;
  p = (char *)PGROUNDUP((uint64)pa_start);
  for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE) {
    increase_ref((uint64)p);
    kfree(p);
  }
}
// junk == 1
// add to freelist
// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void kfree(void *pa) {
  struct run *r;

  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // If pysical page is not in use, decrease_ref will return 0
  if (decrease_ref((uint64)pa))
    return;

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run *)pa;

  acquire(&kmem.lock);
  // 头插
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// 1. 尝试从freelist中取
// 2. fill with junk == 5
// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *kalloc(void) {
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if (r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  increase_ref((uint64)r);
  if (r)
    memset((char *)r, 5, PGSIZE); // fill with junk
  return (void *)r;
}

/// @brief allocate one page of physical memory and fill with 0
/// @param
/// @return physical address
void *kzalloc(void) {
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if (r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if (r)
    memset((char *)r, 0, PGSIZE); // fill with junk
  return (void *)r;
}