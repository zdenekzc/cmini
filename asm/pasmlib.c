/* PASMLIB.CC - assembler pasm jako knihovna (pasmlib.h) */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>            /* getenv (PASM_TARGET) */

#include "key.h"
#include "def.h"
#include "code.h"
#include "expr.h"
#include "pasmlib.h"
#include "asmlist.h"

static void MakeName (char * result, const char * base, const char * ext)
{
   StrSet (result, 255 + 1, base);
   StrAdd (result, 255 + 1, ext);
}

static int Assemble (const char * src, const char * base, int flags, char * msg, int msg_size)
/* src = soubor (nebo jen jmeno pro hlaseni, kdyz je text v pameti) */
{
   char name [255 + 1];

   StopAllowed  = false;
   RetryAllowed = false;

   obj  = (flags & PASM_OBJ) != 0;
   elf  = (flags & PASM_EXE) != 0;
   lst  = (flags & PASM_LST) != 0;
   pas  = (flags & PASM_INL) != 0;
   eobj = (flags & PASM_ELFOBJ) != 0;
   bin  = false;

   /* Windows: PE (-w) je varianta spustitelneho ELF, COFF (-m) varianta
      objektu ELF64 - stejne kontroly a stejny druhy pruchod, jiny zapis
      (code.c).  Spolu s -e / -c nelze (sdileji vyrovnavaci pameti). */
   pe   = (flags & PASM_PE) != 0;
   cobj = (flags & PASM_COFF) != 0;
   if ((pe && elf) || (cobj && (eobj || obj)))
   {
      if (msg != NULL && msg_size > 0)
         snprintf (msg, msg_size, "%s(0,0) Error: %s", src,
                   pe && elf ? "Options -e and -w cannot be combined"
                             : "Option -m cannot be combined with -o or -c");
      return 1;
   }
   if (pe)   elf  = true;
   if (cobj) eobj = true;

   /* Cil obrazce: ToaruOS menuje zakladnu, ne format (stale ELF64 exec).
      Krome prepinace -T to jde rict i volajicimu, ktery flags jen prepisuje
      (cmini -> PasmAssembleList), proto i prostredi PASM_TARGET=toaru. */
   {
      const char * target = getenv ("PASM_TARGET");
      ToaruTarget = (flags & PASM_TOARU) != 0 ||
              (target != NULL && strcmp (target, "toaru") == 0);
      if (pe) ToaruTarget = false;     /* PE (-w): zakladna Windows, PASM_TARGET se neuplatni */
   }

   /* vystupni soubory:  base + pripona,  implicitne src bez pripony */
   if (base != NULL)
      StrSet (name, sizeof (name), base);
   else
   {
      StrSet (name, sizeof (name), src);
      char * slash = strrchr (name, '/');
      char * dot = strrchr (name, '.');
      if (dot != NULL && (slash == NULL || dot > slash)) *dot = 0;
   }

   StrSet (ModName, sizeof (ModName), src);
   MakeName (ObjName, name, ".obj");
   MakeName (ElfName, name, ".bin");
   MakeName (LstName, name, ".lst");
   MakeName (PasName, name, ".inl");
   MakeName (EobjName, name, ".o");
   MakeName (PeName, name, ".exe");
   MakeName (CobjName, name, ".obj");

   Compile ();

   if (ErrMsg [0] != 0)
   {
      if (msg != NULL && msg_size > 0)
         snprintf (msg, msg_size, "%s(%d,%d) Error: %s", ModName, (int) ErrLine, (int) ErrCol, ErrMsg);
      return 1;
   }
   if (msg != NULL && msg_size > 0) msg [0] = 0;
   return 0;
}

int PasmAssemble (const char * src, const char * base, int flags, char * msg, int msg_size)
{
   SetSourceText (NULL, 0);
   return Assemble (src, base, flags, msg, msg_size);
}

int PasmLink (const char * out, const char * const * inputs, int count, char * msg, int msg_size)
{
   return LinkToaru (out, inputs, count, msg, msg_size);
}

int PasmAssembleText (const char * name, const char * const * parts, int count,
                      const char * base, int flags, char * msg, int msg_size)
{
   static const char * empty = "";
   if (parts == NULL || count <= 0)
   {
      parts = &empty;
      count = 1;
   }
   SetSourceText (parts, count);
   int r = Assemble (name, base, flags, msg, msg_size);
   SetSourceText (NULL, 0);
   return r;
}

int PasmAssembleList (const char * name, const void * prog,
                      const char * base, int flags, char * msg, int msg_size)
{
   const AsmProgram * p = (const AsmProgram *) prog;
   SetSourceText (NULL, 0);
   SetSourceNodes (p != NULL ? p->first : NULL, true);
   int r = Assemble (name, base, flags, msg, msg_size);
   SetSourceNodes (NULL, false);
   return r;
}
