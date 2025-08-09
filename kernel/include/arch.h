#ifndef __ARCH_H
#define __ARCH_H

#ifdef __ARCH_LOONGARCH
#include "loongarch.h"
#else
#include "riscv.h"
#endif
#endif  