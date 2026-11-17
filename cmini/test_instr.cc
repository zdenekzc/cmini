
/* TEST_INSTR.CC */

/*
   Testy generatoru kodu (instr.cc, instr1.cc ... instr5.cc) bez Qt.
   Kazdy test je kratky program v C: prelozi se, spusti se jeho main
   a vysledek se porovna s ocekavanou hodnotou.

   Preklad a spusteni:  tests/build-test.sh
   Vypis assembleru:    ./test_instr.bin -v
   Preklad souboru:     ./test_instr.bin [-v] [-p] [-0|-1|-2|-3|-4|-5] tests/examples/quicksort.cc
   Testy jednoho generatoru: ./test_instr.bin -0   (CPARSER_RTL=own => instr.cc s vlastni knihovnou)
                        (-v vypis assembleru, -p vypis zpracovaneho zdrojoveho textu,
                         -0 generator z instr.cc (text pro asm/pasm.bin, ELF64),
                         -1 instr1.cc (vychozi), -2 instr2.cc, -3 instr3.cc, -4 instr4.cc, -5 instr5.cc)
   Testy se spousteji pro vsechny generatory (instr.cc, instr1.cc ... instr5.cc);
   instr.cc umi vse jako instr5.cc a navic tridy (v testech ma uroven 6:
   min_gen <= 6 <= max_gen).
   instr.cc je napsan v podmnozine c-mini (prelozi ho i cparser a cmini.bin -C)
   a sklada program jako seznam instrukci asm/asmlist.h (CHANGELOG.txt, kapitola 30).
*/

#include "ifc.h"
#include "inp.h"
#include "out.h"
#include "lex.h"
#ifdef USE_LEXER
#include "lexer.h"   /* lexer.c misto lex.c (preprocesor) */
#endif
#include "decl.h"
#include "instr.h"

#include <setjmp.h>
#include <climits>
#include <unistd.h>
#include <limits.h>
#include <fstream>
#include <vector>
#include <sstream>
#include <string>
#include <iostream>
#include <string.h>
using namespace std;

/* ------------------------- rozhrani ifc.h ----------------------------- */

static string source;

/* C linkage (inp.c, out.c); showError se nevraci: skoci pres longjmp na setjmp
   v runFile, runFiles nebo v cyklu testu (main). Bez vyjimek C++ - front-end v C
   nemusi mit .eh_frame (TinyCC; cmini/Makefile: -fno-asynchronous-unwind-tables) */
const char * getInpText (void) { return source.c_str (); }
void clearOutLines (void) { }
static bool print_source = false;
void showOutLine (const char * text) { if (print_source) cout << text << endl; }
static jmp_buf error_jump;
static string  error_msg;
void showError (const char * msg) { error_msg = msg; longjmp (error_jump, 1); }

/* ---------------------------------------------------------------------- */

static bool verbose = false;
/* NO_ASMJIT: jen generator instr.cc (-0, assembler pasm jako knihovna),
   instr1.cc ... instr5.cc (asmjit) se neprekladaji */
#ifdef NO_ASMJIT
static int  generator = 0;
static int  max_generator = 0;  /* testy bez parametru: jen instr.cc */
#else
static int  generator = 1;     /* 0 = instr.cc, 1 .. 5 = instr1.cc .. instr5.cc */
static int  max_generator = 5;
#endif

struct TestCase
{
    const char * name;
    const char * source;
    long         expected;
    bool         error = false;   /* ocekava se chyba prekladu */
    int          min_gen = 1;     /* test plati jen pro generatory min_gen .. max_gen */
    int          max_gen = 99;
};

