/* CODE.CC */

/* preklad a vystupni soubory (prevedeno z code.pas) */

#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "code.h"

/* aritmetika longint s pretecenim (jako v Pascalu) */

static inline longint LAdd (longint a, longint b)
{
   return (longint) ((uint32_t) a + (uint32_t) b);
}

static inline longint LSub (longint a, longint b)
{
   return (longint) ((uint32_t) a - (uint32_t) b);
}

static inline longint LMul (longint a, longint b)
{
   return (longint) ((uint32_t) a * (uint32_t) b);
}

/* Pascal: write (f, ' ':n) - retezec delky 1 zarovnany vpravo na sirku n */
static void WrSpaces (FILE * f, int n)
{
   if (n < 1) n = 1;
   while (n-- > 0) fputc (' ', f);
}

/***************************** OBJECT MODULES *****************************/

#define THeaderMark 0x80
#define LNamesMark  0x96
#define SegDefMark  0x98 /* 99 */
#define GrpDefMark  0x9A
#define PubDefMark  0x90 /* 91 */
#define ExtDefMark  0x8C
#define LinNumMark  0x94 /* 95 */
#define LeDataMark  0xA0 /* A1 */
#define LiDataMark  0xA2 /* A3 */
#define FixupMark   0x9C /* 9D */
#define ModEndMark  0x8A /* 8B */
#define CommentMark 0x88

#define RecSize  2048
#define BufSize  4096

#define NameSize  127
#define RepMax     16

#define DatLim  1023
#define FixLim  (RecSize - 11)
#define LinLim  (RecSize - 6)

#define k64  (64*1024)
#define k32  (32*1024)

enum DatTyp { enum_, iter };
typedef enum DatTyp DatTyp;

/* Pascal: packed record - do souboru se zapisuje typ (1 bajt),
   len (2 bajty, little-endian) a data [1..len] */
typedef struct
{
   small typ;
   card  len;
   small data [RecSize + 1 + 1];   /* data [1..RecSize+1] */
}
   ObjRec;

typedef struct
{
   card    count;
   card    blocks;
   card    start;
   longint head;
}
   RepRec;

static card      BufLast;
static small     buf [BufSize];        /* Pascal buf [1..BufSize] -> buf [0..] */

static card      RepLevel;
static RepRec    RepStack [RepMax + 1]; /* RepStack [1..RepMax] */

static card      DatStart;
static ObjRec    DatRec;

static ObjRec    FixRec;

static card      LinStart;
static ObjRec    LinRec;

static bool      This32;
static card      ThisInx;
static longint   StartIP;
static ResultPtr ThisRes;

static card      LstPos;
static bool      LstFirst;

static card      PasPos;
static bool      PasFirst;

/********************************* OUTPUT *********************************/

static void OpenBuf (void)
{
   ObjFile = fopen (ObjName, "wb");
   if (ObjFile == NULL) Abort (ObjFault);
   BufLast = 0;
}

static void SendBuf (void)
{
   if (BufLast > 0)
   {
      if (fwrite (buf, 1, BufLast, ObjFile) != BufLast) Abort (ObjFault);
   }
   BufLast = 0;
}

static void CloseBuf (void)
{
   SendBuf ();
   int r = fclose (ObjFile);
   ObjFile = NULL;
   if (r != 0) Abort (ObjFault);
}

static void PrepRec (ObjRec * rec, small head, bool ext)
{
   if (ext) head ++;
   rec->typ = head;
   rec->len = 0;
}

static void PutByte (ObjRec * rec, small n)
{
   if (rec->len + byte > RecSize) Abort (Bug);
   rec->data [rec->len + 1] = n;
   rec->len ++;
}

static void PutWord (ObjRec * rec, card n)
{
   if (rec->len + word > RecSize) Abort (Bug);
   rec->data [rec->len + 1] = (small) (n & 0xFF);
   rec->data [rec->len + 2] = (small) (n >> 8);
   rec->len += word;
}

static void PutDword (ObjRec * rec, longint n)
{
   if (rec->len + dword > RecSize) Abort (Bug);
   uint32_t u = (uint32_t) n;
   rec->data [rec->len + 1] = (small) (u & 0xFF);
   rec->data [rec->len + 2] = (small) ((u >> 8) & 0xFF);
   rec->data [rec->len + 3] = (small) ((u >> 16) & 0xFF);
   rec->data [rec->len + 4] = (small) ((u >> 24) & 0xFF);
   rec->len += dword;
}

static void PutNumber (ObjRec * rec, longint n)
{
   if (rec->typ & 1) PutDword (rec, n);
   else              PutWord  (rec, (card) n);
}

static void PutBytes (ObjRec * rec, const small * bytes, card size)
{
   if (rec->len + size > RecSize) Abort (Bug);
   memmove (&rec->data [rec->len + 1], bytes, size);
   rec->len += size;
}

/* n: ObjStr = string [127] - jmeno se orizne na 127 znaku */
static void PutName (ObjRec * rec, const char * n)
{
   card s = (card) strlen (n);
   if (s > NameSize) s = NameSize;
   s = s + 1;
   if (rec->len + s > RecSize) Abort (Bug);
   rec->data [rec->len + 1] = (small) (s - 1);   /* delkovy bajt */
   memmove (&rec->data [rec->len + 2], n, s - 1);
   rec->len += s;
}

static void PutIndex (ObjRec * rec, ObjInx n)
{
   if (n <= 127) PutByte (rec, (small) n);
   else if (n < k32)
   {
      PutByte (rec, (small) ((n >> 8) | 0x80));
      PutByte (rec, (small) (n & 0xFF));
   }
   else Abort (Bug);
}

static void SendRec (ObjRec * rec)
{
   small sum;
   card  i, size;

   rec->len ++;
   sum = (small) (rec->typ + (rec->len & 0xFF) + (rec->len >> 8));
   for (i = 1; i <= rec->len - 1; i++) sum = (small) (sum + rec->data [i]);
   rec->data [rec->len] = (small) (- sum);
   size = rec->len + 3;

   if (BufLast + size > BufSize) SendBuf ();
   buf [BufLast]     = rec->typ;
   buf [BufLast + 1] = (small) (rec->len & 0xFF);
   buf [BufLast + 2] = (small) (rec->len >> 8);
   memmove (&buf [BufLast + 3], &rec->data [1], rec->len);
   BufLast += size;

   rec->len = 0;
}

static void CheckSize (ObjRec * rec, card add)
{
   if (rec->len + add > RecSize) SendRec (rec);
}

/***************************** OBJECT RECORDS *****************************/

static bool LargeNum (longint n)
{
   return (n < 0) || (n > k64);
}

static void THeader (void)
{
   PrepRec (&DatRec, THeaderMark, false);
   PutName (&DatRec, ModName);
   SendRec (&DatRec);
}

static void LNames (void)
{
   LocPtr ref;

   PrepRec (&DatRec, LNamesMark, false);
   ref = LocFirst;
   while (ref != NULL)
   {
      CheckSize (&DatRec, (card) (strlen (ref->name) + 1));
      PutName (&DatRec, ref->name);
      ref = ref->next;
   }
   SendRec (&DatRec);
}

static void SegDef (TreePtr par)
{
   bool  ext, big;
   small acbp;
   ExprRec * o = &par->ope;

   ext = (o->SegLen < 0) || (o->SegLen > k64);
   if (ext && ! o->Seg32) Abort (Bug);

   big  = o->SegLen == k64;
   acbp = (small) (((int) o->align << 5) + ((int) o->combi << 2) +
                   ((int) big << 1) + (int) o->Seg32);

   PrepRec (&DatRec, SegDefMark, ext);
   PutByte (&DatRec, acbp);
   if (o->align == AbsSeg)
   {
      PutWord (&DatRec, o->FrameNum);
      PutByte (&DatRec, 0);
   }
   PutNumber (&DatRec, o->SegLen);
   PutIndex  (&DatRec, o->SegName);
   PutIndex  (&DatRec, o->ClassName);
   PutIndex  (&DatRec, NoName);
   SendRec   (&DatRec);
}

static void GrpDef (TreePtr par)
{
   ListPtr ref;

   PrepRec  (&DatRec, GrpDefMark, false);
   PutIndex (&DatRec, par->ope.GrpName);
   ref = par->ope.GrpSeq.first;
   while (ref != NULL)
   {
      PutByte  (&DatRec, 0xFF);
      PutIndex (&DatRec, ref->data->ope.SegInx);
      ref = ref->next;
   }
   SendRec (&DatRec);
}

static void PubDef (TreePtr par)
{
   ExprRec * o = &par->ope;

   PrepRec (&DatRec, PubDefMark, LargeNum (o->val));

   PutIndex (&DatRec, 0);
   if (o->frame == NULL)
   {
      PutIndex (&DatRec, 0);
      PutWord  (&DatRec, 0);
   }
   else
   {
      /* with frame^.ope */
      if (o->frame->ope.sort != segment) Abort (Bug);
      else PutIndex (&DatRec, o->frame->ope.SegInx);
   }

   PutName   (&DatRec, par->name);
   PutNumber (&DatRec, o->val);
   PutIndex  (&DatRec, 0);
   SendRec   (&DatRec);
}

static void ExtDef (void)
{
   ListPtr ref;

   ref = ExtHead.first;
   if (ref != NULL)
   {
      PrepRec (&DatRec, ExtDefMark, false);
      while (ref != NULL)
      {
         CheckSize (&DatRec, (card) (strlen (ref->data->name) + 2));
         PutName   (&DatRec, ref->data->name);
         PutIndex  (&DatRec, 0);
         ref = ref->next;
      }
      SendRec (&DatRec);
   }
}

/* v Pascalu se nikde nevola */
MAYBE_UNUSED static void Comment (bool nopurge, bool nolist, small cls, const char * txt)
{
   card i, n;

   PrepRec (&DatRec, CommentMark, false);
   PutWord (&DatRec, (card) (((card) nopurge << 15) + ((card) nolist << 14) + cls));
   n = (card) strlen (txt);
   if (n > NameSize) n = NameSize;   /* txt: ObjStr */
   for (i = 1; i <= n; i++) PutByte (&DatRec, (small) txt [i - 1]);
   SendRec (&DatRec);
}

static void Header (void)
{
   ListPtr ref;

   THeader ();
   LNames ();

   ref = SegHead.first;
   while (ref != NULL)
   {
      SegDef (ref->data);
      ref = ref->next;
   }

   ref = GrpHead.first;
   while (ref != NULL)
   {
      GrpDef (ref->data);
      ref = ref->next;
   }

   ExtDef ();

   ref = PubHead.first;
   while (ref != NULL)
   {
      PubDef (ref->data);
      ref = ref->next;
   }
}

/********************************* FIXUPS *********************************/

static void Convert (TreePtr ref, card * met, ObjInx * inx)
{
   if (ref == NULL) Abort (Bug);

   switch (ref->ope.sort)
   {
      case segment:
         *met = 0;
         *inx = ref->ope.SegInx;
         break;
      case group:
         *met = 1;
         *inx = ref->ope.GrpInx;
         break;
      case mem:
      case imm:
         *met = 2;
         *inx = ref->ope.ExtInx;
         break;
      default:
         Abort (Bug);
   }
   if (*inx == 0) Abort (Bug);
}

/* zapis hodnoty longint jako 'size' bajtu (Pascal: move (FixVal, ..., size));
   pri size > 4 jsou dalsi bajty nulove */
static void LongBytes (longint n, small * b)
{
   uint32_t u = (uint32_t) n;
   memset (b, 0, 8);
   b [0] = (small) (u & 0xFF);
   b [1] = (small) ((u >> 8) & 0xFF);
   b [2] = (small) ((u >> 16) & 0xFF);
   b [3] = (small) ((u >> 24) & 0xFF);
}

/* result: ResultRec - predava se hodnotou (kopie) */
static void PutFixup (ResultRec result)
{
   bool  m, s;
   card  loc;
   card  delta, first;
   card  FrameMet, TargetMet;
   ObjInx FrameInx, TargetInx;
   card  i;

   switch (result.FixSort)
   {
      case FixOfs:
         switch (result.FixWidth)
         {
            case byte:  m = true;  s = false; loc = 0; break;
            case word:  m = true;  s = false; loc = 1; break;
            case dword: m = true;  s = true;  loc = 1; break;
            default:    Abort (Bug);
         }
         break;

      case FixSeg:
         switch (result.FixWidth)
         {
            case word:
            case dword: m = true; s = false; loc = 2; break;
            default:    Abort (Bug);
         }
         break;

      case FixNear:
         switch (result.FixWidth)
         {
            case byte:  m = false; s = false; loc = 0; break;
            case word:  m = false; s = false; loc = 1; break;
            case dword: m = false; s = true;  loc = 1; break;
            default:    Abort (Bug);
         }
         break;

      case FixFar:
         switch (result.FixWidth)
         {
            case dword: m = true;  s = false; loc = 3; break;
            case fword: m = true;  s = true;  loc = 3; break;
            default:    Abort (Bug);
         }
         break;

      default:
         Abort (Bug);
   }

   delta = (card) (DatRec.len - DatStart);
   if (delta > 1023) Abort (Bug);

   first = (card) (0x80 +
                   ((card) m << 6) +
                   ((card) s << 5) +
                   (loc << 2) +
                   ((delta >> 8) & 3));

   PutByte (&FixRec, (small) first);
   PutByte (&FixRec, (small) (delta & 0xFF));

   Convert (result.FixFrame,  &FrameMet,  &FrameInx);
   Convert (result.FixTarget, &TargetMet, &TargetInx);
   if (result.FixVal == 0) TargetMet += 4;

   PutByte (&FixRec, (small) ((FrameMet << 4) + TargetMet));

   PutIndex (&FixRec, FrameInx);
   PutIndex (&FixRec, TargetInx);
   if (TargetMet <= 3) PutNumber (&FixRec, result.FixVal);

   switch (result.FixWidth)
   {
      case byte:  PutByte  (&DatRec, 0); break;
      case word:  PutWord  (&DatRec, 0); break;
      case dword: PutDword (&DatRec, 0); break;
      case fword: for (i = 1; i <= 3; i++) PutWord (&DatRec, 0); break;
   }
}

static void Fix (ResultRec result)
{
   if (result.FixTarget != NULL) PutFixup (result);
   else
   {
      small b [8];
      LongBytes (result.FixVal, b);
      PutBytes (&DatRec, b, result.FixWidth);
   }
}

static void ModEnd (void)
{
   card   FrameMet, TargetMet;
   ObjInx FrameInx, TargetInx;

   if (Entry == NULL)
   {
      PrepRec (&FixRec, ModEndMark, false);
      /* non-main module with no start address */
      PutByte (&FixRec, 1);
   }
   else
   {
      /* with Entry^ */
      PrepRec (&FixRec, ModEndMark, LargeNum (Entry->FixVal));

      /* main module with start address */
      PutByte (&FixRec, 3*64+1);

      Convert (Entry->FixFrame,  &FrameMet,  &FrameInx);
      Convert (Entry->FixTarget, &TargetMet, &TargetInx);
      if (Entry->FixVal == 0) TargetMet += 4;

      PutByte (&FixRec, (small) ((FrameMet << 4) + TargetMet));

      PutIndex (&FixRec, FrameInx);
      PutIndex (&FixRec, TargetInx);
      if (TargetMet <= 3) PutNumber (&FixRec, Entry->FixVal);
   }
   SendRec (&FixRec);
}

/****************************** DATA RECORDS ******************************/

static void PrepDat (DatTyp par)
{
   if (par == enum_) PrepRec (&DatRec, LeDataMark, This32);
   else              PrepRec (&DatRec, LiDataMark, This32);
   PutIndex  (&DatRec, ThisInx);
   PutNumber (&DatRec, ip);
   DatStart = DatRec.len;

   PrepRec (&FixRec, FixupMark, Any386);
}

static void TermDat (void)
{
   if (DatRec.len > DatStart) SendRec (&DatRec);
   if (FixRec.len > 0) SendRec (&FixRec);
}

static void FlushDat (DatTyp par, bool check)
{
   if (check && (RepLevel > 0)) AsmError (DupTooLong);
   TermDat ();
   PrepDat (par);
}

