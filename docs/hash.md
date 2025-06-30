
---

# `hash.h`：通用哈希表接口文档

该模块定义了一个**通用、可定制**的哈希表结构和操作接口，支持插入、查找、删除等操作，适用于内核开发中对结构体的哈希映射。本内核主要在`futex`相关数据结构的设计上使用了哈希表进行映射加速。

---

## 1. 数据结构说明

### `struct hash_entry`

```c
struct hash_entry {
    struct list_head list;
};
```

哈希表的桶节点。每个桶通过 `list_head` 维护一个链表，解决哈希冲突。

---

### `struct hash_table`

```c
struct hash_table {
    struct spinlock lock;
    uint64 size;
    struct hash_entry *hash_head;
    struct hash_table_operation *op;
};
```

哈希表结构体字段说明：

* `lock`：用于保护哈希表操作的自旋锁。
* `size`：哈希桶总数（表大小）。
* `hash_head`：哈希桶数组，每个桶为一个 `hash_entry`。
* `op`：函数指针集合，指向该哈希表的操作方法。

---

### `struct hash_table_operation`

哈希表操作定义（每个哈希表需实现）：

```c
struct hash_table_operation {
    void *(*hash_lookup)(struct hash_table *table, void *key);
    int (*hash_insert)(struct hash_table *table, void *node);
    void (*hash_delete)(struct hash_table *table, void *node);
    uint64 (*hash_key)(void *key);
};
```

* `hash_lookup`：通过 key 查找元素。
* `hash_insert`：插入一个 node。
* `hash_delete`：删除一个 node。
* `hash_key`：从 key 计算哈希值。

---

## 2. 哈希表初始化

```c
void hash_table_entry_init(struct hash_table *table);
```

初始化哈希表中每个桶的链表头指针：

* 为 `hash_head` 分配 `size` 个 `hash_entry`。
* 每个 `entry` 初始化为空链表。

调用前需设置好 `table->size`。

---

## 3. 查找桶位置

```c
struct hash_entry *hash_get_entry(struct hash_table *table, void *key);
```

* 计算 key 的哈希值。
* 返回该值对应桶的 `hash_entry*`。

调用者需持有 `table->lock`。

---

## 4. 哈希算法实现（名称字符串哈希）

```c
static inline uint64 full_name_hash(const unsigned char *name, unsigned int len);
```

### 4.1 哈希流程

1. 初始值：

   ```c
   #define init_name_hash() 0
   ```

2. 单字符累加：

   ```c
   partial_name_hash(c, prev) = (prev + (c << 4) + (c >> 4)) * 11
   ```

3. 收尾：

   ```c
   end_name_hash(h) = (unsigned int) h
   ```

最终由 `full_name_hash(name, len)` 组合调用完成哈希。

该算法来源于 ReiserFS 使用的 R5 哈希算法，适合用于文件名等字符串哈希。

---

## 5. 示例：构建一个支持字符串键的哈希表

```c
// 用户结构体
struct user {
    char name[32];
    struct list_head hash_link;
};

// 哈希函数
uint64 user_hash_key(void *key) {
    const char *name = (const char *)key;
    return full_name_hash((const unsigned char *)name, strlen(name));
}

// 插入操作
int user_insert(struct hash_table *table, void *node) {
    struct user *u = (struct user *)node;
    struct hash_entry *entry = hash_get_entry(table, u->name);
    list_add(&u->hash_link, &entry->list);
    return 0;
}
```

---

## 6. 注意事项

* 所有操作应在持有 `table->lock` 的前提下进行（除非是原子接口）。
* 使用者需实现完整的 `hash_table_operation` 操作集。
* 可用于 inode 表、打开文件表、用户缓存、调度实体等结构。

---

## 7. 接口汇总

| 函数 / 宏                    | 作用               |
| ------------------------- | ---------------- |
| `hash_get_entry()`        | 获取对应 key 的桶      |
| `hash_table_entry_init()` | 初始化哈希表桶          |
| `full_name_hash()`        | 对字符串生成哈希值        |
| `hash_table_operation`    | 自定义查找/插入/删除函数指针集 |

---