static const TestCase tests [] =
{
    /* vyrazy */
    { "number",      "int main () { return 42; }",                     42 },
    { "hex",         "int main () { return 0x10 + 010; }",             24 },
    { "arith",       "int main () { return 2 + 3 * 4 - 1; }",          13 },
    { "parens",      "int main () { return (2 + 3) * 4; }",            20 },
    { "div",         "int main () { return 17 / 5; }",                  3 },
    { "div neg",     "int main () { return -17 / 5; }",                -3 },
    { "mod",         "int main () { return 17 % 5; }",                  2 },
    { "mod neg",     "int main () { return -17 % 5; }",                -2 },
    { "shl",         "int main () { return 1 << 10; }",              1024 },
    { "shr",         "int main () { return -16 >> 2; }",               -4 },
    { "bitops",      "int main () { return (12 & 10) | (1 ^ 3); }",    10 },
    { "unary",       "int main () { return -(+5) + ~0; }",             -6 },
    { "lognot",      "int main () { return !0 + !7; }",                 1 },
    { "less",        "int main () { return (1 < 2) + (2 < 1); }",       1 },
    { "compare",     "int main () { return (3 <= 3) + (3 >= 4) + (5 > 4) + (2 == 2) + (2 != 2); }", 3 },
    { "and",         "int main () { return (2 && 3) + (2 && 0) + (0 && 3); }", 1 },
    { "or",          "int main () { return (0 || 5) + (0 || 0) + (7 || 0); }", 2 },

    /* zkracene vyhodnoceni */
    { "and short 2", "int a; int main () { if (0 && (a = 1)) a = 5; return a; }", 0 },
    { "or short",    "int a; int main () { if (1 || (a = 1)) a = a + 5; return a; }", 5 },

    /* promenne a prikazy */
    { "global init", "int a = 7; int main () { return a; }",            7 },
    { "assign",      "int a, b; int main () { a = 10 + 20; b = a * 2; return b; }", 60 },
    { "if",          "int main () { if (1) return 1; return 2; }",      1 },
    { "if false",    "int main () { if (0) return 1; return 2; }",      2 },
    { "if else",     "int main () { if (0) return 1; else return 3; }", 3 },
    { "else if",     "int a = 2; int main () { if (a == 1) return 10; else if (a == 2) return 20; else return 30; }", 20 },
    { "while",       "int i, s; int main () { while (i < 10) { i = i + 1; s = s + i; } return s; }", 55 },
    { "while 0",     "int main () { while (0) return 1; return 2; }",   2 },
    { "for",         "int i, s; int main () { for (i = 1; i <= 5; i = i + 1) s = s * 2 + i; return s; }", 57 },
    { "for ever",    "int i; int main () { for (;;) { i = i + 1; if (i == 7) return i; } }", 7 },
    { "no return",   "int a; int main () { a = 5; }",                   0 },
    { "two fces",    "int f () { return 1; } int main () { return 2; }", 2 },

    /* lokalni promenne, parametry, volani */
    { "local var",   "int main () { int x; x = 1; return x; }",          1 },
    { "local init",  "int main () { int x = 3, y = x * 2; return x + y; }", 9 },
    { "param",       "int main (int argc, char * * argv) { return argc; }", 1 },
    { "call",        "int add (int x, int y) { return x - y; } int main () { return add (10, 3); }", 7 },
    { "call 6",      "int f (int a, int b, int c, int d, int e, int g) { return a+2*b+3*c+4*d+5*e+6*g; } int main () { return f (1,2,3,4,5,6); }", 91 },
    { "nested call", "int sq (int x) { return x * x; } int main () { return sq (sq (2) + 1) + sq (3); }", 34 },
    { "recursion",   "int fact (int n) { if (n <= 1) return 1; return n * fact (n - 1); } int main () { return fact (10); }", 3628800 },
    { "fib",         "int fib (int n) { return n < 2 ? n : fib (n-1) + fib (n-2); } int main () { return fib (20); }", 6765 },
    { "void fce",    "int g; void set (int v) { g = v; return; } int main () { set (8); return g; }", 8 },
    { "reference",   "void swap (int & x, int & y) { int t = x; x = y; y = t; } int main () { int a = 1, b = 2; swap (a, b); return a * 10 + b; }", 21 },
    { "ref array",   "int a[3]; inline void inc (int & x) { x++; } int main () { a[1] = 5; inc (a[1]); return a[1]; }", 6 },
    { "array",       "const int N = 10; int a[N]; int main () { for (int i = 0; i < N; i++) a[i] = i * i; return a[9] + a[3]; }", 90 },
    { "local array", "int main () { int b[5]; int s = 0; for (int i = 0; i < 5; i++) b[i] = i + 1; for (int i = 0; i < 5; i++) s += b[i]; return s; }", 15 },
    { "array 2d",    "int m[3][4]; int main () { for (int i = 0; i < 3; i++) for (int k = 0; k < 4; k++) m[i][k] = 10 * i + k; return m[2][3] + m[1][0]; }", 33 },
    { "inc dec",     "int main () { int i = 5; int a = i++; int b = ++i; int c = i--; return a * 100 + b * 10 + c - i; }", 571 },
    { "compound",    "int main () { int x = 10; x += 5; x -= 3; x *= 4; x /= 6; x %= 5; x <<= 3; x |= 1; return x; }", 25 },
    { "bool",        "bool ok = true; int main () { bool b = 7; if (ok && b) return b; return 0; }", 1 },
    { "char",        "int main () { char c = 'A'; return c + 1; }", 66 },
    { "break",       "int main () { int i; for (i = 0; i < 100; i++) if (i == 7) break; return i; }", 7 },
    { "continue",    "int main () { int s = 0; for (int i = 0; i < 10; i++) { if (i % 2) continue; s += i; } return s; }", 20 },
    { "do while",    "int main () { int i = 0; do i++; while (i < 5); return i; }", 5 },
    { "libc",        "int main () { srand (1); return abs (-5) + (rand () >= 0); }", 6 },
    { "printf",      "int main () { return printf (\"printf test %d\\n\", 42); }", 15 },

    /* hluboke vnoreni - v instr2.cc dojdou registry (zaloha push) */
    { "deep right",  "int main () { return 1+(2+(3+(4+(5+(6+(7+(8+(9+(10+(11+(12+(13+(14+(15+16)))))))))))))); }", 136 },
    { "deep left",   "int main () { return ((((((((((((((1-2)-3)-4)-5)-6)-7)-8)-9)-10)-11)-12)-13)-14)-15); }", -118 },
    { "deep calls",  "int f (int x) { return x * (x + (x - (x * 2))); } "
                     "int main () { return f(1) + (f(2) + (f(3) + (f(4) + (f(5) + (f(6) + f(7)))))); }", 0 },
    { "deep div",    "int id (int x) { return x; } "
                     "int main () { return 1000 / (id(2) * (id(5) - (id(3) / (id(1) + (id(1) % (id(7) - id(5))))))); }", 125 },
    { "callee saved","int g (int n) { if (n == 0) return 0; return n + (n * (n + (n - g (n - 1)))); } "
                     "int main () { return 100 + (g(3) + (g(4) - (g(2) * (g(1) + g(0))))); }", 97 },
    { "deep printf", "int main () { return 1 + (2 + (3 + (4 + (5 + (6 + (7 + (8 + (9 + (10 + (11 + (12 + (13 + printf (\"\"))))))))))))); }", 91 },

    /* argumenty volani - poradi, vnorena volani, cykly v registrech */
    { "args order",  "int f (int a, int b, int c, int d, int e, int g) { return ((((a*10+b)*10+c)*10+d)*10+e)*10+g; } "
                     "int main () { return f (1, 2, 3, 4, 5, 6); }", 123456 },
    { "args nested", "int f (int a, int b, int c, int d, int e, int g) { return ((((a*10+b)*10+c)*10+d)*10+e)*10+g; } "
                     "int id (int x) { return x; } "
                     "int main () { return f (1, id (2), 3, id (id (4)), 5, id (6) + 0 * id (7)); }", 123456 },
    { "args heavy",  "int f (int a, int b, int c) { return (a * 100 + b) * 100 + c; } "
                     "int main () { int x = 1, y = 2, z = 3; return f (x, y * (z + (x - (y * z))), z); }", 9603 },
    { "args swap",   "int f (int a, int b) { return a * 1000 + b; } "
                     "int g (int a, int b) { return f (b, a); } int main () { return g (3, 7); }", 7003 },
    { "args rotate", "int f (int a, int b, int c, int d, int e, int g) { return ((((a*10+b)*10+c)*10+d)*10+e)*10+g; } "
                     "int r (int a, int b, int c, int d, int e, int g) { return f (g, a, b, c, d, e); } "
                     "int main () { return r (1, 2, 3, 4, 5, 6); }", 612345 },
    { "args refs",   "void sw (int & x, int & y) { int t = x; x = y; y = t; } int a[4]; "
                     "int main () { a[0] = 1; a[3] = 4; sw (a[0], a[a[0] + 2]); return a[0] * 10 + a[3]; }", 41 },
    { "args deep",   "int f (int a, int b, int c, int d, int e, int g) { return a + b + c + d + e + g; } "
                     "int main () { return f (f (1,2,3,4,5,6), f (1,1,1,1,1,1), 1+(2+(3+(4+(5+(6+(7+(8+(9+(10+(11+(12+13))))))))))), "
                     "f (2,2,2,2,2,2), 4, f (3,3,3,3,3, 1+(2+(3+(4+(5+(6+(7+(8+(9+(10+(11+(12+13)))))))))))) ); }", 240 },
    { "args printf", "int main () { return printf (\"%d %d %d %d %d\\n\", 1, 2 * (3 + 4), 5, 6 - (7 - (8 - 9)), abs (-10)); }", 13 },

    /* promenne v registrech (instr5.cc) */
    { "reg for",     "int main () { int s = 0; for (int i = 0; i < 10; i++) s += i; return s; }", 45 },
    { "reg for 2",   "int m[5][6]; int main () { int s = 0; for (int i = 0; i < 5; i++) for (int k = 0; k < 6; k++) m[i][k] = i * k; "
                     "for (int i = 0; i < 5; i++) for (int k = 0; k < 6; k++) s += m[i][k]; return s; }", 150 },
    { "reg for 5",   "int main () { int s = 0; for (int a = 0; a < 2; a++) for (int b = 0; b < 3; b++) for (int c = 0; c < 4; c++) "
                     "for (int d = 0; d < 5; d++) for (int e = 0; e < 6; e++) s += a + b + c + d + e; return s; }", 5400 },
    { "reg calls",   "int sum (int n) { int s = 0; for (int i = 1; i <= n; i++) s += i; return s; } "
                     "int main () { int t = 0; for (int i = 1; i <= 10; i++) for (int k = 1; k <= 3; k++) t += sum (i) * k; return t; }", 1320 },
    { "reg recurse", "int f (int n) { int s = 0; if (n == 0) return 1; for (int i = 0; i < 3; i++) s += f (n - 1) + i; return s; } "
                     "int main () { return f (4); }", 201 },
    { "reg declared","int main () { register int x = 5; register int y; y = x * 3; x += y; return x * 100 + y; }", 2015 },
    { "reg param",   "int f (register int n) { register int r = 1; while (n > 0) { r *= 2; n--; } return r; } int main () { return f (10); }", 1024 },
    { "reg ref arg", "void inc (int & x) { x++; } int main () { int s = 0; for (int i = 0; i < 5; i++) inc (s); "
                     "int k; for (k = 0; k < 3; k++) inc (k); return s * 10 + k; }", 54 },
    { "reg bool",    "int main () { int n = 0; for (bool b = true; b; b = false) n++; return n; }", 1 },
    { "reg global",  "int i; int main () { int s = 0; for (i = 0; i < 4; i++) s += i; return s * 10 + i; }", 64 },
    { "reg printf",  "int main () { int s = 0; for (int i = 0; i < 3; i++) s += printf (\"%d\", i); printf (\"\\n\"); return s; }", 3 },

    /* struktury, ukazatele, konstruktory (instr5.cc) */
    { "ptr",         "int main () { int x = 5; int * p = &x; *p = *p + 1; return x; }", 6, false, 5 },
    { "ptr array",   "int a[4]; int main () { int * p = a; p[2] = 7; return a[2]; }", 7, false, 5 },
    { "ptr param",   "void set (int * p, int v) { *p = v; } int main () { int x; set (&x, 9); return x; }", 9, false, 5 },
    { "struct",      "struct P { int x; int y; }; P g; int main () { P l; l.x = 3; l.y = 4; g.x = 10; g.y = 20; return l.x * l.y + g.x + g.y; }", 42, false, 5 },
    { "struct arr",  "struct P { int x; int a[3]; int y; }; P t[3]; int main () { for (int i = 0; i < 3; i++) { t[i].x = i; t[i].y = 10 * i; "
                     "for (int k = 0; k < 3; k++) t[i].a[k] = 100 * i + k; } return t[2].a[1] + t[1].y + t[2].x; }", 213, false, 5 },
    { "struct ref",  "struct P { int x; int y; }; void sw (P & p) { int t = p.x; p.x = p.y; p.y = t; } "
                     "int main () { P q; q.x = 1; q.y = 2; sw (q); return q.x * 10 + q.y; }", 21, false, 5 },
    { "new",         "struct N { int v; N * next; }; int main () { N * h = nullptr; for (int i = 1; i <= 5; i++) { N * n = new N; n->v = i; n->next = h; h = n; } "
                     "int s = 0; while (h != nullptr) { s = s * 10 + h->v; h = h->next; } return s; }", 54321, false, 5 },
    { "new zero",    "struct N { int a; int b[3]; N * p; }; int main () { N * n = new N; return n->a + n->b[0] + n->b[2] + (n->p == nullptr); }", 1, false, 5 },
    /* struktury hodnotou (instr.cc): parametr = kopie, vysledek funkce, T x = f (), f ().x */
    { "sval param",  "struct P { int x; int y; }; int f (P p) { p.x = 100; return p.x + p.y; } "
                     "int main () { P a; a.x = 1; a.y = 2; int r = f (a); return r * 10 + a.x; }", 1021, false, 6 },
    { "sval ret",    "struct P { long x; long y; long z; }; P mk (long a) { P p; p.x = a; p.y = 2 * a; p.z = 3 * a; return p; } "
                     "int main () { P q = mk (5); P r; r = mk (1); return q.x + q.y + q.z + r.z + mk (7).y; }", 47, false, 6 },
    { "sval chain",  "struct V { int a; int b; }; V add (V u, V v) { V r; r.a = u.a + v.a; r.b = u.b + v.b; return r; } "
                     "V v2 (int a, int b) { V r; r.a = a; r.b = b; return r; } "
                     "int main () { V s = add (add (v2 (1, 2), v2 (10, 20)), v2 (100, 200)); return s.a + s.b; }", 333, false, 6 },
    { "sval args",   "struct P { int x; int y; }; int f (int a, int b, int c, int d, int e, int g, P p, int h, P q) "
                     "{ return a + b + c + d + e + g + p.x * 10 + p.y * 100 + h + q.x * 1000; } "
                     "int main () { P p; p.x = 1; p.y = 2; P q; q.x = 3; q.y = 0; return f (1, 1, 1, 1, 1, 1, p, 4, q); }", 3220, false, 6 },
    { "sval method", "struct P { int x; int y; }; class C { public: int k; P twice (P p) { P r; r.x = p.x * k; r.y = p.y * k; return r; } }; "
                     "int main () { C c; c.k = 2; P p; p.x = 3; p.y = 4; P r = c.twice (p); return r.x * 10 + r.y + c.k * 100; }", 268, false, 6 },
    { "sval ptr",    "struct P { int x; int y; }; P get (P * p) { return * p; } "
                     "int main () { P a; a.x = 7; a.y = 8; P b = get (& a); a.x = 0; return b.x * 10 + b.y; }", 78, false, 6 },
    { "sval string", "#include <string>\nusing namespace std;\nstruct S { string s; }; S f (S x) { return x; } int main () { S a; a.s = \"abc\"; S b = f (a); return a.s == b.s; }", 1, false, 6 },
    { "new int",     "int main () { int * p = new int; *p = 12; return *p; }", 12, false, 5 },
    { "ctor init",   "struct I { int k; int h; I * l; I () : k (5), h (1), l (nullptr) { } }; int main () { I * p = new I; return p->k * 10 + p->h + (p->l == nullptr); }", 52, false, 5 },
    { "ctor body",   "struct B { int c; int k[3]; B (); }; B::B () { c = 7; for (int i = 0; i < 3; i++) k[i] = -1; } "
                     "int main () { B * b = new B; return b->c * 100 + b->k[0] + b->k[2]; }", 698, false, 5 },
    { "ctor args",   "struct P { int x; int y; P (int a, int b = 4) : x (a) { y = b * 10; } }; "
                     "int main () { P * p = new P (3); P * q = new P (5, 6); return p->x + p->y + q->x + q->y; }", 108, false, 5 },
    { "ctor calls",  "int cnt; int next () { cnt++; return cnt; } struct P { int id; P () { id = next (); } }; "
                     "int main () { P * a = new P; P * b = new P; P * c = new P; return a->id * 100 + b->id * 10 + c->id; }", 123, false, 5 },
    { "ref to ptr",  "struct N { int v; N * l; N * r; N () : v (0), l (nullptr), r (nullptr) { } }; "
                     "void ins (N * & t, int v) { if (t == nullptr) { t = new N; t->v = v; } else if (v < t->v) ins (t->l, v); else ins (t->r, v); } "
                     "int sum (N * t, int d = 1) { if (t == nullptr) return 0; return d * t->v + sum (t->l, d + 1) + sum (t->r, d + 1); } "
                     "int main () { N * root = nullptr; ins (root, 5); ins (root, 3); ins (root, 8); ins (root, 1); return sum (root); }", 30, false, 5 },
    { "default arg", "int f (int a, int b = 10, int c = 100) { return a + b + c; } int main () { return f (1) + f (1, 2) + f (1, 2, 3); }", 220, false },
    { "prototype",   "int f (int a, int b = 3); int g (int); int main () { return f (1) + g (2); } "
                     "int f (int a, int b) { return a * 10 + b; } int g (int x) { return x * 100; }", 213, false },
    { "proto twice", "int f (int); int f (int a = 4); int main () { return f (); } int f (int x) { return x * 2; } int f (int);", 8, false },
    { "proto void",  "int f (); int main () { return f (); } int f (void) { return 6; }", 6, false },
    { "proto redef", "int f (int a) { return a; } int f (int a) { return a; } int main () { return f (1); }", 0, true },
    { "proto params","int f (int a); int f (int a, int b) { return a; } int main () { return f (1, 2); }", 0, true },
    { "method dflt", "class A { public: int k; A () { k = 10; } int f (int a, int b = 5); }; "
                     "int A::f (int a, int b) { return k + a * b; } int main () { A a; return a.f (2) * 100 + a.f (2, 1); }", 2012, false, 6 },
    { "dtor order",  "int k; class A { public: int v; A () { v = 0; } ~A () { k = k * 10 + v; } }; "
                     "int f () { A a; a.v = 1; { A b; b.v = 2; } A c; c.v = 3; return 0; } int main () { f (); return k; }", 231, false, 6 },
    { "dtor virtual","int k; class B { public: virtual ~B () { k += 1; } }; class D : public B { public: ~D () { k += 10; } }; "
                     "int main () { B * p = new D; delete p; return k; }", 11, false, 6 },
    { "dtor outside","class A { public: ~A (); };  A::~A () { }  void A_f () { } A::~A () { } int main () { return 0; }", 0, true, 6 },
    { "dtor undecl", "class A { public: int x; }; A::~A () { } int main () { return 0; }", 0, true, 6 },
    { "dtor array",  "class A { public: ~A () { } }; int main () { A * p = new A [2]; delete [] p; return 0; }", 0, true, 6 },
    { "fn pointer",  "int sq (int x) { return x * x; } int tw (int x) { return 2 * x; } "
                     "int main () { int (*f) (int) = sq; int r = f (5); f = &tw; return r * 100 + (*f) (3); }", 2506, false, 6 },
    { "fn ptr arr",  "int a1 () { return 1; } int a2 () { return 2; } typedef int (*F) (); F t [2] = { a1, a2 }; "
                     "int main () { return t [0] () * 10 + t [1] (); }", 12, false, 6 },
    { "fn ptr int",  "int f (int x) { return x; } int main () { int k = f; return 0; }", 0, true, 6 },
    { "fn ptr meth", "class A { public: int m () { return 1; } }; int main () { A a; return 0 + (a.m == nullptr); }", 0, true, 6 },
    { "seq basic",   "Sequence <int> s; int main () { for (int i = 0; i < 10; i++) s.add (i); int t = 0; for (int x : s) t += x; return t + s.size (); }", 55, false, 6 },
    { "dict basic",  "Dictionary <string, int> d; int main () { d [\"a\"] = 1; d.store (\"b\", 2); return d.get (\"a\") * 10 + d [\"b\"] + d.get (\"c\"); }", 12, false, 6 },
    { "seq long",    "int main () { Sequence <long> s; Sequence <unsigned> u; Dictionary <long, unsigned long> d; s.add (1L << 40); u.add (7); d [3] = 5; return (s [0] >> 40) + u [0] + d.get (3); }", 13, false, 6 },
    { "seq type",    "Sequence <int> a; Sequence <string> b; int main () { a = b; return 0; }", 0, true, 6 },
    { "seq method",  "Sequence <int> a; int main () { a.append (1); return 0; }", 0, true, 6 },
    { "seq nested",  "Sequence <Sequence <int>> a; int main () { return 0; }", 0, true, 6 },
    { "dict key",    "Dictionary <double, int> d; int main () { return 0; }", 0, true, 6 },
    { "seq other gen","Sequence <int> s; int main () { s.add (1); return 0; }", 0, true, 1, 5 },
    { "dyncast",     "class A { public: virtual int f () { return 1; } }; class B : public A { public: int f () { return 2; } }; "
                     "class C : public A { }; int main () { A * a = new B; A * c = new C; "
                     "return (dynamic_cast <B *> (a) != nullptr) * 10 + (dynamic_cast <B *> (c) == nullptr); }", 11, false, 6 },
    { "dyncast poly","class A { public: int x; }; class B : public A { }; int main () { A * a = new B; B * b = dynamic_cast <B *> (a); return 0; }", 0, true, 6 },
    { "dyncast int", "class A { public: virtual int f () { return 1; } }; int main () { A * a = new A; int * p = dynamic_cast <int *> (a); return 0; }", 0, true, 6 },
    { "enum const",  "enum K { a, b, c = 10, d }; K g = d; int f (K k = b) { return k; } "
                     "int main () { K x = c; int s = 0; if (x == c) s = 1; return f () * 1000 + g * 10 + s + (a == 0); }", 1112, false, 6 },
    { "assert ok",   "int main () { int x = 3; assert (x == 3); assert (x > 0 && x < 5); return x; }", 3, false, 5 },
    { "union",       "union U { int a; int b; }; int main () { U u; u.a = 7; return u.b; }", 7, false, 5 },
    { "argv",        "int main (int argc, char * argv []) { return argc; }", 1 },

    /* zatim nepodporovane => chyba */
    { "struct value","struct P { int x; }; P a, b; int main () { a = b; return 0; }", 0, true, 5, 5 },
    { "ptr arith",   "int a[3]; int main () { int * p = a; p = p + 1; return 0; }", 0, true, 5, 5 },
    { "char ptr",    "int main () { char * s = \"abc\"; return s[0]; }", 0, true, 5, 5 },
    { "global ctor", "struct P { int x; P () { x = 1; } }; P g; int main () { return g.x; }", 0, true, 5, 5 },
    { "pointer",     "int main () { int x; int * p = &x; *p = 1; return x; }", 0, true, 1, 4 },
    { "no main",     "int f () { return 1; }",                          0, true },

    /* tridy, dedicnost, konstruktory, virtualni metody - jen instr.cc (uroven 6) */
    { "class",       "class A { public: int x; int get () { return x; } void set (int v) { x = v; } };"
                     "int main () { A a; a.set (7); return a.get (); }", 7, false, 6 },
    { "ctor default","class A { public: int x; A () { x = 5; } }; int main () { A a; A * p = new A; return a.x * 10 + p->x; }", 55, false, 6 },
    { "global obj",  "struct P { int x; P () { x = 1; } }; P g; int main () { return g.x; }", 1, false, 6 },
    { "inherit",     "class A { public: int x; int f () { return x; } };"
                     "class B : public A { public: int y; B () { x = 3; y = 4; } int g () { return f () + y; } };"
                     "int main () { B b; return b.g () * 10 + b.x; }", 73, false, 6 },
    { "virtual",     "class S { public: virtual int area () { return 0; } };"
                     "class R : public S { public: int w; int h; R () { w = 2; h = 3; } int area () { return w * h; } };"
                     "class C : public S { public: int r; C () { r = 2; } int area () override { return 3 * r * r; } };"
                     "int main () { S * a = new R; S * b = new C; S * c = new S; return a->area () * 100 + b->area () * 10 + c->area (); }", 720, false, 6 },
    { "this",        "class N { public: int v; N * next; N () { next = nullptr; } N * add (int x) { v = x; return this; } };"
                     "int main () { N n; return n.add (9)->v + (n.next == nullptr); }", 10, false, 6 },
    { "base call",   "class A { public: virtual int f () { return 1; } };"
                     "class B : public A { public: int f () { return A::f () + 10; } };"
                     "int main () { A * p = new B; return p->f (); }", 11, false, 6 },
    { "ctor chain",  "int log = 0; class A { public: A () { log = log * 10 + 1; } };"
                     "class B : public A { public: B () { log = log * 10 + 2; } };"
                     "class C : public B { public: C () { log = log * 10 + 3; } };"
                     "int main () { C c; return log; }", 123, false, 6 },
    { "member obj",  "int cnt = 0; class A { public: int id; A () { cnt++; id = cnt; } };"
                     "class B { public: int k; A a; A arr [3]; };"
                     "int main () { B b; return b.a.id * 1000 + b.arr[0].id * 100 + b.arr[2].id * 10 + cnt; }", 1244, false, 6 },
    { "obj array",   "int n = 0; class P { public: int v; P () { n++; v = n; } };"
                     "int main () { P a [4]; return a[3].v * 10 + n; }", 44, false, 6 },
    { "virt in ctor","class A { public: int r; A () { r = f (); } virtual int f () { return 1; } };"
                     "class B : public A { public: int f () { return 2; } };"
                     "int main () { B b; return b.r * 10 + b.f (); }", 12, false, 6 },
    { "outside def", "class A { public: int x; A (); int get (); virtual int twice (); };"
                     "A::A () { x = 21; } int A::get () { return x; } int A::twice () { return get () * 2; }"
                     "int main () { A * p = new A; return p->twice (); }", 42, false, 6 },
    { "pure virtual","class S { public: virtual int f () = 0; int g () { return f () + 1; } };"
                     "class T : public S { public: int f () { return 5; } };"
                     "int main () { T t; return t.g (); }", 6, false, 6 },
    { "static meth", "class M { public: static int sq (int x) { return x * x; } int h () { return sq (4); } };"
                     "int main () { M m; return m.h (); }", 16, false, 6 },
    { "access",      "class A { private: int x; protected: int y; public: A () { x = 1; y = 2; } virtual ~A () { } int sum () { return x + y; } };"
                     "int main () { A a; return a.sum (); }", 3, false, 6 },
    { "virt array",  "class B { public: virtual int id () { return 1; } }; class D : public B { public: int id () { return 2; } };"
                     "int main () { B * a [3]; a[0] = new B; a[1] = new D; a[2] = new D; int s = 0;"
                     " for (int i = 0; i < 3; i++) s = s * 10 + a[i]->id (); return s; }", 122, false, 6 },
    { "deep virtual","class A { public: virtual int f () { return 1; } int g () { return 7; } };"
                     "class B : public A { public: virtual int h () { return 20; } };"
                     "class C : public B { public: int f () { return 3; } int h () { return 40; } };"
                     "int main () { A * p = new C; A * q = new B; B * r = new C; return p->f () * 1000 + q->f () * 100 + r->h () + r->g (); }", 3147, false, 6 },
    { "vptr fields", "class A { public: int a; A () { a = 1; } }; class B : public A { public: int b; B () { b = 2; } virtual int s () { return a + b; } };"
                     "class C : public B { public: int c; C () { c = 3; } int s () { return a * 100 + b * 10 + c; } };"
                     "int main () { B * p = new C; A * q = p; return p->s () * 10 + q->a; }", 1231, false, 6 },
    /* std::string - jen instr.cc (uroven 6) */
    { "str assign",  "#include <string>\nusing namespace std;\n"
                     "int main () { string a; string b = \"abc\"; a = b; return a.length () * 10 + a.empty () + b.size (); }", 33, false, 6 },
    { "str concat",  "int main () { std::string a = \"ab\"; std::string b = a + \"cd\" + 'e'; b += a; b += 'x'; b += \"\";"
                     " return printf (\"%s\\n\", b.c_str ()); }", 9, false, 6 },
    { "str compare", "int main () { string a = \"abc\"; string b = \"abd\"; string c = \"ab\"; string e;"
                     " return (a < b) + 2 * (c < a) + 4 * (a == \"abc\") + 8 * (a != b) + 16 * (b >= a) + 32 * (e < c) + 64 * (e == \"\") + 128 * (a > b); }", 127, false, 6 },
    { "str func",    "string twice (const string & s) { return s + s; } void app (string & s, string t) { s += t; }"
                     "int main () { string x = twice (\"ab\"); app (x, \"!\"); return printf (\"%s\\n\", x.c_str ()); }", 6, false, 6 },
    { "str member",  "class P { public: string name; int n; P () { n = 1; } void add (string s) { name += s; n++; } };"
                     "string g = \"glob\"; int main () { P p; P * q = new P; p.add (\"x\"); q->add (g); q->add (\"y\");"
                     " return (p.name == \"x\") + 2 * (q->name == \"globy\") + 4 * (q->name.length () == 5) + 8 * q->n; }", 31, false, 6 },
    { "str loop",    "int main () { string s; for (int i = 0; i < 100; i++) s += 'a' + i % 26; return s.length () + (s < \"abd\"); }", 101, false, 6 },
    { "str array",   "string names [3]; int main () { names [1] = \"b\"; string loc [2]; loc [0] = names [1] + names [0];"
                     " return (loc [0] == \"b\") + 2 * loc [1].empty (); }", 3, false, 6 },
    { "str bad op",  "int main () { string a; a = a * 2; return 0; }", 0, true, 6 },
    { "str bad int", "int main () { string a; int x = 1; x = a; return 0; }", 0, true, 6 },
    { "str old gen", "int main () { string a; return 0; }", 0, true, 1, 5 },

    { "multi inherit","class A { }; class B { }; class C : public A, public B { }; int main () { return 0; }", 0, true, 6 },
    { "this outside","int main () { return this == nullptr; }", 0, true, 6 },
    { "no default ctor","class A { public: int x; A (int v) { x = v; } }; int main () { A a; return a.x; }", 0, true, 6 },
    { "undefined",   "int f (int x); int main () { return f (1); }",    0, true },

#ifdef USE_LEXER
    /* preprocesor (lexer.c) */
    { "pp define",   "#define N 10\nint main () { return N + 1; }", 11 },
    { "pp macro",    "#define SQ(x) ((x) * (x))\n#define MAX(a, b) ((a) > (b) ? (a) : (b))\n"
                     "int main () { return MAX (SQ (3), MAX (2, 7)); }", 9 },
    { "pp nested",   "#define TWICE(x) (2 * (x))\nint main () { return TWICE (TWICE (TWICE (1))); }", 8 },
    { "pp if",       "#define A 2\n#if A * 2 == 4 && defined (A)\nint main () { return 1; }\n"
                     "#elif 1\nint main () { return 2; }\n#else\nint main () { return 3; }\n#endif\n", 1 },
    { "pp elif",     "#if 0\nint main () { return 1; }\n#elif 2 > 1\nint main () { return 2; }\n#else\nint main () { return 3; }\n#endif", 2 },
    { "pp ifdef",    "#ifdef B\nint x = 5;\n#else\nint x = 6;\n#endif\n#ifndef B\n#define B 1\n#endif\nint main () { return x + B; }", 7 },
    { "pp nested if","#if 0\n#if 1\nint x = 1;\n#else\nint x = 2;\n#endif\n#else\nint x = 3;\n#endif\nint main () { return x; }", 3 },
    { "pp paste",    "#define CAT(a, b) a ## b\nint xy = 4;\nint main () { return CAT (x, y) + CAT (1, 2); }", 16 },
    { "pp undef",    "#define N 1\n#undef N\n#define N 2\nint main () { return N; }", 2 },
    { "pp recursion","int g = 3;\n#define g g + 1\nint main () { return g; }", 4 },
    { "pp line",     "#define L __LINE__\n\nint main () { return L; }", 3 },
    { "pp cont",     "#define ADD(a, b) \\\n  ((a) + \\\n   (b))\nint main () { return ADD (2, 3); }", 5 },
    { "pp skip",     "#if 0\nit's not code \"\n#endif\nint main () { return 1; }", 1 },
    { "pp variadic", "#define SUM3(...) sum (__VA_ARGS__)\nint sum (int a, int b, int c) { return a + b + c; }\n"
                     "int main () { return SUM3 (1, 2, 3); }", 6 },
    { "pp string",   "#define S(x) #x\nint main () { return printf (S (a  +  b) \"\\n\"); }", 6 },
    { "pp fn name",  "int f (int x) { return x * 10; }\n#define f(x) (x + 1)\nint main () { return (f) (2) + f (2); }", 23 },
    { "pp error",    "#error stop\nint main () { return 0; }", 0, true },
    { "pp unterm",   "#if 1\nint main () { return 0; }", 0, true },
    { "pp args",     "#define F(a, b) a\nint main () { return F (1); }", 0, true },
#endif
};

