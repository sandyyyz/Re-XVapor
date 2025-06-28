## call number 

The system call number of XV6 is different from the required one (a small pitfall), just change it to the required one  
(call write before calling sys_times here)
![times_call_number](image/image-29.png)

## sleep syscall
The test file seems to use ``timeval`` instead of ``timespec``
 ![struct timeval](image/image-30.png)


## other 
1. be careful about duplicate file names when ``mkfs``, may lead to unknown error




## debug 记录

### thread.1

![panic_thread.1.1](image/image-52.png)
![panic_thread.1.2](image/image-51.png)
![panic_thread.1.3](image/image-53.png)

p->thread == null!!

![panic_thread.1.4](image/image-54.png)
![panic_thread.1.5](image/image-55.png)

![panic_thread.1.6](image/image-56.png)
scheduler刚开中断，c-> = 0时直接进kerneltrap,导致myproc()空指针引用
是时钟中断！
![panic_thread.1.7](image/image-57.png)

调用``myproc()``必须保证c->t != null!!
![panic_thread_1.8](image/image-58.png)

改为
``` c
  if(which_dev == 2 && mythread() != 0 && mythread()->state == TCB_RUNNING) {
    myproc()->ktime++;  
    thread_yield();
  }

```

### thread.2

![panic_thread.2.1](image/image-59.png)

![panic_thread.2.2](image/image-60.png)

![panic_thread.2.3](image/image-61.png)

![panic_thread.2.4](image/image-62.png)

q == null!

原因是没有`tcb_running queue`，但是使用了`tcb_q_change_state()`
因为每一个running线程一定由CPU保存，所以不需要一个额外队列保存

### thread.3

![thread.3.1](image/image-63.png)
lost wakeup

push sleeping queue没问题，问题应该出在遍历queue
原来是调用处写错了member name
![thread.3.2](image/image-64.png)


### thread.4

`usertrapret()`之后卡住

![thread.4.3](image/image-66.png)
![thread.4.4](image/image-67.png)
![thread.4.5](image/image-68.png)
![thread.4.6](image/image-69.png)
0x0处的代码：
![thread.4.11](image/image-74.png)  
用户页表 :
![thread.4.12](image/image-75.png)
![thread.4.14](image/image-77.png)

！！！ 问题在于sret之后不知道发生了什么。但是用户触发了一个外部中断![external intr](image/image-82.png)进入到uservec中，随后在uservec对栈进行了写入操作。不知为何发生了![store pgf](image/image-81.png),继续进入uservec，随后死循环

uservec 暂时先不写栈呢
![thread.4.15](image/image-79.png)

![thread.4.16](image/image-80.png)

![thread.4.17](image/image-83.png)

xv6的:
![thread.4.7](image/image-70.png)
![thread.4.8](image/image-71.png)
![thread.4.9](image/image-72.png)
![thread.4.10](image/image-73.png)
用户页表：
![thread.4.13](image/image-76.png)

### thread.5

do_exec无法成功返回用户态

![thread.5.1](image/image-84.png)
![thread.5.2](image/image-86.png)
![thread.5.3](image/image-87.png)
trapframe未映射,重新分配即可
![thread.5.4](image/image-88.png)

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

![thread.6.1](image/image-89.png)
![thread.6.2](image/image-90.png)
![thread.6.3](image/image-91.png)
![thread.6.4](image/image-92.png)


暂不确定哪里出现了问题，似乎是调试条件打错了导致一直输出调试信息。但是为何一直重入，应该是某个syscall出问题了。

### thread.7
![thread.6.5](image/image-93.png)

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
![ext4.1](image/image-111.png)

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

![init.1.1](image/image-126.png)

执行execve时卡死。。。。。。。

not align version?:
![init.1.2](image/image-127.png)
![init.1.3](image/image-128.png)
出错位置:(实在是难以定位只能print大法了)
似乎是函数`ext4_bdif_bread()`问题
![init.1.4](image/image-142.png)
![init.1.5](image/image-143.png)
![init.1.6](image/image-144.png)
从磁盘读的问题

![init.1.7](image/image-145.png)

这么看应该是lostwakeup问题
![init.1.8](image/image-146.png)
wakeup结束了才sleep??  
好像发现问题所在了。原本xv6使用p->lock保证sleep和wakeup之间的竞态不会发生，也就是保证不会先唤醒再睡眠，但是此时我改成了直接遍历sleeping queue。这会导致有可能还未修改好状态为sleeping，就直接遍历了一个不存在目标即将睡眠线程的sleeping queue.此时该队列中不存在目标线程，自然t->lock对wakeup的约束根本不存在，导致丢失唤醒  
那么存不存在一种情况，就是wakeup先于sleep获取了进程锁和对应状态，导致lost wakeup呢？？  
答案是不可能！这就是条件锁的意义，也是设计精妙之处  
- sleep在获取线程锁之后才会释放条件锁
- wakeup整个过程必须持有条件锁，也就是调用wakeup之前必须持有sleep相关的条件锁！
- wakeup只有同时持有条件锁和线程锁才可以访问线程状态
- 如果二者不存在一个条件锁约束，就有可能导致lostwakeup！

### busybox.1

![busybox.1.1](image/image-112.png)

![busybox.1.2](image/image-113.png)

修改栈布局：  
https://refspecs.linuxbase.org/LSB_3.1.0/LSB-generic/LSB-generic/baselib---libc-start-main-.html  
https://stackoverflow.com/questions/62709030/what-is-libc-start-main-and-start  
http://dbp-consulting.com/tutorials/debugging/linuxProgramStartup.html

### busybox.2

![busybox.2.1](image/image-119.png)
![busybox.2.2](image/image-118.png)

__libc_setup_tls尝试syscall 214, 即 SYS_brk,添加对应syscall即可

### busybox.3

![busybox.3.1](image/image-120.png)
![busybox.3.2](image/image-121.png)
brk后最终调用了syscall 17，然后卡死。（在这之前还有很多的syscall也失败了)  
一个个修吧=-=

### busybox.4
![busybox.4.1](image/image-122.png)
![busybox.4.2](image/image-123.png)
居然写了一个代码段的地址？？


### busybox.5
exit_group之后会不断重试执行init

ash_main 没有解析到命令，然后退出了？事实上非login_sh时，也不需要/dev/tty  

