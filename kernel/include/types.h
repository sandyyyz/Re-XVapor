#ifndef __TYPES_H
#define __TYPES_H


typedef unsigned char		u8;
typedef u8			        __u8;
typedef unsigned short		u16;
typedef u16                 __u16;
typedef unsigned int		u32;
typedef u32                 __u32;
typedef unsigned long long	u64;
typedef u64			        __u64;

typedef signed short		s16;
typedef signed long long    __s64;

typedef unsigned int   uint;
typedef unsigned short ushort;
typedef unsigned char   uchar;

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int  uint32;
typedef unsigned long uint64;
typedef uint64 uint64_t;

typedef uint32 uint32_t;

typedef uint16 uint16_t;

typedef uint8 uint8_t;

typedef int int32_t;

typedef short int16_t;
typedef long int64;
typedef int64 int64_t;

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

typedef long ssize_t;

typedef uint64 pde_t;
typedef int pid_t;
typedef int tid_t;
typedef int tgid_t;

/// used for sys_times
/* TODO: incompatiable with stdlib.h, change name temporary*/
typedef uint64 _clock_t;
typedef long _time_t;
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
typedef long int blksize_t;
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

#ifdef __CHECKER
#define __bitwise	__attribute__((bitwise))
#else
#define __bitwise
#endif

typedef unsigned int __bitwise gfp_t;

#define __aligned_u64 __u64 __attribute__((aligned(8)))

#ifndef INT_MAX
#define INT_MAX			((int)(~0U>>1))
#endif

#ifndef UINT32_MAX
#define UINT32_MAX		((u32)~0U)
#endif

#ifndef INT32_MAX
#define INT32_MAX		((s32)(UINT32_MAX >> 1))
#endif

#ifdef CONFIG_64BIT
typedef struct {
	s64 counter;
} atomic64_t;
#endif

#define _ULCAST_ (unsigned long)
#define _U64CAST_ (u64)

/* The kernel doesn't use this legacy form, but user space does */
#define __bitwise__ __bitwise

typedef __u16 __bitwise __le16;
typedef __u16 __bitwise __be16;
typedef __u32 __bitwise __le32;
typedef __u32 __bitwise __be32;
typedef __u64 __bitwise __le64;
typedef __u64 __bitwise __be64;

typedef __u16 __bitwise __sum16;
typedef __u32 __bitwise __wsum;

#define __aligned_u64 __u64 __attribute__((aligned(8)))
#define __aligned_be64 __be64 __attribute__((aligned(8)))
#define __aligned_le64 __le64 __attribute__((aligned(8)))

typedef unsigned __bitwise __poll_t;


#define __DEV_T_TYPE		__UQUAD_TYPE
#define __UID_T_TYPE		__U32_TYPE
#define __GID_T_TYPE		__U32_TYPE
#define __INO_T_TYPE		__SYSCALL_ULONG_TYPE
#define __INO64_T_TYPE		__UQUAD_TYPE
#define __MODE_T_TYPE		__U32_TYPE
#ifdef __x86_64__
# define __NLINK_T_TYPE		__SYSCALL_ULONG_TYPE
# define __FSWORD_T_TYPE	__SYSCALL_SLONG_TYPE
#else
# define __NLINK_T_TYPE		__UWORD_TYPE
# define __FSWORD_T_TYPE	__SWORD_TYPE
#endif
