/* OTH.CC */

/* vystupni zaznamy a direktivy (prevedeno z oth.pas) */

#include <string.h>
#include <stdlib.h>

#include "key.h"
#include "def.h"
#include "expr.h"
#include "oth.h"

ResultPtr FirstRes;
ResultPtr LastRes;
ResultPtr Entry;

/********************************** PUT ***********************************/

static ResultRec work;

void InitRes (void)
{
   FirstRes = NULL;
   LastRes  = NULL;
   Entry    = NULL;
   work.cls = Data;
   work.DataLen = 0;
}

static void SendRes (void)
{
   ResultPtr ref;

   if (work.cls != Data || work.DataLen > 0)
   {
      /* puvodne ResultSize [work.class] (+ DataLen) - velikosti pro
         16bitove ukazatele; zde cely zaznam */
      work.next = NULL;

      ref = (ResultPtr) malloc (sizeof (ResultRec));
      if (ref == NULL) Abort (HeapFull);
      *ref = work;

      if (LastRes == NULL)
      {
         FirstRes = ref;
         LastRes = ref;
      }
      else
      {
         LastRes->next = ref;
         LastRes = ref;
      }
   }
   work.cls = Data;
   work.DataLen = 0;
}

void Put (small b, char c)
{
   (void) c;
   if (work.cls != Data || work.DataLen >= DataMax) SendRes ();
   work.DataLen++;
   work.DataSeq [work.DataLen] = b;
   ip = (longint) ((uint32_t) ip + 1);
}

static void PutFixup (ExprRec e, FixTyp kind, card width)
{
   SendRes ();
   work.cls       = Fixup;
   work.FixSort   = kind;
   work.FixWidth  = width;
   work.FixFrame  = e.frame;
   work.FixTarget = e.target;
   work.FixAdd    = e.add;
   work.FixVal    = e.val;
   work.Fix64     = x64 && width == qword;
   if (e.target == NULL && e.add == NULL && e.frame == NULL)
      work.FixVal64 = e.val64;      /* konstanta - 64 bitu */
   else
      work.FixVal64 = e.val;        /* adresa - nejvyse 32 bitu */
   SendRes ();
   ip = (longint) ((uint32_t) ip + width);
}

void PutData (ExprRec * e, card width)
{
   if (!e->para)
      PutFixup (*e, FixOfs, width);
   else
   {
      if (pas || bin) AsmError (SegNotAll);
      if (elf || eobj) AsmError (ElfSegRef);
      if (width == byte) AsmError (Bug);
      PutFixup (*e, FixSeg, width);
   }
}

void PutNear (ExprRec * e, card width)
{
   PutFixup (*e, FixNear, width);
}

void PutRel (ExprRec * e, card width, card extra)
/* adresa relativni k RIP (x86-64): cil - konec instrukce,
   extra = pocet bajtu instrukce za adresou (prima hodnota) */
{
   ExprRec t = *e;
   if (e->para) AsmError (BadPar);
   t.val = (longint) ((uint32_t) t.val - extra);
   PutFixup (t, FixRel, width);
}

void PutFar (ExprRec * e, card width)
{
   if (pas || bin) AsmError (FarNotAll);
   if (elf || eobj) AsmError (ElfFar);
   PutFixup (*e, FixFar, (card) (word + width));
}

void PutLine (bool start)
{
   SendRes ();
   if (start) work.cls = LineBeg;
         else work.cls = LineEnd;
   SendRes ();
}

static void PutPos (void)
{
   SendRes ();
   work.cls    = NewPos;
   work.NewInx = ThisSeg->ope.SegInx;
   work.New32  = ThisSeg->ope.Seg32;
   work.NewOfs = ip;
   SendRes ();
}

/******************************* AUXILIARY ********************************/

static bool Present (ExprSort par)
{
   return CurSy == Identif && ans != NULL && ans->ope.sort == par;
}

static bool KeywPresent (keyword par)
{
   return CurSy == Identif && ans != NULL &&
          ans->ope.sort == reserved && ans->ope.res == par;
}

/******************************* DIRECTIVES *******************************/

enum flag { free_, check, done };
typedef enum flag flag;

static flag AlignFlg, CombiFlg, Seg32Flg, ClassFlg;

