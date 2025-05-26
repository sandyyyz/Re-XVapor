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
char musl_basic_dir[] = "/musl/basic";
char glibc_basic_dir[] = "/glibc/basic";
char musl_dir[] = "/musl";
char glibc_dir[] = "/glibc";
char glibc_busybox_path[] = "/glibc/busybox";

char *glibc_basic_test_argv[] = {"/glibc/busybox", "sh", "/glibc/basic/run-all.sh", NULL };
char *glibc_busybox_test_argv[] = {"/glibc/busybox", "sh", "/glibc/busybox_testcode.sh", NULL };
char *musl_basic_test_argv[] = {"/glibc/busybox", "sh", "/musl/basic/run-all.sh", NULL };
char *musl_busybox_test_argv[] = {"/glibc/busybox", "sh", "/musl/busybox_testcode.sh", NULL };
char *shell_argv[] = {"/glibc/busybox", "sh", NULL };
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
    if(chdir(glibc_basic_dir) < 0) {
      printf("init: chdir %s failed\n", glibc_basic_dir);
      exit(-1);
    }
    execve(glibc_busybox_path, glibc_basic_test_argv, busybox_envp);
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
