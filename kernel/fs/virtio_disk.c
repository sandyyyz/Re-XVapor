//
// driver for qemu's virtio disk device.
// uses qemu's mmio interface to virtio.
//
// qemu ... -drive file=fs.img,if=none,format=raw,id=x0 -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0
//

#ifdef __LEGACY
#include "types.h"
#include "arch.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "xv6fs.h"
#include "buf.h"
#include "virtio.h"
#include "debug.h"
#include "thread.h"

// the address of virtio mmio register r.
#define R(r) ((volatile uint32 *)(VIRTIO0 + (r)))

static struct disk {
  // a set (not a ring) of DMA descriptors, with which the
  // driver tells the device where to read and write individual
  // disk operations. there are NUM descriptors.
  // most commands consist of a "chain" (a linked list) of a couple of
  // these descriptors.
  struct virtq_desc *desc;

  // a ring in which the driver writes descriptor numbers
  // that the driver would like the device to process.  it only
  // includes the head descriptor of each chain. the ring has
  // NUM elements.
  struct virtq_avail *avail;

  // a ring in which the device writes descriptor numbers that
  // the device has finished processing (just the head of each chain).
  // there are NUM used ring entries.
  struct virtq_used *used;

  // our own book-keeping.
  char free[NUM];  // is a descriptor free?
  uint16 used_idx; // we've looked this far in used[2..NUM].

  // track info about in-flight operations,
  // for use when completion interrupt arrives.
  // indexed by first descriptor index of chain.
  struct {
    struct buf *b;
    char status;
  } info[NUM];

  // disk command headers.
  // one-for-one with descriptors, for convenience.
  struct virtio_blk_req ops[NUM];
  
  struct spinlock vdisk_lock;
  
} disk;

void
virtio_disk_init(void)
{
  uint32 status = 0;

  initlock(&disk.vdisk_lock, "virtio_disk");

  if(*R(VIRTIO_MMIO_MAGIC_VALUE) != 0x74726976 ||
     *R(VIRTIO_MMIO_VERSION) != 2 ||
     *R(VIRTIO_MMIO_DEVICE_ID) != 2 ||
     *R(VIRTIO_MMIO_VENDOR_ID) != 0x554d4551){
    panic("could not find virtio disk");
  }
  
  // reset device
  *R(VIRTIO_MMIO_STATUS) = status;

  // set ACKNOWLEDGE status bit
  status |= VIRTIO_CONFIG_S_ACKNOWLEDGE;
  *R(VIRTIO_MMIO_STATUS) = status;

  // set DRIVER status bit
  status |= VIRTIO_CONFIG_S_DRIVER;
  *R(VIRTIO_MMIO_STATUS) = status;

  // negotiate features
  uint64 features = *R(VIRTIO_MMIO_DEVICE_FEATURES);
  features &= ~(1 << VIRTIO_BLK_F_RO);
  features &= ~(1 << VIRTIO_BLK_F_SCSI);
  features &= ~(1 << VIRTIO_BLK_F_CONFIG_WCE);
  features &= ~(1 << VIRTIO_BLK_F_MQ);
  features &= ~(1 << VIRTIO_F_ANY_LAYOUT);
  features &= ~(1 << VIRTIO_RING_F_EVENT_IDX);
  features &= ~(1 << VIRTIO_RING_F_INDIRECT_DESC);
  *R(VIRTIO_MMIO_DRIVER_FEATURES) = features;

  // tell device that feature negotiation is complete.
  status |= VIRTIO_CONFIG_S_FEATURES_OK;
  *R(VIRTIO_MMIO_STATUS) = status;

  // re-read status to ensure FEATURES_OK is set.
  status = *R(VIRTIO_MMIO_STATUS);
  if(!(status & VIRTIO_CONFIG_S_FEATURES_OK))
    panic("virtio disk FEATURES_OK unset");

  // initialize queue 0.
  *R(VIRTIO_MMIO_QUEUE_SEL) = 0;

  // ensure queue 0 is not in use.
  if(*R(VIRTIO_MMIO_QUEUE_READY))
    panic("virtio disk should not be ready");

  // check maximum queue size.
  uint32 max = *R(VIRTIO_MMIO_QUEUE_NUM_MAX);
  if(max == 0)
    panic("virtio disk has no queue 0");
  if(max < NUM)
    panic("virtio disk max queue too short");

  // allocate and zero queue memory.
  disk.desc = kalloc();
  disk.avail = kalloc();
  disk.used = kalloc();
  if(!disk.desc || !disk.avail || !disk.used)
    panic("virtio disk kalloc");
  memset(disk.desc, 0, PGSIZE);
  memset(disk.avail, 0, PGSIZE);
  memset(disk.used, 0, PGSIZE);

  // set queue size.
  *R(VIRTIO_MMIO_QUEUE_NUM) = NUM;

  // write physical addresses.
  *R(VIRTIO_MMIO_QUEUE_DESC_LOW) = (uint64)disk.desc;
  *R(VIRTIO_MMIO_QUEUE_DESC_HIGH) = (uint64)disk.desc >> 32;
  *R(VIRTIO_MMIO_DRIVER_DESC_LOW) = (uint64)disk.avail;
  *R(VIRTIO_MMIO_DRIVER_DESC_HIGH) = (uint64)disk.avail >> 32;
  *R(VIRTIO_MMIO_DEVICE_DESC_LOW) = (uint64)disk.used;
  *R(VIRTIO_MMIO_DEVICE_DESC_HIGH) = (uint64)disk.used >> 32;

  // queue is ready.
  *R(VIRTIO_MMIO_QUEUE_READY) = 0x1;

  // all NUM descriptors start out unused.
  for(int i = 0; i < NUM; i++)
    disk.free[i] = 1;

  // tell device we're completely ready.
  status |= VIRTIO_CONFIG_S_DRIVER_OK;
  *R(VIRTIO_MMIO_STATUS) = status;

  // plic.c and trap.c arrange for interrupts from VIRTIO0_IRQ.
}

