# signal

ref: https://man7.org/linux/man-pages/man7/signal.7.html
https://man7.org/linux/man-pages/man2/sigaction.2.html

- signal 是一种异步通信机制，用于通知进程发生了某种事件
- 在进程上下文中异步触发，可以打断原有执行流
- 在送达时，如果进程对该信号设置了处理函数，则会跳转执行该函数(用户态程序)
- 进入内核检查是否需要处理信号 -> 保存原有执行流栈和寄存器 -> 回到用户态信号处理函数 -> 处理完毕回到内核态 -> 内核恢复原有用户态执行流 

大概执行流：  

┌────────────┐  
│ 原程序执行  │  
└────┬───────┘  
     │ （收到 SIGUSR1）  
     ▼  
┌────────────┐  
│ 内核保存上下文 │  
└────┬───────┘  
     │ 构造 signal frame  
     ▼  
┌────────────┐  
│ signal handler│  ← 用户态执行  
└────┬───────┘  
     │ handler 返回  
     ▼  
┌────────────┐  
│ 调用 sigreturn │  
└────┬───────┘  
     │ 内核恢复上下文  
     ▼  
┌────────────┐  
│ 回到原程序  │  
└────────────┘  

## 为了实现信号系统需要实现什么？

### signal.h

这里面要写一些相关结构体的定义，注意一定要参照posix标准，有些结构体在不同的架构下甚至有不同的格式，还有一些为了兼容以前的版本有一些swapped的字段，我是直接用gdb看的

#### sigset_t
用于表示一个信号是否在信号集中  
```c

#define SIGSET_LEN 1
typedef struct
{
    unsigned long __val[SIGSET_LEN];
} __sigset_t;
```

#### sigaction

``` c
// siganal action, to discribe the behavior when receive a given signal
struct sigaction {
    union {
        __sighandler_t sa_handler;	/* signal handler */
        void (*sa_sigaction)(int, siginfo_t*, void *); /* signal handler with extra info */
    };
    __sigset_t sa_mask;	/* mask to apply during handler execution */
    int sa_flags;	/* special flags */
    void (*sa_restorer)(void); /* used by kernel to restore context */
};

```
### 线程结构体变动

`sighand`存储线程的所有sigactions,对应收到不同信号的处理方式  
`sig_pending`存储所有等待处理的信号  
`blocked`表示线程屏蔽的信号集  
`pending_cnt`表示当前等待处理的信号数量

返回用户空间handler之前需要布置好栈，在栈上存放好ucontext以及原本的trapframe，以便返回时可以恢复原有的寄存器信息.  
该使用glibc abi规定的ucontext_t吗，还是自定义一个呢？会不会造成什么不良后果  

在返回用户handler之前把ra设置为sigreturn的地址，这样用户就会调用`sys_sigreturn`，该函数负责恢复原有的用户执行流和上下文  
别忘了在创建进程时映射SIGRETURN这块空间，以便让用户访问  
在`usertrap`中处理信号`signal_handle`

---

# 内核信号机制实现文档（RISC-V 架构）

## 一、概述

本模块实现了 POSIX 风格的信号处理机制，允许线程（或进程）在接收到异步信号时进行用户态自定义处理或执行内核定义的默认行为。

信号是一种异步事件通知机制，适用于：

* 线程之间异步通信
* 内核向线程发出事件（如 `SIGSEGV`、`SIGCHLD`）
* 用户程序通过 `kill()` 发送信号

该机制支持：

* 自定义信号处理函数 (`sigaction`)
* 信号屏蔽 (`sigprocmask`)
* 信号等待 (`sigtimedwait`)
* 信号上下文构造与恢复 (`sigreturn`)
* 基于栈帧的用户态 handler 切换

---

## 二、数据结构说明

### 1. `sigaction`

```c
struct sigaction {
    union {
        __sighandler_t sa_handler;
        void (*sa_sigaction)(int, siginfo_t*, void *);
    };
    __sigset_t sa_mask;
    int sa_flags;
    void (*sa_restorer)(void);
};
```

描述当线程接收到某个信号时的处理方式，支持标准 handler 和带信息的 handler (`SA_SIGINFO`)。mask 表示 handler 执行期间临时阻塞的信号。

### 2. `sigset_t`

