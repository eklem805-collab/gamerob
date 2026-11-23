#ifndef CD_FREESTANDING_TIME_H
#define CD_FREESTANDING_TIME_H
#include <sys/types.h>
struct timespec { time_t tv_sec; long tv_nsec; };
#define CLOCK_MONOTONIC 1
#define CLOCK_REALTIME 0
int clock_gettime(int clk, struct timespec* ts);
int nanosleep(const struct timespec* req, struct timespec* rem);
#endif
