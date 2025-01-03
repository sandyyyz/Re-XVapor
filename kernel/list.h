#ifndef __LIST_H
#define __LIST_H

#include "types.h"
#include <stddef.h>
#include "spinlock.h"
#include "defs.h"


/// get the parent struct of a member in a struct
/*
    * container_of - cast a member of a structure out to the containing structure
    * @ptr:	the pointer to the member.
    * @type:	the type of the container struct this is embedded in.
    * @member:	the name of the member within the struct.
    
*/
#ifndef container_of
#define container_of(ptr, type, member) ({                      \
        const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
        (type *)( (char *)__mptr - offsetof(type,member) ); })
#endif

// 返回包含list_head父类型的结构体
/**
 * list_entry - get the struct for this entry
 * @ptr:	the &struct list_head pointer.
 * @type:	the type of the struct this is embedded in.
 * @member:	the name of the list_head within the struct.
 */
#define list_entry(ptr, type, member) \
    container_of(ptr, type, member)

// 链表数据结构
struct list_head {
    struct list_head *next, *prev;
};
typedef struct list_head list_head_t;

// 链表初始化
static inline void INIT_LIST_HEAD(struct list_head *list) {
    list->next = list;
    list->prev = list;
}

// 链表头

#define LIST_HEAD_INIT(name) \
    { &(name), &(name) }
#define LIST_HEAD(name) \
    struct list_head name = LIST_HEAD_INIT(name)

// 初始化一个结构体

// 给链表增加一个节点
/*
 * Insert a new entry between two known consecutive entries.
 * This is only for internal list manipulation where we know
 * the prev/next entries already!
 */
static inline void __list_add(struct list_head *pnew,
                              struct list_head *prev,
                              struct list_head *next) {
    next->prev = pnew;
    pnew->next = next;
    pnew->prev = prev;
    prev->next = pnew;
}
/**
 * list_add - add a new entry
 * @new: new entry to be added
 * @head: list head to add it after
 * Insert a new entry after the specified head.
 * This is good for implementing stacks.
 */
static inline void list_add(struct list_head *pnew, struct list_head *head) {
    __list_add(pnew, head, head->next);
}

// 从链表中删除一个节点
/*
 * Delete a list entry by making the prev/next entries
 * point to each other.
 * This is only for internal list manipulation where we know
 * the prev/next entries already!
 */
static inline void __list_del_entry(struct list_head *prev, struct list_head *next) {
    next->prev = prev;
    prev->next = next;
}
/**
 * list_del - deletes entry from list.
 * @entry: the element to delete from the list.
 * Note: list_empty() on entry does not return true after this, the entry is
 * in an undefined state.
 */
static inline void list_del_entry(struct list_head *entry) {
    __list_del_entry(entry->prev, entry->next);
}

static inline void list_del(struct list_head *entry) {
    list_del_entry(entry);
    entry->next = (list_head_t *)NULL;
    entry->prev = (list_head_t *)NULL;
}

// delete it and re init
// remove a node from a list and reinitialize it
static inline void list_del_reinit(struct list_head *entry) {
    list_del(entry);
    INIT_LIST_HEAD(entry);
}

// 将节点从一个链表中移动到另一个链表
/**
 * list_move - delete from one list and add as another's head
 * @list: the entry to move
 * @head: the head that will precede our entry
 */
static inline void list_move(struct list_head *list, struct list_head *head) {
    list_del_entry(list);
    list_add(list, head);
}


/**
 * list_add_tail - add a pnew entry to the tail of a list
 * @pnew: pnew entry to be added
 * @head: list head to add it before
 * Insert a pnew entry before the specified head.
 * This is useful for implementing queues.
 */
static inline void list_add_tail(struct list_head *pnew, struct list_head *head) {
    __list_add(pnew, head->prev, head);
}

/**
 * list_move_tail - delete from one list and add to another's tail
 * @list: the entry to move
 * @head: the head that will follow our entry
 */
static inline void list_move_tail(struct list_head *list,
                                  struct list_head *head) {
    list_del_entry(list);
    list_add_tail(list, head);
}

// 检测链表是否为空
/**
 * list_empty - tests whether a list is empty
 * @head: the list to test.
 */
static inline int list_empty(const struct list_head *head) {
    return head->next == head;
}

// test whether a list is empty, with a lock
static inline int list_empty_atomic(const struct list_head *head, struct spinlock *mutex) {
    acquire(mutex);
    int ret = (head->next == head);
    release(mutex);
    return ret;
}


// 合并两个链表
// 将list链接到head之后
// add the list to the head->next
static inline void __list_splice(struct list_head *list,
                                 struct list_head *head) {
    struct list_head *first = list->next;
    struct list_head *last = list->prev;
    struct list_head *at = head->next;

    first->prev = head;
    head->next = first;

    last->next = at;
    at->prev = last;
}
/**
 * list_splice - join two lists
 * @list: the new list to add.
 * @head: the place to add it in the first list.
 */
static inline void list_splice(struct list_head *list, struct list_head *head) {
    if (!list_empty(list))
        __list_splice(list, head);
}