`cmd_loop()` --> `parsecmd()` 

`ls_main()` --> `scan_and_display_dir_cur`  


### busybox.6

![busybox.6.1](image/image-129.png)
问题是getdents传入的f->dir == null

### busybox.7

![busybox.7.1](image/image-130.png)
![busybox.7.2](image/image-131.png)

open的返回值写错了，导致每次都返回fd == 0

### busybox.8

![busybox.8.1](image/image-132.png)
ls一直getdents,怎么解析到读取所有目录项结束呢？，

getdents return 0!! 吗？？？
``ext4_dir_entry_next()``中会改变dir的off,这是判断目录项是否遍历完成的依据。  
传入了临时变量dir指针，导致fp的dir off没有更新。


### busybox.9

![busybox.9.1](image/image-133.png)
![busybox.9.2](image/image-134.png)
![busybox.9.3](image/image-135.png)
![busybox.9.4](image/image-136.png)
![busybox.9.5](image/image-137.png)
![busybox.9.6](image/image-138.png)
`ls_main()` 
nfiles == 0, !cur??
`scan_and_display_dirs_recur()`

![busybox.9.7](image/image-139.png)
![busybox.9.8](image/image-140.png)
![busybox.9.9](image/image-141.png)

第一次到`__bswapdi2()`
第二次到这个地址0xd585c
`__run_exit_handler()`

要不还是先支持musl busybox吧  
如果遇到这种异常的utrap，先把进程杀了而不是panic吧。实在找不到什么原因了

![busybox.9.10](image/image-157.png)
clone传入的flags不为0， 难道是clone实现不规范的原因？

### busybox.10

![busybox.10.1](image/image-147.png)
ls为啥一直在写0个字节？？？  
跳过非法vector就好了

### busybox.11
为什么一直无法退出shell?  
- 没有到达exitshell
- 没有退出cmdloop
![busybox.11.1](image/image-148.png)

执行完一轮之后再hit parsecmd()， n == 0x0?why?  
![busybox.11.2](image/image-149.png)
![busybox.11.3](image/image-150.png)

除了第一次，好像从来没有执行inter++这行，一直在重复解析一个指令  mnb 
![busybox.11.4](image/image-151.png)

就改了两次？？。。。
![busybox.11.5](image/image-152.png)

### busybox.12

虽然我现在没有把sendfile写完，但是为什么他在源源不断地把一个文件内容拷贝到标准输出？难道我fp->pos忘记修改了？？
![busybox.12.1](image/image-153.png)

看样子是的……，难道这就是之前shell一直无限执行一个指令，并且不退出的原因吗？
![busybox.12.2](image/image-154.png)
没错！

### busybox.13

这是爆栈了吗？
![busybox.13.1](image/image-155.png)
![busybox.13.2](image/image-156.png)
回来的时候s0居然和原本的值不一样；；；  
这个真是个玄学

### busybox.14
为什么sleep 1 会传进来这么奇怪一个数字 0x7fff ffff
![busybox.14.1](image/image-158.png)  
是INT_MAX  

### busybox.15
========== END test_read ==========
Testing sleep :
========== START test_sleep ==========
8  282377
[LOG][sysproc.c,137,sys_nanosleep] [sys_nanosleep] ticks: 0, ticks0: 0, rticks: 10
时钟中断问题，启动核不一定为0  

### busybox.16

thread 5 syscall 73: sys_ppoll
thread 5 syscall 94: sys_exit_group
[LOG][ipc/pipe.c,75,pipeclose] pipeclose: pi 0x000000009fb77000 -> readopen = 0
thread 2 syscall 260: sys_wait4
thread 4 syscall 96: sys_set_tid_address
thread 4 syscall 174: sys_getuid
thread 4 syscall 56: sys_openat
[LOG][fs/sysfile.c,960,sys_openat] [sys_openat] abs_path = /musl/busybox_cmd.txt
[LOG][fs/sysfile.c,971,sys_openat] [sys_openat] generic_open success, fd = 4, path = /musl/busybox_cmd.txt, f 0x0000000080235f68, f->fpos 0, flags 8000
thread 4 syscall 71: sys_sendfile
[LOG][fs/sysfile.c,1678,sys_sendfile] [sys_sendfile] out_fd = 1, in_fd = 4, offset_addr = 0x0000000000000000, count = 16777216
[LOG][fs/sysfile.c,1578,do_sendfile] do_sendfile: kbuf = 0x000000009f9ca000, kbuf size = 4096
rw_sharp: filewrite failed, wcnt = -1
thread 4 syscall 63: sys_read
thread 4 syscall 57: sys_close
[LOG][fs/sysfile.c,276,sys_close] sys_close: fd=4, f=0x0000000080235f68, ref after close 0, f->fpos 0, path , f->type 0
thread 4 syscall 94: sys_exit_group
[LOG][ipc/pipe.c,69,pipeclose] pipeclose: pi 0x000000009fb77000 -> writeopen = 0
[LOG][ipc/pipe.c,82,pipeclose] pipeclose: pi 0x0000000000000001 -> both readopen and writeopen are 0, releasing pipe

ppoll未实现 
fixed

### busybox.16

#### musl

[LOG][ipc/signal.c,45,do_sigprocmask] do_sigprocmask: thread 5, how: 0, set: 0xffffffffffffffff
thread 5 syscall 220: sys_clone
[LOG][sched/proc.c,708,do_clone] do_clone: old proc 5, sz 0x00000000001a8000, oldt->trapframe->sp 0x00000000001a4fd0
[LOG][sched/proc.c,709,do_clone] do_clone: np 6, sz 0x00000000001a8000, t->trapframe->sp 0x00000000001a4fd0
thread 5 syscall 135: sys_rt_sigprocmask
[LOG][ipc/signal.c,45,do_sigprocmask] do_sigprocmask: thread 5, how: 2, set: 0xffffffffffffffff
thread 5 syscall 260: sys_wait4
thread 6 syscall 178: sys_gettid
thread 6 syscall 135: sys_rt_sigprocmask
[LOG][ipc/signal.c,45,do_sigprocmask] do_sigprocmask: thread 6, how: 2, set: 0xffffffffffffffff
thread 6 syscall 221: sys_execve
[LOG][fs/exec.c,244,execve] execve abs_path: /musl/busybox, path ./busybox, cinfo.path /musl
[LOG][fs/exec.c,273,execve] ph.type == 0x6474e551
[LOG][fs/exec.c,273,execve] ph.type == 0x6474e552
thread 6 syscall 96: sys_set_tid_address
thread 6 syscall 174: sys_getuid
thread 6 syscall 214: sys_brk
thread 6 syscall 214: sys_brk
thread 6 syscall 64: sys_write