// find a free descriptor, mark it non-free, return its index.
static int
alloc_desc()
{
  for(int i = 0; i < NUM; i++){
    if(disk.free[i]){
      disk.free[i] = 0;
      return i;
    }
  }
  return -1;
}

// mark a descriptor as free.
static void
free_desc(int i)
{
  if(i >= NUM)
    panic("free_desc 1");
  if(disk.free[i])
    panic("free_desc 2");
  disk.desc[i].addr = 0;
  disk.desc[i].len = 0;
  disk.desc[i].flags = 0;
  disk.desc[i].next = 0;
  disk.free[i] = 1;

  thread_wakeup_chan(&disk.free[0]);
#ifdef __DEBUG_FREEDESC
  Log("wakeup chan %p", (void*)&disk.free[0]);
#endif
}

// free a chain of descriptors.
static void
free_chain(int i)
{
  while(1){
    int flag = disk.desc[i].flags;
    int nxt = disk.desc[i].next;
    free_desc(i);
    if(flag & VRING_DESC_F_NEXT)
      i = nxt;
    else
      break;
  }
}

// allocate three descriptors (they need not be contiguous).
// disk transfers always use three descriptors.
static int
alloc3_desc(int *idx)
{
  for(int i = 0; i < 3; i++){
    idx[i] = alloc_desc();
    if(idx[i] < 0){
      for(int j = 0; j < i; j++)
        free_desc(idx[j]);
      return -1;
    }
  }
  return 0;
}

