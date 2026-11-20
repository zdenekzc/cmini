#ifndef CMINI_STDLIB_H_GUARD
#define CMINI_STDLIB_H_GUARD
/* cstdlib - cparser (glibc) */

#ifndef CPARSER_CSTDLIB
#define CPARSER_CSTDLIB

#include <cstddef>

void * malloc (unsigned long size);
void * calloc (unsigned long count, unsigned long size);
void * realloc (void * adr, unsigned long size);
void   free (void * adr);
void   exit (int code);
void   abort ();
int    abs (int n);
long   labs (long n);
int    rand ();
void   srand (unsigned int seed);
char * getenv (const char * name);
int    setenv (const char * name, const char * value, int overwrite);
char * mkdtemp (char * templ);
long   strtol (const char * s, char * * end, int base);
double strtod (const char * s, char * * end);
double atof (const char * s);
unsigned long strtoul (const char * s, char * * end, int base);

#endif

/* tiny-ide: paths and temporary files */
char *realpath (const char *path, char *resolved);
int mkstemp (char *template);

#endif
