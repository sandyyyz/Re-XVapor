#include "types.h"
#include "vfs.h"
#include "debug.h"
#include "defs.h"
#include "spinlock.h"
#include "vfs_xv6fs.h"
#include "vfs_ext4.h"
#include "param.h"

extern struct file_ops ext4_fops;
extern struct file_ops xv6fs_fops;

struct vfs_filesystem *vfs_fs[VFS_MAXFS];

struct file_ops *vfs_fops[VFS_MAXFS] = {
    [VFS_TYPE_UNKNOWN] = NULL,
    [VFS_TYPE_EXT4] = &ext4_fops,
    [VFS_TYPE_XV6FS] = &xv6fs_fops,
};

struct {
    struct spinlock lock;
    struct mount_point mount_points[MAX_MOUNTS];
} vfs_mount_table;

void mount_init() {
    initlock(&vfs_mount_table.lock, "vfs_mount_table");
    for (int i = 0; i < MAX_MOUNTS; i++) {
        vfs_mount_table.mount_points[i].mp = NULL;
        vfs_mount_table.mount_points[i].dev = -1;
        vfs_mount_table.mount_points[i].type = VFS_TYPE_UNKNOWN;
    }
}


void vfs_init() {
    mount_init();
}


/**
 * @brief get fs by type
 * 
 * @param type fs_type
 * @return struct vfs_filesystem* pointer to the filesystem structure , null if not found
 */
struct vfs_filesystem *vfs_getfs_bytype(vfs_type_t type) {
    for (int i = 0; i < VFS_MAXFS; i++) {
        if (vfs_fs[i] && vfs_fs[i]->type == type) {
            return vfs_fs[i];
        }
    }
    return NULL;
}

struct vfs_filesystem *vfs_getfs_bydev(int dev) {
    for (int i = 0; i < VFS_MAXFS; i++) {
        if (vfs_fs[i] && vfs_fs[i]->dev == dev) {
            return vfs_fs[i];
        }
    }
    return NULL;
}
struct vfs_filesystem *vfs_getfs_byname(const char *name) {
    for (int i = 0; i < VFS_MAXFS; i++) {
        if (vfs_fs[i] && strcmp(vfs_fs[i]->name, name) == 0) {
            return vfs_fs[i];
        }
    }
    return NULL;
}

struct vfs_filesystem * vfs_resolve_fs(const char* path) {
    vfs_type_t selected = VFS_TYPE_UNKNOWN;
    int longest_match_len = -1;

    acquire(&vfs_mount_table.lock);
    for (int i = 0; i < MAX_MOUNTS; i++) {
        const char* mp = vfs_mount_table.mount_points[i].mp;
        int len = strlen(mp);

        if (strncmp(path, mp, len) == 0 &&
            (path[len] == '/' || path[len] == '\0')) {
            if (len > longest_match_len) {
                longest_match_len = len;
                selected = vfs_mount_table.mount_points[i].type;
            }
        }
    }
    release(&vfs_mount_table.lock);

    return vfs_getfs_bytype(selected);
}

/**
 * @brief Get the absolute path of a file
 * 
 * @param path path to the file
 * @param cwd current working directory
 * @param absolute_path pointer to the buffer to store the absolute path
 */
void get_absolute_path(const char *path, const char *cwd, char *absolute_path) {
    if (path == NULL) {
        strncpy(absolute_path, cwd, strlen(cwd));
    } else if (path[0] == '/') {
        strcpy(absolute_path, path);
    } else {
        strcpy(absolute_path, cwd);
        strcat(absolute_path, "/");
        strcat(absolute_path, path);
    }
    // handle ./ and ../
    char *p = absolute_path;
    while (*p != '\0') {
        if (*p == '.' && *(p + 1) == '/') {
            strcpy(p, p + 2);
        } else if (*p == '.' && *(p + 1) == '.' && *(p + 2) == '/') {
            char *q = p - 2;
            while (q >= absolute_path && *q != '/') {
                q--;
            }
            if (q >= absolute_path) {
                strcpy(q + 1, p + 3);
                p = q;
            } else {
                strcpy(absolute_path, p + 3);
                p = absolute_path;
            }
        } else {
            p++;
        }
    }

    /* handle trailing . and .. */
    p = absolute_path + strlen(absolute_path) - 2;
    if ((*p == '/' || *p == '.') && *(p + 1) == '.' && *(p + 2) == '\0') {
        *p = *(p + 1) = *(p + 2) = '\0';
    }
    p--;
    if (*p == '/' && *(p + 1) == '.' && *(p + 2) == '.' && *(p + 3) == '\0') {
        if (p == absolute_path) {
            *(p + 1) = *(p + 2) = *(p + 3) = '\0';
            return;
        }
        char *q = p - 1;
        while (q >= absolute_path && *q != '/') {
            q--;
        }
        if (q >= absolute_path) {
            *q = '\0';
            p = q;
        } else {
            strcpy(absolute_path, p + 4);
            p = absolute_path;
        }
    }


    while (absolute_path[0] == '/' && absolute_path[1] == '/') {
        strcpy(absolute_path, absolute_path + 1);
    }
    size_t len = strlen(absolute_path);
    if (absolute_path[len - 1] == '/') {
        absolute_path[len - 1] = '\0';
        --len;
    }
    if (strlen(absolute_path) == 0) {
        strcpy(absolute_path, "/");
    } else if (absolute_path[0] != '/') {
        size_t len2 = strlen(absolute_path);
        char *x = absolute_path + len2;
        *(x + 1) = 0;
        while (x > absolute_path) {
            *x = *(x - 1);
            x--;
        }
        *x = '/';
    }
}

struct inode* vfs_namei(char *path) {
    struct vfs_filesystem *fs = vfs_resolve_fs(path);
    if (fs == NULL) {
        return NULL;
    }
    switch (fs->type) {
        case VFS_TYPE_EXT4:
            return vfs_ext4_namei(path);
        case VFS_TYPE_XV6FS:
            return vfs_xv6fs_namei(path);
        default:
            return NULL;
    }
}