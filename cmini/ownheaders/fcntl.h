#ifndef CMINI_FCNTL_H_GUARD
#define CMINI_FCNTL_H_GUARD
/* fcntl.h - cmini declarations (values of glibc, x86_64) */
#define F_GETFL 3
#define F_SETFL 4
#define O_NONBLOCK 2048
#define O_RDONLY 0
int open (const char *path, int flags, ...);
int fcntl (int fd, int cmd, ...);

#endif