independent command test
thread 6 syscall 94: sys_exit_group
[LOG][ipc/futex.c,298,futex_wake] futex_wake: waking up at most 1 waiters on futex at address 0x0000000000163c14

[WARN][ipc/futex.c,303,futex_wake] futex_wake: futex not found for address 0x0000000000163c14

[LOG][ipc/signal.c,384,signal_send] signal_send: thread 6 send signal 17 to thread 5
thread 5 syscall 260: sys_wait4
thread 5 syscall 64: sys_write
testcase busybox echo "#### independent command test" success


#### glibc

[LOG][fs/exec.c,273,execve] ph.type == 0x70000003
[LOG][fs/exec.c,273,execve] ph.type == 0x4
[LOG][fs/exec.c,273,execve] ph.type == 0x7
[LOG][fs/exec.c,273,execve] ph.type == 0x6474e551
[LOG][fs/exec.c,273,execve] ph.type == 0x6474e552
thread 6 syscall 214: sys_brk
thread 6 syscall 214: sys_brk
thread 6 syscall 96: sys_set_tid_address
thread 6 syscall 99: sys_set_robust_list
thread 6 syscall 160: sys_uname
thread 6 syscall 261: sys_prlimit64
thread 6 syscall 78: sys_readlinkat
thread 6 syscall 278: sys_getrandom
thread 6 syscall 214: sys_brk
thread 6 syscall 214: sys_brk
thread 6 syscall 174: sys_getuid
thread 6 syscall 64: sys_write

independent command test

thread 6 syscall 94: sys_exit_group
[LOG][ipc/futex.c,298,futex_wake] futex_wake: waking up at most 1 waiters on futex at address 0x00000000002020d0

[WARN][ipc/futex.c,303,futex_wake] futex_wake: futex not found for address 0x00000000002020d0

[LOG][ipc/signal.c,384,signal_send] signal_send: thread 6 send signal 17 to thread 5
[LOG][ipc/signal.c,148,signal_handle] signal_handle: thread 5 handle 17 with handler 0x0000000000000000
[LOG][ipc/signal.c,149,signal_handle] t->blockd.sig: 0x0000000000000000
thread 5 syscall 260: sys_wait4
thread 5 syscall 56: sys_openat
[LOG][fs/sysfile.c,960,sys_openat] [sys_openat] abs_path = /usr/lib/riscv64-linux-gnu/gconv/gconv-modules.cache
[WARN][fs/sysfile.c,964,sys_openat] [sys_openat] generic_open failed, abs_path = /usr/lib/riscv64-linux-gnu/gconv/gconv-modules.cache, r = -2

thread 5 syscall 56: sys_openat
[LOG][fs/sysfile.c,960,sys_openat] [sys_openat] abs_path = /usr/lib/riscv64-linux-gnu/gconv/gconv-modules
[WARN][fs/sysfile.c,964,sys_openat] [sys_openat] generic_open failed, abs_path = /usr/lib/riscv64-linux-gnu/gconv/gconv-modules, r = -2

thread 5 syscall 56: sys_openat
[LOG][fs/sysfile.c,960,sys_openat] [sys_openat] abs_path = /usr/lib/riscv64-linux-gnu/gconv/gconv-modules.d
[WARN][fs/sysfile.c,964,sys_openat] [sys_openat] generic_open failed, abs_path = /usr/lib/riscv64-linux-gnu/gconv/gconv-modules.d, r = -2

thread 5 syscall 98: sys_futex
[LOG][sysproc.c,631,sys_futex] [sys_futex] uaddr: 0x00000000001c102c, futex_op: 129, val: 2147483647, timeout_addr: 0x0000000000000000, val2: 0, uaddr2: 0x0000000000000000, val3: 2
[LOG][ipc/futex.c,298,futex_wake] futex_wake: waking up at most 2147483647 waiters on futex at address 0x00000000001c102c

[WARN][ipc/futex.c,303,futex_wake] futex_wake: futex not found for address 0x00000000001c102c

thread 5 usertrap: page fault at 0x0000000000000000
sepc=0x00000000000ed044 stval=0x0000000000000028
scause=0x000000000000000f
sstatus=0x8000000000046020
satp=0x80000000000a01ff
panic: usertrap: page fault

1. 提前拷贝依赖文件（不知道测评机器有无root权限）
2. 绕过（自己构造输出）  

现在伪造了一个空的这个文件，但是发现他最后还是会尝试打开别的路径下的conv文件，还会在同一个地方崩溃。那么其实也许没有这个依赖文件也是可以运行的吧！  

![busybox.16.1](image/image-169.png)
我猜是这行：

```c

	failed:
	  data->fcts = (void *) &__wcsmbs_gconv_fcts_c;

```
难道是这个private为空？  
struct lc_ctype_data *data = new_category->private;  
打出的data为空指针  
刚进函数的时候a0就是0  

#### musl busybox test:

Filesystem           1K-blocks      Used Available Use% Mounted on
df: /proc/mounts: No such file or directory
testcase busybox df fail

[ext4_vfaccess] ext4_raw_inode_fill error!, path /sbin/ls
sys_faccessat: fsops->faccessat failed
[ext4_vfaccess] ext4_raw_inode_fill error!, path /usr/sbin/ls
sys_faccessat: fsops->faccessat failed
[ext4_vfaccess] ext4_raw_inode_fill error!, path /bin/ls
sys_faccessat: fsops->faccessat failed
[ext4_vfaccess] ext4_raw_inode_fill error!, path /usr/bin/ls
sys_faccessat: fsops->faccessat failed
testcase busybox which ls fail


PID   USER     TIME  COMMAND
ps: can't open '/proc': No such file or directory
testcase busybox ps fail

              total        used        free      shared  buff/cache   available
Mem:   free: can't open '/proc/meminfo': No such file or directory
testcase busybox free fail

hwclock: can't open '/dev/misc/rtc': No such file or directory
testcase busybox hwclock fail'

