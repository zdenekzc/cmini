/* dyncast.c - dynamic_cast <T *> (p), dynamic_cast <T &> (x): skutecny typ
   objektu podle retezu tabulek virtualnich metod ([VT - 8] = nadrazena trida) */

#include <stdio.h>

class Shape                              /* abstraktni */
{
   public:
      int id;
      Shape () { id = 0; }
      virtual double area () = 0;
      virtual const char * name () { return "shape"; }
      virtual ~Shape () { }
};

class Circle : public Shape
{
   public:
      double r;
      Circle () { r = 1; }
      double area () { return 3 * r * r; }
      const char * name () { return "circle"; }
};

class ColoredCircle : public Circle      /* bez vlastnich virtualnich metod */
{
   public:
      int color;
      ColoredCircle () { color = 7; }
};

class Square : public Shape
{
   public:
      double a;
      Square () { a = 2; }
      double area () { return a * a; }
};

class Plain { public: int x; };          /* nepolymorfni */

class Never : public Circle               /* objekt se nikdy nevytvori */
{
   public:
      const char * name () { return "never"; }
};

void describe (Shape * s)
{
   Circle * c = dynamic_cast <Circle *> (s);
   ColoredCircle * cc = dynamic_cast <ColoredCircle *> (s);
   Square * q = dynamic_cast <Square *> (s);
   Never * n = dynamic_cast <Never *> (s);
   printf ("%-7s circle %d colored %d square %d never %d", s->name (), c != nullptr, cc != nullptr, q != nullptr, n != nullptr);
   if (cc != nullptr) printf (" color %d r %.1f", cc->color, cc->r);
   if (q != nullptr) printf (" a %.1f", q->a);
   printf ("\n");
}

int main ()
{
   Shape * shapes [4];
   shapes [0] = new Circle;
   shapes [1] = new Square;
   shapes [2] = new ColoredCircle;
   shapes [3] = new Circle;
   for (int i = 0; i < 4; i++)
   {
      shapes [i]->id = i;
      describe (shapes [i]);
   }

   int circles = 0;
   double sum = 0;
   for (int i = 0; i < 4; i++)
   {
      Circle * c = dynamic_cast <Circle *> (shapes [i]);
      if (c)
      {
         circles ++;
         sum += c->area ();
      }
   }
   printf ("circles %d area %.1f\n", circles, sum);

   Shape * none = nullptr;
   printf ("nullptr %d\n", dynamic_cast <Circle *> (none) == nullptr);

   ColoredCircle * cc = new ColoredCircle;
   Shape * up = dynamic_cast <Shape *> (cc);          /* k bazi */
   Circle * same = dynamic_cast <Circle *> (dynamic_cast <Circle *> (cc));
   printf ("up %s same %s\n", up->name (), same->name ());

   Plain * p = dynamic_cast <Plain *> (shapes [0]);   /* nepolymorfni cil */
   printf ("plain %d\n", p == nullptr);

   Circle & cr = dynamic_cast <Circle &> (*shapes [2]);
   cr.r = 5;
   printf ("reference %.1f %s\n", dynamic_cast <ColoredCircle &> (*shapes [2]).r, cr.name ());

   for (int i = 0; i < 4; i++)
      delete shapes [i];
   return 0;
}
