/* INST.CC */

/* instrukce (prevedeno z inst.pas) */

#include <stdio.h>
#include <string.h>
#include <setjmp.h>

#include "inst.h"
#include "expr.h"
#include "asmlist.h"

/******************************* DECLARATIONS *****************************/

static bool     Wait;                  /* Instruction Encoding */
static keyword  InstPref;
static bool     AddrPref;
static bool     DataPref;
static Register SegmPref;
static small    OpCode1;
static small    OpCode2;

static small    mode;                  /* 0..3, Expression Encoding */
static small    center;                /* 0..7 */
static small    mmm;                   /* 0..7 */

static bool     SIB;
static small    scl;                   /* 0..3 */
static small    iii;                   /* 0..7 */
static small    bbb;                   /* 0..7 */

static card     DispSize;

/* x86-64 */
static bool     RexW;                  /* REX.W - operand 64 bitu */
static bool     RexR;                  /* REX.R - rozsireni ModRM.reg */
static bool     RexX;                  /* REX.X - rozsireni SIB.index */
static bool     RexB;                  /* REX.B - rozsireni ModRM.rm, SIB.base, registru v opkodu */
static bool     RexNeed;               /* SPL, BPL, SIL, DIL - REX nutny */
static bool     RexHi;                 /* AH, CH, DH, BH - REX nelze pouzit */
static bool     RipRel;                /* adresa relativni k RIP */
static card     ImmAfter;              /* pocet bajtu prime hodnoty za adresou */

#define Esc 0xD8

enum FloatOp { fld, fstp, fst };
typedef enum FloatOp FloatOp;

/* RegSet - mnoziny registru jako funkce */

static bool InBpSet (Register r)
{
   return r == BP || r == EBP;
}

static bool InBase16 (Register r)
{
   return r == BX || r == BP || r == None;
}

static bool InIndex16 (Register r)
{
   return r == SI || r == DI || r == None;
}

static bool InBase32 (Register r)
{
   return r == EAX || r == EBX || r == ECX || r == EDX ||
          r == ESP || r == EBP || r == ESI || r == EDI || r == None;
}

static bool InIndex32 (Register r)
{
   return r == EAX || r == EBX || r == ECX || r == EDX ||
                      r == EBP || r == ESI || r == EDI || r == None;
}

static bool InReg32 (Register r)
{
   return r == EAX || r == EBX || r == ECX || r == EDX ||
          r == ESP || r == EBP || r == ESI || r == EDI;
}

/* registry pro adresu v 64bitovem rezimu */

static bool InAddr64 (Register r)
{
   return r >= RAX && r <= R15;
}

static bool InAddr32 (Register r)
{
   return (r >= EAX && r <= EDI) || (r >= R8D && r <= R15D);
}

static ExprRec a, b;
static card    w;

/************************** INSTRUCTION ENCODING **************************/

static void PrepInst (void)
{
   Wait     = false;
   InstPref = iNothing;
   AddrPref = false;
   DataPref = false;
   SegmPref = None;
   OpCode1  = 0;
   OpCode2  = 0;

   RexW     = false;
   RexR     = false;
   RexX     = false;
   RexB     = false;
   RexNeed  = false;
   RexHi    = false;
   RipRel   = false;
   ImmAfter = 0;
}

static void PutInst (void)
{
   if (Wait) Put (0x9B, '/');

   switch (InstPref)
   {
      case iNothing: break;
      case iLock:    Put (0xF0, '/'); break;
      case iRepne:
      case iRepnz:   Put (0xF2, '/'); break;
      case iRep:
      case iRepe:
      case iRepz:    Put (0xF3, '/'); break;
      default:       AsmError (Bug);
   }

   if (AddrPref) Put (0x67, '|');
   if (DataPref) Put (0x66, '|');

   switch (SegmPref)
   {
      case None: break;
      case ES:   Put (0x26, ':'); break;
      case CS:   Put (0x2E, ':'); break;
      case SS:   Put (0x36, ':'); break;
      case DS:   Put (0x3E, ':'); break;
      case FS:   Put (0x64, ':'); break;
      case GS:   Put (0x65, ':'); break;
      default:   AsmError (Bug);
   }

   if (RexW || RexR || RexX || RexB || RexNeed)
   {
      if (! x64) AsmError (Only64);
      if (RexHi) AsmError (RexHigh);
      Put (0x40 + 8 * RexW + 4 * RexR + 2 * RexX + RexB, '|');
   }

   if (OpCode1 != 0) Put (OpCode1, ' ');
   Put (OpCode2, ' ');
}

/**************************** OPERAND ENCODING ****************************/

static card seq (Register r)
/* cislo registru 0..7 (bit 3 se nastavuje do REX funkcemi seqR, seqX, seqB) */
{
   if (RegNew (r) && ! x64) AsmError (Only64);
   if (r >= SPL && r <= DIL) RexNeed = true;
   if (r >= AH && r <= BH) RexHi = true;
   return RegCode (r) & 7;
}

static card seqR (Register r)   /* ModRM.reg */
{
   if (RegCode (r) >= 8) RexR = true;
   return seq (r);
}

static card seqX (Register r)   /* SIB.index */
{
   if (RegCode (r) >= 8) RexX = true;
   return seq (r);
}

static card seqB (Register r)   /* ModRM.rm, SIB.base, registr v opkodu */
{
   if (RegCode (r) >= 8) RexB = true;
   return seq (r);
}

static card AddrDef (void)
/* implicitni velikost adresy */
{
   return x64 ? qword : default_;
}

static bool Big64 (ExprRec * e)
/* konstanta v segmentu USE64, ktera se nevejde do 32 bitu se znamenkem */
{
   return x64 && e->sort == imm && ! e->para &&
          e->target == NULL && e->add == NULL && e->frame == NULL &&
          (e->val64 < INT32_MIN || e->val64 > INT32_MAX);
}

static card ImmQ (card t, ExprRec * e)
/* velikost prime hodnoty - u operandu 64 bitu jen 32 bitu se znamenkovym
   rozsirenim (vyjimka: MOV r64, imm64) */
{
   if (t != qword) return t;
   if (Big64 (e)) AsmError (OutOfRange);
   return dword;
}

static void SelectSeg (ExprRec * e)
{
   bool    ok;
   int     rs;
   TreePtr ref;
   ListPtr list;

   if (x64)
   {
      /* v 64bitovem rezimu maji vyznam jen FS a GS */
      if (e->pref != FS && e->pref != GS) e->pref = None;
      SegmPref = e->pref;
      return;
   }

   if (e->pref == None && e->frame != NULL)
   {
      if (InBpSet (e->base) && e->frame == assume [SS - SegMin]) e->pref = SS;
      else if (e->frame == assume [DS - SegMin]) e->pref = DS;

      else
      {
         ok = false;
         rs = SegMin;
         while (! ok && rs <= SegMax)
         {
            if (assume [rs - SegMin] == e->frame)
            {
               e->pref = (Register) rs;
               ok = true;
            }
            rs ++;
         }

         rs = SegMin;
         while (! ok && rs <= SegMax)
         {
            ref = assume [rs - SegMin];
            if (ref != NULL && ref->ope.sort == group)
            {
               list = ref->ope.GrpSeq.first;
               while (! ok && list != NULL)
               {
                  if (list->data == e->frame)
                  {
                     e->pref = (Register) rs;
                     ok = true;
                  }
                  list = list->next;
               }
            }
            rs ++;
         }

         if (! ok) AsmError (AssumeErr);
      }
   }

   if (e->pref == SS && InBpSet (e->base)) e->pref = None;
   if (e->pref == DS && ! InBpSet (e->base)) e->pref = None;
   SegmPref = e->pref;
}

static void Addr16 (ExprRec * e)
{
   Register tem;

   if (e->scale != 1) AsmError (BadScale);

   if (e->base == SI || e->base == DI || e->index == BX || e->index == BP)
   {
      tem = e->base;
      e->base = e->index;
      e->index = tem;
   }

   if (! InIndex16 (e->index)) AsmError (BadIndex);
   if (! InBase16 (e->base)) AsmError (BadBase);

   if (default_ != word) AddrPref = true;
   SIB = false;

   DispSize = 2;
   if (e->add == NULL && e->target == NULL)
   {
      if (e->val == 0) DispSize = 0;
      else if (e->val >= -128 && e->val <= 127) DispSize = 1;
   }

   mode = DispSize;

   switch (e->base)
   {
      case BX:
         switch (e->index)
         {
            case SI:   mmm = 0; break;
            case DI:   mmm = 1; break;
            case None: mmm = 7; break;
            default:   break;
         }
         break;
      case BP:
         switch (e->index)
         {
            case SI:   mmm = 2; break;
            case DI:   mmm = 3; break;
            case None:
               mmm = 6;
               if (mode == 0)
               {
                  mode = 1;
                  DispSize = 1;
               }
               break;
            default:   break;
         }
         break;
      case None:
         switch (e->index)
         {
            case SI:   mmm = 4; break;
            case DI:   mmm = 5; break;
            case None:
               mmm = 6;
               mode = 0;
               DispSize = 2;
               break;
            default:   break;
         }
         break;
      default:
         break;
   }
}

static void Addr32 (ExprRec * e)
{
   Register tem;

   if ((e->base == None || e->index == ESP) && e->scale == 1)
   {
      tem = e->base;
      e->base = e->index;
      e->index = tem;
   }

   if (! InIndex32 (e->index)) AsmError (BadIndex);
   if (! InBase32 (e->base)) AsmError (BadBase);

   DispSize = 4;
   if (e->add == NULL && e->target == NULL)
   {
      if (e->val == 0) DispSize = 0;
      else if (e->val >= -128 && e->val <= 127) DispSize = 1;
   }

   if (default_ != dword) AddrPref = true;
   SIB = true;

   switch (DispSize)
   {
      case 4: mode = 2; break;
      case 1: mode = 1; break;
      case 0: mode = 0; break;
   }

   mmm = 4; /* ESP / SIB */

   switch (e->scale)
   {
      case 1:  scl = 0; break;
      case 2:  scl = 1; break;
      case 4:  scl = 2; break;
      case 8:  scl = 3; break;
      default: AsmError (Bug);
   }

   if (e->index == None) iii = 4; /* ESP / No index */
   else iii = seq (e->index);

   switch (e->base)
   {
      case None:
         bbb = 5; /* EBP / No base */
         mode = 0;
         DispSize = dword;
         break;
      case EBP:
         bbb = 5; /* EBP */
         if (mode == 0)
         {
            mode = 1;
            DispSize = byte;
         }
         break;
      default:
         bbb = seq (e->base);
   }

   if (e->index == None && e->base != ESP)
   {
      SIB = false;
      mmm = bbb;
   }
}

