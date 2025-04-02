#ifndef __VFS_EXT4_H
#define __VFS_EXT4_H

int vfs_ext4_open(const char *path, int flags);
int vfs_ext4_close(int fd);
int vfs_ext4_read(int fd, void *buf, int count);
int vfs_ext4_write(int fd, const void *buf, int count);

#endif