/musl/busybox_testcode.sh: eval: line 0: can't open '/dev/null': No such file or directory
kill: can't kill pid 28: Operation not permitted
testcase busybox sh -c 'sleep 5' & ./busybox kill $! fail

sys_mkdirat: abs_path = /musl/test_dir
testcase busybox mkdir test_dir success
fsops->fstat failed, r = 2
[sys_fstatat] generic_fstat failed
[WARN][syscall.c,134,syscall] thread 57 syscall 276: unknown
mv: can't rename 'test_dir': Operation not permitted
testcase busybox mv test_dir test fail  


__mbsrtowcs_l (似乎没有hit)->   
1. 先弄明白这个函数在哪里会被调用，作用是什么， 可能的问题是什么  

## libc-test

### libc-test-static 

#### libc.1

pgfault handler:  
&pte = 0x9fb82fa0
pgtable addr:0x9fb80000  
va 0x0000003fffff4000, size 0x0000000000001000  

futex_copyin:
pgtable addr: 0x9fb80000  
pte = 0;  


#### libc.2
`thread_cancel_points()`  

/musl # thread 2 syscall 63
/musl/runtest.exe -w entry-static.exe pthread_cancel_points
thread 2 syscall 135
thread 2 syscall 220
[LOG][sysproc.c,221,sys_clone] [sys_clone] flags: 0x11, stack: 0x0000000000000000, ptid: 1595216, tls: 0x0000000000000008, ctid: 0x0000000000000800
thread 2 syscall 135
thread 3 syscall 178
thread 3 syscall 135
thread 2 syscall 260
thread 3 syscall 134
thread 3 syscall 134
thread 3 syscall 134
thread 3 syscall 221
[LOG][fs/exec.c,244,execve] execve abs_path: /musl/runtest.exe, path /musl/runtest.exe, cinfo.path /musl
thread 3 syscall 96
thread 3 syscall 135
thread 3 syscall 135
thread 3 syscall 134
thread 3 syscall 64
========== START entry-static.exe pthread_cancel_points ==========
thread 3 syscall 135
thread 3 syscall 220
[LOG][sysproc.c,221,sys_clone] [sys_clone] flags: 0x11, stack: 0x0000000000000000, ptid: 252736, tls: 0x0000000000000008, ctid: 0x0000000000000000
thread 3 syscall 135
thread 4 syscall 178
thread 3 syscall 137
thread 4 syscall 135
thread 4 syscall 261
thread 4 syscall 221
[LOG][fs/exec.c,244,execve] execve abs_path: /musl/entry-static.exe, path entry-static.exe, cinfo.path /musl
thread 4 syscall 96
thread 4 syscall 135
thread 4 syscall 222
thread 4 syscall 226
thread 4 syscall 135
thread 4 syscall 220
[LOG][sysproc.c,221,sys_clone] [sys_clone] flags: 0x7d0f00, stack: 0x0000003fffff4ad8, ptid: -46272, tls: 0x0000003fffff4be8, ctid: 0x000000000009db80
CLONE_SETTLS
CLONE_CHILD_CLEARTID
set stack to 0x0000003fffff4ad8
CLONE_SIGHAND
CLONE_PARENT_SETTID
thread 4 syscall 135
thread 4 syscall 134
thread 4 syscall 130
thread 4 syscall 98
[LOG][sysproc.c,622,sys_futex] [sys_futex] uaddr: 0x0000003fffff4b48, futex_op: 128, val: 1, timeout_addr: 0x0000000000000000, val2: 0, uaddr2: 0x0000000000000000, val3: 0
thread 5 syscall 135
[LOG][ipc/signal.c,114,signal_handle] signal_handle: thread 5 has 1 pending signals
[WARN][ipc/signal.c,90,signal_default] Default action for signal 33 not implemented
thread 5 syscall 135
[LOG][ipc/futex.c,274,futex_wait] futex_wait: thread 4 waiting on futex at address 0x0000003fffff4b48 with value 1, timeout 0 ticks

thread 5 syscall 98
[LOG][sysproc.c,622,sys_futex] [sys_futex] uaddr: 0x0000003fffff4b48, futex_op: 129, val: 1, timeout_addr: 0x0000000000000000, val2: 0, uaddr2: 0x0000000000000002, val3: -46264
[LOG][ipc/futex.c,297,futex_wake] futex_wake: waking up at most 1 waiters on futex at address 0x0000003fffff4b48

[WARN][ipc/futex.c,301,futex_wake] futex_wake: futex not found for address 0x0000003fffff4b48

thread 5 syscall 93
[LOG][ipc/futex.c,297,futex_wake] futex_wake: waking up at most 1 waiters on futex at address 0x000000000009db80

[WARN][ipc/futex.c,301,futex_wake] futex_wake: futex not found for address 0x000000000009db80

[LOG][sched/thread.c,367,thread_wakeup_timeout] thread_wakeup_timeout: thread 3 wakeup on timeout 74
thread 3 syscall 129
[LOG][sched/proc.c,905,proc_kill] kill: pid 4, name entry-static.ex, sig 9 
[LOG][ipc/futex.c,278,futex_wait] futex_wait: thread 4 woke up from futex wait on address 0x0000003fffff4b48

[LOG][ipc/futex.c,297,futex_wake] futex_wake: waking up at most 1 waiters on futex at address 0x000000000009db80

[WARN][ipc/futex.c,301,futex_wake] futex_wake: futex not found for address 0x000000000009db80

thread 3 syscall 260
thread 3 syscall 64
FAIL pthread_cancel_points [status 255]
thread 3 syscall 64
========== END entry-static.exe pthread_cancel_points ==========
thread 3 syscall 94
[LOG][ipc/futex.c,297,futex_wake] futex_wake: waking up at most 1 waiters on futex at address 0x000000000001d238

[WARN][ipc/futex.c,301,futex_wake] futex_wake: futex not found for address 0x000000000001d238

thread 2 syscall 260
thread 2 syscall 29
thread 2 syscall 17
thread 2 syscall 175
thread 2 syscall 66
/musl # thread 2 syscall 63


### libc.3
这个bug难道和RLIMIT_STACK尚未支持有关吗？  
![libc.3](image/image-159.png)

### libc.4
unsupport now:  