// 0,1 for read or write
void
virtio_disk_rw(struct buf *b, int write)
{

  uint64 sector = b->blockno * (BSIZE / 512);
  acquire(&disk.vdisk_lock);
  // the spec's Section 5.2 says that legacy block operations use
  // three descriptors: one for type/reserved/sector, one for the
  // data, one for a 1-byte status result.

  // allocate the three descriptors.
  int idx[3];  
#ifdef __DEBUG_VDISKRW
  Log("reach sleep point %p",&disk.free[0]);
#endif
  while(1){
    if(alloc3_desc(idx) == 0) {
    break;
    }
    thread_sleep(&disk.free[0], &disk.vdisk_lock, NULL);
  }
#ifdef __DEBUG_VDISKRW
  Log("pass sleep point %p",&disk.vdisk_lock);
#endif

  // format the three descriptors.
  // qemu's virtio-blk.c reads them.

  struct virtio_blk_req *buf0 = &disk.ops[idx[0]];

  if(write)
    buf0->type = VIRTIO_BLK_T_OUT; // write the disk
  else
    buf0->type = VIRTIO_BLK_T_IN; // read the disk
  buf0->reserved = 0;
  buf0->sector = sector;

  disk.desc[idx[0]].addr = (uint64) buf0;
  disk.desc[idx[0]].len = sizeof(struct virtio_blk_req);
  disk.desc[idx[0]].flags = VRING_DESC_F_NEXT;
  disk.desc[idx[0]].next = idx[1];

  disk.desc[idx[1]].addr = (uint64) b->data;
  disk.desc[idx[1]].len = BSIZE;
  if(write)
    disk.desc[idx[1]].flags = 0; // device reads b->data
  else
    disk.desc[idx[1]].flags = VRING_DESC_F_WRITE; // device writes b->data
  disk.desc[idx[1]].flags |= VRING_DESC_F_NEXT;
  disk.desc[idx[1]].next = idx[2];

  disk.info[idx[0]].status = 0xff; // device writes 0 on success
  disk.desc[idx[2]].addr = (uint64) &disk.info[idx[0]].status;
  disk.desc[idx[2]].len = 1;
  disk.desc[idx[2]].flags = VRING_DESC_F_WRITE; // device writes the status
  disk.desc[idx[2]].next = 0;

  // record struct buf for virtio_disk_intr().
  b->disk = 1;
  disk.info[idx[0]].b = b;

  // tell the device the first index in our chain of descriptors.
  disk.avail->ring[disk.avail->idx % NUM] = idx[0];

  // tell the device another avail ring entry is available.
  disk.avail->idx += 1; // not % NUM ...



  __sync_synchronize();

  *R(VIRTIO_MMIO_QUEUE_NOTIFY) = 0; // value is queue number

  // Wait for virtio_disk_intr() to say request has finished.
#ifdef __DEBUG_VDISKRW
  Log("reach sleep point %p",(void*)b);
#endif
  while(b->disk == 1) {
    thread_sleep(b, &disk.vdisk_lock, NULL);
  }
#ifdef __DEBUG_VDISKRW
  Log("pass sleep point %p",(void*)b);
#endif
  disk.info[idx[0]].b = 0;
  free_chain(idx[0]);
  release(&disk.vdisk_lock);

}

void
virtio_disk_intr()
{
  acquire(&disk.vdisk_lock);

  // the device won't raise another interrupt until we tell it
  // we've seen this interrupt, which the following line does.
  // this may race with the device writing new entries to
  // the "used" ring, in which case we may process the new
  // completion entries in this interrupt, and have nothing to do
  // in the next interrupt, which is harmless.
  *R(VIRTIO_MMIO_INTERRUPT_ACK) = *R(VIRTIO_MMIO_INTERRUPT_STATUS) & 0x3;

  __sync_synchronize();

  // the device increments disk.used->idx when it
  // adds an entry to the used ring.

  while(disk.used_idx != disk.used->idx){
    __sync_synchronize();
    int id = disk.used->ring[disk.used_idx % NUM].id;

    if(disk.info[id].status != 0)
      panic("virtio_disk_intr status");

    struct buf *b = disk.info[id].b;
    b->disk = 0;   // disk is done with buf
#ifdef __DEBUG_DISK_INTR
    // Log("thread_wakeup_chan buf 0x%x begin", b);
#endif
    thread_wakeup_chan(b);
#ifdef __DEBUG_DISK_INTR
    Log("thread_wakeup_chan %p end",(void*)b);
#endif
    disk.used_idx += 1;
  }

  release(&disk.vdisk_lock);
}

#else
// Legacy MMIO-based virtio disk driver for QEMU
// Compatible with virtio 1.0 legacy interface (MMIO version 1)

// #include "types.h"
// #include "arch.h"
// #include "defs.h"
// #include "param.h"
// #include "memlayout.h"
// #include "spinlock.h"
// #include "sleeplock.h"
// #include "xv6fs.h"
// #include "buf.h"
// #include "virtio.h"
// #include "debug.h"
// #include "thread.h"

