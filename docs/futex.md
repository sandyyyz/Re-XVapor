
---

# Futex 机制概述

`futex`（Fast Userspace Mutex）是 Linux 提供的一种轻量级用户态同步机制，其核心设计思想是：**在无竞争时完全在用户态操作，只有在争用时才陷入内核**。这极大地减少了系统调用的开销，提高了多线程同步的效率。

---

## 基本原理

futex 的核心是一个位于用户空间的整型变量（通常是 `int`），它被线程用于表示锁的状态。当出现竞争时，线程会通过 `futex` 系统调用请求内核将其挂起，直到该变量的值发生改变。

其系统调用形式为：

```c
int futex(int *uaddr, int op, int val,
          const struct timespec *timeout,
          int *uaddr2, int val3);
````

参数说明：

* `uaddr`：用户空间中用于同步的整数地址。
* `op`：操作类型，如 `FUTEX_WAIT` 或 `FUTEX_WAKE`。
* `val`：与 `FUTEX_WAIT` 配合使用，指定期望值。
* `timeout`：可选的超时时间，用于 `FUTEX_WAIT`。
* `uaddr2`, `val3`：用于复杂操作，如 `FUTEX_REQUEUE`。

---

## 常用操作

### 1. `FUTEX_WAIT`

```c
futex(uaddr, FUTEX_WAIT, val, timeout, NULL, 0);
```

当前线程只有在 `*uaddr == val` 的情况下才会被挂起。内核会检查这个条件并将该线程放入等待队列。如果变量的值不等于 `val`，调用会立即返回 `-1` 并设置 `errno = EAGAIN`。

### 2. `FUTEX_WAKE`

```c
futex(uaddr, FUTEX_WAKE, n, NULL, NULL, 0);
```

唤醒最多 `n` 个在 `uaddr` 上等待的线程。

---

## 示例：用户态互斥锁

以下为典型的用户态互斥锁实现方式：

```c
void lock(futex_t *f) {
    int c;
    if (atomic_cmpxchg(f, 0, 1) != 0) {
        do {
            c = *f;
            if (c == 2 || atomic_cmpxchg(f, 1, 2) != 0)
                continue;
            futex(f, FUTEX_WAIT, 2, NULL, NULL, 0);
        } while (atomic_cmpxchg(f, 0, 2) != 0);
    }
}

void unlock(futex_t *f) {
    if (atomic_fetch_sub(f, 1) != 1) {
        *f = 0;
        futex(f, FUTEX_WAKE, 1, NULL, NULL, 0);
    }
}
```

说明：

* 0 表示未加锁
* 1 表示已加锁但无等待者
* 2 表示已加锁且有等待者

---

## futex 的优势

* 低开销：无锁争用时无内核参与。
* 高性能：只在必要时触发上下文切换。
* 易扩展：支持重排、队列传递、优先级继承等高级操作。

---

## 应用场景

* POSIX 线程库中的 `pthread_mutex`
* Java 虚拟机的线程同步机制
* 内核态模拟 futex（如自研系统实现同步原语）

---

## 内核支持与实现要点

* futex 使用哈希表在内核中索引用户地址对应的等待队列。
* 等待队列基于 `struct futex` 实现，与调度器直接交互。
* 具备“自唤醒”和“伪唤醒”过滤机制（通过比较 `val` 实现）。

本内核实现参考 Linux Futex 的设计，以哈希表管理每个 `uaddr` 所对应的 `futex` 对象，支持 `FUTEX_WAIT` 与 `FUTEX_WAKE` 操作。

---

## 设计概览

- 每个 futex 由用户空间地址 `uaddr` 标识
- 内核为每个 futex 维护一个等待队列 `waiting_queue`
- 使用哈希表（带自旋锁保护）加速查找、插入、删除 futex 实例
- 支持 timeout 等待
- 无需用户手动创建/释放 futex，系统自动管理其生命周期

---

## 核心结构

```c
struct futex {
    struct spinlock lock;
    struct queue waiting_queue;
};

