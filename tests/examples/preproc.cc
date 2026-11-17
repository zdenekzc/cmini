/* preproc.c - test preprocesoru (lexer.c) */

#include <stdio.h>
#include "include/preproc.h"
#include "include/preproc.h"   /* podruhe - ochrana pomoci #ifndef */

#define VERSION 3
#define DEBUG

#if VERSION >= 3 && defined (DEBUG)
   #define MODE 1
#elif VERSION == 2
   #define MODE 2
#else
   #define MODE 0
#endif

#ifndef SIZE
   #error SIZE is not defined
#endif

#define CAT(a, b) a ## b
#define VAR(n) CAT (value_, n)
#define CALL(f, ...) f (__VA_ARGS__)
#define LONG_SUM(a, b, c) \
   ((a) +                 \
    (b) +                 \
    (c))

int VAR (1) = 100;
int VAR (2) = 200;

int a [SIZE];

int sum3 (int x, int y, int z)
{
    return x + y + z;
}

int main ()
{
    for (int i = 0; i < SIZE; i++)
        a [i] = SQUARE (i - 4);

    int m = a [0];
    int n = a [0];
    for (int i = 1; i < SIZE; i++)
    {
        m = MAX (m, a [i]);
        n = MIN (n, a [i]);
    }

    SHOW (m);
    SHOW (n);
    SHOW (MAX (SQUARE (3), MAX (2, 7)));
    SHOW (VAR (1) + VAR (2));
    SHOW (CALL (sum3, 1, 2, 3));
    SHOW (LONG_SUM (10, 20, 30));
    SHOW (MODE);
    SHOW (__LINE__);

#undef MODE
#define MODE 7
    SHOW (MODE);

#if 0
    this text is skipped, it's not C code "
#endif

    printf ("O.K.\n");
}
