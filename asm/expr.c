/* EXPR.CC */

/* lexikalni analyza a vyrazy (prevedeno z expr.pas) */

#include <string.h>

#include "expr.h"
#include "asmlist.h"

/****************************** DECLARATIONS ******************************/

#define CtrlM '\r'
#define CtrlJ '\n'
#define CtrlZ '\x1A'

/* Identif1: ['A'..'Z', 'a'..'z', '_', '?', '$', '@', '.'] */
static bool Identif1 (char c)
{
   return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
          c == '_' || c == '?' || c == '$' || c == '@' || c == '.';
}

/* Identif2: ['A'..'Z', 'a'..'z', '_', '?', '$', '@', '0'..'9'] */
static bool Identif2 (char c)
{
   return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
          c == '_' || c == '?' || c == '$' || c == '@' ||
          (c >= '0' && c <= '9');
}

/* Number1: ['0'..'9'] */
static bool Number1 (char c)
{
   return c >= '0' && c <= '9';
}

/* Number2: ['0'..'9', 'A'..'Z', 'a'..'z'] */
static bool Number2 (char c)
{
   return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
          (c >= 'a' && c <= 'z');
}

/* Special: [CtrlM, CtrlJ, CtrlZ] */
static bool Special (char c)
{
   return c == CtrlM || c == CtrlJ || c == CtrlZ;
}

/* Pascal UpCase - jen 'a'..'z' */
static char UpCase (char c)
{
   if (c >= 'a' && c <= 'z') return (char) (c - 'a' + 'A');
   return c;
}

#define MinLong (-1)          /* v TP: $FFFFFFFF jako longint */
#define MaxLong 0x7FFFFFFF

enum OpTyp
{
   /*  1 */ StOp,
   /*  3 */ ColonOp,
   /*  4 */ PtrOp, OffsetOp, SegOp, TypeOp, ThisOp,
   /*  5 */ HighOp, LowOp,
   /*  6 */ PlusOp, MinusOp,
   /*  7 */ MulOp, DivOp, ModOp, ShlOp, ShrOp,
   /*  8 */ AddOp, SubOp,
   /*  9 */ EqOp, NeOp, LtOp, LeOp, GtOp, GeOp,
   /* 10 */ NotOp,
   /* 11 */ AndOp,
   /* 12 */ OrOp, XorOp,
   /* 14 */ ShortOp,
   /* 15 */ BracketOp, ParOp
};
typedef enum OpTyp OpTyp;

enum KindTyp { unary, binary };
typedef enum KindTyp KindTyp;

typedef struct
{
   OpTyp   op;
   KindTyp kind;
   small   priority;
   bool    fill;
}
   StackRec;

#define HiPriority 14
#define ValueMax   16
#define StackMax   16

/* pole jsou v Pascalu indexovana od 1: stack [i] -> stack [i-1] */
static card     StkTop;
static StackRec stack [StackMax];

static card     ValTop;
static ExprRec  value [ValueMax];

static bool     present;
static StackRec last;
static card     brackets;

/* ScaleSet: set of 0..15 = [1,2,4,8] */
static bool InScaleSet (longint v)
{
   return v == 1 || v == 2 || v == 4 || v == 8;
}

/* Identifikator delsi nez AlphaLen: v Pascalu ma id delku > AlphaLen
   (id[0] := chr (len)), takze ho SearchTree nikdy nenajde (porovnava se
   jen prvnich AlphaLen znaku a pak delka) a NewIdentif ulozi jmeno
   oriznute na AlphaLen znaku. V C je id oriznute a tento priznak
   rika, ze SearchTree nema najit nic. */
static bool     IdLong;
static alpha id_orig;     /* identifikator, jak byl napsan (ELF objekt) */

/********************************* INPUT **********************************/

/* Zdrojovy text: soubor ModName, nebo text v pameti po castech
   (SetSourceText - knihovna pasmlib.h, cparser); ModName je pak jen
   jmeno pro chybova hlaseni.  Cte se po znacich (SrcGetc, SrcUngetc). */

static const char * const * SrcParts = NULL;  /* casti textu (NULL = soubor) */
static int  SrcCount = 0;                     /* pocet casti */
static int  SrcPart = 0;                      /* aktualni cast */
static long SrcPos = 0;                       /* pozice v ni */

/* Program jako spojovy seznam uzlu (asmlist.h, faze 2): jeden uzel = jedna
   radka.  Instrukce a navesti preklada CompileLineBody (inst.c) primo
   z uzlu (NodeDirect, NodeOperands), ostatni uzly se vytisknou do LineBuf
   (NodeSourceText) a ctou jako text.  LineBuf pro vypis (.lst, .inl) se
   plni vzdy. */

static bool          SrcNodes = false;   /* zdroj = seznam uzlu */
static const AsmNode * SrcFirst = NULL;
static const AsmNode * SrcNext = NULL;   /* dalsi radka */
static const AsmNode * SrcCur = NULL;    /* prave radka */
static bool          SrcCurText = false; /* LineBuf obsahuje tisk SrcCur */
static int           NodeGen = 0;        /* cislo prekladu (platnost AsmLabel.sym) */

