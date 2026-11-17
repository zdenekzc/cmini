/* ASMLIST.H - program pro assembler pasm jako spojovy seznam (ciste C) */

/*
   Misto textu pro assembler sestavuje prekladac program jako obousmerny
   seznam uzlu (obdoba x86::Builder z AsmJit):

      AsmProgram p;
      AsmInit (&p);
      AsmLabel * loop = AsmNewLabel (&p, "L");          // L1, L2, ...
      AsmBind (&p, loop);                               // L1:
      AsmIns2 (&p, iMov, AsmReg (RAX), AsmMem (8, RBP, -16));
      AsmIns2 (&p, iCmp, AsmReg (RAX), AsmImm (10));
      AsmIns1 (&p, iJl, AsmLab (loop));

   - instrukce a direktivy = konstanty vyctu keyword z key.h (iMov, iPush,
     iSegment, iDq, ...), registry = vycet Register z reg.h (RAX, R8D, ...),
   - operandy jsou struktury AsmOp (registr, prima hodnota, adresa
     [baze + index * meritko + posun], adresa navesti, navesti, OFFSET),
   - novy uzel se vlozi za kurzor a stane se kurzorem; AsmSetCursor
     presune kurzor kamkoli (napr. prolog funkce az po vygenerovani tela,
     kdy je znama velikost ramce), AsmRemove uzel odebere,
   - navesti jsou objekty AsmLabel (jmeno se vytvori automaticky nebo
     zada), AsmBind vlozi definici navesti na misto kurzoru,
   - AsmTextLine vlozi libovolny radek textu (prechod: behova knihovna
     RTL_... muze zustat textem).

   Preklad (faze 2): AsmAssemble preda seznam knihovne pasm
   (PasmAssembleList) - instrukce a navesti cte pasm primo z uzlu (operandy
   AsmOp => ExprRec, bez tisku a lexikalni analyzy), direktivy, data
   a radky textu z tisku uzlu.  Faze 1 (AsmAssembleText): cely seznam se
   vytiskne jako text (stejny tvar, jaky generuje instr.cc) =>
   PasmAssembleText.  Chyba "prog.asm(r,s) Error: ..." => radek r je r-ty
   uzel (AsmNodeAtLine).  Viz asmlist.txt.
*/

#ifndef ASMLIST_H
#define ASMLIST_H

#include <stdio.h>
#include <stdint.h>

#include "key.h"     /* enum keyword:  iMov, iAdd, iSegment, iDq, ... */
#include "reg.h"     /* enum Register: RAX, RCX, ..., None */