struct futex_hash_node {
    uint64 uaddr;
    struct futex *fp;
    struct list_head futex_hash_list_node;
};

struct hash_table futex_hashtable;
````

* `futex`：内核维护的 futex 对象
* `futex_hash_node`：futex 映射的哈希节点，包含 uaddr → futex 的映射
* `futex_hashtable`：主哈希表结构，管理所有 futex 映射

---

## Futex 操作接口

### 初始化

```c
void futex_hash_init(void);
```

初始化哈希表、锁及操作指针。

---

### 核心系统调用入口

```c
int do_futex(uint64 uaddr, int op, uint32 val, const struct timespec *timeout, ...);
```

* 分发入口函数
* 支持 `FUTEX_WAIT`, `FUTEX_WAKE` 等常用操作
* 内部调用 `futex_wait()` 和 `futex_wake()` 实现同步原语

---

## 主要逻辑

### Futex 等待

```c
static int futex_wait(uint64 uaddr, uint32 val, const struct timespec *timeout);
```

* 若 `*uaddr != val`，立即返回
* 若相等：

  * 调用 `get_futex()` 获取或创建 `futex` 实例
  * 当前线程进入 `TCB_SLEEPING` 状态，挂入 `fp->waiting_queue`
  * 若设置超时，则启动超时计数
  * 进入调度器等待唤醒

---

### Futex 唤醒

```c
int futex_wake(uint64 uaddr, int nr_wake);
```

* 获取指定 futex 对象
* 从其 `waiting_queue` 中唤醒最多 `nr_wake` 个线程
* 若队列空，则自动释放 futex 实例

---

### Futex 获取与释放

```c
struct futex *get_futex(uint64 uaddr, int assert);
void futex_free(uint64 uaddr);
```

* `get_futex()`：从哈希表中查找，若不存在且 `assert == 0` 则新建
* `futex_free()`：在 futex 队列为空时自动调用，释放对应资源

---

## 哈希操作封装

* `__futex_hash_key()`：使用地址本身作为哈希键
* `__futex_hash_lookup()`：遍历 entry 查找 `uaddr`
* `__futex_hash_insert()`：插入新节点
* `__futex_hash_delete()`：删除并释放 futex 资源

---

## 支持的 Futex 操作码

| Futex 操作码            | 描述                   |
| -------------------- | -------------------- |
| `FUTEX_WAIT`         | 阻塞直到 `*uaddr != val` |
| `FUTEX_WAKE`         | 唤醒等待队列中的线程           |
| `FUTEX_WAIT_PRIVATE` | 同 `WAIT`，用于私有锁       |
| `FUTEX_WAKE_PRIVATE` | 同 `WAKE`，用于私有锁       |

---

## 锁机制说明

* futex 全局哈希表加锁 (`futex_hashtable.lock`) 保证并发安全
* 每个线程的 `tcb->lock` 在挂入队列或状态迁移时加锁
* futex 本身的 `fp->lock` **未被实际使用**

---

## 调试宏支持（可选）

* `__DEBUG_FUTEX_WAIT`：启用 wait 调试日志
* `__DEBUG_FUTEX_WAKE`：启用 wake 调试日志
* `__DEBUG_DO_FUTEX`：显示错误操作码

---

## TODO / 可拓展项

* 支持更多操作码（如 `FUTEX_REQUEUE`, `FUTEX_CMP_REQUEUE`）
* 支持优先级继承 (`FUTEX_LOCK_PI`)
* dentry 与 futex 地址映射绑定（用于 namespace 隔离）
* futex 内核资源自动回收（如超时定期清理）

---


## 注意事项

* 共享的 futex 变量必须位于可被多个线程（或进程）访问的内存中。
* 应避免内存模型破坏原子性（建议配合 `atomic` 操作使用）。
* 必须确保地址在调用过程中保持有效。

---

## 参考资料

* Linux man page: `man 2 futex`
* Ulrich Drepper, *Futexes Are Tricky*, Red Hat
* Linux kernel source: `kernel/futex/`

