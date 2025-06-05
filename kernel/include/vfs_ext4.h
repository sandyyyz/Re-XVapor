#ifndef __VFS_EXT4_H
#define __VFS_EXT4_H

int vfs_ext4_open(const char *path, int flags);
void vfs_ext4_close(struct file* fp);
int vfs_ext4_read(struct file *fp, void *buf, int count);
int vfs_ext4_write(struct file *fp, const void *buf, int count);
struct inode *vfs_ext4_namei(const char *path);

#endif