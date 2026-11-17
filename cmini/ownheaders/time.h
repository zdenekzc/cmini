/* ctime - cparser (glibc): cas */

#ifndef CPARSER_CTIME
#define CPARSER_CTIME

typedef long time_t;
typedef long clock_t;

#define CLOCKS_PER_SEC 1000000

time_t  time (time_t * t);
clock_t clock ();

#endif
