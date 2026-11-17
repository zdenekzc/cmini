/* intmodel.c - velikosti a rozlozeni jako v C (x86-64): char a bool 1 bajt, short 2,
   int, unsigned a float 4, long, ukazatel a double 8; zarovnani polozek, sizeof,
   preteceni int a short, unsigned, posuny, smisene porovnani int / unsigned,
   float (presnost), pole int pro libc (qsort, memset, sscanf), struktury a pole
   struktur, ukazatelova aritmetika; vzor z g++ (README.txt, kapitola 55) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Mix
{
   char c;
   int i;
   short s;
   long l;
   bool b;
   float f;
   double d;
   char name [5];
   unsigned u;
};

struct Small
{
   short a;
   char b;
};

struct Pair
{
   int x;
   int y;
};

int cmp_int (const void * a, const void * b)
{
   int x = * (const int *) a;
   int y = * (const int *) b;
   return x < y ? -1 : x > y ? 1 : 0;
}

int add_int (int a, int b) { return a + b; }
short to_short (int v) { return v; }
unsigned to_unsigned (int v) { return v; }
float half_f (float v) { return v / 2; }

int main ()
{
   /* velikosti */
   printf ("sizeof %d %d %d %d %d %d %d %d %d\n", (int) sizeof (char), (int) sizeof (bool), (int) sizeof (short),
           (int) sizeof (int), (int) sizeof (unsigned), (int) sizeof (long), (int) sizeof (float),
           (int) sizeof (double), (int) sizeof (int *));
   printf ("struct %d %d %d\n", (int) sizeof (Mix), (int) sizeof (Small), (int) sizeof (Pair));
   Mix arr [3];
   printf ("array %d %d\n", (int) sizeof (arr), (int) ((char *) &arr [2] - (char *) &arr [0]));
   Mix m;
   printf ("offset %d %d %d %d %d %d %d %d\n", (int) ((char *) &m.i - (char *) &m), (int) ((char *) &m.s - (char *) &m),
           (int) ((char *) &m.l - (char *) &m), (int) ((char *) &m.b - (char *) &m), (int) ((char *) &m.f - (char *) &m),
           (int) ((char *) &m.d - (char *) &m), (int) ((char *) &m.name [0] - (char *) &m), (int) ((char *) &m.u - (char *) &m));

   /* preteceni a zkraceni */
   int big = 2147483647;
   big = big + 1;
   int prod = 100000 * 100000;
   short sh = 70000;
   short sh2 = to_short (40000);
   char ch = 200;
   unsigned char uc = 300;
   printf ("overflow %d %d %d %d %d %d\n", big, prod, sh, sh2, ch, uc);
   int k = 5;
   k += 2147483647;
   k ++;
   printf ("compound %d %d\n", k, add_int (2147483647, 2));

   /* unsigned */
   unsigned u = 0;
   u = u - 1;
   unsigned u2 = to_unsigned (-2);
   int neg = -1;
   printf ("unsigned %u %u %d %u\n", u, u2, neg < 1u ? 1 : 0, u / 3);
   long lu = u;
   printf ("widen %ld %ld\n", lu, (long) neg);

   /* posuny */
   int one = 1;
   int sl = one << 31;
   int sr = sl >> 4;
   unsigned usr = (unsigned) sl >> 4;
   printf ("shift %d %d %u %d\n", sl, sr, usr, one << 33);

   /* float */
   float f = 0.1f;
   float g = f * 3;
   double dd = f;
   float h = half_f (5);
   printf ("float %.10f %.10f %.10f %.3f %d\n", (double) f, (double) g, dd, (double) h, f == 0.1f ? 1 : 0);

   /* polozky struktury */
   m.c = 'x';
   m.i = -5;
   m.s = -3;
   m.l = 1234567890123L;
   m.b = true;
   m.f = 1.5f;
   m.d = 2.25;
   strcpy (m.name, "abcd");
   m.u = 4000000000u;
   Mix m2 = m;
   m2.i = m2.i * 2;
   printf ("fields %c %d %d %ld %d %.2f %.2f %s %u %d\n", m.c, m.i, m.s, m.l, m.b ? 1 : 0, (double) m.f, m.d, m.name, m.u, m2.i);

   /* pole struktur, ukazatele */
   Pair ps [4];
   for (int i = 0; i < 4; i ++) { ps [i].x = i; ps [i].y = i * i; }
   Pair * p = ps;
   p += 2;
   printf ("pairs %d %d %d %d\n", p->x, p->y, (p + 1)->y, (int) (p - ps));
   short ss [4];
   for (int i = 0; i < 4; i ++) ss [i] = (short) (i * 30000);
   printf ("shorts %d %d %d %d\n", ss [0], ss [1], ss [2], ss [3]);

   /* pole int pro libc */
   int nums [6];
   nums [0] = 42; nums [1] = -7; nums [2] = 13; nums [3] = 0; nums [4] = 99; nums [5] = -100;
   qsort (nums, 6, sizeof (int), cmp_int);
   printf ("qsort %d %d %d %d %d %d\n", nums [0], nums [1], nums [2], nums [3], nums [4], nums [5]);
   memset (nums, 0, sizeof (nums));
   int a1 = 0;
   short a2 = 0;
   sscanf ("12 -34", "%d %hd", &a1, &a2);
   printf ("libc %d %d %d\n", nums [3], a1, a2);
   return 0;
}