/musl/runtest.exe -w entry-static.exe pthread_tsd  
/musl/runtest.exe -w entry-static.exe pthread_cond
/musl/runtest.exe -w entry-static.exe setjmp    
/musl/runtest.exe -w entry-static.exe socket
/musl/runtest.exe -w entry-static.exe stat
/musl/runtest.exe -w entry-static.exe utime
/musl/runtest.exe -w entry-static.exe fflush_exit
/musl/runtest.exe -w entry-static.exe pthread_robust_detach
/musl/runtest.exe -w entry-static.exe pthread_cancel_sem_wait
/musl/runtest.exe -w entry-static.exe pthread_cond_smasher
/musl/runtest.exe -w entry-static.exe pthread_once_deadlock
/musl/runtest.exe -w entry-static.exe syscall_sign_extend
/musl/runtest.exe -w entry-static.exe pthread_rwlock_ebusy
/musl/runtest.exe -w entry-static.exe pthread_cancel_points
14/107

for dynamic: 17/110

/glibc/runtest.exe -w entry-static.exe clocale_mbfuncs
/glibc/runtest.exe -w entry-static.exe fdopen
/glibc/runtest.exe -w entry-static.exe fnmatch
/glibc/runtest.exe -w entry-static.exe fwscanf
/glibc/runtest.exe -w entry-static.exe mbc
/glibc/runtest.exe -w entry-static.exe pthread_cancel_points
/glibc/runtest.exe -w entry-static.exe pthread_cancel
/glibc/runtest.exe -w entry-static.exe pthread_cond
/glibc/runtest.exe -w entry-static.exe pthread_tsd
/glibc/runtest.exe -w entry-static.exe setjmp
/glibc/runtest.exe -w entry-static.exe snprintf
/glibc/runtest.exe -w entry-static.exe socket
/glibc/runtest.exe -w entry-static.exe sscanf
/glibc/runtest.exe -w entry-static.exe sscanf_long
/glibc/runtest.exe -w entry-static.exe stat
/glibc/runtest.exe -w entry-static.exe strftime
/glibc/runtest.exe -w entry-static.exe strptime
/glibc/runtest.exe -w entry-static.exe strtod
/glibc/runtest.exe -w entry-static.exe strtod_simple
/glibc/runtest.exe -w entry-static.exe strtof
/glibc/runtest.exe -w entry-static.exe strtol
/glibc/runtest.exe -w entry-static.exe strtold
/glibc/runtest.exe -w entry-static.exe swprintf
/glibc/runtest.exe -w entry-static.exe utime
/glibc/runtest.exe -w entry-static.exe wcstol
/glibc/runtest.exe -w entry-static.exe daemon_failure
/glibc/runtest.exe -w entry-static.exe dn_expand_empty
/glibc/runtest.exe -w entry-static.exe dn_expand_ptr_0
/glibc/runtest.exe -w entry-static.exe fflush_exit
/glibc/runtest.exe -w entry-static.exe fgetwc_buffering
/glibc/runtest.exe -w entry-static.exe ftello_unflushed_append
/glibc/runtest.exe -w entry-static.exe mbsrtowcs_overflow
/glibc/runtest.exe -w entry-static.exe printf_fmt_g_round
/glibc/runtest.exe -w entry-static.exe printf_fmt_g_zeros
/glibc/runtest.exe -w entry-static.exe pthread_cond_smasher
/glibc/runtest.exe -w entry-static.exe pthread_condattr_setclock
/glibc/runtest.exe -w entry-static.exe pthread_exit_cancel
/glibc/runtest.exe -w entry-static.exe pthread_rwlock_ebusy
/glibc/runtest.exe -w entry-static.exe regex_bracket_icase
/glibc/runtest.exe -w entry-static.exe regex_ere_backref
/glibc/runtest.exe -w entry-static.exe regex_escaped_high_byte
/glibc/runtest.exe -w entry-static.exe rewind_clear_error
/glibc/runtest.exe -w entry-static.exe setvbuf_unget
/glibc/runtest.exe -w entry-static.exe sigprocmask_internal
/glibc/runtest.exe -w entry-static.exe sscanf_eof
/glibc/runtest.exe -w entry-static.exe syscall_sign_extend
/glibc/runtest.exe -w entry-static.exe uselocale_0

45/107

好几个bug都是这种类似的错误
thread 8 usertrap: page fault at 0x0000000000000000
sepc=0x00000000000589ba stval=0x0000000000000028
scause=0x000000000000000f
sstatus=0x0000000000000020
satp=0x800000000009ffff

1. 测试signal返回用户空间
2. 检查execve用户栈构造（包括aux）
目前signal没有测试能否成功返回用户空间再回到内核， glibc退出user会崩溃。  
3. entry-static.exe似乎只是检查执行程序退出状态来判断是否执行成功的，如果遇到usertrap直接exit(0)直接就会输出PASS,但是实际上功能并没有完全执行完毕。。。
我真的要疯了。
`__dl_aux_init()`

只有glibc才会有`__libc_start_main()`以及`__run_exit_handlers()`的过程， 难道是这里出了问题？？  
首先搞清楚glibc启动和退出相对于musl而言做了哪些额外工作  

`  __run_exit_handlers (status, &__exit_funcs, true, true);`
![libc.4.1](image/image-160.png)

``` c 
static struct exit_function_list initial;
struct exit_function_list *__exit_funcs = &initial;
```
~~ 这个链表好像是空 ~~

![libc.4.2](image/image-161.png)

不对，其实这是一个存了两个析构函数的表！

![libc.4.4](image/image-163.png)
这行控制转移了 
![libc.4.3](image/image-162.png)

执行第二个析构函数的时候崩了,
第一个
这个函数为call_fini,在aexit时注册了,执行完了好像没问题
注册的时候rtld_fini == NULL, 那这个第一个注册的析构函数到底是什么？
exit_funcs这里有两个函数(idx == 2)，但是为什么有一个非常奇怪的函数，func == 0x3???
![libc.4.6](image/image-165.png)
一些关键的断点：
![libc.4.5](image/image-164.png)
0xd0e7e: jlr __internal_atexit 1
0xd0ea0: jlr __internal_atexit 2
0xd0c08 call_fini
0xd585c jalr s10
0xd5740 ld s9(__exit_funcs)

__start()里a5就被赋值0x3

