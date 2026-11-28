/* FRONT.C - input, error reporting and driver support of the front-end.

   Shared by both drivers:
     instr_drv.c        cmini.bin            pasm/asmlist back end (instr.c)
     translate_drv.c    cmini-translate.bin  translation to C (translate.c)

   The C printer of the old test_instr.cc driver is not used here, so the
   out.h interface (Indent, PutStr, ...) is a set of empty functions. */

#include "decl.h"
#include "lexer.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*************************** INPUT AND OUTPUT *****************************/

char ch;
int  InpPos;
int  InpColNum;
int  InpLineNum;

static char * InpText = NULL;
static int    InpLen = 0;
static const char * InpName = "";

void OpenInp (void)
{
  InpPos = 0;
  InpColNum = 0;
  InpLineNum = 1;
  NextChar ();
}

void CloseInp (void)
{
}

void NextChar (void)
{
  if (InpPos < InpLen)
  {
     ch = InpText [InpPos ++];
     InpColNum ++;
     if (ch == '\n')
     {
        InpLineNum ++;
        InpColNum = 0;
     }
  }
  else
  {
     ch = 0;
     InpPos ++;
  }
}

void Error (char * msg)
{
  if (ReplaySymbol != NULL && ReplayFile != NULL)
     fprintf (stderr, "%s:%d: error: %s\n", ReplayFile, ReplayLine, msg);
  else if (LexCurrentFile () != NULL)
     fprintf (stderr, "%s:%d: error: %s\n", LexCurrentFile (), LexCurrentLine (), msg);
  else
     fprintf (stderr, "%s:%d:%d: error: %s\n", InpName, InpLineNum, InpColNum, msg);
  exit (1);
}

void OpenOut (void)   { }
void CloseOut (void)  { }
void Indent (void)    { }
void Unindent (void)  { }
void PutChr (char c)  { (void) c; }
void PutStr (char * txt) { (void) txt; }
void PutEol (void)    { }

/********************************** API **********************************/

#ifdef __cplusplus
extern "C" {
#endif

/* -D name[=value] (called from both drivers) */
void TrDefine (char * arg)
{
  size_t n = strlen (arg) + 1;
  char * def = (char *) malloc (n);
  memcpy (def, arg, n);
  char * eq = strchr (def, '=');
  if (eq != NULL) *eq = 0;
  LexDefine (def, eq != NULL ? eq + 1 : "1");
}

/* loads source src and prepares the lexer and declarations; 0 = ok */
int TrStart (const char * src)
{
  long len = 0;
  InpText = ReadFile (src, &len, true);   /* without null bytes */
  InpLen = (int) len;
  if (InpText == NULL)
  {
     fprintf (stderr, "cannot open %s\n", src);
     return 2;
  }
  InpName = src;
  LexFileName (src);
  LexDefine ("__CPARSER__", "1");
  OpenInp ();
  OpenOut ();
  InitLex ();
  InitDecl ();
  return 0;
}

#ifdef __cplusplus
}
#endif