static void PrepLin (void)
{
   PrepRec  (&LinRec, LinNumMark, This32);
   PutIndex (&LinRec, 0);
   PutIndex (&LinRec, ThisInx);
   LinStart = LinRec.len;
}

static void TermLin (void)
{
   if (LinRec.len > LinStart) SendRec (&LinRec);
}

static void FlushLin (void)
{
   TermLin ();
   PrepLin ();
}

static void WriteObj (ResultRec * result)
{
   switch (result->cls)
   {
      case Data:
         if (DatRec.len + result->DataLen > DatLim)
            FlushDat (enum_, true);

         if (RepLevel > 0)
         {
            PutNumber (&DatRec, 1);
            PutWord   (&DatRec, 0);
            PutByte   (&DatRec, result->DataLen);
         }

         PutBytes (&DatRec, &result->DataSeq [1], result->DataLen);
         break;

      case Fixup:
         if ((DatRec.len + result->FixWidth > DatLim) ||
             (FixRec.len > FixLim)) FlushDat (enum_, true);

         if (RepLevel > 0)
         {
            PutNumber (&DatRec, 1);
            PutWord   (&DatRec, 0);
            PutByte   (&DatRec, (small) result->FixWidth);
         }

         Fix (*result);
         break;

      case RepBeg:
         if (RepLevel == 1) FlushDat (iter, false);
         PutNumber (&DatRec, result->count);
         RepStack [RepLevel].start = DatRec.len + 1;
         PutWord  (&DatRec, 0);
         break;

      case RepEnd:
      {
         RepRec * r = &RepStack [RepLevel];
         DatRec.data [r->start]     = (small) (r->blocks & 0xFF);
         DatRec.data [r->start + 1] = (small) (r->blocks >> 8);
         if (RepLevel == 1) FlushDat (enum_, false);
         break;
      }

      case LineEnd:
         if (LinRec.len > LinLim) FlushLin ();
         if (StartIP != ip)
         {
            PutWord   (&LinRec, (card) line);
            PutNumber (&LinRec, StartIP);
         }
         break;

      case NewPos:
         TermDat ();
         TermLin ();
         PrepDat (enum_);
         PrepLin ();
         break;

      default:
         break;
   }
}

static void OpenObj (void)
{
   RepLevel = 0;

   OpenBuf ();
   Header ();

   PrepDat (enum_);
   PrepLin ();
}

static void CloseObj (void)
{
   TermDat ();
   TermLin ();

   ModEnd ();
   CloseBuf ();
}


/***************************** ELF EXECUTABLE ******************************/

/* Spustitelny soubor ELF i386 pro Linux (prepinac -e).

   Vsechny segmenty (musi byt USE32) se ulozi za sebou do jedineho useku,
   ktery jadro nahraje od adresy ElfBase s pravy cteni, zapisu i provadeni.
   Adresy (OFFSET navesti, DD navesti, [navesti]) se dosazuji primo jako
   linearni adresy - linker ani relokace nejsou potreba.  Registry
   segmentu se nepouzivaji (Linux nastavi ploche segmenty).

   Usporadani souboru:
      0       hlavicka ELF (52 bajtu)
      52      programova hlavicka PT_LOAD (32 bajtu)
      84 ...  segmenty v poradi definice, zarovnane podle ALIGN */

/* ELF64 (segmenty USE64, x86-64):
      0       hlavicka ELF (64 bajtu)
      64      programova hlavicka PT_LOAD (56 bajtu)
      120 ... segmenty, nahraji se od adresy 400000h */

#define ElfBase32  0x08048000
#define ElfHead32  (52 + 32)
#define ElfBase64  0x00400000
#define ElfHead64  (64 + 56)

/* ToaruOS (prepinac -T, resp. prostredi PASM_TARGET=toaru): TENTYZ writer,
   jen jina zakladna. Duvod neni esteticky - overeno behem 2026-11-10 na QEMU
   + ISO 2.3.2 s vlastnim ELF64 (toaruos-tools/probe):

   * s zakladnou 0x400000 proces spadne na SIGSEGV jeste pred prvnim syscallem
     (strace ukaze jen execve), tj. nizka adresa se jadru nelibi;
   * 0x3F000000 je adresa, kterou pouziva jejich linker/link.ld a ktera je
     i na staticke binarce /lib/ld.so; s ni probe1 i probe2 probehly vcetne
     write(1) a parametru od shellu.

   Dalsi pozadavky zavadece (kernel/misc/elf64.c, kernel/arch/x86_64/user.c),
   ktere nas output splnuje uz dnes: ET_EXEC (ET_DYN/PIE = -EINVAL,
   elf64.c:276), jen PT_LOAD bez PT_DYNAMIC a PT_INTERP (s PT_DYNAMIC by se
   execnul /lib/ld.so), mapovani po 4 KiB strankach; p_flags ani p_align se
   v zavadeci vubec necitou, vsechno je RWX - proto ani nemuzeme cast obrazce
   oznacit jako neprovaditelnou. e_machine se nekontroluje, entry = e_entry
   (jadro nezada _start ani symboly), bss nulujeme sami.
   Pozn. pro pouziti: syscally s PTR_VALIDATE (napr. open na cestu) vraci
   -EFAULT, lezi-li buffer pod adresou e_entry - data tedy nesmi lezet pod
   zakladnou obrazce. Overeno kontrastnim pokusem (probe2 se sekci .ktmp). */
#define ElfBase64Toaru  0x3F000000
#define ElfHeadMax ElfHead64
#define ElfSegMax  64

static bool    Elf64;                    /* ELF64 - segmenty USE64 */
static bool    ElfObj;                   /* relokovatelny objekt ELF64 (prepinac -c) */

/* relokace objektu ELF64 (Elf64_Rela) */
typedef struct
{
   longint offset;      /* misto v sekci */
   int     type;        /* R_X86_64_... */
   int     sym;         /* index symbolu */
   int64_t addend;
}
   ElfRela;

#define R_X86_64_64     1
#define R_X86_64_PC32   2
#define R_X86_64_PLT32  4
#define R_X86_64_32    10

static ElfRela * RelaTab [ElfSegMax + 1];  /* relokace segmentu [1..ElfSegMax] */
static int       RelaCnt [ElfSegMax + 1];
static int       RelaCap [ElfSegMax + 1];
static longint ElfBase;
static longint ElfHead;

/* ToaruOS (-T): skok na vstupni bod hned za hlavickou, kdyz nejaky segment
   lezi pod vstupnim bodem.  Jadro ToaruOS 2.3.2 odmita ukazatele pod e_entry
   (PTR_INRANGE - napr. open na cestu vrati chybu, write ne; overeno v hostovi
   2026-11-11, toaruos-tools/README.md); s JMP start na nejnizsi adrese
   lezi vsechny segmenty nad e_entry.  Program, ktery zacina kodem se vstupnim
   bodem (probe/toaru64.asm), skok nedostane - obraz zustane bajtove stejny. */
#define ToaruStubLen 5          /* E9 rel32 */
static bool    ToaruStub;

typedef small * ElfPtr;

static ElfPtr  ElfImg [ElfSegMax + 1];   /* obsah segmentu [1..ElfSegMax] */
static longint ElfLen [ElfSegMax + 1];   /* delka segmentu */
static longint ElfAdr [ElfSegMax + 1];   /* adresa segmentu v pameti */
static card    ElfCnt;                   /* pocet alokovanych */
static longint ElfSize;                  /* velikost cele pameti */
static bool    ElfOpen;                  /* ElfFile je otevren */

static longint ElfAlign (AlignTyp a)
{
   switch (a)
   {
      case WordAlign:  return 2;
      case DwordAlign: return 4;
      case ParaAlign:  return 16;
      case PageAlign:  return 4096;
      default:         return 1;
   }
}

/* Windows PE / COFF - dale v souboru (WINDOWS PE A COFF) */
#define PeBase     0x00400000   /* ImageBase (bez relokaci, pevna adresa) */
#define PePage     0x1000       /* SectionAlignment */
static longint PeAddr (TreePtr target);
static void    PeLayout (longint rva);
static void    ClosePe (void);
static void    FreePe (void);
static void    CloseCoffObj (void);

static void FreeElf (void)
{
   card i;

   for (i = 1; i <= ElfCnt; i++)
   {
      free (ElfImg [i]);
      ElfImg [i] = NULL;
      free (RelaTab [i]);
      RelaTab [i] = NULL;
      RelaCnt [i] = 0;
      RelaCap [i] = 0;
   }
   ElfCnt = 0;
   FreePe ();
}

static void OpenElf (void)
{
   ListPtr ref;
   longint adr, a;

   ElfCnt  = 0;
   ElfOpen = false;
   if (SegHead.count > ElfSegMax) Abort (Unsupported);

   ElfObj = eobj;

   /* ELF32 nebo ELF64 - vsechny segmenty musi byt stejne */
   Elf64 = SegHead.first != NULL && SegHead.first->data->ope.Seg64;
   if (ElfObj && SegHead.first != NULL && ! Elf64) Abort (EobjUse64);
   for (ref = SegHead.first; ref != NULL; ref = ref->next)
      if (ref->data->ope.Seg64 != Elf64) Abort (ElfMixed);
   /* Zakladna obrazce: Linux 0x400000 (ELF64) / 0x08048000 (ELF32),
      ToaruOS 0x3F000000 - duvod i mereni u definice ElfBase64Toaru.
      Relokovatelny objekt (-c) zakladnu nema, tu urci az linker. */
   if (ToaruTarget && ! Elf64) Abort (ToaruUse64);
   ElfBase = (ToaruTarget && Elf64) ? ElfBase64Toaru : (Elf64 ? ElfBase64 : ElfBase32);
   ElfHead = Elf64 ? ElfHead64 : ElfHead32;
   if (pe)
   {
      /* PE: hlavicky na 1. strance, kazdy segment je sekce od nove stranky */
      ElfBase = PeBase;
      ElfHead = PePage;
   }

   adr = ElfBase + ElfHead;
   ref = SegHead.first;
   while (ref != NULL)
   {
      ExprRec * o = &ref->data->ope;

      if (! o->Seg32) Abort (ElfUse16);
      if (o->align == AbsSeg) Abort (ElfAbsSeg);
      if (o->SegInx != ElfCnt + 1) Abort (Bug);

      a = pe ? PePage : ElfAlign (o->align);
      adr = LMul (LSub (LAdd (adr, a), 1) / a, a);

      ElfAdr [o->SegInx] = ElfObj ? 0 : adr;    /* objekt: kazda sekce od 0 */
      ElfLen [o->SegInx] = o->SegLen;
      RelaTab [o->SegInx] = NULL;
      RelaCnt [o->SegInx] = 0;
      RelaCap [o->SegInx] = 0;
      ElfImg [o->SegInx] = (ElfPtr) malloc ((size_t) (uint32_t) LAdd (o->SegLen, 1));
      if (ElfImg [o->SegInx] == NULL) Abort (HeapFull);
      memset (ElfImg [o->SegInx], 0, (size_t) (uint32_t) LAdd (o->SegLen, 1));
      ElfCnt = o->SegInx;

      adr = LAdd (adr, o->SegLen);

      ref = ref->next;
   }
   ElfSize = LSub (adr, ElfBase);
   if (pe) PeLayout (ElfSize);   /* importy (thunky, IAT) za segmenty */

   ToaruStub = false;
   if (ToaruTarget && Elf64 && ! ElfObj && Entry != NULL && Entry->FixTarget != NULL &&
       Entry->FixTarget->ope.sort == segment)
   {
      longint v = Entry->FixVal;
      if (Entry->FixAdd != NULL) v = LAdd (v, Entry->FixAdd->ope.val);
      longint start = LAdd (ElfAdr [Entry->FixTarget->ope.SegInx], v);
      card    i;
      for (i = 1; i <= ElfCnt; i++)
         if (ElfLen [i] > 0 && ElfAdr [i] < start) ToaruStub = true;
      if (ToaruStub)
      {
         /* segmenty znovu od adresy za skokem (stejne zarovnani) */
         adr = ElfBase + ElfHead + ToaruStubLen;
         for (ref = SegHead.first; ref != NULL; ref = ref->next)
         {
            ExprRec * o = &ref->data->ope;
            a = ElfAlign (o->align);
            adr = LMul (LSub (LAdd (adr, a), 1) / a, a);
            ElfAdr [o->SegInx] = adr;
            adr = LAdd (adr, o->SegLen);
         }
         ElfSize = LSub (adr, ElfBase);
      }
   }
}

/* zapis do prave prekladaneho segmentu */
static void ElfPut (longint pos, const small * bytes, longint size)
{
   if ((ThisInx < 1) || (ThisInx > ElfCnt) ||
       (pos < 0) || (LAdd (pos, size) > ElfLen [ThisInx])) Abort (Bug);
   memmove (&ElfImg [ThisInx] [pos], bytes, (size_t) size);
}

/* linearni adresa: segment + posunuti
   (pozor: uvnitr 'with target^.ope' by 'val' byla polozka ExprRec) */
static longint ElfAddr (TreePtr target, longint offs)
{
   if (target == NULL)
      return offs;
   else
   {
      if (pe && target->ope.sort != segment)
         return LAdd (PeAddr (target), offs);     /* import z DLL */
      if (target->ope.sort != segment) AsmError (ElfExtrn);
      return LAdd (ElfAdr [target->ope.SegInx], offs);
   }
}

/* index symbolu objektu ELF: 1..SegCnt sekce, potom externi symboly, potom verejne */
static int ElfSym (TreePtr target)
{
   if (target->ope.sort == segment)
      return target->ope.SegInx;
   if (target->ope.ExtInx > 0 && (target->ope.sort == mem || target->ope.sort == imm) &&
       target->ope.target == target)
      return SegHead.count + target->ope.ExtInx;
   AsmError (InvSegRef);
}

static void ElfAddRela (longint offset, int type, int sym, int64_t addend)
{
   if (ThisInx < 1 || ThisInx > ElfCnt) Abort (Bug);
   if (RelaCnt [ThisInx] >= RelaCap [ThisInx])
   {
      int cap = (RelaCap [ThisInx] == 0) ? 64 : 2 * RelaCap [ThisInx];
      ElfRela * p = (ElfRela *) realloc (RelaTab [ThisInx], cap * sizeof (ElfRela));
      if (p == NULL) Abort (HeapFull);
      RelaTab [ThisInx] = p;
      RelaCap [ThisInx] = cap;
   }
   ElfRela * r = &RelaTab [ThisInx] [RelaCnt [ThisInx] ++];
   r->offset = offset;
   r->type   = type;
   r->sym    = sym;
   r->addend = addend;
}

static void WriteElf (ResultRec * res)
{
   longint v, n, i;

   switch (res->cls)
   {
      case Data:
         ElfPut (ip, &res->DataSeq [1], res->DataLen);
         break;

      case Fixup:
      {
         small b [8];

         if (ElfObj && res->FixTarget != NULL)
         {
            /* objekt ELF: misto se vynuluje, adresu doplni linker (Elf64_Rela) */
            int     type = 0;
            int64_t addend = 0;
            if (RepLevel > 0) AsmError (EobjDup);
            switch (res->FixSort)
            {
               case FixOfs:
                  if (res->FixWidth == qword) type = R_X86_64_64;
                  else if (res->FixWidth == dword) type = R_X86_64_32;
                  else AsmError (ElfRange);
                  addend = res->FixVal;
                  break;
               case FixRel:
                  type = R_X86_64_PC32;
                  addend = (int64_t) res->FixVal - res->FixWidth;
                  break;
               case FixNear:
                  if (res->FixWidth != dword) AsmError (JumpTooLong);
                  type = (res->FixTarget->ope.sort == segment) ? R_X86_64_PC32 : R_X86_64_PLT32;
                  addend = (int64_t) res->FixVal - res->FixWidth;
                  break;
               default:
                  AsmError (ElfSegRef);
            }
            ElfAddRela (ip, type, ElfSym (res->FixTarget), addend);
            memset (b, 0, sizeof (b));
            ElfPut (ip, b, res->FixWidth);
            break;
         }

         if (res->Fix64)
         {
            /* 8 bajtu v segmentu USE64 */
            int64_t v64;
            if (res->FixTarget == NULL)
               v64 = res->FixVal64;
            else
            {
               if (res->FixSort != FixOfs) AsmError (ElfSegRef);
               v64 = ElfAddr (res->FixTarget, res->FixVal);
            }
            for (int k = 0; k < 8; k++)
               b [k] = (small) (((uint64_t) v64 >> (8 * k)) & 0xFF);
            ElfPut (ip, b, 8);
            break;
         }

         if (res->FixTarget == NULL)
            v = res->FixVal;
         else
         {
            if (res->FixSort != FixOfs) AsmError (ElfSegRef);
            v = ElfAddr (res->FixTarget, res->FixVal);
            if (((res->FixWidth == byte) && ((v < -128) || (v > 255))) ||
                ((res->FixWidth == word) && ((v < -32768) || (v > 0xFFFF))))
               AsmError (ElfRange);
         }
         LongBytes (v, b);
         ElfPut (ip, b, res->FixWidth);
         break;
      }

      case RepEnd:
      {
         /* with RepStack [RepLevel] */
         RepRec * r = &RepStack [RepLevel];
         /* prvni kopie je hotova, zbyvajici count-1 kopii */
         n = LSub (ip, r->head);
         for (i = 1; i <= (longint) r->count - 1; i++)
            ElfPut (LAdd (r->head, LMul (i, n)), &ElfImg [ThisInx] [r->head], n);
         break;
      }

      default:
         break;
   }
}

