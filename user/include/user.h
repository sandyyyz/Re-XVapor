struct stat;
struct tms;
struct utsname;
struct timespec;
// system calls
int fork(void);
int exit(int) __attribute__((noreturn));
int wait(int*);
int pipe(int*);
int write(int, const void*, int);
int read(int, void*, int);
int close(int);
int kill(int);
int exec(char*, char**);
int execve(char *path, char **argv, char **envp);
int open(const char*, int);
int mknod(const char*, short, short);
int unlink(const char*);
int fstat(int fd, struct stat*);
int link(const char*, const char*);
int mkdir(const char*);
int chdir(const char*);
int dup(int);
int getpid(void);
char* sbrk(int);
int brk(uint64);
int sleep(int);
int uptime(void);
int nanosleep(struct timespec *req, struct timespec *rem);
uint64 mmap(uint64 addr, uint64 length, uint64 prot, uint64 flags, uint64 fd, uint64 offset);
int munmap(uint64 addr, int len);

// proc related
int wait4(pid_t pid, uint64 status, int options);

// other syscall
_clock_t times(struct tms *mytime);
int uname(struct utsname *uts);
int sched_yield(void);
int gettimeofday(struct timespec *ts, void *tz);
// ulib.c
int stat(const char*, struct stat*);
char* strcpy(char*, const char*);
void *memmove(void*, const void*, uint);
char* strchr(const char*, char c);
int strcmp(const char*, const char*);
void fprintf(int, const char*, ...);
void printf(const char*, ...);
char* gets(char*, int max);
int strlen(const char*);
void* memset(void*, int, uint);
void* malloc(uint);
void free(void*);
int atoi(const char*);
int memcmp(const void *, const void *, uint);
void *memcpy(void *, const void *, uint);
