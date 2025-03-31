#ifndef __PRINTF_H
#define __PRINTF_H

void printf(const char *fmt, ...);
void panic(char *s) __attribute__((noreturn));
void printfinit(void);

#endif