static void SetClass (void)
{
   alpha  tem;
   ObjInx x;

   StrSet (tem, sizeof (alpha), chs);
   for (int i = 0; tem[i] != 0; i++)
      if (tem[i] >= 'a' && tem[i] <= 'z') tem[i] = tem[i] - 'a' + 'A';
   Insymbol ();

   x = LocAppend (tem);
   switch (ClassFlg)
   {
      case free_: lab->ope.ClassName = x; break;
      case check: if (lab->ope.ClassName != x) AsmError (SegRedef); break;
      case done:  AsmError (SegRedef);
   }
   ClassFlg = done;
}

static void SetAlign (AlignTyp x)
{
   Insymbol ();
   switch (AlignFlg)
   {
      case free_: lab->ope.align = x; break;
      case check: if (lab->ope.align != x) AsmError (SegRedef); break;
      case done:  AsmError (SegRedef);
   }
   AlignFlg = done;
}

static void SetAbs (void)
{
   ExprRec a;

   Insymbol ();
   Expression (&a);
   if (!AbsExp (&a)) AsmError (AbsOpExp);
   if (a.val < 0 || a.val > 64 * 1024) AsmError (OutOfRange);

   switch (AlignFlg)
   {
      case free_:
         lab->ope.align = AbsSeg;
         lab->ope.FrameNum = (card) a.val;
         break;
      case check:
         if (lab->ope.align != AbsSeg ||
             (longint) lab->ope.FrameNum != a.val) AsmError (SegRedef);
         break;
      case done:
         AsmError (SegRedef);
   }
   AlignFlg = done;
}

static void SetCombi (CombiTyp x)
{
   Insymbol ();
   switch (CombiFlg)
   {
      case free_: lab->ope.combi = x; break;
      case check: if (lab->ope.combi != x) AsmError (SegRedef); break;
      case done:  AsmError (SegRedef);
   }
   CombiFlg = done;
}

static void SetSeg32 (bool x, bool x64v)
/* USE16, USE32, USE64 (x64v: USE64, x je pak take true) */
{
   Insymbol ();
   if (x64v && proc < i64) AsmError (InvProc);
   switch (Seg32Flg)
   {
      case free_: lab->ope.Seg32 = x;
                  lab->ope.Seg64 = x64v;
                  break;
      case check: if (lab->ope.Seg32 != x || lab->ope.Seg64 != x64v) AsmError (SegRedef); break;
      case done:  AsmError (SegRedef);
   }
   Seg32Flg = done;
}

void SegmentDir (void)
{
   flag init;

   if (lab == NULL) AsmError (MissingLab);
   if (lab->ope.sort == unknown) ForwSegment (lab);
   if (lab->ope.sort != segment) AsmError (DuplId);

   if ((pas || bin) && SegHead.count > 1) AsmError (TooManySeg);

   if (lab->def) init = check;
            else init = free_;
   lab->def = true;

   AlignFlg = init;
   CombiFlg = init;
   Seg32Flg = init;
   ClassFlg = init;

   while (CurSy != EOL)
   {
      if (CurSy == Chars) SetClass ();
      else if (Present (reserved))
         switch (ans->ope.res)
         {
            case iAt:     SetAbs (); break;
            case iByte:   SetAlign (ByteAlign); break;
            case iWord:   SetAlign (WordAlign); break;
            case iPara:   SetAlign (ParaAlign); break;
            case iPage:   SetAlign (PageAlign); break;
            case iDword:  SetAlign (DwordAlign); break;
            case iMemory: SetCombi (MemorySeg); break;
            case iPublic: SetCombi (PublicSeg); break;
            case iStack:  SetCombi (StackSeg); break;
            case iCommon: SetCombi (CommonSeg); break;
            case iUse16:  SetSeg32 (false, false); break;
            case iUse32:  SetSeg32 (true, false); break;
            case iUse64:  SetSeg32 (true, true); break;
            default:      AsmError (Syntax);
         }
      else AsmError (Syntax);
   }

   if (ThisSeg != NULL) AsmError (NestSeg);
   ThisSeg  = lab;
   ThisProc = NULL;
   if (ThisSeg->ope.Seg32) default_ = dword;   /* i USE64: operandy 32 bitu */
                      else default_ = word;
   x64 = ThisSeg->ope.Seg64;
   if (x64 && (obj || pas || bin)) AsmError (Use64NotAll);
   ip = ThisSeg->ope.SegLen;

   PutPos ();
   MainSeg = ThisSeg;
}

