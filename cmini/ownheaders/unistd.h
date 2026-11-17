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
