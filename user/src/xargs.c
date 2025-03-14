#include "types.h"
#include "user.h"
#include "param.h"

#define STDIN 0

#define BUFFER_SIZE 512

int main(int argc, char *argv[]) {
  char buf[BUFFER_SIZE];
  char *newargv[MAXARG];
  int i = 0, j;

  if (argc < 2) {
    fprintf(2, "Usage: xargs [options] [command [initial-arguments]]\n");
    exit(1);
  }

  while (read(STDIN, &buf[i], 1) != 0) {
    if (buf[i] == '\n') {
      buf[i] = '\0';
      i = 0;
      for (j = 0; j + 1 < argc; j++) {
        newargv[j] = argv[j + 1];
      }
      newargv[j++] = buf;
      newargv[j] = (void *)0;

      if (fork() == 0) {
        exec(argv[1], newargv);
      } else {
        wait((void *)0);
      }
    } else {
      ++i;
    }
  }
  exit(0);
}