static const AsmNode * NodeOps = NULL;   /* operandy instrukce z uzlu (Insymbol, Expression) */
static int           NodePos;            /* poradi symbolu: operand 0, carka, operand 1, ... */

void SetSourceNodes (const AsmNode * first, bool on)
{
   SrcNodes = on;
   SrcFirst = first;
   SrcNext = NULL;
   SrcCur = NULL;
   NodeOps = NULL;
   NodeGen ++;
}

void SetSourceText (const char * const * parts, int count)
{
   SrcParts = parts;
   SrcCount = (parts == NULL) ? 0 : count;
}

static int SrcGetc (void)
{
   if (SrcParts == NULL) return getc (source);
   while (SrcPart < SrcCount)
   {
      const char * t = SrcParts [SrcPart];
      if (t != NULL && t [SrcPos] != 0) return (unsigned char) t [SrcPos ++];
      SrcPart ++;
      SrcPos = 0;
   }
   return EOF;
}

static void SrcUngetc (int c)
/* vraceni posledniho precteneho znaku (c != EOF) */
{
   if (SrcParts == NULL)
   {
      ungetc (c, source);
      return;
   }
   if (SrcPos > 0) SrcPos --;
   else
   {
      /* znak byl posledni v predchozi neprazdne casti */
      do SrcPart --; while (SrcParts [SrcPart] == NULL || SrcParts [SrcPart][0] == 0);
      SrcPos = (long) strlen (SrcParts [SrcPart]) - 1;
   }
}

/* eof (source) jako ve FPC: pravda, kdyz uz v souboru neni zadny znak */
bool SourceEof (void)
{
   if (SrcNodes) return SrcNext == NULL;
   int c = SrcGetc ();
   if (c == EOF) return true;
   SrcUngetc (c);
   return false;
}

/* readln (source, LineBuf) jako ve FPC: radka konci LF, CR nebo CR LF,
   do LineBuf (string [160]) se ulozi nejvyse LineLen znaku, zbytek
   radky se preskoci */
static void ReadLine (void)
{
   int len = 0;
   int c;

   for (;;)
   {
      c = SrcGetc ();
      if (c == EOF || c == '\n') break;
      if (c == '\r')
      {
         c = SrcGetc ();
         if (c != '\n' && c != EOF) SrcUngetc (c);
         break;
      }
      if (len < LineLen) LineBuf [len++] = (char) c;
   }
   LineBuf [len] = 0;
}

void OpenSource (void)
{
   line = 0;
   SrcPart = 0;
   SrcPos = 0;
   if (SrcNodes)
   {
      SrcNext = SrcFirst;
      SrcCur = NULL;
      return;
   }
   if (SrcParts != NULL) return;              /* text v pameti */
   source = fopen (ModName, "rb");
   if (source == NULL) Abort (SrcFault);
}

void CloseSource (void)
{
   if (source != NULL) fclose (source);
   source = NULL;
}

static void NextChar (void)
{
   card len = (card) strlen (LineBuf);

   column++;
   if (column <= len) cc = LineBuf [column - 1];
   else if (column == len + 1) cc = CtrlJ;
   else Abort (Bug);
}

void NodeSourceText (void)
{
   if (SrcCur == NULL) return;
   if (! SrcCurText)
   {
      AsmNodeLine (SrcCur, LineBuf, LineLen + 1);   /* jako ReadLine - nejvyse LineLen znaku */
      SrcCurText = true;
   }
   NodeOps = NULL;
   column = 0;
   NextChar ();
}

const AsmNode * SourceNode (void)
{
   return SrcNodes ? SrcCur : NULL;
}

void NextLine (void)
{
   NodeOps = NULL;
   if (SrcNodes)
   {
      if (SrcNext == NULL) Abort (EndOfFile);
      SrcCur = SrcNext;
      SrcNext = SrcNext->next;
      SrcCurText = false;
      line++;
      LineBuf [0] = 0;
      column = 1;
      cc = CtrlJ;
      start = 1;
      if (lst || pas) NodeSourceText ();
      return;
   }
   if (SourceEof ()) Abort (EndOfFile);
   ReadLine ();
#ifdef list
   printf ("%s\n", LineBuf);
#endif
   line++;
   column = 0;
   NextChar ();
}

/***************************** LEXICAL ANALYSIS ***************************/

static void SetSy (symbol par)
{
   CurSy = par;
   NextChar ();
}

static void StoreChar (card * len)
{
   (*len)++;
   if (*len > NumLen) AsmError (NumTooLong);
   num [*len - 1] = UpCase (cc);
   NextChar ();
}

