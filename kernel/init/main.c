#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"
#include "thread.h"
#include "sched.h"
#include "xv6fs.h"
#include "vfs_mount.h"
#include "vfs.h"
#include "device.h"
#include "ext4fs.h"

volatile static int started = 0;
int init_finished = 0;
volatile static int cpunum = 4;
// start() jumps here in supervisor mode on all CPUs.

static void initfss() {
  init_vfssw();   // init vfs switch list
  init_vfsmlist();  // init mount list
  mntinit();      //  init mount table , maybe duplicate with vfs_mount_table
  bdev_table_init();  // init block device table
  init_vfs_mtable(); // init vfs mount table
  
#ifdef __USE_XV6FS
  if(init_xv6fs() < 0) {
    panic("xv6fs_init failed");
  }
  #else 
  if(init_ext4fs() < 0) {
    panic("ext4fs_init failed");
  }
#endif
  install_rootfs(); 
}
void
main()
{
  if(cpuid() == 0){

    consoleinit();
    printfinit();
    printf("\n");
    printf("xv6fs kernel is booting\n");
    printf("\n");
    kinit();         // physical page allocator
    kvminit();       // create kernel page table
    kvminithart();   // turn on paging

    procinit();      // process table
    tcb_init();
    
    trapinit();      // trap vectors
    trapinithart();  // install kernel trap vector
    plicinit();      // set up interrupt controller
    plicinithart();  // ask PLIC for device interrupts
    binit();         // buffer cache
    iinit();         // inode table
    fileinit();      // file table
    initfss();      // init all filesystems
    virtio_disk_init(); // emulated hard disk
    userinit();      // first user process
    __sync_synchronize();
    started = 1;
    cpunum--;
  } else {
    while(started == 0)
      ;
    __sync_synchronize();
    printf("hart %d starting\n", cpuid());
    kvminithart();    // turn on paging
    trapinithart();   // install kernel trap vector
    plicinithart();   // ask PLIC for device interrupts
    cpunum--;
  }
  if(!cpunum)
    init_finished = 1;

  thread_scheduler();        
}

