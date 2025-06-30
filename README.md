
# Re-XVapor: A Custom Operating System Based on xv6

## 项目简介

**Re-XVapor** 是一个正在开发中的类 Unix 操作系统，基于 [MIT xv6-riscv](https://github.com/mit-pdos/xv6-riscv) 深度改造而成。项目旨在探索和实现现代操作系统的核心功能，适配多核、多线程执行环境，具备良好的可移植性与模块化设计，适用于系统编程实验以及自定义内核功能开发。

截至目前：

- 实现并扩展了 **81 个系统调用**
- 支持运行 **glibc** 和 **musl-busybox** 的常用命令
- 支持动态链接、共享库、现代 ELF 加载机制
- 通过了 [libctest](https://github.com/sandyyyz/libctest) 框架的稳定性和功能性测试

---

## 项目地址

- **GitHub**  
  [https://github.com/sandyyyz/Re-XVapor](https://github.com/sandyyyz/Re-XVapor)

- **GitLab (教育平台)**  
  [https://gitlab.eduxiji.net/T202510183995907/Re-XVapor](https://gitlab.eduxiji.net/T202510183995907/Re-XVapor/-/tree/develop?ref_type=heads)

---

## 项目特点

Re-XVapor 在 xv6 基础上完成了大量系统级功能扩展与重构，主要特性包括：

- [x] 多核调度与 HART 管理
- [x] 进程家族树管理（支持 `getppid`、`waitpid`、`killpg` 等）
- [x] 支持内核级线程与 `clone` 系统调用
- [x] 支持 Futex，采用哈希表加速队列唤醒机制
- [x] 条件变量、信号量同步原语支持
- [x] 完善的信号机制：支持 signal mask、sigaction、信号队列等
- [x] 虚拟文件系统（VFS）架构 + ext4 文件系统挂载与访问
- [x] 支持用户态 ELF 动态链接，兼容 glibc/musl 程序
- [x] 支持管道、共享内存等进程间通信方式
- [x] 提供自动化系统调用表生成工具
- [x] 支持 QEMU 虚拟化运行，支持 RISC-V 和 LoongArch (TODO)架构

---

## 内核架构设计

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
Re-XVapor 采用经典的宏内核结构，所有核心服务（进程调度、内存管理、文件系统、IPC 等）都运行在特权态中。

内核划分为三层：

- **硬件抽象层**：统一封装底层硬件接口，如中断控制器、串口、时钟等
- **内核服务层**：管理进程、内存、设备、系统调用等核心资源
- **用户接口层**：提供系统调用接口和用户初始化支持（支持 ELF 加载与动态链接）

整个内核功能模块高度模块化，便于增量开发、测试和维护。

---

## 目录结构

```

├── build/                  # 编译输出目录
│   └── kernel/             # 编译生成的内核镜像
├── conf/                   # 配置文件
├── docs/                   # 项目文档
│   └── image/              # 文档插图
├── include/                # 公共头文件
├── kernel/                 # 内核源码
│   ├── arch/               # 架构相关（riscv/loongarch）
│   ├── asm/                # 启动汇编、上下文切换
│   ├── atomic/             # 原子操作封装
│   ├── fs/                 # 文件系统，含 VFS 层
│   ├── include/            # 内核私有头文件
│   ├── init/               # 启动与内核初始化
│   ├── ipc/                # 信号、信号量、futex 等 IPC
│   ├── lib/                # 内核通用库函数
│   ├── mm/                 # 虚拟内存管理
│   └── sched/              # 调度器与线程支持
├── mkfs/                   # 文件系统镜像生成工具
├── mnt/                    # 文件系统挂载点（用于测试）
├── scripts/                # 编译脚本、构建工具
├── user/                   # 用户空间程序源码
│   ├── asm/                # 启动用汇编代码
│   ├── include/            # 用户空间头文件
│   ├── init/               # 用户空间初始化程序
│   ├── lib/                # 用户态 libc/musl 兼容库
│   ├── shell/              # 简易 shell 实现
│   ├── src/                # 普通用户程序
│   └── test/               # 测试用例与断言脚本

````

---

## 开发路径

本项目最初基于 [MIT xv6-riscv](https://github.com/mit-pdos/xv6-riscv) 开发，逐步对其进行架构重构与功能增强，以下是从最初的单核、无线程、简化内核，到目前具备多核调度、现代 ELF 加载与 ext4 支持的完整演化过程。

---

### 第一阶段：基础内核功能增强

- [x] 支持内核线程 `clone()` 接口  
  ➤ 引入线程控制块（TCB）、实现与进程资源共享机制
- [x] 支持 Futex 原语与 hash-based 快速唤醒  
  ➤ 用于实现用户态高性能锁与条件同步
- [x] 信号机制初步支持  
  ➤ 包括 `kill`, `signal`, `sigaction`, `sigreturn` 以及 mask 控制
- [x] 条件变量与信号量支持  
  ➤ 用于替换原 `sleep-wakeup` 机制，支持更现代的同步语义
- [x] 多核支持与 CPU 启动  
  ➤ 支持通过 OpenSBI 管理 HART，完成多核调度框架搭建
- [x] 支持基本的 shell + musl-busybox 命令集

---

### 第二阶段：用户态与文件系统支持拓展

- [x] 虚拟文件系统（VFS）框架搭建  
  ➤ 支持多文件系统挂载，提供统一接口分发
- [x] 支持 ext4 文件系统读写（基于 lwext4）  
  ➤ 支持 inode 索引、块缓存、目录结构、文件内容访问
- [x] 支持 ELF 动态链接与用户态加载器  
  ➤ 加载 `.so`，支持 libc、musl 等通用运行时
- [x] 支持共享内存机制 `shmget`/`shmat`
- [x] 支持管道、socketpair，完善 IPC 接口集

---

### 第三阶段：开发工具链与测试支持

- [x] 自动生成 syscall 表（带注释/编号）  
  ➤ 通过脚本自动维护 `syscall.c`、`syscall.h`
- [x] 构建基础 libc 接口映射（如 printf、malloc 等）
- [x] 支持 libctest 框架与 oscomp/basic 测试
- [x] 文件系统镜像自动打包工具（mkfs）

---

### 第四阶段：系统模块重构与通用组件引入（进行中）

- [x] 支持通用数据结构（如 `list.h`、`bitmap.h`、`rbtree.h`）
  ➤ 替换静态数组和 ad hoc 实现，提升可复用性与代码可读性
- [ ] 引入 slab 分配器 / buddy allocator 等高级内存管理策略
- [ ] 模块化构建系统，支持 feature-based build 配置
- [ ] 支持 LoongArch 架构完整适配与测试
- [ ] 构建更丰富的用户态工具链（如 ps、ls、top、vi 等）
- [ ] 引入内核模块/驱动热插拔机制（Kernel Module Framework）

---

### 长期目标

- ☐ 完整的 POSIX 接口支持（如 `mmap`, `poll`, `epoll`, `pthread`）
- ☐ 实现完整的 TCP/IP 网络协议栈（或集成 lwIP）
- ☐ 移植到真实硬件（如 FPGA + RISC-V SoC）
- ☐ 支持图形界面（SDL/FrameBuffer 模拟实现）
- ☐ 提供远程调试接口 GDB stub + QEMU debug port 支持

---

### 开发节奏说明

本项目遵循模块化推进策略：每个阶段集中解决一类问题，例如“线程调度与同步机制”，“文件系统接入与挂载”，在实现基本功能后逐步优化性能与抽象能力，以确保内核保持可运行性和可测试性。


---


## 环境与配置

- 交叉编译器 riscv64-unknown-elf-gcc
- QEMU模拟器 qemu-riscv64，最好是7.0以上，开发时遇到过低版本无法运行的问题
- RISC-V GCC

--- 
## 构建与运行

### 编译内核

```bash
make all
````

会自动生成两个内核镜像：

* `kernel-rv`：支持 RISC-V 架构（默认）
* `kernel-la`：支持 LoongArch 架构

### 运行内核

```bash
make qemu
```

在 QEMU 虚拟机中启动内核，默认使用 riscv64 架构模拟器。

### 清理编译结果

```bash
make clean
```

---

## 文档目录（docs/）

### 系统基础

* [xv6 设计简析](docs/xv6.md)
* [RISC-V 架构基础](docs/riscv.md)
* [OpenSBI 简介](docs/sbi.md)
* [其他杂项](docs/misc.md)

### 内核子系统设计

* **进程管理**：[进程与线程调度](docs/proc&thread.md)
* **内存管理**：[基于 VMA 的内存管理](docs/vma.md)
* **文件系统**：

  * [虚拟文件系统](docs/vfs.md)
  * [Ext4 文件系统集成](docs/ext4.md)
  * [用户态 ELF 加载机制](docs/elf.md)
* **通信与同步**：

  * [条件变量](docs/cond.md)
  * [信号量](docs/semaphore.md)
  * [信号机制](docs/signal.md)
  * [Futex 原理与实现](docs/futex.md)
* **通用数据结构**
  * [通用链表](docs/list.md)
  * [通用队列](docs/queue.md)
  * [通用哈希表](docs/hash.md)
  
### 实验过程与调试记录

* [系统调用支持与自动生成工具](docs/syscall.md)
* [测试集支持过程（如 oscomp/basic）](docs/oscomp.md)
* [开发中遇到的问题与修复原始资料](docs/debug.md)
* [开发中遇到的问题与修复的总结](docs/debug_summary.md)
* [项目总体开发日志](docs/devlog.md)
---

## 设计幻灯片和演示视频

[**幻灯片和视频**](https://pan.baidu.com/s/11vAKy2br9p37ZwtR42LLcA?pwd=s3zw)

## 参考项目与资料

* [MIT xv6](https://github.com/mit-pdos/xv6-riscv)
* [Linux Kernel Source](https://github.com/torvalds/linux)
* [Operating Systems: Three Easy Pieces](https://pages.cs.wisc.edu/~remzi/OSTEP/)
* [riscv-sbi-doc](https://github.com/riscv-non-isa/riscv-sbi-doc)
* [OpenSBI 原理与入门](https://tinylab.org/introduction-to-riscv-sbi/)

此外，本项目中 **线程调度设计** 借鉴了往届项目 [LostWakeupOS](https://gitlab.eduxiji.net/202310336101112/LostWakeup.git)，特此致谢！

---

## 作者

**黄梓胜 - 吉林大学**

* 📧 邮箱：[huangzs2122@mails.jlu.edu.cn](mailto:huangzs2122@mails.jlu.edu.cn)
* 🔗 GitHub：[sandyyyz](https://github.com/sandyyyz)

欢迎交流、反馈与技术探讨！