static void Addr64 (ExprRec * e)
/* adresa v 64bitovem rezimu: registry RAX .. R15, nebo EAX .. R15D s prefixem 67h,
   navesti bez registru - relativne k RIP, cislo bez registru - absolutne (SIB) */
{
   Register tem;
   bool     a32;

   if (e->base == None && e->index == None)
   {
      SIB = false;
      mode = 0;
      DispSize = dword;
      if (e->target != NULL || e->add != NULL)
      {
         RipRel = true;    /* [RIP + disp32] */
         mmm = 5;
      }
      else
      {
         mmm = 4;          /* SIB bez baze a indexu, disp32 */
         SIB = true;
         scl = 0;
         iii = 4;
         bbb = 5;
      }
      return;
   }

   if ((e->base == None || RegCode (e->index) == 4) && e->scale == 1)
   {
      tem = e->base;
      e->base = e->index;
      e->index = tem;
   }

   if (e->index != None && RegCode (e->index) == 4) AsmError (BadIndex);  /* RSP, ESP */

   if (e->target == NULL && e->add == NULL &&
       (e->val64 < INT32_MIN || e->val64 > INT32_MAX)) AsmError (OutOfRange);  /* disp32 */

   if (e->base != None)
      a32 = InAddr32 (e->base);
   else
      a32 = InAddr32 (e->index);

   if (e->base != None && ! (a32 ? InAddr32 (e->base) : InAddr64 (e->base))) AsmError (BadBase);
   if (e->index != None && ! (a32 ? InAddr32 (e->index) : InAddr64 (e->index))) AsmError (BadIndex);
   if (a32) AddrPref = true;

   DispSize = dword;
   if (e->add == NULL && e->target == NULL)
   {
      if (e->val == 0) DispSize = 0;
      else if (e->val >= -128 && e->val <= 127) DispSize = 1;
   }

   SIB = true;
   switch (DispSize)
   {
      case 4: mode = 2; break;
      case 1: mode = 1; break;
      case 0: mode = 0; break;
   }

   mmm = 4; /* SIB */

   switch (e->scale)
   {
      case 1:  scl = 0; break;
      case 2:  scl = 1; break;
      case 4:  scl = 2; break;
      case 8:  scl = 3; break;
      default: AsmError (Bug);
   }

   if (e->index == None) iii = 4; /* bez indexu */
   else iii = seqX (e->index);

   if (e->base == None)
   {
      bbb = 5; /* bez baze */
      mode = 0;
      DispSize = dword;
   }
   else
   {
      bbb = seqB (e->base);
      if (bbb == 5 && mode == 0) /* RBP, R13 */
      {
         mode = 1;
         DispSize = byte;
      }
   }

   if (e->index == None && bbb != 4) /* RSP, R12 vyzaduji SIB */
   {
      SIB = false;
      mmm = bbb;
   }
}

static void PrepAddr (ExprRec * e)
{
   card flag;

   switch (e->sort)
   {
      case reg:
         mode     = 3;
         mmm      = seqB (e->rx);
         SIB      = false;
         DispSize = 0;
         break;

      case st:
         mode     = 3;
         mmm      = e->ceil;
         SIB      = false;
         DispSize = 0;
         break;

      case xmm:                        /* XMM0 .. XMM15 v ModRM.rm */
         mode     = 3;
         mmm      = e->ceil & 7;
         if (e->ceil >= 8)
         {
            if (! x64) AsmError (Only64);
            RexB = true;
         }
         SIB      = false;
         DispSize = 0;
         break;

      case mem:
         if (x64)
         {
            if (e->index == None) e->scale = 1;
            Addr64 (e);
            SelectSeg (e);
            break;
         }

         flag = default_;
         if (e->target != NULL &&
             e->target->ope.sort == segment &&
             e->target->ope.Seg32) flag = dword;

         if (e->index == None) e->scale = 1;
         if (e->base != None && e->index != None &&
             (InReg32 (e->base) || InReg32 (e->index))) flag = dword;

         if (flag == word) Addr16 (e);
         else Addr32 (e);
         SelectSeg (e);
         break;

      default:
         break;
   }
}

static void PutAddr (ExprRec * e)
{
   Put (mode*64 + center*8 + mmm, '*');
   if (SIB) Put (scl*64 + iii*8 + bbb, '*');
   if (DispSize > 0)
   {
      if (RipRel) PutRel (e, DispSize, ImmAfter);
      else PutData (e, DispSize);
   }
}

/************************** INSTRUCTION FORMAT ****************************/

static void Choose (Intel p)
{
   if (p <= LastProc) proc = p;
   else coproc = p;
   if (p == i386 || p == i64 || p == i387) Any386 = true;
}

static void Check (Intel p)
{
   if (p <= LastProc)
   {
      if (p > proc) AsmError (InvProc);
   }
   else
   {
      if (p > coproc) AsmError (InvProc);
   }
}

static void Ins (Intel p, card dsize, card ic)
{
   if (dsize == qword)
   {
      if (! x64) AsmError (BadSize);
      RexW = true;
   }
   else if ((dsize == word || dsize == dword) && dsize != default_)
      DataPref = true;
   OpCode1 = (ic >> 8) & 0xFF;   /* hi (ic) */
   OpCode2 = ic & 0xFF;          /* lo (ic) */

   Check (p);
   if (DataPref || AddrPref) Check (i386);
   PutInst ();
}

static void InsI (Intel p, card d, card ic, ExprRec i, card w)
{
   Ins (p, d, ic);
   PutData (&i, w);
}

static void InsII (Intel p, card d, card ic, ExprRec i1, card w1,
                                             ExprRec i2, card w2)
{
   Ins (p, d, ic);
   PutData (&i1, w1);
   PutData (&i2, w2);
}

static void InsM (Intel p, card d, card ic, card ap, ExprRec m)
{
   PrepAddr (&m);
   Ins (p, d, ic);
   center = ap;
   PutAddr (&m);
}

static void InsRM (Intel p, card d, card ic, Register r, ExprRec m)
{
   InsM (p, d, ic, seqR (r), m);
}

static void InsMI (Intel p, card d, card ic, card ap, ExprRec m, ExprRec i, card w)
{
   ImmAfter = w;   /* pro adresu relativni k RIP */
   InsM (p, d, ic, ap, m);
   PutData (&i, w);
}

static void InsRMI (Intel p, card d, card ic, Register r, ExprRec m, ExprRec i, card w)
{
   ImmAfter = w;
   InsM (p, d, ic, seqR (r), m);
   PutData (&i, w);
}

static void InsNear (Intel p, card d, card ic, ExprRec m, card w, card a)
{
   if (a != AddrDef ())
   {
      if (x64 && a != dword) AsmError (Inv64);
      AddrPref = true;
   }
   Ins (p, d, ic);
   PutNear (&m, w);
}

static void InsFar (Intel p, card d, card ic, ExprRec m, card w)
{
   Ins (p, d, ic);
   PutFar (&m, w);
}

static void InsA (Intel p, card d, card ic, ExprRec m)
{
   PrepAddr (&m);
   Ins (p, d, ic);
   PutData (&m, default_);
}

static void InsB (Intel p, card d, card ic, ExprRec m1, ExprRec m2)
{
   (void) m1;
   PrepAddr (&m2);
   Ins (p, d, ic);
}

/**************************************************************************/

static bool acc (Register r)
{
   return r == AL || r == AX || r == EAX || r == RAX;
}

static bool tiny (ExprRec * e)
{
   return e->frame == NULL && e->target == NULL &&
          e->add == NULL && e->val >= -128 && e->val <= 127 &&
          ! Big64 (e);
}

static bool RelTiny (ExprRec * e)
{
   longint x;

   /* kratky skok (EB, 7x) ma 2 bajty, posunuti se pocita od jeho konce;
      puvodne ip + 1 => skok vzad o 129 bajtu hlasil "Jump too long" */
   x = (longint) ((uint32_t) e->val - ((uint32_t) ip + 2));
   /* navesti v aktualnim segmentu, ktere uz je definovane (add = nil);
      puvodne jen (frame = nil) and (target = nil), tj. nikdy pro navesti */
   return (e->target == NULL || e->target == ThisSeg) &&
          e->add == NULL && x >= -128 && x <= 127;
}

static void Separator (void)
{
   if (CurSy != Comma) AsmError (CommaExp);
   Insymbol ();
}

static void Binary (void)
{
   Expression (&a);
   Separator ();
   Expression (&b);

   if (a.typ == any && b.typ == any) AsmError (UndefSize);
   if (a.typ == any) a.typ = b.typ;
   if (b.typ == any) b.typ = a.typ;
   if (a.typ != b.typ) AsmError (BadSize);

   if (a.typ == byte) w = 0;
   else if (a.typ == word) w = 1;
   else if (a.typ == dword) w = 1;
   else if (a.typ == qword) w = 1;
   else AsmError (BadSize);
}

static void Unary (void)
{
   Expression (&a);
   if (a.sort != reg && a.sort != mem) AsmError (BadSort);
   if (a.typ == byte) w = 0;
   else if (a.typ == word) w = 1;
   else if (a.typ == dword) w = 1;
   else if (a.typ == qword) w = 1;
   else AsmError (BadSize);
}

static void RegOnly (ExprRec * e)
{
   Expression (e);
   if (e->sort != reg) AsmError (BadSort);
}

static void RegOrMem (ExprRec * e)
{
   Expression (e);
   if (e->sort != reg && e->sort != mem) AsmError (BadSort);
}

/*************************** Intel 8086 and 80186 ***************************/

static void Func (card ic, card ap)
{
   Unary ();
   InsM (i86, a.typ, ic + w, ap, a);
}

/* data transfer */

