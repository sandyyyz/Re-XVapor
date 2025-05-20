#ifndef __EXT4FS_H
#define __EXT4FS_H

#include "ext4_types.h"

struct ext4_minode {
    int ref;
    struct ext4_inode ext4_ientry;
};

struct ext4_mfile {
    int ref;
    struct ext4_file ext4_fentry;
};

int ext4_init();
int init_ext4fs();
struct inode *ext4_namei(char *rel_path);
void ext4_ilock(struct inode *ip);
int ext4_vfread(struct file *fp, int user_dst, uint64 dst, uint off, uint size, int *rcnt);
int ext4_vfopen(struct file *fp, const char *path, int flags);
int ext4_vfclose(struct file *fp);
int ext4_vfstat(struct file *f, struct kstat *st);
int ext4_vstat(char *path, struct kstat *st);
int ext4_vcleansf(struct file *fp);
int ext4_temp_vgentdents(struct file *fp, __user_space struct linux_dirent64 *u_dirp, int count);

#endif