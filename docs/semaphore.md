
## 信号量

信号量（Semaphore）是一种经典的同步原语，用于实现线程/进程之间的互斥（mutual exclusion）和同步（synchronization）。在本内核中，我们实现了内核态的信号量机制，并用其替代了原有的 `sleeplock` 以及部分条件变量的使用。

---

### 使用场景

#### 1. 实现互斥（mutex）

通过将信号量初始值设为 `1`，可以实现类似互斥锁的功能：

- 第一个调用 `down()` 的线程会立即获得资源；
- 后续调用者在资源释放前将被阻塞；
- 调用 `up()` 释放资源，唤醒阻塞线程。

这类信号量可以安全地替代内核中的 `sleeplock` 机制，用于保护临界区，如设备寄存器访问、共享数据结构等。

#### 2. 实现同步（condition-like）

将信号量初始值设为 `0`，用于线程之间的同步，例如：

- 一个线程等待某个事件发生（通过 `down()` 阻塞）；
- 另一个线程在事件发生后通过 `up()` 唤醒等待者。

这种用法与条件变量相似，但逻辑更简单。它尤其适合替代 xv6 中对 `sleep()` + `wakeup()` 的部分使用。

---

### 接口定义

```c
struct semaphore {
    int value;
    struct spinlock lock;
    struct proc *wait_queue[NPROC];
    int head, tail;
};

void sem_init(struct semaphore *sem, int value);
void sem_down(struct semaphore *sem);
void sem_up(struct semaphore *sem);
````

---

### 实现简述

* `sem_init()`：初始化信号量，设置初始值，并初始化等待队列。
* `sem_down()`：

  * 如果 `value > 0`，直接减一返回；
  * 否则，将当前进程加入等待队列并 `sleep`。
* `sem_up()`：

  * 增加 `value`；
  * 如果有等待者，从队列中唤醒一个进程。

---

### 示例：基于信号量的互斥访问

```c
struct semaphore sem;

void init() {
    sem_init(&sem, 1); // 互斥锁
}

void critical_section() {
    sem_down(&sem);
    // 访问临界资源
    sem_up(&sem);
}
```

---

### 示例：信号量实现生产者-消费者同步

```c
struct semaphore empty, full;

void init() {
    sem_init(&empty, N); // N 个空槽
    sem_init(&full, 0);  // 初始无产品
}

void producer() {
    sem_down(&empty);
    // 生产产品
    sem_up(&full);
}

void consumer() {
    sem_down(&full);
    // 消费产品
    sem_up(&empty);
}
```

---

### 与 sleeplock 的对比

| 特性    | sleeplock | 信号量 (sem=1) |
| ----- | --------- | ----------- |
| 阻塞行为  | 阻塞进程      | 阻塞进程        |
| 可重入性  | 不可重入      | 可控制         |
| 通用性   | 仅用于互斥     | 可用于互斥和同步    |
| 使用复杂度 | 简单        | 略高          |
| 内存占用  | 小         | 略大          |

信号量的优势在于通用性更强，既能实现互斥，又能进行进程之间的同步，是更通用的原语。

---

### 后续优化方向

* 引入基于优先级的信号量调度策略；
* 支持超时阻塞（如 `sem_timedwait`）；
* 用户态信号量接口封装，支持 `libc` 集成；
* 支持多核调度下的 NUMA 信号量模型。

```
