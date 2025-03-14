#ifndef __TIMER_H
#define __TIMER_H

#include "types.h"

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