void EndsDir (void)
{
   const longint k64 = 64 * 1024;

   if (lab == NULL) AsmError (MissingLab);
   if (ThisSeg != lab) AsmError (DifName);
   {
      ExprRec * o = &ThisSeg->ope;
      if (ip < 0) AsmError (SegTooLong);
      if ((bin || !o->Seg32) && ip > k64) AsmError (SegTooLong);
      if (elf && !o->Seg32) AsmError (ElfUse16);
      if (elf && o->align == AbsSeg) AsmError (ElfAbsSeg);
      if (eobj && !o->Seg64) AsmError (EobjUse64);
      if (eobj && o->align == AbsSeg) AsmError (ElfAbsSeg);
      if (o->SegLen < ip) o->SegLen = ip;
   }
   ThisSeg = NULL;
   x64 = false;
   if (ThisProc != NULL) AsmError (EndpExp);
}

void ProcDir (void)
{
   card t;

   if (lab == NULL) AsmError (MissingLab);
   t = near_;
   if (KeywPresent (iFar))
   {
      t = far_;
      Insymbol ();
   }
   else if (KeywPresent (iNear))
   {
      t = near_;
      Insymbol ();
   }
   DefineLabel (lab, t);

   if (ThisProc != NULL) AsmError (NestProc);
   ThisProc = lab;
}

void EndpDir (void)
{
   if (lab == NULL) AsmError (MissingLab);
   if (ThisProc != lab) AsmError (DifName);
   ThisProc = NULL;
}

void EndDir (void)
{
   ExprRec a;

   if (ThisProc != NULL) AsmError (EndpExp);
   if (ThisSeg != NULL) AsmError (EndsExp);

   if (CurSy != EOL)
   {
      if (pas) AsmError (EntryNotAll);
      Expression (&a);
      if (a.sort != mem) AsmError (BadSort);
      if (a.typ != near_ && a.typ != far_) AsmError (BadSize);
      if (a.frame == NULL) AsmError (BadPar);

      Entry = (ResultPtr) calloc (1, sizeof (ResultRec));
      if (Entry == NULL) Abort (HeapFull);
      Entry->cls       = Fixup;
      Entry->FixSort   = FixOfs;
      Entry->FixWidth  = any;
      Entry->FixFrame  = a.frame;
      Entry->FixTarget = a.target;
      Entry->FixAdd    = a.add;
      Entry->FixVal    = a.val;
   }

   SendRes ();
   terminate_ = true;
}

static bool Stop (void)
{
   if (CurSy != Comma)
      return true;
   else
   {
      Insymbol ();
      return false;
   }
}

void GroupDir (void)
{
   ObjInx dummy;

   if (pas || bin) AsmError (GroupNotAll);
   if (elf || eobj) AsmError (ElfGroup);
   if (lab == NULL) AsmError (MissingLab);
   NewGroup (lab);

   do
   {
      if (CurSy != Identif) AsmError (IdExp);
      if (ans->ope.sort == unknown) ForwSegment (ans);
      if (ans->ope.sort != segment) AsmError (SegExp);
      dummy = ListAppend (&lab->ope.GrpSeq, ans);
      (void) dummy;
      Insymbol ();
   }
   while (!Stop ());
}

void ExtrnDir (void)
{
   TreePtr ref;
   ExprRec a;

   if (bin) AsmError (ExtrnNotAll);
   if (elf && ! pe) AsmError (ElfExtrn);   /* PE: import z DLL (code.c) */
   do
   {
      if (CurSy != Identif) AsmError (IdExp);
      ref = ans;
      Insymbol ();

      if (CurSy != Colon) AsmError (ColonExp);
      Insymbol ();

      Expression (&a);
      if (a.sort == reserved && a.res == iAbs)
         NewExtrn (ref, imm, any);
      else
         NewExtrn (ref, mem, TypeConv (a));

      /* jmeno pro linker:  EXTRN X1: NEAR 'printf' */
      if (CurSy == Chars)
      {
         StrSet (ref->extname, sizeof (ref->extname), chs);
         Insymbol ();
      }
   }
   while (!Stop ());
}

void PublicDir (void)
{
   ObjInx dummy;

   do
   {
      if (CurSy != Identif) AsmError (IdExp);
      TreePtr ref = ans;
      dummy = ListAppend (&PubHead, ans);
      (void) dummy;
      Insymbol ();

      /* jmeno pro linker:  PUBLIC FMAIN 'main' */
      if (CurSy == Chars)
      {
         StrSet (ref->extname, sizeof (ref->extname), chs);
         Insymbol ();
      }
   }
   while (!Stop ());
}

void LabelDir (void)
{
   ExprRec a;
   card    t;

   if (lab == NULL) AsmError (MissingLab);
   Expression (&a);
   t = TypeConv (a);
   DefineLabel (lab, t);
}

