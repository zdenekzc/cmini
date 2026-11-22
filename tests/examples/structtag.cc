/* structtag.cc - "struct X" in a function or in another structure refers to the
   structure X declared in an enclosing scope (C style declarations struct X v,
   struct X * p); cmini used to create a new empty local type X there and then
   reported "Member of structure expected"; expected output from g++ */

#include <stdio.h>

struct Small { char c; short s; int i; };

struct Node { int value; struct Node * next; };

struct Pair { struct Small first; struct Small second; };

int sum (struct Small * p)
{
   return p->c + p->s + p->i;
}

int main ()
{
   struct Small a = { 'A', 2, 3 };
   struct Small b;
   b.c = 'B';
   b.s = 20;
   b.i = 30;
   struct Small * p = &a;
   printf ("a: %c %d %d, b: %c %d %d, p->i %d\n", a.c, a.s, a.i, b.c, b.s, b.i, p->i);
   printf ("sum (&a) %d, sum (&b) %d\n", sum (&a), sum (&b));

   struct Node n3 = { 3, NULL };
   struct Node n2 = { 2, &n3 };
   struct Node n1 = { 1, &n2 };
   int total = 0;
   for (struct Node * q = &n1; q != NULL; q = q->next)
      total = total * 10 + q->value;
   printf ("list: %d\n", total);

   struct Pair pr = { { 'x', 1, 2 }, { 'y', 3, 4 } };
   printf ("pair: %c %d %d %c %d %d\n", pr.first.c, pr.first.s, pr.first.i, pr.second.c, pr.second.s, pr.second.i);

   struct Local { int k; };
   struct Local loc = { 42 };
   printf ("local: %d\n", loc.k);
   return 0;
}
