
# OpenSBI 简介

OpenSBI（Open Source Supervisor Binary Interface）是一个开源项目，提供对 RISC-V 架构中 Supervisor 模式（S-mode）操作系统的支持。它实现了 SBI（Supervisor Binary Interface）规范，为操作系统屏蔽不同 RISC-V 硬件平台之间的底层差异。

SBI 是操作系统（如 Linux、Xv6、自研内核）与 Machine 模式（M-mode）固件之间的标准接口，OpenSBI 就是该接口的一个开源实现。

---

## 参考资料

- [riscv-sbi-doc（SBI 官方文档）](https://github.com/riscv-non-isa/riscv-sbi-doc)
- [OpenSBI 入门介绍（TinyLab）](https://tinylab.org/introduction-to-riscv-sbi/)

---

## OpenSBI 的作用

- 为 S-mode 提供标准接口访问 M-mode 功能
- 实现不同平台上通用的启动逻辑、异常转发等机制
- 屏蔽具体 SoC 的实现差异（类似于 ARM 的 PSCI + bootloader）
- 允许多个 OS 重用相同的 M-mode 服务

---

## 架构关系图

```

+----------------------+

| M-mode (OpenSBI)         |
| ------------------------ |
| SBI 实现 + 平台支持            |
| +----------------------+ |

```
      ▲
      | SBI 接口
      ▼
```

+----------------------+
\|  S-mode OS (如 Xv6)  |
+----------------------+
▲
\| U-mode Trap
▼
+----------------------+
\|  U-mode 用户程序     |
+----------------------+

````

---

## 核心接口概览（SBI 调用）

OpenSBI 实现了多个标准 SBI 扩展接口，OS 可以通过 `ecall` 指令调用这些服务。以下是常见的接口：

### 1. `sbi_set_timer()`

设置下一次 timer 中断触发时间。

```c
sbi_set_timer(uint64_t stime_value);
````

操作系统中常用于初始化时钟、周期性调度。

### 2. `sbi_console_putchar()` / `sbi_console_getchar()`

用于访问底层串口设备，输出字符或接收输入字符。

```c
sbi_console_putchar(int ch);
int sbi_console_getchar();
```

在内核 `printf()` 实现中常用。

### 3. `sbi_shutdown()`

关闭系统电源（模拟器中一般用于退出）。

```c
sbi_shutdown();
```

### 4. `sbi_remote_fence_i()` / `sbi_remote_sfence_vma()`

跨核执行指令缓存或页表刷新，适用于 SMP 多核系统。

---

## OpenSBI 加载流程简述

当 QEMU 启动时，OpenSBI 位于 M-mode，是系统的第一段可执行代码。启动流程如下：

1. OpenSBI 以 M-mode 启动（在 QEMU 中通常通过 `-bios` 参数加载）。
2. 初始化平台信息、设备树、HART（CPU 核）信息。
3. 设置 S-mode 程序入口（即你的内核），并将控制权转交给内核。
4. 在运行过程中，S-mode OS 通过 `ecall` 发起 SBI 调用，OpenSBI 在 M-mode 响应。

---

## 使用方式

在使用 QEMU 运行操作系统时，OpenSBI 通常作为 `-bios` 参数指定：

```bash
qemu-system-riscv64 \
  -machine virt \
  -m 512M \
  -nographic \
  -bios /path/to/opensbi-fw_jump.elf \
  -kernel /path/to/your-kernel.elf
```

其中：

* `opensbi-fw_jump.elf` 是 OpenSBI 的固件镜像；
* `-kernel` 参数指定内核入口，OpenSBI 会跳转到此地址运行。

---

## 你自己的内核如何与 OpenSBI 交互

你可以直接使用封装好的函数调用 SBI 接口，例如：

```c
// 设置时钟中断（定时器）
sbi_set_timer(r_time() + interval);

// 打印字符
sbi_console_putchar('A');

// 关机
sbi_shutdown();
```

也可以通过手动触发 `ecall` 实现低层接口访问：

```c
static inline long sbi_call(long which, long arg0, long arg1, long arg2) {
    register long a0 asm("a0") = arg0;
    register long a1 asm("a1") = arg1;
    register long a2 asm("a2") = arg2;
    register long a7 asm("a7") = which;
    asm volatile("ecall"
                 : "+r"(a0)
                 : "r"(a1), "r"(a2), "r"(a7)
                 : "memory");
    return a0;
}
```

---

## 小结

OpenSBI 是构建 RISC-V 操作系统时不可或缺的组件。它将底层硬件操作封装成标准 SBI 接口，使得操作系统在不直接操作 M-mode 的前提下，也能实现定时器、关机、中断等底层功能。

在开发基于 RISC-V 架构的内核时，理解和正确使用 OpenSBI 是完成基本系统调用、时钟中断、多核支持等功能的关键步骤。


