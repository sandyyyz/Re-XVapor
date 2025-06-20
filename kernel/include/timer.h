#ifndef __TIMER_H
#define __TIMER_H

#include "types.h"

#define QEMU 1

#if defined QEMU
#define FREQUENCY 10000000L // qemu时钟频率12500000
#elif defined VISIONFIVE
#define FREQUENCY 4000000L
#endif

#define CLK_FREQ 10000000
#define TICKS_PER_SECOND 10  
#define INTERVAL CLK_FREQ / TICKS_PER_SECOND

#define TIMESPEC2TICKS(ts) ((ts).tv_sec * TICKS_PER_SECOND + (ts).tv_nsec * TICKS_PER_SECOND / 1000000000)
#define TICKS2TIMESPEC(ticks, ts) \
  do { \
    (ts).tv_sec = (ticks) / TICKS_PER_SECOND; \
    (ts).tv_nsec = ((ticks) % TICKS_PER_SECOND) * 1000000000 / TICKS_PER_SECOND; \
  } while(0)
  
#define TIME2SEC(time) (time / FREQUENCY)
#define TIME2MS(time) (time * 1000 / FREQUENCY)
#define TIME2US(time) (time * 1000 * 1000 / FREQUENCY)
#define TIME2NS(time) (time * 1000 * 1000 * 1000 / FREQUENCY)
#define NS2SEC(ns) (ns / (1000 * 1000 * 1000))
#define S2NS(sec) (sec * 1000 * 1000 * 1000)
#define TIME2TIMESPEC(time)                                                                                            \
    (struct timespec) { .tv_sec = TIME2SEC(time), .tv_nsec = TIME2NS(time) % (1000 * 1000 * 1000) }

#define CLOCK_REALTIME			0
#define CLOCK_MONOTONIC			1
#define CLOCK_PROCESS_CPUTIME_ID	2
#define CLOCK_THREAD_CPUTIME_ID		3
#define CLOCK_MONOTONIC_RAW		4
#define CLOCK_REALTIME_COARSE		5
#define CLOCK_MONOTONIC_COARSE		6
#define CLOCK_BOOTTIME			7
#define CLOCK_REALTIME_ALARM		8
#define CLOCK_BOOTTIME_ALARM		9
#define CLOCK_TAI                      11
struct tms {
    _clock_t  tms_utime;  // user CPU time
    _clock_t  tms_stime;  // system CPU time
    _clock_t  tms_cutime; // user CPU time of terminated child processes
    _clock_t   tms_cstime; // system CPU time of terminated child processes
};


struct timespec {
    _time_t tv_sec; // seconds
    uint64 tv_nsec;  // nanoseconds
};


struct timeval
{
    _time_t tv_sec;       // seconds
    long tv_usec; // microseconds
};

#endif