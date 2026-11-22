/* initsize.cc - local initializers { ... } write every member with its own size:
   int, short, char, bool and float members of a struct and the elements of an int
   array must not overwrite their neighbours (cmini used to store each of them as
   8 bytes: TEXT t = { NULL, 0, 0 } cleared the lower half of the parameter f next to
   it - asm/asmlist.c AsmPrintNode, crash of cmini built for ToaruOS); expected
   output from g++ */

#include <stdio.h>

typedef struct { char * text; int len; int cap; } TEXT;

struct Small { char c; short s; int i; bool b; float f; };

struct Outer { int a; Small inner; int z; };

long keep (const char * s, long f)
{
   TEXT t = { NULL, 0, 0 };
   if (s != NULL) t.len = 1;
   return f;
}

int main ()
{
   long r = keep (NULL, 0x1122334455667788);
   printf ("param after TEXT init: %lx\n", r);

   int before = 111;
   int a [3] = { 1, 2, 3 };
   int after = 222;
   printf ("int array: %d %d %d, neighbours %d %d\n", a [0], a [1], a [2], before, after);

   Small sm = { 'x', 7, 9, true, 2.5f };
   printf ("small: %c %d %d %d %.2f\n", sm.c, sm.s, sm.i, sm.b ? 1 : 0, (double) sm.f);

   Outer o = { 10, { 'y', -3, 40, false, 0.25f }, 50 };
   printf ("outer: %d %c %d %d %d %.2f %d\n", o.a, o.inner.c, o.inner.s, o.inner.i, o.inner.b ? 1 : 0, (double) o.inner.f, o.z);

   short sh [4] = { 1, -2, 3, -4 };
   char ch [4] = { 'a', 'b', 'c', 'd' };
   printf ("short array: %d %d %d %d, char array: %c%c%c%c\n", sh [0], sh [1], sh [2], sh [3], ch [0], ch [1], ch [2], ch [3]);
   return 0;
}