static void Move (void)
{
   const card MemReg = 0x88;   /* w */
   const card RegMem = 0x8A;   /* w */
   const card MemSeg = 0x8C;
   const card SegMem = 0x8E;
   const card RegImm = 0xB0;   /* 8*w */
   const card MemImm = 0xC6;   /* w */
   const card AccMem = 0xA0;   /* w */
   const card MemAcc = 0xA2;   /* w */
   const card RegCr  = 0x0F20;
   const card RegDr  = 0x0F21;
   const card RegTr  = 0x0F24;
   const card CrReg  = 0x0F22;
   const card DrReg  = 0x0F23;
   const card TrReg  = 0x0F26;

   Binary ();
   switch (a.sort)
   {
      case reg:
         switch (b.sort)
         {
            case reg:  InsRM (i86, a.typ, RegMem + w, a.rx, b); break;
            case mem:  if (acc (a.rx) && b.base == None && b.index == None && ! x64)
                          InsA  (i86, a.typ, AccMem + w, b);
                       else
                          InsRM (i86, a.typ, RegMem + w, a.rx, b);
                       break;

            case imm:  if (a.typ == qword && Big64 (&b))   /* MOV r64, imm64 */
                          InsI  (i86, a.typ, RegImm + 8*w + seqB (a.rx), b, qword);
                       else if (a.typ == qword)          /* MOV r64, imm32 se znamenkovym rozsirenim */
                          InsMI (i86, a.typ, MemImm + w, 0, a, b, dword);
                       else
                          InsI  (i86, a.typ, RegImm + 8*w + seqB (a.rx), b, a.typ);
                       break;
            case seg:  InsRM (i86, any, MemSeg, b.rx, a); break;
            case cr:   InsM  (i386, any, RegCr, b.ceil, a); break;
            case dr:   InsM  (i386, any, RegDr, b.ceil, a); break;
            case tr:   InsM  (i386, any, RegTr, b.ceil, a); break;
            default:   AsmError (BadSort);
         }
         break;

      case mem:
         switch (b.sort)
         {
            case reg:  if (acc (b.rx) && a.base == None && a.index == None && ! x64)
                          InsA  (i86, a.typ, MemAcc + w, a);
                       else
                          InsRM (i86, a.typ, MemReg + w, b.rx, a);
                       break;

            case imm:  InsMI (i86, a.typ, MemImm + w, 0, a, b, ImmQ (a.typ, &b)); break;
            case seg:  InsRM (i86, any, MemSeg, b.rx, a); break;
            default:   AsmError (BadSort);
         }
         break;

      case seg:
         switch (b.sort)
         {
            case reg:
            case mem:  InsRM (i86, any, SegMem, a.rx, b); break;
            default:   AsmError (BadSort);
         }
         break;

      case cr:
         switch (b.sort)
         {
            case reg:  InsM (i386, any, CrReg, a.ceil, b); break;
            default:   AsmError (BadSort);
         }
         break;

      case dr:
         switch (b.sort)
         {
            case reg:  InsM (i386, any, DrReg, a.ceil, b); break;
            default:   AsmError (BadSort);
         }
         break;

      case tr:
         switch (b.sort)
         {
            case reg:  InsM (i386, any, TrReg, a.ceil, b); break;
            default:   AsmError (BadSort);
         }
         break;

      default:
         AsmError (BadSort);
   }
}

static void Push64 (void)
/* PUSH v 64bitovem rezimu: operand 64 bitu (implicitne, bez REX.W) nebo 16 bitu */
{
   card d;

   if (a.sort == imm && a.typ == any) a.typ = qword;
   if (a.typ != word && a.typ != qword) AsmError (BadSize);
   if (a.typ == word) d = word; else d = any;

   switch (a.sort)
   {
      case reg: Ins  (i86, d, 0x50 + seqB (a.rx)); break;
      case mem: InsM (i86, d, 0xFF, 6, a); break;
      case imm: if (tiny (&a)) InsI (i186, d, 0x6A, a, byte);
                else InsI (i186, d, 0x68, a, ImmQ (a.typ, &a));
                break;
      case seg:
         switch (a.rx)
         {
            case FS: Ins (i386, any, 0x0FA0); break;
            case GS: Ins (i386, any, 0x0FA8); break;
            default: AsmError (Inv64);
         }
         break;
      default: AsmError (BadSort);
   }
}

static void Pop64 (void)
{
   card d;

   if (a.typ != word && a.typ != qword) AsmError (BadSize);
   if (a.typ == word) d = word; else d = any;

   switch (a.sort)
   {
      case reg: Ins  (i86, d, 0x58 + seqB (a.rx)); break;
      case mem: InsM (i86, d, 0x8F, 0, a); break;
      case seg:
         switch (a.rx)
         {
            case FS: Ins (i386, any, 0x0FA1); break;
            case GS: Ins (i386, any, 0x0FA9); break;
            default: AsmError (Inv64);
         }
         break;
      default: AsmError (BadSort);
   }
}

static void Push (void)
{
   Expression (&a);
   if (x64) { Push64 (); return; }
   if (a.sort == imm && a.typ == any) a.typ = default_;
   if (a.typ != word && a.typ != dword) AsmError (BadSize);
   switch (a.sort)
   {
      case reg: Ins  (i86, a.typ, 0x50 + seq (a.rx)); break;
      case mem: InsM (i86, a.typ, 0xFF, 6, a); break;
      case imm: if (tiny (&a)) InsI (i186, a.typ, 0x6A, a, byte);
                else InsI (i186, a.typ, 0x68, a, a.typ);
                break;
      case seg:
         switch (a.rx)
         {
            case ES: Ins (i86,  any, 0x06  ); break;
            case CS: Ins (i86,  any, 0x0E  ); break;
            case SS: Ins (i86,  any, 0x16  ); break;
            case DS: Ins (i86,  any, 0x1E  ); break;
            case FS: Ins (i386, any, 0x0FA0); break;
            case GS: Ins (i386, any, 0x0FA8); break;
            default: break;
         }
         break;
      default: AsmError (BadSort);
   }
}

static void Pop (void)
{
   Expression (&a);
   if (x64) { Pop64 (); return; }
   if (a.typ != word && a.typ != dword) AsmError (BadSize);
   switch (a.sort)
   {
      case reg: Ins  (i86, a.typ, 0x58 + seq (a.rx)); break;
      case mem: InsM (i86, a.typ, 0x8F, 0, a); break;
      case seg:
         switch (a.rx)
         {
            case ES: Ins (i86,  any, 0x07  ); break;
            case SS: Ins (i86,  any, 0x17  ); break;
            case DS: Ins (i86,  any, 0x1F  ); break;
            case FS: Ins (i386, any, 0x0FA1); break;
            case GS: Ins (i386, any, 0x0FA9); break;
            case CS: AsmError (BadPar);
            default: break;
         }
         break;
      default: AsmError (BadSort);
   }
}

static void Xchg (void)
{
   const card RegReg = 0x90;
   const card RegMem = 0x86; /* w */

   Binary ();
   switch (a.sort)
   {
      case reg:
         switch (b.sort)
         {
            case reg: if (w == 1 && (acc (a.rx) || acc (b.rx)))
                         Ins   (i86, a.typ, RegReg + seqB (a.rx) + seqB (b.rx));
                      else
                         InsRM (i86, a.typ, RegMem + w, a.rx, b);
                      break;
            case mem: InsRM (i86, a.typ, RegMem + w, a.rx, b); break;
            default:  AsmError (BadSort);
         }
         break;
      case mem:
         switch (b.sort)
         {
            case reg: InsRM (i86, a.typ, RegMem + w, b.rx, a); break;
            default:  AsmError (BadSort);
         }
         break;
      default: AsmError (BadSort);
   }
}

static void Load (Register rs)
{
   Expression (&a);
   if (a.sort != reg) AsmError (BadSort);
   if (a.typ != word && a.typ != dword) AsmError (BadSize);
   Separator ();
   Expression (&b);
   if (b.sort != mem) AsmError (BadSort);
   if (b.typ != a.typ + word && b.typ != any) AsmError (BadSize);

   switch (rs)
   {
      case ES: InsRM (i86,  a.typ, 0xC4,   a.rx, b); break;
      case DS: InsRM (i86,  a.typ, 0xC5,   a.rx, b); break;
      case SS: InsRM (i386, a.typ, 0x0FB2, a.rx, b); break;
      case FS: InsRM (i386, a.typ, 0x0FB4, a.rx, b); break;
      case GS: InsRM (i386, a.typ, 0x0FB5, a.rx, b); break;
      default: break;
   }
}

static void Lea (void)
{
   Expression (&a);
   if (a.sort != reg) AsmError (BadSort);
   if (a.typ != word && a.typ != dword && a.typ != qword) AsmError (BadSize);
   Separator ();
   Expression (&b);
   if (b.sort != mem) AsmError (BadSort);
   InsRM (i86, a.typ, 0x8D, a.rx, b);
}

/* arithmetic and logic */

static void Arit (card ap, bool opt)
{
   const card MemReg = 0x00; /* w + 8*ap */
   const card RegMem = 0x02; /* w + 8*ap */
   const card AccImm = 0x04; /* w + 8*ap */
   const card MemImm = 0x80; /* w */
   const card MemOpt = 0x83; /* w */
   card n;

   Binary ();
   n = 8 * ap + w;
   if (proc >= i386) opt = true;

   switch (a.sort)
   {
      case reg:
         switch (b.sort)
         {
            case reg:
            case mem: InsRM (i86, a.typ, RegMem + n, a.rx, b); break;
            case imm: if (acc (a.rx)) InsI (i86, a.typ, AccImm + n, b, ImmQ (a.typ, &b));
                      else if (w == 1 && opt && tiny (&b))
                         InsMI (i86, a.typ, MemOpt, ap, a, b, byte);
                      else
                         InsMI (i86, a.typ, MemImm + w, ap, a, b, ImmQ (a.typ, &b));
                      break;
            default:  AsmError (BadSort);
         }
         break;

      case mem:
         switch (b.sort)
         {
            case reg: InsRM (i86, a.typ, MemReg + n, b.rx, a); break;
            case imm: if (w == 1 && opt && tiny (&b))
                         InsMI (i86, a.typ, MemOpt, ap, a, b, byte);
                      else
                         InsMI (i86, a.typ, MemImm + w, ap, a, b, ImmQ (a.typ, &b));
                      break;
            default:  AsmError (BadSort);
         }
         break;

      default: AsmError (BadSort);
   }
}

