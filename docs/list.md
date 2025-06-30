
---

# `list.h` 双向链表工具库文档

该文件定义了基于 Linux 内核风格的双向循环链表操作接口，适用于嵌入式系统或操作系统内核开发。在本内核中，各种链表都是通过该通用list实现和操作的。

---

## 1. 基本结构定义

### `struct list_head`

双向链表的基础节点结构：

```c
struct list_head {
    struct list_head *next, *prev;
};
```

用于将其嵌入在其他结构体中，实现链式连接。

### `INIT_LIST_HEAD`

初始化链表头节点（令其 next/prev 指向自身）：

```c
void INIT_LIST_HEAD(struct list_head *list);
```

---

## 2. 宏定义辅助

### `container_of`

通过成员指针返回结构体的首地址：

```c
#define container_of(ptr, type, member)
```

### `list_entry`

获取链表节点对应的结构体指针：

```c
#define list_entry(ptr, type, member)
```

---

## 3. 添加与删除节点

### `list_add`

将新节点添加到链表头（适用于栈结构）：

```c
void list_add(struct list_head *pnew, struct list_head *head);
```

### `list_add_tail`

添加到链表尾部（适用于队列）：

```c
void list_add_tail(struct list_head *pnew, struct list_head *head);
```

### `list_del` / `list_del_entry`

删除节点并置空指针：

```c
void list_del(struct list_head *entry);
```

### `list_del_reinit`

删除节点并重新初始化为空链表：

```c
void list_del_reinit(struct list_head *entry);
```

---

## 4. 节点移动

### `list_move`

将节点移动到另一个链表的头部：

```c
void list_move(struct list_head *entry, struct list_head *head);
```

### `list_move_tail`

将节点移动到另一个链表的尾部：

```c
void list_move_tail(struct list_head *entry, struct list_head *head);
```

---

## 5. 链表合并与拼接

### `list_splice`

将一个链表接入到另一个链表的 head 之后：

```c
void list_splice(struct list_head *list, struct list_head *head);
```

### `list_join_given_first`

通过已知链表的第一个节点，将两个链表拼接：

```c
void list_join_given_first(struct list_head *first_new, struct list_head *first_old);
```

---

## 6. 链表状态检查

### `list_empty`

判断链表是否为空：

```c
int list_empty(const struct list_head *head);
```

### `list_empty_atomic`

带锁判断链表是否为空：

```c
int list_empty_atomic(const struct list_head *head, struct spinlock *mutex);
```

---

## 7. 遍历宏接口

### 基础遍历

```c
list_for_each(pos, head)                      // 基本链表节点遍历
list_for_each_entry(pos, head, member)        // 获取结构体指针的正向遍历
list_for_each_entry_reverse(pos, head, member)// 反向遍历
```

### 安全遍历（支持删除节点）

```c
list_for_each_entry_safe(pos, n, head, member)
list_for_each_entry_safe_reverse(pos, n, head, member)
```

### 条件遍历

```c
list_for_each_entry_condition(pos, head, member, condition)
list_for_each_entry_safe_condition(pos, n, head, member, condition)
```

### 给定头指针的遍历

```c
list_for_each_entry_given_first(pos, head_f, member, flag)
list_for_each_entry_safe_given_first(pos, n, head_f, member, flag)
```

---

## 8. 获取链表中的元素

```c
list_first_entry(ptr, type, member)
list_last_entry(ptr, type, member)
list_next_entry(pos, member)
list_prev_entry(pos, member)
```

---

## 9. 使用

* **线程安全性**：配合 `spinlock` 实现原子操作（如 `list_empty_atomic`）。
* **类型安全性**：通过 `typeof` 实现结构体访问的类型推导。
* **遍历时删除**：请使用 `_safe` 系列宏以避免失效指针。

---

## 10. 示例代码

```c
struct task {
    int id;
    struct list_head node;
};

LIST_HEAD(task_list); // 初始化任务链表

struct task t1 = { .id = 1 };
INIT_LIST_HEAD(&t1.node);
list_add(&t1.node, &task_list);

// 遍历任务
struct task *pos;
list_for_each_entry(pos, &task_list, node) {
    printf("Task ID: %d\n", pos->id);
}
```

---
