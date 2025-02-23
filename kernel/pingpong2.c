#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int PipeFd[2];                // 管道的两个文件描述符
  pipe(PipeFd);                 // 创建一个管道
  if(fork() == 0){              // 子进程
    char Send = 'a';
    char Receive;
    read(PipeFd[0], &Receive, 1);	// 在没有接收到数据之前，子进程会被阻塞在此
    fprintf(1, "%d: received ping\n", getpid());
    write(PipeFd[1], &Send, 1);
    
  }
  else{                         // 父进程
    char Send = 'b';
    char Receive;
    write(PipeFd[1], &Send, 1);
    wait(0);					// 等待子进程结束之后再读出子进程的数据，否则可能自发自收
    read(PipeFd[0], &Receive, 1);
    fprintf(1, "%d: received pong\n", getpid());
  }
  close(PipeFd[0]);				// 关闭文件描述符
  close(PipeFd[1]);
  exi t(0);
}