static void Mul (void) /* IMUL */
{
   card    n;
   ExprRec c;

   Expression (&a);
   n = 1;
   if (CurSy == Comma)
   {
      Separator ();
      Expression (&b);
      n = 2;
      if (CurSy == Comma)
      {
         Separator ();
         Expression (&c);
         n = 3;
      }
   }

   if (n == 1)
   {
      if (a.sort != reg && a.sort != mem) AsmError (BadSort);
      if (a.typ == byte) w = 0;
      else if (a.typ == word) w = 1;
      else if (a.typ == dword) w = 1;
      else if (a.typ == qword) w = 1;
      else AsmError (BadSize);
      InsM (i86, a.typ, 0xF6 + w, 5, a);
   }
   else
   {
      if (a.sort != reg) AsmError (BadSort);
      if (a.typ != word && a.typ != dword && a.typ != qword) AsmError (BadSize);

      if (b.typ == any) b.typ = a.typ;
      if (a.typ != b.typ) AsmError (BadSize);

      if (n == 2)
         switch (b.sort)
         {
            case reg:
            case mem: InsRM (i386, a.typ, 0x0FAF, a.rx, b); break;
            case imm: if (tiny (&b))
                         InsRMI (i186, a.typ, 0x6B, a.rx, a, b, byte);
                      else
                         InsRMI (i186, a.typ, 0x69, a.rx, a, b, ImmQ (a.typ, &b));
                      break;
            default:  break;
         }
      else
      {
         if (b.sort != reg && b.sort != mem) AsmError (BadSort);
         if (c.sort != imm) AsmError (BadSort);
         if (tiny (&c)) InsRMI (i186, a.typ, 0x6B, a.rx, b, c, byte);
         else InsRMI (i186, a.typ, 0x69, a.rx, b, c, ImmQ (a.typ, &c));
      }
   }
}

static void Incr (bool sel)
{
   card ap,
        IncReg;

   Unary ();
   if (sel) { ap = 0; IncReg = 0x40; }  /* inc */
   else     { ap = 1; IncReg = 0x48; }  /* dec */

   switch (a.sort)
   {
      case reg: if (w == 0 || x64) InsM (i86, a.typ, 0xFE + w, ap, a);  /* 40h..4Fh jsou v x86-64 REX */
                else Ins (i86, a.typ, IncReg + seq (a.rx));
                break;
      case mem: InsM (i86, a.typ, 0xFE + w, ap, a); break;  /* puvodne neinicializovane IncMem */
      default:  AsmError (BadSort);
   }
}

static void Rot (card ap)
{
   const card Rot1   = 0xD0; /* w */
   const card RotCl  = 0xD2; /* w */
   const card RotImm = 0xC0; /* w */

   Unary ();
   Separator ();
   Expression (&b);

   switch (b.sort)
   {
      case reg: if (b.rx == CL) InsM (i86, a.typ, RotCl + w, ap, a);
                else AsmError (BadPar);
                break;
      case imm: if (tiny (&b) && b.val == 1)
                   InsM  (i86,  a.typ, Rot1 + w, ap, a);
                else
                   InsMI (i186, a.typ, RotImm + w, ap, a, b, byte);
                break;
      default:  AsmError (BadSort);
   }
}

static void Test (void)
{
   const card RegMem = 0x84; /* w */
   const card AnyImm = 0xF6; /* w */
   const card AccImm = 0xA8; /* w */

   Binary ();
   switch (a.sort)
   {
      case reg:
         switch (b.sort)
         {
            case reg:
            case mem: InsRM (i86, a.typ, RegMem + w, a.rx, b); break;
            case imm: if (acc (a.rx))
                         InsI  (i86, a.typ, AccImm + w, b, ImmQ (a.typ, &b));
                      else
                         InsMI (i86, a.typ, AnyImm + w, 0, a, b, ImmQ (a.typ, &b));
                      break;
            default:  AsmError (BadSort);
         }
         break;
      case mem:
         switch (b.sort)
         {
            case reg: InsRM (i86, a.typ, RegMem + w, b.rx, a); break;
            case imm: InsMI (i86, a.typ, AnyImm + w, 0, a, b, ImmQ (a.typ, &b)); break;
            default:  AsmError (BadSort);
         }
         break;
      default: AsmError (BadSort);
   }
}

/* string operations */

static void Block (Intel pro, card ic, card des, card sou)
   /* des = 0 - no destination
            1 - es:[di]
            2 - dx
      sou = 0 - no source
            1 - ds:[si]
            2 - dx             */
{
   ExprRec e;

   (void) sou;   /* v Pascalu se sou nepouziva (druhy case testuje des) */

   a = zero;
   switch (des)
   {
      case 1:
         Expression (&a);
         if (a.sort != reg && a.sort != mem) AsmError (BadSort);
         break;
      case 2:
         Expression (&e);
         if (e.sort != reg || e.rx != DX) AsmError (BadPar);
         break;
   }

   b = zero;
   switch (des)
   {
      case 1:
         if (des != 0) Separator ();
         Expression (&b);
         if (b.sort != reg && b.sort != mem) AsmError (BadSort);
         break;
      case 2:
         Expression (&e);
         if (e.sort != reg || e.rx != DX) AsmError (BadPar);
         break;
   }

   if (a.typ == any && b.typ == any) AsmError (UndefSize);
   if (a.typ == any) a.typ = b.typ;
   if (b.typ == any) b.typ = a.typ;
   if (a.typ != b.typ) AsmError (BadSize);

   if (a.typ == byte) InsB (pro, byte, ic, a, b);
   else if (a.typ == word)  InsB (pro, word,  ic + 1, a, b);
   else if (a.typ == dword) InsB (pro, dword, ic + 1, a, b);
   else if (a.typ == qword) InsB (pro, qword, ic + 1, a, b);
   else AsmError (BadSize);
}

/* input and output */

static void Accum (ExprRec * e)
{
   Expression (e);
   if (e->sort != reg) AsmError (BadSort);
   if (! acc (e->rx) || e->typ == qword) AsmError (BadPar);
   if (e->typ == byte) w = 0;
   else w = 1;
}

static void In86 (void)
{
   Accum (&a);
   Separator ();
   Expression (&b);

   switch (b.sort)
   {
      case reg: if (b.rx == DX) Ins (i86, a.typ, 0xEC + w);
                else AsmError (BadPar);
                break;
      case imm: InsI (i86, a.typ, 0xE4 + w, b, byte); break;
      default:  AsmError (BadSort);
   }
}

static void Out86 (void)
{
   Expression (&a);
   Separator ();
   Accum (&b);

   switch (a.sort)
   {
      case reg: if (a.rx == DX) Ins (i86, b.typ, 0xEE + w);
                else AsmError (BadPar);
                break;
      case imm: InsI (i86, b.typ, 0xE6 + w, a, byte); break;
      default:  AsmError (BadSort);
   }
}

static void Escape (void) /* ! */
{
   Expression (&a);
   if (a.sort != imm) AsmError (BadSort);
   if (a.val < 0 || a.val > 63) AsmError (BadPar);
   Separator ();
   Expression (&b);
   if (b.sort != st && b.sort != mem) AsmError (BadSort);
   InsM (i87, any, 0xD8 + a.val / 8, a.val % 8, b);
}

/* control transfer */

static void Jc (card n)
{
   Expression (&a);
   if (a.sort != mem) AsmError (BadSort);
   if (a.typ != near_ && a.typ != short_) AsmError (BadSize);
   if (proc < i386 || RelTiny (&a))
      InsNear (i86,  byte,     0x70 + n,   a, byte,     AddrDef ());
   else
      InsNear (i386, default_, 0x0F80 + n, a, default_, AddrDef ());
}

static void Loop (card ic, card address)
{
   Expression (&a);
   if (a.sort != mem) AsmError (BadSort);
   if (a.typ != near_) AsmError (BadSize);
   InsNear (i86, byte, ic, a, byte, address);
}

static void Jump (bool jmp)
{
   card icn, apn, icf, apf;

   if (jmp)
   {
      icn = 0xE9;
      apn = 4;
      icf = 0xEA;
      apf = 5;
   }
   else
   {
      icn = 0xE8;
      apn = 2;
      icf = 0x9A;
      apf = 3;
   }

   RegOrMem (&a);

   if (x64)
      switch (a.typ)
      {
         case word:
         case dword:
         case far_:   AsmError (Inv64);
         case qword:  InsM (i86, any, 0xFF, apn, a); return;   /* neprimy skok, operand 64 bitu */
         default:     break;
      }

   switch (a.typ)
   {
      case word:   InsM (i86, word, 0xFF, apn, a); break;
      case dword:
         switch (default_)
         {
            case word:  InsM (i86,  word,  0xFF, apf, a); break;
            case dword: InsM (i386, dword, 0xFF, apn, a); break;
            default:    break;
         }
         break;
      case fword:  InsM (i386, dword, 0xFF, apf, a); break;

      case short_: if (jmp) InsNear (i86, byte, 0xEB, a, byte, AddrDef ());
                   else InsNear (i86, default_, icn, a, default_, AddrDef ());
                   break;
      case near_:  if (jmp && RelTiny (&a))
                      InsNear (i86, byte, 0xEB, a, byte, AddrDef ());
                   else
                      InsNear (i86, default_, icn, a, default_, AddrDef ());
                   break;
      case far_:   InsFar (i86, default_, icf, a, default_); break;

      default:     AsmError (BadSize);
   }
}

static void Ret (card ic)
{
   if (CurSy == EOL)
      Ins (i86, default_, ic + 1);
   else
   {
      Expression (&a);
      if (a.sort != imm) AsmError (BadSort);
      InsI (i86, default_, ic, a, word);
   }
}

static void Enter (void)
{
   Expression (&a);
   Separator ();
   Expression (&b);
   if (a.sort != imm || b.sort != imm) AsmError (BadSort);
   InsII (i186, default_, 0xC8, a, word, b, byte);
}

static void Bound (void)
{
   Expression (&a);
   if (a.sort != reg) AsmError (BadSort);
   if (a.typ != word && a.typ != dword) AsmError (BadSize);
   Separator ();
   Expression (&b);
   if (b.sort != mem) AsmError (BadSort);
   if (b.typ != any && b.typ != 2 * a.typ) AsmError (BadSize);
   InsRM (i186, a.typ, 0x62, a.rx, b);
}

static void Int86 (void)
{
   Expression (&a);
   if (a.sort != imm) AsmError (BadSort);
   if (a.val == 3) Ins (i86, any, 0xCC);
   else InsI (i86, any, 0xCD, a, byte);
}

static void XlatB (void)
{
   a = zero;
   Expression (&b);
   if (b.sort != mem) AsmError (BadSort);
   if (b.typ != any && b.typ != byte) AsmError (BadSize);
   InsB (i86, byte, 0xD7, a, b);
}

/****************************** Intel 80286 *******************************/

