// Buffer cache.
//
// bcache -- spinlock
// buf -- sleeplock
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "arch.h"
#include "defs.h"
#include "xv6fs.h"
#include "buf.h"
#include "debug.h"
#include "libahci.h"

extern struct ahci_device g_ahci_dev;

struct {
  struct spinlock lock;
  // NBUF == 10*3??
  // cache number
  // buf 是 LRU node 
  // 指针在buf内
  struct buf buf[NBUF];

  //应该是一个双向循环列表？
  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head;
} bcache;

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  // Create linked list of buffers
  bcache.head.prev = &bcache.head;
  bcache.head.next = &bcache.head;
  // 遍历整个buf
  // 在head之后插入b
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    initsleeplock(&b->lock, "buffer");
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
// 并不会从硬盘读取数据块
// 若未找到，则返回一个locked buffer(refcnt == 0)(不抹除数据块)
struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  acquire(&bcache.lock);

  // Is the block already cached?
  for(b = bcache.head.next; b != &bcache.head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;
#ifdef __DEBUG_BREAD
  Log("bread: dev %d, blockno %d", dev, blockno);
#endif
  b = bget(dev, blockno);
  if(!b->valid) {
    // read new block from disk
#ifdef __DEBUG_BREAD
    Log("bread: read block from disk");
#endif
#ifdef __VIRTIO
    virtio_disk_rw(b, 0);
#elif defined __AHCI
    ahci_sata_read_common(&g_ahci_dev, b->blockno, 1, b->data);
#endif
    b->valid = 1;
  }
#ifdef __DEBUG_BREAD
  Log("bread: read block end");
#endif
  return b; 
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
#ifdef __VIRTIO
  virtio_disk_rw(b, 1);
#elif defined __AHCI
  ahci_sata_write_common(&g_ahci_dev, b->blockno, 1, b->data);
#endif
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
// why??
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  acquire(&bcache.lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    // head 后 插入
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
  
  release(&bcache.lock);
}

// b->refcnt++
void
bpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt++;
  release(&bcache.lock);
}

//b->refcnt--
void
bunpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt--;
  release(&bcache.lock);
}


