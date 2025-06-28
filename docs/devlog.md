
---

### 1. 项目概述（Project Overview）

## 项目概述

本操作系统最初基于 MIT 的教学操作系统 xv6 开发，目标是逐步扩展其功能并向现代 Unix/Linux 内核行为靠拢。主要特性包括：

- 多线程支持（thread API）
- 兼容 POSIX 的信号系统
- futex支持
- 多进程调度与 syscalls 接入
- ELF 可执行文件加载
- 支持现代ext4文件系统
- musl / glibc 动态链接器兼容
- 用户空间堆栈、trapframe 恢复等完整上下文切换逻辑

---

### 2. 演化路线图（Evolution Timeline）

``` mermaid
timeline
    title Re-XVapor 演化路线图（2025 起）

    section 阶段一：xv6 基础重构
      学习xv6-riscv整体架构和具体实现: 2025-01-01
      移除用户空间程序与简化系统调用逻辑: 2025-01-25
      添加通用数据结构支持: 2025-01-30

    section 阶段二：线程系统实现
      引入 TCB 支持多线程: 2025-02-15
      实现 thread scheduler 与 join/exit/wakeup: 2025-03-20

    section 阶段三：ext4 文件系统支持
      引入 VFS 框架: 2025-04-17
      集成 lwext4 支持 ext4: 2025-05-16

    section 阶段四：POSIX 信号机制支持
      实现 sigaction/sigreturn 等 syscall: 2025-05-25
      支持用户态信号栈构建与恢复: 2025-06-04

    section 阶段五：兼容性增强
      支持 ELF 动态段解析与 auxv 构造: 2025-06-17
      支持动态链接器与 glibc busybox: 2025-06-25

    section 阶段六：现代应用与测试
      支持大赛 basic 测例: 2025-06-26
      支持 busybox 与 libctest: 2025-06-26

    section 阶段七：调试与稳定性提升
      修复死锁与页表问题: 2025-06-30
      增加日志与调试宏: 2025-06-30


```
## 演化阶段

### 阶段一：xv6 基础重构
- 移除原始 xv6 的用户空间程序与简化系统调用逻辑
- 添加一些通用的数据结构，为后续实现系统功能提供便利

### 阶段二：线程系统实现
- 引入 tcb（thread control block）结构体
- 支持一个进程中包含多个线程
- 自研 thread scheduler 与 join/exit/wakeup 逻辑

### 阶段三：ext4文件系统支持
- 引入vfs
- 移植lwext4

### 阶段四：POSIX 信号机制支持
- 实现 sigaction、sigprocmask、sigpending、sigreturn 等系统调用
- 支持信号 handler 的用户栈构建与上下文恢复
- 兼容 glibc/musl 提供的信号行为

### 阶段五：兼容性增强
- 支持动态链接器（libc.so）加载并运行 glibc busybox
- 支持完整 ELF 动态段解析、auxv 启动参数构造

### 阶段六：支持一些现代应用与测试样例
- 支持大赛basic测例
- 支持busybox测例
- 支持libctest

### 阶段七：调试与稳定性提升
- 修复内核死锁、信号丢失、页表映射混乱等问题
- 增加日志系统、调试宏（DEBUG_SIGNAL、DEBUG_TRAP等）

---

### 3. 架构设计（System Architecture）

``` mermaid
graph TD
  A["硬件平台<br>RISC-V 或 LoongArch"]

  subgraph HAL["硬件抽象层"]
    B["时钟控制器"] 
    C["中断控制器"] 
    D["串口驱动"]
    E["OpenSBI 接口"]
  end

  subgraph Kernel["内核服务层"]
    F["进程线程调度器<br>(sched)"]
    G["虚拟内存管理<br>(mm)"]
    H["VFS 层 + ext4 支持<br>(fs)"]
    I["IPC 机制<br>信号/信号量/Futex"]
    J["系统调用接口<br>(syscall.c)"]
    K["ELF 加载器 + 动态链接"]
  end

  subgraph User["用户接口层"]
    L["用户空间库<br>libc 或 musl"]
    M["动态链接器<br>ld.so"]
    N["Shell + Busybox"]
    O["用户程序（测试/应用）"]
  end

  A --> B
  A --> C
  A --> D
  A --> E

  B --> F
  C --> F
  D --> F
  E --> F

  F --> G
  F --> H
  F --> I
  F --> J
  F --> K

  F -->|调度与唤醒| I
  I -->|Futex/信号| G
  H -->|挂载/访问| G
  K --> L
  J --> L
  L --> M
  M --> N
  M --> O
``` 
## 系统架构

当前系统由以下核心模块组成：

- **内核层**
  - Trap/Exception 处理：trap.c
  - 线程调度：thread.c / scheduler.c
  - 信号系统：signal.c / signal.h
  - 虚拟内存系统：vm.c / pagetable.c
  - 文件系统：fs.c / file.c

- **用户层**
  - libc 实现：syscall stub / uapi
  - ELF loader 支持 musl / glibc 动态链接器
  - 用户线程栈自动构造与返回机制

模块之间通过结构体（如 proc、tcb、sigaction、sigpending）协作。

---

### 4. 关键改动与设计点（Design Highlights）

## 关键设计点

### 4.1 信号处理机制
- 支持默认处理、自定义 handler、忽略信号三种方式
- 通过信号 frame 构造完整的 ucontext，在 handler 返回后恢复上下文
- 支持 siginfo 与 SA_SIGINFO 行为（传入 handler 的额外参数）

### 4.2 Trap/信号切换流程
- 用户程序 trap → 内核检测 pending 信号 → 构建 signal frame → 切回用户 signal handler
- handler 返回后触发 sigreturn → 内核恢复上下文 → 继续执行原用户程序

### 4.3 支持 musl/glibc 的兼容性
- 实现 auxv、AT_SYSINFO、AT_ENTRY 等传参机制
- 处理 __restore_rt / __libc_start_main 的特殊调用

### 4.4 用户空间工具链适配
- 通过交叉编译方式构建 busybox+musl/glibc 二进制
- 自动生成 syscall stub 与动态链接入口

---

### 5. 问题记录与解决方案（Issues and Fixes）

## 遇到的问题与解决

开发中遇到的大大小小的bug不计其数，只选择一些难以调试或者具有代表性的bug进行列举和总结。  

> **debug原始资料见：
[debug.md](debug.md)**  

> **debug总结见
[debug_summary](debug_summary.md)**


### Bug #1 scheduler中空指针引用
- *描述*

---

### 6. 后续规划（Future Work）

## 后续目标

- [ ] 支持loongarch
- [ ] 支持更多测例
- [ ] 引入网络栈
- [ ] 支持多设备多文件系统挂载

---

### 7. 附录与参考（Appendix / References）

## 参考资料

- [xv6 book](https://pdos.csail.mit.edu/6.828/2023/xv6/book-rev11.pdf)
- [man7.org - signal(7)](https://man7.org/linux/man-pages/man7/signal.7.html)
- [musl source code](https://git.musl-libc.org/cgit/musl/)
- glibc `ucontext.h` 源码与 ABI 说明

---