static void Selector (ExprRec * e)
{
   Expression (e);
   if (e->sort != reg && e->sort != mem) AsmError (BadSort);
   if (e->typ != any && e->typ != word) AsmError (BadSize);
}

static void Arpl (void)
{
   Selector (&a);
   Separator ();
   RegOnly (&b);
   if (b.typ != word) AsmError (BadSize);
   InsRM (i286, any, 0x63, b.rx, a);
}

static void Lar (void)
{
   RegOnly (&a);
   if (a.typ != word && a.typ != dword) AsmError (BadSize);
   Separator ();
   Expression (&b);
   if (b.sort != reg && b.sort != mem) AsmError (BadSort);
   if (b.typ != any && b.typ != a.typ) AsmError (BadSize);
   InsRM (i286, a.typ, 0x0F02, a.rx, b);
}

static void Lsl (void)
{
   RegOnly (&a);
   if (a.typ != word && a.typ != dword) AsmError (BadSize);
   Separator ();
   Expression (&b);
   if (b.sort != reg && b.sort != mem) AsmError (BadSort);
   if (b.typ != any && b.typ != a.typ) AsmError (BadSize);
   InsRM (i286, a.typ, 0x0F03, a.rx, b);
}

static void Lxx (card ic, card ap, card width)
{
   Expression (&a);
   if (a.sort != reg && a.sort != mem) AsmError (BadSort);
   if (a.typ != any && a.typ != width) AsmError (BadSize);
   InsM (i286, any, 0x0F00 + ic, ap, a);
}

/****************************** Intel 80386 *******************************/

static void Bsf (bool sel)
{
   card ic;

   if (sel) ic = 0x0FBC;  /* bsf */
   else     ic = 0x0FBD;  /* bsr */
   Binary ();
   if (a.sort != reg) AsmError (BadSort);
   if (b.sort != reg && b.sort != mem) AsmError (BadSort);
   if (a.typ == byte) AsmError (BadSize);
   InsRM (i386, a.typ, ic, a.rx, b);
}

static void Bt (card ic, card ap)
{
   Binary ();
   if (a.sort != reg && a.sort != mem) AsmError (BadSort);
   if (a.typ == byte) AsmError (BadSize);
   switch (b.sort)
   {
      case reg: InsRM (i386, a.typ, 0x0F00 + ic, b.rx, a); break;
      case imm: InsMI (i386, a.typ, 0x0FBA, ap, a, b, byte); break;
      default:  AsmError (BadSize);
   }
}

static void Movsx (bool sel)
{
   card ic;

   if (sel) ic = 0x0FBE;  /* movsx */
   else     ic = 0x0FB6;  /* movsz */
   RegOnly (&a);
   Separator ();
   RegOrMem (&b);
   switch (a.typ)
   {
      case word:
         switch (b.typ)
         {
            case byte: InsRM (i386, word, ic, a.rx, b); break;
            default:   AsmError (BadSize);
         }
         break;
      case dword:
         switch (b.typ)
         {
            case byte: InsRM (i386, dword, ic,     a.rx, b); break;
            case word: InsRM (i386, dword, ic + 1, a.rx, b); break;
            default:   AsmError (BadSize);
         }
         break;
      case qword:
         switch (b.typ)
         {
            case byte: InsRM (i386, qword, ic,     a.rx, b); break;
            case word: InsRM (i386, qword, ic + 1, a.rx, b); break;
            default:   AsmError (BadSize);   /* dword - MOVSXD */
         }
         break;
      default:
         AsmError (BadSize);
   }
}

static void Setcc (card ic)
{
   RegOrMem (&a);
   if (a.typ != any && a.typ != byte) AsmError (BadSize);
   InsM (i386, byte, 0x0F00 + ic, 0, a);
}

static void Shld (bool sel)
{
   card    ic;
   ExprRec c;

   if (sel) ic = 0x0FA4;  /* shld */
   else     ic = 0x0FAC;  /* shrd */
   Binary ();
   Separator ();
   Expression (&c);
   if (a.sort != reg && a.sort != mem) AsmError (BadSort);
   if (b.sort != reg) AsmError (BadSort);
   if (a.typ == byte) AsmError (BadSize);
   switch (c.sort)
   {
      case reg: if (c.rx == CL) InsRM (i386, a.typ, ic + 1, b.rx, a);
                else AsmError (BadPar);
                break;
      case imm: InsRMI (i386, a.typ, ic, b.rx, a, c, byte); break;
      default:  AsmError (BadSort);
   }
}

/******************************* intel 8087 *******************************/

static void Sim (card op, card group, card order, bool w)
{
   if (w) Wait = true;
   Ins (i87, any, Esc + op);
   Put (0xC0 + group*8 + order, ' ');
}

static void Fce (card group, card order)
{
   Sim (1, group, order, true);
}

static void Normal (card n1, card n2, ExprRec e)
{
   InsM (i87, any, Esc + n1, n2, e);
}

static void Ope (card op1, card op2, card width, bool w)
{
   if (w) Wait = true;
   Expression (&a);
   if (a.sort != mem) AsmError (BadSort);
   if (width != any &&
       a.typ != any && a.typ != width) AsmError (BadSize);
   Normal (op1, op2, a);
}

  /* memory  ->  Esc + MF0

  MF : 00 for dword real                      M = 0  ... dword
       01     dword integer                   F = 0 ... real
       10     qword real
       11      word integer

  register  -> Esc + DP0

  DP : 00  for  st(0) := st(0) * st(i)
       01                                     P = 1 ... pop
       10  for  st(i) := st(0) * st(i)
       11  for  st(i) := st(0) * st(i) and pop                  */

static void Bin (card op1, card op2)
     /* op1 ... reverse
        op2 ... normal  */
{
   Wait = true;
   if (CurSy == EOL) Normal (6, op1, st1);              /* no parameters */
   else
   {
      Expression (&a);
      if (CurSy == EOL)                                  /* one parameter */
      {
         if (a.sort != mem) AsmError (BadSort);
         if (a.typ == dword) Normal (0, op2, a);
         else if (a.typ == qword) Normal (4, op2, a);
         else AsmError (BadSize);
      }
      else                                            /* two parameters */
      {
         Separator ();
         Expression (&b);
         if (a.sort != st || b.sort != st) AsmError (BadSort);
         if (a.ceil == 0) Normal (0, op2, b);         /* 0 := 0 * i */
         else if (b.ceil == 0) Normal (4, op1, a);    /* i := 0 * i */
         else AsmError (BadPar);
      }
   }
}

static void BinP (card op)
{
   Wait = true;
   Expression (&a);
   if (a.sort != st) AsmError (BadSort);
   if (CurSy != EOL)
   {
      Separator ();
      Expression (&b);
      if (b.sort != st) AsmError (BadSort);
      if (b.ceil != 0) AsmError (BadPar);
   }
   Normal (6, op, a);
}

static void BinI (card op)
{
   Wait = true;
   Expression (&a);
   if (a.sort != mem) AsmError (BadSort);
   if (a.typ == word) Normal (6, op, a);
   else if (a.typ == dword) Normal (2, op, a);
   else AsmError (BadSize);
}

static void Comp (card op)
{
   Wait = true;
   if (CurSy == EOL) Normal (0, op, st1);
   else
   {
      Expression (&a);
      switch (a.sort)
      {
         case st:  Normal (0, op, a); break;
         case mem: if (a.typ == dword) Normal (0, op, a);
                   else if (a.typ == qword) Normal (4, op, a);
                   else AsmError (BadSize);
                   break;
         case imm:
         case reg:
         case seg: AsmError (BadSort);
         default:  break;
      }
   }
}

/* Pascal: Float (koliduje s Def.Float - symbol) */
static void Float_ (FloatOp mode)
{
   card icr = 0, ap = 0, apt = 0;   /* apt pro fst nepouzito */

   switch (mode)
   {
      case fld:  icr = 1; ap = 0; apt = 5; break;    /* D9 /0,  D9 C0+i */
      case fstp: icr = 5; ap = 3; apt = 7; break;    /* D9 /3,  DD D8+i */
      case fst:  icr = 5; ap = 2;          break;    /* D9 /2,  DD D0+i */
   }

   Wait = true;
   Expression (&a);
   switch (a.sort)
   {
      case st:  Normal (icr, ap, a); break;
      case mem:
         if (a.typ == dword) Normal (1, ap, a);
         else if (a.typ == qword) Normal (5, ap, a);
         else if (a.typ == tbyte)
         {
            if (mode != fst) Normal (3, apt, a);
            else AsmError (BadSize);
         }
         else AsmError (BadSize);
         break;
      case imm:
      case reg:
      case seg: AsmError (BadSort);
      default:  break;
   }
}

static void Whole (FloatOp mode)
{
   card ap = 0, apq = 0;   /* apq pro fst nepouzito */

   switch (mode)
   {
      case fld:  ap = 0; apq = 5; break;
      case fstp: ap = 3; apq = 7; break;
      case fst:  ap = 2;          break;    /* DF /2, DB /2 */
   }

   Wait = true;
   Expression (&a);
   if (a.sort != mem) AsmError (BadSort);
   if (a.typ == word) Normal (7, ap, a);
   else if (a.typ == dword) Normal (3, ap, a);
   else if (a.typ == qword)
   {
      if (mode != fst) Normal (7, apq, a);
      else AsmError (BadSize);
   }
   else AsmError (BadSize);
}

static void Fstsw (bool w)
/* FSTSW / FNSTSW - pametovy operand (DD /7) nebo AX (DF E0, od 80287) */
{
   if (w) Wait = true;
   Expression (&a);
   if (a.sort == reg && a.rx == AX)
   {
      Ins (i287, any, Esc + 7);
      Put (0xE0, ' ');
   }
   else
   {
      if (a.sort != mem) AsmError (BadSort);
      Normal (5, 7, a);
   }
}

static void Ffree (void)
{
   Wait = true;
   Expression (&a);
   if (a.sort != st) AsmError (BadSort);
   Normal (5, 0, a);
}

static void Fxch (card n1, card n2)
{
   Wait = true;
   if (CurSy == EOL) Normal (n1, n2, st1);
   else
   {
      Expression (&a);
      if (a.sort != st) AsmError (BadSort);
      Normal (n1, n2, a);
   }
}

/********************************* x86-64 ***********************************/

static void Need64 (void)
{
   if (! x64) AsmError (Only64);
}

static void SendQ (card ic)
/* instrukce s operandem 64 bitu (REX.W) */
{
   Need64 ();
   Ins (i64, qword, ic);
}

