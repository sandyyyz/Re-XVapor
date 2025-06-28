
---

# 开发与调试日志汇总

本文档汇总了在内核功能开发过程中遇到的关键问题、错误类型、触发条件、分析过程和解决方案。旨在为后续调试、系统维护和功能扩展提供参考依据。

---

## 线程系统相关 Bug 汇总

### 1. panic: `p->thread == NULL`

* **触发场景**：`kerneltrap` 中中断触发时，当前 CPU 的线程指针为 `NULL`。
* **原因**：scheduler 开启中断后立即触发时钟中断，导致 `mythread()` 为空。
* **解决方案**：

  ```c
  if(which_dev == 2 && mythread() != 0 && mythread()->state == TCB_RUNNING) {
    myproc()->ktime++;  
    thread_yield();
  }
  ```

---

### 2. `q == NULL` in `tcb_q_change_state`

* **原因**：未设置 `RUNNING` 状态的队列，但错误地尝试将其从队列中移除。
* **解决方案**：`RUNNING` 状态不依赖于队列，因为运行中的线程由 CPU 持有。

---

### 3. Lost Wakeup

* **原因**：队列遍历代码写错字段名。
* **解决方案**：修正结构体成员名称使用错误。

---

### 4. 用户态 `sret` 后立即异常

* **原因**：用户栈未能正确映射，或中断处理写栈导致页错误，进入死循环。
* **解决方案**：暂时避免在 `uservec` 写入栈，使用 `sscratch` 传递 `trapframe`。

---

### 5. `exec` 返回失败，`trapframe` 未映射

* **解决方案**：确保在 `exec` 重新分配 `trapframe` 时正确建立页表映射。

---

### 6. `exec` 返回用户态后陷入 `usertrap` 死循环

* **疑点**：反复进入陷阱可能是 `syscall` 重入或调试信息误导。
* **解决方案**：确认 `sys_exec` 及 `usertrapret` 的调用流程。

---

### 7. `fork` 后 panic: `sched t locks`

* **原因**：`thread_exit()` 中未释放进程组 leader 的锁。
* **解决方案**：整理 `fork` 后线程锁的释放流程。

---

### 8. `usertrapret()` 卡住，`usertrap: not from user mode`

* **原因**：用户返回后立即陷入 trap，可能是栈权限错误。
* **解决方案**：避免在 `uservec` 中写栈，改由 `sscratch` 传递 trapframe。

---

### 9. `thread_exit` 期间持锁调用 `thread_sleep` 导致 panic

* **解决方案**：在 `thread_exit()` 中释放锁后再调用 `proc_exit()`。

---

### 10. `freewalk: leaf` panic during `thread_exit`

* **原因**：页表遍历过程中遇到非法 `pte`，多见于 `copyin/copyinstr`。
* **备注**：偶发错误，可能是并发或释放时机导致。

---

### 11. `acquire` panic during `killstatus` 测试

* **推测**：并发调用中出现未预期的锁重入或未释放。

---

### 12. `load page fault` during `reparent` 测试

* **原因**：`thread` 已被释放但 `proc` 尚未完全退出。
* **解决方案**：新增 `lth_exitlock` 锁，保证退出流程完整。

---

### 13. `reparent` 测试中 `panic: kerneltrap`（偶发）

* **推测**：释放与切换时存在竞态。

---

### 14. `exitiput` 测试中 `panic: sched t locks`

* **原因**：`thread_exit()` 时 `c->noff != 0`
* **解决方案**：确保 `iput()` 前 `noff == 0`

---

### 15–17. 并发压力测试中偶发 panic 或死锁

* **表现**：`writebig`, `writetest` 多线程并发下系统死锁或崩溃。
* **建议**：加入更多 deadlock 检测与 lost wakeup 日志。

---

## 📦 mmap 模块相关问题

### mmap.1

* **panic**: `uvmcopy: pte should exist`
* **原因**：初始 state\_list 结构未正确初始化，导致页表不完整。

