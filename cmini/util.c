/* UTIL.C - pomocne funkce generatoru c-mini (util.h): retezce, Seq, Dict, soubory
   (2026-11-16, CHANGELOG.txt kapitola 103: drive v instr.c, PMAP a ReadWholeFile v translate.c) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

void * MemAlloc (long size)
{
    void * p = calloc (1, size > 0 ? size : 1);
    if (p == NULL)
    {
        fprintf (stderr, "out of memory\n");
        exit (1);
    }
    return p;
}

char * StrN (const char * s, long n)
{
    char * r = (char *) MemAlloc (n + 1);
    memcpy (r, s, n);
    r [n] = 0;
    return r;
}

char * Str (const char * s)
{
    if (s == NULL) return (char *) "";
    return StrN (s, strlen (s));
}

char * Cat (const char * a, const char * b)
{
    long na = strlen (a);
    long nb = strlen (b);
    char * r = (char *) MemAlloc (na + nb + 1);
    memcpy (r, a, na);
    memcpy (r + na, b, nb);
    r [na + nb] = 0;
    return r;
}

char * Cat3 (const char * a, const char * b, const char * c)
{
    return Cat (Cat (a, b), c);
}

char * Cat4 (const char * a, const char * b, const char * c, const char * d)
{
    return Cat (Cat (Cat (a, b), c), d);
}

char * CatChar (const char * a, long c)
{
    long na = strlen (a);
    char * r = (char *) MemAlloc (na + 2);
    memcpy (r, a, na);
    r [na] = (char) c;
    r [na + 1] = 0;
    return r;
}

/* rovnost retezcu; NULL se nerovna nicemu (jako Eq v translate.c) */
bool StrEq (const char * a, const char * b)
{
    return a != NULL && b != NULL && strcmp (a, b) == 0;
}

bool StrEmpty (const char * a)
{
    return a [0] == 0;
}

/* s.substr (pos, n): n < 0 - do konce */
char * SubStr (const char * s, long pos, long n)
{
    long len = strlen (s);
    if (pos > len) pos = len;
    if (n < 0 || pos + n > len) n = len - pos;
    return StrN (s + pos, n);
}

/* s.find (x, pos): -1 = nenalezeno */
long StrFind (const char * s, const char * x, long pos)
{
    long len = strlen (s);
    if (pos > len) return -1;
    const char * p = strstr (s + pos, x);
    if (p == NULL) return -1;
    return p - s;
}

long StrFindChar (const char * s, long c, long pos)
{
    long len = strlen (s);
    long i;
    for (i = pos; i < len; i++)
        if (s [i] == c) return i;
    return -1;
}

/* s.rfind (x): -1 = nenalezeno */
long StrRFind (const char * s, const char * x)
{
    long len = strlen (s);
    long nx = strlen (x);
    long i;
    for (i = len - nx; i >= 0; i--)
        if (strncmp (s + i, x, nx) == 0) return i;
    return -1;
}

char * IntStr (long n)
{
    char buf [32];
    snprintf (buf, sizeof (buf), "%ld", n);
    return Str (buf);
}

char * UIntStr (unsigned long n)
{
    char buf [32];
    snprintf (buf, sizeof (buf), "%lu", n);
    return Str (buf);
}

/* ---- Seq ---- */

void SeqAdd (Seq * s, long x)
{
    if (s->size >= s->cap)
    {
        long cap = s->cap < 8 ? 8 : 2 * s->cap;
        long * items = (long *) MemAlloc (cap * 8);
        if (s->size > 0) memcpy (items, s->items, s->size * 8);
        if (s->items != NULL) free (s->items);
        s->items = items;
        s->cap = cap;
    }
    s->items [s->size] = x;
    s->size ++;
}

long SeqGet (Seq * s, long i)
{
    if (i < 0 || i >= s->size)
    {
        fprintf (stderr, "Seq index %ld out of range (size %ld)\n", i, s->size);
        exit (1);
    }
    return s->items [i];
}

void SeqSet (Seq * s, long i, long x)
{
    SeqGet (s, i);
    s->items [i] = x;
}

long SeqSize (Seq * s) { return s->size; }
bool SeqEmpty (Seq * s) { return s->size == 0; }
long SeqLast (Seq * s) { return SeqGet (s, s->size - 1); }

void SeqPop (Seq * s)
{
    if (s->size > 0) s->size --;
}

void SeqClear (Seq * s) { s->size = 0; }

void SeqInsertFirst (Seq * s, long x)
{
    long i;
    SeqAdd (s, 0);
    for (i = s->size - 1; i > 0; i--)
        s->items [i] = s->items [i - 1];
    s->items [0] = x;
}

/* vlozeni na pozici i (0 .. size) */
void SeqInsert (Seq * s, long pos, long x)
{
    long i;
    SeqAdd (s, 0);
    for (i = s->size - 1; i > pos; i--)
        s->items [i] = s->items [i - 1];
    s->items [pos] = x;
}

bool SeqContains (Seq * s, long x)
{
    long i;
    for (i = 0; i < s->size; i++)
        if (s->items [i] == x) return true;
    return false;
}

