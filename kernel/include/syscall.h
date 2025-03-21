// System call numbers
#define SYS_fork    1
#define SYS_exit    93  /// changed from 2 to 93
#define SYS_wait    3
#define SYS_pipe    4
#define SYS_read    5
#define SYS_kill    6
#define SYS_exec    7
#define SYS_fstat   8
#define SYS_chdir   9
#define SYS_dup    10

#define SYS_sbrk   12
#define SYS_sleep  13
#define SYS_uptime 14
#define SYS_open   15
#define SYS_write  64   /// changed from 15 to 64
#define SYS_mknod  17
#define SYS_unlink 18
#define SYS_link   19
#define SYS_mkdir  20
#define SYS_close  21

// other syscalls
#define SYS_times 153
#define SYS_gettimeofday 169
#define SYS_uname 160
#define SYS_sched_yield 124
#define SYS_nanosleep 101

// process related
#define SYS_execve 221
#define SYS_clone 220
#define SYS_getpid 172
#define SYS_getppid 173
#define SYS_wait4 260