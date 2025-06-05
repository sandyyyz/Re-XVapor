#include "types.h"
#include "stat.h"
#include "user.h"
#include "xv6fs.h"
#include "fcntl.h"

int
main(int argc, char *argv[])
{
  int fd, i;
  char path[] = "stressfs0";
  char data[512];

  printf("writetest starting\n");
  memset(data, 'a', sizeof(data));

  for(i = 0; i < 2; i++)
    if(fork() > 0)
      break;

//   printf("write %d\n", i);

  path[8] += i;
  fd = open(path, O_CREATE | O_RDWR);
//   printf("open done\n");
  for(i = 0; i < 20; i++) {
    write(fd, data, sizeof(data));
    // sleep(1);
    // printf("write done %d\n",i);
  }
//    printf(fd, "%d\n", i);


  close(fd);
  printf("close\n");

  printf("read\n");

  fd = open(path, O_RDONLY);
  for (i = 0; i < 20; i++)
    read(fd, data, sizeof(data));
  close(fd);

  wait(0);
  printf("wait done\n");

  exit(0);
}
