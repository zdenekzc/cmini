/* EXPR.H */

/* lexical analysis and expressions (translated from expr.pas) */

#ifndef EXPR_H
#define EXPR_H

#include "key.h"
#include "def.h"
#include "asmlist.h"

void OpenSource (void);
void SetSourceText (const char * const * parts, int count);  /* text in memory (NULL = file ModName) */
void CloseSource (void);

/* program as a linked list of nodes (asmlist.h, phase 2) */
void SetSourceNodes (const AsmNode * first, bool on);  /* on = false: text / file */
const AsmNode * SourceNode (void);             /* node of the current line (NULL = text) */
bool NodeDirect (const AsmNode * n);           /* instruction / label can be translated directly */
void NodeSourceText (void);                    /* line from the printed node into LineBuf (for lexical analysis) */
void NodeOperands (const AsmNode * n);         /* CurSy = first operand or EOL */
TreePtr NodeLabel (AsmLabel * l);              /* label record (after NodeDirect) */

void NextLine (void);
void NextElement (void);
void Insymbol (void);
bool SourceEof (void);    /* eof (source) as in FPC */
bool AbsExp (ExprRec * e);
card TypeConv (ExprRec e);
void Expression (ExprRec * e);

#endif /* EXPR_H */
