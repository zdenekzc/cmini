/* cstring - cparser (glibc) */

#ifndef CPARSER_CSTRING
#define CPARSER_CSTRING

#include <cstddef>

void * memcpy (void * dst, const void * src, unsigned long n);
void * memmove (void * dst, const void * src, unsigned long n);
void * memset (void * dst, int c, unsigned long n);
int    memcmp (const void * a, const void * b, unsigned long n);
unsigned long strlen (const char * s);
char * strdup (const char * s);
char * strndup (const char * s, unsigned long n);
int    strcmp (const char * a, const char * b);
int    strncmp (const char * a, const char * b, unsigned long n);
char * strcpy (char * dst, const char * src);
char * strncpy (char * dst, const char * src, unsigned long n);
char * strcat (char * dst, const char * src);
char * strncat (char * dst, const char * src, unsigned long n);
char * strchr (const char * s, int c);
char * strrchr (const char * s, int c);
char * strstr (const char * s, const char * t);

#endif

#ifndef CMINI_STRING_TINY_IDE_GUARD
#define CMINI_STRING_TINY_IDE_GUARD
/* tiny-ide: without these prototypes cparser assumes int results, which
 * truncates pointers (memchr, strerror) and sizes (strcspn). */
void * memchr (const void * s, int c, unsigned long n);
unsigned long strcspn (const char * s, const char * reject);
char * strerror (int errnum);
#endif