static void ElfWrite (const small * bytes, longint size)
{
   if (size > 0)
   {
      if (fwrite (bytes, 1, (size_t) size, ElfFile) != (size_t) size)
         Abort (ElfFault);
   }
}

static void H16 (small * hdr, card ofs, longint n)
{
   hdr [ofs]     = (small) (n & 0xFF);
   hdr [ofs + 1] = (small) (((uint32_t) n >> 8) & 0xFF);
}

static void H32 (small * hdr, card ofs, longint n)
{
   H16 (hdr, ofs, n & 0xFFFF);
   H16 (hdr, ofs + 2, ((uint32_t) n >> 16) & 0xFFFF);
}

static void H64 (small * hdr, card ofs, longint n)
/* 64bitova polozka - adresy jsou mensi nez 2 GB */
{
   H32 (hdr, ofs, n);
   H32 (hdr, ofs + 4, 0);
}

static void CloseElfObj (void);

static void CloseElf (void)
{
   if (ElfObj)
   {
      if (cobj) CloseCoffObj (); else CloseElfObj ();
      return;
   }
   if (pe)
   {
      ClosePe ();
      return;
   }

   small   hdr [ElfHeadMax];
   small   zero [4096];
   longint start, v, pos, gap;
   card    i;

   if (errors)
   {
      FreeElf ();
      return;
   }
   if (Entry == NULL) Abort (ElfNoEntry);

   /* with Entry^ */
   v = Entry->FixVal;
   if (Entry->FixAdd != NULL) v = LAdd (v, Entry->FixAdd->ope.val);
   if ((Entry->FixTarget == NULL) || (Entry->FixTarget->ope.sort != segment))
      Abort (ElfNoEntry);
   start = ElfAddr (Entry->FixTarget, v);

   /* ToaruOS: vstupni bod je skok JMP start za hlavickou (viz ToaruStub) */
   longint target = start;
   if (ToaruStub) start = ElfBase + ElfHead;

   memset (hdr, 0, sizeof (hdr));
   memset (zero, 0, sizeof (zero));

   if (Elf64)
   {
      /* hlavicka ELF64 */
      hdr [0] = 0x7F;  hdr [1] = 'E';
      hdr [2] = 'L';   hdr [3] = 'F';
      hdr [4] = 2;              /* ELFCLASS64 */
      hdr [5] = 1;              /* ELFDATA2LSB */
      hdr [6] = 1;              /* EV_CURRENT */
      H16 (hdr, 16, 2);         /* e_type = ET_EXEC */
      H16 (hdr, 18, 0x3E);      /* e_machine = EM_X86_64 */
      H32 (hdr, 20, 1);         /* e_version */
      H64 (hdr, 24, start);     /* e_entry */
      H64 (hdr, 32, 64);        /* e_phoff */
      H16 (hdr, 52, 64);        /* e_ehsize */
      H16 (hdr, 54, 56);        /* e_phentsize */
      H16 (hdr, 56, 1);         /* e_phnum */

      /* programova hlavicka */
      H32 (hdr, 64, 1);         /* p_type = PT_LOAD */
      H32 (hdr, 68, 7);         /* p_flags = R W X */
      H64 (hdr, 72, 0);         /* p_offset */
      H64 (hdr, 80, ElfBase);   /* p_vaddr */
      H64 (hdr, 88, ElfBase);   /* p_paddr */
      H64 (hdr, 96, ElfSize);   /* p_filesz */
      H64 (hdr, 104, ElfSize);  /* p_memsz */
      H64 (hdr, 112, 0x1000);   /* p_align */
   }
   else
   {
      /* hlavicka ELF32 */
      hdr [0] = 0x7F;  hdr [1] = 'E';
      hdr [2] = 'L';   hdr [3] = 'F';
      hdr [4] = 1;              /* ELFCLASS32 */
      hdr [5] = 1;              /* ELFDATA2LSB */
      hdr [6] = 1;              /* EV_CURRENT */
      H16 (hdr, 16, 2);         /* e_type = ET_EXEC */
      H16 (hdr, 18, 3);         /* e_machine = EM_386 */
      H32 (hdr, 20, 1);         /* e_version */
      H32 (hdr, 24, start);     /* e_entry */
      H32 (hdr, 28, 52);        /* e_phoff */
      H16 (hdr, 40, 52);        /* e_ehsize */
      H16 (hdr, 42, 32);        /* e_phentsize */
      H16 (hdr, 44, 1);         /* e_phnum */

      /* programova hlavicka */
      H32 (hdr, 52, 1);         /* p_type = PT_LOAD */
      H32 (hdr, 56, 0);         /* p_offset */
      H32 (hdr, 60, ElfBase);   /* p_vaddr */
      H32 (hdr, 64, ElfBase);   /* p_paddr */
      H32 (hdr, 68, ElfSize);   /* p_filesz */
      H32 (hdr, 72, ElfSize);   /* p_memsz */
      H32 (hdr, 76, 7);         /* p_flags = R W X */
      H32 (hdr, 80, 0x1000);    /* p_align */
   }

   ElfFile = fopen (ElfName, "wb");
   if (ElfFile == NULL) Abort (ElfFault);
   ElfOpen = true;

   ElfWrite (hdr, ElfHead);
   pos = ElfBase + ElfHead;
   if (ToaruStub)
   {
      small jmp [ToaruStubLen];
      small rel [8];                     /* LongBytes pise 8 bajtu */
      LongBytes (LSub (target, LAdd (pos, ToaruStubLen)), rel);
      jmp [0] = 0xE9;                    /* JMP rel32 */
      memcpy (&jmp [1], rel, 4);
      ElfWrite (jmp, ToaruStubLen);
      pos = LAdd (pos, ToaruStubLen);
   }
   for (i = 1; i <= ElfCnt; i++)
   {
      gap = LSub (ElfAdr [i], pos);
      while (gap > 0)
      {
         if (gap > (longint) sizeof (zero)) v = sizeof (zero); else v = gap;
         ElfWrite (zero, v);
         gap -= v;
      }
      ElfWrite (ElfImg [i], ElfLen [i]);
      pos = LAdd (ElfAdr [i], ElfLen [i]);
   }

   {
      int r = fclose (ElfFile);
      ElfFile = NULL;
      ElfOpen = false;   /* soubor je zavren i pri chybe */
      if (r != 0) Abort (ElfFault);
   }
   ElfOpen = false;

   if (chmod (ElfName, 493) != 0) Abort (ElfFault);  /* rwxr-xr-x */

   FreeElf ();
}

/************************* ELF RELOCATABLE OBJECT ***************************/

/* Relokovatelny objekt ELF64 pro x86-64 (prepinac -c), napr. pro gcc / ld:
     - kazdy segment (USE64) je sekce PROGBITS: segment se jmenem obsahujicim
       CODE nebo TEXT => .text (AX), ostatni => .data (WA); dalsi segmenty
       stejneho druhu .text.<jmeno>, .data.<jmeno>,
     - relokace .rela<sekce>: R_X86_64_64, R_X86_64_32 (OFFSET, DQ, DD),
       R_X86_64_PC32 (adresa relativni k RIP, skok do jineho segmentu),
       R_X86_64_PLT32 (volani / skok na externi symbol),
     - symboly: sekce (lokalni), EXTRN (nedefinovane), PUBLIC (globalni);
       jmeno pro linker je  EXTRN X: NEAR 'jmeno' / PUBLIC X 'jmeno',
       jinak identifikator, jak byl napsan,
     - .note.GNU-stack (zasobnik neni spustitelny). */

typedef struct
{
   small * p;
   size_t  len;
   size_t  cap;
}
   ElfBuf;

static void BufPut (ElfBuf * b, const void * data, size_t n)
{
   if (b->len + n > b->cap)
   {
      size_t cap = (b->cap == 0) ? 4096 : b->cap;
      while (cap < b->len + n) cap *= 2;
      small * p = (small *) realloc (b->p, cap);
      if (p == NULL) Abort (HeapFull);
      b->p = p;
      b->cap = cap;
   }
   if (data != NULL) memcpy (b->p + b->len, data, n);
   else memset (b->p + b->len, 0, n);
   b->len += n;
}

static void BufNum (ElfBuf * b, uint64_t v, int size)   /* little endian */
{
   small t [8];
   for (int i = 0; i < size; i++)
      t [i] = (small) ((v >> (8 * i)) & 0xFF);
   BufPut (b, t, size);
}

static void BufAlign (ElfBuf * b, size_t a)
{
   while (b->len % a != 0) BufPut (b, NULL, 1);
}

static uint32_t BufStr (ElfBuf * b, const char * s)  /* retezec do tabulky, vraci posun */
{
   uint32_t ofs = (uint32_t) b->len;
   BufPut (b, s, strlen (s) + 1);
   return ofs;
}

static void BufPatch (ElfBuf * b, size_t ofs, uint64_t v, int size)
{
   for (int i = 0; i < size; i++)
      b->p [ofs + i] = (small) ((v >> (8 * i)) & 0xFF);
}

static const char * ElfSymName (TreePtr t)
{
   if (t->extname [0] != 0) return t->extname;
   if (t->orig [0] != 0) return t->orig;
   return t->name;
}

static bool ElfCodeSeg (TreePtr seg)
{
   return strstr (seg->name, "CODE") != NULL || strstr (seg->name, "TEXT") != NULL;
}

/* segment EH_FRAME => sekce .eh_frame (CFI pro unwind, viz cmm/instr-pasm.cc) */
static bool ElfEhFrameSeg (TreePtr seg)
{
   return strcmp (seg->name, "EH_FRAME") == 0;
}

/* segment INIT_ARRAY / FINI_ARRAY => sekce .init_array / .fini_array
   (ukazatele DQ OFFSET f na funkce volane pred main / pri ukonceni) */
static uint32_t ElfArraySeg (TreePtr seg)
{
   if (strcmp (seg->name, "INIT_ARRAY") == 0) return 14;   /* SHT_INIT_ARRAY */
   if (strcmp (seg->name, "FINI_ARRAY") == 0) return 15;   /* SHT_FINI_ARRAY */
   return 0;
}

/* sekce ELF (hlavicka Elf64_Shdr) */
static void ElfShdr (ElfBuf * b, uint32_t name, uint32_t type, uint64_t flags,
                     uint64_t offset, uint64_t size, uint32_t link, uint32_t info,
                     uint64_t align, uint64_t entsize)
{
   BufNum (b, name, 4);
   BufNum (b, type, 4);
   BufNum (b, flags, 8);
   BufNum (b, 0, 8);          /* sh_addr */
   BufNum (b, offset, 8);
   BufNum (b, size, 8);
   BufNum (b, link, 4);
   BufNum (b, info, 4);
   BufNum (b, align, 8);
   BufNum (b, entsize, 8);
}

static void ElfSymEnt (ElfBuf * b, uint32_t name, int info, int shndx, uint64_t value)
{
   BufNum (b, name, 4);
   BufNum (b, info, 1);
   BufNum (b, 0, 1);          /* st_other */
   BufNum (b, shndx, 2);
   BufNum (b, value, 8);
   BufNum (b, 0, 8);          /* st_size */
}

