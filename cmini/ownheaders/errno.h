#ifndef CMINI_ERRNO_H_GUARD
#define CMINI_ERRNO_H_GUARD
/* errno.h - cmini declarations (values of glibc, x86_64) */
extern int * __errno_location ();
#define errno (*__errno_location ())
#define EFBIG 27
#define EINTR 4
#define EINVAL 22
#define EIO 5
#define ENAMETOOLONG 36
#define ENOENT 2
#define ENOMEM 12
#define EPERM 1
#define EACCES 13
#define EEXIST 17
#define ENOTDIR 20
#define EISDIR 21
#define EAGAIN 11
#define ECHILD 10
#define EBADF 9

#endif