#ifdef __cplusplus
extern "C" {      /* asmlist.c je C (../cmini/instr.cc, ../cmm/instr-pasm.cc - C++) */
#endif

/******************************** OPERANDY ********************************/

typedef struct AsmLabel AsmLabel;

typedef enum
{
   AO_NONE,
   AO_REG,      /* registr:          rax */
   AO_XMM,      /* registr XMM:      xmm3  (num = cislo) */
   AO_IMM,      /* prima hodnota:    10 */
   AO_MEM,      /* adresa:           qword ptr [rbp-16],  [rax+rcx*8+4],
                                     dword ptr [count],  msg (navesti bez size) */
   AO_LABEL,    /* navesti (skok, volani):  jmp L5,  call F3 */
   AO_OFFSET,   /* adresa navesti jako hodnota:  dq offset ok */
   AO_STR,      /* retezec v datech:  'Hello' */
   AO_DUP       /* opakovani v datech:  16 dup (0)  (num = pocet, val = hodnota) */
}
   AsmOpKind;

typedef struct
{
   AsmOpKind  kind;
   int        size;       /* AO_MEM: 0 (bez PTR), 1, 2, 4, 8 => byte .. qword ptr */
   Register   reg;        /* AO_REG; AO_MEM: baze (None = bez baze) */
   Register   index;      /* AO_MEM: index (None) */
   int        scale;      /* AO_MEM: 1, 2, 4, 8 */
   int64_t    val;        /* AO_IMM: hodnota; AO_MEM, AO_LABEL: posun; AO_DUP: hodnota */
   int        num;        /* AO_XMM: cislo registru; AO_DUP: pocet; AO_IMM: 1 = tisk 0...h, 2 = bez znamenka,
                                     3 = ...h */
   AsmLabel * label;      /* AO_MEM: [navesti + posun]; AO_LABEL, AO_OFFSET */
   const char * str;      /* AO_STR */
}
   AsmOp;

AsmOp AsmReg (Register r);
AsmOp AsmXmm (int n);
AsmOp AsmImm (int64_t v);
AsmOp AsmImmHex (int64_t v);                                          /* tisk 0...h */
AsmOp AsmImmU (uint64_t v);                                           /* tisk bez znamenka */
AsmOp AsmImmHexLit (int64_t v);                                       /* tisk ...h (bez 0 na zacatku) */
AsmOp AsmMem (int size, Register base, int64_t disp);                  /* size ptr [base+disp] */
AsmOp AsmMemIdx (int size, Register base, Register index, int scale, int64_t disp);
AsmOp AsmMemLab (int size, AsmLabel * label, int64_t disp);           /* size ptr [label+disp] */
AsmOp AsmLab (AsmLabel * label);                                      /* cil skoku / volani */
AsmOp AsmOffset (AsmLabel * label);                                   /* offset label */
AsmOp AsmStr (const char * s);                                        /* data: 'text' */
AsmOp AsmDup (int count, int64_t value);                              /* data: n dup (v) */

/********************************** UZLY **********************************/

typedef enum
{
   AN_INST,     /* instrukce:  [prefix] inst op, op, op */
   AN_LABEL,    /* definice navesti:  jmeno: */
   AN_DATA,     /* data:  [jmeno] db / dw / dd / dq  polozky */
   AN_EQU,      /* jmeno equ hodnota */
   AN_DIR,      /* direktiva:  .x64, segment, ends, proc, endp, extrn, public, align, end */
   AN_TEXT,     /* libovolny radek textu */
   AN_CFI       /* informace pro .eh_frame (jako .cfi_... v GNU as): cfi, cfi_reg, value;
                   pasm ho tiskne jako poznamku, .eh_frame z nich sestavi prekladac */
}
   AsmNodeKind;

/* AN_CFI: druh (pravidla plati od mista uzlu - za predchozi instrukci) */
typedef enum
{
   ASM_CFI_STARTPROC = 1,      /* .cfi_startproc */
   ASM_CFI_ENDPROC,            /* .cfi_endproc */
   ASM_CFI_DEF_CFA_OFFSET,     /* .cfi_def_cfa_offset value */
   ASM_CFI_DEF_CFA_REGISTER,   /* .cfi_def_cfa_register reg */
   ASM_CFI_DEF_CFA,            /* .cfi_def_cfa reg, value */
   ASM_CFI_OFFSET,             /* .cfi_offset reg, value  (value zaporne: CFA - n) */
   ASM_CFI_RESTORE,            /* .cfi_restore reg */
   ASM_CFI_REMEMBER_STATE,     /* .cfi_remember_state */
   ASM_CFI_RESTORE_STATE       /* .cfi_restore_state */
}
   AsmCfiKind;

#define ASM_MAX_OPS 3

typedef struct AsmNode AsmNode;

struct AsmNode
{
   AsmNodeKind  kind;
   AsmNode *    prev;
   AsmNode *    next;

   keyword      inst;         /* AN_INST: iMov ...;  AN_DATA: iDb .. iDq;  AN_DIR: iSegment ... */
   keyword      prefix;       /* AN_INST: iRep, iLock, ... (iOther = zadny) */
   int          nops;
   AsmOp        op [ASM_MAX_OPS];

   AsmLabel *   label;        /* AN_LABEL, AN_DATA (jmeno nebo NULL), AN_EQU, AN_DIR (jmeno) */
   AsmOp *      items;        /* AN_DATA: polozky */
   int          nitems;
   int64_t      value;        /* AN_EQU, AN_DIR iAlign */
   const char * text;         /* AN_DIR: atributy (segment: "use64 para", extrn/public: jmeno
                                 pro linker);  AN_TEXT: radek */
   const char * comment;      /* poznamka za ; (NULL = zadna) */
   int          cfi;          /* AN_CFI: AsmCfiKind */
   Register     cfi_reg;      /* AN_CFI: registr (RAX .. R15) */
};

struct AsmLabel
{
   char *     name;
   AsmLabel * next;           /* seznam navesti programu */
   void *     sym;            /* pasm (faze 2): zaznam ve stromu jmen (TreePtr) */
   int        sym_gen;        /*   platny pro preklad cislo sym_gen */
};

typedef struct AsmProgram
{
   AsmNode *  first;
   AsmNode *  last;
   AsmNode *  cursor;         /* novy uzel se vlozi za kurzor (NULL = na zacatek) */
   AsmLabel * labels;
   int        label_count;    /* automaticka jmena */
   int        node_count;
   void *     op_blocks;      /* operandy z rozhrani s ukazateli (AsmPReg, ...) - uvolni AsmFree */
}
   AsmProgram;

void       AsmInit (AsmProgram * p);
void       AsmFree (AsmProgram * p);

/* navesti:  AsmNewLabel (p, "L") => L1, L2, ...;  AsmNamedLabel (p, "start") => start */
AsmLabel * AsmNewLabel (AsmProgram * p, const char * prefix);
AsmLabel * AsmNamedLabel (AsmProgram * p, const char * name);
AsmNode *  AsmBind (AsmProgram * p, AsmLabel * label);

/* instrukce */
AsmNode *  AsmIns (AsmProgram * p, keyword inst, int nops, const AsmOp * ops);
AsmNode *  AsmIns0 (AsmProgram * p, keyword inst);
AsmNode *  AsmIns1 (AsmProgram * p, keyword inst, AsmOp a);
AsmNode *  AsmIns2 (AsmProgram * p, keyword inst, AsmOp a, AsmOp b);
AsmNode *  AsmIns3 (AsmProgram * p, keyword inst, AsmOp a, AsmOp b, AsmOp c);
AsmNode *  AsmPrefixed (AsmProgram * p, keyword prefix, keyword inst);   /* rep movsb */

/* data a direktivy */
AsmNode *  AsmData (AsmProgram * p, AsmLabel * name, keyword dir, int nitems, const AsmOp * items);
AsmNode *  AsmEqu (AsmProgram * p, AsmLabel * name, int64_t value);
AsmNode *  AsmDir (AsmProgram * p, keyword dir, AsmLabel * name, const char * text);   /* iAlign: text "qword" */
AsmNode *  AsmAlign (AsmProgram * p, int n);
AsmNode *  AsmTextLine (AsmProgram * p, const char * line);

/* .eh_frame: AsmCfi (p, ASM_CFI_OFFSET, RBX, -24) => .cfi_offset rbx, -24 (pasm: poznamka);
   AsmCfiName - jmeno direktivy (".cfi_offset"), AsmCfiText - cely text bez poznamky */
AsmNode *  AsmCfi (AsmProgram * p, int cfi, Register reg, int64_t value);
const char * AsmCfiName (int cfi);
int        AsmCfiText (const AsmNode * n, char * buf, int size);

/* poznamka k uzlu (napr. jmeno promenne) */
AsmNode *  AsmComment (AsmNode * n, const char * comment);

/* kurzor:  AsmSetCursor vraci puvodni kurzor */
AsmNode *  AsmCursor (AsmProgram * p);
AsmNode *  AsmSetCursor (AsmProgram * p, AsmNode * n);
void       AsmRemove (AsmProgram * p, AsmNode * n);

/* text */
const char * AsmInstName (keyword k);                 /* iMov => "mov" */
const char * AsmRegName (Register r);                 /* RAX => "rax" */
void       AsmPrintNode (const AsmNode * n, FILE * f);
void       AsmPrint (const AsmProgram * p, FILE * f);
AsmNode *  AsmNodeAtLine (const AsmProgram * p, int line);  /* radek textu => uzel */

/* jeden radek textu uzlu (bez konce radky, oriznuty na size - 1 znaku);
   vysledek = delka celeho radku */
int        AsmNodeLine (const AsmNode * n, char * buf, int size);

/* preklad knihovnou pasm (pasmlib.h: flags PASM_ELFOBJ, PASM_EXE, PASM_LST, ...);
   vystupy base.o / base.bin / base.lst;  0 = bez chyby, jinak msg
      AsmAssemble      faze 2: pasm cte uzly primo (PasmAssembleList)
      AsmAssembleText  faze 1: tisk na text => PasmAssembleText
   oba davaji bajtove shodne vystupy */
int        AsmAssemble (const AsmProgram * p, const char * base, int flags, char * msg, int msg_size);
int        AsmAssembleText (const AsmProgram * p, const char * base, int flags, char * msg, int msg_size);


/************************* ROZHRANI S UKAZATELI ***************************/

/* Bez struktur predavanych hodnotou a bez pristupu k polozkam - pro
   prekladac cparser (vlastni model pameti: polozka struktury = 8 bajtu,
   struktury hodnotou nepredava) a pro generator ../cmini/instr.cc.
   AsmProgram, AsmOp, AsmNode, AsmLabel jsou zde jen ukazatele.

   Operand (AsmPReg, ...) patri programu p: plati do AsmDestroy / AsmFree,
   instrukce a data si ho zkopiruji (lze ho pouzit opakovane). */

AsmProgram * AsmCreate (void);                 /* novy program (AsmInit) */
void         AsmDestroy (AsmProgram * p);      /* AsmFree a uvolneni */

AsmOp *    AsmPReg (AsmProgram * p, Register r);
AsmOp *    AsmPXmm (AsmProgram * p, int n);
AsmOp *    AsmPImm (AsmProgram * p, int64_t v);
AsmOp *    AsmPImmHex (AsmProgram * p, int64_t v);
AsmOp *    AsmPImmU (AsmProgram * p, uint64_t v);
AsmOp *    AsmPImmHexLit (AsmProgram * p, int64_t v);
AsmOp *    AsmPMem (AsmProgram * p, int size, Register base, int64_t disp);
AsmOp *    AsmPMemIdx (AsmProgram * p, int size, Register base, Register index, int scale, int64_t disp);
AsmOp *    AsmPMemLab (AsmProgram * p, int size, AsmLabel * label, int64_t disp);
AsmOp *    AsmPLab (AsmProgram * p, AsmLabel * label);
AsmOp *    AsmPOffset (AsmProgram * p, AsmLabel * label);
AsmOp *    AsmPStr (AsmProgram * p, const char * s);
AsmOp *    AsmPDup (AsmProgram * p, int count, int64_t value);

/* instrukce s 0 .. 3 operandy (NULL = operand neni; za NULL uz zadny) */
AsmNode *  AsmInsP (AsmProgram * p, keyword inst, const AsmOp * a, const AsmOp * b, const AsmOp * c);

/* data:  AsmDataP - uzel s jednou polozkou (item NULL = bez polozek),
          AsmDataAdd - dalsi polozka na konec uzlu n */
AsmNode *  AsmDataP (AsmProgram * p, AsmLabel * name, keyword dir, const AsmOp * item);
AsmNode *  AsmDataAdd (AsmNode * n, const AsmOp * item);

#ifdef __cplusplus
}
#endif

#endif
