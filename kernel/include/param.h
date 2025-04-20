#ifndef __PARAM_H
#define __PARAM_H

#define NPROC        64  // maximum number of processes
#define NCPU          8  // maximum number of CPUs
#define NOFILE       16  // open files per process
#define NFILE       100  // open files per system
#define NINODE       50  // maximum number of active i-nodes
#define NDEV         10  // maximum major device number
#define ROOTDEV       1  // device number of file system root disk
#define BLOCKMAJOR   1  // major number of block device
#define MAXARG       32  // max exec arguments
#define MAXENV       8  // max exec environment variables
#define MAXOPBLOCKS  10  // max # of blocks any FS op writes
#define LOGSIZE      (MAXOPBLOCKS*3)  // max data blocks in on-disk log
#define NBUF         (MAXOPBLOCKS*3)  // size of disk block cache
#define FSSIZE       2000  // size of file system in blocks
#define MAXPATH      128   // maximum file path name
#define MAXVFSSIZE   4  // size of file system in blocks
#define MAXBDEV       4  // maximum numbers of block devices

#ifdef __USE_XV6FS
#define ROOTFSTYPE  "xv6fs"
#else
#define ROOTFSTYPE  "ext4"
#endif

#define NTHREADS_PER_PROC 2
#define NTHREADS (NPROC * NTHREADS_PER_PROC)

#define CLK_FREQ 10000000
#define TICKS_PER_SECOND 10  

#endif