static void CloseElfObj (void)
{
   ElfBuf   file   = { NULL, 0, 0 };
   ElfBuf   strtab = { NULL, 0, 0 };
   ElfBuf   shstr  = { NULL, 0, 0 };
   ElfBuf   symtab = { NULL, 0, 0 };
   ElfBuf   shdr   = { NULL, 0, 0 };
   TreePtr  segs [ElfSegMax + 1];
   uint64_t secofs [ElfSegMax + 1];
   uint64_t relofs [ElfSegMax + 1];
   uint32_t secname [ElfSegMax + 1];
   uint32_t relname [ElfSegMax + 1];
   int      n = ElfCnt;
   int      i;

   if (errors)
   {
      FreeElf ();
      return;
   }

   for (ListPtr ref = SegHead.first; ref != NULL; ref = ref->next)
      segs [ref->data->ope.SegInx] = ref->data;

   /* jmena sekci */
   BufStr (&shstr, "");
   bool text_used = false, data_used = false;
   for (i = 1; i <= n; i++)
   {
      bool code = ElfCodeSeg (segs [i]);
      char name [64];
      char lower [AlphaLen + 1];
      int k;
      for (k = 0; segs [i]->name [k] != 0; k++)
         lower [k] = (char) tolower ((unsigned char) segs [i]->name [k]);
      lower [k] = 0;
      if (ElfArraySeg (segs [i]) == 14) strcpy (name, ".init_array");
      else if (ElfArraySeg (segs [i]) == 15) strcpy (name, ".fini_array");
      else if (ElfEhFrameSeg (segs [i])) strcpy (name, ".eh_frame");
      else if (code && ! text_used) { strcpy (name, ".text"); text_used = true; }
      else if (! code && ! data_used) { strcpy (name, ".data"); data_used = true; }
      else snprintf (name, sizeof (name), "%s.%s", code ? ".text" : ".data", lower);
      secname [i] = BufStr (&shstr, name);
      char rname [80];
      snprintf (rname, sizeof (rname), ".rela%s", name);
      relname [i] = BufStr (&shstr, rname);
   }
   uint32_t note_name   = BufStr (&shstr, ".note.GNU-stack");
   uint32_t symtab_name = BufStr (&shstr, ".symtab");
   uint32_t strtab_name = BufStr (&shstr, ".strtab");
   uint32_t shstr_name  = BufStr (&shstr, ".shstrtab");

   /* indexy sekci: 1..n segmenty, potom relokace, note, symtab, strtab, shstrtab */
   int nrel = 0;
   for (i = 1; i <= n; i++)
      if (RelaCnt [i] > 0) nrel ++;
   int note_idx   = n + nrel + 1;
   int symtab_idx = note_idx + 1;
   int strtab_idx = symtab_idx + 1;
   int shstr_idx  = strtab_idx + 1;
   int shnum      = shstr_idx + 1;

   /* symboly: 0, sekce (lokalni), EXTRN, PUBLIC */
   BufStr (&strtab, "");
   ElfSymEnt (&symtab, 0, 0, 0, 0);
   for (i = 1; i <= n; i++)
      ElfSymEnt (&symtab, 0, 3, i, 0);                     /* STB_LOCAL, STT_SECTION */
   int first_global = n + 1;
   for (ListPtr ref = ExtHead.first; ref != NULL; ref = ref->next)
      ElfSymEnt (&symtab, BufStr (&strtab, ElfSymName (ref->data)), 0x10, 0, 0);   /* STB_GLOBAL, UNDEF */
   for (ListPtr ref = PubHead.first; ref != NULL; ref = ref->next)
   {
      TreePtr t = ref->data;
      ExprRec * o = &t->ope;
      uint32_t name = BufStr (&strtab, ElfSymName (t));
      if (o->sort == mem && o->target != NULL && o->target->ope.sort == segment)
         ElfSymEnt (&symtab, name, 0x10, o->target->ope.SegInx, (uint64_t) (int64_t) o->val);
      else if (o->sort == imm && o->target == NULL && o->add == NULL)
         ElfSymEnt (&symtab, name, 0x10, 0xFFF1, (uint64_t) (int64_t) o->val);   /* SHN_ABS */
      else
      {
         StrSet (ErrId, sizeof (alpha), t->name);
         Abort (UndefId);
      }
   }

   /* soubor: hlavicka, obsah sekci, relokace, symboly, retezce, hlavicky sekci */
   BufPut (&file, NULL, 64);
   for (i = 1; i <= n; i++)
   {
      BufAlign (&file, ElfAlign (segs [i]->ope.align));
      secofs [i] = file.len;
      BufPut (&file, ElfImg [i], (size_t) ElfLen [i]);
   }
   for (i = 1; i <= n; i++)
      if (RelaCnt [i] > 0)
      {
         BufAlign (&file, 8);
         relofs [i] = file.len;
         for (int k = 0; k < RelaCnt [i]; k++)
         {
            ElfRela * r = &RelaTab [i] [k];
            BufNum (&file, (uint64_t) (uint32_t) r->offset, 8);
            BufNum (&file, ((uint64_t) r->sym << 32) | (uint32_t) r->type, 8);
            BufNum (&file, (uint64_t) r->addend, 8);
         }
      }
   BufAlign (&file, 8);
   uint64_t symofs = file.len;
   BufPut (&file, symtab.p, symtab.len);
   uint64_t strofs = file.len;
   BufPut (&file, strtab.p, strtab.len);
   uint64_t shstrofs = file.len;
   BufPut (&file, shstr.p, shstr.len);
   BufAlign (&file, 8);
   uint64_t shoff = file.len;

   ElfShdr (&shdr, 0, 0, 0, 0, 0, 0, 0, 0, 0);
   for (i = 1; i <= n; i++)
   {
      bool code = ElfCodeSeg (segs [i]);
      uint32_t arr = ElfArraySeg (segs [i]);
      if (arr != 0)
         ElfShdr (&shdr, secname [i], arr, 3 /* WA */,
                  secofs [i], (uint64_t) ElfLen [i], 0, 0, 8, 8);
      else if (ElfEhFrameSeg (segs [i]))
         ElfShdr (&shdr, secname [i], 1 /* PROGBITS */, 2 /* A */,
                  secofs [i], (uint64_t) ElfLen [i], 0, 0, 8, 0);
      else
         ElfShdr (&shdr, secname [i], 1 /* PROGBITS */, code ? 6 /* AX */ : 3 /* WA */,
                  secofs [i], (uint64_t) ElfLen [i], 0, 0, (uint64_t) ElfAlign (segs [i]->ope.align), 0);
   }
   for (i = 1; i <= n; i++)
      if (RelaCnt [i] > 0)
         ElfShdr (&shdr, relname [i], 4 /* RELA */, 0x40 /* INFO_LINK */,
                  relofs [i], (uint64_t) RelaCnt [i] * 24, symtab_idx, i, 8, 24);
   ElfShdr (&shdr, note_name, 1, 0, shoff, 0, 0, 0, 1, 0);
   ElfShdr (&shdr, symtab_name, 2 /* SYMTAB */, 0, symofs, symtab.len, strtab_idx, first_global, 8, 24);
   ElfShdr (&shdr, strtab_name, 3 /* STRTAB */, 0, strofs, strtab.len, 0, 0, 1, 0);
   ElfShdr (&shdr, shstr_name, 3, 0, shstrofs, shstr.len, 0, 0, 1, 0);
   BufPut (&file, shdr.p, shdr.len);

   /* hlavicka ELF64, ET_REL */
   static const small ident [16] = { 0x7F, 'E', 'L', 'F', 2, 1, 1, 0 };
   memcpy (file.p, ident, 16);
   BufPatch (&file, 16, 1, 2);          /* e_type = ET_REL */
   BufPatch (&file, 18, 0x3E, 2);       /* e_machine = EM_X86_64 */
   BufPatch (&file, 20, 1, 4);          /* e_version */
   BufPatch (&file, 24, 0, 8);          /* e_entry */
   BufPatch (&file, 32, 0, 8);          /* e_phoff */
   BufPatch (&file, 40, shoff, 8);      /* e_shoff */
   BufPatch (&file, 48, 0, 4);          /* e_flags */
   BufPatch (&file, 52, 64, 2);         /* e_ehsize */
   BufPatch (&file, 54, 0, 2);          /* e_phentsize */
   BufPatch (&file, 56, 0, 2);          /* e_phnum */
   BufPatch (&file, 58, 64, 2);         /* e_shentsize */
   BufPatch (&file, 60, shnum, 2);      /* e_shnum */
   BufPatch (&file, 62, shstr_idx, 2);  /* e_shstrndx */

   ElfFile = fopen (EobjName, "wb");
   if (ElfFile == NULL) Abort (ElfFault);
   ElfOpen = true;
   ElfWrite (file.p, (longint) file.len);
   {
      int r = fclose (ElfFile);
      ElfFile = NULL;
      ElfOpen = false;
      if (r != 0) Abort (ElfFault);
   }

   free (file.p);
   free (strtab.p);
   free (shstr.p);
   free (symtab.p);
   free (shdr.p);
   FreeElf ();
}

/*************************** WINDOWS PE A COFF ******************************/

/* Spustitelny soubor PE pro Windows (prepinac -w, soubor .exe)
   ------------------------------------------------------------
   Varianta spustitelneho ELF (-e): stejny druhy pruchod, adresy se dosazuji
   primo (pevna ImageBase 400000h, bez relokaci), jen jine hlavicky a importy.
   USE32 => PE32 (i386), USE64 => PE32+ (x86-64); subsystem konzole.

   Usporadani obrazce (RVA = adresa - ImageBase):
      0       hlavicka MS-DOS (64 bajtu, e_lfanew = 40h), "PE\0\0", hlavicka
              COFF (20), volitelna hlavicka (PE32 224 / PE32+ 240 bajtu),
              hlavicky sekci (40 bajtu)
      1000h   segmenty v poradi definice, kazdy od nove stranky (4 KiB):
              jmeno obsahuje CODE nebo TEXT => .text (R X), jinak .data (R W);
              dalsi segmenty stejneho druhu .<jmeno> (nejvyse 8 znaku)
      ...     .itext - thunky importovanych funkci:  JMP [IAT] (FF 25)
      ...     .idata - importy: popisovace DLL, ILT, IAT, jmena funkci a DLL

   Importy: EXTRN X: NEAR 'kernel32.dll!ExitProcess' (bez "dll!" => kernel32.dll)
      CALL X, JMP X, OFFSET X, [X]   => thunk (jako u linkeru: funkce)
      jmeno __imp_ExitProcess         => polozka IAT (CALL QWORD PTR [X])
   Thunk i IAT jsou sdilene pro vsechny EXTRN se stejnou DLL a funkci.

   Objekt COFF x64 (prepinac -m, soubor .obj)
   ------------------------------------------
   Varianta relokovatelneho objektu ELF64 (-c): stejne relokace z druheho
   pruchodu (RelaTab), jine zapsani:
      R_X86_64_64 => IMAGE_REL_AMD64_ADDR64, R_X86_64_32 => ADDR32,
      R_X86_64_PC32 / PLT32 => REL32;  pricitana hodnota se zapise do sekce
      (COFF nema addend v relokaci; REL32: + 4, misto se pocita od konce pole)
   Sekce: .text, .data, dalsi .text$<jmeno>, .data$<jmeno> (linker je pripoji
   za .text / .data), INIT_ARRAY => .CRT$XCU (konstruktory CRT), EH_FRAME =>
   .eh_frame (jen data).  Jmeno 'dll!funkce' => funkce. */

typedef struct
{
   char     dll [ChsLen + 1];
   char     name [ChsLen + 1];
   int      dllnum;
   longint  iat;        /* RVA polozky IAT */
   longint  thunk;      /* RVA thunku */
}
   PeFunc;

static PeFunc * PeFn;          /* importovane funkce, serazene podle DLL */
static int      PeFnCnt;
static int *    PeExtFn;       /* ExtInx -> index v PeFn */
static bool *   PeExtImp;      /* ExtInx: jmeno __imp_... (polozka IAT) */
static int      PeDllCnt;
static longint  PeThunkRva, PeThunkLen;
static longint  PeIdataRva, PeIdataLen;
static longint  PeIatRva, PeIatLen;
static ElfBuf   PeIdata = { NULL, 0, 0 };
static ElfBuf   PeThunk = { NULL, 0, 0 };

static void FreePe (void)
{
   free (PeFn);     PeFn = NULL;     PeFnCnt = 0;
   free (PeExtFn);  PeExtFn = NULL;
   free (PeExtImp); PeExtImp = NULL;
   free (PeIdata.p); PeIdata.p = NULL; PeIdata.len = 0; PeIdata.cap = 0;
   free (PeThunk.p); PeThunk.p = NULL; PeThunk.len = 0; PeThunk.cap = 0;
   PeDllCnt = 0;
}

static longint PeAlignUp (longint n, longint a)
{
   return LMul (LSub (LAdd (n, a), 1) / a, a);
}

/* 'dll!funkce' => dll, funkce;  bez '!' => kernel32.dll */
static void PeSplit (const char * s, char * dll, char * name)
{
   const char * x = strchr (s, '!');
   if (x == NULL)
   {
      StrSet (dll, ChsLen + 1, "kernel32.dll");
      StrSet (name, ChsLen + 1, s);
   }
   else
   {
      int k = (int) (x - s);
      if (k > ChsLen) k = ChsLen;
      memcpy (dll, s, (size_t) k);
      dll [k] = 0;
      StrSet (name, ChsLen + 1, x + 1);
   }
}

static int PeStrCaseCmp (const char * a, const char * b)
{
   while (*a != 0 && tolower ((unsigned char) *a) == tolower ((unsigned char) *b))
   {
      a ++;
      b ++;
   }
   return tolower ((unsigned char) *a) - tolower ((unsigned char) *b);
}

/* rozvrzeni importu za segmenty (rva = konec segmentu) - vola OpenElf;
   obsah .itext a .idata se pripravi hned, adresy pak zna druhy pruchod */
static void PeLayout (longint rva)
{
   int      next = ExtHead.count;
   int      ptr = Elf64 ? 8 : 4;
   PeFunc * all;
   int      i, k, d;

   FreePe ();
   PeThunkRva = PeAlignUp (rva, PePage);
   PeThunkLen = 0;
   PeIdataRva = PeThunkRva;
   PeIdataLen = 0;
   PeIatRva = 0;
   PeIatLen = 0;
   if (next == 0) return;

   PeExtFn  = (int *) calloc ((size_t) next + 1, sizeof (int));
   PeExtImp = (bool *) calloc ((size_t) next + 1, sizeof (bool));
   all      = (PeFunc *) calloc ((size_t) next, sizeof (PeFunc));
   PeFn     = (PeFunc *) calloc ((size_t) next, sizeof (PeFunc));
   if (PeExtFn == NULL || PeExtImp == NULL || all == NULL || PeFn == NULL) Abort (HeapFull);

   /* funkce v poradi EXTRN (bez opakovani), cislo DLL podle prvniho vyskytu */
   int cnt = 0;
   int * ext2all = (int *) calloc ((size_t) next + 1, sizeof (int));
   if (ext2all == NULL) Abort (HeapFull);
   for (ListPtr ref = ExtHead.first; ref != NULL; ref = ref->next)
   {
      TreePtr t = ref->data;
      char dll [ChsLen + 1];
      char name [ChsLen + 1];
      int  x = t->ope.ExtInx;
      if (x < 1 || x > next) Abort (Bug);
      PeSplit (ElfSymName (t), dll, name);
      if (strncmp (name, "__imp_", 6) == 0)
      {
         PeExtImp [x] = true;
         memmove (name, name + 6, strlen (name + 6) + 1);
      }
      for (k = 0; k < cnt; k++)
         if (PeStrCaseCmp (all [k].dll, dll) == 0 && strcmp (all [k].name, name) == 0) break;
      if (k == cnt)
      {
         StrSet (all [k].dll, ChsLen + 1, dll);
         StrSet (all [k].name, ChsLen + 1, name);
         all [k].dllnum = -1;
         for (i = 0; i < k; i++)
            if (PeStrCaseCmp (all [i].dll, dll) == 0) all [k].dllnum = all [i].dllnum;
         if (all [k].dllnum < 0) all [k].dllnum = PeDllCnt ++;
         cnt ++;
      }
      ext2all [x] = k;
   }

   /* serazeni podle DLL (ILT a IAT kazde DLL jsou souvisle pole) */
   int * all2fn = (int *) calloc ((size_t) cnt, sizeof (int));
   if (all2fn == NULL) Abort (HeapFull);
   PeFnCnt = 0;
   for (d = 0; d < PeDllCnt; d++)
      for (k = 0; k < cnt; k++)
         if (all [k].dllnum == d)
         {
            all2fn [k] = PeFnCnt;
            PeFn [PeFnCnt ++] = all [k];
         }
   for (i = 1; i <= next; i++) PeExtFn [i] = all2fn [ext2all [i]];
   free (all2fn);
   free (ext2all);
   free (all);

   /* .idata: popisovace (DLL + 1) * 20, ILT, IAT (po DLL, s nulou na konci),
      jmena funkci (hint 0, jmeno, zarovnano na 2), jmena DLL */
   PeThunkLen = 8 * PeFnCnt;
   PeIdataRva = PeAlignUp (LAdd (PeThunkRva, PeThunkLen), PePage);
   longint ilt  = (PeDllCnt + 1) * 20;
   longint iat  = ilt + (PeFnCnt + PeDllCnt) * ptr;
   longint hint = iat + (PeFnCnt + PeDllCnt) * ptr;
   PeIatRva = PeIdataRva + iat;
   PeIatLen = (PeFnCnt + PeDllCnt) * ptr;

   BufPut (&PeIdata, NULL, (size_t) hint);
   longint * hints = (longint *) calloc ((size_t) PeFnCnt, sizeof (longint));
   if (hints == NULL) Abort (HeapFull);
   for (k = 0; k < PeFnCnt; k++)
   {
      hints [k] = PeIdataRva + (longint) PeIdata.len;
      BufNum (&PeIdata, 0, 2);
      BufStr (&PeIdata, PeFn [k].name);
      BufAlign (&PeIdata, 2);
   }
   k = 0;
   for (d = 0; d < PeDllCnt; d++)
   {
      longint dllname = PeIdataRva + (longint) PeIdata.len;
      BufStr (&PeIdata, PeFn [k].dll);
      BufPatch (&PeIdata, (size_t) (d * 20), (uint32_t) (PeIdataRva + ilt), 4);   /* OriginalFirstThunk */
      BufPatch (&PeIdata, (size_t) (d * 20 + 12), (uint32_t) dllname, 4);         /* Name */
      BufPatch (&PeIdata, (size_t) (d * 20 + 16), (uint32_t) (PeIdataRva + iat), 4); /* FirstThunk */
      for (; k < PeFnCnt && PeFn [k].dllnum == d; k++)
      {
         PeFn [k].iat = PeIdataRva + iat;
         BufPatch (&PeIdata, (size_t) ilt, (uint32_t) hints [k], ptr);   /* jmenem (bit 31/63 = 0) */
         BufPatch (&PeIdata, (size_t) iat, (uint32_t) hints [k], ptr);   /* loader prepise adresou */
         ilt += ptr;
         iat += ptr;
      }
      ilt += ptr;     /* nula na konci */
      iat += ptr;
   }
   free (hints);
   BufAlign (&PeIdata, 4);
   PeIdataLen = (longint) PeIdata.len;

   /* .itext: JMP [IAT]  - x86-64 relativne k RIP, i386 absolutni adresa */
   for (k = 0; k < PeFnCnt; k++)
   {
      longint at = PeThunkRva + 8 * k;
      PeFn [k].thunk = at;
      BufNum (&PeThunk, 0xFF, 1);
      BufNum (&PeThunk, 0x25, 1);
      if (Elf64) BufNum (&PeThunk, (uint32_t) LSub (PeFn [k].iat, LAdd (at, 6)), 4);
      else       BufNum (&PeThunk, (uint32_t) LAdd (PeBase, PeFn [k].iat), 4);
      BufNum (&PeThunk, 0xCCCC, 2);   /* INT 3 */
   }
}

