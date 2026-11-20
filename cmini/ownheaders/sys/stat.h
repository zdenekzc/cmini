#ifndef CMINI_SYS_STAT_H_GUARD
#define CMINI_SYS_STAT_H_GUARD
/* sys/stat.h - cparser (glibc) */

#ifndef CPARSER_SYS_STAT
#define CPARSER_SYS_STAT

int chmod (const char * name, unsigned int mode);
int mkdir (const char * name, unsigned int mode);

#endif

/* tiny-ide: fchmod of the saved file */
int fchmod (int fd, unsigned int mode);

#endif
