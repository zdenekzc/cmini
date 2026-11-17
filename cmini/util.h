/* UTIL.H - helper functions of the c-mini generators in C (instr.c, translate.c): strings,
   dynamic array Seq, hash table Dict, file reading.  Plain C in the c-mini subset
   (compiled by gcc, tcc and cmini.bin). */

#ifndef UTIL_H
#define UTIL_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*

   Strings: char * (never NULL, "" = empty); new strings (Cat, Str, ...) are
   allocated and never freed (the generator runs once per translation).
   Seq: dynamic array of long values (numbers, pointers, strings cast to long).
   Dict: hash table long => long; key is a number / pointer (Dict...) or
   a string (SDict... - compares contents).  Missing key: Get returns 0, GetStr "".
   All struct members are long or pointers (c-mini: member = 8 bytes). */

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
    long * used;     /* 0 free, 1 in use */
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