---

### mmap.2

* **panic**: `log_write outside of trans`
* **原因**：mmap 写回时未使用文件接口，而直接使用 inode 层操作。
* **解决方案**：改用 `filewrite()` 替代 `iwrite()`。

---

### mmap.3

* **panic**: `uvmunmap: not mapped`
* **原因**：VMA 尚未触发 page fault，未实际分配页。
* **解决方案**：在 `uvmunmap` 前检查是否存在有效 `pte`。

---

以下是你所提供内容整理为 `.md` 格式的文档版本，可直接保存为 `ext4_debug_notes.md`：

---


##  ext4

###  ext4.1 - 奇怪的 block_group

![ext4.1](image/image-111.png)

- 问题现象：
  `ext4_fs_init_inode_bitmap(struct ext4_block_group_ref *bg_ref)` 中计算出的 `bitmap_block_addr` 数值异常大，最终导致：

```c
  ext4_trans_block_get_noread()
```
报错。

* 问题根源：
  这个 block\_group 的元数据是错误的，**checksum 检查时已经发出 warning**，说明数据早已损坏。

* 错误调用路径：

  ```c
  static int __ext4_fs_get_inode_ref(struct ext4_fs *fs, uint32_t index,
                                     struct ext4_inode_ref *ref,
                                     bool initialized)
  ```

  会调用：

  ```c
  int ext4_fs_get_block_group_ref(struct ext4_fs *fs, uint32_t bgid,
                                  struct ext4_block_group_ref *ref)
  ```

  而只要调用该函数就会出现异常。

* 进一步调用链：

  ```c
  int ext4_block_get(struct ext4_blockdev *bdev, struct ext4_block *b,
                     uint64_t lba)
  {
      int r = ext4_block_get_noread(bdev, b, lba);
      ...
  }
  ```

* 根本原因：

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

  >  **kalloc() 分配了未初始化的页，并被填充为 0x05。** 导致后续对结构体字段解释出错，间接引发了 bitmap\_block\_addr 的异常。

---

## init.1 - execve 卡死问题

![init.1.1](image/image-126.png)

* 问题现象：
  执行 `/init` 时卡死，陷入无法恢复的状态。

* 可疑提示：
  ![init.1.2](image/image-127.png)
  ![init.1.3](image/image-128.png)

  * 推测：可能与 alignment 异常或数据不一致有关。
  * 最终定位到函数：

    ```c
    ext4_bdif_bread()
    ```

* 卡死追踪过程：
  ![init.1.4](image/image-142.png)
  ![init.1.5](image/image-143.png)
  ![init.1.6](image/image-144.png)

  > 最终确认问题出现在磁盘读路径中。

  ![init.1.7](image/image-145.png)

* 根本原因：**lost wakeup** 导致线程卡在 sleep 无法唤醒。

---

## Lost Wakeup 深度剖析

![init.1.8](image/image-146.png)

* 你重构了 wakeup/sleep 实现方式，不再使用 xv6 原本的 `p->lock` + 条件锁机制，而是改为：

  > 遍历 sleeping queue 进行 wakeup。

* 这带来了一个竞态窗口：

  * sleep 的线程还未设置 `state = SLEEPING`。
  * wakeup 已经开始扫描 queue，此时还找不到它。
  * sleep 设置完成后就永远没人再来唤醒它 → 卡死。

---

### 原设计中条件锁的精妙之处

> 正确的 sleep/wakeup 实现，需要如下条件保证同步原子性：

* **sleep 步骤：**

  1. 持有线程锁（`t->lock`）
  2. 修改线程状态为 `SLEEPING`
  3. 调用 `sched()`
  4. **再释放条件锁**

* **wakeup 步骤：**

  1. 必须先持有条件锁
  2. 遍历 queue
  3. 获取目标线程锁
  4. 如果发现线程为 `SLEEPING`，则设置为 `RUNNABLE`

