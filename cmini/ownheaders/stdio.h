/* cstdio - cparser (glibc) */

#ifndef CPARSER_CSTDIO
#define CPARSER_CSTDIO

#include <cstddef>

typedef struct _IO_FILE FILE;       /* jen ukazatele (stejne jako glibc) */

FILE * fopen (const char * name, const char * mode);
FILE * fdopen (int fd, const char * mode);
int    fclose (FILE * f);
unsigned long fread (void * adr, unsigned long size, unsigned long count, FILE * f);
unsigned long fwrite (const void * adr, unsigned long size, unsigned long count, FILE * f);
int    fputs (const char * s, FILE * f);
int    fputc (int c, FILE * f);
int    fgetc (FILE * f);
int    getc (FILE * f);
int    ungetc (int c, FILE * f);
int    fflush (FILE * f);
int    setvbuf (FILE * f, char * buf, int mode, unsigned long size);
int    puts (const char * s);
int    putchar (int c);
int    printf (const char * format, ...);
int    snprintf (char * buf, unsigned long size, const char * format, ...);
int    remove (const char * name);
int    fseek (FILE * f, long offset, int whence);
long   ftell (FILE * f);
int    ferror (FILE * f);

#define EOF      (-1)
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#define _IONBF   2

/* stderr je v glibc promenna - cparser zatim nezna promenne z knihovny:
   vlastni proud nad deskriptorem 2 bez vyrovnavaci pameti */
static FILE * cparser_stderr_file = nullptr;

inline FILE * cparser_stderr ()
{
   if (cparser_stderr_file == nullptr)
   {
      cparser_stderr_file = fdopen (2, "w");
      setvbuf (cparser_stderr_file, nullptr, _IONBF, 0);
   }
   return cparser_stderr_file;
}

#define stderr (cparser_stderr ())

/* stdout - totez nad deskriptorem 1 (printf ma vlastni vyrovnavaci pamet,
   v programech prelozenych instr*.cc nemichat s printf);  translate.c
   prevede cparser_stdout () i cparser_stderr () zpet na stdout, stderr */
static FILE * cparser_stdout_file = nullptr;

inline FILE * cparser_stdout ()
{
   if (cparser_stdout_file == nullptr)
      cparser_stdout_file = fdopen (1, "w");
   return cparser_stdout_file;
}

#define stdout (cparser_stdout ())

#endif
