## call number 

The system call number of XV6 is different from the required one (a small pitfall), just change it to the required one  
(call write before calling sys_times here)
![times_call_number](image-29.png)

## sleep syscall
The test file seems to use ``timeval`` instead of ``timespec``
 ![struct timeval](image-30.png)


## other 
1. be careful about duplicate file names when ``mkfs``, may lead to unknown error




## debug 记录

### thread.1

![panic_thread.1.1](image-52.png)
![panic_thread.1.2](image-51.png)
![panic_thread.1.3](image-53.png)

p->thread == null!!

![panic_thread.1.4](image-54.png)
![panic_thread.1.5](image-55.png)

![panic_thread.1.6](image-56.png)
scheduler刚开中断，c-> = 0时直接进kerneltrap,导致myproc()空指针引用
是时钟中断！
![panic_thread.1.7](image-57.png)

调用``myproc()``必须保证c->t != null!!
![panic_thread_1.8](image-58.png)

改为
``` c
  if(which_dev == 2 && mythread() != 0 && mythread()->state == TCB_RUNNING) {
    myproc()->ktime++;  
    thread_yield();
  }

```

### thread.2

![panic_thread.2.1](image-59.png)

![panic_thread.2.2](image-60.png)

![panic_thread.2.3](image-61.png)

![panic_thread.2.4](image-62.png)

q == null!

原因是没有`tcb_running queue`，但是使用了`tcb_q_change_state()`
因为每一个running线程一定由CPU保存，所以不需要一个额外队列保存

### thread.3

![thread.3.1](image-63.png)
lost wakeup

push sleeping queue没问题，问题应该出在遍历queue
原来是调用处写错了member name
![thread.3.2](image-64.png)


### thread.4

`usertrapret()`之后卡住

![thread.4.3](image-66.png)
![thread.4.4](image-67.png)
![thread.4.5](image-68.png)
![thread.4.6](image-69.png)
0x0处的代码：
![thread.4.11](image-74.png)  
用户页表 :
![thread.4.12](image-75.png)
![thread.4.14](image-77.png)

！！！ 问题在于sret之后不知道发生了什么。但是用户触发了一个外部中断![external intr](image-82.png)进入到uservec中，随后在uservec对栈进行了写入操作。不知为何发生了![store pgf](image-81.png),继续进入uservec，随后死循环

uservec 暂时先不写栈呢
![thread.4.15](image-79.png)

![thread.4.16](image-80.png)

![thread.4.17](image-83.png)

xv6的:
![thread.4.7](image-70.png)
![thread.4.8](image-71.png)
![thread.4.9](image-72.png)
![thread.4.10](image-73.png)
用户页表：
![thread.4.13](image-76.png)

### thread.5

do_exec无法成功返回用户态

![thread.5.1](image-84.png)
![thread.5.2](image-86.png)
![thread.5.3](image-87.png)
trapframe未映射,重新分配即可
![thread.5.4](image-88.png)

```
[LOG][kernel/sysfile.c,447,sys_exec] do sys_exec
trapframe at 0x0000000087f31000
  kernel_satp 0x8000000000087fff
  kernel_sp 0x0000003fffffe000
  kernel_trap 0x00000000800022de
  kernel_hartid 0x0000000000000000
  epc 0x0000000000000018
  ra 0x0000000000000000
  sp 0x0000000000001000
  gp 0x0000000000000000
  tp 0x0000000000000000
  t0 0x0000000000000000
page table 0x0000000087f34000
 ..0: pte 0x0000000021fcbc01 pa 0x0000000087f2f000
 .. ..0: pte 0x0000000021fcb801 pa 0x0000000087f2e000
 .. .. ..0: pte 0x0000000021fcc05f pa 0x0000000087f30000
 ..255: pte 0x0000000021fccc01 pa 0x0000000087f33000
 .. ..511: pte 0x0000000021fcc801 pa 0x0000000087f32000
 .. .. ..510: pte 0x0000000021fcc4c7 pa 0x0000000087f31000
 .. .. ..511: pte 0x000000002000244b pa 0x0000000080009000
check va: 0x0000003fffffe000
pte 0x0000000021fcc4c7
pa 0x0000000087f31000
unknow devintr()
scause 0x000000000000000d
sepc=0x000000008000828e stval=0x0000000000000000
panic: kerneltrap
QEMU: Terminated
```
### thread.6