void NextElement (void)
{
   card len;
   char quote;

   while (cc == ' ') NextChar ();

   if (cc == ';')
      while (! Special (cc)) NextChar ();

   start = column;

   if (Identif1 (cc))
   {
      CurSy = Identif;
      id [0] = UpCase (cc);
      id_orig [0] = cc;
      len = 1;
      NextChar ();
      while (Identif2 (cc))
      {
         len++;
         if (len <= AlphaLen)
         {
            id [len - 1] = UpCase (cc);
            id_orig [len - 1] = cc;
         }
         NextChar ();
      }
      if (len <= AlphaLen)
      {
         id [len] = 0;
         id_orig [len] = 0;
         IdLong = false;
      }
      else
      {
         id [AlphaLen] = 0;
         id_orig [AlphaLen] = 0;
         IdLong = true;
      }
   }

   else if (Number1 (cc))
   {
      CurSy = Number;
      len = 0;
      while (Number2 (cc)) StoreChar (&len);
      if (num [len - 1] == 'R') CurSy = Float;
      num [len] = 0;
      if (Identif2 (cc)) AsmError (BadNum);
   }

   else if (cc == '\'' || cc == '"')
   {
      CurSy = Chars;
      len = 0;
      quote = cc;
      NextChar ();
      while (cc != quote)
      {
         len = len + 1;
         if (len > ChsLen) AsmError (StrTooLong);
         if (Special (cc)) AsmError (QuoteExp);
         chs [len - 1] = cc;
         NextChar ();
      }
      NextChar ();
      chs [len] = 0;
   }

   else if (cc == '[') SetSy (LBracket);
   else if (cc == ']') SetSy (RBracket);
   else if (cc == '(') SetSy (LPar);
   else if (cc == ')') SetSy (RPar);
   else if (cc == '<') SetSy (LAngle);
   else if (cc == '>') SetSy (RAngle);
   else if (cc == '+') SetSy (Plus);
   else if (cc == '-') SetSy (Minus);
   else if (cc == '*') SetSy (Asterisk);
   else if (cc == '/') SetSy (Slash);
   else if (cc == ':') SetSy (Colon);
   else if (cc == ',') SetSy (Comma);
   else if (cc == '=') SetSy (Equal);
   else if (cc == '&') SetSy (Ampersand);
   else if (cc == '%') SetSy (Percent);
   else if (cc == '!') SetSy (Excl);

   else if (cc == CtrlJ) CurSy = EOL;
   else AsmError (UnknownSym);
}

/******************************** INSYMBOL ********************************/

static void NodeSy (void)
/* symboly instrukce z uzlu:  Operand, Comma, Operand, ..., EOL */
{
   int k = NodePos / 2;
   if (NodePos % 2 == 0 && k < NodeOps->nops) CurSy = Operand;
   else if (NodePos % 2 == 1 && k + 1 < NodeOps->nops) CurSy = Comma;
   else CurSy = EOL;
}

void NodeOperands (const AsmNode * n)
{
   NodeOps = n;
   NodePos = 0;
   ans = NULL;
   NodeSy ();
}

void Insymbol (void)
{
   if (NodeOps != NULL)
   {
      ans = NULL;
      if (CurSy != EOL) NodePos ++;
      NodeSy ();
      return;
   }
   NextElement ();
   ans = NULL;

   if (CurSy == Identif)
   {
      if (! IdLong)
      {
         ans = SearchTree (DispUsr, id);
         if (ans == NULL) ans = SearchTree (DispStd, id);
      }
      if (ans == NULL)
      {
         NewIdentif (&ans, id);
         StrSet (ans->orig, sizeof (alpha), id_orig);  /* pro linker (ELF objekt) */
      }
   }
}

static void GetNumber (ExprRec * e)
{
   card len, max, radix;
   uint32_t sum;
   uint64_t sum64;
   char c;
   card i, dig;

   len = (card) strlen (num);
   max = len - 1;
   switch (num [len - 1])
   {
      case 'B':           radix = 2;  break;
      case 'O': case 'Q': radix = 8;  break;
      case 'D':           radix = 10; break;
      case 'H':           radix = 16; break;
      default:
         radix = 10;
         max = len;
         break;
   }

   sum = 0;
   sum64 = 0;
   for (i = 1; i <= max; i++)
   {
      c = num [i - 1];
      if (c >= '0' && c <= '9') dig = (card) (c - '0');
      else if (c >= 'A' && c <= 'F') dig = (card) (c - 'A' + 10);
      else AsmError (BadNum);
      if (dig >= radix) AsmError (BadNum);
      sum = sum * radix + dig;           /* preteceni jako longint v Pascalu */
      sum64 = sum64 * radix + dig;       /* x86-64 */
   }

   *e = zero;
   e->val = (longint) sum;
   e->val64 = (int64_t) sum64;
   Insymbol ();
}

static void GetChars (ExprRec * e)
{
   if (strlen (chs) != 1) AsmError (BadChar);
   *e = zero;
   e->val = (unsigned char) chs [0];
   e->val64 = e->val;
   Insymbol ();
}

static void IdentValue (TreePtr t, ExprRec * e)
/* hodnota identifikatoru (GetIdentif, operand z uzlu) */
{
   ExprSort s;

   s = t->ope.sort;
   if (s == segment || s == group)
   {
      *e        = zero;
      e->sort   = imm;
      e->frame  = t;
      e->target = t;
      e->para   = true;
   }
   else if (s == unknown)
   {
      ForwLabel (t);
      *e = t->ope;
   }
   else *e = t->ope;
}

static void GetIdentif (ExprRec * e)
{
   IdentValue (ans, e);
   Insymbol ();
}

