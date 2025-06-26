// Format of an ELF executable file

#include "types.h"

#define ELF_MAGIC 0x464C457FU  // "\x7FELF" in little endian

// File header
struct elfhdr {
  uint magic;  // must equal ELF_MAGIC
  uchar elf[12];
  ushort type;
  ushort machine;
  uint version;
  uint64 entry;
  uint64 phoff;
  uint64 shoff;
  uint flags;
  ushort ehsize;
  ushort phentsize;
  ushort phnum;
  ushort shentsize;
  ushort shnum;
  ushort shstrndx;
};

// Program section header
struct proghdr {
  uint32 type;
  uint32 flags;
  uint64 off;
  uint64 vaddr;
  uint64 paddr;
  uint64 filesz;
  uint64 memsz;
  uint64 align;
};

// Values for Proghdr type
#define ELF_PROG_LOAD           1
#define ELF_PROG_DYNAMIC 2
#define ELF_PROG_INTERP 3
#define ELF_PROG_PHDR 6
#define ELF_PROG_TLS 7
#define ELF_GNU_STACK 0x6474e551
#define ELF_GNU_RELRO 0x6474e552

// Flag bits for Proghdr flags
#define ELF_PROG_FLAG_EXEC      1
#define ELF_PROG_FLAG_WRITE     2
#define ELF_PROG_FLAG_READ      4

// Env
#define MAX_ENV 8

// aux
#define MAX_AT 33
//重要且常见的类型值，动态链接器在启动时所需要的
/* Legal values for a_type (entry type).  */

#define AT_NULL		0		/* End of vector */
#define AT_IGNORE	1		/* Entry should be ignored */
#define AT_EXECFD	2		/* File descriptor of program */
#define AT_PHDR		3		/* Program headers for program */
#define AT_PHENT	4		/* Size of program header entry */
#define AT_PHNUM	5		/* Number of program headers */
#define AT_PAGESZ	6		/* System page size */
#define AT_BASE		7		/* Base address of interpreter */
#define AT_FLAGS	8		/* Flags */
#define AT_ENTRY	9		/* Entry point of program */
#define AT_NOTELF	10		/* Program is not ELF */
#define AT_UID		11		/* Real uid */
#define AT_EUID		12		/* Effective uid */
#define AT_GID		13		/* Real gid */
#define AT_EGID		14		/* Effective gid */
#define AT_CLKTCK	17		/* Frequency of times() */

/* Some more special a_type values describing the hardware.  */
#define AT_PLATFORM	15		/* String identifying platform.  */
#define AT_HWCAP	16		/* Machine-dependent hints about
					   processor capabilities.  */

/* This entry gives some information about the FPU initialization
   performed by the kernel.  */
#define AT_FPUCW	18		/* Used FPU control word.  */

/* Cache block sizes.  */
#define AT_DCACHEBSIZE	19		/* Data cache block size.  */
#define AT_ICACHEBSIZE	20		/* Instruction cache block size.  */
#define AT_UCACHEBSIZE	21		/* Unified cache block size.  */

/* A special ignored value for PPC, used by the kernel to control the
   interpretation of the AUXV. Must be > 16.  */
#define AT_IGNOREPPC	22		/* Entry should be ignored.  */

#define	AT_SECURE	23		/* Boolean, was exec setuid-like?  */

#define AT_BASE_PLATFORM 24		/* String identifying real platforms.*/

#define AT_RANDOM	25		/* Address of 16 random bytes.  */

#define AT_HWCAP2	26		/* More machine-dependent hints about
					   processor capabilities.  */

#define AT_RSEQ_FEATURE_SIZE	27	/* rseq supported feature size.  */
#define AT_RSEQ_ALIGN	28		/* rseq allocation alignment.  */

/* More machine-dependent hints about processor capabilities.  */
#define AT_HWCAP3	29		/* extension of AT_HWCAP.  */
#define AT_HWCAP4	30		/* extension of AT_HWCAP.  */

#define AT_EXECFN	31		/* Filename of executable.  */

/* Pointer to the global system page used for system calls and other
   nice things.  */
#define AT_SYSINFO	32
#define AT_SYSINFO_EHDR	33

/* Shapes of the caches.  Bits 0-3 contains associativity; bits 4-7 contains
   log2 of line size; mask those to get cache size.  */
#define AT_L1I_CACHESHAPE	34
#define AT_L1D_CACHESHAPE	35
#define AT_L2_CACHESHAPE	36
#define AT_L3_CACHESHAPE	37

/* Shapes of the caches, with more room to describe them.
   *GEOMETRY are comprised of cache line size in bytes in the bottom 16 bits
   and the cache associativity in the next 16 bits.  */
#define AT_L1I_CACHESIZE	40
#define AT_L1I_CACHEGEOMETRY	41
#define AT_L1D_CACHESIZE	42
#define AT_L1D_CACHEGEOMETRY	43
#define AT_L2_CACHESIZE		44
#define AT_L2_CACHEGEOMETRY	45
#define AT_L3_CACHESIZE		46
#define AT_L3_CACHEGEOMETRY	47

#define AT_MINSIGSTKSZ		51 /* Stack needed for signal delivery  */