* 若无“条件锁 + 线程锁”双锁协作，必然会导致 lost wakeup。

---

>  **总结：**
> wakeup 必须持有 sleep 的条件锁，并且 wakeup 的判断/唤醒过程必须与 sleep 设置状态的过程“锁定在同一个原子区间”，否则就会出错。

---


#  BusyBox 调试笔记

---

##  busybox.1 - 栈布局

![busybox.1.1](image/image-112.png)
![busybox.1.2](image/image-113.png)

- 修改了栈布局以适配 `__libc_start_main` 调用流程：
  - [LSB libc_start_main 文档](https://refspecs.linuxbase.org/LSB_3.1.0/LSB-generic/LSB-generic/baselib---libc-start-main-.html)
  - [StackOverflow 讨论](https://stackoverflow.com/questions/62709030/what-is-libc-start-main-and-start)
  - [程序启动流程分析](http://dbp-consulting.com/tutorials/debugging/linuxProgramStartup.html)

---

##  busybox.2 - `__libc_setup_tls`

![busybox.2.1](image/image-119.png)
![busybox.2.2](image/image-118.png)

- 调用 `__libc_setup_tls` 时会尝试 syscall 214，即 `SYS_brk`。
- 添加对该 syscall 的实现即可解决。

---

##  busybox.3 - brk 导致 syscall 17 卡死

![busybox.3.1](image/image-120.png)
![busybox.3.2](image/image-121.png)

- brk 后最终调用 syscall 17（`SYS_getpid`）卡死。
- 在这之前还有很多 syscall 均失败。
- 解决思路：一个个修复。

---

##  busybox.4 - 写入代码段地址？

![busybox.4.1](image/image-122.png)
![busybox.4.2](image/image-123.png)

- 可疑现象：代码似乎尝试写入代码段地址，需进一步确认 ELF 映射与保护策略。

---

##  busybox.5 - shell 不断重启 `init`

- `exit_group()` 后 busybox 似乎不断重启。
- `ash_main` 没有成功解析命令，导致退出。
- 实际上对于非 `login_sh` 情况，也**不需要 `/dev/tty`**。
- 命令解析流程：
  - `cmd_loop()` → `parsecmd()`
  - `ls_main()` → `scan_and_display_dir_cur()`

---

##  busybox.6 - getdents 问题

![busybox.6.1](image/image-129.png)

- `getdents` 传入的 `f->dir == NULL`，无法遍历目录。
- 需要确保打开目录文件后设置 `fp->dir` 正确初始化。

---

##  busybox.7 - open 返回值错误

![busybox.7.1](image/image-130.png)
![busybox.7.2](image/image-131.png)

- 错误地将 `open` 的返回值设为 0，导致混淆 stdin。
- 正确实现：`open` 返回新分配的最小可用 fd。

---

##  busybox.8 - `ls` 一直 `getdents`

![busybox.8.1](image/image-132.png)

- 疑问：`ls` 何时知道目录遍历结束？
- 正解：
  - `getdents` 返回 0 表示读取完成。
  - `ext4_dir_entry_next()` 会更新 `dir->off`。
  -  错误写法：传入临时变量 `dir`，未更新 `fp->dir->off`。

---

##  busybox.9 - `ls_main()` 中 `nfiles == 0`

![busybox.9.1](image/image-133.png)  
...（省略中间若干图）

- 问题：
  - 多次陷入 `__bswapdi2()` / `__run_exit_handler()`。
- 猜测：
  - clone 的 flags 不规范？
- 建议：
  - 先稳定运行 `musl-busybox`。
  - 对于这种异常 `utrap`，考虑直接杀进程而非 panic。

---

##  busybox.10 - ls 写入 0 字节

![busybox.10.1](image/image-147.png)

- 诊断为非法 vector。
- 跳过非法 vector 即可恢复。

---

##  busybox.11 - shell 无法退出

![busybox.11.1](image/image-148.png)
![busybox.11.2](image/image-149.png)
...

- `parsecmd()` 重复解析同一条命令 `mnb`。
- 原因：
  - `fp->pos` 未更新（见 busybox.12）。

---

##  busybox.12 - 文件无限复制到 stdout

![busybox.12.1](image/image-153.png)
![busybox.12.2](image/image-154.png)

- 问题：`sendfile` 拷贝循环不止。
- 原因：未更新 `fp->pos`，导致重复读取相同内容。
-  这也可能导致 shell 无限解析同一指令。

---

##  busybox.13 - 爆栈怀疑

![busybox.13.1](image/image-155.png)
![busybox.13.2](image/image-156.png)

- `s0` 回来时值不一致。
- 怀疑栈越界，需使用更大页或检查 `trapframe` 保存/恢复逻辑。

---

##  busybox.14 - `sleep 1` 参数异常

![busybox.14.1](image/image-158.png)

- 传入参数为 `0x7fffffff`，即 `INT_MAX`。
- 可在用户态打印参数确认来源。

---

##  busybox.15 - 睡眠与中断问题

- `sleep 5` 时 `ticks == 0`
- 原因：
  - 启动核未必为 0 核，tick counter 未更新。

---

## busybox.16 - syscall & futex & signal

- `ppoll` syscall 未实现 → **已修复**。
- `sendfile`、`wait4`、`clone`、`exit_group` 等 syscall 路径调通。
- futex 实现中打出：

 ```c
  [WARN] futex not found for address ...
```

* 如果 futex 不存在可以考虑跳过唤醒。

---

### musl busybox 测试

* 多个测试失败原因：

  * 缺少 `/proc`, `/dev/null`, `/dev/tty` 等虚拟节点。
  * 建议提前构造最小 `/proc`, `/dev` 模拟挂载或手动绕过。

---

###  glibc busybox

* 运行时会尝试访问 `/usr/lib/.../gconv/...`
* 不存在相关文件时仍崩溃，怀疑原因：

  ```c
  struct lc_ctype_data *data = new_category->private;
  ```

  * `private == NULL`，访问 `data->fcts` 崩溃。
  * 需要手动模拟 locale 初始化。

---


---

# libc-test 调试笔记整理

---

## libc-test-static

### libc.1 - 页错误处理与 futex\_copyin

* **页表错误信息**：

  * `&pte = 0x9fb82fa0`
  * `pgtable addr: 0x9fb80000`
  * `va 0x0000003fffff4000`, size `0x1000`

* **futex\_copyin**：

  * `pgtable addr: 0x9fb80000`
  * `pte = 0`

---

### libc.2 - `pthread_cancel_points()` 测试失败

* syscall trace：

  * 多线程创建及执行流程（clone、execve 等）均执行正常。
  * futex 系统调用执行过程包含：

    * `futex_wait` 线程进入睡眠。
    * `futex_wake` 执行后提示 futex 地址未注册。

* 报错日志：

  * `signal_handle` 显示线程 5 有 pending signal。
  * `signal_default` 对信号 33 未实现默认行为。
  * futex 错误地址：

    * `0x3fffff4b48`
    * `0x000000000009db80`

* 分析结论：

  * futex 唤醒阶段找不到等待队列，可能是线程管理或 futex 哈希表清理逻辑出错。
  * 最终测试 `pthread_cancel_points` 返回 status 255，标记为失败。

---

### libc.3 - `RLIMIT_STACK` 相关栈限制问题？

* 错误日志截图指示疑似栈上限问题。
* 待验证是否是因为缺乏 `RLIMIT_STACK` 支持。

---

### libc.4 - 尚未支持的测试项汇总

#### Musl static 失败列表（14/107）：

```
pthread_tsd
pthread_cond
setjmp
socket
stat
utime
fflush_exit
pthread_robust_detach
pthread_cancel_sem_wait
pthread_cond_smasher
pthread_once_deadlock
syscall_sign_extend
pthread_rwlock_ebusy
pthread_cancel_points
```

#### Glibc static 失败列表（45/107）：

示例（节选）：

```
clocale_mbfuncs
fdopen
fnmatch
fwscanf
mbc
pthread_cancel
...
syscall_sign_extend
uselocale_0
```

---

### libc.4.1 - Glibc 程序崩溃分析

* 崩溃发生在调用 `__run_exit_handlers()` 的过程中。
* `__exit_funcs` 链表中注册了两个析构函数，但一个值为 `0x3`，不合法。
* 原因定位：

  * `_start()` 函数中，`a0` 被传给 `__libc_start_main`，作为 atexit 的回调。
  * 然而该值被误设置为 `argc`，导致注册了一个非法指针（3）为析构函数。
* **解决方向**：

  * 修正 `execve` 返回值不应直接传 `argc`，应为 NULL 或合法函数指针。

---

## libc.5 - 栈崩溃与 guard page

* 日志显示在执行 syscall `execve` 后进入 `kerneltrap`，崩溃地址：

  * `0x0000003ffffea000` 等不同值（多次 trap 后地址不同）

* 崩溃堆栈显示为：

  * 线程使用的内核栈超出一页，访问到 guard page 后触发 page fault。

* 分析路径（精简）：

  ```
  execve
    -> ext4_vfopen
      -> ext4_raw_inode_fill
        -> ...
          -> ext4_block_flush_buf
            -> end_write(bc, buf, ...)
  ```

* 栈溢出原因为：

  * 每个线程仅分配一页内核栈。
  * 某些内核路径中调用深度过深，栈空间不足。

* **处理**：

  * 增大内核栈分配页数。
  * 增加栈边界 guard page 后，已经能看到非法中断 `devintr`，说明越界捕获生效。

---

## 用户栈与数据段重合分析

* 栈与程序数据重合日志片段：

  ```
  uargv = 0x0000000000000d70
  uenvp = 0x0000000000000f70
  sz = 0x0000000000003000
  ```

* 分析：

  * `initcode_len = 0xf6e`，说明整个用户空间只映射了一页，可能造成 `argv[]` 和 `envp[]` 越界。

* 可疑逻辑：

  * `uvmfirst()` 只映射了一页，导致栈空间过小。
  * AI 修改了 `uvmfirst()`，可能存在逻辑错误。

---

# Bug Logs — reXvapor OS Debug Notes

---

## \[BUG #001] 动态链接错误：传入错误的 AT\_PHDR 值

* **模块**：Dynamic Linking (ld.so)
* **描述**：

  * 在动态链接器启动阶段，`AT_PHDR` auxiliary vector 传入了错误地址 `a0 = 0x101210`。
  * 导致 `sysv_lookup()` 中访问异常。
* **调用栈**：

  ```
  _dlstart
  └── _dlstart_c
      └── __dls2
          └── reloc_all
              └── do_relocs
                  └── find_sym
                      └── sysv_lookup
  ```
* **解决建议**：

  * 检查 `execve()` 中 auxv 构建逻辑，确保 `AT_PHDR` 使用 ELF Program Header 的正确地址。

---

## \[BUG #002] 内核文件操作异常：sendfile 崩溃（段错误）

* **模块**：VFS / `sendfile()`
* **描述**：

  * 在调用 `sendfile()` 过程中，线程陷入 `kerneltrap`，多次 page fault，地址形如：

    ```
    page fault at 0x0000003effffffa8
    sepc=0x8021d6a2
    ```
  * 日志显示崩溃发生在 `fileread()` 函数调用后的栈恢复过程中，寄存器 s0 被破坏。
* **可疑现象**：

  * 栈内容异常：返回地址附近保存的 `s0` 被覆盖。
  * 不同优化等级下表现不同，开启 `-O2` 后无法输出 hello world 即崩。
* **原因分析**：

  * 初步怀疑为内核栈越界，或者 `rcnt` 类型与 `ext4_fread` 的签名不一致，导致返回值覆盖栈。
* **可疑代码**：

  ```c
  int ext4_vfread(..., int *rcnt) {
      ...
      (size_t*) rcnt  // 强制转换可能造成非法写入
  }
  ```
* **解决建议**：

  * 将 `rcnt` 类型更改为 `size_t *`。
  * 检查栈帧对齐与调用约定。
  * 增大线程内核栈页数，并添加 guard page。

---

## \[BUG #003] busybox 文件系统操作失败（文件创建/删除）

* **模块**：VFS / sys\_openat / sys\_sendfile
* **描述**：

  * busybox 中 `touch`、`cat` 等操作在进行文件写入或读取时崩溃。
  * 日志表明：

    ```
    generic_open : path /musl/test.txt successfully opened, type = 2
    sys_sendfile → page fault → panic: kerneltrap
    ```
* **稳定复现点**：

  * 使用 busybox 创建文件，然后用 `cat` 或 `sendfile()` 读取。
* **原因分析**：

  * 在 `sys_sendfile` → `fileread` → `ext4_fread` 中，读取栈或内存出错。
  * 与 \[BUG #002] 相似，可能共因于栈管理或类型错误。
* **解决建议**：

  * 合并与 BUG #002 的处理方案。
  * 增加 `sendfile()` 路径中 `fileread` 调试日志，确认调用约定与栈正确性。

---

## \[BUG #004] Glibc 程序崩溃：`__run_exit_handlers` 注册了非法析构函数

* **模块**：Dynamic Linking / libc
* **描述**：

  * 执行 `glibc` 程序时，程序退出触发 `__run_exit_handlers`，其中注册了一个值为 `0x3` 的函数指针，导致段错误。
* **调用栈简略**：

  ```
  _start()
  └── __libc_start_main(argc)  // 错误地传入了 argc 作为 exit handler
  ```
* **原因分析**：

  * `execve()` 实现错误地将 `argc` 作为函数指针返回给 `_start()`，传入 `__libc_start_main()`。
* **解决建议**：

  * `execve` 应返回合法的 `_start()` 入口或 NULL，不应传递整型参数。

---

## \[BUG #005] 内核栈空间不足导致页错误

* **模块**：内核栈调度与异常处理
* **描述**：

  * 多个测试中，线程在执行文件相关系统调用（如 `fileread`, `sendfile`）时陷入 kerneltrap：

    ```
    page fault at 0x3ffffea000 或其他接近 stack 顶部地址
    ```
  * 崩溃常发生在内核函数调用较深处，`kstack` 被完全用尽。
* **日志示例**：

  ```
  t->kstack = 0x3ffff3c000
  sepc = 0x8021d6a2
  ```
* **解决建议**：

  * 增大线程 `kstack` 分配页数（例如从 1 页增至 2\~4 页）。
  * 分配 guard page 用于检测栈溢出。

---

## \[BUG #006] 栈与数据段重叠（user\_stack 与 .data）

* **模块**：execve / ELF loader
* **描述**：

  * `uvmfirst()` 只分配了一页给用户程序，结果 `argv[]` 与 .data 段重叠。
* **现象**：

  * `argv = 0xd70`，`envp = 0xf70`，但 `.data` 段也位于低地址页。
* **解决建议**：

  * 为用户空间程序映射更多页，至少包括：

    * text
    * data
    * bss
    * 用户栈空间（向高地址增长）

---

## \[BUG #007] futex cancel 时找不到等待队列

* **模块**：futex 子系统
* **描述**：

  * 测试 `pthread_cancel_points` 失败，futex\_wake 找不到地址。
* **日志**：

  ```
  syscall futex_wake addr = 0x3fffff4b48
  hash entry not found
  ```
* **推测原因**：

  * 线程销毁后 futex 条目未正确清除或未插入。
* **解决建议**：

  * 审查 futex 等待队列的生命周期管理，特别是 `exit()` 后的清理逻辑。

---