static void GetElement (ExprRec * e)
{
   switch (CurSy)
   {
      case Number:  GetNumber  (e); break;
      case Chars:   GetChars   (e); break;
      case Identif: GetIdentif (e); break;
      default:      AsmError (Syntax);
   }
}

/******************************* OPERATIONS *******************************/

static longint logic (bool b)
{
   if (b) return 0;
   else return MinLong;
}

bool AbsExp (ExprRec * e)
{
   return e->sort == imm && e->target == NULL &&
          e->frame == NULL && e->add == NULL && ! e->para;
}

static bool memo (ExprRec * e)
{
   return e->sort == mem && ! e->para;
}

static bool addr (ExprRec * e)
{
   return (e->sort == mem || e->sort == imm) && ! e->para;
}

card TypeConv (ExprRec e)
{
   if (e.sort != reserved) AsmError (TypeExp);
   switch (e.res)
   {
      case iByte:  return byte;
      case iWord:  return word;
      case iDword: return dword;
      case iFword: return fword;
      case iQword: return qword;
      case iTbyte: return tbyte;
      case iNear:  return near_;
      case iFar:   return far_;
      default:     AsmError (TypeExp);
   }
}

static void UnaryCalc (ExprRec * z, OpTyp op, ExprRec x)
{
   *z = zero;
   switch (op)
   {
      case StOp:
         if (! AbsExp (&x)) AsmError (AbsOpExp);
         if (x.val < 0 || x.val > 7) AsmError (OutOfRange);
         *z = st0;
         z->ceil = (small) x.val;
         break;

      case OffsetOp:
         if (! memo (&x)) AsmError (MemOpExp);
         z->frame  = x.frame;
         z->target = x.target;
         z->val    = x.val;
         z->val64  = x.val;
         z->add    = x.add;
         break;

      case SegOp:
         if (! memo (&x)) AsmError (MemOpExp);
         z->target = x.frame;
         z->frame  = x.frame;
         z->para   = true;
         break;

      case TypeOp:
         if (! memo (&x)) AsmError (MemOpExp);
         switch (x.typ)
         {
            case short_: case near_: z->val = 0xFFFF; break;
            case far_:               z->val = 0xFFFE; break;
            default:                 z->val = x.typ;  break;
         }
         z->val64 = z->val;
         break;

      case ThisOp:
         ThisLabel (z, TypeConv (x));
         break;

      case ShortOp:
         if (! memo (&x) || x.typ != near_) AsmError (NearOpExp);
         *z = x;
         z->typ = short_;
         break;

      default:
         if (! AbsExp (&x)) AsmError (AbsOpExp);
         switch (op)
         {
            case HighOp:  z->val = (longint) (((uint32_t) x.val >> 8) & 255);
                          z->val64 = z->val;
                          break;
            case LowOp:   z->val = x.val & 255;
                          z->val64 = z->val;
                          break;
            case PlusOp:  z->val = x.val;
                          z->val64 = x.val64;
                          break;
            case MinusOp: z->val = (longint) (0u - (uint32_t) x.val);
                          z->val64 = (int64_t) (0u - (uint64_t) x.val64);
                          break;
            case NotOp:   z->val = ~ x.val;
                          z->val64 = ~ x.val64;
                          break;
            default:      AsmError (Bug);
         }
         break;
   }
}

static void ConvReg (ExprRec * r)
{
   Register t;

   if (r->sort == xmm && brackets > 0) AsmError (BadBase);   /* [xmm0] */

   if (r->sort == reg && brackets > 0)
   {
      t = r->rx;
      *r = zero;
      r->sort = mem;
      r->base = t;
   }
}

static void MulReg (ExprRec * m, ExprRec * r, ExprRec * i)
{
   if (r->sort != reg) AsmError (InvOp);
   if (! AbsExp (i) || i->val < 0 || i->val > 15 ||
       ! InScaleSet (i->val)) AsmError (BadScale);
   *m = zero;
   m->sort = mem;
   m->index = r->rx;
   m->scale = (small) i->val;
}

