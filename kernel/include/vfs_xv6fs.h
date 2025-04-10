#ifndef __VFS_XV6FS_H
#define __VFS_XV6FS_H

int vfs_xv6fs_open(const char *path, int flags);
int vfs_xv6fs_close(int fd);
int vfs_xv6fs_read(int fd, void *buf, int count);
int vfs_xv6fs_write(int fd, const void *buf, int count);
struct inode* vfs_xv6fs_namei(char *path);
#endif