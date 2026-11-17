/* DEF.CC */

/* spolecne typy, promenne a procedury assembleru (prevedeno z def.pas) */

#include <string.h>
#include <stdlib.h>

#include "def.h"

longint ErrLine;
card    ErrCol;
char    ErrMsg [80 + 1];

FILE *  source = NULL;

card    column;
card    start;
longint line;
char    LineBuf [LineLen + 1];

char    cc;
symbol  CurSy;

alpha   id;
char    num [NumLen + 1];
char    chs [ChsLen + 1];

LocPtr  LocFirst;
ObjInx  NoName;

HeadRec SegHead;
HeadRec GrpHead;
HeadRec ExtHead;
HeadRec PubHead;

TreePtr DispStd;
TreePtr DispUsr;

TreePtr ans;

TreePtr ThisSeg;
TreePtr MainSeg;
TreePtr ThisProc;
card    default_;
bool    x64;
longint ip;

Intel   proc;
Intel   coproc;
bool    Any386;

TreePtr assume [SegMax - SegMin + 1];
bool    terminate_;
bool    errors;

char    ModName [255 + 1];
TreePtr lab;

bool    obj;
FILE *  ObjFile = NULL;
char    ObjName [255 + 1];

bool    bin;
FILE *  BinFile = NULL;
char    BinName [255 + 1];

bool    eobj;
char    EobjName [255 + 1];

bool    elf;
FILE *  ElfFile = NULL;
char    ElfName [255 + 1];

bool    ToaruTarget;    /* cil ToaruOS (-T / PASM_TARGET=toaru), viz code.c */

bool    pe;             /* PE pro Windows (-w), viz code.c */
char    PeName [255 + 1];
bool    cobj;           /* objekt COFF x64 (-m), viz code.c */
char    CobjName [255 + 1];

bool    lst;
FILE *  LstFile = NULL;
char    LstName [255 + 1];

bool    pas;
FILE *  PasFile = NULL;
char    PasName [255 + 1];

bool    StopAllowed = false;
bool    RetryAllowed = false;

alpha   ErrId;

jmp_buf LineJump;
jmp_buf AbortJump;
bool    AbortSet = false;

/******************************** STRINGS *********************************/

void StrSet (char * dst, int size, const char * src)
{
   if (dst == src) return;
   int len = strlen (src);
   if (len > size - 1) len = size - 1;
   memmove (dst, src, len);
   dst [len] = 0;
}

void StrAdd (char * dst, int size, const char * src)
{
   int len = strlen (dst);
   int add = strlen (src);
   if (len + add > size - 1) add = size - 1 - len;
   if (add > 0)
   {
      memmove (dst + len, src, add);
      dst [len + add] = 0;
   }
}

void StrAddChr (char * dst, int size, char c)
{
   int len = strlen (dst);
   if (len < size - 1)
   {
      dst [len] = c;
      dst [len + 1] = 0;
   }
}

/********************************* CONST **********************************/

/* nulovy vyraz a ST(0), ST(1) - konstanty, nastavi je Init (InitConst);
   ne inicializatorem struktury: designatory .typ = ... nezna cparser
   ani cmm, poradove hodnoty vsech polozek by byly necitelne */
ExprRec zero;
ExprRec st0;
ExprRec st1;

static void InitConst (void)
{
   memset (&zero, 0, sizeof (zero));
   zero.typ   = any;
   zero.sort  = imm;
   zero.rx    = None;
   zero.pref  = None;
   zero.base  = None;
   zero.index = None;
   zero.scale = 1;
   st0 = zero;
   st0.sort = st;
   st0.ceil = 0;
   st1 = st0;
   st1.ceil = 1;
}

const char RegTable [None + 1][4 + 1] =
{
   "AL",  "CL",  "DL",  "BL",  "AH",  "CH",  "DH",  "BH",
   "AX",  "CX",  "DX",  "BX",  "SP",  "BP",  "SI",  "DI",
   "EAX", "ECX", "EDX", "EBX", "ESP", "EBP", "ESI", "EDI",
   "ES",  "CS",  "SS",  "DS",  "FS",  "GS",

   "RAX",  "RCX",  "RDX",  "RBX",  "RSP",  "RBP",  "RSI",  "RDI",
   "R8",   "R9",   "R10",  "R11",  "R12",  "R13",  "R14",  "R15",
   "R8D",  "R9D",  "R10D", "R11D", "R12D", "R13D", "R14D", "R15D",
   "R8W",  "R9W",  "R10W", "R11W", "R12W", "R13W", "R14W", "R15W",
   "R8B",  "R9B",  "R10B", "R11B", "R12B", "R13B", "R14B", "R15B",
   "SPL",  "BPL",  "SIL",  "DIL",

   ""
};