static void BinaryCalc (ExprRec * z, OpTyp op, ExprRec x, ExprRec y)
{
   longint a, b, c;
   int64_t a64, b64, c64;

   switch (op)
   {
      case ColonOp:
         if (! memo (&y)) AsmError (MemOpExp);
         *z = y;
         switch (x.sort)
         {
            case seg: z->pref = x.rx; break;
            case imm:
               if (x.para) z->frame = x.frame;
               else AsmError (SegOpExp);
               break;
            default: AsmError (SegOpExp);
         }
         break;

      case PtrOp:
         if (! addr (&y)) AsmError (MemImmExp);
         *z = y;
         z->sort = mem;
         z->typ = TypeConv (x);
         break;

      case MulOp:
         if (! AbsExp (&x)) MulReg (z, &x, &y);
         else if (! AbsExp (&y)) MulReg (z, &y, &x);
         else
         {
            *z = zero;
            z->val = (longint) ((uint32_t) x.val * (uint32_t) y.val);
            z->val64 = (int64_t) ((uint64_t) x.val64 * (uint64_t) y.val64);
         }
         break;

      case AddOp:
         ConvReg (&x);
         ConvReg (&y);
         if (! addr (&x) || ! addr (&y)) AsmError (MemImmExp);

         if (x.sort == mem) y.sort = mem;

         if (x.typ != any)
         {
            if (y.typ != any) AsmError (InvOp);
            else y.typ = x.typ;
         }

         if (x.pref != None)
         {
            if (y.pref != None) AsmError (InvOp);
            else y.pref = x.pref;
         }

         if (x.base != None)
         {
            if (y.base == None) y.base = x.base;
            else if (y.index != None) AsmError (InvOp);
            else
            {
               y.index = x.base;
               y.scale = 1;
            }
         }

         if (x.index != None)
         {
            if (y.index == None)
            {
               y.index = x.index;
               y.scale = x.scale;
            }
            else if (y.base == None && y.scale == 1)
            {
               y.base = y.index;
               y.index = x.index;
               y.scale = x.scale;
            }
            else if (y.base == None && x.scale == 1) y.base = x.index;
            else AsmError (InvOp);
         }

         if (x.frame != NULL)
         {
            if (y.frame != NULL) AsmError (InvOp);
            else y.frame = x.frame;
         }

         if (x.target != NULL)
         {
            if (y.target != NULL) AsmError (InvOp);
            else y.target = x.target;
         }

         if (x.add != NULL)
         {
            if (y.add != NULL) AsmError (InvOp);
            else y.add = x.add;
         }

         y.val = (longint) ((uint32_t) y.val + (uint32_t) x.val);
         y.val64 = (int64_t) ((uint64_t) y.val64 + (uint64_t) x.val64);

         *z = y;
         break; /* Add */

      case SubOp:
         ConvReg (&x);
         ConvReg (&y);
         if (! addr (&x) || ! addr (&y)) AsmError (MemImmExp);

         if (AbsExp (&y))
         {
            *z = x;
            z->val = (longint) ((uint32_t) z->val - (uint32_t) y.val);
            z->val64 = (int64_t) ((uint64_t) z->val64 - (uint64_t) y.val64);
         }
         else
         {
            if (x.frame != y.frame ||
                x.target != y.target ||
                x.add != y.add) AsmError (InvOp);
            if (x.pref != None || x.base != None || x.index != None ||
                y.pref != None || y.base != None || y.index != None)
               AsmError (InvOp);
            *z = zero;
            z->val = (longint) ((uint32_t) x.val - (uint32_t) y.val);
            z->val64 = z->val;   /* rozdil navesti */
         }
         break;

      default:
         if (! AbsExp (&x) || ! AbsExp (&y)) AsmError (AbsOpExp);
         a = x.val;
         b = y.val;
         a64 = x.val64;
         b64 = y.val64;
         c64 = 0;

         /* shl / shr: ve FPC 32 bitove, pocet posunu mod 32,
            shr je logicky posun;
            soubezne 64bitovy vypocet (val64, pocet posunu mod 64);
            v segmentu USE64 se deleni nulou a porovnani posuzuji podle 64 bitu */
         switch (op)
         {
            case DivOp:
            case ModOp:
               if (x64 ? b64 == 0 : b == 0) AsmError (ZeroDiv);
               if (b == 0) c = 0;
               else if (op == DivOp) c = a / b;
               else c = a % b;
               if (b64 == 0 || (a64 == INT64_MIN && b64 == -1)) c64 = 0;
               else if (op == DivOp) c64 = a64 / b64;
               else c64 = a64 % b64;
               break;

            case ShlOp: c = (longint) ((uint32_t) a << ((uint32_t) b & 31));
                        c64 = (int64_t) ((uint64_t) a64 << ((uint64_t) b64 & 63));
                        break;
            case ShrOp: c = (longint) ((uint32_t) a >> ((uint32_t) b & 31));
                        c64 = (int64_t) ((uint64_t) a64 >> ((uint64_t) b64 & 63));
                        break;
            case AndOp: c = a & b; c64 = a64 & b64; break;
            case OrOp:  c = a | b; c64 = a64 | b64; break;
            case XorOp: c = a ^ b; c64 = a64 ^ b64; break;

            case EqOp:  c = logic (x64 ? a64 == b64 : a == b); c64 = c; break;
            case NeOp:  c = logic (x64 ? a64 != b64 : a != b); c64 = c; break;
            case LtOp:  c = logic (x64 ? a64 <  b64 : a <  b); c64 = c; break;
            case LeOp:  c = logic (x64 ? a64 <= b64 : a <= b); c64 = c; break;
            case GtOp:  c = logic (x64 ? a64 >  b64 : a >  b); c64 = c; break;
            case GeOp:  c = logic (x64 ? a64 >= b64 : a >= b); c64 = c; break;

            default:    AsmError (Bug);
         }
         *z = zero;
         z->val = c;
         z->val64 = c64;
         break; /* else */
   }
}

/******************************* EXPRESSION *******************************/