// #define R(r) ((volatile uint32 *)(VIRTIO0 + (r)))

// static struct disk {
//   struct virtq_desc *desc;
//   struct virtq_avail *avail;
//   struct virtq_used *used;
//   char free[NUM];
//   uint16 used_idx;
//   struct {
//     struct buf *b;
//     char status;
//   } info[NUM];
//   struct virtio_blk_req ops[NUM];
//   struct spinlock vdisk_lock;
// } disk;

// void
// virtio_disk_init(void)
// {
//   uint32 status = 0;

//   initlock(&disk.vdisk_lock, "virtio_disk");

//   if(*R(VIRTIO_MMIO_MAGIC_VALUE) != 0x74726976 ||
//      *R(VIRTIO_MMIO_VERSION) != 1 ||
//      *R(VIRTIO_MMIO_DEVICE_ID) != 2 ||
//      *R(VIRTIO_MMIO_VENDOR_ID) != 0x554d4551){
//     panic("could not find legacy virtio disk");
//   }

//   *R(VIRTIO_MMIO_STATUS) = status;
//   status |= VIRTIO_CONFIG_S_ACKNOWLEDGE;
//   *R(VIRTIO_MMIO_STATUS) = status;
//   status |= VIRTIO_CONFIG_S_DRIVER;
//   *R(VIRTIO_MMIO_STATUS) = status;

//   uint64 features = *R(VIRTIO_MMIO_DEVICE_FEATURES);
//   features &= ~(1 << VIRTIO_BLK_F_RO);
//   features &= ~(1 << VIRTIO_BLK_F_SCSI);
//   features &= ~(1 << VIRTIO_BLK_F_CONFIG_WCE);
//   features &= ~(1 << VIRTIO_BLK_F_MQ);
//   *R(VIRTIO_MMIO_DRIVER_FEATURES) = features;

//   status |= VIRTIO_CONFIG_S_FEATURES_OK;
//   *R(VIRTIO_MMIO_STATUS) = status;

//   status = *R(VIRTIO_MMIO_STATUS);
//   if(!(status & VIRTIO_CONFIG_S_FEATURES_OK))
//     panic("virtio FEATURES_OK unset");

//   *R(VIRTIO_MMIO_QUEUE_SEL) = 0;
//   if(*R(VIRTIO_MMIO_QUEUE_READY))
//     panic("virtio disk should not be ready");
//   if(*R(VIRTIO_MMIO_QUEUE_NUM_MAX) < NUM)
//     panic("virtio NUM too big");
//   *R(VIRTIO_MMIO_QUEUE_NUM) = NUM;

//   char *pages = kalloc();
//   memset(pages, 0, 3*PGSIZE);
//   disk.desc = (struct virtq_desc *) pages;
//   disk.avail = (struct virtq_avail *) (pages + PGSIZE);
//   disk.used = (struct virtq_used *) (pages + 2*PGSIZE);

//   uint64 pa = (uint64)pages;
//   *R(VIRTIO_MMIO_QUEUE_PFN) = pa >> 12;

//   for(int i = 0; i < NUM; i++)
//     disk.free[i] = 1;

//   status |= VIRTIO_CONFIG_S_DRIVER_OK;
//   *R(VIRTIO_MMIO_STATUS) = status;
// }

// static int alloc_desc() {
//   for(int i = 0; i < NUM; i++) {
//     if(disk.free[i]) {
//       disk.free[i] = 0;
//       return i;
//     }
//   }
//   return -1;
// }

// static void free_desc(int i) {
//   if(i >= NUM || disk.free[i])
//     panic("free_desc");
//   disk.desc[i].addr = 0;
//   disk.desc[i].len = 0;
//   disk.desc[i].flags = 0;
//   disk.desc[i].next = 0;
//   disk.free[i] = 1;
//   thread_wakeup_chan(&disk.free[0]);
// }

// static void free_chain(int i) {
//   while(1) {
//     int flag = disk.desc[i].flags;
//     int nxt = disk.desc[i].next;
//     free_desc(i);
//     if(flag & VRING_DESC_F_NEXT)
//       i = nxt;
//     else break;
//   }
// }