`exec()`尝试返回用户态时无限重入`usertrapret()`

![thread.6.1](image-89.png)
![thread.6.2](image-90.png)
![thread.6.3](image-91.png)
![thread.6.4](image-92.png)
![thread.6.5](image-93.png)

暂不确定哪里出现了问题，似乎是调试条件打错了导致一直输出调试信息。但是为何一直重入，应该是某个syscall出问题了。

### thread.7

`fork`之后panic

panic:

``` c
    if(mycpu()->noff != 1)
    panic("sched t locks");
```
`fork()` 中未将group leader thread lock 释放

### thread.8

```
init fork finished!
[LOG][kernel/sched.c,96,thread_sched] thread 1 is in thread_sched
panic: usertrap: not from user mode
QEMU: Terminated

```

`usertrapret()` 
- $sepc == 0x3d8  ret
- $ra == 0x64
- first pgfault in uservec: $sp = 0x3eb0
- can store: $sp = 0x2fe0

```
thread 2 is ready to run
try to get thread2's lock
get the lock of thread 2
ready to switch to thread 2
thread 2 ready to userret
thread 2 usertrap!

process 2, thread 2
sstatus : 0x100
scause: 0xf
panic: usertrap: not from user mode
```

这个bug和thread.4一样，都是无法写栈导致的问题。问题在于我已经检查了物理内存分配且被映射于用户页表中，权限也没问题。  

现在的解决方案： 暂时不用栈来做计算，而是调用`usertrapret()`时将当前线程的trampframe虚拟地址传入函数，最终写入sscratch寄存器，`uservec()`时直接从sscratch中取即可

### thread.9

[LOG][kernel/virtio_disk.c,228,virtio_disk_rw] into disk_rw!
[LOG][kernel/virtio_disk.c,232,virtio_disk_rw] thread 19 try to acquire disk.vdisk_lock
[LOG][kernel/virtio_disk.c,236,virtio_disk_rw] thread 19 has acquired disk.vdisk_lock
[LOG][kernel/virtio_disk.c,247,virtio_disk_rw] alloc_desc break
[LOG][kernel/virtio_disk.c,296,virtio_disk_rw] reach sync1
[LOG][kernel/virtio_disk.c,300,virtio_disk_rw] pass sync1
[LOG][kernel/virtio_disk.c,311,virtio_disk_rw] pass sync2
[LOG][kernel/virtio_disk.c,319,virtio_disk_rw] thread sleep 0x0000000080014230
panic: acquire

带着线程锁进入了`thread_sleep()`
解决方法: `thread_exit()`中不应带着线程锁进入`proc_exit()`.调整获取锁顺序


### thread.10
test copyin: [INFO] fork: parent 3, child 5, child->leader_thread id: 5
[LOG][kernel/thread.c,250,thread_exit] thread 5 exit

[LOG][kernel/thread.c,271,thread_exit] thread 5 has release p->lock

[LOG][kernel/thread.c,277,thread_exit] thread 5 has acquired p->tg.lock

[LOG][kernel/thread.c,285,thread_exit] thread 5 try to release p->tg.lock

[LOG][kernel/thread.c,291,thread_exit] thread 5 released p->tg.lock

panic: freewalk: leaf

[INFO] thread 3's trapframe in freewalk: 0x0000000087f33000
[INFO] panic pte 0x0000000020ab58c7
panic: freewalk: leaf

test copyinstr3:
page table 0x0000000087f20000
 ..0: pte 0x00000000209b5001 pa 0x00000000826d4000
 .. ..0: pte 0x0000000020934c01 pa 0x00000000824d3000
 ..255: pte 0x0000000021fcb401 pa 0x0000000087f2d000
 .. ..511: pte 0x0000000021fcd001 pa 0x0000000087f34000
 .. .. ..510: pte 0x00000000200b08c7 pa 0x00000000802c2000
[INFO] thread 3's trapframe in freewalk: 0x0000000087f33000
[INFO] panic pte 0x00000000200b08c7
panic: freewalk: leaf