/* ---------------------------------------------------------------------- */

static long compile (const char * text)
{
    source = text;
    OpenInp ();
    OpenOut ();
    InitLex ();
    InitDecl ();

    while (sy != EOS)
        GlobalDecl ();

    if (print_source)
    {
        PutGlobalDecl ();
        CloseOut ();
    }

#ifdef NO_ASMJIT
    switch (generator)
    {
        case 0:  return runInstructions (verbose);
        default: showError ("generators -1 .. -5 need asmjit (built with NO_ASMJIT)"); return 0;
    }
#else
    switch (generator)
    {
        case 2:  return runInstructions2 (verbose);
        case 3:  return runInstructions3 (verbose);
        case 4:  return runInstructions4 (verbose);
        case 5:  return runInstructions5 (verbose);
        case 0:  return runInstructions (verbose);
        case 1:
        default: return runInstructions1 (verbose);
    }
#endif
}

static int runFile (const char * file_name)
{
    ifstream f (file_name);
    if (!f)
    {
        cerr << "Cannot open " << file_name << endl;
        return 2;
    }
    stringstream buf;
    buf << f.rdbuf ();
    string text = buf.str ();

#ifdef USE_LEXER
    LexFileName (file_name); /* adresar pro #include "..." */
    LexDefine ("__CPARSER__", "1");   /* zdrojovy text muze rozlisit cparser a g++ */
#endif

    if (setjmp (error_jump) == 0)
    {
        long result = compile (text.c_str ());
        if (InstrOutputFile != nullptr || InstrObjectFile != nullptr)
            return 0;
        cout << "main returned " << result << endl;
        return 0;
    }
    else
    {
        /* chyba pri prehravani tela metody (konec tridy): pozice symbolu */
        if (ReplaySymbol != nullptr && ReplayFile != nullptr)
        {
            cerr << ReplayFile << ":" << ReplayLine << ": error: " << error_msg << endl;
            return 1;
        }
#ifdef USE_LEXER
        /* chyba ve vlozenem souboru: soubor a radka z preprocesoru */
        if (LexCurrentFile () != nullptr)
        {
            cerr << LexCurrentFile () << ":" << LexCurrentLine () << ": error: " << error_msg << endl;
            return 1;
        }
#endif
        cerr << file_name << ":" << InpLineNum << ":" << InpColNum << ": error: " << error_msg << endl;
        return 1;
    }
}

