#ifndef __TCB_H
#define __TCB_H

#include "list.h"

struct tcb{
    char name[20];
    list_head_t threads;
    list_head_t wait_list;
    list_head_t state_list;
};

#endif