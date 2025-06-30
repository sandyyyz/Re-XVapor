# `queue.h` 通用队列系统文档

该文件定义了一个基于双向链表的通用队列系统，支持并发安全操作、插入、移除、遍历等功能，并封装了对 `list.h` 的链表接口。通用队列可以用于实现各种等待队列，如条件等待队列，futex队列等等。

---

## 1. 队列结构定义

```c
struct queue {
    struct spinlock lock;     // 队列的自旋锁
    struct list_head list;    // 双向链表头
    char name[30];            // 队列名称，用于调试
    enum queue_type type;     // 队列类型（决定 node 类型）
    int cnt;                  // 队列节点计数器
};
```

### `enum queue_type`

定义不同的队列用途：

```c
enum queue_type {
    TCB_STATE_QUEUE,     // TCB 状态队列
    PCB_STATE_QUEUE,     // PCB 状态队列
    TCB_WAIT_QUEUE,      // TCB 等待队列
    INODE_FREE_QUEUE     // inode 空闲队列
};
```

---

## 2. 接口函数说明

### 初始化

```c
void queue_init(queue_t *q, char *name, enum queue_type type);
```

初始化一个队列，分配名称并设置类型。

---

### 判空

```c
int queue_isempty(queue_t *q);
int queue_isempty_atomic(queue_t *q);
```

判断队列是否为空。`_atomic` 版本加锁检查。

---

### 获取节点信息

```c
struct list_head *queue_entry(void *node, enum queue_type type);
void *queue_first_node(queue_t *q);
```

* `queue_entry`：根据 `type` 获取 node 中的 `list_head` 成员。
* `queue_first_node`：返回队首的节点指针。

---

### 插入节点

```c
void queue_push_back(queue_t *q, void *node);
void queue_push_back_atomic(queue_t *q, void *node);
```

将节点插入队列尾部。原子版本带锁。

---

### 删除节点

```c
void queue_remove(void *node, enum queue_type type);
void queue_remove_atomic(queue_t *q, void *node);
```

将节点从其所属队列中移除。`_atomic` 版本操作时加锁。

---

### 弹出节点

```c
void *queue_pop(queue_t *q, int remove);
void *queue_pop_atomic(queue_t *q, int remove);
```

* 返回队首节点。
* 如果 `remove=1`，则将其从队列中删除。

---

### 获取队列长度

```c
int get_queue_count(queue_t *q);
```

返回当前队列中的节点数量。

---

## 3. 安全遍历宏

```c
#define queue_for_each_entry_safe(pos, tmp, q, member) \
    list_for_each_entry_safe(pos, tmp, &(q->list), member)
```

* 用于安全遍历队列中的结构体节点。
* `pos` 是当前元素指针，`tmp` 是临时变量。
* 注意：需要**在使用前先加锁**，以避免并发修改。

---

## 4. 使用示例

```c
struct tcb {
    int tid;
    struct list_head state_queue_node;
};

queue_t tcb_queue;
queue_init(&tcb_queue, "tcb_q", TCB_STATE_QUEUE);

struct tcb t1 = { .tid = 1 };
INIT_LIST_HEAD(&t1.state_queue_node);

queue_push_back(&tcb_queue, &t1);

struct tcb *pos, *tmp;
acquire(&tcb_queue.lock);
queue_for_each_entry_safe(pos, tmp, &tcb_queue, state_queue_node) {
    printf("TID: %d\n", pos->tid);
}
release(&tcb_queue.lock);
```

---

## 5. 注意事项

* 队列中的节点类型必须与 `queue_type` 匹配。
* 所有操作均应保持一致的并发控制策略（避免一边锁一边不锁）。
* `_atomic` 接口适用于内核线程或中断上下文中需要快速安全修改队列的情况。
* `cnt` 字段主要用于调试验证，非强一致。

---