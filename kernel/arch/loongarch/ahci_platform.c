#include "ahci_platform.h"
#include "libahci.h"
#include "types.h"
#include "defs.h"
#include "memlayout.h"

static uint8_t ahci_malloc_area[4096]
  __attribute__((aligned(4096)));

void ahci_mdelay(uint32_t ms) {
  while (ms--) {
    for (volatile int i = 0; i < 100000; i++) {
      // busy wait
    }
  }
}

int ahci_printf(const char *fmt, ...) {
  printf(fmt);
}

void *ahci_memset(void *s, int c, uint64_t count) {
  memset(s, c, count);
}

void *ahci_memcpy(void *dest, const void *src, uint64_t n) {
  memcpy(dest, src, n);
}

uint64_t ahci_malloc_align(uint64_t size, uint32_t align) {
  ahci_printf("ahci_malloc_area: %p\n", ahci_malloc_area);
  return (uint64_t)ahci_malloc_area;
}

// sync all dcache data
void ahci_sync_dcache() {
  asm volatile ("dbar 0":::"memory");
}

uint64_t ahci_phys_to_uncached(uint64_t va) {
  return va | CSR_DMW0_BASE;
}

// convert virtual address to physical address
// ahci sata can accept 64bit dma address
uint64_t ahci_virt_to_phys(uint64_t va) {
  return va & (~CSR_DMW0_BASE) & (~CSR_DMW1_BASE);
}
