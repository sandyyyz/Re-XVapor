#ifndef __TIMER_H
#define __TIMER_H

#include "types.h"

#define QEMU 1

#if defined QEMU
#define FREQUENCY 10000000L // qemu时钟频率12500000
#elif defined VISIONFIVE
#define FREQUENCY 4000000L
#endif

#define TIME2SEC(time) (time / FREQUENCY)
#define TIME2MS(time) (time * 1000 / FREQUENCY)
#define TIME2US(time) (time * 1000 * 1000 / FREQUENCY)
#define TIME2NS(time) (time * 1000 * 1000 * 1000 / FREQUENCY)
#define TIME2TIMESPEC(time)                                                                                            \
    (struct timespec) { .tv_sec = TIME2SEC(time), .tv_nsec = TIME2NS(time) % (1000 * 1000 * 1000) }

struct tms {
    _clock_t  tms_utime;  // user CPU time
    _clock_t  tms_stime;  // system CPU time
    _clock_t  tms_cutime; // user CPU time of terminated child processes
    _clock_t   tms_cstime; // system CPU time of terminated child processes
};


struct timespec {
    _time_t tv_sec; // seconds
    long tv_nsec;  // nanoseconds
};


struct timeval
{
    _time_t tv_sec;       // seconds
    long tv_usec; // microseconds
};

#endif