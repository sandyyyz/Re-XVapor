#ifndef __FCNTL_H
#define __FCNTL_H

#include "types.h"
#include "file.h"

#ifndef O_RDONLY
#define O_RDONLY  0x000
#endif

#ifndef O_WRONLY
#define O_WRONLY  0x001
#endif

#ifndef O_RDWR
#define O_RDWR    0x002
#endif

#ifndef O_TRUNC
#define O_TRUNC   0x400
#endif

#ifndef O_CREATE
#define O_CREATE  0x200
#endif


// utimensat
#define AT_FDCWD -100
#define AT_REMOVEDIR 0x200
#define AT_SYMLINK_NOFOLLOW 0x100 /* Do not follow symbolic links.  */
#define AT_SYMLINK_FOLLOW 0x400 /* Follow symbolic links.  */


/* Values for the second argument to `fcntl'.  */
#define F_DUPFD		0	/* Duplicate file descriptor.  */
#define F_GETFD		1	/* Get file descriptor flags.  */
#define F_SETFD		2	/* Set file descriptor flags.  */
#define F_GETFL		3	/* Get file status flags.  */
#define F_SETFL		4	/* Set file status flags.  */

/*         F_DUPFD (int)
              Duplicate the file descriptor fd using the lowest-numbered
              available file descriptor greater than or equal to arg.
              This is different from dup2(2), which uses exactly the file
              descriptor specified.

              On success, the new file descriptor is returned.

              See dup(2) for further details.

       F_DUPFD_CLOEXEC (int; since Linux 2.6.24)
              As for F_DUPFD, but additionally set the close-on-exec flag
              for the duplicate file descriptor.  Specifying this flag
              permits a program to avoid an additional fcntl() F_SETFD
              operation to set the FD_CLOEXEC flag.  For an explanation
              of why this flag is useful, see the description of
              O_CLOEXEC in open(2).
 */
#define F_DUPFD_CLOEXEC 1030

int do_fcntl(struct file *f, int cmd, uint64 arg);

#endif
