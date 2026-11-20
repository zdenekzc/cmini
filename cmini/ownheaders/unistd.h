#ifndef CMINI_UNISTD_H_GUARD
#define CMINI_UNISTD_H_GUARD
/* unistd.h - cparser (glibc): procesy, deskriptory, adresare */

#ifndef CPARSER_UNISTD
#define CPARSER_UNISTD

#include <cstddef>

typedef int  pid_t;
typedef long ssize_t;

pid_t   fork ();
int     pipe (int * fd);
int     dup2 (int fd, int fd2);
int     close (int fd);
ssize_t read (int fd, void * buf, unsigned long n);
ssize_t write (int fd, const void * buf, unsigned long n);
int     execvp (const char * file, char * * argv);
int     execl (const char * path, const char * arg, ...);
void    _exit (int code);
int     chdir (const char * dir);
int     unlink (const char * name);
int     rmdir (const char * name);
int     fileno (struct _IO_FILE * f);
pid_t   getpid ();
int     access (const char * name, int mode);
enum { _SC_PAGESIZE = 30 };   /* jako glibc (v C makro) */
long    sysconf (int name);

#endif

/* tiny-ide: descriptors, files and processes */
#define R_OK 4
#define X_OK 1
#define SEEK_END 2
#define SEEK_SET 0
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
long lseek (int fd, long offset, int whence);
int fsync (int fd);
char *getcwd (char *buf, unsigned long size);
int dup2 (int fd, int fd2);
int execvp (const char *file, char *const argv[]);
typedef long off_t;

#endif
