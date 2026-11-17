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
long   strtol (const char * s, char * * end, int base);
double strtod (const char * s, char * * end);
double atof (const char * s);
unsigned long strtoul (const char * s, char * * end, int base);

#endif