static void Movsxd (void)
/* MOVSXD r64, r/m32 */
{
   Need64 ();
   RegOnly (&a);
   Separator ();
   RegOrMem (&b);
   if (a.typ != qword) AsmError (BadSize);
   if (b.typ != dword && b.typ != any) AsmError (BadSize);
   InsRM (i64, qword, 0x63, a.rx, b);
}

/******************************** SSE, SSE2 *********************************/

/* Skalarni operace s plovouci carkou v registrech XMM:
      prefix (F2 - double, F3 - float, 66 - pd / celociselne) pred REX,
      0F xx, ModRM (reg = XMM nebo obecny registr, rm = XMM, registr, pamet) */

static card xmmR (ExprRec * e)
/* registr XMM v ModRM.reg */
{
   if (e->ceil >= 8)
   {
      if (! x64) AsmError (Only64);
      RexR = true;
   }
   return e->ceil & 7;
}

static void SsePrefix (small pref)
{
   if (pref == 0xF2) InstPref = iRepne;
   else if (pref == 0xF3) InstPref = iRep;
   else if (pref == 0x66) DataPref = true;
}

static void InsX (small pref, card ic, card cent, ExprRec m, bool w64)
/* instrukce SSE: center = ModRM.reg (0..7, REX.R uz je nastaven), m = ModRM.rm */
{
   SsePrefix (pref);
   PrepAddr (&m);
   Ins (i386, w64 ? qword : any, ic);
   center = cent;
   PutAddr (&m);
}

static void XmmOnly (ExprRec * e)
{
   Expression (e);
   if (e->sort != xmm) AsmError (BadSort);
}

static void CheckMemSize (ExprRec * e, card size)
/* pamet: velikost podle instrukce (bez PTR je take spravne) */
{
   /* size == any: 16 bajtu (ANDPD, MOVAPD, ...) - XMMWORD neni, velikost se nekontroluje */
   if (e->sort == mem && size != any && e->typ != any && e->typ != size) AsmError (BadSize);
}

static void XmmOrMem (ExprRec * e, card size)
{
   Expression (e);
   if (e->sort != xmm && e->sort != mem) AsmError (BadSort);
   CheckMemSize (e, size);
}

static void SseArith (small pref, small op, card size)
/* ADDSD xmm, xmm/m64 ... (size = velikost pametoveho operandu, any = 16 bajtu) */
{
   XmmOnly (&a);
   Separator ();
   XmmOrMem (&b, size);
   InsX (pref, 0x0F00 | op, xmmR (&a), b, false);
}

static void SseMove (small pref, small load, small store, card size)
/* MOVSD, MOVSS, MOVAPD, MOVAPS:  xmm, xmm/m  (load)   m, xmm  (store) */
{
   Expression (&a);
   Separator ();
   Expression (&b);
   if (a.sort == xmm && (b.sort == xmm || b.sort == mem))
   {
      CheckMemSize (&b, size);
      InsX (pref, 0x0F00 | load, xmmR (&a), b, false);
   }
   else if (a.sort == mem && b.sort == xmm)
   {
      CheckMemSize (&a, size);
      InsX (pref, 0x0F00 | store, xmmR (&b), a, false);
   }
   else AsmError (BadSort);
}

static void MovdMovq (bool q)
/* MOVD xmm, r/m32   MOVD r/m32, xmm      (66 0F 6E / 7E)
   MOVQ xmm, r/m64   MOVQ r/m64, xmm      (66 REX.W 0F 6E / 7E)
   MOVQ xmm, xmm/m64 (F3 0F 7E)   MOVQ m64, xmm (66 0F D6) */
{
   card size = q ? qword : dword;
   Expression (&a);
   Separator ();
   Expression (&b);
   if (a.sort == xmm && b.sort == reg)
   {
      if (b.typ != size) AsmError (BadSize);
      InsX (0x66, 0x0F6E, xmmR (&a), b, q);
   }
   else if (a.sort == reg && b.sort == xmm)
   {
      if (a.typ != size) AsmError (BadSize);
      InsX (0x66, 0x0F7E, xmmR (&b), a, q);
   }
   else if (q && a.sort == xmm && (b.sort == xmm || b.sort == mem))
   {
      CheckMemSize (&b, qword);
      InsX (0xF3, 0x0F7E, xmmR (&a), b, false);
   }
   else if (q && a.sort == mem && b.sort == xmm)
   {
      CheckMemSize (&a, qword);
      InsX (0x66, 0x0FD6, xmmR (&b), a, false);
   }
   else if (! q && a.sort == xmm && b.sort == mem)
   {
      CheckMemSize (&b, dword);
      InsX (0x66, 0x0F6E, xmmR (&a), b, false);
   }
   else if (! q && a.sort == mem && b.sort == xmm)
   {
      CheckMemSize (&a, dword);
      InsX (0x66, 0x0F7E, xmmR (&b), a, false);
   }
   else AsmError (BadSort);
}

static void CvtToXmm (small pref)
/* CVTSI2SD / CVTSI2SS xmm, r/m32 nebo r/m64 */
{
   XmmOnly (&a);
   Separator ();
   RegOrMem (&b);
   if (b.typ == any) AsmError (UndefSize);
   if (b.typ != dword && b.typ != qword) AsmError (BadSize);
   InsX (pref, 0x0F2A, xmmR (&a), b, b.typ == qword);
}

static void CvtToReg (small pref, small op, card size)
/* CVTTSD2SI / CVTSD2SI / CVTTSS2SI / CVTSS2SI  r32 nebo r64, xmm/m */
{
   RegOnly (&a);
   Separator ();
   XmmOrMem (&b, size);
   if (a.typ != dword && a.typ != qword) AsmError (BadSize);
   InsX (pref, 0x0F00 | op, seqR (a.rx), b, a.typ == qword);
}

static bool Invalid64 (keyword i)
/* instrukce, ktere v 64bitovem rezimu neexistuji */
{
   switch (i)
   {
      case iAaa:   case iAad:    case iAam:   case iAas:   case iDaa:   case iDas:
      case iInto:  case iBound:  case iLds:   case iLes:   case iArpl:  case iJcxz:
      case iPusha: case iPopa:   case iPushad: case iPopad:
         return true;
      default:
         return false;
   }
}

/******************************** SELECTION *********************************/

static void Send (card ic)
{
   Ins (i86, any, ic);
}

static void SendW (card ic)
{
   Ins (i86, word, ic);
}

static void SendD (card ic)
{
   Ins (i386, dword, ic);
}

