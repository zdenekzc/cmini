/* floats.c - plovouci carka (SSE2): float, double, prevody, porovnani,
   volani podle System V (xmm0 .. xmm7, zasobnik), printf, knihovna libm */

#include <stdio.h>
#include <math.h>

double sqrt (double x);
double pow (double x, double y);
double floor (double x);

double gd = 2.5;
float gf = 1.25f;
double garr [4] = { 1.0, -2.5, 3.25, 1e3 };
int gi = 7;

double add3 (double a, double b, double c) { return a + b + c; }
float halve (float x) { return x / 2; }
int trunc_div (double a, double b) { return (int) (a / b); }

/* 10 hodnot double + 7 celych cisel: cast na zasobniku */
double many (double a, int i, double b, int j, double c, int k, double d, int l,
             double e, int m, double f, int n, double g, int o, double h, double p, double q)
{
   return a + b * 2 + c * 3 + d * 4 + e * 5 + f * 6 + g * 7 + h * 8 + p * 9 + q * 10
          + i + j * 10 + k * 100 + l * 1000 + m * 10000 + n * 100000 + o * 1000000;
}

struct Point
{
   double x;
   double y;
   float w;
};

class Shape
{
   public:
      double scale;
      Shape (double s) { scale = s; }
      virtual double area () { return 0; }
};

class Circle : public Shape
{
   public:
      double r;
      Circle (double radius, double s) : Shape (s) { r = radius; }
      double area () { return 3.14159 * r * r * scale; }
};

double avg (double * a, int n)
{
   double s = 0;
   for (int i = 0; i < n; i++)
      s += a [i];
   return s / n;
}

double counter ()
{
   static double total = 0.5;
   total = total * 2;
   return total;
}

int main ()
{
   double a = 3.5, b = 1.25;
   float f = 2.5f, g = 0.1f;
   int i = 7;

   printf ("arit: %f %f %f %f\n", a + b, a - b, a * b, a / b);
   printf ("float: %f %f %.10f\n", f * g, f / 3, g + g + g);
   printf ("mix: %f %f %f %d\n", a + i, i / 2.0, f + a, (int) (a * i));
   printf ("prevody: %d %d %d %f %f\n", (int) -2.75, (int) 2.75, (int) (f * 3), (double) i / 4, (float) a);
   char c = 66.9;
   long big = 1e12;
   printf ("char %c long %ld\n", c, big);

   printf ("porovnani: %d %d %d %d %d %d\n", a < b, a > b, a <= 3.5, a >= 4, a == 3.5, a != b);
   double zero = 0;
   double nan = zero / zero;
   printf ("nan: %d %d %d %d\n", nan == nan, nan != nan, nan < 1, nan > 1);
   printf ("bool: %d %d %d %d\n", !zero, !a, zero || a, (a && zero) ? 1 : 2);
   if (a) printf ("if double ok\n");
   double neg = -0.0;
   if (neg) printf ("chyba\n"); else printf ("-0.0 je nepravda\n");

   printf ("format: %g %e %.3f %10.2f\n", 1e-5, 12345.678, 2.0 / 3, -a);

   double d = 1;
   d += 0.5;
   d *= 4;
   d -= 1;
   d /= 2;
   printf ("prirazeni: %f\n", d);
   d ++;
   ++ d;
   double old = d --;
   printf ("inkrement: %f %f\n", d, old);
   int k = 10;
   k += 2.7;
   k *= 1.5;
   printf ("int op= double: %d\n", k);
   double m = -a;
   printf ("minus: %f %f %f\n", m, -m, -f);

   printf ("funkce: %f %f %d\n", add3 (1, 2.5, b), halve (5), trunc_div (7, 2));
   printf ("many: %f\n", many (1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 9, 10));
   printf ("libm: %f %f %f\n", sqrt (2), pow (2, 10), floor (-1.5));

   Point p;
   p.x = 1.5;
   p.y = p.x * 2;
   p.w = 0.25f;
   Point * pp = &p;
   printf ("struct: %f %f %f\n", pp->x, pp->y, pp->w + 1);

   Shape * s = new Circle (2.0, 0.5);
   printf ("virtual: %.4f\n", s->area ());

   printf ("pole: %f %f\n", avg (garr, 4), garr [1]);
   printf ("globalni: %f %f %d\n", gd, gf, gi);
   counter ();
   printf ("static: %f\n", counter ());

   double h = i > 3 ? 1.5 : 2;
   printf ("ternar: %f %f\n", h, i < 3 ? 1.5 : i);
   return 0;
}