/* adresa importu: thunk, nebo polozka IAT (__imp_...) */
static longint PeAddr (TreePtr target)
{
   ExprRec * o = &target->ope;
   if (o->ExtInx < 1 || o->target != target || PeExtFn == NULL) AsmError (InvSegRef);
   if (o->sort != mem) AsmError (PeImport);    /* EXTRN X: ABS */
   PeFunc * f = &PeFn [PeExtFn [o->ExtInx]];
   return LAdd (PeBase, PeExtImp [o->ExtInx] ? f->iat : f->thunk);
}

/* hlavicka sekce (obrazec PE i objekt COFF) */
static void PeShdr (ElfBuf * b, const char * name, longint vsize, longint rva,
                    longint rawsize, longint rawptr, longint relptr, int nrel,
                    uint32_t flags)
{
   char n [8];
   memset (n, 0, sizeof (n));
   memcpy (n, name, strlen (name) > 8 ? 8 : strlen (name));
   BufPut (b, n, 8);
   BufNum (b, (uint32_t) vsize, 4);
   BufNum (b, (uint32_t) rva, 4);
   BufNum (b, (uint32_t) rawsize, 4);
   BufNum (b, (uint32_t) rawptr, 4);
   BufNum (b, (uint32_t) relptr, 4);
   BufNum (b, 0, 4);                 /* PointerToLinenumbers */
   BufNum (b, (uint32_t) nrel, 2);
   BufNum (b, 0, 2);                 /* NumberOfLinenumbers */
   BufNum (b, flags, 4);
}

#define PeScnCode   0x60000020    /* CNT_CODE | MEM_EXECUTE | MEM_READ */
#define PeScnData   0xC0000040    /* CNT_INITIALIZED_DATA | MEM_READ | MEM_WRITE */
#define PeScnRdata  0x40000040    /* CNT_INITIALIZED_DATA | MEM_READ */
#define PeFileAlign 0x200

/* jmeno sekce: prvni kodovy segment .text, prvni datovy .data,
   ostatni sep = 0 => .<jmeno> (PE, 8 znaku),  sep = '$' => .text$<jmeno> (COFF) */
static void PeSecName (TreePtr seg, bool * text_used, bool * data_used, char sep,
                       char * name, int size)
{
   bool code = ElfCodeSeg (seg);
   char lower [AlphaLen + 1];
   int k;
   for (k = 0; seg->name [k] != 0; k++)
      lower [k] = (char) tolower ((unsigned char) seg->name [k]);
   lower [k] = 0;
   if (code && ! *text_used) { StrSet (name, size, ".text"); *text_used = true; }
   else if (! code && ! *data_used) { StrSet (name, size, ".data"); *data_used = true; }
   else if (sep == 0)
   {
      StrSet (name, size, ".");
      StrAdd (name, size, lower);
      if (strlen (name) > 8) name [8] = 0;
   }
   else snprintf (name, (size_t) size, "%s%c%s", code ? ".text" : ".data", sep, lower);
}

static void ClosePe (void)
{
   ElfBuf   file = { NULL, 0, 0 };
   ElfBuf   shdr = { NULL, 0, 0 };
   TreePtr  segs [ElfSegMax + 1];
   longint  start, v;
   int      i, nsec = 0;
   int      opt = Elf64 ? 240 : 224;
   bool     text_used = false, data_used = false;
   longint  code_size = 0, data_size = 0, code_base = 0, data_base = 0;

   if (errors)
   {
      FreeElf ();
      return;
   }
   if (Entry == NULL) Abort (ElfNoEntry);
   v = Entry->FixVal;
   if (Entry->FixAdd != NULL) v = LAdd (v, Entry->FixAdd->ope.val);
   if ((Entry->FixTarget == NULL) || (Entry->FixTarget->ope.sort != segment))
      Abort (ElfNoEntry);
   start = ElfAddr (Entry->FixTarget, v);

   for (ListPtr ref = SegHead.first; ref != NULL; ref = ref->next)
      segs [ref->data->ope.SegInx] = ref->data;

   longint hdrsize = PeAlignUp (64 + 4 + 20 + opt + 40 * (ElfCnt + 2), PeFileAlign);
   if (hdrsize > PePage) Abort (Unsupported);
   BufPut (&file, NULL, (size_t) hdrsize);

   /* sekce: segmenty (prazdne se vynechaji), .itext, .idata */
   for (i = 1; i <= ElfCnt + 2; i++)
   {
      char name [16];
      longint rva, len, raw;
      const small * data;
      uint32_t flags;
      if (i <= ElfCnt)
      {
         if (ElfLen [i] == 0) continue;
         bool code = ElfCodeSeg (segs [i]);
         PeSecName (segs [i], &text_used, &data_used, 0, name, sizeof (name));
         rva = LSub (ElfAdr [i], PeBase);
         len = ElfLen [i];
         data = ElfImg [i];
         flags = code ? PeScnCode : PeScnData;
      }
      else if (i == ElfCnt + 1)
      {
         if (PeThunkLen == 0) continue;
         StrSet (name, sizeof (name), ".itext");
         rva = PeThunkRva; len = PeThunkLen; data = PeThunk.p;
         flags = PeScnCode;
      }
      else
      {
         if (PeIdataLen == 0) continue;
         StrSet (name, sizeof (name), ".idata");
         rva = PeIdataRva; len = PeIdataLen; data = PeIdata.p;
         flags = PeScnData;
      }
      raw = PeAlignUp (len, PeFileAlign);
      PeShdr (&shdr, name, len, rva, raw, (longint) file.len, 0, 0, flags);
      BufPut (&file, data, (size_t) len);
      BufPut (&file, NULL, (size_t) (raw - len));
      if (flags == PeScnCode)
      {
         if (code_base == 0) code_base = rva;
         code_size = LAdd (code_size, raw);
      }
      else
      {
         if (data_base == 0) data_base = rva;
         data_size = LAdd (data_size, raw);
      }
      nsec ++;
   }
   longint image = PeAlignUp (PeIdataLen > 0 ? LAdd (PeIdataRva, PeIdataLen) : PeThunkRva, PePage);

   /* hlavicka MS-DOS: jen "MZ" a e_lfanew (program pro DOS se nepise) */
   BufPatch (&file, 0, 0x5A4D, 2);
   BufPatch (&file, 60, 64, 4);
   /* "PE\0\0" a hlavicka COFF */
   BufPatch (&file, 64, 0x00004550, 4);
   BufPatch (&file, 68, Elf64 ? 0x8664 : 0x14C, 2);   /* Machine */
   BufPatch (&file, 70, (uint32_t) nsec, 2);
   BufPatch (&file, 72, 0, 4);                        /* TimeDateStamp (opakovatelny vystup) */
   BufPatch (&file, 84, (uint32_t) opt, 2);           /* SizeOfOptionalHeader */
   BufPatch (&file, 86, Elf64 ? 0x0023 : 0x0103, 2);  /* RELOCS_STRIPPED | EXECUTABLE | LARGE_ADDRESS / 32BIT */

   /* volitelna hlavicka (PE32 / PE32+) */
   size_t h = 88;
   BufPatch (&file, h + 0, Elf64 ? 0x20B : 0x10B, 2); /* Magic */
   BufPatch (&file, h + 2, 1, 1);                     /* MajorLinkerVersion */
   BufPatch (&file, h + 4, (uint32_t) code_size, 4);
   BufPatch (&file, h + 8, (uint32_t) data_size, 4);
   BufPatch (&file, h + 16, (uint32_t) LSub (start, PeBase), 4);   /* AddressOfEntryPoint */
   BufPatch (&file, h + 20, (uint32_t) code_base, 4);
   if (Elf64)
      BufPatch (&file, h + 24, PeBase, 8);            /* ImageBase */
   else
   {
      BufPatch (&file, h + 24, (uint32_t) data_base, 4);
      BufPatch (&file, h + 28, PeBase, 4);
   }
   BufPatch (&file, h + 32, PePage, 4);               /* SectionAlignment */
   BufPatch (&file, h + 36, PeFileAlign, 4);          /* FileAlignment */
   BufPatch (&file, h + 40, 6, 2);                    /* OS 6.0 (Vista a novejsi) */
   BufPatch (&file, h + 48, 6, 2);                    /* Subsystem 6.0 */
   BufPatch (&file, h + 56, (uint32_t) image, 4);     /* SizeOfImage */
   BufPatch (&file, h + 60, (uint32_t) hdrsize, 4);   /* SizeOfHeaders */
   BufPatch (&file, h + 68, 3, 2);                    /* Subsystem = WINDOWS_CUI (konzole) */
   BufPatch (&file, h + 70, 0x8100, 2);               /* NX_COMPAT | TERMINAL_SERVER_AWARE */
   int p = Elf64 ? 8 : 4;
   size_t sz = h + 72;
   BufPatch (&file, sz,         0x100000, p);         /* SizeOfStackReserve */
   BufPatch (&file, sz + p,     0x1000, p);           /* SizeOfStackCommit */
   BufPatch (&file, sz + 2 * p, 0x100000, p);         /* SizeOfHeapReserve */
   BufPatch (&file, sz + 3 * p, 0x1000, p);           /* SizeOfHeapCommit */
   size_t dirs = sz + 4 * p + 8;
   BufPatch (&file, dirs - 4, 16, 4);                 /* NumberOfRvaAndSizes */
   if (PeIdataLen > 0)
   {
      BufPatch (&file, dirs + 1 * 8, (uint32_t) PeIdataRva, 4);           /* import */
      BufPatch (&file, dirs + 1 * 8 + 4, (uint32_t) ((PeDllCnt + 1) * 20), 4);
      BufPatch (&file, dirs + 12 * 8, (uint32_t) PeIatRva, 4);            /* IAT */
      BufPatch (&file, dirs + 12 * 8 + 4, (uint32_t) PeIatLen, 4);
   }
   memcpy (file.p + h + opt, shdr.p, shdr.len);

   ElfFile = fopen (PeName, "wb");
   if (ElfFile == NULL) Abort (ElfFault);
   ElfOpen = true;
   ElfWrite (file.p, (longint) file.len);
   {
      int r = fclose (ElfFile);
      ElfFile = NULL;
      ElfOpen = false;
      if (r != 0) Abort (ElfFault);
   }
   free (file.p);
   free (shdr.p);
   FreeElf ();
}

/* symbol COFF (18 bajtu): jmeno do 8 znaku primo, delsi v tabulce retezcu */
static void CoffSymEnt (ElfBuf * b, ElfBuf * str, const char * name, uint32_t value,
                        int section, int type, int cls, int aux)
{
   if (strlen (name) <= 8)
   {
      char n [8];
      memset (n, 0, sizeof (n));
      memcpy (n, name, strlen (name));
      BufPut (b, n, 8);
   }
   else
   {
      BufNum (b, 0, 4);
      BufNum (b, (uint32_t) str->len, 4);
      BufStr (str, name);
   }
   BufNum (b, value, 4);
   BufNum (b, (uint32_t) (section & 0xFFFF), 2);
   BufNum (b, (uint32_t) type, 2);
   BufNum (b, (uint32_t) cls, 1);
   BufNum (b, (uint32_t) aux, 1);
}

/* jmeno pro linker; 'dll!funkce' (z importu pro -w) => funkce */
static const char * CoffSymName (TreePtr t)
{
   const char * s = ElfSymName (t);
   const char * x = strchr (s, '!');
   return x != NULL ? x + 1 : s;
}

static uint32_t CoffAlign (longint a)
{
   uint32_t k = 1;
   while (a > 1) { a /= 2; k ++; }
   return k << 20;              /* IMAGE_SCN_ALIGN_<a>BYTES */
}

static void CloseCoffObj (void)
{
   ElfBuf   file   = { NULL, 0, 0 };
   ElfBuf   shdr   = { NULL, 0, 0 };
   ElfBuf   symtab = { NULL, 0, 0 };
   ElfBuf   strtab = { NULL, 0, 0 };
   TreePtr  segs [ElfSegMax + 1];
   char     secname [ElfSegMax + 1] [80];
   int      n = ElfCnt;
   int      i, k;
   bool     text_used = false, data_used = false;

   if (errors)
   {
      FreeElf ();
      return;
   }
   for (ListPtr ref = SegHead.first; ref != NULL; ref = ref->next)
      segs [ref->data->ope.SegInx] = ref->data;

   BufNum (&strtab, 0, 4);       /* delka tabulky - doplni se na konci */

   /* symboly: sekce (STATIC + pomocny zaznam), EXTRN, PUBLIC;
      index ELF (ElfSym): 1..n sekce => 2 (i-1), n + ExtInx => 2 n + ExtInx - 1 */
   for (i = 1; i <= n; i++)
   {
      if (ElfArraySeg (segs [i]) == 15) Abort (CoffSect);
      if (ElfArraySeg (segs [i]) == 14) StrSet (secname [i], 80, ".CRT$XCU");
      else if (ElfEhFrameSeg (segs [i])) StrSet (secname [i], 80, ".eh_frame");
      else PeSecName (segs [i], &text_used, &data_used, '$', secname [i], 80);
      if (RelaCnt [i] > 0xFFFF) Abort (Unsupported);

      CoffSymEnt (&symtab, &strtab, secname [i], 0, i, 0, 3 /* STATIC */, 1);
      BufNum (&symtab, (uint32_t) ElfLen [i], 4);      /* Length */
      BufNum (&symtab, (uint32_t) RelaCnt [i], 2);     /* NumberOfRelocations */
      BufNum (&symtab, 0, 2);                          /* NumberOfLinenumbers */
      BufNum (&symtab, 0, 4);                          /* CheckSum */
      BufNum (&symtab, 0, 2);                          /* Number */
      BufNum (&symtab, 0, 1);                          /* Selection */
      BufPut (&symtab, NULL, 3);
   }
   for (ListPtr ref = ExtHead.first; ref != NULL; ref = ref->next)
      CoffSymEnt (&symtab, &strtab, CoffSymName (ref->data), 0, 0, 0, 2 /* EXTERNAL */, 0);
   for (ListPtr ref = PubHead.first; ref != NULL; ref = ref->next)
   {
      TreePtr t = ref->data;
      ExprRec * o = &t->ope;
      if (o->sort == mem && o->target != NULL && o->target->ope.sort == segment)
         CoffSymEnt (&symtab, &strtab, CoffSymName (t), (uint32_t) o->val, o->target->ope.SegInx,
                     ElfCodeSeg (o->target) ? 0x20 /* funkce */ : 0, 2, 0);
      else if (o->sort == imm && o->target == NULL && o->add == NULL)
         CoffSymEnt (&symtab, &strtab, CoffSymName (t), (uint32_t) o->val, -1 /* ABSOLUTE */, 0, 2, 0);
      else
      {
         StrSet (ErrId, sizeof (alpha), t->name);
         Abort (UndefId);
      }
   }
   int nsym = (int) (symtab.len / 18);

   /* hlavicka (20), hlavicky sekci, data a relokace sekci, symboly, retezce */
   BufPut (&file, NULL, 20 + 40 * (size_t) n);
   for (i = 1; i <= n; i++)
   {
      uint32_t flags;
      longint  rawptr, relptr = 0;
      char     name [16];

      /* pricitane hodnoty relokaci do obsahu sekce */
      for (k = 0; k < RelaCnt [i]; k++)
      {
         ElfRela * r = &RelaTab [i] [k];
         int w = (r->type == R_X86_64_64) ? 8 : 4;
         int64_t a = r->addend;
         if (r->type == R_X86_64_PC32 || r->type == R_X86_64_PLT32) a += 4;
         if (r->offset < 0 || r->offset + w > ElfLen [i]) Abort (Bug);
         for (int j = 0; j < w; j++)
            ElfImg [i] [r->offset + j] = (small) (((uint64_t) a >> (8 * j)) & 0xFF);
      }

      BufAlign (&file, 4);
      rawptr = (longint) file.len;
      BufPut (&file, ElfImg [i], (size_t) ElfLen [i]);
      if (RelaCnt [i] > 0)
      {
         BufAlign (&file, 2);
         relptr = (longint) file.len;
         for (k = 0; k < RelaCnt [i]; k++)
         {
            ElfRela * r = &RelaTab [i] [k];
            int sym = (r->sym <= n) ? 2 * (r->sym - 1) : 2 * n + (r->sym - n - 1);
            int type = (r->type == R_X86_64_64) ? 1 /* ADDR64 */ :
                       (r->type == R_X86_64_32) ? 2 /* ADDR32 */ : 4 /* REL32 */;
            BufNum (&file, (uint32_t) r->offset, 4);
            BufNum (&file, (uint32_t) sym, 4);
            BufNum (&file, (uint32_t) type, 2);
         }
      }

      if (ElfArraySeg (segs [i]) == 14 || ElfEhFrameSeg (segs [i]))
         flags = PeScnRdata | CoffAlign (8);
      else
         flags = (ElfCodeSeg (segs [i]) ? PeScnCode : PeScnData) |
                 CoffAlign (ElfAlign (segs [i]->ope.align));
      if (strlen (secname [i]) <= 8)
         StrSet (name, sizeof (name), secname [i]);
      else
      {
         snprintf (name, sizeof (name), "/%u", (unsigned) strtab.len);   /* jmeno v tabulce retezcu */
         BufStr (&strtab, secname [i]);
      }
      PeShdr (&shdr, name, 0, 0, ElfLen [i], ElfLen [i] > 0 ? rawptr : 0, relptr, RelaCnt [i], flags);
   }
   BufAlign (&file, 4);
   longint symptr = (longint) file.len;
   BufPut (&file, symtab.p, symtab.len);
   BufPatch (&strtab, 0, strtab.len, 4);
   BufPut (&file, strtab.p, strtab.len);

   BufPatch (&file, 0, 0x8664, 2);              /* Machine = AMD64 */
   BufPatch (&file, 2, (uint32_t) n, 2);        /* NumberOfSections */
   BufPatch (&file, 4, 0, 4);                   /* TimeDateStamp */
   BufPatch (&file, 8, (uint32_t) symptr, 4);   /* PointerToSymbolTable */
   BufPatch (&file, 12, (uint32_t) nsym, 4);    /* NumberOfSymbols */
   memcpy (file.p + 20, shdr.p, shdr.len);

   ElfFile = fopen (CobjName, "wb");
   if (ElfFile == NULL) Abort (ElfFault);
   ElfOpen = true;
   ElfWrite (file.p, (longint) file.len);
   {
      int r = fclose (ElfFile);
      ElfFile = NULL;
      ElfOpen = false;
      if (r != 0) Abort (ElfFault);
   }
   free (file.p);
   free (shdr.p);
   free (symtab.p);
   free (strtab.p);
   FreeElf ();
}