``` 
mov a5, a0
```
在__libc_start_main_impl里又将a5给了s4，之后对s4判断是否为空，如果空就跳过存入析构函数，否则就把这儿数当作一个析构函数指针注册。。。
那么这个_start()中为什么a5 == 3?

!!! 详见glibc start.S
``` c

/* The entry point's job is to call __libc_start_main.  Per the ABI,
   a0 contains the address of a function to be passed to atexit.
   __libc_start_main wants this in a5.  */

```
也就是说，glibc需要a0中存入的是动态链接器的析构函数的地址！这将在atexit中得到注册！  
先前execve一直return argc， argc == 3！！！！  这和先前的定义是不一样的！

``` c
  return argc; // this ends up in a0, the first argument to main(argc, argv)
```

### libc.5 

START thread 10 syscall 64: sys_write
test_execvethread 10 syscall 64: sys_write
 ==========
thread 10 syscall 221: sys_execve
[LOG][fs/exec.c,244,execve] execve abs_path: /musl/basic/test_echo, path test_echo, cinfo.path /musl/basic
thread 10 kerneltrap: page fault at 0x0000003ffffea000
sepc=0x000000008022a896 stval=0x0000003ffffea000
scause=0x000000000000000f
sstatus=0x8000000000006100
satp=0x80000000000a01ff
panic: kerneltrap: page fault
3ffffe8000
0x0000003ffffe6000
kernel 里访问了一个0x3f未映射的地址，然后一直对栈递减，最后call了kerneltrap
难道是访问到了guard page??
和调用的速度和具体的测试程序无关，跑几个测试程序就在固定第N个崩溃。

execve->ext4_vfopen->ext4_raw_inode_fill->ext4_generic_open2->ext4_fs_get_inode_ref->__ext4_fs_get_inode_ref->ext4_fs_get_block_group_ref->ext4_trans_block_get->ext4_block_get->ext4_block_get_noread->ext4_block_cache_shake->ext4_block_flush_buf(->buf->end_write(bc, buf, r, buf->end_write_arg);?->jbd_write_sb)->ext4_blocks_set_direct->ext4_bdif_bwrite->int r = bdev->bdif->bwrite(bdev, buf, blk_id, blk_cnt);

有时在endwrite时崩溃了，有时在bwrite时崩溃了。。  
但是我之前测，哪怕没有修改为legacy磁盘时，也会崩  

0x8020126c这条指令跳到了release函数里,里面的popoff  
![libc.5.1](image/image-166.png)
![libc.5.2](image/image-167.png)
![libc.5.2](image/image-168.png)
这里的栈好像炸了。。  
[WARN][sched/thread.c,111,alloc_thread] thread 21 alloc with kstack 0x0000003ffffd3000  
我只给每个线程开了一个页大小的内核栈，这个地方已经超出了一个页，所以访问到了一个guard page  
那么怎么增加内核栈的大小呢？    
但是在这之前，第一个kernelvec似乎已经进入kerneltrap了，是在kerneltrap里崩的，这也许是两个问题  
第一个是Supervisor external interrupt，这个没问题.  
所以应该是第一次不知何种原因有一个外部中断（但是应该是正常的），随后尝试在kerneltrap中处理的时候爆栈了，这个时候又跳到了kernelvec, 栈一直往下递减直到越过了guard page，到达下一个线程内核栈的时候输出了trap信息，但是此时的信息和原本的原因已经相差甚远了。    
但是这样的话，我之前尝试映射guard page为什么还是跑不了？？试一下。     
居然可以跑到panic未知devintr那里！  
unknow devintr()
scause 0x000000000000000c
sepc=0x00000000802361e8 stval=0x00000000802361e8
panic: kerneltrap  

unknow devintr()
scause 0x000000000000000c
sepc=0x0000000080337eb0 stval=0x0000000080337eb0
panic: kerneltrap
QEMU: Terminated

unknow devintr()
scause 0x000000000000000c
sepc=0x0000003ffffd2fc0 stval=0x0000003ffffd2fc0
panic: kerneltrap

unknow devintr()
scause 0x000000000000000c
sepc=0x0000003ffffd2f90 stval=0x0000003ffffd2f90
panic: kerneltrap

居然每次trap的sepc都不一样。。。。。。。。  
[LOG][fs/exec.c,244,execve] execve abs_path: /musl/basic/mount, path ./mount, cinfo.path /musl/basic
[LOG][fs/blockdev.c,95,blockdev_bwrite] blockdev_bwrite: bdev 0x0000000080233f38, buf 0x000000009fe9a000, blockid 6032, blk_cnt 8
[LOG][fs/blockdev.c,100,blockdev_bwrite] blockdev_bwrite: writing block 6032, i = 0
[LOG][fs/blockdev.c,100,blockdev_bwrite] blockdev_bwrite: writing block 6033, i = 1
[LOG][fs/blockdev.c,100,blockdev_bwrite] blockdev_bwrite: writing block 6034, i = 2
[LOG][fs/blockdev.c,100,blockdev_bwrite] blockdev_bwrite: writing block 6035, i = 3
[LOG][fs/blockdev.c,100,blockdev_bwrite] blockdev_bwrite: writing block 6036, i = 4
[LOG][fs/blockdev.c,100,blockdev_bwrite] blockdev_bwrite: writing block 6037, i = 5
[LOG][fs/blockdev.c,100,blockdev_bwrite] blockdev_bwrite: writing block 6038, i = 6
[LOG][fs/blockdev.c,100,blockdev_bwrite] blockdev_bwrite: writing block 6039, i = 7
[LOG][fs/blockdev.c,95,blockdev_bwrite] blockdev_bwrite: bdev 0x0000000080233f38, buf 0x00000000802e90d0, blockid 3932160, blk_cnt 2
[LOG][fs/blockdev.c,100,blockdev_bwrite] blockdev_bwrite: writing block 3932160, i = 0
[LOG][fs/blockdev.c,100,blockdev_bwrite] blockdev_bwrite: writing block 3932161, i = 1
[LOG][fs/exec.c,271,execve] ph.type == 0x6
[WARN][fs/exec.c,269,execve] ELF_PROG_INTERP not supported

