#ifndef __STAT_H
#define __STAT_H

#define T_DIR     1   // Directory
#define T_FILE    2   // File
#define T_DEVICE  3   // Device
#define T_MOUNT   4   // Mount point

#include "timer.h"

struct kstat {
  dev_t     st_dev;     /* ID of device containing file */
  ino_t     st_ino;     /* inode number */
  mode_t    st_mode;    /* protection */
  nlink_t   st_nlink;   /* number of hard links */
  uid_t     st_uid;     /* user ID of owner */
  gid_t     st_gid;     /* group ID of owner */
  dev_t     st_rdev;    /* device ID (if special file) */
  off_t     st_size;    /* total size, in bytes */
	blksize_t st_blksize; /* blocksize for file system I/O */
	uint32 __pad2;
	blkcnt_t st_blocks; /* number of 512B blocks allocated */
	time_t st_atime_sec;
	time_t st_atime_nsec;
	time_t st_mtime_sec;
	time_t st_mtime_nsec;
	time_t st_ctime_sec;
	time_t st_ctime_nsec;
	unsigned __unused[2];
};

// for inode
struct stat {
  dev_t st_dev; /* ID of device containing file */
  ino_t st_ino; /* Inode number */
  mode_t st_mode; /* File type and mode */
  nlink_t st_nlink; /* Number of hard links */
  uid_t st_uid; /* User ID of owner */
  gid_t st_gid; /* Group ID of owner */
  dev_t st_rdev; /* Device ID (if special file) */
  uint16 __pad2;
  off_t st_size; /* Total size, in bytes */
  blksize_t st_blksize; /* Block size for filesystem I/O */
  blkcnt_t st_blocks; /* Number of 512B blocks allocated */

  /* Since Linux 2.6, the kernel supports nanosecond
      precision for the following timestamp fields.
      For the details before Linux 2.6, see NOTES. */

  struct timespec st_atim; /* Time of last access */
  struct timespec st_mtim; /* Time of last modification */
  struct timespec st_ctim; /* Time of last status change */
  unsigned __unused[2];

#define st_atime st_atim.tv_sec /* Backward compatibility */
#define st_mtime st_mtim.tv_sec
#define st_ctime st_ctim.tv_sec
};

#define __unused __attribute__((__unused__))
#endif