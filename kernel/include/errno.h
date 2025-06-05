#ifndef __ERRNO_H
#define __ERRNO_H

#ifndef EPERM
#define EPERM 1      /* Operation not permitted */
#endif

#ifndef ENOENT
#define ENOENT 2     /* No such file or directory */
#endif

#ifndef EIO
#define EIO 5        /* I/O error */
#endif

#ifndef ENXIO
#define ENXIO 6      /* No such device or address */
#endif

#ifndef E2BIG
#define E2BIG 7      /* Argument list too long */
#endif

#ifndef ENOMEM
#define ENOMEM 12    /* Out of memory */
#endif

#ifndef EACCES
#define EACCES 13    /* Permission denied */
#endif

#ifndef EFAULT
#define EFAULT 14    /* Bad address */
#endif

#ifndef EEXIST
#define EEXIST 17    /* File exists */
#endif

#ifndef ENODEV
#define ENODEV 19    /* No such device */
#endif

#ifndef ENOTDIR
#define ENOTDIR 20   /* Not a directory */
#endif

#ifndef EISDIR
#define EISDIR 21    /* Is a directory */
#endif

#ifndef EINVAL
#define EINVAL 22    /* Invalid argument */
#endif

#ifndef EFBIG
#define EFBIG 27     /* File too large */
#endif

#ifndef ENOSPC
#define ENOSPC 28    /* No space left on device */
#endif

#ifndef EROFS
#define EROFS 30     /* Read-only file system */
#endif

#ifndef EMLINK
#define EMLINK 31    /* Too many links */
#endif

#ifndef ERANGE
#define ERANGE 34    /* Math result not representable */
#endif

#ifndef ENOTEMPTY
#define ENOTEMPTY 39 /* Directory not empty */
#endif

#ifndef ENODATA
#define ENODATA 61   /* No data available */
#endif

#ifndef ENOTSUP
#define ENOTSUP 95   /* Not supported */
#endif

#endif