static bool endsWith (string s, string e)
{
    return s.length () >= e.length () && s.compare (s.length () - e.length (), e.length (), e) == 0;
}

static string baseName (string path)
{
    size_t p = path.rfind ('/');
    string b = (p == string::npos) ? path : path.substr (p + 1);
    size_t d = b.rfind ('.');
    return (d == string::npos) ? b : b.substr (0, d);
}

static string absPath (string path)
{
    char buf [PATH_MAX];
    if (realpath (path.c_str (), buf) != nullptr) return buf;
    return path;
}

/* vice souboru:  -c => objekty .o;  jinak preklad po souborech do docasnych
   objektu, slinkovani (i .o, .a, -l..., -L...) a spusteni nebo ulozeni (-o) */
static bool keep_asm = false;   /* -S: s objektem (-c) i text pro assembler x.o => x.asm */

static int runFiles (vector <string> sources, vector <string> inputs, bool compile_only)
{
    if (generator != 0)
    {
        cerr << "Object files and more source files are supported only by -0 (instr.cc)" << endl;
        return 2;
    }
    const char * out = InstrOutputFile;
    InstrOutputFile = nullptr;

    if (compile_only)
    {
        for (string & src : sources)
        {
            string obj = (out != nullptr && sources.size () == 1) ? string (out) : baseName (src) + ".o";
            string stem = endsWith (obj, ".o") ? obj.substr (0, obj.length () - 2) : obj;
            InstrObjectFile = obj.c_str ();
            if (keep_asm) InstrOutputFile = stem.c_str ();     /* instr.cc: stem + ".asm" */
            int rc = runFile (src.c_str ());
            InstrObjectFile = nullptr;
            InstrOutputFile = nullptr;
            if (rc != 0) return rc;
        }
        return 0;
    }

    const char * tmp = getenv ("TMPDIR");
    string templ = string (tmp != nullptr && tmp [0] != 0 ? tmp : "/tmp") + "/cparser-objs-XXXXXX";
    vector <char> buf (templ.begin (), templ.end ());
    buf.push_back (0);
    if (mkdtemp (buf.data ()) == nullptr)
    {
        cerr << "Cannot create temporary directory" << endl;
        return 2;
    }
    string dir = buf.data ();
    vector <string> objs;
    int rc = 0;
    for (size_t i = 0; i < sources.size () && rc == 0; i++)
    {
        string obj = dir + "/" + to_string (i) + "_" + baseName (sources [i]) + ".o";
        objs.push_back (obj);
        InstrObjectFile = obj.c_str ();
        rc = runFile (sources [i].c_str ());
        InstrObjectFile = nullptr;
    }
    if (rc == 0)
    {
        vector <string> all = objs;
        for (string & in : inputs)
        {
            if (in.compare (0, 2, "-l") == 0) all.push_back (in);
            else if (in.compare (0, 2, "-L") == 0) all.push_back ("-L" + absPath (in.substr (2)));
            else all.push_back (absPath (in));
        }
        vector <char *> args;   /* pred setjmp - longjmp nevola destruktory */
        for (string & a : all) args.push_back ((char *) a.c_str ());
        if (setjmp (error_jump) == 0)
        {
            long result = InstrLink (args.data (), (int) args.size (), out);
            if (out == nullptr)
                cout << "main returned " << result << endl;
        }
        else
        {
            cerr << "error: " << error_msg << endl;
            rc = 1;
        }
    }
    for (string & o : objs) unlink (o.c_str ());
    rmdir (dir.c_str ());
    return rc;
}

