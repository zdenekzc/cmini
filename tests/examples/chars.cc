/* chars.c - pole znaku a ukazatele na znaky (1 bajt), aritmetika ukazatelu,
   metody std::string: substr, find, rfind, clear, reserve, s [i], string::npos,
   string (p, n), string (n, c) */

#include <stdio.h>
#include <string.h>
#include <string>
using namespace std;

struct Block
{
   int len;
   char buf [20];
   Block * next;
};

int count (const char * s)
{
   int n = 0;
   while (*s != 0)
   {
      s ++;
      n ++;
   }
   return n;
}

void upper (char * p, int n)
{
   for (int i = 0; i < n; i++)
      if (p [i] >= 'a' && p [i] <= 'z')
         p [i] = p [i] - 'a' + 'A';
}

int main ()
{
   char buf [10];
   for (int i = 0; i < 9; i++)
      buf [i] = 'a' + i;
   buf [9] = 0;
   printf ("%s %d\n", buf, count (buf));
   upper (buf + 2, 3);
   printf ("%s\n", buf);

   char * p = buf;
   p += 4;
   printf ("%c %c %d\n", *p, p [1], (int) (p - buf));
   p ++;
   -- p;
   * p = '#';
   printf ("%s\n", buf);

   unsigned char u [4];
   u [0] = 200;
   u [1] = 255;
   int sum = u [0] + u [1];
   char c [2];
   c [0] = 200;                       /* znamenkovy char: -56 */
   printf ("%d %d\n", sum, c [0]);

   Block * b = new Block;
   b->len = 0;
   const char * txt = "hello world";
   memcpy (& b->buf [b->len], txt + 6, 5);
   b->len += 5;
   b->buf [b->len] = 0;
   printf ("[%s] %d\n", b->buf, b->len);

   int a [4];
   for (int i = 0; i < 4; i++) a [i] = i * 10;
   int * q = a;
   q = q + 2;
   printf ("%d %d %d\n", *q, q [1], (int) (q - a));

   string s = "abc/def/ghi.txt";
   printf ("%s %s %s\n", s.substr (4).c_str (), s.substr (4, 3).c_str (), s.substr (0, 100).c_str ());
   printf ("%d %d %d %d\n", (int) s.find ('/'), (int) s.find ("def"), (int) s.find ('/', 4), (int) s.rfind ('/'));
   printf ("%d %d\n", s.find ("xyz") == string::npos, s.rfind ('#') == std::string::npos);
   printf ("%d %d\n", (int) s.rfind ("/", 5), (int) s.find (""));
   string t = "";
   for (int i = 0; i < (int) s.length (); i++)
      if (s [i] != '/')
         t += s [i];
   printf ("%s %c\n", t.c_str (), t [2]);
   string::size_type pos = s.find ('.');
   printf ("%s\n", s.substr (pos + 1).c_str ());
   t.clear ();
   printf ("[%s] %d\n", t.c_str (), (int) t.length ());
   string e = "";
   printf ("%d %d [%s]\n", (int) e.find ('a'), (int) e.rfind ("a"), e.substr (0).c_str ());

   /* string (p, n), string (n, c), reserve */
   char raw [8];
   for (int i = 0; i < 8; i++) raw [i] = 'k' + i;
   string part (raw + 2, 3);
   string from = string (raw, 5) + "|" + string (txt + 6, 5);
   string stars = string (4, '*') + string (0, '-') + string (1, 'x');
   string dash (3, '-');
   from.reserve (100);
   printf ("[%s] [%s] [%s] [%s] %d\n", part.c_str (), from.c_str (), stars.c_str (), dash.c_str (), (int) from.length ());
   return 0;
}