// static int alloc3_desc(int *idx) {
//   for(int i = 0; i < 3; i++) {
//     idx[i] = alloc_desc();
//     if(idx[i] < 0) {
//       for(int j = 0; j < i; j++)
//         free_desc(idx[j]);
//       return -1;
//     }
//   }
//   return 0;
// }

// void virtio_disk_rw(struct buf *b, int write) {
//   uint64 sector = b->blockno * (BSIZE / 512);
//   acquire(&disk.vdisk_lock);
//   int idx[3];
//   while(1) {
//     if(alloc3_desc(idx) == 0)
//       break;
//     thread_sleep(&disk.free[0], &disk.vdisk_lock, NULL);
//   }

//   struct virtio_blk_req *buf0 = &disk.ops[idx[0]];
//   buf0->type = write ? VIRTIO_BLK_T_OUT : VIRTIO_BLK_T_IN;
//   buf0->reserved = 0;
//   buf0->sector = sector;

//   disk.desc[idx[0]].addr = (uint64) buf0;
//   disk.desc[idx[0]].len = sizeof(struct virtio_blk_req);
//   disk.desc[idx[0]].flags = VRING_DESC_F_NEXT;
//   disk.desc[idx[0]].next = idx[1];

//   disk.desc[idx[1]].addr = (uint64) b->data;
//   disk.desc[idx[1]].len = BSIZE;
//   disk.desc[idx[1]].flags = (write ? 0 : VRING_DESC_F_WRITE) | VRING_DESC_F_NEXT;
//   disk.desc[idx[1]].next = idx[2];

//   disk.info[idx[0]].status = 0xff;
//   disk.desc[idx[2]].addr = (uint64)&disk.info[idx[0]].status;
//   disk.desc[idx[2]].len = 1;
//   disk.desc[idx[2]].flags = VRING_DESC_F_WRITE;
//   disk.desc[idx[2]].next = 0;

//   b->disk = 1;
//   disk.info[idx[0]].b = b;

//   disk.avail->ring[disk.avail->idx % NUM] = idx[0];
//   __sync_synchronize();
//   disk.avail->idx += 1;

//   __sync_synchronize();
//   *R(VIRTIO_MMIO_QUEUE_NOTIFY) = 0;

//   while(b->disk == 1)
//     thread_sleep(b, &disk.vdisk_lock, NULL);

//   disk.info[idx[0]].b = 0;
//   free_chain(idx[0]);
//   release(&disk.vdisk_lock);
// }

// void virtio_disk_intr() {
//   acquire(&disk.vdisk_lock);
//   *R(VIRTIO_MMIO_INTERRUPT_ACK) = *R(VIRTIO_MMIO_INTERRUPT_STATUS) & 0x3;
//   __sync_synchronize();

//   while(disk.used_idx != disk.used->idx) {
//     __sync_synchronize();
//     int id = disk.used->ring[disk.used_idx % NUM].id;
//     if(disk.info[id].status != 0)
//       panic("virtio_disk_intr status");
//     struct buf *b = disk.info[id].b;
//     b->disk = 0;
//     thread_wakeup_chan(b);
//     disk.used_idx += 1;
//   }
//   release(&disk.vdisk_lock);
// }

#endif

//
// driver for qemu's virtio disk device.
// uses qemu's mmio interface to virtio.
//
// qemu ... -drive file=fs.img,if=none,format=raw,id=x0 -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0
//

#include "types.h"
#include "arch.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "virtio.h"
#include "buf.h"
#include "memlayout.h"

// the address of virtio mmio register r.
#define R(r) ((volatile uint32 *)(VIRTIO0 + (r)))

static struct disk
{
    char pages[2 * PGSIZE];

    // a set (not a ring) of DMA descriptors, with which the
    // driver tells the device where to read and write individual
    // disk operations. there are NUM descriptors.
    // most commands consist of a "chain" (a linked list) of a couple of
    // these descriptors.
    struct virtq_desc *desc;

    // a ring in which the driver writes descriptor numbers
    // that the driver would like the device to process.  it only
    // includes the head descriptor of each chain. the ring has
    // NUM elements.
    struct virtq_avail *avail;