test copyin:
page table 0x0000000087f20000
 ..0: pte 0x00000000200b0801 pa 0x00000000802c2000
 .. ..0: pte 0x0000000020130c01 pa 0x00000000804c3000
 ..255: pte 0x0000000021fcb401 pa 0x0000000087f2d000
 .. ..511: pte 0x0000000021fcd001 pa 0x0000000087f34000
 .. .. ..510: pte 0x0000000021fc14c7 pa 0x0000000087f05000
[INFO] thread 3's trapframe in freewalk: 0x0000000087f33000
[INFO] panic pte 0x0000000021fc14c7, pa 0x0000000087f05000
panic: freewalk: leaf
??? 偶发错误？


### thread.11
test killstatus: [INFO] fork: parent 3, child 30, child->leader_thread id: 30
[INFO] fork: parent 30, child 31, child->leader_thread id: 31
panic: acquire


### thread.12
scause 0d:
load page fault
$ usertests reparent
usertests starting
test reparent: unknow devintr()
scause 0x000000000000000d
sepc=0x000000008000399a stval=0x00000000000001d8
panic: kerneltrap

``` c

holding(struct spinlock *lk)
{
  int r;
  r = (lk->locked && lk->cpu == mycpu());
    8000399a:	411c                	lw	a5,0(a0)
    8000399c:	e399                	bnez	a5,800039a2 <holding+0x8>
    8000399e:	4501                	li	a0,0
  return r;
}

```

usertests starting
[INFO] fork: parent 3, child 4, child->leader_thread id: 4
[LOG][sched/thread.c,254,thread_exit] thread 4 exit

[LOG][sched/proc.c,578,proc_exit] thread 4 proc_exit 4
[LOG][sched/thread.c,275,thread_exit] thread 4 has release p->lock

[LOG][sched/thread.c,158,free_thread] thread 3 free thread 4

[LOG][sched/thread.c,281,thread_exit] thread 4 has acquired p->tg.lock

[LOG][sched/thread.c,289,thread_exit] thread 0 try to release p->tg.lock

[LOG][sched/thread.c,295,thread_exit] thread 0 released p->tg.lock

[LOG][sched/thread.c,300,thread_exit] thread 0 try to acquire t->lock

[LOG][sched/thread.c,304,thread_exit] thread 0 has acquired t->lock

[LOG][sched/thread.c,158,free_thread] thread 0 free thread 0

unknow devintr()
scause 0x000000000000000d
sepc=0x0000000080003b3a stval=0x00000000000001d8
panic: kerneltrap

只是把proc退出，还没有完全退出thread时被free了(wait收尸)

在pcb中加入了一个`lth_exitlock`用于保护thread完全退出


### thread.13

usertests reparent 
pid2 == p91
[LOG][sched/thread.c,254,thread_exit] thread 90 exit

[LOG][sched/proc.c,592,proc_exit] thread 90 proc_exit 90
[LOG][sched/thread.c,277,thread_exit] thread 90 has release p->lock

[LOG][sched/thread.c,283,thread_exit] thread 90 has acquired p->tg.lock

[LOG][sched/thread.c,291,thread_exit] thread 90 try to release p->tg.lock

[LOG][sched/thread.c,297,threiad_exit] thread 90 released p->tg.lock

[LOG][sched/thread.c,302,thread_exit] thread 90 try to acquire t->lock

[LOG][sched/thread.c,306,thread_exit] thread 90 has acquired t->lock

[LOG][schedd/thread.c,158,free_thread] thread 90 free thread 90

2 == 0
[LOG][sched/thread.c,254,thread_exit] thread 91 exit

[LOG][sched/proc.c,592,proc_exit] thread 91 proc_exit 91
[LOG][sched/thread.c,277,thread_exit] thread 91 has release p->lock

unknow devintr()
[LOG][sched/thread.c,283,thread_exit] thread 91 has acquired p->tg.lock

[LOG][sched/thread.c,291,thread_exit] thread 91 try to release p->tg.lock

[LOG][sched/thread.c,297,thread_exit] thread 91 released p->tg.lock

scause 0x000000000000000f
sepc=0x000000008000049e stval=0x0000000000000001
panic[LOG][sched/thread.c,302,thread_exit] thr: keread 91 try to acquire t->lock