/***************************** SOURCE LISTING ******************************/

static void WrHex (FILE * f, longint n, card typ)
{
   int i;
   static const char digit [16 + 1] = "0123456789ABCDEF";

   /* FPC: shr na longint je logicky posun, pocet bitu se bere mod 32 */
   for (i = 2 * typ - 1; i >= 0; i--)
      fputc (digit [((uint32_t) n >> ((4 * i) & 31)) & 15], f);
}

static card Range (longint n)
{
   if ((n >= 0) && (n <= 0xFFFF)) return word;
   else                           return dword;
}

static const char AlignTable [DwordAlign + 1][5 + 1] =
   { "Abs", "Byte", "Word", "Para", "Page", "Dword" };

static const char CombiTable [Combi7 + 1][7 + 1] =
   { "Private", "Memory", "Public", "", "", "Stack", "Common", "" };

/* e: ExprRec - v Pascalu hodnotou, zde se nemeni */
static void Disp (const ExprRec * e)
{
   card    i;
   LocPtr  loc;
   bool    ext;

   switch (e->sort)
   {
      case reg:
      case seg:
         fprintf (LstFile, "Register %s", RegTable [e->rx]);
         break;
      case st:
         fprintf (LstFile, "Register ST (%u)", (unsigned int) e->ceil);
         break;
      case cr:
         fprintf (LstFile, "Register CR%u", (unsigned int) e->ceil);
         break;
      case dr:
         fprintf (LstFile, "Register DR%u", (unsigned int) e->ceil);
         break;
      case tr:
         fprintf (LstFile, "Register TR%u", (unsigned int) e->ceil);
         break;

      case mem:
      case imm:
         if (e->sort == imm)       fputs ("Number  ", LstFile);
         else if (e->typ == byte)  fputs ("Byte    ", LstFile);
         else if (e->typ == word)  fputs ("Word    ", LstFile);
         else if (e->typ == dword) fputs ("Dword   ", LstFile);
         else if (e->typ == qword) fputs ("Qword   ", LstFile);
         else if (e->typ == tbyte) fputs ("Tbyte   ", LstFile);
         else if (e->typ == near_) fputs ("Near    ", LstFile);
         else if (e->typ == far_)  fputs ("Far     ", LstFile);

         if (e->pref != None) fprintf (LstFile, "%s: ", RegTable [e->pref]);

         if (e->frame != e->target)
         {
            switch (e->frame->ope.sort)
            {
               case mem:
               case imm:
                  fputs ("SEG ", LstFile);
                  break;
               default:
                  break;
            }
            fprintf (LstFile, "%s: ", e->frame->name);
         }

         ext = false;

         if (e->target != NULL)
         {
            fputs (e->target->name, LstFile);
            switch (e->target->ope.sort)
            {
               case mem:
               case imm:
                  if (e->val != 0)
                  {
                     fputs ("+", LstFile);
                     WrHex (LstFile, e->val, Range (e->val));
                     ext = true;
                  }
                  break;
               case segment:
               case group:
                  fputs (":", LstFile);
                  WrHex (LstFile, e->val, Range (e->val));
                  break;
               default:
                  break;
            }
         }

         if (e->base != None)
            fprintf (LstFile, " [%s]", RegTable [e->base]);

         if (e->index != None)
         {
            fputs (" [", LstFile);
            if (e->scale != 1) fprintf (LstFile, "%u*", (unsigned int) e->scale);
            fputs ("]", LstFile);
         }

         if (ext) fputs (" External", LstFile);
         break;

      case segment:
         fputs ("Segment ", LstFile);
         if (e->align == AbsSeg)
         {
            fputs (" At ", LstFile);
            WrHex (LstFile, e->FrameNum, word);
            fputs ("0", LstFile);
         }
         else
         {
            fputs (AlignTable [e->align], LstFile);
            fprintf (LstFile, " %s", CombiTable [e->combi]);
         }
         if (e->Seg64)      fputs (" Use64", LstFile);
         else if (e->Seg32) fputs (" Use32", LstFile);
         else               fputs (" Use16", LstFile);

         i = 1;
         loc = LocFirst;
         while ((i < e->ClassName) && (loc != NULL))
         {
            i ++;
            loc = loc->next;
         }
         if ((i == e->ClassName) && (loc != NULL) &&
             (loc->name [0] != 0))
            fprintf (LstFile, "'%s'", loc->name);

         fputs (" Size=", LstFile);
         WrHex (LstFile, e->SegLen, Range (e->SegLen));
         break;

      case group:
         fputs ("Group", LstFile);
         break;

      case reserved:
         i = 1;
         while ((i <= KeyMax) && (e->res != KeyTable [i].k))
            i ++;
         if (i <= KeyMax)
            fprintf (LstFile, "Keyword %s", KeyTable [i].n);
         break;

      default:
         break;
   }
}

static void Dictionary (TreePtr ref)
{
   card i;

   if (ref != NULL)
   {
      Dictionary (ref->left);

      fputs (ref->name, LstFile);
      for (i = (card) strlen (ref->name) + 1; i <= AlphaLen; i++) fputc ('.', LstFile);
      WrSpaces (LstFile, 2);
      Disp (&ref->ope);
      fputc ('\n', LstFile);

      Dictionary (ref->right);
   }
}

static void OpenLst (void)
{
   LstFile = fopen (LstName, "w");
   if (LstFile == NULL) Abort (LstFault);
}

static void CloseLst (void)
{
   fputc ('\n', LstFile);
   fputc ('\n', LstFile);
   Dictionary (DispUsr);
   fclose (LstFile);
   LstFile = NULL;
}

#define PosMax   9
#define HexMax  28
#define TxtMin  30

static void LstRequest (card col)
{
   card t;

   if (LstFirst)
   {
      LstFirst = false;
      t = Range (ip);
      WrHex (LstFile, ip, t);
      WrSpaces (LstFile, PosMax - 2 * (int) t);
      LstPos = PosMax;
   }

   if (LstPos + col > HexMax)
   {
      fputc ('\n', LstFile);
      WrSpaces (LstFile, PosMax);
      LstPos = PosMax;
   }
   LstPos += col;
}

static void WriteLst (ResultRec * res)
{
   card i, t;

   switch (res->cls)
   {
      case Data:
         LstRequest (3 * res->DataLen);
         for (i = 1; i <= res->DataLen; i++)
         {
            WrHex (LstFile, res->DataSeq [i], byte);
            fputc (' ', LstFile);
         }
         break;

      case Fixup:
         if (res->Fix64)
         {
            /* 8 bajtu v segmentu USE64 */
            uint64_t u = (res->FixTarget == NULL) ? (uint64_t) res->FixVal64
                                                  : (uint64_t) (int64_t) res->FixVal;
            LstRequest (res->FixTarget == NULL ? 17 : 19);
            fprintf (LstFile, "%08X%08X", (unsigned int) (u >> 32), (unsigned int) (u & 0xFFFFFFFFu));
            if (res->FixTarget == NULL) fputc (' ', LstFile);
            else fputs (" R ", LstFile);
         }
         else if (res->FixTarget == NULL)
         {
            LstRequest (2 * res->FixWidth + 1);
            WrHex (LstFile, res->FixVal, res->FixWidth);
            fputc (' ', LstFile);
         }
         else if (res->FixSort == FixSeg)
         {
            LstRequest (7);
            fputs ("---- R ", LstFile);
         }
         else
         {
            LstRequest (2 * res->FixWidth + 3);
            WrHex (LstFile, res->FixVal, res->FixWidth);
            if ((res->FixTarget->ope.sort == mem) || (res->FixTarget->ope.sort == imm))
               fputs (" E ", LstFile);
            else
               fputs (" R ", LstFile);
         }
         break;

      case RepBeg:
         t = Range (res->count);
         LstRequest (2 * t + 3);
         WrHex (LstFile, res->count, t);
         fputs (" [ ", LstFile);
         break;

      case RepEnd:
         LstRequest (2);
         fputs ("] ", LstFile);
         break;

      case LineBeg:
         LstFirst = true;
         LstPos = 0;
         break;

      case LineEnd:
         WrSpaces (LstFile, TxtMin - (int) LstPos);
         fputs (LineBuf, LstFile);
         fputc ('\n', LstFile);
         break;

      default:
         break;
   }
}

/************************** TURBO PASCAL INLINE ****************************/

#define indent   8
#define CodMax  28
#define SrcMin  30
#define border  64

static void OpenPas (void)
{
   PasFile = fopen (PasName, "w");
   if (PasFile == NULL) Abort (PasFault);
   WrSpaces (PasFile, indent);
   fputs ("inline (\n", PasFile);
   PasFirst = true;
}

static void ClosePas (void)
{
   WrSpaces (PasFile, indent);
   fputs (");\n", PasFile);
   fclose (PasFile);
   PasFile = NULL;
}

static void PasRequest (card col)
{
   if (PasPos + col > CodMax)
   {
      fputc ('\n', PasFile);
      WrSpaces (PasFile, indent + 3);
      PasPos = indent + 3;
   }
   PasPos += col;
}

static void PasFollow (card col)
{
   if (PasFirst)
   {
      PasFirst = false;
      PasRequest (col);
   }
   else
   {
      PasRequest (col + 1);
      fputc ('/', PasFile);
   }
}

static void WritePas (ResultRec * res)
{
   card i, n;

   switch (res->cls)
   {
      case Data:
         for (i = 1; i <= res->DataLen; i++)
         {
            PasFollow (3);
            fputc ('$', PasFile);
            WrHex (PasFile, res->DataSeq [i], byte);
         }
         break;

      case Fixup:
         if ((res->FixTarget == NULL) || (res->FixTarget->ope.sort == segment) ||
             (res->FixTarget->ope.sort == group))
         {
            PasFollow (2 * res->FixWidth + 1);
            fputc ('$', PasFile);
            WrHex (PasFile, res->FixVal, res->FixWidth);
         }
         else
         {
            n = (card) strlen (res->FixTarget->name);
            PasFollow (n + 1);
            switch (res->FixWidth)
            {
               case byte:  fputc ('<', PasFile); break;
               case word:  fputc ('>', PasFile); break;
               case dword: Abort (Bug);
               default:    break;
            }
            fputs (res->FixTarget->name, PasFile);

            if (res->FixVal != 0)
            {
               PasRequest (2 * res->FixWidth + 2);
               fputs ("+$", PasFile);
               WrHex (PasFile, res->FixVal, res->FixWidth);
            }
         }
         break;

      case LineBeg:
         WrSpaces (PasFile, indent + 3);
         PasPos = indent + 3;
         break;

      case LineEnd:
         WrSpaces (PasFile, SrcMin - (int) PasPos);
         fprintf (PasFile, "{ %s", LineBuf);
         n = (card) (SrcMin + 2 + strlen (LineBuf));
         if (n < border) WrSpaces (PasFile, border - n);
         fputs ("}\n", PasFile);
         break;

      default:
         break;
   }
}

/******************************** SECOND PASS ******************************/

static void CompileResBody (void)
{
   ResultRec * r = ThisRes;   /* with ThisRes^ */

   switch (r->cls)
   {
      case Data:
         if (RepLevel > 0) RepStack [RepLevel].blocks ++;
         break;

      case Fixup:
         if (RepLevel > 0) RepStack [RepLevel].blocks ++;

         if (r->FixAdd != NULL)
         {
            r->FixVal = LAdd (r->FixVal, r->FixAdd->ope.val);
            r->FixVal64 = r->FixVal;   /* dopredny odkaz - 32 bitu */
            /* dopredny odkaz: v 1. pruchodu byl cilem
               soucasny segment, navesti muze byt v jinem */
            {
               ExprRec * a = &r->FixAdd->ope;   /* with FixAdd^.ope */
               if ((a->sort == mem) && (a->target != NULL) &&
                   (a->target->ope.sort == segment) &&
                   (r->FixTarget != NULL) &&
                   (r->FixTarget->ope.sort == segment))
                  r->FixTarget = a->target;
            }
            r->FixAdd = NULL;
         }

         if (pe && (r->FixTarget != NULL) &&
             ((r->FixSort == FixNear) || (r->FixSort == FixRel)) &&
             ((r->FixTarget->ope.sort != segment) ||
              (r->FixTarget->ope.SegInx != ThisInx)))
         {
            /* PE: skok / adresa relativni k RIP do jineho segmentu nebo na
               import (thunk, IAT) - linearni adresy jsou zname */
            r->FixVal = LSub (ElfAddr (r->FixTarget, r->FixVal),
                              LAdd (LAdd (ElfAdr [ThisInx], ip), r->FixWidth));
            r->FixFrame = NULL;
            r->FixTarget = NULL;
            if ((r->FixWidth == byte) &&
                ((r->FixVal < -128) || (r->FixVal > 127)))
               AsmError (JumpTooLong);
         }
         else if ((r->FixSort == FixNear) && (r->FixTarget != NULL) &&
                  (r->FixTarget->ope.sort != segment))
         {
            /* externi symbol - fixup zustane pro linker
               (puvodne se cil vzdy pocital uvnitr segmentu) */
         }
         else if (r->FixSort == FixNear && eobj && (r->FixTarget != NULL) &&
                  (r->FixTarget->ope.SegInx != ThisInx))
         {
            /* objekt ELF: skok do jineho segmentu - relokace pro linker */
         }
         else if (r->FixSort == FixNear)
         {
            if (elf && (r->FixTarget != NULL) &&
                (r->FixTarget->ope.SegInx != ThisInx))
               AsmError (ElfNearSeg);
            r->FixVal = LSub (LSub (r->FixVal, r->FixWidth), ip);
            r->FixFrame = NULL;
            r->FixTarget = NULL;
            r->FixAdd = NULL;
            if ((r->FixWidth == byte) &&
                ((r->FixVal < -128) || (r->FixVal > 127)))
               AsmError (JumpTooLong);
         }
         else if (r->FixSort == FixRel)
         {
            /* adresa relativni k RIP (x86-64): cil - konec instrukce
               (FixVal je jiz zmensena o bajty za adresou, viz PutRel) */
            if ((r->FixTarget != NULL) && (r->FixTarget->ope.sort != segment))
            {
               if (! eobj) AsmError (ElfExtrn);    /* objekt ELF: relokace */
            }
            else if ((r->FixTarget == NULL) || (r->FixTarget->ope.SegInx == ThisInx))
            {
               r->FixVal = LSub (LSub (r->FixVal, r->FixWidth), ip);
               r->FixFrame = NULL;
               r->FixTarget = NULL;
            }
            else if (elf)
            {
               /* jiny segment - linearni adresy v ELF */
               r->FixVal = LSub (ElfAddr (r->FixTarget, r->FixVal),
                                 LAdd (LAdd (ElfAdr [ThisInx], ip), r->FixWidth));
               r->FixFrame = NULL;
               r->FixTarget = NULL;
            }
            /* jinak (jen vypis) zustane jako odkaz do jineho segmentu */
         }
         break;

      case RepBeg:
         if (RepLevel > 0) RepStack [RepLevel].blocks ++;
         RepLevel ++;
         if (RepLevel > RepMax) Abort (Bug);
         RepStack [RepLevel].head   = ip;
         RepStack [RepLevel].blocks = 0;
         /* puvodne se 'count' nenastavoval a po DUP se
            ip vratilo na zacatek bloku */
         RepStack [RepLevel].count  = (card) r->count;
         break;

      case LineBeg:
         NextLine ();
         StartIP = ip;
         break;

      case NewPos:
         ThisInx = r->NewInx;
         This32  = r->New32;
         ip      = r->NewOfs;
         StartIP = r->NewOfs;
         break;

      default:
         break;
   }

   if (obj) WriteObj (ThisRes);
   if (elf || eobj) WriteElf (ThisRes);
   if (lst) WriteLst (ThisRes);
   if (pas) WritePas (ThisRes);

   switch (r->cls)
   {
      case Data:
         ip = LAdd (ip, r->DataLen);
         break;
      case Fixup:
         ip = LAdd (ip, r->FixWidth);
         break;
      case RepEnd:
      {
         if (RepLevel == 0) Abort (Bug);
         RepRec * s = &RepStack [RepLevel];
         ip = LAdd (ip, LMul (LSub (ip, s->head), (longint) s->count - 1));
         RepLevel --;
         break;
      }
      default:
         break;
   }
}

