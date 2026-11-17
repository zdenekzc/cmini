/* dtors.c - destruktory: virtualni destruktor, baze a vnorene objekty,
   implicitni destruktor, pole objektu, lokalni objekty (konec bloku,
   return, break, continue), globalni a staticke objekty, delete */

#include <stdio.h>

int order = 0;

class Res
{
   public:
      int id;
      Res () { id = 0; }
      ~Res () { order ++; printf ("  ~Res %d (%d)\n", id, order); }
};

class Base
{
   public:
      int n;
      Base () { n = 1; printf ("  Base ()\n"); }
      virtual ~Base ();
      virtual int value () { return n; }
};

Base::~Base ()
{
   printf ("  ~Base %d\n", n);
}

class Derived : public Base
{
   public:
      Res r;
      Derived () { n = 2; r.id = 20; printf ("  Derived ()\n"); }
      ~Derived () { printf ("  ~Derived\n"); }
      int value () { return n * 10; }
};

class Plain : public Derived     /* bez vlastniho destruktoru - implicitni */
{
   public:
      Res extra;
      Plain () { n = 3; extra.id = 30; }
};

class Holder
{
   public:
      Res items [3];
      Holder () { for (int i = 0; i < 3; i++) items [i].id = 100 + i; }
};

Res global_res;

int early (int k)
{
   Res a;
   a.id = 500 + k;
   if (k > 0)
   {
      Res b;
      b.id = 600 + k;
      return k * 2;          /* b, potom a */
   }
   return -1;                /* jen a */
}

void counter ()
{
   static Res st;
   st.id ++;
}

int main ()
{
   global_res.id = 1;

   printf ("delete Derived pres Base *:\n");
   Base * b = new Derived;
   printf ("  value %d\n", b->value ());
   delete b;

   printf ("delete Plain pres Base *:\n");
   b = new Plain;
   delete b;

   printf ("delete nullptr:\n");
   Base * z = nullptr;
   delete z;

   printf ("blok:\n");
   {
      Holder h;
      Res x;
      x.id = 7;
      printf ("  konec bloku\n");
   }

   printf ("return:\n");
   printf ("  early %d\n", early (3));
   printf ("  early %d\n", early (0));

   printf ("break a continue:\n");
   for (int i = 0; i < 5; i++)
   {
      Res loop;
      loop.id = 800 + i;
      if (i == 1) continue;
      if (i == 3) break;
      printf ("  telo %d\n", i);
   }

   counter ();
   counter ();
   printf ("konec main\n");
   return 0;
}
