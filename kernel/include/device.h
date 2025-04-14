#ifndef XV6_DEVICE_H_
#define XV6_DEVICE_H_


// Block device switch table entry.
struct bdev_ops {
  int (*open)(int minor);
  int (*close)(int minor);
};

struct bdev {
  int major;
  struct bdev_ops *ops;
};

/* assumes size > 256 */
unsigned int blksize_bits(unsigned int size);
void bdev_table_init(void);
int register_bdev(struct bdev dev);
int unregister_bdev(struct bdev dev);
int bdev_open(struct inode *devi);

#endif /* XV6_DEVICE_H_ */

