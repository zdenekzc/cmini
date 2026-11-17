/* INP.H - vstup a chybova hlaseni */

#ifndef INP_H
#define INP_H

#ifdef __cplusplus
extern "C" {   /* front-end je v cistem C (decl.c, ...); g++ (instr.cc, test_instr.cc) */
#endif

#define NUL        0    /* konec vstupniho textu */
#define TAB        9
#define CR        13
#define LF        10
#define CTRLZ     26

extern char ch;        /* aktualni znak */
extern int  InpPos;     /* pozice ve vstupu */
extern int  InpColNum;  /* cislo sloupce */
extern int  InpLineNum; /* cislo radku */

void OpenInp (void);    /* otevri vstupni soubor */
void CloseInp (void);   /* zavri vstupni soubor */
void NextChar (void);   /* precti dalsi znak */

void Error (char * msg); /* vypis chybove hlaseni */

#ifdef __cplusplus
}
#endif

#endif
