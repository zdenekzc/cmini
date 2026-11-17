/* DEF.H */

/* common types, variables and procedures of the assembler (translated from def.pas) */

#ifndef DEF_H
#define DEF_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <setjmp.h>

#include "key.h"

/* function attributes (C99: neither [[noreturn]] nor _Noreturn; gcc, clang and tcc know __attribute__) */
#if defined (__GNUC__) || defined (__TINYC__)
#define NORETURN    __attribute__ ((noreturn))
#define MAYBE_UNUSED __attribute__ ((unused))
#else
#define NORETURN
#define MAYBE_UNUSED
#endif

/*
   Types from Pascal:
      small   = 0..$FF      -> uint8_t
      int     = 0..$FFFF    -> card (uint16_t)
      long    = longint     -> longint (int32_t)
      bool    = boolean     -> bool
      integer (16-bit signed, TP mode) -> int (beware of overflow)
      string [n]            -> char [n+1], zero-terminated strings
*/

typedef uint8_t  small;
typedef uint16_t card;
typedef int32_t  longint;

extern longint ErrLine;
extern card    ErrCol;
extern char    ErrMsg [80 + 1];

/****************************** STRINGS ***********************************/

/* assignment and concatenation of strings, truncated to the size (like string [n]) */
void StrSet    (char * dst, int size, const char * src);
void StrAdd    (char * dst, int size, const char * src);
void StrAddChr (char * dst, int size, char c);

/**************************** LEXICAL ANALYSIS ****************************/

#define LineLen  160
#define AlphaLen  31
#define NumLen    80
#define ChsLen    80

typedef char alpha [AlphaLen + 1];

enum symbol
{
   LBracket,   RBracket,   LPar,       RPar,
   LAngle,     RAngle,     Plus,       Minus,
   Slash,      Asterisk,   Colon,      Comma,
   Equal,      Ampersand,  Percent,    Excl,

   Identif,    Number,     Float,      Chars,
   EOL,
   Operand     /* operand from an asmlist node (phase 2) - see NodeOperands */
};
typedef enum symbol symbol;

extern FILE *  source;

extern card    column;     /* position in LineBuf, numbered from 1 as in Pascal */
extern card    start;
extern longint line;
extern char    LineBuf [LineLen + 1];

extern char    cc;
extern symbol  CurSy;

extern alpha   id;
extern char    num [NumLen + 1];
extern char    chs [ChsLen + 1];

/******************************** SYMBOLS *********************************/

/* sizes and types of operands (Pascal: byte, word, ..., short, near, far) */
#define byte    1
#define word    2
#define dword   4
#define fword   6
#define qword   8
#define tbyte  10
#define any     0
#define short_ 0xFD
#define near_  0xFF
#define far_   0xFE

typedef struct LocRec  LocRec;
typedef struct ListRec ListRec;
typedef struct TreeRec TreeRec;

typedef LocRec  * LocPtr;
typedef ListRec * ListPtr;
typedef TreeRec * TreePtr;
typedef card      ObjInx;

struct LocRec
{
   alpha  name;
   LocPtr next;
};

struct ListRec
{
   TreePtr data;
   ListPtr next;
};

typedef struct
{
   ListPtr first;
   ListPtr last;
   ObjInx  count;
}
   HeadRec;

/* enum Register and RegTable - reg.h (shared with asmlist.c) */
#include "reg.h"

small RegCode (Register r);  /* register number in the instruction 0..15 (bit 3 => REX) */
bool  RegNew  (Register r);  /* x86-64 register (only in segment USE64) */
bool  RegRex  (Register r);  /* register requires a REX prefix (R8.., SPL .. DIL) */

enum AlignTyp
{
   AbsSeg, ByteAlign, WordAlign,
   ParaAlign, PageAlign, DwordAlign
};
typedef enum AlignTyp AlignTyp;

enum CombiTyp
{
   PrivateSeg, MemorySeg, PublicSeg, Combi3,
   Combi4, StackSeg, CommonSeg, Combi7
};
typedef enum CombiTyp CombiTyp;

enum ExprSort
{
   reg, seg, st, cr, dr, tr, mem, imm,
   segment, group, reserved, unknown,
   xmm                                 /* XMM0 .. XMM15 (ceil = number), SSE / SSE2 */
};
typedef enum ExprSort ExprSort;

/* variant record from Pascal - all variants as separate items */
typedef struct
{
   card     typ;
   ExprSort sort;

   /* reg, seg */
   Register rx;

   /* st, cr, dr, tr */
   small    ceil;

   /* mem, imm */
   Register pref;
   Register base;
   Register index;
   small    scale;
   longint  val;
   int64_t  val64;    /* 64-bit constant value (x86-64); val = its lower 32 bits */
   TreePtr  add;
   TreePtr  frame;
   TreePtr  target;
   bool     para;
   ObjInx   ExtInx;

   /* segment */
   AlignTyp align;
   CombiTyp combi;
   bool     Seg32;
   bool     Seg64;    /* USE64 (Seg32 is also true) */
   longint  SegLen;
   card     FrameNum;
   ObjInx   SegInx;
   ObjInx   SegName;
   ObjInx   ClassName;

   /* group */
   ObjInx   GrpInx;
   ObjInx   GrpName;
   HeadRec  GrpSeq;

   /* reserved */
   keyword  res;
}
   ExprRec;

struct TreeRec
{
   alpha   name;
   bool    def;

   alpha   orig;                  /* name as written (upper / lower case) */
   char    extname [ChsLen + 1];  /* name for the linker (EXTRN / PUBLIC 'name'), otherwise orig */

   TreePtr left;
   TreePtr right;

   ExprRec ope;
};

extern LocPtr  LocFirst;
extern ObjInx  NoName;

