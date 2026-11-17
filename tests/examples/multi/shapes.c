/* shapes.c - definice trid a funkci (tests/examples/multi) */

#include <stdio.h>
#include "shapes.h"

int shape_count = 0;
Sequence <int> history;
Registry registry;

static int helper () { return 1; }          /* static - i v main.c stejne jmeno */

Registry::Registry ()
{
   created = 100;
   printf ("Registry () %d\n", helper ());
}

Registry::~Registry ()
{
   printf ("~Registry %d shapes, history %d\n", shape_count, history.size ());
}

Shape::Shape ()
{
   name = "?";
   shape_count ++;
}

Shape::~Shape ()
{
   shape_count --;
}

Circle::Circle (double radius)
{
   r = radius;
   name = "kruh";
   history.add (1);
}

Circle::~Circle ()
{
   history.add (-1);
}

double Circle::area ()
{
   return 3 * r * r;
}

string Circle::kind ()
{
   return "circle";
}

Square::Square (double side)
{
   a = side;
   name = "ctverec";
   history.add (2);
}

double Square::area ()
{
   return a * a;
}

Shape * make_shape (int k)
{
   if (k % 2 == 0) return new Circle (k + 1);
   return new Square (k + 1);
}

double total_area (Shape * * a, int n)
{
   double s = 0;
   for (int i = 0; i < n; i++) s += a [i]->area ();
   return s;
}

int counter_value ()
{
   return shape_count * 10 + registry.created;
}

int cp_callback (int x)
{
   return x + 1000;
}
