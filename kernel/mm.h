#ifndef __MM_H
#define __MM_H

#include "types.h"
#include "list.h"

typedef struct mm_struct {
    list_head_t vma_list;
    spinlock_t lock;
    pagetable_t pagetable; // user pagetable        
} mm_struct_t;
#endif