static void Calc (small p)
{
   while (StkTop > 0 && stack [StkTop - 1].priority <= p)
   {
      StackRec * s = &stack [StkTop - 1];
      if (s->kind == unary)
      {
         if (ValTop == 0) AsmError (Bug);
         UnaryCalc (&value [ValTop - 1], s->op, value [ValTop - 1]);
      }
      else
      {
         if (ValTop < 2) AsmError (Bug);
         BinaryCalc (&value [ValTop - 2], s->op, value [ValTop - 2], value [ValTop - 1]);
         ValTop--;
      }
      StkTop--;
   }
}

/* vnorena procedura SetOp z GetOp - par je parametr GetOp */
static void SetOp (KindTyp par, OpTyp o, small p, KindTyp k)
{
   if (k == par)
   {
      present = true;
      last.op = o;
      last.kind = k;
      last.priority = p;
      Insymbol ();
   }
}

static void GetOp (KindTyp par)
{
   present = false;
   if (CurSy == Identif && ans != NULL && ans->ope.sort == reserved)
   {
      switch (ans->ope.res)
      {
         case iSt:     SetOp (par, StOp,     1, unary ); break;
         case iPtr:    SetOp (par, PtrOp,    4, binary); break;
         case iOffset: SetOp (par, OffsetOp, 4, unary ); break;
         case iSeg:    SetOp (par, SegOp,    4, unary ); break;
         case iType:   SetOp (par, TypeOp,   4, unary ); break;
         case iThis:   SetOp (par, ThisOp,   4, unary ); break;
         case iHigh:   SetOp (par, HighOp,   5, unary ); break;
         case iLow:    SetOp (par, LowOp,    5, unary ); break;
         case iMod:    SetOp (par, ModOp,    7, binary); break;
         case iShl:    SetOp (par, ShlOp,    7, binary); break;
         case iShr:    SetOp (par, ShrOp,    7, binary); break;
         case iEq:     SetOp (par, EqOp,     9, binary); break;
         case iNe:     SetOp (par, NeOp,     9, binary); break;
         case iLt:     SetOp (par, LtOp,     9, binary); break;
         case iLe:     SetOp (par, LeOp,     9, binary); break;
         case iGt:     SetOp (par, GtOp,     9, binary); break;
         case iGe:     SetOp (par, GeOp,     9, binary); break;
         case iNot:    SetOp (par, NotOp,   10, unary ); break;
         case iAnd:    SetOp (par, AndOp,   11, binary); break;
         case iOr:     SetOp (par, OrOp,    12, binary); break;
         case iXor:    SetOp (par, XorOp,   12, binary); break;
         case iShort:  SetOp (par, ShortOp, 14, unary ); break;
         default:      break;
      }
   }
   else
   {
      switch (CurSy)
      {
         case LBracket:
            if (par == unary) SetOp (par, BracketOp, 15, unary);
            else
            {
               present       = true;
               last.op       = AddOp;
               last.priority = 1;
               last.kind     = binary;
               /* No Insymbol */
            }
            break;

         case LPar:     SetOp (par, ParOp,     15, unary ); break;
         case Colon:    SetOp (par, ColonOp,    3, binary); break;
         case Asterisk: SetOp (par, MulOp,      7, binary); break;
         case Slash:    SetOp (par, DivOp,      7, binary); break;

         case Plus:
            if (par == binary) SetOp (par, AddOp,   8, binary);
            else               SetOp (par, PlusOp,  6, unary );
            break;
         case Minus:
            if (par == binary) SetOp (par, SubOp,   8, binary);
            else               SetOp (par, MinusOp, 6, unary );
            break;

         default: break;
      }
   }
}

static void StoreOp (void)
{
   StkTop++;
   if (StkTop > StackMax) AsmError (Overflow);
   stack [StkTop - 1] = last;
}

static void UnarySeq (void)
{
   bool  cont;
   OpTyp left;

   cont = true;
   GetOp (unary);
   while (present && cont)
   {
      if (last.op == BracketOp) brackets++;
      if (last.op == StOp && CurSy != LPar) cont = false;
      else
      {
         StoreOp ();
         GetOp (unary);
      }
   }

   ValTop++;
   if (ValTop > ValueMax) AsmError (Overflow);
   if (cont) GetElement (&value [ValTop - 1]);
   else value [ValTop - 1] = st0;

   while (StkTop > 0 && (CurSy == RBracket || CurSy == RPar))
   {
      if (CurSy == RBracket) left = BracketOp;
      else left = ParOp;
      Insymbol ();
      Calc (HiPriority);

      if (left == BracketOp)
      {
         ConvReg (&value [ValTop - 1]);
         if (value [ValTop - 1].sort == imm) value [ValTop - 1].sort = mem;
         brackets--;
      }

      /* StkTop = 0: Pascal cte stack [0] mimo pole (range checks vypnuty),
         tam nikdy neni BracketOp/ParOp -> chyba (overeno na pasm.bin) */
      if (StkTop == 0 || stack [StkTop - 1].op != left)
      {
         switch (left)
         {
            case BracketOp: AsmError (RBracketExp);
            case ParOp:     AsmError (RParExp);
            default:        break;
         }
      }

      StkTop--;
   }
}