static void Select (keyword i)
{
   if (x64 && Invalid64 (i)) AsmError (Inv64);

   switch (i)
   {
      case iAaa:      Send   (0x37); break;               /* intel 86/186/286 */
      case iAad:      Send   (0xD50A); break;
      case iAam:      Send   (0xD40A); break;
      case iAas:      Send   (0x3F); break;
      case iAdc:      Arit   (2, true); break;
      case iAdd:      Arit   (0, true); break;
      case iAnd:      Arit   (4, false); break;

      case iBound:    Bound (); break;

      case iCall:     Jump  (false); break;
      case iCbw:      SendW (0x98); break;
      case iClc:      Send  (0xF8); break;
      case iCld:      Send  (0xFC); break;
      case iCli:      Send  (0xFA); break;
      case iCmc:      Send  (0xF5); break;
      case iCmp:      Arit  (7, true); break;
      case iCmps:     Block (i86, 0xA6, 1, 1); break;
      case iCmpsb:    Send  (0xA6); break;
      case iCmpsw:    SendW (0xA7); break;
      case iCwd:      SendW (0x99); break;

      case iDaa:      Send  (0x27); break;
      case iDas:      Send  (0x2F); break;
      case iDec:      Incr  (false); break;
      case iDiv:      Func  (0xF6, 6); break;

      case iEnter:    Enter (); break;
      case iEsc:      Escape (); break;

      case iHlt:      Send  (0xF4); break;

      case iIdiv:     Func  (0xF6, 7); break;
      case iImul:     Mul (); break;
      case iIn:       In86 (); break;
      case iInc:      Incr  (true); break;
      case iIns:      Block (i186, 0x6C, 1, 2); break;
      case iInsb:     Send  (0x6C); break;
      case iInsw:     SendW (0x6D); break;
      case iInt:      Int86 (); break;
      case iInto:     Send  (0xCE); break;
      case iIret:     SendW (0xCF); break;

      case iJo:       Jc    (0); break;
      case iJno:      Jc    (1); break;
      case iJb:
      case iJnae:
      case iJc:       Jc    (2); break;
      case iJnb:
      case iJae:
      case iJnc:      Jc    (3); break;
      case iJe:
      case iJz:       Jc    (4); break;
      case iJne:
      case iJnz:      Jc    (5); break;
      case iJbe:
      case iJna:      Jc    (6); break;
      case iJnbe:
      case iJa:       Jc    (7); break;
      case iJs:       Jc    (8); break;
      case iJns:      Jc    (9); break;
      case iJp:
      case iJpe:      Jc    (0xA); break;
      case iJnp:
      case iJpo:      Jc    (0xB); break;
      case iJl:
      case iJnge:     Jc    (0xC); break;
      case iJnl:
      case iJge:      Jc    (0xD); break;
      case iJle:
      case iJng:      Jc    (0xE); break;
      case iJnle:
      case iJg:       Jc    (0xF); break;

      case iJcxz:     Loop  (0xE3, word); break;
      case iJrcxz:    Need64 (); Loop (0xE3, qword); break;
      case iJmp:      Jump  (true); break;

      case iLahf:     Send  (0x9F); break;
      case iLds:      Load  (DS); break;
      case iLea:      Lea (); break;
      case iLeave:    Send  (0xC9); break;
      case iLes:      Load  (ES); break;
      case iLods:     Block (i86, 0xAC, 0, 1); break;
      case iLodsb:    Send  (0xAC); break;
      case iLodsw:    SendW (0xAD); break;
      case iLoop:     Loop  (0xE2, AddrDef ()); break;
      case iLoopz:
      case iLoope:    Loop  (0xE1, AddrDef ()); break;
      case iLoopnz:
      case iLoopne:   Loop  (0xE0, AddrDef ()); break;

      case iMov:      Move (); break;
      case iMovs:     Block (i86, 0xA4, 1, 1); break;
      case iMovsb:    Send  (0xA4); break;
      case iMovsw:    SendW (0xA5); break;
      case iMul:      Func  (0xF6, 4); break;

      case iNeg:      Func  (0xF6, 3); break;
      case iNop:      Send  (0x90); break;
      case iNot:      Func  (0xF6, 2); break;

      case iOr:       Arit  (1, false); break;
      case iOut:      Out86 (); break;
      case iOuts:     Block (i186, 0x6E, 2, 1); break;
      case iOutsb:    Send  (0x6E); break;
      case iOutsw:    SendW (0x6F); break;

      case iPop:      Pop (); break;
      case iPopa:     SendW (0x61); break;
      case iPopf:     SendW (0x9D); break;
      case iPush:     Push (); break;
      case iPusha:    SendW (0x60); break;
      case iPushf:    SendW (0x9C); break;

      case iRcl:      Rot   (2); break;
      case iRcr:      Rot   (3); break;
      case iRet:      if (NearProc ()) Ret (0xC2); else Ret (0xCA); break;
      case iRetf:     Ret   (0xCA); break;
      case iRetn:     Ret   (0xC2); break;
      case iRol:      Rot   (0); break;
      case iRor:      Rot   (1); break;

      case iSahf:     Send  (0x9E); break;
      case iShl:
      case iSal:      Rot   (4); break;
      case iSar:      Rot   (7); break;
      case iSbb:      Arit  (3, true); break;
      case iScas:     Block (i86, 0xAE, 1, 0); break;
      case iScasb:    Send  (0xAE); break;
      case iScasw:    SendW (0xAF); break;
      case iShr:      Rot   (5); break;
      case iStc:      Send  (0xF9); break;
      case iStd:      Send  (0xFD); break;
      case iSti:      Send  (0xFB); break;
      case iStos:     Block (i86, 0xAA, 1, 0); break;
      case iStosb:    Send  (0xAA); break;
      case iStosw:    SendW (0xAB); break;
      case iSub:      Arit  (5, true); break;

      case iTest:     Test (); break;

      case iWait:     Send  (0x9B); break;

      case iXchg:     Xchg (); break;
      case iXlat:     Send  (0xD7); break;
      case iXlatb:    XlatB (); break;
      case iXor:      Arit  (6, false); break;
                                                      /* intel 286 */
      case iArpl:     Arpl (); break;
      case iClts:     Ins   (i286, any, 0x0F06); break;
      case iLar:      Lar (); break;
      case iLgdt:     Lxx   (0x01, 2, 6); break;
      case iLidt:     Lxx   (0x01, 3, 6); break;
      case iLldt:     Lxx   (0x00, 2, word); break;
      case iLmsw:     Lxx   (0x01, 6, word); break;
      case iLsl:      Lsl (); break;
      case iLtr:      Lxx   (0x00, 3, word); break;
      case iSgdt:     Lxx   (0x01, 0, 6); break;
      case iSidt:     Lxx   (0x01, 1, 6); break;
      case iSldt:     Lxx   (0x00, 0, word); break;
      case iSmsw:     Lxx   (0x01, 4, word); break;
      case iStr:      Lxx   (0x00, 1, word); break;
      case iVerr:     Lxx   (0x00, 4, word); break;
      case iVerw:     Lxx   (0x00, 5, word); break;
                                                      /* intel 386 */
      case iBsf:      Bsf   (true); break;
      case iBsr:      Bsf   (false); break;
      case iBt:       Bt    (0xA3, 4); break;
      case iBtc:      Bt    (0xBB, 7); break;
      case iBtr:      Bt    (0xB3, 6); break;
      case iBts:      Bt    (0xAB, 5); break;
      case iCdq:      SendD (0x99); break;
      case iCmpsd:    SendD (0xA7); break;
      case iCwde:     SendD (0x98); break;
      case iInsd:     SendD (0x6D); break;
      case iIretd:    SendD (0xCF); break;
      case iJecxz:    Loop  (0xE3, dword); break;
      case iLfs:      Load  (FS); break;
      case iLgs:      Load  (GS); break;
      case iLodsd:    SendD (0xAD); break;
      case iLss:      Load  (SS); break;
      case iMovsd:                     /* bez operandu retezcova, jinak SSE2 */
         if (CurSy == EOL) SendD (0xA5);
         else SseMove (0xF2, 0x10, 0x11, qword);
         break;
      case iMovsx:    Movsx (true); break;
      case iMovzx:    Movsx (false); break;
      case iOutsd:    SendD (0x6F); break;
      case iPopad:    SendD (0x61); break;
      case iPopfd:    SendD (0x9D); break;
      case iPushad:   SendD (0x60); break;
      case iPushfd:   SendD (0x9C); break;
      case iScasd:    SendD (0xAF); break;
      case iSeta:     Setcc (0x97); break;
      case iSetae:    Setcc (0x93); break;
      case iSetb:     Setcc (0x92); break;
      case iSetbe:    Setcc (0x96); break;
      case iSetc:     Setcc (0x92); break;
      case iSete:     Setcc (0x94); break;
      case iSetg:     Setcc (0x9F); break;
      case iSetge:    Setcc (0x9D); break;
      case iSetl:     Setcc (0x9C); break;
      case iSetle:    Setcc (0x9E); break;
      case iSetna:    Setcc (0x96); break;
      case iSetnae:   Setcc (0x92); break;
      case iSetnb:    Setcc (0x93); break;
      case iSetnbe:   Setcc (0x97); break;
      case iSetnc:    Setcc (0x93); break;
      case iSetne:    Setcc (0x95); break;
      case iSetng:    Setcc (0x9E); break;
      case iSetnge:   Setcc (0x9C); break;
      case iSetnl:    Setcc (0x9D); break;
      case iSetnle:   Setcc (0x9F); break;
      case iSetno:    Setcc (0x91); break;
      case iSetnp:    Setcc (0x9B); break;
      case iSetns:    Setcc (0x99); break;
      case iSetnz:    Setcc (0x95); break;
      case iSeto:     Setcc (0x90); break;
      case iSetp:     Setcc (0x9A); break;
      case iSetpe:    Setcc (0x9A); break;
      case iSetpo:    Setcc (0x9B); break;
      case iSets:     Setcc (0x98); break;
      case iSetz:     Setcc (0x94); break;
      case iShld:     Shld  (true); break;
      case iShrd:     Shld  (false); break;
      case iStosd:    SendD (0xAB); break;

      case iCdqe:     SendQ (0x98); break;                /* x86-64 */
      case iCqo:      SendQ (0x99); break;
      case iCmpsq:    SendQ (0xA7); break;
      case iLodsq:    SendQ (0xAD); break;
      case iMovsq:    SendQ (0xA5); break;
      case iMovsxd:   Movsxd (); break;

      /* SSE, SSE2 */
      case iAddsd:    SseArith (0xF2, 0x58, qword); break;
      case iSubsd:    SseArith (0xF2, 0x5C, qword); break;
      case iMulsd:    SseArith (0xF2, 0x59, qword); break;
      case iDivsd:    SseArith (0xF2, 0x5E, qword); break;
      case iSqrtsd:   SseArith (0xF2, 0x51, qword); break;
      case iMinsd:    SseArith (0xF2, 0x5D, qword); break;
      case iMaxsd:    SseArith (0xF2, 0x5F, qword); break;
      case iAddss:    SseArith (0xF3, 0x58, dword); break;
      case iSubss:    SseArith (0xF3, 0x5C, dword); break;
      case iMulss:    SseArith (0xF3, 0x59, dword); break;
      case iDivss:    SseArith (0xF3, 0x5E, dword); break;
      case iSqrtss:   SseArith (0xF3, 0x51, dword); break;
      case iMinss:    SseArith (0xF3, 0x5D, dword); break;
      case iMaxss:    SseArith (0xF3, 0x5F, dword); break;
      case iUcomisd:  SseArith (0x66, 0x2E, qword); break;
      case iComisd:   SseArith (0x66, 0x2F, qword); break;
      case iUcomiss:  SseArith (0,    0x2E, dword); break;
      case iComiss:   SseArith (0,    0x2F, dword); break;
      case iCvtsd2ss: SseArith (0xF2, 0x5A, qword); break;
      case iCvtss2sd: SseArith (0xF3, 0x5A, dword); break;
      case iAndpd:    SseArith (0x66, 0x54, any); break;
      case iAndnpd:   SseArith (0x66, 0x55, any); break;
      case iOrpd:     SseArith (0x66, 0x56, any); break;
      case iXorpd:    SseArith (0x66, 0x57, any); break;
      case iAndps:    SseArith (0,    0x54, any); break;
      case iAndnps:   SseArith (0,    0x55, any); break;
      case iOrps:     SseArith (0,    0x56, any); break;
      case iXorps:    SseArith (0,    0x57, any); break;
      case iPxor:     SseArith (0x66, 0xEF, any); break;
      case iMovss:    SseMove (0xF3, 0x10, 0x11, dword); break;
      case iMovapd:   SseMove (0x66, 0x28, 0x29, any); break;
      case iMovaps:   SseMove (0,    0x28, 0x29, any); break;
      case iMovd:     MovdMovq (false); break;
      case iMovq:     MovdMovq (true); break;
      case iCvtsi2sd: CvtToXmm (0xF2); break;
      case iCvtsi2ss: CvtToXmm (0xF3); break;
      case iCvttsd2si: CvtToReg (0xF2, 0x2C, qword); break;
      case iCvtsd2si: CvtToReg (0xF2, 0x2D, qword); break;
      case iCvttss2si: CvtToReg (0xF3, 0x2C, dword); break;
      case iCvtss2si: CvtToReg (0xF3, 0x2D, dword); break;
      case iScasq:    SendQ (0xAF); break;
      case iStosq:    SendQ (0xAB); break;
      case iSyscall:  Need64 (); Ins (i64, any, 0x0F05); break;

      case iF2xm1:    Fce   (6, 0); break;                /* intel 87 */

      case iFabs:     Fce   (4, 1); break;
      case iFadd:     Bin   (0, 0); break;
      case iFaddp:    BinP  (0); break;

      case iFbld:     Ope   (7, 4, tbyte, true); break;
      case iFbstp:    Ope   (7, 6, tbyte, true); break;

      case iFchs:     Fce   (4, 0); break;
      case iFclex:    Sim   (3, 4, 2, true); break;
      case iFcom:     Comp  (2); break;
      case iFcomp:    Comp  (3); break;
      case iFcompp:   Sim   (6, 3, 1, true); break;

      case iFdecstp:  Fce   (6, 6); break;
      case iFdisi:    Sim   (3, 4, 1, true); break;
      case iFdiv:     Bin   (7, 6); break;
      case iFdivp:    BinP  (7); break;
      case iFdivr:    Bin   (6, 7); break;
      case iFdivrp:   BinP  (6); break;

      case iFeni:     Sim   (3, 4, 0, true); break;

      case iFfree:    Ffree (); break;

      case iFiadd:    BinI  (0); break;
      case iFicom:    BinI  (2); break;
      case iFicomp:   BinI  (3); break;
      case iFidiv:    BinI  (6); break;
      case iFidivr:   BinI  (7); break;
      case iFild:     Whole (fld); break;
      case iFimul:    BinI  (1); break;
      case iFincstp:  Fce   (6, 7); break;
      case iFinit:    Sim   (3, 4, 3, true); break;
      case iFist:     Whole (fst); break;
      case iFistp:    Whole (fstp); break;
      case iFisub:    BinI  (4); break;
      case iFisubr:   BinI  (5); break;

      case iFld:      Float_ (fld); break;
      case iFld1:     Fce   (5, 0); break;
      case iFldcw:    Ope   (1, 5, any, true); break;
      case iFldenv:   Ope   (1, 4, any, true); break;
      case iFldl2e:   Fce   (5, 2); break;
      case iFldl2t:   Fce   (5, 1); break;
      case iFldlg2:   Fce   (5, 4); break;
      case iFldln2:   Fce   (5, 5); break;
      case iFldpi:    Fce   (5, 3); break;
      case iFldz:     Sim   (1, 5, 6, true); break;

      case iFmul:     Bin   (1, 1); break;
      case iFmulp:    BinP  (1); break;

      case iFnclex:   Sim   (3, 4, 2, false); break;
      case iFndisi:   Sim   (3, 4, 1, false); break;
      case iFneni:    Sim   (3, 4, 0, false); break;
      case iFninit:   Sim   (3, 4, 3, false); break;
      case iFnop:     Sim   (1, 2, 0, true ); break;
      case iFnsave:   Ope   (5, 6, any, false); break;
      case iFnstcw:   Ope   (1, 7, any, false); break;
      case iFnstenv:  Ope   (1, 6, any, false); break;
      case iFnstsw:   Fstsw (false); break;

      case iFpatan:   Fce   (6, 3); break;
      case iFprem:    Fce   (7, 0); break;
      case iFptan:    Fce   (6, 2); break;

      case iFrndint:  Fce   (7, 4); break;
      case iFrstor:   Ope   (5, 4, any, true); break;

      case iFsave:    Ope   (5, 6, any, true); break;
      case iFscale:   Fce   (7, 5); break;
      case iFsqrt:    Fce   (7, 2); break;
      case iFst:      Float_ (fst); break;
      case iFstcw:    Ope   (1, 7, any, true); break;
      case iFstenv:   Ope   (1, 6, any, true); break;
      case iFstp:     Float_ (fstp); break;
      case iFstsw:    Fstsw (true); break;
      case iFsub:     Bin   (5, 4); break;
      case iFsubp:    BinP  (5); break;
      case iFsubr:    Bin   (4, 5); break;
      case iFsubrp:   BinP  (4); break;

      case iFtst:     Fce   (4, 4); break;

      case iFwait:    Send  (0x9B); break; /* ! */

      case iFxam:     Fce   (4, 5); break;
      case iFxch:     Fxch  (1, 1); break;
      case iFxtract:  Fce   (6, 4); break;

      case iFyl2x:    Fce   (6, 1); break;
      case iFyl2xp1:  Fce   (7, 1); break;

      case iFsetpm:   Sim   (3, 4, 4, true); break;       /* intel 287 */

      case iFcos:     Fce   (7, 7); break;
      case iFprem1:   Fce   (6, 5); break;
      case iFsin:     Fce   (7, 6); break;
      case iFsincos:  Fce   (7, 3); break;

      case iFucom:    Fxch  (5, 4); break;
      case iFucomp:   Fxch  (5, 5); break;
      case iFucompp:  Sim   (2, 5, 1, true); break;

      case ip8086:    Choose (i86); break;
      case ip8087:    Choose (i87); break;
      case ip186:     Choose (i186); break;
      case ip286:
      case ip286c:
      case ip286p:    Choose (i286); break;
      case ip287:     Choose (i287); break;
      case ip386:
      case ip386c:
      case ip386p:    Choose (i386); break;
      case ip387:     Choose (i387); break;
      case ipX64:     Choose (i64); break;

      case iAlign:    AlignDir (); break;
      case iAssume:   AssumeDir (); break;
      case iDb:       DefDir (byte); break;
      case iDw:       DefDir (word); break;
      case iDd:       DefDir (dword); break;
      case iDf:       DefDir (fword); break;
      case iDq:       DefDir (qword); break;
      case iDt:       DefDir (tbyte); break;
      case iEnd:      EndDir (); break;
      case iEndp:     EndpDir (); break;
      case iEnds:     EndsDir (); break;
      case iEqu:      EquDir (); break;
      case iEven:     EvenDir (); break;
      case iExtrn:    ExtrnDir (); break;
      case iGroup:    GroupDir (); break;
      case iLabel:    LabelDir (); break;
      case iOrg:      OrgDir (); break;
      case iProc:     ProcDir (); break;
      case iPublic:   PublicDir (); break;
      case iSegment:  SegmentDir (); break;

      default:        AsmError (Unsupported);
   }
}

