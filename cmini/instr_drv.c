/* instr_drv.c - cmini.bin: the c-mini compiler (front-end decl, expr, stat, sem,
   lexer) with the pasm/asmlist back end (instr.c).

   The back end always produces assembler text for pasm (program, object,
   ToaruOS, ToaruOS libc).  Translation to C is a separate program,
   cmini-translate.bin (translate_drv.c, translate.c). */

#include "decl.h"
#include "front.h"   /* TrStart, TrDefine (front.c) */
#include "instr.h"
#include "lexer.h"   /* LexIncludeDir */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void Usage (FILE * f, int code)
{
  fprintf (f, "usage: cmini.bin [-v] [-I dir] [-iquote dir] [-D name[=value]] [-o prog]\n");
  fprintf (f, "                 [-c object.o] [-T] [-L dir] source.cc\n");
  fprintf (f, "\n");
  fprintf (f, "The pasm/asmlist back end (instr.c) produces assembler text; pasm turns it\n");
  fprintf (f, "into a program (ELF64, standalone - own library) or an object.\n");
  fprintf (f, "  -o prog  program prog and assembler text prog.asm; listing prog.lst (-v: also stdout)\n");
  fprintf (f, "  -c x.o   ELF64 object (relocatable, for gcc - glibc)\n");
  fprintf (f, "  -T       program for ToaruOS (ToaruOS syscalls, image from 0x3F000000)\n");
  fprintf (f, "  -L dir   program for ToaruOS with the ToaruOS libc (dir = ToaruOS /lib: crt0.o\n");
  fprintf (f, "           crti.o crtn.o libc.a), static linker pasm; requires -o\n");
  fprintf (f, "  -v       print the assembler listing (prog.lst) to stdout; without -o/-c run the program\n");
  fprintf (f, "  -S x.asm save the generated assembler text (even when the assembler fails)\n");
  fprintf (f, "  without -o, -c, -T, -L: the program is run and prints main returned N\n");
  fprintf (f, "switches: -I dir (headers), -iquote dir (program headers), -D name[=value],\n");
  fprintf (f, "  -h (this help)\n");
  fprintf (f, "translation to C: use cmini-translate.bin [-C] ... (a separate program)\n");
  exit (code);
}

int main (int argc, char * argv [])
{
  int a = 1;
  int verbose = 0;                  /* -v: print the assembler listing */
  const char * out = NULL;          /* -o */

  for (; a < argc && argv [a][0] == '-'; a++)
  {
     if (strcmp (argv [a], "-h") == 0) Usage (stdout, 0);
     if (strcmp (argv [a], "-I") == 0 && a + 1 < argc) LexIncludeDir (argv [++ a]);
     else if (strncmp (argv [a], "-I", 2) == 0) LexIncludeDir (argv [a] + 2);
     else if (strcmp (argv [a], "-iquote") == 0 && a + 1 < argc) LexIncludeDir (argv [++ a]);
     else if (strcmp (argv [a], "-D") == 0 && a + 1 < argc) TrDefine (argv [++ a]);
     else if (strcmp (argv [a], "-v") == 0) verbose = 1;
     else if (strcmp (argv [a], "-o") == 0 && a + 1 < argc) out = argv [++ a];
     else if (strcmp (argv [a], "-S") == 0 && a + 1 < argc) InstrAsmFile = argv [++ a];
     else if (strcmp (argv [a], "-c") == 0 && a + 1 < argc) InstrObjectFile = argv [++ a];
     else if (strcmp (argv [a], "-T") == 0) InstrToaru = 1;
     else if (strcmp (argv [a], "-L") == 0 && a + 1 < argc) InstrToaruLib = argv [++ a];
     else if (strcmp (argv [a], "-C") == 0)
     {
        fprintf (stderr, "cmini.bin: -C is not available; use cmini-translate.bin\n");
        return 2;
     }
     else { fprintf (stderr, "cmini.bin: unknown option %s\n", argv [a]); Usage (stderr, 2); }
  }
  if (a >= argc) Usage (stderr, 2);
  InstrOutputFile = out;

  if (TrStart (argv [a]) != 0) return 2;
  while (sy != EOS) GlobalDecl ();
  long r = runInstructions (verbose);
  if (InstrOutputFile == NULL && InstrObjectFile == NULL) printf ("main returned %ld\n", r);
  return 0;
}