static void BinarySeq (void)
{
   UnarySeq ();
   GetOp (binary);
   while (present)
   {
      Calc (last.priority);
      StoreOp ();
      UnarySeq ();
      GetOp (binary);
   }

   Calc (HiPriority);
   if (StkTop != 0)
   {
      switch (stack [StkTop - 1].op)
      {
         case BracketOp: AsmError (RBracketExp);
         case ParOp:     AsmError (RParExp);
         default:        AsmError (Bug);
      }
   }
}

/***************************** OPERAND Z UZLU *****************************/

/* Operand AsmOp => ExprRec stejnymi operacemi (UnaryCalc, BinaryCalc,
   ConvReg) a ve stejnem poradi, jako kdyby se cetl jeho tisk (asmlist.c,
   Operand):  qword ptr [rbp-16],  [rax+rcx*8+4],  L5,  offset msg, ... */

static TreePtr RegSym [None];
static TreePtr XmmSym [16];
static signed char KeySym [KeyMax + 64];   /* 0 = nezjisteno, 1 = instrukce, -1 = text */
static int     SymGen = 0;

static void NodeCache (void)
{
   if (SymGen == NodeGen) return;
   memset (RegSym, 0, sizeof (RegSym));
   memset (XmmSym, 0, sizeof (XmmSym));
   memset (KeySym, 0, sizeof (KeySym));
   SymGen = NodeGen;
}

static TreePtr StdName (const char * name)
/* identifikator, jak by ho nasel Insymbol, jen kdyz je ve DispStd */
{
   alpha up;
   size_t len = strlen (name);
   if (len == 0 || len > AlphaLen) return NULL;
   for (size_t i = 0; i <= len; i++) up [i] = UpCase (name [i]);
   if (SearchTree (DispUsr, up) != NULL) return NULL;
   return SearchTree (DispStd, up);
}

static TreePtr NodeReg (Register r)
{
   NodeCache ();
   if ((int) r < 0 || r >= None) return NULL;
   if (RegSym [r] == NULL) RegSym [r] = StdName (RegTable [r]);
   return RegSym [r];
}

static TreePtr NodeXmm (int n)
{
   char name [8];
   NodeCache ();
   if (n < 0 || n > 15) return NULL;
   if (XmmSym [n] == NULL)
   {
      snprintf (name, sizeof (name), "XMM%d", n);
      XmmSym [n] = StdName (name);
   }
   return XmmSym [n];
}

static bool NodeKey (keyword k)
/* jmeno instrukce (tisk) najde Insymbol jako stejne klicove slovo */
{
   NodeCache ();
   if ((int) k <= 0 || (int) k >= KeyMax + 64) return false;
   if (KeySym [k] == 0)
   {
      TreePtr t = StdName (AsmInstName (k));
      KeySym [k] = (t != NULL && t->ope.sort == reserved && t->ope.res == k) ? 1 : -1;
   }
   return KeySym [k] > 0;
}

TreePtr NodeLabel (AsmLabel * l)
/* zaznam navesti jako po Insymbol (NewIdentif, orig);  NULL = jmeno nelze
   prelozit primo (delsi nez AlphaLen, jine znaky, klicove slovo, registr) */
{
   if (l->sym_gen == NodeGen) return (TreePtr) l->sym;

   TreePtr t = NULL;
   const char * name = l->name;
   size_t len = strlen (name);
   bool ok = len >= 1 && len <= AlphaLen && Identif1 (name [0]);
   for (size_t i = 1; ok && i < len; i++) ok = Identif2 (name [i]);
   if (ok)
   {
      alpha up;
      for (size_t i = 0; i <= len; i++) up [i] = UpCase (name [i]);
      t = SearchTree (DispUsr, up);
      if (t == NULL && SearchTree (DispStd, up) == NULL)
      {
         NewIdentif (&t, up);
         StrSet (t->orig, sizeof (alpha), name);
      }
   }
   l->sym = t;
   l->sym_gen = NodeGen;
   return t;
}

static bool NodeOpDirect (const AsmOp * o)
{
   switch (o->kind)
   {
      case AO_REG:    return NodeReg (o->reg) != NULL;
      case AO_XMM:    return NodeXmm (o->num) != NULL;
      case AO_IMM:    return true;
      case AO_LABEL:
      case AO_OFFSET: return o->label != NULL && NodeLabel (o->label) != NULL;
      case AO_MEM:
         if (o->label != NULL && NodeLabel (o->label) == NULL) return false;
         if (o->reg != None && NodeReg (o->reg) == NULL) return false;
         if (o->index != None && NodeReg (o->index) == NULL) return false;
         switch (o->size)
         {
            case 0: case 1: case 2: case 4: case 8: case 10: return true;
            default: return false;
         }
      default:        return false;
   }
}

bool NodeDirect (const AsmNode * n)
{
   switch (n->kind)
   {
      case AN_LABEL:
         return n->label != NULL && NodeLabel (n->label) != NULL;

      case AN_INST:
         if (n->prefix != iOther)
         {
            if (n->prefix != iLock && n->prefix != iRep && n->prefix != iRepe &&
                n->prefix != iRepne && n->prefix != iRepnz && n->prefix != iRepz) return false;
            if (! NodeKey (n->prefix)) return false;
         }
         if (! NodeKey (n->inst)) return false;
         if (n->nops < 0 || n->nops > ASM_MAX_OPS) return false;
         for (int i = 0; i < n->nops; i++)
            if (! NodeOpDirect (&n->op [i])) return false;
         return true;

      default:
         return false;
   }
}

