#ifndef __PROCFS_H
#define __PROCFS_H

#include "file.h"

#define MAXINTR 16

int init_procfs();
void record_intr(uint64 intr);

#endif