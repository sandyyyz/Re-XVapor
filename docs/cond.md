## 使用条件变量代替 xv6 原本的 sleep/wakeup 机制

在 xv6 中，传统的进程同步依赖 `sleep(chan, lock)` 和 `wakeup(chan)` 机制。这种方式虽然简单，但存在以下问题：

- `chan` 是任意类型的地址，缺乏类型安全；
- 无法实现条件同步（如等待某个状态变为真）；
- 需要外部锁保护，使用容易出错；
- 与现代操作系统（如 Linux）中 `wait_event()` 或 `pthread_cond_wait()` 相比，表达力较弱。

为此，我们实现了基于条件变量（Condition Variable, CV）的同步机制，目标是替代原有的 `sleep-wakeup` 方法，实现更模块化、语义更清晰的同步控制。

---

### 参考设计：Linux 条件变量模型

在 Linux 内核中，常用 `wait_queue_head_t` 与 `wait_event()` 等宏实现条件等待。而我们采用类似的接口与语义，使用 `cv_wait()` / `cv_signal()` 替代传统 `sleep()` / `wakeup()`。

参考文章：[Linux 条件变量机制解析](https://zhuanlan.zhihu.com/p/374385534)

---

### 条件变量接口设计

```c
struct condvar {
    struct queue_t wait_queue; // 内部等待队列（含自旋锁）
};

void cv_init(struct condvar *cv);
void cv_wait(struct condvar *cv, struct spinlock *lk);
void cv_signal(struct condvar *cv);
void cv_broadcast(struct condvar *cv);
````

由于 `queue_t` 内部已自带自旋锁，所以无需在 `condvar` 结构体中额外加入锁字段。这避免了双锁交叉问题，也简化了使用方式。

---

### 替代原逻辑：sleep(chan, lock) → cv\_wait()

**原始 xv6 实现示例：**

```c
acquire(&lock);
while (!condition)
    sleep(chan, &lock);
release(&lock);
```

**新的条件变量实现：**

```c
acquire(&lock);
while (!condition)
    cv_wait(&cv, &lock);
release(&lock);
```

这里的 `cv_wait()` 会在当前条件不满足时自动释放传入的 `lock`，将当前进程加入 `cv` 的等待队列，并在被唤醒后重新获取 `lock`。

---

### 替代原逻辑：wakeup(chan) → cv\_signal()/cv\_broadcast()

**原始 xv6 实现：**

```c
wakeup(chan);
```

**条件变量版本：**

```c
cv_signal(&cv);    // 唤醒一个等待者
cv_broadcast(&cv); // 唤醒所有等待者（例如资源释放等广播场景）
```

---

### 示例：生产者-消费者问题（基于条件变量）

```c
struct spinlock lock;
struct condvar not_full, not_empty;
int buffer[N], count;

void producer() {
    acquire(&lock);
    while (count == N)
        cv_wait(&not_full, &lock);

    buffer[count++] = item;
    cv_signal(&not_empty);
    release(&lock);
}

void consumer() {
    acquire(&lock);
    while (count == 0)
        cv_wait(&not_empty, &lock);

    item = buffer[--count];
    cv_signal(&not_full);
    release(&lock);
}
```

此模式清晰表达了等待条件与唤醒逻辑，避免使用 `chan` 的裸指针语义，更安全且更具模块性。

---

### 实现说明

* `cv_wait()`：

  * 当前线程进入等待队列 `cv->wait_queue`；
  * 释放外部锁 `lk`；
  * 睡眠并等待唤醒；
  * 被唤醒后重新获取锁。
* `cv_signal()`：

  * 从 `cv->wait_queue` 中唤醒一个等待进程；
* `cv_broadcast()`：

  * 唤醒所有在 `cv` 上等待的线程。

等待队列实现使用内核中统一的 `queue_t` 结构管理：

```c
struct queue_t {
    struct spinlock lock;
    struct proc *procs[NPROC];
    int head, tail;
};
```

---

### 与 sleep/wakeup 的对比

| 功能点    | sleep/wakeup | condvar + queue\_t |
| ------ | ------------ | ------------------ |
| 类型安全   | 否            | 是                  |
| 条件表达能力 | 弱            | 强                  |
| 封装性    | 差            | 好                  |
| 调试难度   | 高            | 较低                 |
| 内核一致性  | 与 Linux 不一致  | 语义接近 Linux         |

---

### 总结

通过引入条件变量机制，我们在 xv6 内核中实现了更符合现代操作系统设计的同步原语，解决了 `sleep/wakeup` 模型的多个痛点。新接口更易于使用，语义明确，适用于互斥控制、条件等待等多种场景。后续还可以进一步引入：

* 支持 `cv_wait_timeout()` 实现超时同步；
* 优先级调度支持（PRIO\_WAIT）；
* 与信号量、futex 联合优化更复杂同步场景。

```
