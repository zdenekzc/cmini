#define _GNU_SOURCE
#ifdef __CPARSER__
#include "cmini_libc.h"   /* glibc/ncursesw declarations for cmini (cmini/gen_libc.py) */
#else
#define _XOPEN_SOURCE_EXTENDED 1
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <grp.h>
#include <limits.h>
#include <ncurses.h>
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
/* gcc build: the same type names the cmini header defines (see cmini/cmini_libc.h) */
typedef struct dirent dirent_t;
typedef struct group group_t;
typedef struct option option_t;
typedef struct passwd passwd_t;
typedef struct stat stat_t;
typedef struct timespec timespec_t;
typedef struct timeval timeval_t;
typedef struct tm tm_t;
typedef struct utsname utsname_t;
#endif

#ifdef TOARU_CMINI
/* timersub is a macro of glibc; ToaruOS does not define it: res = a - b */
static void mc_timersub(timeval_t *a, timeval_t *b, timeval_t *res)
{
    res->tv_sec = a->tv_sec - b->tv_sec;
    res->tv_usec = a->tv_usec - b->tv_usec;
    if (res->tv_usec < 0) {
        res->tv_usec += 1000000;
        res->tv_sec--;
    }
}
#define timersub(a, b, r) mc_timersub((a), (b), (r))
#endif

/* sizes of the character buffers (the 8-bit port: one byte per cell) */
#ifndef MB_LEN_MAX
#define MB_LEN_MAX 1
#endif
#ifndef CCHARW_MAX
#define CCHARW_MAX 4
#endif
