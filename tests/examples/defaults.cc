/* defaults.c - prototypy funkci, implicitni hodnoty parametru (string, char,
   vyctova konstanta, NULL, bool), metody definovane mimo tridu, virtualni metody */

#include <stdio.h>
#include <string>
using namespace std;
const char quote2 = '"';
enum InclKind { normal_incl, user_incl, system_incl };
struct Item { int v; };

/* deklarace s implicitnimi hodnotami, definice pozdeji bez nich */
void show (string msg, string file_name = "", int line = 0, int column = 0);
string quoteStr (const string value, char quote = quote2);
int kind (string name, InclKind k = normal_incl);
int item (string msg, Item * p = NULL);

class A
{
   public:
      int base;
      A () { base = 100; }
      int add (int x, bool twice = false);
      string pre (string s, string prefix = "") { return prefix + s; }
      virtual int vf (int a, int b = 7) { return a + b; }
};

class B : public A
{
   public:
      int vf (int a, int b = 7) override { return a * b; }
};

int A::add (int x, bool twice) { return twice ? base + 2 * x : base + x; }

void show (string msg, string file_name, int line, int column)
{
   printf ("[%s] [%s] %d %d\n", msg.c_str (), file_name.c_str (), line, column);
}
string quoteStr (const string value, char quote) { string r = ""; r += quote; r += value; r += quote; return r; }
int kind (string name, InclKind k) { return k; }
int item (string msg, Item * p) { return p == NULL ? -1 : p->v; }

int main ()
{
   show ("a");
   show ("b", "f.c");
   show ("c", "f.c", 3);
   show ("d", "f.c", 3, 4);
   printf ("%s %s\n", quoteStr ("x").c_str (), quoteStr ("y", '\'').c_str ());
   printf ("%d %d\n", kind ("k"), kind ("k", system_incl));
   Item it; it.v = 9;
   printf ("%d %d\n", item ("i"), item ("i", &it));
   A a; A * p = new A; A * q = new B;
   printf ("%d %d %d\n", a.add (1), p->add (2, true), a.add (3, false));
   printf ("%s %s\n", a.pre ("s").c_str (), p->pre ("s", "p_").c_str ());
   printf ("%d %d %d\n", p->vf (2), q->vf (2), q->vf (2, 3));
   return 0;
}
