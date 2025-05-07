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
int
main(void)
{
  printf("init: starting xv6fs init\n");
  exec(path, argv);
  while(1);
  return 0;
}
