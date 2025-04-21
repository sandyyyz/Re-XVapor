#ifndef __EXT4FS_H
#define __EXT4FS_H

#include "ext4_types.h"

struct ext4_minode {
    int ref;
    struct ext4_inode ext4_ientry;
};

int ext4_init();
int init_ext4fs();
struct inode *ext4_namei(char *rel_path);

#endif