int main (int argc, char * argv [])
{
    int arg = 1;
    bool compile_only = false;   /* -c */
    bool gen_given = false;      /* -0 .. -5 bez souboru => testy jen tohoto generatoru */
    for (; arg < argc && argv [arg][0] == '-'; arg ++)
        if (strlen (argv [arg]) == 2 && argv [arg][1] >= '0' && argv [arg][1] <= '5')
        {
            generator = argv [arg][1] - '0';
            gen_given = true;
        }
        else if (strcmp (argv [arg], "-v") == 0)
            verbose = true;
        else if (strcmp (argv [arg], "-p") == 0)
            print_source = true;
#ifdef USE_LEXER
        else if (strcmp (argv [arg], "-I") == 0 && arg + 1 < argc)
            LexIncludeDir (argv [++ arg]);       /* adresar pro #include */
#endif
        else if (strcmp (argv [arg], "-o") == 0 && arg + 1 < argc)
            InstrOutputFile = argv [++ arg];     /* jen -0: ulozit program, nespoustet */
        else if (strcmp (argv [arg], "-c") == 0)
            compile_only = true;                 /* jen -0: objekty .o */
        else if (strcmp (argv [arg], "-S") == 0)
            keep_asm = true;                     /* s -c: i text pro assembler (x.o => x.asm) */
        else if (strcmp (argv [arg], "-0") == 0)
            generator = 0;
        else if (strcmp (argv [arg], "-1") == 0)
            generator = 1;
        else if (strcmp (argv [arg], "-2") == 0)
            generator = 2;
        else if (strcmp (argv [arg], "-3") == 0)
            generator = 3;
        else if (strcmp (argv [arg], "-4") == 0)
            generator = 4;
        else if (strcmp (argv [arg], "-5") == 0)
            generator = 5;

    /* test_instr.bin [-v] soubor.c => prelozit a spustit soubor;
       -c a.c b.c => objekty;  a.c b.c x.o -lm => slinkovat a spustit (-o ulozit) */
    if (arg < argc)
    {
        vector <string> sources, inputs;
        for (; arg < argc; arg ++)
        {
            string a = argv [arg];
            if (a.compare (0, 2, "-l") == 0 || a.compare (0, 2, "-L") == 0 ||
                endsWith (a, ".o") || endsWith (a, ".a") || endsWith (a, ".so"))
                inputs.push_back (a);
            else
                sources.push_back (a);
        }
        if (!compile_only && sources.size () == 1 && inputs.empty ())
            return runFile (sources [0].c_str ());
        return runFiles (sources, inputs, compile_only);
    }

    int total_failed = 0;

    int first_gen = gen_given ? generator : 0;
    int last_gen  = gen_given ? generator : max_generator;
    for (generator = first_gen; generator <= last_gen; generator ++)
    {
        static const char * names [] = { "instr.cc", "instr1.cc", "instr2.cc", "instr3.cc", "instr4.cc", "instr5.cc" };
        cout << "=== " << names [generator] << endl;

        int passed = 0;
        int failed = 0;
        int skipped = 0;

        for (const TestCase & t : tests)
        {
            string status;
            bool ok;

            int g = generator == 0 ? 6 : generator;   /* instr.cc: instr5.cc + tridy */
            if (g < t.min_gen || g > t.max_gen)
            {
                skipped ++;
                continue;
            }

            if (setjmp (error_jump) == 0)
            {
                long result = compile (t.source);
                ok = !t.error && result == t.expected;
                status = "result " + to_string (result);
                if (!ok)
                    status += t.error ? ", expected error"
                                      : ", expected " + to_string (t.expected);
            }
            else
            {
                ok = t.error;
                status = "error: " + error_msg;
            }

            cout << (ok ? "  OK    " : "  FAIL  ") << t.name << "  (" << status << ")" << endl;
            if (ok) passed ++; else failed ++;
        }

        cout << passed << " passed, " << failed << " failed";
        if (skipped > 0) cout << ", " << skipped << " skipped (not supported by this generator)";
        cout << endl << endl;
        total_failed += failed;
    }

    return total_failed == 0 ? 0 : 1;
}