[LOG][fs/exec.c,271,execve] ph.type == 0x3
[LOG][fs/exec.c,271,execve] ph.type == 0x70000003
[LOG][fs/exec.c,271,execve] ph.type == 0x2
[LOG][fs/exec.c,271,execve] ph.type == 0x4
[LOG][fs/exec.c,271,execve] ph.type == 0x6474e551

不过此时其实已经open成功了，会不会是因为前面并没有真的好好安排内核栈，只是映射了guard page导致栈被写坏了。（真的吗？》。。。）  
先把内核栈安排好再考虑这件事情吧   
[WARN][sched/thread.c,111,alloc_thread] thread 2 alloc with kstack 0x0000003fffff0000  这内核栈地址不太对劲吧？这位置应该是trampoline的吧。  
不对 trampoline应该是0x3ffffff000

new :
[LOG][mm/vm.c,592,map_ustack] map_ustack: stack_low_aligned 0x0000000000166000, stack_high 0x00000000001a6000, pages 63
[LOG][fs/exec.c,320,execve] execve: sz = 0x0000000000166000, sp = 0x00000000001a6000, stackbase = 0x0000000000167000

old:
[LOG][fs/exec.c,309,execve] uvmalloc sz = 0x0000000000166000, sz + 64 * PGSIZE = 0x00000000001a6000
[LOG][fs/exec.c,322,execve] execve: sz = 0x00000000001a6000, sp = 0x00000000001a6000, stackbase = 0x0000000000167000


[LOG][sched/proc.c,450,userinit] userinit: proc 1, pagetable 0x000000009fef4000, sz 0x0000000000041000, initcode_len 3950, sz - 64 * PGSIZE: 0x0000000000001000

[LOG][sched/proc.c,708,do_clone] do_clone: old proc 1, sz 0x0000000000041000, oldt->trapframe->sp 0x0000000000040ff0

[LOG][sched/proc.c,709,do_clone] do_clone: np 2, sz 0x0000000000041000, t->trapframe->sp 0x0000000000040ff0

[LOG][fs/sysfile.c,609,sys_execve] sys_execve: path = /musl/busybox, uargv = 0x0000000000000d70, uenvp = 0x0000000000000f70



with and without allocating stack for initproc:

[LOG][sched/proc.c,708,do_clone] do_clone: old proc 1, sz 0x0000000000041000, oldt->trapframe->sp 0x0000000000040ff0
[LOG][sched/proc.c,709,do_clone] do_clone: np 2, sz 0x0000000000041000, t->trapframe->sp 0x0000000000040ff0
thread 1 syscall 3: sys_wait
thread 2 syscall 49: sys_chdir
thread 2 syscall 221: sys_execve
[LOG][fs/sysfile.c,609,sys_execve] sys_execve: path = /musl/busybox, uargv = 0x0000000000000d70, uenvp = 0x0000000000000f70
[LOG][fs/sysfile.c,618,sys_execve] sys_execve: fetching envp[0] from 0x0000000000000f70
[LOG][fs/sysfile.c,633,sys_execve] try to fetch envp[0] from 0x0000000080310d08


[LOG][sched/proc.c,708,do_clone] do_clone: old proc 1, sz 0x0000000000001000, oldt->trapframe->sp 0x0000000000000ff0
[LOG][sched/proc.c,709,do_clone] do_clone: np 2, sz 0x0000000000001000, t->trapframe->sp 0x0000000000000ff0
thread 1 syscall 3: sys_wait
thread 2 syscall 49: sys_chdir
thread 2 syscall 221: sys_execve
[LOG][fs/sysfile.c,609,sys_execve] sys_execve: path = /musl/busybox, uargv = 0x0000000000000d70, uenvp = 0x0000000000000f70
[LOG][fs/sysfile.c,618,sys_execve] sys_execve: fetching envp[0] from 0x0000000000000f70

难道是栈和数据段重合了？？
0xf70

[LOG][sched/proc.c,450,userinit] userinit: proc 1, pagetable 0x000000009fef4000, sz 0x0000000000003000, initcode_len 0x0000000000000f6e, sz - 64 * PGSIZE: 0xfffffffffffc3000
好像是uvmfirst的问题。。。（哈哈，AI改的代码）  
接下来思考怎么映射大于1个页大小的代码就好啦  

### dynamic-linking

zoe@sreyuim:~/rexvapor$ ls ./mnt/glibc/lib
dlopen_dso.so                libc.so    libm.so    tls_align_dso.so        tls_init_dso.so
ld-linux-riscv64-lp64d.so.1  libc.so.6  libm.so.6  tls_get_new-dtv_dso.so
zoe@sreyuim:~/rexvapor$ ls ./mnt/musl/lib
dlopen_dso.so  libc.so  tls_align_dso.so  tls_get_new-dtv_dso.so  tls_init_dso.so

0xc5b08 _dlstart
0xc5b24 _dlstart_c
0xc82d8 __dls2
0xc6d30 reloc_all
0xc6398 do_relocs
0xC5fa4 find_sym

0xC5D54 sysv_lookup
a0: 0x101210

AT_PHDR 传错了！！！=-=  



### testing

local:  

OpenSBI v1.5.1
   ____                    _____ ____ _____
  / __ \                  / ____|  _ \_   _|
 | |  | |_ __   ___ _ __ | (___ | |_) || |
 | |  | | '_ \ / _ \ '_ \ \___ \|  _ < | |
 | |__| | |_) |  __/ | | |____) | |_) || |_
  \____/| .__/ \___|_| |_|_____/|____/_____|
        | |
        |_|

Platform Name             : riscv-virtio,qemu
Platform Features         : medeleg
Platform HART Count       : 1
Platform IPI Device       : aclint-mswi
Platform Timer Device     : aclint-mtimer @ 10000000Hz
Platform Console Device   : uart8250
Platform HSM Device       : ---
Platform PMU Device       : ---
Platform Reboot Device    : syscon-reboot
Platform Shutdown Device  : syscon-poweroff
Platform Suspend Device   : ---
Platform CPPC Device      : ---
Firmware Base             : 0x80000000
Firmware Size             : 327 KB
Firmware RW Offset        : 0x40000
Firmware RW Size          : 71 KB
Firmware Heap Offset      : 0x49000
Firmware Heap Size        : 35 KB (total), 2 KB (reserved), 11 KB (used), 21 KB (free)
Firmware Scratch Size     : 4096 B (total), 416 B (used), 3680 B (free)
Runtime SBI Version       : 2.0