bool SeqContainsStr (Seq * s, const char * x)
{
    long i;
    for (i = 0; i < s->size; i++)
        if (StrEq ((char *) s->items [i], x)) return true;
    return false;
}

/* kopie (Sequence predana hodnotou) */
Seq * SeqCopy (Seq * s)
{
    Seq * r = (Seq *) MemAlloc (sizeof (Seq));
    long i;
    for (i = 0; i < s->size; i++)
        SeqAdd (r, s->items [i]);
    return r;
}

Seq * SeqNew (void)
{
    return (Seq *) MemAlloc (sizeof (Seq));
}

/* ---- Dict ---- */

static void DictPut (Dict * d, long k, long v, bool str);

static unsigned long HashStr (const char * s)
{
    unsigned long h = 5381;
    long i;
    for (i = 0; s [i] != 0; i++)
        h = h * 33 + (unsigned char) s [i];
    return h;
}

static unsigned long HashKey (long k, bool str)
{
    if (str) return HashStr ((char *) k);
    unsigned long h = (unsigned long) k;
    return (h >> 3) * 2654435761UL + h;
}

static bool KeyEq (long a, long b, bool str)
{
    if (str) return StrEq ((char *) a, (char *) b);
    return a == b;
}

/* index klice, -1 = neni */
static long DictFind (Dict * d, long k, bool str)
{
    if (d->cap == 0) return -1;
    long i = (long) (HashKey (k, str) % (unsigned long) d->cap);
    while (d->used [i] != 0)
    {
        if (KeyEq (d->keys [i], k, str)) return i;
        i = (i + 1) % d->cap;
    }
    return -1;
}

static void DictGrow (Dict * d, bool str)
{
    long old_cap = d->cap;
    long * old_keys = d->keys;
    long * old_vals = d->vals;
    long * old_used = d->used;
    long i;
    d->cap = old_cap < 16 ? 16 : 2 * old_cap;
    d->keys = (long *) MemAlloc (d->cap * 8);
    d->vals = (long *) MemAlloc (d->cap * 8);
    d->used = (long *) MemAlloc (d->cap * 8);
    d->size = 0;
    for (i = 0; i < old_cap; i++)
        if (old_used [i] != 0)
            DictPut (d, old_keys [i], old_vals [i], str);
    if (old_cap > 0)
    {
        free (old_keys);
        free (old_vals);
        free (old_used);
    }
}

static void DictPut (Dict * d, long k, long v, bool str)
{
    long i = DictFind (d, k, str);
    if (i >= 0)
    {
        d->vals [i] = v;
        return;
    }
    if (2 * (d->size + 1) > d->cap) DictGrow (d, str);
    i = (long) (HashKey (k, str) % (unsigned long) d->cap);
    while (d->used [i] != 0)
        i = (i + 1) % d->cap;
    d->used [i] = 1;
    d->keys [i] = str ? (long) Str ((char *) k) : k;   /* klic retezec: kopie */
    d->vals [i] = v;
    d->size ++;
}

void DictClear (Dict * d)
{
    long i;
    for (i = 0; i < d->cap; i++)
        d->used [i] = 0;
    d->size = 0;
}

long DictSize (Dict * d) { return d->size; }

/* klic cislo / ukazatel */
void DictStore (Dict * d, long k, long v) { DictPut (d, k, v, false); }
bool DictContains (Dict * d, long k) { return DictFind (d, k, false) >= 0; }
long DictGet (Dict * d, long k)
{
    long i = DictFind (d, k, false);
    return i >= 0 ? d->vals [i] : 0;
}
char * DictGetStr (Dict * d, long k)
{
    long i = DictFind (d, k, false);
    return i >= 0 ? (char *) d->vals [i] : (char *) "";
}

/* klic retezec */
void SDictStore (Dict * d, const char * k, long v) { DictPut (d, (long) k, v, true); }
bool SDictContains (Dict * d, const char * k) { return DictFind (d, (long) k, true) >= 0; }
long SDictGet (Dict * d, const char * k)
{
    long i = DictFind (d, (long) k, true);
    return i >= 0 ? d->vals [i] : 0;
}
char * SDictGetStr (Dict * d, const char * k)
{
    long i = DictFind (d, (long) k, true);
    return i >= 0 ? (char *) d->vals [i] : (char *) "";
}


/* ---- soubory ---- */

/* cely soubor (NULL = nelze otevrit); skip_zero - vynechat nulove bajty (zdrojovy text);
   len (neni-li NULL) = pocet bajtu vysledku */
char * ReadFile (const char * name, long * len, bool skip_zero)
{
    FILE * f = fopen (name, "rb");
    if (f == NULL) return NULL;
    long cap = 4096;
    long n = 0;
    char * buf = (char *) MemAlloc (cap + 1);
    int c;
    while ((c = fgetc (f)) != EOF)
    {
        if (c == 0 && skip_zero) continue;
        if (n >= cap)
        {
            char * b2 = (char *) MemAlloc (2 * cap + 1);
            memcpy (b2, buf, n);
            free (buf);
            buf = b2;
            cap = 2 * cap;
        }
        buf [n] = (char) c;
        n ++;
    }
    fclose (f);
    buf [n] = 0;
    if (len != NULL) * len = n;
    return buf;
}
