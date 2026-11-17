/* main.c - hlavni soubor programu z vice souboru (tests/examples/multi) */

#include <stdio.h>
#include "shapes.h"

static int helper () { return 2; }          /* static - stejne jmeno jako v shapes.c */

int main ()
{
   Shape * a [4];
   for (int i = 0; i < 4; i++)
      a [i] = make_shape (i);

   for (int i = 0; i < 4; i++)
   {
      Circle * c = dynamic_cast <Circle *> (a [i]);   /* objekt z jineho souboru */
      printf ("%s area %.1f circle %d\n", a [i]->describe ().c_str (), a [i]->area (), c != nullptr);
   }
   printf ("total %.1f count %d value %d helper %d\n", total_area (a, 4), shape_count, counter_value (), helper ());

   double (*f) (Shape * *, int) = total_area;       /* ukazatel na funkci z jineho souboru */
   printf ("pointer %.1f\n", f (a, 2));

   history.add (7);
   shape_count += 0;
   printf ("C: %d %d\n", c_twice (21), c_apply (5));

   delete a [0];
   delete a [1];
   printf ("after delete %d, history", shape_count);
   for (int h : history) printf (" %d", h);
   printf ("\n");
   return 0;
}