/******************************* FIRST PASS *******************************/

/* DirSet = set of ip186 .. iStruc  (iStruc = 33, vejde se do 64 bitu) */
typedef uint64_t DirSet;

#define DIR(k) ((DirSet) 1 << (k))

/* PrefixSet = [iLock, iRep, iRepe, iRepne, iRepnz, iRepz] - viz CompileLineBody */

/* mnoziny jako funkce - konstantni vyraz s posuny v inicializaci promenne
   nezna cparser ani cmm --llvm */
static DirSet NoSegSet (void)
{
   return
      DIR (iSegment) | DIR (iEnds) | DIR (iEnd) | DIR (iGroup) | DIR (iExtrn) |
      DIR (iPublic) | DIR (iEqu) | DIR (iAssume) |
      DIR (ip8086) | DIR (ip8087) | DIR (ip186) | DIR (ip286) | DIR (ip286c) |
      DIR (ip286p) | DIR (ip386) | DIR (ip386c) | DIR (ip386p) | DIR (ip387) |
      DIR (ipX64);
}

static DirSet NameSet (void)
{
   return
      DIR (iSegment) | DIR (iEnds) | DIR (iProc) | DIR (iEndp) | DIR (iGroup) |
      DIR (iLabel) | DIR (iEqu) |
      DIR (iDb) | DIR (iDw) | DIR (iDd) | DIR (iDf) | DIR (iDq) | DIR (iDt);
}

static bool InDir (keyword k, DirSet s)
/* k in s - ve Free Pascalu dava 'in' u male mnoziny chybne vysledky
   pro hodnoty mimo rozsah mnoziny (index bitu se bere modulo 32) */
{
   return k <= iStruc && ((s >> k) & 1) != 0;
}

static bool InPrefixSet (keyword k)
{
   return k == iLock || k == iRep || k == iRepe ||
          k == iRepne || k == iRepnz || k == iRepz;
}

static void CompileNode (const AsmNode * n)
/* instrukce nebo navesti primo z uzlu (faze 2) - stejne jako text nize */
{
   lab = NULL;

   if (n->kind == AN_LABEL)                   /* jmeno: */
   {
      TreePtr t = NodeLabel (n->label);
      if (t->ope.sort == unknown) NewLabel (t, near_);
      else SetLabel (t);
      return;
   }

   if (n->prefix != iOther) InstPref = n->prefix;
   if (ThisSeg == NULL && ! InDir (n->inst, NoSegSet ())) AsmError (NoSeg);
   NodeOperands (n);
   Select (n->inst);
   if (CurSy != EOL) AsmError (ExtChar);
}

static void CompileLineBody (void)
{
   keyword i;

   PutLine (true);
   NextLine ();
   PrepInst ();

   const AsmNode * n = SourceNode ();
   if (n != NULL)
   {
      if (NodeDirect (n))
      {
         CompileNode (n);
         PutLine (false);
         return;
      }
      NodeSourceText ();                       /* direktivy, data, text */
   }

   Insymbol ();
   lab = NULL;

   if (CurSy == Identif && ans->ope.sort != reserved)
   {
      lab = ans;
      Insymbol ();

      if (CurSy == Colon)
      {
         Insymbol ();
         if (lab->ope.sort == unknown) NewLabel (lab, near_);
         else SetLabel (lab);
         lab = NULL;
      }
      else if (CurSy != Identif || ans->ope.sort != reserved ||
               ! InDir (ans->ope.res, NameSet ()))
      {
         NewLabel (lab, far_);
         lab = NULL;
      }
   }

   if (CurSy == Identif)
      if (ans->ope.sort == reserved &&
          ans->ope.res >= iLock && ans->ope.res <= iRepz &&
          InPrefixSet (ans->ope.res))
      {
         InstPref = ans->ope.res;
         Insymbol ();
      }

   if (CurSy == Identif)
   {
      if (ans->ope.sort == reserved)
      {
         i = ans->ope.res;
         if (ThisSeg == NULL && ! InDir (i, NoSegSet ())) AsmError (NoSeg);
         Insymbol ();
         Select (i);
      }
      else AsmError (InstExp);
   }

   if (CurSy != EOL) AsmError (ExtChar);
   PutLine (false);
}

static void CompileLine (void)
/* Pri chybe (AsmError) se pokracuje dalsi radkou, viz Def.Retry */
{
   if (setjmp (LineJump) == 0) CompileLineBody ();
}

static void CheckTree (TreePtr ref)
{
   if (ref != NULL)
   {
      CheckTree (ref->left);
      if (! ref->def)
      {
         StrSet (ErrId, sizeof (alpha), ref->name);
         Message (UndefId);
      }
      if ((ref->ope.sort == mem || ref->ope.sort == imm) && ref->ope.add != NULL)
      {
         ref->ope.val = (longint) ((uint32_t) ref->ope.val + (uint32_t) ref->ope.add->ope.val);
         ref->ope.val64 = ref->ope.val;
         ref->ope.add = NULL;
      }
      CheckTree (ref->right);
   }
}

void FirstPass (void)
{
   OpenSource ();
   while (! SourceEof ()) CompileLine ();
   CloseSource ();

   CheckTree (DispUsr);
}