void EquDir (void)
{
   ExprRec a;

   if (lab == NULL) AsmError (MissingLab);
   if (lab->ope.sort != unknown) AsmError (DuplId);
   Expression (&a);
   lab->def = true;
   lab->ope = a;
}

void OrgDir (void)
{
   ExprRec a;

   if (pas) AsmError (OrgNotAll);
   Expression (&a);
   if (!AbsExp (&a)) AsmError (AbsOpExp);
   ip = a.val;
   PutPos ();
}

void AssumeDir (void)
{
   Register rs;

   do
   {
      if (!Present (seg)) AsmError (SegRegExp);
      rs = ans->ope.rx;
      Insymbol ();

      if (CurSy != Colon) AsmError (ColonExp);
      Insymbol ();

      if (CurSy == Identif && ans->ope.sort == unknown) ForwSegment (ans);
      if (KeywPresent (iNothing)) assume [rs - SegMin] = NULL;
      else if (Present (segment) || Present (group)) assume [rs - SegMin] = ans;
      else AsmError (SegGrpExp);
      Insymbol ();
   }
   while (!Stop ());
}

void AlignDir (void)
{
   ExprRec a;
   card    t;

   Expression (&a);
   t = TypeConv (a);
   while (ip % (longint) t != 0) Put (0x00, ' ');
}

void EvenDir (void)
{
   if (ip & 1) Put (0x00, ' ');
}

#define RepMax 16

static int conv (char c)
{
   if (c >= '0' && c <= '9') return c - '0';
   if (c >= 'A' && c <= 'F') return c - 'A' + 10;
   AsmError (BadFloat);
}

static void PutFloat (card t)
{
   int len, start, stop, pos, n;

   len = (int) strlen (num);
   /* Pascal: num[0] (delka 0) <> 'R' */
   if (len == 0 || num[len - 1] != 'R') AsmError (BadFloat);

   stop = len - 1;
   start = 1;
   if (((stop - start + 1) & 1) && num[0] == '0') start = 2;
   if (stop - start + 1 != 2 * t) AsmError (BadFloat);

   pos = stop;
   while (pos >= start)
   {
      /* Pascal: chs [pos], chs [pos-1] (cislovano od 1) */
      n = conv (chs [pos - 1]) + 16 * conv (chs [pos - 2]);
      Put ((small) n, ' ');
      pos -= 2;
   }
}

void DefDir (card t)
{
   int     DupLevel;
   longint DupStart [RepMax + 1];   /* [1..RepMax] - poloha (i nad 64 kB, drive card) */
   card    DupCount [RepMax + 1];
   ExprRec a;

   if (lab != NULL) DefineLabel (lab, t);
   DupLevel = 0;

L1:
   if (CurSy == Float)
   {
      PutFloat (t);
      Insymbol ();
   }

   else if (CurSy == Chars && strlen (chs) > 1)
   {
      int len = (int) strlen (chs);
      for (int i = 0; i < len; i++) Put ((small) chs[i], ' ');
      Insymbol ();
   }

   else
   {
      Expression (&a);
      if (KeywPresent (iDup))
      {
         if (pas) AsmError (DupNotAll);
         if (!AbsExp (&a)) AsmError (AbsOpExp);
         DupLevel++;
         DupStart [DupLevel] = ip;
         DupCount [DupLevel] = (card) a.val;
         Insymbol ();

         if (CurSy != LPar) AsmError (LParExp);
         Insymbol ();

         SendRes ();
         work.cls = RepBeg;
         work.count = a.val;
         SendRes ();

         Expression (&a);
      }

      if (a.sort == reserved && a.res == iQuestion)
      {
         if (DupLevel > 0)
         {
            a = zero;
            PutData (&a, t);
         }
         else
         {
            ip = (longint) ((uint32_t) ip + t);
            PutPos ();
         }
      }
      else if (a.sort != mem && a.sort != imm) AsmError (MemImmExp);
      else PutData (&a, t);
   }

   if (CurSy == Comma)
   {
      Insymbol ();
      goto L1;
   }

   if (DupLevel > 0)
   {
      if (CurSy == RPar)
      {
         Insymbol ();
         ip = (longint) (uint32_t) ((int64_t) ip +
                 ((int64_t) ip - DupStart [DupLevel]) *
                 ((int64_t) DupCount [DupLevel] - 1));
         DupLevel--;

         SendRes ();
         work.cls = RepEnd;
         SendRes ();

         if (DupLevel > 0) goto L1;
      }
      else AsmError (RParExp);
   }
}