    // a ring in which the device writes descriptor numbers that
    // the device has finished processing (just the head of each chain).
    // there are NUM used ring entries.
    struct virtq_used *used;

    // our own book-keeping.
    char free[NUM];  // is a descriptor free?
    uint16 used_idx; // we've looked this far in used[2..NUM].

    // track info about in-flight operations,
    // for use when completion interrupt arrives.
    // indexed by first descriptor index of chain.
    struct
    {
        struct buf *b;
        char status;
        int idx1; // first index of a chain
    } info[NUM];

    // disk command headers.
    // one-for-one with descriptors, for convenience.
    struct virtio_blk_req ops[NUM];

    struct spinlock vdisk_lock;

} __attribute__((aligned(PGSIZE))) disk;

void virtio_disk_init(void)
{
    uint32 status = 0;

    initlock(&disk.vdisk_lock, "virtio_disk");

    // if(*R(VIRTIO_MMIO_MAGIC_VALUE) != 0x74726976 ||
    //    *R(VIRTIO_MMIO_VERSION) != 2 ||
    //    *R(VIRTIO_MMIO_DEVICE_ID) != 2 ||
    //    *R(VIRTIO_MMIO_VENDOR_ID) != 0x554d4551){
    //   panic("could not find virtio disk");
    // }

    // // reset device
    // *R(VIRTIO_MMIO_STATUS) = status;

    // set ACKNOWLEDGE status bit
    status |= VIRTIO_CONFIG_S_ACKNOWLEDGE;
    *R(VIRTIO_MMIO_STATUS) = status;

    // set DRIVER status bit
    status |= VIRTIO_CONFIG_S_DRIVER;
    *R(VIRTIO_MMIO_STATUS) = status;

    // negotiate features
    uint64 features = *R(VIRTIO_MMIO_DEVICE_FEATURES);
    features &= ~(1 << VIRTIO_BLK_F_RO);
    features &= ~(1 << VIRTIO_BLK_F_SCSI);
    features &= ~(1 << VIRTIO_BLK_F_CONFIG_WCE);
    features &= ~(1 << VIRTIO_BLK_F_MQ);
    features &= ~(1 << VIRTIO_F_ANY_LAYOUT);
    features &= ~(1 << VIRTIO_RING_F_EVENT_IDX);
    features &= ~(1 << VIRTIO_RING_F_INDIRECT_DESC);
    *R(VIRTIO_MMIO_DRIVER_FEATURES) = features;

    // tell device that feature negotiation is complete.
    status |= VIRTIO_CONFIG_S_FEATURES_OK;
    *R(VIRTIO_MMIO_STATUS) = status;

    // re-read status to ensure FEATURES_OK is set.
    // status = *R(VIRTIO_MMIO_STATUS);
    // if(!(status & VIRTIO_CONFIG_S_FEATURES_OK))
    //   panic("virtio disk FEATURES_OK unset");

    // tell device we're completely ready.
    status |= VIRTIO_CONFIG_S_DRIVER_OK;
    *R(VIRTIO_MMIO_STATUS) = status;

    *R(VIRTIO_MMIO_GUEST_PAGE_SIZE) = PGSIZE;

    // initialize queue 0.
    *R(VIRTIO_MMIO_QUEUE_SEL) = 0;

    // ensure queue 0 is not in use.
    // if(*R(VIRTIO_MMIO_QUEUE_READY))
    //   panic("virtio disk should not be ready");

    // check maximum queue size.
    uint32 max = *R(VIRTIO_MMIO_QUEUE_NUM_MAX);
    if (max == 0)
        panic("virtio disk has no queue 0");
    if (max < NUM)
        panic("virtio disk max queue too short");

    // allocate and zero queue memory.
    // disk.desc = kalloc();
    // disk.avail = kalloc();
    // disk.used = kalloc();
    // if(!disk.desc || !disk.avail || !disk.used)
    //   panic("virtio disk kalloc");
    // memset(disk.desc, 0, PGSIZE);
    // memset(disk.avail, 0, PGSIZE);
    // memset(disk.used, 0, PGSIZE);

    // set queue size.
    *R(VIRTIO_MMIO_QUEUE_NUM) = NUM;
    memset(disk.pages, 0, sizeof(disk.pages));
    *R(VIRTIO_MMIO_QUEUE_PFN) = ((uint64)disk.pages) >> PGSHIFT;

    // write physical addresses.
    // *R(VIRTIO_MMIO_QUEUE_DESC_LOW) = (uint64)disk.desc;
    // *R(VIRTIO_MMIO_QUEUE_DESC_HIGH) = (uint64)disk.desc >> 32;
    // *R(VIRTIO_MMIO_DRIVER_DESC_LOW) = (uint64)disk.avail;
    // *R(VIRTIO_MMIO_DRIVER_DESC_HIGH) = (uint64)disk.avail >> 32;
    // *R(VIRTIO_MMIO_DEVICE_DESC_LOW) = (uint64)disk.used;
    // *R(VIRTIO_MMIO_DEVICE_DESC_HIGH) = (uint64)disk.used >> 32;

    // queue is ready.
    // *R(VIRTIO_MMIO_QUEUE_READY) = 0x1;
    disk.desc = (struct virtq_desc *)disk.pages;
    disk.avail = (struct virtq_avail *)(disk.pages + NUM * sizeof(struct virtq_desc));
    disk.used = (struct virtq_used *)(disk.pages + PGSIZE);

    // all NUM descriptors start out unused.
    for (int i = 0; i < NUM; i++)
        disk.free[i] = 1;

    // tell device we're completely ready.
    // status |= VIRTIO_CONFIG_S_DRIVER_OK;
    // *R(VIRTIO_MMIO_STATUS) = status;

    // plic.c and trap.c arrange for interrupts from VIRTIO0_IRQ.
}

