#ifndef __DIRENT_H
#define __DIRENT_H

#include "types.h"

struct linux_dirent64 {
    uint64 d_ino;
    int64 d_off;
    unsigned short d_reclen;
    unsigned char d_type;
    char d_name[0];
};

#endif