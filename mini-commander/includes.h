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