/******************************** REGISTERS *******************************/

small RegCode (Register r)
{
   if (r <= GS)  return (small) (r % 8);           /* AL .. GS */
   if (r <= R15) return (small) (r - RAX);         /* RAX .. R15 */
   if (r <= R15B) return (small) (8 + (r - R8D) % 8); /* R8D .. R15B */
   if (r <= DIL) return (small) (4 + (r - SPL));   /* SPL .. DIL */
   return 0;
}

bool RegNew (Register r)
{
   return r >= RAX && r <= DIL;
}

bool RegRex (Register r)
{
   return RegNew (r) && (RegCode (r) >= 8 || (r >= SPL && r <= DIL));
}

static card RegTyp (Register r)
/* velikost registru */
{
   if (r <= BH)   return byte;
   if (r <= DI)   return word;
   if (r <= EDI)  return dword;
   if (r <= GS)   return word;
   if (r <= R15)  return qword;
   if (r <= R15D) return dword;
   if (r <= R15W) return word;
   return byte;                                    /* R8B .. R15B, SPL .. DIL */
}

static void * NewRec (size_t size)
{
   void * p = calloc (1, size);
   if (p == NULL) Abort (HeapFull);
   return p;
}

/********************************** LIST **********************************/

void ListInit (HeadRec * head)
{
   head->first = NULL;
   head->last  = NULL;
   head->count = 0;
}

ObjInx ListAppend (HeadRec * head, TreePtr add)
{
   ListPtr ref = (ListPtr) NewRec (sizeof (ListRec));
   ref->data = add;
   ref->next = NULL;

   if (head->first == NULL)
   {
      head->first = ref;
      head->last  = ref;
   }
   else
   {
      head->last->next = ref;
      head->last = ref;
   }
   head->count ++;
   return head->count;
}

void LocInit (void)
{
   LocFirst = NULL;
}

ObjInx LocAppend (const char * add)
{
   bool   done = false;
   LocPtr ref  = LocFirst;
   LocPtr last = NULL;
   ObjInx count = 0;

   while (ref != NULL && ! done)
   {
      count ++;
      if (strcmp (ref->name, add) == 0) done = true;
      last = ref;
      ref = ref->next;
   }

   if (! done)
   {
      count ++;
      ref = (LocPtr) NewRec (sizeof (LocRec));
      StrSet (ref->name, sizeof (alpha), add);
      ref->next = NULL;
      if (LocFirst == NULL)
         LocFirst = ref;
      else
         last->next = ref;
   }

   return count;
}

/********************************** TREE **********************************/

void Store (TreePtr * root, TreePtr add)
{
   add->left  = NULL;
   add->right = NULL;

   if (*root == NULL)
   {
      *root = add;
      return;
   }

   TreePtr ref = *root;
   for (;;)
   {
      int cmp = strcmp (add->name, ref->name);
      if (cmp < 0)
      {
         if (ref->left != NULL)
            ref = ref->left;
         else
         {
            ref->left = add;
            return;
         }
      }
      else if (cmp > 0)
      {
         if (ref->right != NULL)
            ref = ref->right;
         else
         {
            ref->right = add;
            return;
         }
      }
      else
         AsmError (DuplId);
   }
}

TreePtr SearchTree (TreePtr root, const char * par)
{
   while (root != NULL)
   {
      int cmp = strcmp (par, root->name);
      if (cmp < 0)
         root = root->left;
      else if (cmp > 0)
         root = root->right;
      else
         break;
   }
   return root;
}

/********************************* LABELS *********************************/

void NewIdentif (TreePtr * ref, const char * p_name)
{
   *ref = (TreePtr) NewRec (sizeof (TreeRec));

   StrSet ((*ref)->name, sizeof (alpha), p_name);
   StrSet ((*ref)->orig, sizeof (alpha), p_name);
   (*ref)->extname [0] = 0;
   (*ref)->def      = false;
   (*ref)->ope      = zero;
   (*ref)->ope.sort = unknown;

   Store (&DispUsr, *ref);
}

bool NearProc (void)
{
   return ThisProc == NULL || ThisProc->ope.typ == near_;
}

void ThisLabel (ExprRec * e, card p_typ)
{
   *e = zero;
   e->sort   = mem;
   e->typ    = p_typ;
   e->frame  = ThisSeg;
   e->target = ThisSeg;
   e->val    = ip;
   e->add    = NULL;
}