// find a free descriptor, mark it non-free, return its index.
static int
alloc_desc()
{
    for (int i = 0; i < NUM; i++)
    {
        if (disk.free[i])
        {
            disk.free[i] = 0;
            return i;
        }
    }
    return -1;
}

// mark a descriptor as free.
static void
free_desc(int i)
{
    if (i >= NUM)
        panic("free_desc 1");
    if (disk.free[i])
        panic("free_desc 2");
    disk.desc[i].addr = 0;
    disk.desc[i].len = 0;
    disk.desc[i].flags = 0;
    disk.desc[i].next = 0;
    disk.free[i] = 1;
    thread_wakeup_chan(&disk.free[0]);
}

// free a chain of descriptors.
static void
free_chain(int i)
{
    while (1)
    {
        int flag = disk.desc[i].flags;
        int nxt = disk.desc[i].next;
        free_desc(i);
        if (flag & VRING_DESC_F_NEXT)
            i = nxt;
        else
            break;
    }
}

// allocate three descriptors (they need not be contiguous).
// disk transfers always use three descriptors.
static int
alloc_descs(int *restrict idx, int n)
{
    for (int i = 0; i < n; i++)
    {
        idx[i] = alloc_desc();
        if (idx[i] < 0)
        {
            for (int j = 0; j < i; j++)
                free_desc(idx[j]);
            return -1;
        }
    }
    return 0;
}

