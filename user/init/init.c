// init: The initial user-level program

#include "types.h"
#include "stat.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "xv6fs.h"
#include "file.h"
#include "user.h"
#include "fcntl.h"
#include "debug.h"

char *argv[] = { "sh", 0 };
char path[] = "/glibc/busybox_unstripped";
char *envp[] = {0};
int
main(void)
{
  dev(O_RDWR, CONSOLE, 0);
  dup(0); // stdout
  dup(0); // stderr
  printf("======================== init: starting xv6fs init ========================\n");
  execve(path, argv, envp);
  // int pid = fork();
  // if (pid == 0)
  // {
  //     execve(path,argv, envp);
  // }
  // else
  // {
  //     wait(0);
  // }

  while(1);
  return 0;
}