void ForwLabel (TreePtr ref)
{
   if (ref->ope.sort != unknown) AsmError (DuplId);
   ref->ope = zero;
   ref->ope.sort   = mem;
   ref->ope.typ    = near_;
   ref->ope.frame  = ThisSeg;
   ref->ope.target = ThisSeg;
   ref->ope.add    = ref;
}

void SetLabel (TreePtr ref)
{
   if (ref->def) AsmError (DuplId);
   if (ref->ope.sort != mem) AsmError (DuplId);
   ref->def = true;
   ThisLabel (&ref->ope, near_);
}

void NewLabel (TreePtr ref, card p_typ)
{
   if (ref->ope.sort != unknown) AsmError (DuplId);
   ref->def = true;
   ThisLabel (&ref->ope, p_typ);
}

void DefineLabel (TreePtr ref, card p_typ)
/* Definice navesti (PROC, LABEL, DB ...), na ktere uz mohl byt
   dopredny odkaz (ForwLabel) - puvodne hlasilo Duplicate identifier */
{
   if (ref->ope.sort == mem && ! ref->def)
   {
      ref->def = true;
      ThisLabel (&ref->ope, p_typ);
   }
   else
      NewLabel (ref, p_typ);
}

void NewExtrn (TreePtr ref, ExprSort p_sort, card p_typ)
{
   if (ref->ope.sort != unknown) AsmError (DuplId);
   ref->def = true;
   ref->ope = zero;
   ref->ope.sort   = p_sort;
   ref->ope.typ    = p_typ;
   ref->ope.target = ref;
   if (ThisSeg == NULL)
      ref->ope.frame = ref;
   else
      ref->ope.frame = ThisSeg;
   ref->ope.ExtInx = ListAppend (&ExtHead, ref);
}

void ForwSegment (TreePtr ref)
{
   if (ref->ope.sort != unknown) AsmError (DuplId);
   ref->def = false;
   ref->ope.sort      = segment;
   ref->ope.align     = ParaAlign;
   ref->ope.combi     = PrivateSeg;
   ref->ope.Seg32     = false;
   ref->ope.Seg64     = false;
   ref->ope.SegLen    = 0;
   ref->ope.FrameNum  = 0;
   ref->ope.SegInx    = ListAppend (&SegHead, ref);
   ref->ope.SegName   = LocAppend (ref->name);
   ref->ope.ClassName = NoName;
}

void NewGroup (TreePtr ref)
{
   if (ref->ope.sort != unknown) AsmError (DuplId);
   ref->def = true;
   ref->ope.sort    = group;
   ref->ope.GrpInx  = ListAppend (&GrpHead, ref);
   ref->ope.GrpName = LocAppend (ref->name);
   ListInit (&ref->ope.GrpSeq);
}

/******************************* INIT DISP ********************************/

static TreeRec * InitArea; /* [1..KeyMax] */

static TreePtr Enter (int a, int c)
/* vyvazeny strom z serazene tabulky klicovych slov */
{
   if (a > c) return NULL;

   int b = (a + c) / 2;
   TreePtr tem = &InitArea [b];

   StrSet (tem->name, sizeof (alpha), KeyTable[b].n);
   tem->def   = true;
   tem->left  = Enter (a, b-1);
   tem->right = Enter (b+1, c);
   tem->ope.sort = reserved;
   tem->ope.res  = KeyTable[b].k;

   return tem;
}

static void EnterRegs (void)
{
   for (int r = AL; r < None; r++)
   {
      Register rr = (Register) r;
      TreePtr ref = (TreePtr) NewRec (sizeof (TreeRec));

      StrSet (ref->name, sizeof (alpha), RegTable [rr]);
      ref->def = true;

      if (rr >= ES && rr <= GS)
         ref->ope.sort = seg;
      else
         ref->ope.sort = reg;

      ref->ope.typ = RegTyp (rr);

      ref->ope.rx = rr;

      Store (&DispStd, ref);
   }
}

static void EnterSpec (const char * n, ExprSort s, small c)
{
   TreePtr ref = (TreePtr) NewRec (sizeof (TreeRec));
   StrSet (ref->name, sizeof (alpha), n);
   ref->def = true;
   ref->ope.sort = s;
   ref->ope.typ  = dword;
   ref->ope.ceil = c;
   Store (&DispStd, ref);
}

