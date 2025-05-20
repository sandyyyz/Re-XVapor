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

char *argv[] = {"/glibc/busybox_unstripped", "sh", "glibc/basic_testcode.sh",  NULL };
char path[] = "/glibc/busybox_unstripped";
char *envp[] = {0};
int main(void)
{
  dev(O_RDWR, CONSOLE, 0);
  dup(0); // stdout
  dup(0); // stderr
  printf("======================== init: starting rexvapor init !!! ========================\n");

  // if(openat(AT_FDCWD, "/dev/tty", O_RDWR, 0) < 0) {
  //   // while(1);
  //   if(mkdir("/dev", 0755) < 0) {
  //     printf("init: mkdir /dev failed\n");
  //     return -1;
  //   }
  //   // while(1);
  //   if(mknod("/dev/tty", S_IFCHR | S_IRUSR | S_IWUSR, CONSOLE) < 0) {
  //     printf("init: mknod tty failed\n");
  //     return -1;
  //   }
  // }
  // int pid = fork();
  // if(pid == 0) {
  //   printf("init: child process, pid = %d\n", getpid());
  //   execve(path, argv, envp);
  // } else {
  //   wait(0);
  // }
  execve(path, argv, envp);
  // while(1);
  return 0;
}