```c
typedef struct {
    unsigned long sig;
} sigset_t;
```

本项目中简化为单个 `unsigned long`，表示最多支持 64 种信号（使用位图掩码方式）。

### 3. `siginfo_t`

每个 signal queue 元素附带的元信息，遵循 POSIX 定义，包括来源、代码、发送者等。

### 4. `sigpending`

```c
struct sigpending {
    sigset_t signal;                  // pending 信号掩码集合
    struct spinlock siglock;         // 信号队列锁
    struct list_head list;           // 信号队列链表头
};
```

记录当前线程收到但尚未处理的信号列表。

### 5. `tcb` 线程控制块中新增字段

* `struct sighand *sigs`: 记录所有的 `sigaction`
* `sigset_t blocked`: 当前屏蔽的信号
* `struct sigpending sig_pending`: 待处理信号列表
* `int pending_cnt`: 当前队列中信号个数
* `int sig_processing`: 正在处理的信号编号

---

## 三、信号处理流程

### 1. 信号发送（signal\_send）

* 分配一个 `sigqueue` 结构并填入 `siginfo`
* 添加至 `t->sig_pending.list` 队列
* 更新 `sig_pending.signal` 中的位图
* 若是致命信号（如 `SIGKILL`），立即标记线程 `killed=1`

### 2. 信号接收（signal\_handle）

* 遍历 `sig_pending` 队列，查找非屏蔽、合法的信号
* 根据 `sigaction` 设置分情况处理：

  * `SIG_IGN`：忽略，跳过
  * `SIG_DFL`：调用 `signal_default` 处理（如杀死线程）
  * 自定义 handler：构造信号帧，切换到用户态执行

### 3. 信号屏蔽设置（do\_sigprocmask）

* 根据 `how` 参数更新 `t->blocked` 信号屏蔽集合
* 不允许屏蔽 `SIGKILL` 与 `SIGSTOP`

### 4. 信号行为设置（do\_sigaction）

* 注册或查询线程对特定信号的处理方式
* 不允许设置 `SIGKILL` 和 `SIGSTOP` 的 handler

---

## 四、用户态信号 handler 切换

### 1. 设置 signal frame（setup\_rt\_frame）

* 调用 `get_sigframe()` 计算用户态信号栈顶部地址
* 调用 `signal_frame_setup()` 设置两个 `ucontext`：

  * 一个兼容 `glibc` ABI（`ucontext_t`）
  * 一个自定义结构体用于 trapframe 恢复
* 设置 `trapframe`：

  * `epc` 指向 handler
  * `sp` 指向构造好的 signal frame
  * `ra` 指向 trampoline 地址（`SIGRETURN`）

> 当用户 handler 返回后，会跳转到 trampoline，触发 syscall `sigreturn`。

### 2. 恢复原始执行上下文（signal\_frame\_restore）

* 拷贝并恢复 `trapframe`
* 恢复 `blocked` 屏蔽集
* 若 `ucontext_t` 中设置了 `PC`，还原 `epc` 寄存器（如 `pthread_cancel` 场景）

---

## 五、特殊功能说明

### 1. `do_sigtimedwait`

允许线程阻塞等待一个信号集合中的任意信号，并支持设置超时：

* 若 `sigset_t` 中信号未到达，调用 `thread_sleep`
* 被 `signal_send()` 唤醒后处理信号
* 若超时则返回 `-EAGAIN`

---

## 六、设计要点与注意事项

* 所有信号处理相关的操作都加锁，防止并发访问信号队列
* 栈对齐：构造 signal frame 时手动对齐栈指针（16 字节对齐）
* 用户 handler 返回后必须调用 `sys_sigreturn` 来恢复 trapframe，否则程序会挂起
* `SIGRETURN` trampoline 必须映射给用户可访问
* handler 中信号屏蔽集临时生效，不影响 handler 返回后的状态

---

## 参考资料

* [man7.org - signal(7)](https://man7.org/linux/man-pages/man7/signal.7.html)
* [man7.org - sigaction(2)](https://man7.org/linux/man-pages/man2/sigaction.2.html)
* Linux Kernel Source: `arch/riscv/kernel/signal.c`
* glibc Source: `sysdeps/unix/sysv/linux/riscv/nptl/sigaction.c`

---