static void NodeNumber (uint64_t u, ExprRec * e)
/* cislo bez znamenka jako GetNumber */
{
   *e = zero;
   e->val = (longint) (uint32_t) u;
   e->val64 = (int64_t) u;
}

static void NodeSigned (int64_t v, ExprRec * e)
/* -16 = unarni minus a cislo */
{
   if (v < 0)
   {
      ExprRec x;
      NodeNumber (0u - (uint64_t) v, &x);
      UnaryCalc (e, MinusOp, x);
   }
   else NodeNumber ((uint64_t) v, e);
}

static void NodeDisp (ExprRec * acc, int64_t v)
/* ... +16,  ... -16 */
{
   ExprRec y;
   if (v < 0)
   {
      NodeNumber (0u - (uint64_t) v, &y);
      BinaryCalc (acc, SubOp, *acc, y);
   }
   else
   {
      NodeNumber ((uint64_t) v, &y);
      BinaryCalc (acc, AddOp, *acc, y);
   }
}

static void NodeTerm (ExprRec * acc, bool * first, ExprRec t)
/* scitanec adresy:  [a + b + ...] */
{
   if (*first) *acc = t;
   else BinaryCalc (acc, AddOp, *acc, t);
   *first = false;
}

static void NodeOperand (const AsmOp * o, ExprRec * e)
{
   ExprRec t;

   brackets = 0;
   switch (o->kind)
   {
      case AO_REG:
         IdentValue (NodeReg (o->reg), e);
         break;

      case AO_XMM:
         IdentValue (NodeXmm (o->num), e);
         break;

      case AO_IMM:
         if (o->num == 0) NodeSigned (o->val, e);           /* desitkove, i zaporne */
         else NodeNumber ((uint64_t) o->val, e);            /* bez znamenka, 0...h, ...h */
         break;

      case AO_LABEL:
         IdentValue (NodeLabel (o->label), e);
         if (o->val != 0) NodeDisp (e, o->val);
         break;

      case AO_OFFSET:
         IdentValue (NodeLabel (o->label), &t);
         UnaryCalc (e, OffsetOp, t);
         break;

      case AO_MEM:
      {
         if (o->label != NULL && o->reg == None && o->index == None &&
             o->size == 0 && o->val == 0)
         {
            IdentValue (NodeLabel (o->label), e);     /* lea rsi, msg */
            break;
         }

         ExprRec acc = zero;
         bool first = true;
         brackets = 1;
         if (o->label != NULL)
         {
            IdentValue (NodeLabel (o->label), &t);
            NodeTerm (&acc, &first, t);
         }
         if (o->reg != None)
         {
            IdentValue (NodeReg (o->reg), &t);
            NodeTerm (&acc, &first, t);
         }
         if (o->index != None)
         {
            IdentValue (NodeReg (o->index), &t);
            if (o->scale != 1)
            {
               ExprRec s;
               NodeSigned (o->scale, &s);
               BinaryCalc (&t, MulOp, t, s);
            }
            NodeTerm (&acc, &first, t);
         }
         if (first) NodeSigned (o->val, &acc);
         else if (o->val != 0) NodeDisp (&acc, o->val);

         /* ] */
         ConvReg (&acc);
         if (acc.sort == imm) acc.sort = mem;
         brackets = 0;

         if (o->size == 0) *e = acc;
         else
         {
            ExprRec x = zero;               /* qword ptr */
            x.sort = reserved;
            switch (o->size)
            {
               case 1:  x.res = iByte;  break;
               case 2:  x.res = iWord;  break;
               case 4:  x.res = iDword; break;
               case 8:  x.res = iQword; break;
               default: x.res = iTbyte; break;
            }
            BinaryCalc (e, PtrOp, x, acc);
         }
         break;
      }

      default:
         AsmError (Bug);
   }
}

/******************************* EXPRESSION *******************************/

void Expression (ExprRec * e)
{
   if (NodeOps != NULL)
   {
      /* operand z uzlu */
      if (CurSy != Operand) AsmError (Syntax);
      NodeOperand (&NodeOps->op [NodePos / 2], e);
      Insymbol ();
   }
   else
   {
      StkTop   = 0;
      ValTop   = 0;
      brackets = 0;

      BinarySeq ();
      if (ValTop != 1) AsmError (Bug);
      *e = value [0];
   }

   /* segment USE64: konstanta (i posunuti v adrese) se pocita v 64 bitech,
      val = dolnich 32 bitu z val64 (ne 32bitovy vypocet, napr. 100000000h/10h) */
   if (x64 && (e->sort == imm || e->sort == mem) &&
       e->target == NULL && e->add == NULL && e->frame == NULL && ! e->para)
      e->val = (longint) (uint32_t) (uint64_t) e->val64;
}
