/* PASM.CC */

/***************************************************************************/
/*                                                                         */
/*         PASM - i386 assembler from EASM (originally ASM.PAS)            */
/*         version for Linux - command line (translated from pasm.pas)     */
/*                                                                         */
/***************************************************************************/

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "key.h"
#include "def.h"
#include "expr.h"
#include "oth.h"
#include "inst.h"
#include "code.h"
#include "pasmlib.h"

static void Usage (void)
{
   printf ("Usage: pasm [-e] [-l] [-c] [-T] [-x] [-w] [-m] [-o] [-i] file[.asm]\n");
   printf ("  -e  Linux i386 executable (file.bin, ELF; USE32 segments, END start)\n"
           "      or x86-64 executable (ELF64; .X64, USE64 segments)\n");
   printf ("  -l  source listing (file.lst)\n");
   printf ("  -c  x86-64 ELF relocatable object (file.o, USE64 segments; link e.g. with gcc)\n");
   printf ("  -T  target ToaruOS (with -e): ELF64 based at 0x3F000000 instead of 0x400000;\n"
           "      may be set by environment PASM_TARGET=toaru\n");
   printf ("  -x  ToaruOS executable linked with ToaruOS libc (file.o and static file.bin,\n"
           "      USE64; PASM_LIBDIR = directory with crt0.o crti.o crtn.o libc.a from ToaruOS /lib)\n");
   printf ("  -w  Windows executable (file.exe, PE; USE32 => PE32 i386, USE64 => PE32+ x64);\n"
           "      EXTRN = import from DLL: EXTRN X: NEAR 'kernel32.dll!ExitProcess'\n");
   printf ("  -m  Windows x64 COFF object (file.obj, USE64 segments; link e.g. with link, lld-link)\n");
   printf ("  -o  object module  (file.obj, OMF)\n");
   printf ("  -i  Turbo Pascal inline (file.inl)\n");
   printf ("usage: pasm -k out.bin file.o ... lib.a ...\n"
           "  -k  only link (static ToaruOS linker): objects and archives in linker order,\n"
           "      e.g. /lib/crt0.o /lib/crti.o prog.o /lib/libc.a /lib/crtn.o\n");
   printf ("  default: -e\n");
   exit (2);
}

static void FSplit (const char * path, char * dir, char * name, char * ext)
/* like FSplit from the Dos unit: directory (including '/'), name, extension (including '.') */
{
   const char * slash = strrchr (path, '/');
   const char * base = (slash == NULL) ? path : slash + 1;
   const char * dot = strrchr (base, '.');

   int dlen = base - path;
   memcpy (dir, path, dlen);
   dir [dlen] = 0;

   if (dot == NULL) dot = base + strlen (base);
   int nlen = dot - base;
   memcpy (name, base, nlen);
   name [nlen] = 0;

   strcpy (ext, dot);
}

static void MakeName (char * result, const char * dir, const char * name, const char * ext)
{
   StrSet (result, 255 + 1, dir);
   StrAdd (result, 255 + 1, name);
   StrAdd (result, 255 + 1, ext);
}

/* assembler - pasmlib.c (PasmAssemble) */

int main (int argc, char * argv [])
{
   char SrcName [255 + 1];
   char dir [255 + 1];
   char name [255 + 1];
   char ext [255 + 1];
   char src [255 + 1];
   char base [255 + 1];
   char msg [512];
   bool any_ = false;
   bool link = false;          /* -x: object + PasmLink with the ToaruOS libc */
   int flags = 0;

   SrcName [0] = 0;

   /* pasm -k out.bin inputs... - only the static linker (PasmLink) */
   if (argc >= 2 && strcmp (argv [1], "-k") == 0)
   {
      if (argc < 4) Usage ();
      if (PasmLink (argv [2], (const char * const *) &argv [3], argc - 3, msg, sizeof (msg)) != 0)
      {
         printf ("%s\n", msg);
         exit (1);
      }
      return 0;
   }

   for (int n = 1; n < argc; n++)
   {
      const char * par = argv [n];
      if (strlen (par) == 2 && par [0] == '-')
      {
         switch (toupper ((unsigned char) par [1]))
         {
            case 'O': flags |= PASM_OBJ; break;
            case 'E': flags |= PASM_EXE; break;
            case 'L': flags |= PASM_LST; break;
            case 'I': flags |= PASM_INL; break;
            case 'C': flags |= PASM_ELFOBJ; break;
            case 'T': flags |= PASM_TOARU; break;
            case 'W': flags |= PASM_PE; break;
            case 'M': flags |= PASM_COFF; break;
            case 'X': flags |= PASM_ELFOBJ; link = true; break;
            default:  Usage (); break;
         }
         any_ = true;
      }
      else if (SrcName [0] == 0)
         StrSet (SrcName, sizeof (SrcName), par);
      else
         Usage ();
   }

   if (SrcName [0] == 0) Usage ();
   if (! any_)
      flags = PASM_EXE;
   /* -T can only be used with -e: it changes the base of the image written by -e.
      Neither the OMF object nor ET_REL (.o) has a base (the linker determines it). */
   else if (((flags & PASM_TOARU) != 0) && ((flags & PASM_EXE) == 0))
      Usage ();
   /* -x and -e both write file.bin */
   if (link && ((flags & PASM_EXE) != 0)) Usage ();
   /* -m and -o both write file.obj, -w with -e / -m with -c share the output memory */
   if ((((flags & PASM_COFF) != 0) && ((flags & (PASM_OBJ | PASM_ELFOBJ)) != 0)) ||
       (((flags & PASM_PE) != 0) && ((flags & PASM_EXE) != 0)))
      Usage ();

   FSplit (SrcName, dir, name, ext);
   if (ext [0] == 0) strcpy (ext, ".asm");

   MakeName (src, dir, name, ext);
   MakeName (base, dir, name, "");

   if (PasmAssemble (src, base, flags, msg, sizeof (msg)) != 0)
   {
      printf ("%s\n", msg);
      exit (1);
   }

   if (link)
   {
      /* statically with the ToaruOS libc:  crt0.o crti.o file.o libc.a crtn.o  => file.bin */
      const char * dir = getenv ("PASM_LIBDIR");
      char crt0 [255 + 1], crti [255 + 1], libc [255 + 1], crtn [255 + 1];
      char obj [255 + 1], out [255 + 1];
      const char * inputs [5];
      if (dir == NULL || dir [0] == 0)
      {
         printf ("PASM_LIBDIR not set (directory with crt0.o crti.o crtn.o libc.a from ToaruOS /lib)\n");
         exit (1);
      }
      MakeName (crt0, dir, "/crt0", ".o");
      MakeName (crti, dir, "/crti", ".o");
      MakeName (libc, dir, "/libc", ".a");
      MakeName (crtn, dir, "/crtn", ".o");
      MakeName (obj, base, "", ".o");
      MakeName (out, base, "", ".bin");
      inputs [0] = crt0;
      inputs [1] = crti;
      inputs [2] = obj;
      inputs [3] = libc;
      inputs [4] = crtn;
      if (PasmLink (out, inputs, 5, msg, sizeof (msg)) != 0)
      {
         printf ("%s\n", msg);
         exit (1);
      }
   }

   return 0;
}
