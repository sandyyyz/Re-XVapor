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
char basic_dir[] = "/musl/basic";
char *basic_argv[] = {"/glibc/busybox", "sh", "/musl/basic/run-all.sh", NULL };
char busybox_path[] = "/glibc/busybox";
char *busybox_envp[] = {"PATH=/",0};
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
  //     return -1;`
  //   }
  //   // while(1);
  //   if(mknod("/dev/tty", S_IFCHR | S_IRUSR | S_IWUSR, CONSOLE) < 0) {
  //     printf("init: mknod tty failed\n");
  //     return -1;
  //   }
  // }
  int pid = fork();
  if(pid == 0) {
    printf("init: child process, pid = %d\n", getpid());
    if(chdir(basic_dir) < 0) {
      printf("init: chdir /musl failed\n");
      exit(-1);
    }
    execve(busybox_path, basic_argv, busybox_envp);
    exit(0);
  } else {
    wait(0);
    printf("child process exited, pid = %d\n", pid);
    exit(0);
  }
  // execve(path, argv, envp);
  // while(1);

  exit(0);
}
