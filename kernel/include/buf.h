#ifndef __BUF_H
#define __BUF_H
#include "sleeplock.h"

#define BSIZE 512

struct buf {
  int valid;   // has data been read from disk?
  int disk;    // does disk "own" buf?
  uint dev;
  uint blockno;
  struct sleeplock lock;
  uint refcnt;
  struct buf *prev; // LRU cache list
  struct buf *next;
  uchar data[BSIZE];

};

#endif 