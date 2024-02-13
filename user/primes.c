#include "kernel/types.h"
#include "user/user.h"

#define READ_END 0
#define WRITE_END 1

void do_prime_sieve(int oldChan[2]) {
  int p, n;
  int newChan[2];

  /* `oldChan` is inhedited from its parent, just read from the pipe. */
  close(oldChan[WRITE_END]);
  /* nothing in the pipe */
  if (read(oldChan[READ_END], &p, 4) == 0) {
    close(oldChan[READ_END]);
    return;
  }
  // 当前未被剔除的最小数字是质数
  printf("prime %d\n", p);

  if (pipe(newChan) < 0) {
    fprintf(2, "pipe failed\n");
    exit(1);
  }

  /* create a new child as its right neighbour */
  if (fork() == 0) {
    // child
    do_prime_sieve(newChan);
  } else {
    // parent
    close(newChan[READ_END]);
    // 若当前数字不被筛出最小数字(质数)整除
    // 则当前数字为质数，加入newchan(筛出的数字)
    // 否则跳过
    while (read(oldChan[READ_END], &n, 4) != 0) {
      if (n % p != 0) {
        write(newChan[WRITE_END], &n, 4);
      }
    }
    /* close all holding pipes after use */
    close(oldChan[READ_END]);
    close(newChan[WRITE_END]);
  }
}

int main(int argc, char *argv[]) {
  int oldChan[2];
  if (pipe(oldChan) < 0) {
    fprintf(2, "pipe failed\n");
    exit(1);
  }

  if (fork() == 0) {
    // child
    do_prime_sieve(oldChan);
  } else {
    // parenet
    close(oldChan[READ_END]);
    for (int i = 2; i <= 35; ++i) {
      write(oldChan[WRITE_END], &i, 4);
    }
    close(oldChan[WRITE_END]);
  }

  wait((void *)0);
  exit(0);
}