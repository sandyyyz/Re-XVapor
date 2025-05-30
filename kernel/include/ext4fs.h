#ifndef __EXT4FS_H
#define __EXT4FS_H

#include "ext4_types.h"
#include "types.h"

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
int ext4_vwritev(struct file *fp, int user_src, uint64 iovec, int iovcnt, int *wcnt);
int ext4_visdir(const char *path);
int ext4_vlink(const char *oldpath, const char *newpath, int flags);
int ext4_vunlink(const char *path, int flags);
int ext4_vfaccess(char *path, int amode, int flags);
int ext4_vutimens(const char *path, __nullable const struct timespec *ts);
int ext4_vfile_exist(const char *path);

#endif