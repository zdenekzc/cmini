/* asmlist_test.c - tests/hello64.asm sestaveny pres asmlist.h (ciste C)

   Kod se generuje prvni, datovy segment se vlozi na zacatek az potom
   (AsmSetCursor) - jako u x86::Builder z AsmJit.  Vysledek (hello64_list.bin)
   musi byt bajtove shodny s  pasm -e hello64.asm  a skoncit kodem 35.
   AsmAssemble (faze 2, uzly primo) a AsmAssembleText (faze 1, text) musi dat
   stejny .bin i .lst (hello64_text.*) a stejne chybove hlaseni.

   preklad:  gcc -std=c99 -I .. asmlist_test.c ../asmlist.c ../libpasm.a -o asmlist_test
*/

#include "asmlist.h"
#include "pasmlib.h"

#include <string.h>

int main (void)
{
   AsmProgram p;
   AsmInit (&p);

   AsmLabel * msg   = AsmNamedLabel (&p, "msg");
   AsmLabel * ptab  = AsmNamedLabel (&p, "ptab");
   AsmLabel * count = AsmNamedLabel (&p, "count");
   AsmLabel * nums  = AsmNamedLabel (&p, "nums");
   AsmLabel * buf   = AsmNamedLabel (&p, "buf");
   AsmLabel * ok    = AsmNamedLabel (&p, "ok");
   AsmLabel * big   = AsmNamedLabel (&p, "big");
   AsmLabel * data  = AsmNamedLabel (&p, "data");
   AsmLabel * code  = AsmNamedLabel (&p, "code");
   AsmLabel * start = AsmNamedLabel (&p, "start");
   AsmLabel * next  = AsmNamedLabel (&p, "next");
   AsmLabel * bad   = AsmNamedLabel (&p, "bad");
   AsmLabel * write = AsmNamedLabel (&p, "write");
   AsmLabel * later = AsmNamedLabel (&p, "later");

   AsmDir (&p, ipX64, NULL, NULL);
   AsmNode * head = AsmCursor (&p);           /* sem se pozdeji vlozi data */

   /* ---- kod ---- */
   AsmDir (&p, iSegment, code, "use64");
   AsmBind (&p, start);
   AsmIns2 (&p, iLea, AsmReg (RSI), AsmMemLab (0, msg, 0));
   AsmIns2 (&p, iMov, AsmReg (EDX), AsmImm (13));
   AsmIns1 (&p, iCall, AsmLab (write));
   AsmIns2 (&p, iMov, AsmReg (RSI), AsmMemLab (8, ptab, 0));
   AsmIns2 (&p, iMov, AsmReg (EDX), AsmImm (3));
   AsmIns1 (&p, iCall, AsmLab (write));

   AsmIns2 (&p, iXor, AsmReg (R8D), AsmReg (R8D));
   AsmIns2 (&p, iXor, AsmReg (R9), AsmReg (R9));
   AsmIns2 (&p, iLea, AsmReg (R10), AsmMemLab (0, nums, 0));
   AsmBind (&p, next);
   AsmComment (AsmIns2 (&p, iMovsxd, AsmReg (RAX), AsmMemIdx (4, R10, R9, 4, 0)), "nums [r9]");
   AsmIns2 (&p, iAdd, AsmReg (R8), AsmReg (RAX));
   AsmIns1 (&p, iInc, AsmReg (R9));
   AsmIns2 (&p, iCmp, AsmReg (R9), AsmImm (4));
   AsmIns1 (&p, iJl, AsmLab (next));

   AsmIns2 (&p, iMov, AsmMemLab (4, count, 0), AsmImm (3));
   AsmIns2 (&p, iAdd, AsmMemLab (4, count, 0), AsmImm (4));
   AsmIns2 (&p, iCmp, AsmMemLab (1, later, 0), AsmImm ('L'));
   AsmIns1 (&p, iJne, AsmLab (bad));
   AsmIns2 (&p, iMov, AsmReg (EAX), AsmMemLab (4, count, 0));
   AsmIns2 (&p, iAdd, AsmReg (R8), AsmReg (RAX));

   AsmIns2 (&p, iMov, AsmReg (RAX), AsmReg (R8));
   AsmIns1 (&p, iNeg, AsmReg (RAX));
   AsmIns0 (&p, iCqo);
   AsmIns2 (&p, iMov, AsmReg (RCX), AsmImm (10));
   AsmIns1 (&p, iIdiv, AsmReg (RCX));
   AsmIns1 (&p, iPush, AsmReg (RAX));
   AsmIns1 (&p, iPop, AsmReg (R15));
   AsmIns1 (&p, iNeg, AsmReg (R15));
   AsmIns2 (&p, iAdd, AsmReg (R8), AsmReg (R15));

   AsmIns2 (&p, iLea, AsmReg (RSI), AsmMemLab (0, ok, 0));
   AsmIns2 (&p, iLea, AsmReg (RDI), AsmMemLab (0, buf, 0));
   AsmIns2 (&p, iMov, AsmReg (ECX), AsmImm (3));
   AsmPrefixed (&p, iRep, iMovsb);
   AsmIns2 (&p, iLea, AsmReg (RSI), AsmMemLab (0, buf, 0));
   AsmIns2 (&p, iMov, AsmReg (EDX), AsmImm (3));
   AsmIns1 (&p, iCall, AsmLab (write));

   AsmIns2 (&p, iMov, AsmReg (RAX), AsmImm (0x123456789ABCDEF0LL));
   AsmIns2 (&p, iCmp, AsmReg (RAX), AsmMemLab (8, big, 0));
   AsmIns1 (&p, iJne, AsmLab (bad));
   AsmIns2 (&p, iMov, AsmReg (RCX), AsmImm (0xFFFFFFFFLL));
   AsmIns1 (&p, iInc, AsmReg (RCX));
   AsmIns2 (&p, iShr, AsmReg (RCX), AsmImm (32));
   AsmIns2 (&p, iAdd, AsmReg (R8), AsmReg (RCX));
   AsmIns2 (&p, iMov, AsmReg (RDX), AsmImm (0x300000000LL));
   AsmIns2 (&p, iShr, AsmReg (RDX), AsmImm (32));
   AsmIns2 (&p, iAdd, AsmReg (R8), AsmReg (RDX));
   AsmIns2 (&p, iMov, AsmReg (RAX), AsmImm (0x3000000));
   AsmIns2 (&p, iShr, AsmReg (RAX), AsmImm (24));
   AsmIns2 (&p, iAdd, AsmReg (R8), AsmReg (RAX));

   AsmIns2 (&p, iMov, AsmReg (RDI), AsmReg (R8));
   AsmIns2 (&p, iSub, AsmReg (RDI), AsmImm (100));
   AsmIns2 (&p, iMov, AsmReg (EAX), AsmImm (60));
   AsmIns0 (&p, iSyscall);

   AsmBind (&p, bad);
   AsmIns2 (&p, iMov, AsmReg (EDI), AsmImm (1));
   AsmIns2 (&p, iMov, AsmReg (EAX), AsmImm (60));
   AsmIns0 (&p, iSyscall);

   AsmDir (&p, iProc, write, "near");
   AsmIns2 (&p, iMov, AsmReg (EAX), AsmImm (1));
   AsmIns2 (&p, iMov, AsmReg (EDI), AsmImm (1));
   AsmIns0 (&p, iSyscall);
   AsmIns0 (&p, iRet);
   AsmDir (&p, iEndp, write, NULL);

   AsmOp l [] = { AsmStr ("L") };
   AsmData (&p, later, iDb, 1, l);
   AsmDir (&p, iEnds, code, NULL);
   AsmDir (&p, iEnd, start, NULL);

   /* ---- data: vlozit za .x64 (kurzor na zacatek) ---- */
   AsmNode * save = AsmSetCursor (&p, head);
   AsmDir (&p, iSegment, data, "use64 para");
   AsmOp d1 [] = { AsmStr ("Hello, world"), AsmImm (10) };
   AsmData (&p, msg, iDb, 2, d1);
   AsmOp d2 [] = { AsmOffset (ok) };
   AsmData (&p, ptab, iDq, 1, d2);
   AsmOp d3 [] = { AsmImm (0) };
   AsmData (&p, count, iDd, 1, d3);
   AsmOp d4 [] = { AsmImm (5), AsmImm (-7), AsmImm (12), AsmImm (100) };
   AsmData (&p, nums, iDd, 4, d4);
   AsmOp d5 [] = { AsmDup (16, 0) };
   AsmData (&p, buf, iDb, 1, d5);
   AsmOp d6 [] = { AsmStr ("ok"), AsmImm (10) };
   AsmData (&p, ok, iDb, 2, d6);
   AsmOp d7 [] = { AsmImm (0x123456789ABCDEF0LL) };
   AsmData (&p, big, iDq, 1, d7);
   AsmDir (&p, iEnds, data, NULL);
   AsmSetCursor (&p, save);

   AsmPrint (&p, stdout);

   char m [512];
   if (AsmAssemble (&p, "hello64_list", PASM_EXE | PASM_LST, m, sizeof (m)) != 0)
   {
      printf ("%s\n", m);
      return 1;
   }
   if (AsmAssembleText (&p, "hello64_text", PASM_EXE | PASM_LST, m, sizeof (m)) != 0)
   {
      printf ("%s\n", m);
      return 1;
   }

   /* chyba v instrukci z uzlu (na konci):  mov 1, rax  => radka = poradi uzlu */
   AsmNode * e = AsmIns2 (&p, iMov, AsmImm (1), AsmReg (RAX));
   char m1 [512], m2 [512];
   int r1 = AsmAssemble (&p, "hello64_err", PASM_EXE, m1, sizeof (m1));
   int r2 = AsmAssembleText (&p, "hello64_err", PASM_EXE, m2, sizeof (m2));
   int line = 0;
   for (AsmNode * n = p.first; n != NULL; n = n->next)
   {
      line ++;
      if (n == e) break;
   }
   char want [64];
   snprintf (want, sizeof (want), "hello64_err.asm(%d,", line);
   if (r1 == 0 || r2 == 0 || strncmp (m1, want, strlen (want)) != 0 ||
       strchr (m1, ')') == NULL || strcmp (strchr (m1, ')'), strchr (m2, ')')) != 0)
   {
      fprintf (stderr, "chybove hlaseni:\n  uzly: %s\n  text: %s\n", m1, m2);
      return 1;
   }
   AsmRemove (&p, e);

   AsmFree (&p);
   return 0;
}
