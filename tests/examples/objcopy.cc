/* objcopy.c - kopie objektu hodnotou: prirazeni, inicializace kopii,
   parametr hodnotou, vysledek funkce a metody, destruktory kopii,
   prirazeni odvozeneho objektu do baze (vptr zustava) */

#include <stdio.h>

struct Vec
{
   int x;
   int y;
   int tab [3];
};

Vec make (int a, int b)
{
   Vec v;
   v.x = a;
   v.y = b;
   for (int i = 0; i < 3; i++) v.tab [i] = a * 10 + i;
   return v;
}

Vec add (Vec p, Vec q)
{
   Vec r;
   r.x = p.x + q.x;
   r.y = p.y + q.y;
   for (int i = 0; i < 3; i++) r.tab [i] = p.tab [i] + q.tab [i];
   return r;
}

void change (Vec v)
{
   v.x = 999;                 /* kopie - volajici se nezmeni */
   v.tab [0] = 999;
}

void print (const char * name, Vec v)
{
   printf ("%s: %d %d [%d %d %d]\n", name, v.x, v.y, v.tab [0], v.tab [1], v.tab [2]);
}

int alive = 0;

class Res
{
   public:
      int id;
      Res () { id = 0; alive ++; }
      ~Res () { alive --; }
};

int take (Res r)               /* kopie se zrusi na konci funkce */
{
   return r.id * 2;
}

class Base
{
   public:
      int n;
      Base () { n = 1; }
      virtual int kind () { return 1; }
};

class Derived : public Base
{
   public:
      int extra;
      Derived () { n = 2; extra = 5; }
      int kind () { return 2; }
};

class Pair
{
   public:
      Vec a;
      int k;
      Pair swap_xy () { Pair p; p.k = k; p.a = a; p.a.x = a.y; p.a.y = a.x; return p; }
};

int main ()
{
   Vec a = make (1, 2);
   print ("make", a);

   Vec b;
   b = a;                     /* prirazeni */
   b.x = 7;
   print ("a", a);
   print ("b", b);

   Vec c = b;                 /* inicializace kopii */
   c.tab [2] = 0;
   print ("c", c);
   print ("b", b);

   change (a);
   print ("po change", a);

   Vec s = add (make (1, 1), make (2, 3));   /* dva docasne vysledky */
   print ("add", s);
   print ("vyraz", add (a, b));

   Res r;
   r.id = 21;
   printf ("take %d\n", take (r));
   printf ("alive %d\n", alive);
   {
      Res copy = r;
      printf ("copy %d, alive %d\n", copy.id, alive);
   }
   printf ("alive %d\n", alive);

   Derived d;
   Base x;
   x = d;                     /* rez: kopie polozek baze, vptr zustava */
   printf ("base: n %d kind %d\n", x.n, x.kind ());

   Pair p;
   p.k = 3;
   p.a = make (4, 5);
   Pair q = p.swap_xy ();
   printf ("pair: %d %d %d\n", q.a.x, q.a.y, q.k);
   printf ("O.K.\n");
   return 0;
}
