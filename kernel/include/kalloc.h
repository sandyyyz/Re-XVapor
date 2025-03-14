#ifndef __KALLOC_H
#define __KALLOC_H

// kalloc.c
void*           kalloc(void);
void            kfree(void *);
void            kinit(void);
void*           kzalloc(void); 

#endif