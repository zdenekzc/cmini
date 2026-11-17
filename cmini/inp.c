/* INP.C - vstup a chybova hlaseni (text od ovladace, ifc.h) */

#include <string.h>

#include "inp.h"
#include "out.h"

#include "ifc.h"

char   ch;         /* zpracovavany znak */
int    InpPos;     /* pozice ve vstupnim textu */
int    InpColNum;  /* cislo sloupce */
int    InpLineNum; /* cislo radky */

static const char * InpTxt;  /* vstupni text */
static int          InpLen;  /* delka vstupniho textu */

/********************************* INPUT **********************************/

void OpenInp (void)
{
  InpTxt = getInpText (); /* text od ovladace */
  if (InpTxt == NULL) InpTxt = "";
  InpLen = strlen (InpTxt);
  InpPos = 0;

  InpColNum = 0;
  InpLineNum = 1;

  NextChar (); /* precti prvni znak */
}

/* ---------------------------------------------------------------------- */

void CloseInp (void)
{
  /* v nasem pripade nic */
}

/* ---------------------------------------------------------------------- */

void NextChar (void)
{
  if (InpPos < InpLen)
  {
     ch = InpTxt [InpPos]; /* precti jeden znak */
     InpPos ++;

     InpColNum ++; /* cislo sloupce */
     if (ch == LF)
     {
        InpLineNum ++; /* cislo radky */
        InpColNum = 0;
     }
  }

  else if (InpPos == InpLen)
  {
     ch = NUL; /* znak 0 - konec vstupniho textu */
     InpPos ++;
  }

  else
  {
     Error ("End of source"); /* konec vstupniho textu jiz byl ohlasen */
  }
}

/***************************** ERROR MESSAGES *****************************/

void Error (char * msg)
{
  showError (msg);
}
