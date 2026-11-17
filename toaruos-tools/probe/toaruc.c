/* toaruc.c - C program for ToaruOS compiled by c-mini (its own runtime library,
   ToaruOS syscalls):   cmini.bin -T -I include -o toaruc.bin toaruc.c
   Checks printf (write 4), the heap via sbrk (10), also above 64 KB, time
   (gettimeofday 6) and exit (0). */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

struct Item
{
   int value;
   struct Item * next;
};
typedef struct Item Item;

int main ()
{
   printf ("TOARUC: C program from c-mini, ToaruOS syscalls\n");

   /* linked list - the heap grows in 64 KB steps several times */
   Item * first = NULL;
   int n = 20000;
   for (int i = 1; i <= n; i++)
   {
      Item * p = (Item *) malloc (sizeof (Item));
      p->value = i;
      p->next = first;
      first = p;
   }
   long sum = 0;
   int count = 0;
   for (Item * p = first; p != NULL; p = p->next)
   {
      sum = sum + p->value;
      count ++;
   }
   printf ("list: %d items, sum %ld\n", count, sum);

   char * big = (char *) calloc (300000, 1);
   big [299999] = 'x';
   printf ("calloc 300000: %s\n", big [0] == 0 && big [299999] == 'x' ? "OK" : "ERROR");

   long t = time (NULL);
   printf ("time: %s\n", t > 1600000000 ? "OK" : "ERROR");

   printf ("end\n");
   return 0;
}
