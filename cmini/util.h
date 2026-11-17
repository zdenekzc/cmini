/* UTIL.H - pomocne funkce generatoru c-mini v C (instr.c, translate.c): retezce,
   dynamicke pole Seq, hashovaci tabulka Dict, cteni souboru.  Ciste C v podmnozine
   c-mini (prelozi gcc, tcc i cmini.bin). */

#ifndef UTIL_H
#define UTIL_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*

   Retezce: char * (nikdy NULL, "" = prazdny); nove retezce (Cat, Str, ...) se
   alokuji a neuvolnuji (generator bezi jednou na preklad).
   Seq: dynamicke pole hodnot long (cisla, ukazatele, retezce pretypovane na long).
   Dict: hashovaci tabulka long => long; klic cislo / ukazatel (Dict...) nebo
   retezec (SDict... - porovnani obsahu).  Chybejici klic: Get vraci 0, GetStr "".
   Vsechny polozky struktur jsou long nebo ukazatel (c-mini: polozka = 8 bajtu). */

typedef struct Seq
{
    long * items;
    long   size;
    long   cap;
}
    Seq;

typedef struct Dict
{
    long * keys;
    long * vals;
    long * used;     /* 0 volne, 1 obsazene */
    long   size;
    long   cap;
}
    Dict;


void * MemAlloc (long size);
char * StrN (const char * s, long n);
char * Str (const char * s);
char * Cat (const char * a, const char * b);
char * Cat3 (const char * a, const char * b, const char * c);
char * Cat4 (const char * a, const char * b, const char * c, const char * d);
char * CatChar (const char * a, long c);
bool StrEq (const char * a, const char * b);
bool StrEmpty (const char * a);
char * SubStr (const char * s, long pos, long n);
long StrFind (const char * s, const char * x, long pos);
long StrFindChar (const char * s, long c, long pos);
long StrRFind (const char * s, const char * x);
char * IntStr (long n);
char * UIntStr (unsigned long n);
void SeqAdd (Seq * s, long x);
long SeqGet (Seq * s, long i);
void SeqSet (Seq * s, long i, long x);
long SeqSize (Seq * s);
bool SeqEmpty (Seq * s);
long SeqLast (Seq * s);
void SeqPop (Seq * s);
void SeqClear (Seq * s);
void SeqInsertFirst (Seq * s, long x);
void SeqInsert (Seq * s, long pos, long x);
bool SeqContains (Seq * s, long x);
bool SeqContainsStr (Seq * s, const char * x);
Seq * SeqCopy (Seq * s);
Seq * SeqNew (void);
void DictClear (Dict * d);
long DictSize (Dict * d);
void DictStore (Dict * d, long k, long v);
bool DictContains (Dict * d, long k);
long DictGet (Dict * d, long k);
char * DictGetStr (Dict * d, long k);
void SDictStore (Dict * d, const char * k, long v);
bool SDictContains (Dict * d, const char * k);
long SDictGet (Dict * d, const char * k);
char * SDictGetStr (Dict * d, const char * k);
char * ReadFile (const char * name, long * len, bool skip_zero);

#ifdef __cplusplus
}
#endif

#endif
