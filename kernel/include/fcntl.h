#ifndef __FCNTL_H
#define __FCNTL_H

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

#endif