extern HeadRec SegHead;
extern HeadRec GrpHead;
extern HeadRec ExtHead;
extern HeadRec PubHead;

extern TreePtr DispStd;
extern TreePtr DispUsr;

extern TreePtr ans;

/**************************** CODE GENERATION *****************************/

enum Intel { i86, i186, i286, i386, i64, i00, i87, i287, i387 };
typedef enum Intel Intel;

#define LastProc i64
#define SegMin   ES
#define SegMax   GS

extern TreePtr ThisSeg;
extern TreePtr MainSeg;
extern TreePtr ThisProc;
extern card    default_;     /* Pascal: default */
extern bool    x64;          /* current segment is USE64 */
extern longint ip;

extern Intel   proc;
extern Intel   coproc;
extern bool    Any386;

extern TreePtr assume [SegMax - SegMin + 1];   /* assume [rs - SegMin] */
extern bool    terminate_;   /* Pascal: terminate */
extern bool    errors;

extern char    ModName [255 + 1];
extern TreePtr lab;

extern bool    obj;
extern FILE *  ObjFile;
extern char    ObjName [255 + 1];

extern bool    bin;
extern FILE *  BinFile;
extern char    BinName [255 + 1];

extern bool    eobj;         /* relocatable object ELF64 (-c) */
extern char    EobjName [255 + 1];

extern bool    elf;          /* executable ELF i386 file for Linux */
extern FILE *  ElfFile;
extern char    ElfName [255 + 1];

extern bool    ToaruTarget;  /* target ToaruOS (-T): different base image; not "toaru" - gcc in ToaruOS defines it as macro 1 */

extern bool    pe;           /* executable PE file for Windows (-w, .exe; elf = true) */
extern char    PeName [255 + 1];
extern bool    cobj;         /* COFF x64 object for Windows (-m, .obj; eobj = true) */
extern char    CobjName [255 + 1];

extern bool    lst;
extern FILE *  LstFile;
extern char    LstName [255 + 1];

extern bool    pas;
extern FILE *  PasFile;
extern char    PasName [255 + 1];

/* name of the source file is ModName */

/************************* DEFINITION PROCEDURES **************************/

void    ListInit (HeadRec * head);
ObjInx  ListAppend (HeadRec * head, TreePtr add);

void    LocInit (void);
ObjInx  LocAppend (const char * add);

void    Store  (TreePtr * root, TreePtr add);
TreePtr SearchTree (TreePtr root, const char * par);

void    NewIdentif (TreePtr * ref, const char * p_name);

bool    NearProc (void);
void    ThisLabel (ExprRec * e, card p_typ);

void    ForwLabel (TreePtr ref);
void    SetLabel  (TreePtr ref);
void    NewLabel  (TreePtr ref, card p_typ);
void    DefineLabel (TreePtr ref, card p_typ);

void    NewExtrn    (TreePtr ref, ExprSort p_sort, card p_typ);
void    ForwSegment (TreePtr ref);
void    NewGroup    (TreePtr ref);

void    Init (void);

/********************************* CONST **********************************/

extern ExprRec zero;      /* constants - set by Init */
extern ExprRec st0;
extern ExprRec st1;


/***************************** ERROR MESSAGES *****************************/

enum ErrTyp
{
   /* Sym */  EndOfFile,    IdTooLong,    NumTooLong,   StrTooLong,   QuoteExp,
              UnknownSym,

   /* Def */  DuplId,

   /* Expr */ BadNum,       BadChar,      TypeExp,      MemOpExp,     AbsOpExp,
              SegOpExp,     NearOpExp,    MemImmExp,    OutOfRange,   InvOp,
              ZeroDiv,      RParExp,      RBracketExp,  Syntax,

   /* Oth */  SegRedef,     NestSeg,      NestProc,     EndsExp,      EndpExp,
              DifName,      NoSeg,        SegRegExp,    SegGrpExp,    SegExp,
              ColonExp,     IdExp,        MissingLab,   InvSegRef,    UndefId,
              LParExp,      BadFloat,     JumpTooLong,  SegTooLong,

              TooManySeg,   EntryNotAll,  GroupNotAll,  ExtrnNotAll,  OrgNotAll,
              DupNotAll,    DwordNotAll,  SegNotAll,    FarNotAll,
              ElfGroup,     ElfExtrn,     ElfSegRef,    ElfFar,       ElfUse16,
              ElfAbsSeg,    ElfNoEntry,   ElfRange,     ElfNearSeg,

   /* Inst */ InvProc,      BadScale,     BadIndex,     BadBase,      ExtChar,
              CommaExp,     UndefSize,    BadSize,      BadSort,      BadPar,
              AssumeErr,    InstExp,

   /* x86-64 */
              Inv64,        Only64,       RexHigh,      Use64NotAll,  ElfMixed,
              EobjUse64,    EobjDup,      ToaruUse64,
              PeImport,     CoffSect,

   /* Code */ SrcFault,     ObjFault,     BinFault,     LstFault,     PasFault,     DupTooLong,
              ElfFault,

              Overflow,     HeapFull,     Unsupported,  Bug
};
typedef enum ErrTyp ErrTyp;

extern bool  StopAllowed;
extern bool  RetryAllowed;

extern alpha ErrId;

/* Error recovery with setjmp / longjmp:
      LineJump  - return to the end of the line being assembled (RetryAllowed)
      AbortJump - terminate the whole assembly */

extern jmp_buf LineJump;
extern jmp_buf AbortJump;
extern bool    AbortSet;     /* AbortJump is set */

NORETURN void Message (ErrTyp msg);
NORETURN void AsmError   (ErrTyp msg);
NORETURN void Abort   (ErrTyp msg);
NORETURN void StopCompile (void);  /* end of assembly - jump to AbortJump */

#endif /* DEF_H */
