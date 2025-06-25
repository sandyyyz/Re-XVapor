#ifndef __PARAM_H
#define __PARAM_H

#define NPROC        16  // maximum number of processes
#define NCPU          8  // maximum number of CPUs
#define NOFILE       256  // open files per process
#define NFILE       1024  // open files per system
#define NINODE       50  // maximum number of active i-nodes
#define NDEV         10  // maximum major device number
#define ROOTDEV       1  // device number of file system root disk
#define BLOCKMAJOR   1  // major number of block device
#define MAXARG       512  // max exec arguments
#define MAXENV       8  // max exec environment variables
#define MAXOPBLOCKS  10  // max # of blocks any FS op writes
#define LOGSIZE      (MAXOPBLOCKS*3)  // max data blocks in on-disk log
#define NBUF         (MAXOPBLOCKS*3)  // size of disk block cache
#define FSSIZE       2000  // size of file system in blocks
#define MAXPATH      256   // maximum file path name
#define MAXVFSSIZE   4  // size of file system in blocks
#define MAXBDEV       4  // maximum numbers of block devices

#define NPROCID     1 << 16 // maximum number of process IDs
#define NPROC_GROUP  64 // maximum number of process groups

#define ROOTFSTYPE  "ext4"
#define NTHREADS_PER_PROC 4
#define NTHREADS (NPROC * NTHREADS_PER_PROC)

#define INT_MAX 2147483647 // maximum value for int
#define INT_MIN (-INT_MAX - 1) // minimum value for int

#endif
