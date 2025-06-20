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
#include "uname.h"
#include "futex.h"
#include "sbi.h"

extern struct utsname g_uts;
volatile static int started = 0;
volatile static int boot_hart = -1;
extern char __bss_start, __bss_end; // 引用链接器脚本中定义的符号
extern void _entry();
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
void clear_bss_section(void)
{
    char *bss = &__bss_start;
    char *bss_end = &__bss_end;

    while (bss < bss_end)
    {
        *bss++ = 0;
    }
}

static void start_harts()
{
    for (int i = 0; i < NCPU; i++)
    {
        if (sbi_hart_get_status(i) == SBI_HSM_STATE_STOPPED)
        {
            sbi_hart_start(i, (uint64)_entry, 0);
        } else {
          // printf("hart %d status %d\n", i, sbi_hart_get_status(i));
        }
    }
}
void
main()
{
   if(boot_hart == -1){
    boot_hart = cpuid();
    clear_bss_section();
    consoleinit();
    printfinit();
    printf("\n");
    printf("reXvapor kernel is booting\n");
    printf("\n");
    kinit();         // physical page allocator
    kvminit();       // create kernel page table
    kvminithart();   // turn on paging

    procinit();      // process table
    tcb_init();
    
    INIT_UTS(g_uts); // initialize utsname structur
    futex_hash_init(); // init futex hash table
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
    printf("hart %d started\n", cpuid());
    start_harts();
  } else {
    while(started == 0)
      ;
    __sync_synchronize();
    printf("hart %d starting\n", cpuid());
    kvminithart();    // turn on paging
    trapinithart();   // install kernel trap vector
    plicinithart();   // ask PLIC for device interrupts
  }
  set_next_trigger();

  thread_scheduler();        
}

