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