#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"

void find(char *path, char *name) {
  int fd;
  struct stat st;
  struct dirent de;

  char buf[512];
  int len;

  if ((fd = open(path, 0)) < 0) {
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }

  if (fstat(fd, &st) < 0) {
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    exit(1);
  }

  if (st.type != T_DIR) {
    close(fd);
    return;
  }

  // 遍历文件中所有目录项
  while (read(fd, &de, sizeof(de)) == sizeof(de)) {
    if (de.inum == 0)
      continue;

    if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0) {
      continue;
    }

    if (strcmp(de.name, name) == 0) {
      printf("%s%s\n", path, name);
      continue;
    }

    memmove(buf, path, strlen(path));
    memmove(buf + strlen(path), de.name, strlen(de.name));
    len = strlen(path) + strlen(de.name);
    if (buf[len - 1] != '/') {
      buf[len] = '/';
      buf[len + 1] = '\0';
    } else {
      buf[len] = '\0';
    }
    // 递归在子目录中寻找
    find(buf, name);
  }

  close(fd);
}

int main(int argc, char *argv[]) {
  char buf[DIRSIZ + 1];
  char *pathname;
  int pathlen;

  if (argc != 3) {
    fprintf(2, "Usage: find [dir] [filename]\n");
    exit(1);
  }

  pathname = argv[1];
  pathlen = strlen(pathname);
  if (pathname[pathlen - 1] != '/') {
    memmove(buf, pathname, pathlen);
    buf[pathlen] = '/';
    buf[pathlen + 1] = '\0';
  }

  find(buf, argv[2]);
  exit(0);
}