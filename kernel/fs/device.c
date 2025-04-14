// Simple device driver interface code.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "vfs.h"
#include "file.h"
#include "device.h"

void bdev_table_init(void);
int register_bdev(struct bdev dev);
int unregister_bdev(struct bdev dev);
int bdev_open(struct inode *devi);

struct {
  struct spinlock lock;
  struct bdev_ops *bdeventry[MAXBDEV];
} bdev_table;

void
bdev_table_init(void)
{
  initlock(&bdev_table.lock, "bdev_table");
}

int
register_bdev(struct bdev dev)
{

  if (dev.major > MAXBDEV - 1)
    return -1;

  acquire(&bdev_table.lock);
  bdev_table.bdeventry[dev.major] = dev.ops;
  release(&bdev_table.lock);

  return 0;
}

int
unregister_bdev(struct bdev dev)
{

  if (dev.major > MAXBDEV - 1)
    return -1;

  acquire(&bdev_table.lock);
  bdev_table.bdeventry[dev.major] = 0;
  release(&bdev_table.lock);

  return 0;
}

int
bdev_open(struct inode *devi)
{
  if (devi->major > MAXBDEV - 1)
    return -1;

  struct bdev_ops *bops = bdev_table.bdeventry[devi->major];
  if (bops == 0) return -1;

  return bops->open(devi->minor);
}

unsigned int blksize_bits(unsigned int size)
{
  unsigned int bits = 8;
  do {
      bits++;
      size >>= 1;
    } while (size > 256);
  return bits;
}
