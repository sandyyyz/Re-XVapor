#ifndef __STAT_H
#define __STAT_H

#define T_UNKNOWN 0
#define T_FIFO 1
#define T_CHR 2
#define T_DIR 4
#define T_BLK 6
#define T_REG 8
#define T_LNK 10
#define T_SOCK 12
#define T_WHT 14

// for xv6fs
// #define T_DIR     15   // Directory
#define T_FILE    16   // File
#define T_DEVICE  17   // Device
#define T_MOUNT   18   // Mount point

#include "timer.h"

#ifndef __unused
#define __unused __attribute__((__unused__))
#endif

#define S_IFMT  00170000    /* bit mask for file type */
#define S_IFSOCK 0140000    /* socket */
#define S_IFLNK	 0120000    /* symbolic link */
#define S_IFREG  0100000    /* regular file */
#define S_IFBLK  0060000    /* block device */
#define S_IFDIR  0040000    /* directory */
#define S_IFCHR  0020000    /* character device */
#define S_IFIFO  0010000    /* FIFO */
#define S_ISUID  0004000    /* set-user-ID bit */
#define S_ISGID  0002000    /* set-group-ID bit */
#define S_ISVTX  0001000    /* sticky bit */

#define S_ISLNK(m)	(((m) & S_IFMT) == S_IFLNK)
#define S_ISREG(m)	(((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m)	(((m) & S_IFMT) == S_IFDIR)
#define S_ISCHR(m)	(((m) & S_IFMT) == S_IFCHR)
#define S_ISBLK(m)	(((m) & S_IFMT) == S_IFBLK)
#define S_ISFIFO(m)	(((m) & S_IFMT) == S_IFIFO)
#define S_ISSOCK(m)	(((m) & S_IFMT) == S_IFSOCK)

#define S_IRWXU 00700   /* owner has read, write, and execute permission */
#define S_IRUSR 00400   /* owner has read permission */
#define S_IWUSR 00200   /* owner has write permission */
#define S_IXUSR 00100   /* owner has execute permission */

#define S_IRWXG 00070   /* group has read, write, and execute permission */
#define S_IRGRP 00040   /* group has read permission */
#define S_IWGRP 00020   /* group has write permission */
#define S_IXGRP 00010   /* group has execute permission */

#define S_IRWXO 00007   /* other has read, write, and execute permission */
#define S_IROTH 00004   /* other has read permission */
#define S_IWOTH 00002   /* other has write permission */
#define S_IXOTH 00001   /* other has execute permission */

// struct kstat {
//   uint64     st_dev;     /* ID of device containing file */
//   ino_t     st_ino;     /* inode number */
//   mode_t    st_mode;    /* protection */
//   nlink_t   st_nlink;   /* number of hard links */
//   uid_t     st_uid;     /* user ID of owner */
//   gid_t     st_gid;     /* group ID of owner */
//   dev_t     st_rdev;    /* device ID (if special file) */
//   off_t     st_size;    /* total size, in bytes */
// 	blksize_t st_blksize; /* blocksize for file system I/O */
// 	uint32 __pad2;
// 	blkcnt_t st_blocks; /* number of 512B blocks allocated */
// 	time_t st_atime_sec;
// 	time_t st_atime_nsec;
// 	time_t st_mtime_sec;
// 	time_t st_mtime_nsec;
// 	time_t st_ctime_sec;
// 	time_t st_ctime_nsec;
// 	// unsigned __unused u[2];
// };

struct kstat {
  uint64     st_dev;     /* ID of device containing file */
  ino_t     st_ino;     /* inode number */
  mode_t    st_mode;    /* protection */
  nlink_t   st_nlink;   /* number of hard links */
  uid_t     st_uid;     /* user ID of owner */
  gid_t     st_gid;     /* group ID of owner */
  dev_t     st_rdev;    /* device ID (if special file) */
  unsigned long long __pad;
  off_t     st_size;    /* total size, in bytes */
	blksize_t st_blksize; /* blocksize for file system I/O */
  int __pad2;
	blkcnt_t st_blocks; /* number of 512B blocks allocated */
	time_t st_atime_sec;
	time_t st_atime_nsec;
	time_t st_mtime_sec;
	time_t st_mtime_nsec;
	time_t st_ctime_sec;
	time_t st_ctime_nsec;
  unsigned int unused[2];
};

struct stat {
  uint64     st_dev;     /* ID of device containing file */
  ino_t     st_ino;     /* inode number */
  mode_t    st_mode;    /* protection */
  nlink_t   st_nlink;   /* number of hard links */
  uid_t     st_uid;     /* user ID of owner */
  gid_t     st_gid;     /* group ID of owner */
  dev_t     st_rdev;    /* device ID (if special file) */
  unsigned long long __pad;
  off_t     st_size;    /* total size, in bytes */
	blksize_t st_blksize; /* blocksize for file system I/O */
  int __pad2;
	blkcnt_t st_blocks; /* number of 512B blocks allocated */
	time_t st_atime_sec;
	time_t st_atime_nsec;
	time_t st_mtime_sec;
	time_t st_mtime_nsec;
	time_t st_ctime_sec;
	time_t st_ctime_nsec;
  unsigned int unused[2];
};


// // for inode
// struct stat {
//   dev_t st_dev; /* ID of device containing file */
//   ino_t st_ino; /* Inode number */
//   mode_t st_mode; /* File type and mode */
//   nlink_t st_nlink; /* Number of hard links */
//   uid_t st_uid; /* User ID of owner */
//   gid_t st_gid; /* Group ID of owner */
//   dev_t st_rdev; /* Device ID (if special file) */
//   uint16 __pad2;
//   off_t st_size; /* Total size, in bytes */
//   blksize_t st_blksize; /* Block size for filesystem I/O */
//   blkcnt_t st_blocks; /* Number of 512B blocks allocated */

//   /* Since Linux 2.6, the kernel supports nanosecond
//       precision for the following timestamp fields.
//       For the details before Linux 2.6, see NOTES. */

//   struct timespec st_atim; /* Time of last access */
//   struct timespec st_mtim; /* Time of last modification */
//   struct timespec st_ctim; /* Time of last status change */
//   // unsigned __unused u[2];

// #define st_atime st_atim.tv_sec /* Backward compatibility */
// #define st_mtime st_mtim.tv_sec
// #define st_ctime st_ctim.tv_sec
// };

typedef struct {
  int val[2];
} __kernel_fsid_t;
typedef __kernel_fsid_t fsid_t;
typedef uint64 fsblkcnt_t;
typedef uint64 fsfilcnt_t;
struct statfs {
    uint64 f_type; /* type of file system (see below) */
    uint64 f_bsize; /* optimal transfer block size */
    fsblkcnt_t f_blocks; /* total data blocks in file system */
    fsblkcnt_t f_bfree; /* free blocks in fs */
    fsblkcnt_t f_bavail; /* free blocks available to
                            unprivileged user */
    fsfilcnt_t f_files; /* total file nodes in file system */
    fsfilcnt_t f_ffree; /* free file nodes in fs */
    fsid_t f_fsid; /* file system ID */
    uint64 f_namelen; /* maximum length of filenames */
    uint64 f_frsize; /* fragment size (since Linux 2.6) */
    uint64 f_flags; /* mount flags of filesystem (since Linux 2.6.36) */
    uint64 f_spare[4]; /* padding for future expansion */
};


// for utimensat
#define UTIME_NOW ((1l << 30) - 1l)
#define UTIME_OMIT ((1l << 30) - 2l)


#endif