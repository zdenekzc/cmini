/* EXPR.H */

/* lexikalni analyza a vyrazy (prevedeno z expr.pas) */

#ifndef EXPR_H
#define EXPR_H

#include "key.h"
#include "def.h"
#include "asmlist.h"

void OpenSource (void);
void SetSourceText (const char * const * parts, int count);  /* text v pameti (NULL = soubor ModName) */
void CloseSource (void);

/* program jako spojovy seznam uzlu (asmlist.h, faze 2) */
void SetSourceNodes (const AsmNode * first, bool on);  /* on = false: text / soubor */
const AsmNode * SourceNode (void);             /* uzel prave radky (NULL = text) */
bool NodeDirect (const AsmNode * n);           /* instrukci / navesti lze prelozit primo */
void NodeSourceText (void);                    /* radka z tisku uzlu do LineBuf (pro lexikalni analyzu) */
void NodeOperands (const AsmNode * n);         /* CurSy = prvni operand nebo EOL */
TreePtr NodeLabel (AsmLabel * l);              /* zaznam navesti (po NodeDirect) */

void NextLine (void);
void NextElement (void);
void Insymbol (void);
bool SourceEof (void);    /* eof (source) jako ve FPC */
bool AbsExp (ExprRec * e);
card TypeConv (ExprRec e);
void Expression (ExprRec * e);

#endif /* EXPR_H */