static void InitDisp (void)
{
   char n [6];

   InitArea = (TreeRec *) NewRec ((KeyMax + 1) * sizeof (TreeRec));
   DispStd = Enter (1, KeyMax);
   EnterRegs ();

   for (int i = 0; i <= 3; i++)
   {
      snprintf (n, sizeof (n), "CR%d", i);
      EnterSpec (n, cr, i);
   }
   for (int i = 0; i <= 7; i++)
   {
      snprintf (n, sizeof (n), "DR%d", i);
      EnterSpec (n, dr, i);
   }
   for (int i = 6; i <= 7; i++)
   {
      snprintf (n, sizeof (n), "TR%d", i);
      EnterSpec (n, tr, i);
   }
   for (int i = 0; i <= 15; i++)      /* SSE: XMM8 .. XMM15 jen v USE64 (REX) */
   {
      snprintf (n, sizeof (n), "XMM%d", i);
      EnterSpec (n, xmm, i);
   }

   DispUsr = NULL;
}

/********************************** INIT **********************************/

void Init (void)
{
   InitConst ();
   LocInit ();
   NoName = LocAppend ("");

   ListInit (&SegHead);
   ListInit (&GrpHead);
   ListInit (&ExtHead);
   ListInit (&PubHead);

   InitDisp ();

   ThisSeg  = NULL;
   MainSeg  = NULL;
   ThisProc = NULL;
   default_ = word;
   x64      = false;
   ip       = 0;

   proc     = i86;
   coproc   = i00;
   Any386   = false;

   for (int rs = SegMin; rs <= SegMax; rs++)
      assume [rs - SegMin] = NULL;
   terminate_ = false;
   errors = false;

   line = 0;
   LineBuf [0] = 0;
}

/***************************** ERROR MESSAGES *****************************/

#define InPas  " not allowed in Turbo Pascal inline"
#define InBin  " not allowed in .COM file"
#define InBoth " not allowed in Turbo Pascal inline or .COM file"
#define InElf  " not allowed in ELF executable"