// mycode : join two lists given first pointer of head
static inline void list_join_given_first(struct list_head *first_new, struct list_head *first_old) {
    if (first_new != NULL && first_old != NULL) {
        struct list_head *last = first_new->prev;
        struct list_head *first = first_new;

        first_old->prev->next = first;
        first->prev = first_old->prev;

        last->next = first_old;
        first_old->prev = last;
    }
}

// 遍历链表
/**
 * list_first_entry - get the first element from a list
 * @ptr:	the list head to take the element from.
 * @type:	the type of the struct this is embedded in.
 * @member:	the name of the list_head within the struct.
 *
 * Note, that list is expected to be not empty.
 */
#define list_first_entry(ptr, type, member) \
    list_entry((ptr)->next, type, member)
/**
 * list_next_entry - get the next element in list
 * @pos:	the type * to cursor
 * @member:	the name of the list_head within the struct.
 */
#define list_next_entry(pos, member) \
    list_entry((pos)->member.next, typeof(*(pos)), member)

/**
 * list_last_entry - get the last element from a list
 * @ptr:	the list head to take the element from.
 * @type:	the type of the struct this is embedded in.
 * @member:	the name of the list_head within the struct.
 *
 * Note, that list is expected to be not empty.
 */
#define list_last_entry(ptr, type, member) \
    list_entry((ptr)->prev, type, member)
/**
 * list_prev_entry - get the prev element in list
 * @pos:	the type * to cursor
 * @member:	the name of the list_head within the struct.
 */
#define list_prev_entry(pos, member) \
    list_entry((pos)->member.prev, typeof(*(pos)), member)

/**
 * list_for_each	-	iterate over a list
 * @pos:	the &struct list_head to use as a loop cursor.
 * @head:	the head for your list.
 */
#define list_for_each(pos, head) \
    for (pos = (head)->next; pos != (head); pos = pos->next)

/**
 * list_entry - get the struct for this entry
 * @ptr:	the &struct list_head pointer.
 * @type:	the type of the struct this is embedded in.
 * @member:	the name of the list_head within the struct.
 */
#define list_entry(ptr, type, member) \
    container_of(ptr, type, member)
/**
 * list_for_each_entry	-	iterate over list of given type
 * @pos:	the type * to use as a loop cursor.
 * @head:	the head for your list.
 * @member:	the name of the list_head within the struct.
 */
#define list_for_each_entry(pos, head, member)               \
    for (pos = list_first_entry(head, typeof(*pos), member); \
         &pos->member != (head);                             \
         pos = list_next_entry(pos, member))

// given first
#define list_for_each_entry_given_first(pos, head_f, member, flag) \
    for (pos = head_f;                                             \
         flag || &pos->member != (&head_f->member);                \
         pos = list_next_entry(pos, member), flag = 0)

// 反向遍历链表
/**
 * list_for_each_entry_reverse - iterate backwards over list of given type.
 * @pos:	the type * to use as a loop cursor.
 * @head:	the head for your list.
 * @member:	the name of the list_head within the struct.
 */
#define list_for_each_entry_reverse(pos, head, member)      \
    for (pos = list_last_entry(head, typeof(*pos), member); \
         &pos->member != (head);                            \
         pos = list_prev_entry(pos, member))

// 正向安全遍历链表（遍历的同时删除节点）
/**
 * list_for_each_entry_safe - iterate over list of given type safe against removal of list entry
 * @pos:	the type * to use as a loop cursor.
 * @n:		another type * to use as temporary storage
 * @head:	the head for your list.
 * @member:	the name of the list_head within the struct.
 */
#define list_for_each_entry_safe(pos, n, head, member)       \
    for (pos = list_first_entry(head, typeof(*pos), member), \
        n = list_next_entry(pos, member);                    \
         &pos->member != (head);                             \
         pos = n, n = list_next_entry(n, member))

// mycode : given additonal condition of loop (not safe)
#define list_for_each_entry_condition(pos, head, member, condition) \
    for (pos = list_first_entry(head, typeof(*pos), member);        \
         &pos->member != (head) && (condition);                     \
         pos = list_next_entry(pos, member))

// mycode : given additional condition of loop (safe)
#define list_for_each_entry_safe_condition(pos, n, head, member, condition) \
    for (pos = list_first_entry(head, typeof(*pos), member),                \
        n = list_next_entry(pos, member);                                   \
         &pos->member != (head) && (condition);                             \
         pos = n, n = list_next_entry(n, member))

// given first head
#define list_for_each_entry_safe_given_first(pos, n, head_f, member, flag) \
    for (pos = head_f,                                                     \
        n = list_next_entry(pos, member);                                  \
         flag || &pos->member != (&head_f->member);                        \
         pos = n, n = list_next_entry(n, member), flag = 0)

// 反向安全遍历链表（反向遍历的同时删除节点）
#define list_for_each_entry_safe_reverse(pos, n, head, member) \
    for (pos = list_last_entry(head, typeof(*pos), member),    \
        n = list_prev_entry(pos, member);                      \
         &pos->member != (head);                               \
         pos = n, n = list_prev_entry(n, member))

#endif // __LIST_H