/* Pri chybe (AsmError) se pokracuje dalsi radkou, viz Def.Retry */
static void CompileRes (void)
{
   if (setjmp (LineJump) == 0) CompileResBody ();
}

static void SecondPass (void)
{
   This32   = false;
   ThisInx  = 0;
   ip       = 0;
   StartIP  = 0;
   RepLevel = 0;

   OpenSource ();
   if (obj) OpenObj ();
   if (elf || eobj) OpenElf ();
   if (lst) OpenLst ();
   if (pas) OpenPas ();

   ThisRes = FirstRes;
   while (ThisRes != NULL)
   {
      CompileRes ();
      ThisRes = ThisRes->next;
   }

   CloseSource ();
   if (obj) CloseObj ();
   if (elf || eobj) CloseElf ();
   if (lst) CloseLst ();
   if (pas) ClosePas ();
}

/********************************** MAIN ***********************************/

/* Uzavreni souboru po preruseni prekladu (Abort, AsmError);
   chyby pri zavirani se ignoruji, jiz zavrene soubory maji ukazatel NULL */
static void CloseAll (void)
{
   if (source != NULL) { fclose (source); source = NULL; }
   if (obj)
   {
      if (ObjFile != NULL) { fclose (ObjFile); ObjFile = NULL; }
   }
   if (elf || eobj)
   {
      if (ElfOpen)
      {
         if (ElfFile != NULL) { fclose (ElfFile); ElfFile = NULL; }
      }
      ElfOpen = false;
      FreeElf ();
   }
   if (lst)
   {
      if (LstFile != NULL) { fclose (LstFile); LstFile = NULL; }
   }
   if (pas)
   {
      if (PasFile != NULL) { fclose (PasFile); PasFile = NULL; }
   }
}

static void CompileBody (void)
{
   Init ();
   InitRes ();
   FirstPass ();
   if (! errors) SecondPass ();
}

void Compile (void)
{
   ErrLine = 0;
   ErrCol = 0;
   ErrMsg [0] = 0;

   AbortSet = true;
   if (setjmp (AbortJump) == 0) CompileBody ();
   else                         CloseAll ();
   AbortSet = false;
}

/********************************* LINKER ***********************************/

/* Staticky linker ELF64 x86-64 pro ToaruOS (PasmLink v pasmlib.h, pasm -x,
   cmini -L): relokovatelne objekty (pasm -c, gcc -c) a archivy .a (libc.a
   ToaruOS) => samostatny spustitelny ELF64 ET_EXEC - bez ld.so v hostovi.

   Vstupy v poradi prikazove radky linkeru:  crt0.o crti.o program.o libc.a crtn.o
     - objekt se pripoji vzdy, z archivu jen cleny, ktere definuji dosud
       nedefinovany symbol (opakovane, dokud nejaky pribude); archiv se cte
       sam (bez ar x) - libc.a ToaruOS ma dva cleny math.o;
     - symboly: GLOBAL / WEAK, silny symbol dvakrat => chyba, nedefinovany
       slaby => 0; linker sam definuje _GLOBAL_OFFSET_TABLE_,
       __init_array_start / __init_array_end (pre_main z libc.a je vola),
       __bss_start, _edata, _end, end.

   Obrazec (jako pasm -e -T: jedina PT_LOAD R W X od 0x3F000000, bez sekci):
       3F000000  hlavicka ELF64 + programova hlavicka (120 bajtu)
       3F000080  .text (crt0.o prvni => _start je nejnize)  .init  .fini
                 .rodata*  .data*  pole konstruktoru (.init_array*, .ctors* -
                 nejdriv z archivu = libc, pak z objektu programu)  GOT
                 .bss (jen p_memsz - jadro ToaruOS doplni nuly)
   Nic nesmi lezet pod e_entry: jadro ToaruOS odmita ukazatele pod vstupnim
   bodem (PTR_INRANGE) a stdio libc pak write opakuje do nekonecna - zmereno
   s GNU ld, ktery dava .rodata pred .text (README toaruos-tools §5g).
   .eh_frame, .comment, .note* se vynechaji.

   Relokace: R_X86_64_64, PC32, PLT32, 32, 32S, PC64, GOTPCREL, GOTPCRELX,
   REX_GOTPCRELX (GOT = tabulka 8bajtovych adres, jedna polozka na symbol). */

#define LnkBase    0x3F000000ULL
#define LnkHead    128               /* hlavicka + PT_LOAD (120), zarovnano */
#define LnkMaxSym  8192

typedef struct
{
   const small * data;   /* cely objekt (patri souboru) */
   size_t   size;
   int      shnum;
   size_t   shoff;
   int      symsec;       /* index .symtab, 0 = zadna */
   const char * name;     /* jmeno pro hlaseni */
   bool     member;       /* clen archivu (libc) */
   uint64_t * secaddr;    /* adresa sekce ve vystupu, 0 = vynechana */
   int      * symglob;    /* index globalniho symbolu, -1 = lokalni */
}
   LnkObj;

typedef struct
{
   const char * name;
   bool     defined;
   bool     weak;
   int      obj;          /* definujici objekt, -1 = linker / absolutni */
   int      sec;          /* sekce v objektu (SHN_ABS = 0xFFF1) */
   uint64_t value;
   int      got;          /* polozka GOT + 1, 0 = zadna */
}
   LnkSym;

typedef struct
{
   const small * data;    /* soubor archivu (cely) */
   size_t   size;
   size_t * moff;         /* zacatek clenu (ELF) */
   size_t * msize;
   bool   * used;
   int      count;
}
   LnkArch;

static LnkObj  * LnkObjs;
static int       LnkObjCnt, LnkObjCap;
static LnkSym  * LnkSyms;
static int       LnkSymCnt;
static int       LnkGotCnt;
static small * * LnkFiles;           /* nactene soubory - uvolni se na konci */
static int       LnkFileCnt, LnkFileCap;
static jmp_buf   LnkJump;
static char      LnkMsg [512];

static void LnkError (const char * text, const char * what)
{
   snprintf (LnkMsg, sizeof (LnkMsg), "%s%s%s", text, what != NULL ? ": " : "", what != NULL ? what : "");
   longjmp (LnkJump, 1);
}

static uint64_t LnkRd (const small * p, int n)        /* little endian */
{
   uint64_t v = 0;
   int i;
   for (i = n - 1; i >= 0; i--) v = (v << 8) | p [i];
   return v;
}

static void LnkWr (small * p, uint64_t v, int n)
{
   int i;
   for (i = 0; i < n; i++) p [i] = (small) ((v >> (8 * i)) & 0xFF);
}

static void * LnkAlloc (size_t n)
{
   void * p = calloc (1, n > 0 ? n : 1);
   if (p == NULL) LnkError ("No more memory", NULL);
   return p;
}

static small * LnkReadFile (const char * name, size_t * size)
{
   FILE * f = fopen (name, "rb");
   long   n;
   small * p;
   if (f == NULL) LnkError ("Cannot open", name);
   fseek (f, 0, SEEK_END);
   n = ftell (f);
   fseek (f, 0, SEEK_SET);
   p = (small *) LnkAlloc ((size_t) n + 1);
   if (n > 0 && fread (p, 1, (size_t) n, f) != (size_t) n)
   {
      fclose (f);
      LnkError ("Cannot read", name);
   }
   fclose (f);
   if (LnkFileCnt >= LnkFileCap)
   {
      LnkFileCap = LnkFileCap == 0 ? 16 : 2 * LnkFileCap;
      LnkFiles = (small * *) realloc (LnkFiles, (size_t) LnkFileCap * sizeof (small *));
      if (LnkFiles == NULL) LnkError ("No more memory", NULL);
   }
   LnkFiles [LnkFileCnt ++] = p;
   *size = (size_t) n;
   return p;
}

/* hlavicka sekce i objektu o */
static const small * LnkShdr (LnkObj * o, int i)
{
   return o->data + o->shoff + (size_t) i * 64;
}

static const char * LnkSecName (LnkObj * o, int i)
{
   const small * sh = LnkShdr (o, i);
   int shstr = (int) LnkRd (o->data + 62, 2);
   const small * st = LnkShdr (o, shstr);
   return (const char *) (o->data + LnkRd (st + 24, 8) + LnkRd (sh, 4));
}

/* symbol k objektu o: ukazatel na Elf64_Sym a jeho jmeno */
static const small * LnkSymEnt (LnkObj * o, int k)
{
   const small * sh = LnkShdr (o, o->symsec);
   return o->data + LnkRd (sh + 24, 8) + (size_t) k * 24;
}

static int LnkSymCount (LnkObj * o)
{
   if (o->symsec == 0) return 0;
   return (int) (LnkRd (LnkShdr (o, o->symsec) + 32, 8) / 24);
}

static const char * LnkSymName (LnkObj * o, int k)
{
   const small * sh = LnkShdr (o, o->symsec);
   const small * st = LnkShdr (o, (int) LnkRd (sh + 40, 4));          /* sh_link => strtab */
   return (const char *) (o->data + LnkRd (st + 24, 8) + LnkRd (LnkSymEnt (o, k), 4));
}

static int LnkFind (const char * name)
{
   int i;
   for (i = 0; i < LnkSymCnt; i++)
      if (strcmp (LnkSyms [i].name, name) == 0) return i;
   return -1;
}

static int LnkGlobal (const char * name)                 /* najit nebo zalozit */
{
   int i = LnkFind (name);
   if (i >= 0) return i;
   if (LnkSymCnt >= LnkMaxSym) LnkError ("Too many symbols", NULL);
   i = LnkSymCnt ++;
   LnkSyms [i].name = name;
   LnkSyms [i].defined = false;
   LnkSyms [i].weak = false;
   LnkSyms [i].obj = -1;
   LnkSyms [i].sec = 0;
   LnkSyms [i].value = 0;
   LnkSyms [i].got = 0;
   return i;
}

static void LnkCheckElf (const small * p, size_t n, const char * name)
{
   if (n < 64 || p [0] != 0x7F || p [1] != 'E' || p [2] != 'L' || p [3] != 'F')
      LnkError ("Not an ELF object", name);
   if (p [4] != 2 || p [5] != 1 || LnkRd (p + 16, 2) != 1 || LnkRd (p + 18, 2) != 0x3E)
      LnkError ("Not an ELF64 x86-64 relocatable object", name);
}

/* pripojit objekt: globalni symboly do tabulky */
static void LnkAddObj (const small * p, size_t n, const char * name, bool member)
{
   LnkObj * o;
   int i, cnt;

   LnkCheckElf (p, n, name);
   if (LnkObjCnt >= LnkObjCap)
   {
      LnkObjCap = LnkObjCap == 0 ? 64 : 2 * LnkObjCap;
      LnkObjs = (LnkObj *) realloc (LnkObjs, (size_t) LnkObjCap * sizeof (LnkObj));
      if (LnkObjs == NULL) LnkError ("No more memory", NULL);
   }
   o = &LnkObjs [LnkObjCnt];
   o->data = p;
   o->size = n;
   o->name = name;
   o->member = member;
   o->shoff = (size_t) LnkRd (p + 40, 8);
   o->shnum = (int) LnkRd (p + 60, 2);
   o->symsec = 0;
   for (i = 1; i < o->shnum; i++)
      if (LnkRd (LnkShdr (o, i) + 4, 4) == 2) o->symsec = i;       /* SHT_SYMTAB */
   o->secaddr = (uint64_t *) LnkAlloc ((size_t) o->shnum * sizeof (uint64_t));
   cnt = LnkSymCount (o);
   o->symglob = (int *) LnkAlloc ((size_t) (cnt + 1) * sizeof (int));
   for (i = 0; i < cnt; i++)
   {
      const small * s = LnkSymEnt (o, i);
      int bind = s [4] >> 4;
      int shndx = (int) LnkRd (s + 6, 2);
      o->symglob [i] = -1;
      if (i == 0 || (bind != 1 && bind != 2)) continue;           /* GLOBAL, WEAK */
      {
         int g = LnkGlobal (LnkSymName (o, i));
         LnkSym * y = &LnkSyms [g];
         o->symglob [i] = g;
         if (shndx == 0) continue;                                  /* SHN_UNDEF */
         if (shndx == 0xFFF2) LnkError ("COMMON symbols not supported", y->name);
         if (y->defined && ! (bind == 2) && ! y->weak)
            LnkError ("Duplicate symbol", y->name);
         if (y->defined && (bind == 2 || ! y->weak)) continue;      /* silny / prvni slaby zustava */
         y->defined = true;
         y->weak = (bind == 2);
         y->obj = LnkObjCnt;
         y->sec = shndx;
         y->value = LnkRd (s + 8, 8);
      }
   }
   LnkObjCnt ++;
}

/* definuje clen archivu (objekt p) nejaky dosud nedefinovany (silne odkazovany) symbol? */
static bool LnkMemberNeeded (const small * p, size_t n)
{
   LnkObj t;
   int i, cnt;
   if (n < 64 || p [0] != 0x7F || p [1] != 'E') return false;
   t.data = p;
   t.size = n;
   t.shoff = (size_t) LnkRd (p + 40, 8);
   t.shnum = (int) LnkRd (p + 60, 2);
   t.symsec = 0;
   for (i = 1; i < t.shnum; i++)
      if (LnkRd (LnkShdr (&t, i) + 4, 4) == 2) t.symsec = i;
   cnt = LnkSymCount (&t);
   for (i = 1; i < cnt; i++)
   {
      const small * s = LnkSymEnt (&t, i);
      int bind = s [4] >> 4;
      int shndx = (int) LnkRd (s + 6, 2);
      int g;
      if ((bind != 1 && bind != 2) || shndx == 0) continue;
      g = LnkFind (LnkSymName (&t, i));
      if (g >= 0 && ! LnkSyms [g].defined) return true;
   }
   return false;
}