static void Info (ErrTyp msg)
{
   char m [80 + 1];
   const char * s;

   errors = true;

   switch (msg)
   {
      case EndOfFile:    s = "Unexpected end of source"; break;
      case IdTooLong:    s = "Identifier is too long"; break;
      case NumTooLong:   s = "Number is too long"; break;
      case StrTooLong:   s = "String is too long"; break;
      case QuoteExp:     s = "Quote expected"; break;
      case UnknownSym:   s = "Unknown symbol"; break;

      case DuplId:       s = "Duplicate identifier"; break;

      case BadNum:       s = "Bad integer constant"; break;
      case BadChar:      s = "Bad character constant"; break;
      case TypeExp:      s = "Type specifier expected"; break;
      case MemOpExp:     s = "Must be memory operand"; break;
      case AbsOpExp:     s = "Must be absolute operand"; break;
      case SegOpExp:     s = "Operand must have segment"; break;
      case NearOpExp:    s = "Operand must be near label"; break;
      case InvOp:        s = "Invalid operation"; break;
      case OutOfRange:   s = "Value is out of range"; break;
      case ZeroDiv:      s = "Division by zero"; break;
      case RParExp:      s = "\")\" expected"; break;
      case RBracketExp:  s = "\"]\" expected"; break;

      case SegRedef:     s = "Improper segment attributes"; break;
      case NestSeg:      s = "Segment nesting not allowed"; break;
      case NestProc:     s = "Procedure nesting not allowed"; break;
      case EndsExp:      s = "ENDS dirrective expected"; break;
      case EndpExp:      s = "ENDP dirrective expected"; break;
      case DifName:      s = "Improper name"; break;
      case NoSeg:        s = "No opened segment"; break;
      case SegRegExp:    s = "Segment register expected"; break;
      case SegGrpExp:    s = "Segment or group name expected"; break;
      case SegExp:       s = "Segment name expected"; break;
      case ColonExp:     s = "\":\" expected"; break;
      case IdExp:        s = "Identifier expected"; break;
      case MissingLab:   s = "Missing label"; break;
      case InvSegRef:    s = "Invalid segment reference"; break;
      case UndefId:      s = NULL; break;
      case LParExp:      s = "\"(\" expected"; break;
      case BadFloat:     s = "Bad real constant"; break;
      case JumpTooLong:  s = "Jump too long"; break;
      case SegTooLong:   s = "Segment too long"; break;

      case TooManySeg:   s = "Two segments" InBoth; break;
      case EntryNotAll:  s = "Start address" InPas; break;
      case GroupNotAll:  s = "Group" InBoth; break;
      case ExtrnNotAll:  s = "Extrn" InBin; break;
      case OrgNotAll:    s = "ORG" InPas; break;
      case DupNotAll:    s = "DUP operator" InPas; break;
      case DwordNotAll:  s = "Dword reference" InPas; break;
      case SegNotAll:    s = "Segment reference" InBoth; break;
      case FarNotAll:    s = "Far jump or call" InBoth; break;

      case ElfGroup:     s = "Group" InElf; break;
      case ElfExtrn:     s = "Extrn" InElf; break;
      case ElfSegRef:    s = "Segment reference" InElf; break;
      case ElfFar:       s = "Far jump or call" InElf; break;
      case ElfUse16:     s = "USE16 segment" InElf; break;
      case ElfAbsSeg:    s = "AT segment" InElf; break;
      case ElfNoEntry:   s = "Start address required (END label)"; break;
      case ElfRange:     s = "Address does not fit (use DWORD)"; break;
      case ElfNearSeg:   s = "Near jump or call to other segment"; break;

      case InvProc:      s = "Invalid instruction"; break;
      case BadScale:     s = "Bad scale factor"; break;
      case BadIndex:     s = "Bad index register"; break;
      case BadBase:      s = "Bad base register"; break;
      case ExtChar:      s = "Extra characters on line"; break;
      case CommaExp:     s = "Comma expected"; break;
      case UndefSize:    s = "Undefined size"; break;
      case BadSize:      s = "Improper operand size"; break;
      case BadSort:      s = "Wrong sort of operand"; break;
      case BadPar:       s = "Bad operand"; break;
      case AssumeErr:    s = "Invalid segment"; break;
      case InstExp:      s = "Instruction or directive expected"; break;

      case Inv64:        s = "Not allowed in 64-bit mode"; break;
      case Only64:       s = "Allowed only in 64-bit mode (USE64 segment)"; break;
      case RexHigh:      s = "AH, BH, CH, DH not allowed with REX prefix"; break;
      case Use64NotAll:  s = "USE64 segment allowed only in ELF executable, ELF object or listing"; break;
      case ElfMixed:     s = "USE32 and USE64 segments together" InElf; break;
      case EobjUse64:    s = "ELF object requires USE64 segments"; break;
      case EobjDup:      s = "Relocation inside DUP not allowed in ELF object"; break;
      case ToaruUse64:   s = "ToaruOS target requires USE64 segments"; break;
      case PeImport:     s = "Extrn ABS not allowed in PE executable"; break;
      case CoffSect:     s = "FINI_ARRAY segment not allowed in COFF object"; break;

      case Overflow:     s = "Expression too complicated"; break;
      case HeapFull:     s = "No more memory"; break;
      case SrcFault:     s = "Error reading source file"; break;
      case LstFault:     s = "Error writing listing file"; break;
      case PasFault:     s = "Error writing inline file"; break;
      case ObjFault:     s = "Error writing object file"; break;
      case ElfFault:     s = "Error writing executable file"; break;
      case Unsupported:  s = "Unsupported"; break;
      case Bug:          s = "Compiler error"; break;

      default:           s = "Error"; break;
   }

   if (msg == UndefId)
   {
      StrSet (m, sizeof (m), "Undefined identifier (");
      StrAdd (m, sizeof (m), ErrId);
      StrAdd (m, sizeof (m), ")");
   }
   else
      StrSet (m, sizeof (m), s);

   /* -w / -m pouzivaji kontroly ELF (-e / -c): "ELF executable" => "PE executable",
      "ELF object" => "COFF object" */
   if ((pe || cobj) && msg != Use64NotAll)
   {
      char * p = strstr (m, "ELF ");
      if (p != NULL)
      {
         char t [80 + 1];
         StrSet (t, sizeof (t), p + 3);
         *p = 0;
         StrAdd (m, sizeof (m), (strncmp (t, " object", 7) == 0) ? "COFF" : "PE");
         StrAdd (m, sizeof (m), t);
      }
   }

   ErrLine = line;
   ErrCol  = start;
   StrSet (ErrMsg, sizeof (ErrMsg), m);
}

void StopCompile (void)
{
   if (AbortSet) longjmp (AbortJump, 1);
   exit (1);
}

NORETURN static void Retry (void)
{
   if (StopAllowed) StopCompile ();
   if (RetryAllowed) longjmp (LineJump, 1);
   StopCompile ();
}

void Message (ErrTyp msg)
{
   Info (msg);
   Retry ();
}

void AsmError (ErrTyp msg)
{
   Info (msg);
   Retry ();
}

void Abort (ErrTyp msg)
{
   Info (msg);
   StopCompile ();
}
