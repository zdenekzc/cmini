#ifndef CMINI_TIME_H_GUARD
#define CMINI_TIME_H_GUARD
/* ctime - cparser (glibc): cas */

#ifndef CPARSER_CTIME
#define CPARSER_CTIME

typedef long time_t;
typedef long clock_t;

#define CLOCKS_PER_SEC 1000000

time_t  time (time_t * t);
clock_t clock ();

#endif

/* clock_gettime (tiny-ide: monotonic time for run timeouts) */
#define CLOCK_MONOTONIC 1
struct timespec { long tv_sec; long tv_nsec; };
int clock_gettime (int clock, struct timespec *ts);

#endif