static void LnkArchive (const small * p, size_t n, const char * name)
{
   LnkArch a;
   size_t pos = 8;
   int i;
   bool more;

   a.data = p;
   a.size = n;
   a.count = 0;
   a.moff = (size_t *) LnkAlloc ((n / 60 + 1) * sizeof (size_t));
   a.msize = (size_t *) LnkAlloc ((n / 60 + 1) * sizeof (size_t));
   a.used = (bool *) LnkAlloc ((n / 60 + 1) * sizeof (bool));
   while (pos + 60 <= n)
   {
      const small * h = p + pos;
      char  num [11];
      size_t sz;
      memcpy (num, h + 48, 10);
      num [10] = 0;
      sz = (size_t) strtoul (num, NULL, 10);
      if (h [58] != 0x60 || h [59] != 0x0A || pos + 60 + sz > n) LnkError ("Bad archive", name);
      if (h [0] != '/')                       /* "/" tabulka symbolu, "//" dlouha jmena */
      {
         a.moff [a.count] = pos + 60;
         a.msize [a.count] = sz;
         a.used [a.count] = false;
         a.count ++;
      }
      pos = pos + 60 + sz + (sz & 1);
   }
   /* cleny, dokud nejaky definuje nedefinovany symbol */
   do
   {
      more = false;
      for (i = 0; i < a.count; i++)
         if (! a.used [i] && LnkMemberNeeded (p + a.moff [i], a.msize [i]))
         {
            a.used [i] = true;
            LnkAddObj (p + a.moff [i], a.msize [i], name, true);
            more = true;
         }
   }
   while (more);
   free (a.moff);
   free (a.msize);
   free (a.used);
}

/* skupina vystupu podle jmena a typu sekce: 0 = vynechat */
#define LnkText  1
#define LnkInit  2
#define LnkFini  3
#define LnkRo    4
#define LnkData  5
#define LnkArr   6
#define LnkBss   7

static int LnkGroup (LnkObj * o, int i)
{
   const small * sh = LnkShdr (o, i);
   uint32_t type = (uint32_t) LnkRd (sh + 4, 4);
   uint64_t flags = LnkRd (sh + 8, 8);
   const char * nm = LnkSecName (o, i);
   if ((flags & 2) == 0) return 0;                                   /* !SHF_ALLOC */
   if (strncmp (nm, ".eh_frame", 9) == 0 || strncmp (nm, ".note", 5) == 0) return 0;
   if (type == 8) return LnkBss;                                     /* NOBITS */
   if (type == 14 || strncmp (nm, ".init_array", 11) == 0 || strncmp (nm, ".ctors", 6) == 0)
      return LnkArr;
   if (type == 15 || type == 16 || strncmp (nm, ".fini_array", 11) == 0 || strncmp (nm, ".dtors", 6) == 0)
      return 0;                     /* fini_array: ToaruOS libc je nevola (kap. 67) */
   if (strcmp (nm, ".init") == 0) return LnkInit;
   if (strcmp (nm, ".fini") == 0) return LnkFini;
   if (flags & 4) return LnkText;                                    /* SHF_EXECINSTR */
   if (flags & 1) return LnkData;                                    /* SHF_WRITE */
   return LnkRo;
}

static uint64_t LnkAlignUp (uint64_t a, uint64_t al)
{
   if (al <= 1) return a;
   return (a + al - 1) / al * al;
}

/* adresa symbolu k objektu o (lokalni i globalni) */
static uint64_t LnkSymAddr (LnkObj * o, int k, int * glob)
{
   const small * s = LnkSymEnt (o, k);
   int g = o->symglob [k];
   *glob = g;
   if (g >= 0)
   {
      LnkSym * y = &LnkSyms [g];
      if (! y->defined)
      {
         if (y->weak || (s [4] >> 4) == 2) return 0;                /* slaby nedefinovany */
         LnkError ("Undefined symbol", y->name);
      }
      if (y->obj < 0 || y->sec == 0xFFF1) return y->value;
      return LnkObjs [y->obj].secaddr [y->sec] + y->value;
   }
   {
      int shndx = (int) LnkRd (s + 6, 2);
      if (shndx == 0xFFF1) return LnkRd (s + 8, 8);
      if (shndx == 0 || shndx >= o->shnum) return 0;
      return o->secaddr [shndx] + LnkRd (s + 8, 8);
   }
}

static int LnkWriteImage (const char * out, const small * img, uint64_t filesz, uint64_t memsz, uint64_t entry)
{
   small hdr [LnkHead];
   FILE * f;
   memset (hdr, 0, sizeof (hdr));
   hdr [0] = 0x7F; hdr [1] = 'E'; hdr [2] = 'L'; hdr [3] = 'F';
   hdr [4] = 2; hdr [5] = 1; hdr [6] = 1;
   LnkWr (hdr + 16, 2, 2);            /* ET_EXEC */
   LnkWr (hdr + 18, 0x3E, 2);         /* EM_X86_64 */
   LnkWr (hdr + 20, 1, 4);
   LnkWr (hdr + 24, entry, 8);
   LnkWr (hdr + 32, 64, 8);           /* e_phoff */
   LnkWr (hdr + 52, 64, 2);           /* e_ehsize */
   LnkWr (hdr + 54, 56, 2);           /* e_phentsize */
   LnkWr (hdr + 56, 1, 2);            /* e_phnum */
   LnkWr (hdr + 64, 1, 4);            /* PT_LOAD */
   LnkWr (hdr + 68, 7, 4);            /* R W X */
   LnkWr (hdr + 72, 0, 8);            /* p_offset */
   LnkWr (hdr + 80, LnkBase, 8);
   LnkWr (hdr + 88, LnkBase, 8);
   LnkWr (hdr + 96, filesz, 8);
   LnkWr (hdr + 104, memsz, 8);
   LnkWr (hdr + 112, 0x1000, 8);
   f = fopen (out, "wb");
   if (f == NULL) LnkError ("Cannot write", out);
   if (fwrite (hdr, 1, LnkHead, f) != LnkHead ||
       fwrite (img + LnkHead, 1, (size_t) (filesz - LnkHead), f) != (size_t) (filesz - LnkHead))
   {
      fclose (f);
      LnkError ("Cannot write", out);
   }
   if (fclose (f) != 0) LnkError ("Cannot write", out);
   chmod (out, 493);                  /* rwxr-xr-x */
   return 0;
}

static void LnkBody (const char * out, const char * const * inputs, int count)
{
   uint64_t addr, initbeg = 0, initend = 0, gotaddr = 0, edata, bssbeg, end;
   small * img;
   int i, j, k, grp, pass;
   int g_got, g_ia0, g_ia1, g_bss, g_edata, g_end, g_end2;

   /* 1. vstupy */
   for (i = 0; i < count; i++)
   {
      size_t n;
      const small * p = LnkReadFile (inputs [i], &n);
      if (n >= 8 && memcmp (p, "!<arch>\n", 8) == 0) LnkArchive (p, n, inputs [i]);
      else LnkAddObj (p, n, inputs [i], false);
   }
   /* symboly, ktere dava linker */
   g_got = LnkGlobal ("_GLOBAL_OFFSET_TABLE_");
   g_ia0 = LnkGlobal ("__init_array_start");
   g_ia1 = LnkGlobal ("__init_array_end");
   g_bss = LnkGlobal ("__bss_start");
   g_edata = LnkGlobal ("_edata");
   g_end = LnkGlobal ("_end");
   g_end2 = LnkGlobal ("end");
   if (LnkFind ("_start") < 0 || ! LnkSyms [LnkFind ("_start")].defined)
      LnkError ("Undefined symbol", "_start");

   /* 2. polozky GOT (symboly s relokaci GOTPCREL*) */
   LnkGotCnt = 0;
   for (i = 0; i < LnkObjCnt; i++)
   {
      LnkObj * o = &LnkObjs [i];
      for (j = 1; j < o->shnum; j++)
      {
         const small * sh = LnkShdr (o, j);
         uint64_t off, size;
         if (LnkRd (sh + 4, 4) != 4) continue;                          /* SHT_RELA */
         off = LnkRd (sh + 24, 8);
         size = LnkRd (sh + 32, 8);
         for (k = 0; (uint64_t) (k + 1) * 24 <= size; k++)
         {
            const small * r = o->data + off + (size_t) k * 24;
            uint64_t info = LnkRd (r + 8, 8);
            int type = (int) (info & 0xFFFFFFFF);
            int sym = (int) (info >> 32);
            int g;
            if (type != 9 && type != 41 && type != 42) continue;
            g = o->symglob [sym];
            if (g < 0) LnkError ("GOT relocation against local symbol", o->name);
            if (LnkSyms [g].got == 0) LnkSyms [g].got = ++ LnkGotCnt;
         }
      }
   }

   /* 3. rozvrzeni: skupiny v poradi, v kazde objekty v poradi vstupu
         (pole konstruktoru: nejdriv cleny archivu = libc, pak program) */
   addr = LnkBase + LnkHead;
   for (grp = LnkText; grp <= LnkBss; grp++)
   {
      if (grp == LnkArr) { addr = LnkAlignUp (addr, 8); initbeg = addr; }
      if (grp == LnkBss)
      {
         addr = LnkAlignUp (addr, 8);
         gotaddr = addr;                                               /* GOT pred .bss */
         addr += (uint64_t) LnkGotCnt * 8;
         edata = addr;
      }
      for (pass = 0; pass < 2; pass++)
         for (i = 0; i < LnkObjCnt; i++)
         {
            LnkObj * o = &LnkObjs [i];
            if (grp == LnkArr ? (pass == 0) != o->member : pass == 1) continue;
            for (j = 1; j < o->shnum; j++)
            {
               const small * sh = LnkShdr (o, j);
               if (LnkGroup (o, j) != grp) continue;
               addr = LnkAlignUp (addr, LnkRd (sh + 48, 8));
               o->secaddr [j] = addr;
               addr += LnkRd (sh + 32, 8);
            }
         }
      if (grp == LnkArr) initend = addr;
   }
   edata = gotaddr + (uint64_t) LnkGotCnt * 8;
   bssbeg = edata;
   end = addr;
   if (end - LnkBase > 0x40000000ULL) LnkError ("Program too large", NULL);

   /* symboly linkeru */
   LnkSyms [g_got].defined = true;   LnkSyms [g_got].value = gotaddr;
   LnkSyms [g_ia0].defined = true;   LnkSyms [g_ia0].value = initbeg;
   LnkSyms [g_ia1].defined = true;   LnkSyms [g_ia1].value = initend;
   LnkSyms [g_bss].defined = true;   LnkSyms [g_bss].value = bssbeg;
   LnkSyms [g_edata].defined = true; LnkSyms [g_edata].value = edata;
   LnkSyms [g_end].defined = true;   LnkSyms [g_end].value = end;
   LnkSyms [g_end2].defined = true;  LnkSyms [g_end2].value = end;

   /* 4. obsah: sekce (PROGBITS) do obrazu, GOT */
   img = (small *) LnkAlloc ((size_t) (edata - LnkBase));
   for (i = 0; i < LnkObjCnt; i++)
   {
      LnkObj * o = &LnkObjs [i];
      for (j = 1; j < o->shnum; j++)
      {
         const small * sh = LnkShdr (o, j);
         int gr = LnkGroup (o, j);
         if (gr == 0 || gr == LnkBss) continue;
         memcpy (img + (o->secaddr [j] - LnkBase), o->data + LnkRd (sh + 24, 8), (size_t) LnkRd (sh + 32, 8));
      }
   }
   for (i = 0; i < LnkSymCnt; i++)
      if (LnkSyms [i].got > 0)
      {
         LnkSym * y = &LnkSyms [i];
         uint64_t v = 0;
         if (y->defined)
            v = (y->obj < 0 || y->sec == 0xFFF1) ? y->value : LnkObjs [y->obj].secaddr [y->sec] + y->value;
         else if (! y->weak)
            LnkError ("Undefined symbol", y->name);
         LnkWr (img + (gotaddr + (uint64_t) (y->got - 1) * 8 - LnkBase), v, 8);
      }

   /* 5. relokace */
   for (i = 0; i < LnkObjCnt; i++)
   {
      LnkObj * o = &LnkObjs [i];
      for (j = 1; j < o->shnum; j++)
      {
         const small * sh = LnkShdr (o, j);
         uint64_t off, size;
         int target, gr;
         if (LnkRd (sh + 4, 4) != 4) continue;
         target = (int) LnkRd (sh + 44, 4);                              /* sh_info */
         gr = LnkGroup (o, target);
         if (gr == 0 || gr == LnkBss) continue;
         off = LnkRd (sh + 24, 8);
         size = LnkRd (sh + 32, 8);
         for (k = 0; (uint64_t) (k + 1) * 24 <= size; k++)
         {
            const small * r = o->data + off + (size_t) k * 24;
            uint64_t info = LnkRd (r + 8, 8);
            int type = (int) (info & 0xFFFFFFFF);
            int sym = (int) (info >> 32);
            int64_t a = (int64_t) LnkRd (r + 16, 8);
            uint64_t p = o->secaddr [target] + LnkRd (r, 8);
            small * w = img + (p - LnkBase);
            int g;
            uint64_t s = LnkSymAddr (o, sym, &g);
            int64_t v;
            switch (type)
            {
               case 1:                                                   /* R_X86_64_64 */
                  LnkWr (w, s + (uint64_t) a, 8);
                  break;
               case 24:                                                  /* PC64 */
                  LnkWr (w, s + (uint64_t) a - p, 8);
                  break;
               case 2: case 4:                                           /* PC32, PLT32 */
                  v = (int64_t) (s + (uint64_t) a - p);
                  if (v < -2147483648LL || v > 2147483647LL) LnkError ("Relocation out of range", LnkSymName (o, sym));
                  LnkWr (w, (uint64_t) v, 4);
                  break;
               case 10: case 11:                                         /* 32, 32S */
                  v = (int64_t) (s + (uint64_t) a);
                  if (v < 0 || v > 0x7FFFFFFFLL) LnkError ("Relocation out of range", LnkSymName (o, sym));
                  LnkWr (w, (uint64_t) v, 4);
                  break;
               case 9: case 41: case 42:                                 /* GOTPCREL, (REX_)GOTPCRELX */
                  v = (int64_t) (gotaddr + (uint64_t) (LnkSyms [g].got - 1) * 8 + (uint64_t) a - p);
                  LnkWr (w, (uint64_t) v, 4);
                  break;
               default:
               {
                  char t [32];
                  snprintf (t, sizeof (t), "%d", type);
                  LnkError ("Unsupported relocation type", t);
               }
            }
         }
      }
   }

   {
      int st = LnkFind ("_start");
      LnkSym * y = &LnkSyms [st];
      LnkWriteImage (out, img, edata - LnkBase, end - LnkBase,
                     LnkObjs [y->obj].secaddr [y->sec] + y->value);
   }
   free (img);
}

static void LnkFree (void)
{
   int i;
   for (i = 0; i < LnkObjCnt; i++)
   {
      free (LnkObjs [i].secaddr);
      free (LnkObjs [i].symglob);
   }
   free (LnkObjs);
   LnkObjs = NULL; LnkObjCnt = 0; LnkObjCap = 0;
   for (i = 0; i < LnkFileCnt; i++) free (LnkFiles [i]);
   free (LnkFiles);
   LnkFiles = NULL; LnkFileCnt = 0; LnkFileCap = 0;
   free (LnkSyms);
   LnkSyms = NULL; LnkSymCnt = 0;
}

/* staticke slinkovani pro ToaruOS (pasmlib.h PasmLink); 0 = v poradku,
   jinak hlaseni v msg */
static int LnkRc;                     /* staticka - longjmp ji neprepise */

int LinkToaru (const char * out, const char * const * inputs, int count, char * msg, int msg_size)
{
   int rc;
   LnkRc = 0;
   LnkMsg [0] = 0;
   LnkSyms = (LnkSym *) calloc (LnkMaxSym, sizeof (LnkSym));
   if (LnkSyms == NULL) LnkRc = 1;
   else if (setjmp (LnkJump) == 0) LnkBody (out, inputs, count);
   else LnkRc = 1;
   rc = LnkRc;
   LnkFree ();
   if (msg != NULL && msg_size > 0)
   {
      if (rc == 0) msg [0] = 0;
      else snprintf (msg, (size_t) msg_size, "link: %s", LnkMsg [0] != 0 ? LnkMsg : "No more memory");
   }
   return rc;
}