static int
virtio_disk_rw_multiple(struct buf *restrict bufs[], int nbuf, int write)
{
    // uint64 sector = b->blockno * (BSIZE / 512);
    int ndesc = nbuf + 2;
    if (ndesc > NUM)
        return -1;

    acquire(&disk.vdisk_lock);

    // the spec's Section 5.2 says that legacy block operations use
    // three descriptors: one for type/reserved/sector, one for the
    // data, one for a 1-byte status result.

    // allocate the three descriptors.
    int idx[NUM];
    while (1)
    {
        if (alloc_descs(idx, ndesc) == 0)
            break;
        if (write)
        { // write don't wait
            release(&disk.vdisk_lock);
            return -1;
        }
        thread_sleep(&disk.free[0], &disk.vdisk_lock, 0);
    }

    // format the three descriptors.
    // qemu's virtio-blk.c reads them.

    struct virtio_blk_req *buf0 = &disk.ops[idx[0]];
    struct virtq_desc *desc;

// if(write)
    //   buf0->type = VIRTIO_BLK_T_OUT; // write the disk
    // else
    //   buf0->type = VIRTIO_BLK_T_IN; // read the disk
    buf0->type = write ? VIRTIO_BLK_T_OUT : // write the disk
                         VIRTIO_BLK_T_IN;       // read the disk
    buf0->sector = bufs[0]->blockno * (BSIZE / 512);

    // first
    desc = &disk.desc[idx[0]];
    desc->addr = (uint64)buf0;
    desc->len = sizeof(struct virtio_blk_req);
    desc->flags = VRING_DESC_F_NEXT;
    desc->next = idx[1];

    for (int i = 1; i <= nbuf; i++)
    {
        desc = &disk.desc[idx[i]];
        desc->addr = (uint64)bufs[i - 1]->data;
        desc->len = BSIZE;
        desc->flags = write ? 0 :             // device reads b->data
                              VRING_DESC_F_WRITE; // device writes b->data
        desc->flags |= VRING_DESC_F_NEXT;
        desc->next = idx[i + 1];
    }

    // last
    desc = &disk.desc[idx[ndesc - 1]];
    desc->addr = (uint64)&disk.info[idx[0]].status;
    desc->len = 1;
    desc->flags = VRING_DESC_F_WRITE; // device writes the status
    desc->next = 0;

    /**
     * Record struct buf for virtio_disk_intr().
     * If we are committing a wirte, we are not going to wait here.
     * After the write is done, the interrupt handler will use idx1
     * to free the desc chain that we allocated. In other way, this
     * is also a hint that this is a write buf, the handler needs to
     * do something about it.
     */
    disk.info[idx[0]].idx1 = write ? idx[0] : -1;
    disk.info[idx[0]].status = 0xff; // device writes 0 on success
    disk.info[idx[0]].b = bufs[0];

    // tell the device the first index in our chain of descriptors.
    disk.avail->ring[disk.avail->idx % NUM] = idx[0];

    __sync_synchronize();

    // tell the device another avail ring entry is available.
    disk.avail->idx += 1; // not % NUM ...

    __sync_synchronize();

    *R(VIRTIO_MMIO_QUEUE_NOTIFY) = 0; // value is queue number

    // Wait for virtio_disk_intr() to say request has finished.
    int res = 0;
    if (!write)
    {
        bufs[0]->disk = 1;
        // Wait for virtio_disk_intr() to say request has finished.
        while (bufs[0]->disk == 1)
        {
            // printf(__INFO("virtio_disk_rw")" sleep on %d\n", b->sectorno);
            thread_sleep(bufs[0], &disk.vdisk_lock, 0);
        }
        res = -(disk.info[idx[0]].status != 0);
        free_chain(idx[0]);
    }

    release(&disk.vdisk_lock);
    return res;
}

void virtio_disk_rw(struct buf *b, int write)
{
    virtio_disk_rw_multiple(&b, 1, write);
}

void virtio_disk_intr()
{
    acquire(&disk.vdisk_lock);

    // the device won't raise another interrupt until we tell it
    // we've seen this interrupt, which the following line does.
    // this may race with the device writing new entries to
    // the "used" ring, in which case we may process the new
    // completion entries in this interrupt, and have nothing to do
    // in the next interrupt, which is harmless.
    *R(VIRTIO_MMIO_INTERRUPT_ACK) = *R(VIRTIO_MMIO_INTERRUPT_STATUS) & 0x3;

    __sync_synchronize();

    // the device increments disk.used->idx when it
    // adds an entry to the used ring.

    while (disk.used_idx != disk.used->idx)
    {
        __sync_synchronize();
        int id = disk.used->ring[disk.used_idx % NUM].id;
        disk.used_idx += 1;
        if (disk.info[id].status != 0)
            panic("virtio_disk_intr status");

        struct buf *b = disk.info[id].b;
        // disk.info[id].b = 0;
        if (disk.info[id].idx1 < 0)
        {                // read interrupt
            b->disk = 0; // disk is done with buf
            thread_wakeup_chan(b);
        }
        else
        { // write interrupt
            free_chain(disk.info[id].idx1);
        }
    }

    release(&disk.vdisk_lock);
}
