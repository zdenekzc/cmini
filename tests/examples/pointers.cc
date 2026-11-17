/* pointers.c - ukazatele (na ukazatele, void *, double *, pole ukazatelu,
   aritmetika, seznam) a ukazatele na funkce (promenne, pole, parametry,
   typedef, navratova hodnota, polozky struktur, globalni inicializace,
   zpetne volani z glibc - qsort), lokalni pole s inicializaci */

#include <stdio.h>
#include <stdlib.h>
#include <string>
using namespace std;

int puts (const char * s);
struct Node { int v; Node * next; };
void swap (int * a, int * b) { int t = *a; *a = *b; *b = t; }
void set (int ** pp, int * q) { *pp = q; }
int sum (int * a, int n) { int s = 0; for (int * p = a; p < a + n; p++) s += *p; return s; }

int add (int a, int b) { return a + b; }
int mul (int a, int b) { return a * b; }
double half (double x) { return x / 2; }

typedef int (*BinOp) (int, int);

int apply (BinOp f, int a, int b) { return f (a, b); }
int apply2 (int (*f) (int, int), int x) { return f (x, x); }

BinOp choose (int k) { if (k == 0) return add; return &mul; }

struct Command { const char * name; BinOp op; };

int cmp_int (const void * a, const void * b) { return * (const int *) a - * (const int *) b; }

void (*logger) (const char * msg) = nullptr;
void print_msg (const char * msg) { printf ("log: %s\n", msg); }


/* globalni ukazatele na funkce s inicializaci */
BinOp global_op = mul;
int (*global_table [3]) (const char *) = { puts, nullptr, puts };

double integrate (double (*g) (double), double a, double b, int n)
{
   double h = (b - a) / n, s = 0;
   for (int i = 0; i < n; i++)
      s += g (a + (i + 0.5) * h) * h;
   return s;
}

double square (double x) { return x * x; }

int fold (int * a, int n, BinOp op = add)
{
   int r = a [0];
   for (int i = 1; i < n; i++) r = op (r, a [i]);
   return r;
}

void pointers ()
{
   int x = 1, y = 2;
   swap (&x, &y);
   printf ("swap %d %d\n", x, y);
   int * p = &x;
   int ** pp = &p;
   **pp = 10;
   printf ("pp %d %d\n", x, *p);
   set (pp, &y);
   *p = 20;
   printf ("set %d %d\n", x, y);
   int a [5] = { 1, 2, 3, 4, 5 };
   printf ("sum %d\n", sum (a, 5));
   int * q = &a [2];
   q [1] = 40;
   *(q - 1) = 20;
   printf ("arr %d %d %d %d\n", a [1], a [3], (int) (q - a), q > a);
   Node n1, n2;
   n1.v = 1; n1.next = &n2; n2.v = 2; n2.next = nullptr;
   Node * h = &n1;
   int t = 0;
   for (Node * k = h; k != nullptr; k = k->next) t += k->v;
   Node ** link = &n1.next;
   (*link)->v = 5;
   printf ("list %d %d\n", t, n2.v);
   void * vp = &x;
   int * ip = (int *) vp;
   printf ("void %d\n", *ip);
   double d = 1.5;
   double * dp = &d;
   *dp += 1;
   printf ("double %f\n", d);
   const char * names [3] = { "a", "bb", "ccc" };
   const char ** np = names;
   printf ("names %s %s\n", np [1], *(np + 2));
   int * arr [2] = { &x, &y };
   *arr [1] = 7;
   printf ("arrptr %d\n", y);
}

void function_pointers ()
{
   int (*f) (int, int) = add;
   printf ("f %d\n", f (2, 3));
   f = mul;
   printf ("f %d %d\n", f (2, 3), (*f) (4, 5));
   BinOp ops [2] = { add, mul };
   for (int i = 0; i < 2; i++) printf ("ops %d\n", ops [i] (6, 7));
   printf ("apply %d %d\n", apply (add, 1, 2), apply2 (mul, 9));
   printf ("choose %d %d\n", choose (0) (3, 4), choose (1) (3, 4));
   double (*h) (double) = half;
   printf ("half %f\n", h (5));
   Command cmds [2] = { { "plus", add }, { "krat", mul } };
   for (int i = 0; i < 2; i++) printf ("%s %d\n", cmds [i].name, cmds [i].op (10, 3));
   Command * c = &cmds [1];
   printf ("arrow %d\n", c->op (2, 2));
   if (logger == nullptr) printf ("no logger\n");
   logger = print_msg;
   if (logger) logger ("hello");
   int v [6] = { 5, 3, 9, 1, 7, 2 };
   qsort (v, 6, sizeof (int), cmp_int);
   for (int i = 0; i < 6; i++) printf ("%d ", v [i]);
   printf ("\n");
   string s = "abc";
   string * sp = &s;
   *sp += "d";
   printf ("strptr %s %d\n", sp->c_str (), (int) sp->length ());
   BinOp g = nullptr;
   printf ("null %d %d\n", g == nullptr, f != g);
}

int main ()
{
   pointers ();
   function_pointers ();
   printf ("global %d\n", global_op (6, 7));
   global_table [0] ("puts pres ukazatel");
   printf ("table %d\n", global_table [1] == nullptr);
   printf ("integrate %.6f\n", integrate (square, 0, 3, 1000));
   int w [4] = { 1, 2, 3, 4 };
   printf ("fold %d %d\n", fold (w, 4), fold (w, 4, mul));
   return 0;
}
