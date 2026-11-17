/* instr_drv.c - cmini.bin: compiler c-mini (front-end decl, expr, stat, sem, lexer)
   with two modes (see usage):
     -C        => translation to C (translate.c): output -o file.c or stdout
     no -C     => generator instr.cc: assembler pasm (program, object, ToaruOS, ToaruOS libc)
   used by: tests/build-translate.sh, tests/02-cmini-asm.sh, tests/03-cmini-c.sh */

#include "decl.h"
#include "lexer.h"
#include "instr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* translate.c - input, lexer and translation to C (called from main) */
#ifdef __cplusplus
extern "C" {
#endif
int  TrStart (const char * src);
int  TrTranslate (const char * src, const char * out_name);
void TrIncludeDir (char * dir);
void TrDefine (char * arg);
#ifdef __cplusplus
}
#endif

static void Usage (FILE * f, int code)
{
  fprintf (f, "usage: cmini.bin -C [-I dir] [-iquote dir] [-D name[=value]] [-o out.c] source.cc\n");
  fprintf (f, "       cmini.bin [-v] [-I dir] [-iquote dir] [-D name[=value]] [-o prog]\n");
  fprintf (f, "                 [-c object.o] [-T] [-L dir] source.cc\n");
  fprintf (f, "\n");
  fprintf (f, "rezim -C: preklad c-mini do C (translate.c); bez -o text na stdout. Nelze s -v,\n");
  fprintf (f, "  -c, -T, -L. Vystup se prelozi gcc (cminirt.c jako behova knihovna).\n");
  fprintf (f, "rezim bez -C (vychozi): generator instr.cc, text pro assembler pasm, pasm z neho\n");
  fprintf (f, "  udela program (ELF64, samostatny - vlastni knihovna) nebo objekt.\n");
  fprintf (f, "  -o prog  program prog a text prog.asm; vypis prog.lst (s -v to jde i na stdout)\n");
  fprintf (f, "  -c x.o   objekt ELF64 (relokovatelny, pro gcc - glibc)\n");
  fprintf (f, "  -T       program pro ToaruOS (syscally ToaruOS, obraz od 0x3F000000)\n");
  fprintf (f, "  -L dir   program pro ToaruOS s libc ToaruOS (dir = /lib ToaruOS: crt0.o crti.o\n");
  fprintf (f, "           crtn.o libc.a), staticky linker pasm; vyzaduje -o\n");
  fprintf (f, "  -v       vypis assembleru (prog.lst) na stdout; bez -o/-c se program take spusti\n");
  fprintf (f, "  -S x.asm ulozi vygenerovany text pro assembler (i pri chybe assembleru)\n");
  fprintf (f, "  -o prog  s -o se text uklada take jako prog.asm\n");
  fprintf (f, "  bez -o, -c, -T, -L: program se spusti a vypise main returned N\n");
  fprintf (f, "prepinace: -I dir (hlavicky), -iquote dir (hlavicky programu), -D jmeno[=hodnota],\n");
  fprintf (f, "  -h (tato napoveda)\n");
  exit (code);
}

int main (int argc, char * argv [])
{
  int a = 1;
  int to_c = 0;                     /* -C: output in C (translate.c) */
  int verbose = 0;                  /* -v: print the assembler listing (pasm mode) */
  const char * out = NULL;          /* -o */

  for (; a < argc && argv [a][0] == '-'; a++)
  {
     if (strcmp (argv [a], "-h") == 0) Usage (stdout, 0);
     if (strcmp (argv [a], "-I") == 0 && a + 1 < argc) TrIncludeDir (argv [++ a]);
     else if (strncmp (argv [a], "-I", 2) == 0) TrIncludeDir (argv [a] + 2);
     else if (strcmp (argv [a], "-iquote") == 0 && a + 1 < argc) LexIncludeDir (argv [++ a]);
     else if (strcmp (argv [a], "-D") == 0 && a + 1 < argc) TrDefine (argv [++ a]);
     else if (strcmp (argv [a], "-C") == 0) to_c = 1;
     else if (strcmp (argv [a], "-v") == 0) verbose = 1;
     else if (strcmp (argv [a], "-o") == 0 && a + 1 < argc) out = argv [++ a];
     else if (strcmp (argv [a], "-S") == 0 && a + 1 < argc) InstrAsmFile = argv [++ a];
     else if (strcmp (argv [a], "-c") == 0 && a + 1 < argc) InstrObjectFile = argv [++ a];
     else if (strcmp (argv [a], "-T") == 0) InstrToaru = 1;
     else if (strcmp (argv [a], "-L") == 0 && a + 1 < argc) InstrToaruLib = argv [++ a];
     else { fprintf (stderr, "cmini.bin: unknown option %s\n", argv [a]); Usage (stderr, 2); }
  }
  if (a >= argc) Usage (stderr, 2);
  if (to_c)
  {
     if (InstrObjectFile != NULL || InstrToaru || InstrToaruLib != NULL || verbose || InstrAsmFile != NULL)
     {
        fprintf (stderr, "cmini.bin: -C nelze kombinovat s -v, -S, -c, -T, -L\n");
        return 2;
     }
     return TrTranslate (argv [a], out);
  }
  InstrOutputFile = out;

  if (TrStart (argv [a]) != 0) return 2;
  while (sy != EOS) GlobalDecl ();
  long r = runInstructions (verbose);
  if (InstrOutputFile == NULL && InstrObjectFile == NULL) printf ("main returned %ld\n", r);
  return 0;
}
