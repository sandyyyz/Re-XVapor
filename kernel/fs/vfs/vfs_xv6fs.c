#include "types.h"
#include "vfs_xv6fs.h"
#include "vfs.h"
#include "defs.h"
#include "param.h"


int vfs_xv6fs_open(const char *path, int flags);
int vfs_xv6fs_fileread(struct file *f, void *buf, int count);
int vfs_xv6fs_filewrite(struct file* f, const void *buf, int count);
int vfs_xv6fs_readi(struct inode *ip, int user_dst, uint64 dst, uint off, uint n);
void vfs_xv6fs_iput(struct inode *ip);
void vfs_xv6fs_fileclose(struct file *f);

struct file_ops xv6fs_fops = {
    .open = vfs_xv6fs_open,
    .close = vfs_xv6fs_fileclose,
    .read = vfs_xv6fs_fileread,
    .write = vfs_xv6fs_filewrite,

};

struct inode_ops xv6fs_inode_ops = {
    .readi = vfs_xv6fs_readi,
};
int vfs_xv6fs_open(const char *path, int flags) {
    // Open the xv6fsFS filesystem
    // This function should locate the file in the xv6fsFS filesystem and return a file descriptor
    // For simplicity, we will just return a dummy file descriptor
    return 0;
}

// Close file f.  (Decrement ref count, close when reaches 0.)
void
vfs_xv6fs_fileclose(struct file *f)
{
    fileclose(f);
}

int vfs_xv6fs_fileread(struct file *f, void *buf, int count) {
    int ret = fileread(f, ADDR2N(buf), count);
    return ret;
}

int vfs_xv6fs_filewrite(struct file* f, const void *buf, int count) {
    int ret = filewrite(f, ADDR2N(buf), count);
    return ret;
}

int vfs_xv6fs_readi(struct inode *ip, int user_dst, uint64 dst, uint off, uint n) {
    // Read data from the xv6fsFS filesystem
    // This function should read 'n' bytes from the inode 'ip' into 'dst'
    // For simplicity, we will just return the number of bytes read
    return n;
}

struct inode* vfs_xv6fs_namei(char *path) {
    struct inode *ip = namei(path);
    if (ip == NULL) {
        return NULL;
    }
    return ip;
}

void vfs_xv6fs_iput(struct inode *ip) {
    iput(ip);
    return;
}