Domain0 Name              : root
Domain0 Boot HART         : 0
Domain0 HARTs             : 0*
Domain0 Region00          : 0x0000000000100000-0x0000000000100fff M: (I,R,W) S/U: (R,W)
Domain0 Region01          : 0x0000000010000000-0x0000000010000fff M: (I,R,W) S/U: (R,W)
Domain0 Region02          : 0x0000000002000000-0x000000000200ffff M: (I,R,W) S/U: ()
Domain0 Region03          : 0x0000000080040000-0x000000008005ffff M: (R,W) S/U: ()
Domain0 Region04          : 0x0000000080000000-0x000000008003ffff M: (R,X) S/U: ()
Domain0 Region05          : 0x000000000c400000-0x000000000c5fffff M: (I,R,W) S/U: (R,W)
Domain0 Region06          : 0x000000000c000000-0x000000000c3fffff M: (I,R,W) S/U: (R,W)
Domain0 Region07          : 0x0000000000000000-0xffffffffffffffff M: () S/U: (R,W,X)
Domain0 Next Address      : 0x0000000080200000
Domain0 Next Arg1         : 0x00000000bfe00000
Domain0 Next Mode         : S-mode
Domain0 SysReset          : yes
Domain0 SysSuspend        : yes

Boot HART ID              : 0
Boot HART Domain          : root
Boot HART Priv Version    : v1.12
Boot HART Base ISA        : rv64imafdch
Boot HART ISA Extensions  : sstc,zicntr,zihpm,zicboz,zicbom,sdtrig,svadu
Boot HART PMP Count       : 16
Boot HART PMP Granularity : 2 bits
Boot HART PMP Address Bits: 54
Boot HART MHPM Info       : 16 (0x0007fff8)
Boot HART Debug Triggers  : 2 triggers
Boot HART MIDELEG         : 0x0000000000001666
Boot HART MEDELEG         : 0x0000000000f0b509

reXvapor kernel is booting  

testcase busybox ls success
testcase busybox sleep 1 success
'#### file opration test'
testcase busybox echo "#### file opration test" success
testcase busybox touch test.txt success
panic: filewrite

[LOG][fs/sysfile.c,919,generic_open] generic_open : path /musl/busybox_cmd.bak successfully opened, type = 6
thread 16 syscall 25: sys_fcntl
thread 16 syscall 61: sys_getdents64
thread 16 kerneltrap: page fault at 0x0000000000000028
sepc=0x000000008020a150 stval=0x0000000000000028
scause=0x000000000000000d
sstatus=0x8000000200046120
satp=0x80000000000a01ff
t->kstack=0x0000003fffbef000
panic: kerneltrap: page fault  

好像只有busybox文件系统操作的时候会出问题。主要涉及创建和删除的时候。 远程平台应该在cat这个文件，但是还没结束就崩了。

[LOG][fs/sysfile.c,939,sys_openat] [sys_openat] abs_path = /musl/test.txt, flags = 0x8000, omode = 0x0
[LOG][fs/sysfile.c,919,generic_open] generic_open : path /musl/test.txt successfully opened, type = 2
[LOG][fs/sysfile.c,950,sys_openat] [sys_openat] generic_open success, fd = 3, path = /musl/test.txt, f 0x0000000080239478, f->fpos 0, flags 8000
thread 7 syscall 71: sys_sendfile
[LOG][fs/sysfile.c,1565,do_sendfile] do_sendfile: kbuf = 0x000000009ed67000, kbuf size = 4096
hello world
thread 7 kerneltrap: page fault at 0x0000003effffff88
sepc=0x000000008021d830 stval=0x0000003effffff88
scause=0x000000000000000d
sstatus=0x8000000200046120
satp=0x80000000000a01ff
t->kstack=0x0000003fffe38000
panic: kerneltrap: page fault

哇，改了sendfile符合要求之后居然在远程平台同款的位置崩了！  
每次崩的位置都不一样

thread 3 kerneltrap: page fault at 0x0000003effffffa8
sepc=0x000000008021d6a2 stval=0x0000003effffffa8
scause=0x000000000000000d
sstatus=0x8000000200046120
satp=0x80000000000a01ff
t->kstack=0x0000003ffff3c000
panic: kerneltrap: page fault


thread 3 kerneltrap: page fault at 0x0000003effffffa8
sepc=0x000000008021d6a2 stval=0x0000003effffffa8
scause=0x000000000000000d
sstatus=0x8000000200046120
satp=0x80000000000a01ff
t->kstack=0x0000003ffff3c000
panic: kerneltrap: page fault

正确的s0应该是: 0x3ffff7bf40  
fileread() sp : 0x3ffff7be40
0x802064fc 栈里的内容不对！

0x8020639c
rw_sharp()->fileread()开始往栈里存了a0，但是结束之后栈里内容不对。另外，改了编译等级之后发现输出不了hello world就崩了，怀疑是内核栈的问题
![testing.1](image/image-170.png)
0x8020edc2修改了栈里寸的s0 ...

![alt text](image/image-171.png) 

我好像找到问题所在在了 ,看rcnt类型。。。这样清空内存会多清4个字节。。。

``` c

typedef long unsigned int size_t;

int ext4_vfread(struct file *fp, int user_dst, uint64 dst, uint off, uint size, int *rcnt) {
    int r = EOK;
    struct ext4_file *efp = fp->private_data;
    char *kbuf = NULL;
    if(!efp) {
        printf("[ext4] efp is NULL!\n");
        return EINVAL;
    }
    if((r = ext4_fseek(efp, off, SEEK_SET)) != EOK) {
        printf("[ext4] ext4_fseek error! r=%d\n", r);
        return r;
    }
    if(user_dst) {
        kbuf = (char *)kmalloc(size);
        if(!kbuf) {
            // printf("[ext4] kmalloc error!\n");
            return ENOMEM;
        }
    } else {
        kbuf = (char *)dst;
    }
    #ifdef __DEBUG_EXT4_VFREAD
    Log("[ext4] ext4_vfread: kbuf = %p, dst = %p, size = %d", kbuf, dst, size);
    #endif
    if((r = ext4_fread(efp, kbuf, (size_t)size, (size_t*) rcnt)) != EOK) {
```  
