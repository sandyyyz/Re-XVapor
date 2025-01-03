#ifndef __DEBUG_H__
#define __DEBUG_H__

// void backtrace();
#define ANSI_FG_BLACK "\33[1;30m"
#define ANSI_FG_RED "\33[1;31m"
#define ANSI_FG_GREEN "\33[1;32m"
#define ANSI_FG_YELLOW "\33[1;33m"
#define ANSI_FG_BLUE "\33[1;34m"
#define ANSI_FG_MAGENTA "\33[1;35m"
#define ANSI_FG_CYAN "\33[1;36m"
#define ANSI_FG_WHITE "\33[1;37m"
#define ANSI_BG_BLACK "\33[1;40m"
#define ANSI_BG_RED "\33[1;41m"
#define ANSI_BG_GREEN "\33[1;42m"
#define ANSI_BG_YELLOW "\33[1;43m"
#define ANSI_BG_BLUE "\33[1;44m"
#define ANSI_BG_MAGENTA "\33[1;35m"
#define ANSI_BG_CYAN "\33[1;46m"
#define ANSI_BG_WHITE "\33[1;47m"
#define ANSI_NONE "\33[0m"

#define ANSI_FMT(str, fmt) fmt str ANSI_NONE

/* ANSI-color code printf */
#define printfRed(format, ...)          \
    printf("\33[1;31m" format "\33[0m", \
           ##__VA_ARGS__)

#define printfGreen(format, ...)        \
    printf("\33[1;32m" format "\33[0m", \
           ##__VA_ARGS__)

#define printfBlue(format, ...)         \
    printf("\33[1;34m" format "\33[0m", \
           ##__VA_ARGS__)

#define printfCYAN(format, ...)         \
    printf("\33[1;36m" format "\33[0m", \
           ##__VA_ARGS__)

#define printfYELLOW(format, ...)       \
    printf("\33[1;33m" format "\33[0m", \
           ##__VA_ARGS__)

#define printfBWhite(format, ...)       \
    printf("\33[1;37m" format "\33[0m", \
           ##__VA_ARGS__)

#define printfMAGENTA(format, ...)      \
    printf("\33[1;35m" format "\33[0m", \
           ##__VA_ARGS__)

/* assert/log/warn/info */
#define ASSERT(cond)                                                                     \
    do {                                                                                 \
        if (!(cond)) {                                                                   \
            printf("\33[1;31m[ASSERT][%s,%d,%s] ASSERT: \"" #cond "\" failed \t \33[0m", \
                   __FILE__, __LINE__, __func__);                                        \
            panic("assert failed");                                                      \
        }                                                                                \
    } while (0)

#define Log(format, ...)                                  \
    printf("\33[1;34m[LOG][%s,%d,%s] " format "\33[0m\n", \
           __FILE__, __LINE__, __func__, ##__VA_ARGS__)

#define Warn(format, ...)                                  \
    printf("\33[1;31m[WARN][%s,%d,%s] " format "\33[0m\n", \
           __FILE__, __LINE__, __func__, ##__VA_ARGS__)

#define Info(fmt, ...) printf("[INFO] " fmt "", ##__VA_ARGS__);

/* misc */
#define DEBUG_ACQUIRE(format, ...) \
    printf(ANSI_FMT(format, ANSI_FG_RED), ##__VA_ARGS__)
#define DEBUG_RELEASE(format, ...) \
    printf(ANSI_FMT(format, ANSI_FG_BLUE), ##__VA_ARGS__)

#define PTE(format, ...) \
    printf(ANSI_FMT(format, ANSI_FG_GREEN), ##__VA_ARGS__)

#define VMA(format, ...) \
    printf(ANSI_FMT(format, ANSI_FG_GREEN), ##__VA_ARGS__)

#define STRACE(format, ...) \
    printf(ANSI_FMT(format, ANSI_FG_YELLOW), ##__VA_ARGS__)

#define RED(str) "\e[31;1m" str "\e[0m"
#define Info_R(fmt, ...) printf("[INFO] " RED(fmt) "", ##__VA_ARGS__);
#define TODO() 0
#define DEBUG_SECTOR(sec_num) ((sec_num) * (BSIZE))

#endif // __DEBUG_H__
