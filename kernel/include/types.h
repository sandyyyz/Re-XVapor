#ifndef __TYPES_H
#define __TYPES_H

typedef unsigned int   uint;
typedef unsigned short ushort;
typedef unsigned char   uchar;

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int  uint32;
typedef unsigned long uint64;
typedef long int64;

#if !(defined (off_t))
typedef long off_t;
#endif

#ifndef NULL
#define NULL ((void*)0)
#endif

// #if !(defined (__GNUG__) && defined (uintptr_t))
// typedef unsigned int uintptr_t;
// #endif

#if !(defined (__GNUG__) && defined (size_t))
typedef long unsigned int size_t;
#endif

typedef uint64 pde_t;
typedef int pid_t;
typedef int tid_t;
typedef int tgid_t;

/// used for sys_times
/* TODO: incompatiable with stdlib.h, change name temporary*/
typedef uint64 _clock_t;
typedef uint64 _time_t;
// #include <stdlib.h>

typedef uint64 pte_t;
typedef uint64 *pagetable_t; // 512 PTEs

// signal
typedef uint64 sig_t;

// mm
typedef uint64 paddr_t;
#endif

#define true 1
#define false 0

typedef uint32 dev_t;
typedef uint64 ino_t;
typedef unsigned int mode_t;
typedef uint32 nlink_t;
typedef uint32 uid_t;
typedef uint32 gid_t;
typedef uint32 blksize_t;
typedef uint64 blkcnt_t;
typedef uint64 time_t;

typedef unsigned char	cc_t;
typedef unsigned int	speed_t;
typedef unsigned int	tcflag_t;

#ifndef __user_space
#define __user_space
#endif

#ifndef __kernel_space
#define __kernel_space
#endif

#ifndef __nullable
#define __nullable
#endif

#ifndef __nonnull
#define __nonnull
#endif

// typedef uint32 dev_t;