[LOG][neltrap

偶发错误，考虑竟态？

### thread.14

test exitiput: [INFO] fork: parent 407, child 422, child->leader_thread id: 422
[INFO] fork: parent 422, child 423, child->leader_thread id: 423
[LOG][sched/thread.c,254,thread_exit] thread 423 exit

panic: sched t locks

thread.c:513 end_op()

[INFO] noff when come in thread_exit: 0
[INFO] noff before begin_op 1
[INFO] noff after begin_op 1
[INFO] noff after iput 1
[INFO] noff 2

恰恰是因为加了thread.12中的保护锁，当进入proc_exit时noff!=0...
调整顺序，保证iput()之前c->noff == 0

### thread.15

test writebig: unknow devintr()
scause 0x000000000000000f
sepc=0x00000000800004a6 stval=0x0000000000000001
panic: kerneltrap

$ usertests writebig
usertests starting
test writebig: OK
ALL TESTS PASSED
单独又能测过？，。。。

### thread.16

[INFO] noff when finish thread_exit: 1
[INFO] fork: parent 5, child 184, child->leader_thread id: 184
pid ==p id1 =8=4 
0
[INFO] fork: parent 184, child 185, child->leader_thread id: 185
pid2 ==p id182 5
=[LOG][sched/thread.c,254,thread_exit] thread 184 exi=t

[INFO] noff when come in thread_exit: 0
[LOG][sched/thread.c,278,thread_exit] thread 184 has release p->lock

[LOG][sched/thread.c,284,thread_exit] thread 184 has acquired p->tg.lock

[LOG][sched/thread.c,292,thread_exit] thread 184 try to release p->tg.lock

[LOG][sched/thread.c,298,thread_exit] thread 184 released p->tg.lock

[LOG][sched/thread.c,303,thread_exit] thread 184 try to acquire t->lock

[LOG][sched/thread.c,307,thread_exit] thread 184 has acquired t->lock

[INFO] noff when finish thread_exit: 1
[INFO] thread 5 kerneltrap: unexpected scause 0x000000000000000f
unknow devintr()
scause 0x000000000000000f
sepc=0x00000000800004a6 stval=0x0000000000000001
panic: kerneltrap

``` c

// push back (atomic)
void queue_push_back_atomic(queue_t *q, void *node) {
    80000476:	1101                	addi	sp,sp,-32
    80000478:	ec06                	sd	ra,24(sp)
    8000047a:	e822                	sd	s0,16(sp)
    8000047c:	e426                	sd	s1,8(sp)
    8000047e:	e04a                	sd	s2,0(sp)
    80000480:	1000                	addi	s0,sp,32
    80000482:	84aa                	mv	s1,a0
    80000484:	892e                	mv	s2,a1
    acquire(&q->lock);
    80000486:	00003097          	auipc	ra,0x3
    8000048a:	7a4080e7          	jalr	1956(ra) # 80003c2a <acquire>
    struct list_head *list = queue_entry(node, q->type);
    8000048e:	44ac                	lw	a1,72(s1)
    80000490:	854a                	mv	a0,s2
    80000492:	00000097          	auipc	ra,0x0
    80000496:	f3e080e7          	jalr	-194(ra) # 800003d0 <queue_entry>
    __list_add(pnew, head->prev, head);
    8000049a:	709c                	ld	a5,32(s1)
    next->prev = pnew;
    8000049c:	f088                	sd	a0,32(s1)
    list_add_tail(list, &(q->list));
    8000049e:	01848713          	addi	a4,s1,24
    pnew->next = next;
    800004a2:	e118                	sd	a4,0(a0)
    pnew->prev = prev;
    800004a4:	e51c                	sd	a5,8(a0)
    prev->next = pnew;
    这条指令报错：
    800004a6:	e388                	sd	a0,0(a5)
    release(&q->lock);
    800004a8:	8526                	mv	a0,s1
    800004aa:	00004097          	auipc	ra,0x4
    800004ae:	834080e7          	jalr	-1996(ra) # 80003cde <release>
}
```

[INFO] noff when come in thread_exit: 0
[INFO] thread 5 change state from 4 to 2
[INFO] thread 5 ready to push to state 2 queue
[INFO] thread 5 pushed to state 2 queue
[LOG][sched/thread.c,278,thread_exit] thread 96 has release p->lock

[LOG][sched/thread.c,284,thread_exit] thread 96 has acquired p->tg.lock

[LOG][sched/thread.c,292,thread_exit] thread 96 try to release p->tg.lock

[LOG][sched/thread.c,298,thread_exit] thread 96 released p->tg.lock

[LOG][sched/thread.c,303,thread_exit] thread 96 try to acquire t->lock

[LOG][sched/thread.c,307,thread_exit] thread 96 has acquired t->lock

[INFO] noff when finish thread_exit: 1
[INFO] thread 5 kerneltrap: unexpected scause 0x000000000000000f
unknow devintr()
scause 0x000000000000000f
sepc=0x00000000800004a6 stval=0x0000000000000001
panic: kerneltrap

thread5成功change thread state
难道是process的问题？

### thread.17

$ writetest
[INFO] fork: parent 2, child 4, child->leader_thread id: 4
[INFO] thread 2 wait, ready to sleep
stressfs starting
[INFO] fork: parent 4, child 5, child->leader_thread id: 5
write 0
[INFO] fork: parent 5, child 6, child->leader_thread id: 6
wrwrite 2
ite 1
open done
open done
write downre it0e
 done 0w
rite done 1
write done 1
write done 2
write done 2
wwrite donre 3
ite done 3
open donwerite
 done 4
wwrirte idteon ed 5
one 4
wwrriite tdeo nedo ne5
 0
wriwrite done 1
te done 6
wwriter idteo donne 7
e wri6
te done 2
QEMU: Terminated
只要一涉及多核多线程并发，就有可能死锁，检查死锁和lostwakeup可能


### mmap.1

old pagetable:
page table 0x0000000087f2d000
 ..255: pte 0x0000000021fcd001 pa 0x0000000087f34000
 .. ..511: pte 0x0000000021fccc01 pa 0x0000000087f33000
 .. .. ..510: pte 0x0000000021fcc807 pa 0x0000000087f32000
 .. .. ..511: pte 0x000000002000240b pa 0x0000000080009000
new pagetable:
page table 0x0000000087f2d000
 ..255: pte 0x0000000021fcd001 pa 0x0000000087f34000
 .. ..511: pte 0x0000000021fccc01 pa 0x0000000087f33000
 .. .. ..510: pte 0x0000000021fcc807 pa 0x0000000087f32000
 .. .. ..511: pte 0x000000002000240b pa 0x0000000080009000
panic: uvmcopy: pte should exist
初始化state_list位置错误导致头尾相接

### mmap.2
$ mmaptest
mmap_test starting
test mmap f
fp->writeable: 0
test mmap f: OK
test mmap private
fp->writeable: 0
test mmap private: OK
test mmap read-only
fp->writeable: 0
test mmap read-only: OK
test mmap read/write
fp->writeable: 1
panic: log_write outside of trans
QEMU: Terminated
应该filewrite而不只是iwrite

### mmap.3

$ mmaptest
mmap_test starting
test mmap f
test mmap f: OK
test mmap private
test mmap private: OK
test mmap read-only
test mmap read-only: OK
test mmap read/write
test mmap read/write: OK
test mmap dirty
test mmap dirty: OK
test not-mapped unmap
panic: uvmunmap: not mapped

$ mmaptest
mmap_test starting
test mmap f
[LOG][mm/mmap.c,152,do_mmap] process 3 do mmap: vma_start 0x0000003fffff4000, vma_end 0x0000003fffff6000, fd 3, offset 0

[LOG][sched/trap.c,110,usertrap] proc 3 thread 3 usertrap: mappages va 0x0000003fffff4000, size 0x0000000000001000, mem 0x0000000087f20000, prot 0x0000000000000019

[LOG][sched/trap.c,110,usertrap] proc 3 thread 3 usertrap: mappages va 0x0000003fffff5000, size 0x0000000000001000, mem 0x0000000087f2d000, prot 0x0000000000000019

[LOG][mm/mmap.c,179,mmap_writeback_unmapf] mmap_writeback_unmapf: va 0x0000003fffff4000, pte 0x0000000021fc805b, pa 0x0000000087f20000

[LOG][mm/mmap.c,179,mmap_writeback_unmapf] mmap_writeback_unmapf: va 0x0000003fffff5000, pte 0x0000000021fcb45b, pa 0x0000000087f2d000

test mmap f: OK
test mmap private
[LOG][mm/mmap.c,152,do_mmap] process 3 do mmap: vma_start 0x0000003fffff2000, vma_end 0x0000003fffff4000, fd 3, offset 0

[LOG][sched/trap.c,110,usertrap] proc 3 thread 3 usertrap: mappages va 0x0000003fffff2000, size 0x0000000000001000, mem 0x0000000087f2d000, prot 0x000000000000001b

[LOG][sched/trap.c,110,usertrap] proc 3 thread 3 usertrap: mappages va 0x0000003fffff3000, size 0x0000000000001000, mem 0x0000000087f20000, prot 0x000000000000001b

[LOG][mm/mmap.c,179,mmap_writeback_unmapf] mmap_writeback_unmapf: va 0x0000003fffff2000, pte 0x0000000021fcb4df, pa 0x0000000087f2d000

[LOG][mm/mmap.c,179,mmap_writeback_unmapf] mmap_writeback_unmapf: va 0x0000003fffff3000, pte 0x0000000021fc80df, pa 0x0000000087f20000

test mmap private: OK
test mmap read-only
test mmap read-only: OK
test mmap read/write
[LOG][mm/mmap.c,152,do_mmap] process 3 do mmap: vma_start 0x0000003ffffef000, vma_end 0x0000003fffff2000, fd 3, offset 0

[LOG][sched/trap.c,110,usertrap] proc 3 thread 3 usertrap: mappages va 0x0000003ffffef000, size 0x0000000000001000, mem 0x0000000087f20000, prot 0x000000000000001b

[LOG][sched/trap.c,110,usertrap] proc 3 thread 3 usertrap: mappages va 0x0000003fffff0000, size 0x0000000000001000, mem 0x0000000087f2d000, prot 0x000000000000001b

[LOG][mm/mmap.c,179,mmap_writeback_unmapf] mmap_writeback_unmapf: va 0x0000003ffffef000, pte 0x0000000021fc80df, pa 0x0000000087f20000

[LOG][mm/mmap.c,179,mmap_writeback_unmapf] mmap_writeback_unmapf: va 0x0000003fffff0000, pte 0x0000000021fcb4df, pa 0x0000000087f2d000

test mmap read/write: OK
test mmap dirty
test mmap dirty: OK
test not-mapped unmap
[LOG][mm/mmap.c,179,mmap_writeback_unmapf] mmap_writeback_unmapf: va 0x0000003fffff1000, pte 0x0000000000000000, pa 0x0000000000000000

panic: uvmunmap: not mapped

vma页可能因为还没被访问而未被分配和映射，所以加个判断


## ext4

### ext4.1
奇怪的block_group
![ext4.1](image-111.png)

导致后续``ext4_fs_init_inode_bitmap(struct ext4_block_group_ref *bg_ref)``函数中bitmap_block_addr是一个非常夸张大的数字，导致``ext4_trans_block_get_noread()``失败。事实上这个block_group的数据应该就是错的。checksum时已经抛出了warning

这个函数
```c 
static int
__ext4_fs_get_inode_ref(struct ext4_fs *fs, uint32_t index,
			struct ext4_inode_ref *ref,
			bool initialized)
      ```
    会调用函数
    ```c
    int ext4_fs_get_block_group_ref(struct ext4_fs *fs, uint32_t bgid,
				struct ext4_block_group_ref *ref)
  ```

只要调用
```c
int ext4_fs_get_block_group_ref(struct ext4_fs *fs, uint32_t bgid,
				struct ext4_block_group_ref *ref)
        ```
就会有问题

``` c
int ext4_block_get(struct ext4_blockdev *bdev, struct ext4_block *b,
		   uint64_t lba)
{
	int r = ext4_block_get_noread(bdev, b, lba);

  ```

  最后居然发现是
  ```c
  void *kalloc(void) {
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if (r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if (r)
    memset((char *)r, 5, PGSIZE); // fill with junk
  return (void *)r;
}
```
塞的junk的问题哈哈哈:(

### init.1

![init.1.1](image-126.png)

执行execve时卡死。。。。。。。

not align version?:
![init.1.2](image-127.png)
![init.1.3](image-128.png)
出错位置:(实在是难以定位只能print大法了)
似乎是函数`ext4_bdif_bread()`问题
![init.1.4](image-142.png)
![init.1.5](image-143.png)
![init.1.6](image-144.png)
从磁盘读的问题

![init.1.7](image-145.png)

这么看应该是lostwakeup问题
![init.1.8](image-146.png)
wakeup结束了才sleep??  
好像发现问题所在了。原本xv6使用p->lock保证sleep和wakeup之间的竞态不会发生，也就是保证不会先唤醒再睡眠，但是此时我改成了直接遍历sleeping queue。这会导致有可能还未修改好状态为sleeping，就直接遍历了一个不存在目标即将睡眠线程的sleeping queue.此时该队列中不存在目标线程，自然t->lock对wakeup的约束根本不存在，导致丢失唤醒  
那么存不存在一种情况，就是wakeup先于sleep获取了进程锁和对应状态，导致lost wakeup呢？？  
答案是不可能！这就是条件锁的意义，也是设计精妙之处  
- sleep在获取线程锁之后才会释放条件锁
- wakeup整个过程必须持有条件锁，也就是调用wakeup之前必须持有sleep相关的条件锁！
- wakeup只有同时持有条件锁和线程锁才可以访问线程状态
- 如果二者不存在一个条件锁约束，就有可能导致lostwakeup！

### busybox.1

![busybox.1.1](image-112.png)

![busybox.1.2](image-113.png)

修改栈布局：  
https://refspecs.linuxbase.org/LSB_3.1.0/LSB-generic/LSB-generic/baselib---libc-start-main-.html  
https://stackoverflow.com/questions/62709030/what-is-libc-start-main-and-start  
http://dbp-consulting.com/tutorials/debugging/linuxProgramStartup.html

### busybox.2

![busybox.2.1](image-119.png)
![busybox.2.2](image-118.png)

__libc_setup_tls尝试syscall 214, 即 SYS_brk,添加对应syscall即可

### busybox.3

![busybox.3.1](image-120.png)
![busybox.3.2](image-121.png)
brk后最终调用了syscall 17，然后卡死。（在这之前还有很多的syscall也失败了)  
一个个修吧=-=

### busybox.4
![busybox.4.1](image-122.png)
![busybox.4.2](image-123.png)
居然写了一个代码段的地址？？


### busybox.5
exit_group之后会不断重试执行init

ash_main 没有解析到命令，然后退出了？事实上非login_sh时，也不需要/dev/tty  

`cmd_loop()` --> `parsecmd()` 

`ls_main()` --> `scan_and_display_dir_cur`  


### busybox.6

![busybox.6.1](image-129.png)
问题是getdents传入的f->dir == null

### busybox.7

![busybox.7.1](image-130.png)
![busybox.7.2](image-131.png)

open的返回值写错了，导致每次都返回fd == 0

### busybox.8

![busybox.8.1](image-132.png)
ls一直getdents,怎么解析到读取所有目录项结束呢？，

getdents return 0!! 吗？？？
``ext4_dir_entry_next()``中会改变dir的off,这是判断目录项是否遍历完成的依据。  
传入了临时变量dir指针，导致fp的dir off没有更新。


### busybox.9

![busybox.9.1](image-133.png)
![busybox.9.2](image-134.png)
![busybox.9.3](image-135.png)
![busybox.9.4](image-136.png)
![busybox.9.5](image-137.png)
![busybox.9.6](image-138.png)
`ls_main()` 
nfiles == 0, !cur??
`scan_and_display_dirs_recur()`

![busybox.9.7](image-139.png)
![busybox.9.8](image-140.png)
![busybox.9.9](image-141.png)

第一次到`__bswapdi2()`
第二次到这个地址0xd585c
`__run_exit_handler()`

要不还是先支持musl busybox吧

### busybox.10

![busybox.10.1](image-147.png)
ls为啥一直在写0个字节？？？  
·