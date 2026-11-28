/* translate_drv.c - cmini-translate.bin: translation of c-mini source to C.

   The front-end (decl, expr, stat, sem, lexer) builds the same tree as for the
   pasm back end; translate.c prints it as a C program (C99) that uses the
   runtime library cminirt.h / cminirt.c.

   -C is accepted for compatibility and has no effect (translation is the only
   mode).  The program is not part of "make all"; tests that need it build it
   with "make -C cmini cmini-translate.bin". */

#include "front.h"   /* TrStart, TrDefine (front.c) */
#include "lexer.h"   /* LexIncludeDir */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif
int  TrTranslate (const char * src, const char * out_name);
void TrIncludeDir (char * dir);
#ifdef __cplusplus
}
#endif

static void Usage (FILE * f, int code)
{
  fprintf (f, "usage: cmini-translate.bin [-C] [-I dir] [-iquote dir] [-D name[=value]]\n");
  fprintf (f, "                              [-o out.c] source.cc\n");
  fprintf (f, "\n");
  fprintf (f, "Translates c-mini to C (translate.c); without -o the text goes to stdout.\n");
  fprintf (f, "The output is compiled by gcc or tcc (cminirt.c is the runtime library).\n");
  fprintf (f, "  -C       accepted for compatibility, no effect\n");
  fprintf (f, "  -I dir   directory with replacement system headers (cmini/ownheaders)\n");
  fprintf (f, "  -iquote dir  directory of program headers (#include \"...\")\n");
  fprintf (f, "  -D name[=value]  macro definition\n");
  fprintf (f, "  -h       this help\n");
  exit (code);
}

int main (int argc, char * argv [])
{
  int a = 1;
  const char * out = NULL;          /* -o */

  for (; a < argc && argv [a][0] == '-'; a++)
  {
     if (strcmp (argv [a], "-h") == 0) Usage (stdout, 0);
     if (strcmp (argv [a], "-I") == 0 && a + 1 < argc) TrIncludeDir (argv [++ a]);
     else if (strncmp (argv [a], "-I", 2) == 0) TrIncludeDir (argv [a] + 2);
     else if (strcmp (argv [a], "-iquote") == 0 && a + 1 < argc) LexIncludeDir (argv [++ a]);
     else if (strcmp (argv [a], "-D") == 0 && a + 1 < argc) TrDefine (argv [++ a]);
     else if (strcmp (argv [a], "-C") == 0) { }
     else if (strcmp (argv [a], "-o") == 0 && a + 1 < argc) out = argv [++ a];
     else { fprintf (stderr, "cmini-translate.bin: unknown option %s\n", argv [a]); Usage (stderr, 2); }
  }
  if (a >= argc) Usage (stderr, 2);
  return TrTranslate (argv [a], out);
}
