/* CMINIRT.H - behova knihovna pro programy prelozene translate.c (cmini => C) */

/*
   std::string    cm_string = ukazatel na znaky ukoncene nulou; retezce se
                  nemeni (+, += vytvori novy retezec), proto staci kopirovat
                  ukazatel; NULL je prazdny retezec (vynulovana pamet).
                  Pocitani referenci (programy z cmm): pred znaky je hlavicka
                  (magic, refs); vytvoreny retezec se vlozi na zasobnik docasnych
                  hodnot (cm_tmp_mark / cm_tmp_end - konec celeho vyrazu), ulozeni
                  do promenne, polozky, prvku zvysi pocet (cm_retain, cm_store),
                  konec promenne ho snizi (cm_release, cm_assign); zasobnik se
                  zapne prvnim cm_tmp_mark - programy z cmini (translate.c, instr.cc)
                  ho nevolaji a pamet se jim neuvolnuje jako drive
   Sequence <T>   cm_Seq  - pole prvku velikosti sizeof (T)
   Dictionary <K, V>  cm_Dict - hasovaci tabulka, klic intptr_t (cele
                  cislo, ukazatel) nebo retezec (kind = CM_KSTR)
   Vynulovana struktura cm_Seq / cm_Dict je prazdny kontejner.
   Prirazeni kontejneru je kopie (cm_seq_copy, cm_dict_copy).
*/

#ifndef CMINIRT_H
#define CMINIRT_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* funkce volana pred main (globalni objekty s konstruktorem) */
#if defined (__GNUC__) || defined (__TINYC__)
   #define CM_CONSTRUCTOR __attribute__ ((constructor))
#else
   #define CM_CONSTRUCTOR
#endif

/* promenna nemusi byt pouzita (const z hlavicky - v C++ jen v jednom souboru) */
#if defined (__GNUC__) || defined (__TINYC__)
   #define CM_UNUSED __attribute__ ((unused))
#else
   #define CM_UNUSED
#endif

/****************************** PAMET *******************************/

void * cm_new (size_t size);                  /* vynulovana pamet */
void * cm_new_array (long n, size_t size);
void   cm_free (void * p);

#define CM_NEW(T)            ((T *) cm_new (sizeof (T)))
#define CM_NEW_ARRAY(T, n)   ((T *) cm_new_array ((n), sizeof (T)))

void cm_pure_virtual (void);                  /* volani ciste virtualni metody */

/***************************** VYJIMKY ******************************/

/* cmm: try / catch / throw (cmm/cmm-classes.cc exc_lower) => setjmp / longjmp
   if (_setjmp (cm_try_push ("|T|")) == 0) { telo; cm_try_pop (); }
   else if (cm_catch ("T")) { int h = cm_exc_begin (); { T e = * (T *) cm_exc_data ();
      if (_setjmp (cm_try_push ("")) == 0) { { obsluha } cm_try_pop (); } else h = 1; }
      cm_exc_end (h); }
   else cm_exc_resume ();
   throw x;  =>  { T cm_exc_v = x; cm_throw ("|T|Baze|", & cm_exc_v, sizeof (T), drop); }
   lokalni objekt s destruktorem: { int u = 0; { T a; if (_setjmp (cm_try_push ("")) == 0)
      { zbytek bloku cm_try_pop (); } else u = 1; } if (u) cm_exc_resume (); } */
#include <setjmp.h>
void * cm_try_push (const char * catches);    /* jmp_buf bloku try ("|T|...|"; "" uklid) */
void   cm_try_pop (void);                     /* konec bloku try (i return, break) */
void   cm_throw (const char * types, void * data, int size, void (* drop) (void *));
                                              /* kopie hodnoty, longjmp; drop = delete kopie */
void   cm_rethrow (void);                     /* throw; v obsluze */
void   cm_exc_resume (void);                  /* konec uklidu - vyjimka se siri dal */
int    cm_catch (const char * type);          /* typ vyjimky (i baze tridy), "..." vse */
int    cm_exc_begin (void);                   /* zacatek obsluhy (vraci 0) */
void * cm_exc_data (void);                    /* hodnota obsluhovane vyjimky */
void   cm_exc_end (int escaped);              /* konec obsluhy (1 - opustila ji vyjimka) */
void   cm_exc_keep (void * p);                /* nic: promenna v pameti (setjmp) */

/****************************** STRING ******************************/

typedef char * cm_string;

#define CM_NPOS ((unsigned long) -1)

cm_string     cm_str (const char * p);                 /* kopie */
cm_string     cm_str_n (const char * p, long n);       /* string (p, n) */
cm_string     cm_str_rep (long n, int c);              /* string (n, c) */
cm_string     cm_str_char (int c);                     /* string (1, c) */
cm_string     cm_str_long (long long v);               /* std::to_string (v) */
cm_string     cm_str_ulong (unsigned long long v);
cm_string     cm_str_double (double v);
cm_string     cm_cat (const char * a, const char * b); /* a + b */
cm_string     cm_append (cm_string * s, const char * b); /* s += b */
int           cm_str_set (cm_string * s, long i, int c);  /* s [i] = c (kopie retezce) */
cm_string     cm_substr (const char * s, unsigned long pos, unsigned long n);
unsigned long cm_find (const char * s, const char * x, unsigned long pos);
unsigned long cm_rfind (const char * s, const char * x, unsigned long pos);
int           cm_strcmp (const char * a, const char * b);

/* pocitani referenci (generatory cmm) */
long          cm_tmp_mark (void);                       /* zacatek celeho vyrazu (zapne zasobnik) */
int           cm_tmp_active (void);                     /* zasobnik zapnuty (novy retezec mu patri) */
void          cm_tmp_end (long mark);                   /* konec: docasne retezce uvolnit */
cm_string     cm_tmp_own (cm_string s);                 /* +1 reference na zasobnik (vysledek return) */
cm_string     cm_retain (cm_string s);                  /* +1 (ulozeni) */
void          cm_release (cm_string s);                 /* -1 (konec promenne) */
cm_string     cm_store (cm_string * p, cm_string v);    /* *p = v, +1 (polozka, prvek - stara hodnota zustane) */
cm_string     cm_assign (cm_string * p, cm_string v);   /* *p = v, +1, stara -1 (vlastni promenna) */
cm_string     cm_append_own (cm_string * s, const char * b);   /* s += b (vlastni promenna) */
int           cm_str_set_own (cm_string * s, long i, int c);   /* s [i] = c (vlastni promenna) */

static inline const char * cm_cstr (const char * s) { return s != NULL ? s : ""; }
static inline int  cm_len (const char * s)   { return s != NULL ? (int) strlen (s) : 0; }
static inline bool cm_empty (const char * s) { return s == NULL || s [0] == 0; }

/***************************** SEQUENCE *****************************/

typedef struct
{
   char * data;
   int    size;
   int    cap;
   int    esize;    /* velikost prvku */
}
   cm_Seq;

void * cm_seq_at (cm_Seq * s, long i, size_t esize);    /* kontrola indexu */
void   cm_seq_add (cm_Seq * s, const void * x, size_t esize);
void   cm_seq_insert_first (cm_Seq * s, const void * x, size_t esize);
bool   cm_seq_contains (cm_Seq * s, const void * x, size_t esize);
bool   cm_seq_contains_str (cm_Seq * s, const char * x);
void   cm_seq_pop (cm_Seq * s);
cm_Seq cm_seq_copy (cm_Seq * s);
cm_Seq * cm_seq_box (cm_Seq s);                         /* docasna hodnota => adresa */

static inline int  cm_seq_size (cm_Seq * s)  { return s->size; }
static inline bool cm_seq_empty (cm_Seq * s) { return s->size == 0; }
static inline void cm_seq_clear (cm_Seq * s) { s->size = 0; }

#define CM_SEQ_AT(T, s, i)          (* (T *) cm_seq_at ((s), (i), sizeof (T)))
#define CM_SEQ_LAST(T, s)           (* (T *) cm_seq_at ((s), (s)->size - 1, sizeof (T)))
/* (T []) { x }: i struktura x (Sequence <AsmOp>) - (T) { x } by inicializoval jen prvni polozku */
#define CM_SEQ_ADD(T, s, x)         cm_seq_add ((s), (T []) { x }, sizeof (T))
#define CM_SEQ_INSERT_FIRST(T, s, x) cm_seq_insert_first ((s), (T []) { x }, sizeof (T))
#define CM_SEQ_CONTAINS(T, s, x)    cm_seq_contains ((s), (T []) { x }, sizeof (T))

/**************************** DICTIONARY ****************************/

enum { CM_KINT = 0, CM_KSTR = 1 };   /* druh klice */

typedef struct
{
   intptr_t * keys;
   char *     vals;
   char *     used;
   int        size;
   int        cap;
   int        vsize;
   int        kind;
}
   cm_Dict;

void * cm_dict_ref (cm_Dict * d, int kind, intptr_t key, size_t vsize);  /* chybejici klic vlozi */
void * cm_dict_get (cm_Dict * d, int kind, intptr_t key, size_t vsize);  /* chybejici => 0 */
bool   cm_dict_contains (cm_Dict * d, int kind, intptr_t key);
void   cm_dict_clear (cm_Dict * d);
cm_Dict cm_dict_copy (cm_Dict * d);
cm_Dict * cm_dict_box (cm_Dict d);

/* pro generatory cmm (instr-asm.cc): prvek hodnotou (do 8 bajtu), kopie do mista */
void   cm_seq_add_v (cm_Seq * s, intptr_t x, size_t esize);
void   cm_seq_insert_first_v (cm_Seq * s, intptr_t x, size_t esize);
long   cm_seq_contains_v (cm_Seq * s, intptr_t x, size_t esize);
void   cm_seq_copy_to (cm_Seq * dst, cm_Seq * src);
void   cm_dict_copy_to (cm_Dict * dst, cm_Dict * src);

static inline int  cm_dict_size (cm_Dict * d)  { return d->size; }
static inline bool cm_dict_empty (cm_Dict * d) { return d->size == 0; }

#define CM_DICT_REF(V, d, kind, k)  (* (V *) cm_dict_ref ((d), (kind), (intptr_t) (k), sizeof (V)))
#define CM_DICT_GET(V, d, kind, k)  (* (V *) cm_dict_get ((d), (kind), (intptr_t) (k), sizeof (V)))
#define CM_DICT_CONTAINS(d, kind, k) cm_dict_contains ((d), (kind), (intptr_t) (k))

/************************ POPIS TYPU (cm_Type) ***********************/

/* uvolneni a kopie hodnot s retezci a kontejnery (generatory cmm): popis typu
   hodnoty - retezec, kontejner (popis prvku, u Dictionary hodnoty; NULL = prvek
   bez uvolnovani), objekt tridy (destruktor uvolni i polozky; polozky s retezci,
   kontejnery a vnorenymi objekty pro kopii). Vsechny polozky maji 8 bajtu
   (generatory asm je vypisuji jako dq). */

enum { CM_T_STR = 1, CM_T_SEQ = 2, CM_T_DICT = 3, CM_T_OBJ = 4,
       CM_T_UOBJ = 5 };   /* UOBJ: trida s kopirovacim konstruktorem T (const T &) - elem je adresa
                             konstruktoru (void (*) (void * dst, const void * src)), dtor destruktor */

typedef struct cm_Type cm_Type;

typedef struct
{
   long            offset;
   const cm_Type * type;
}
   cm_Field;

struct cm_Type
{
   long             kind;
   long             size;      /* velikost hodnoty */
   const cm_Type *  elem;      /* SEQ prvek, DICT hodnota */
   void          (* dtor) (void *);   /* OBJ: destruktor tridy (NULL = jen polozky) */
   long             nfields;   /* OBJ: polozky (i baze, vnorene objekty) */
   const cm_Field * fields;
};

extern const cm_Type cm_type_str;

void cm_drop (void * p, const cm_Type * t);                      /* konec hodnoty (vynuluje) */
void cm_drop_fields (void * p, const cm_Type * t);               /* OBJ: jen retezce a kontejnery (konec destruktoru) */
void cm_copy (void * dst, const void * src, const cm_Type * t);  /* dst = kopie src (dst bez hodnoty) */
void cm_set (void * dst, const void * src, const cm_Type * t);   /* dst = src (src vlastni hodnota - presun), stara hodnota dst se uvolni */
void cm_copy_set (void * dst, const void * src, const cm_Type * t); /* dst = kopie src, stara hodnota dst se uvolni */

/* kontejnery s popisem prvku (et: NULL = bez uvolnovani); t = popis kontejneru */
cm_Seq  cm_seq_copy_t (cm_Seq * s, const cm_Type * t);
cm_Dict cm_dict_copy_t (cm_Dict * d, const cm_Type * t);
void cm_seq_pop_t (cm_Seq * s, const cm_Type * et);
void cm_seq_clear_t (cm_Seq * s, const cm_Type * et);
void cm_dict_clear_t (cm_Dict * d, const cm_Type * vt);
void cm_seq_add_t (cm_Seq * s, const void * x, size_t esize, const cm_Type * et);         /* kopie x */
void cm_seq_insert_first_t (cm_Seq * s, const void * x, size_t esize, const cm_Type * et);
void cm_seq_change_t (cm_Seq * s, long i, const void * x, size_t esize, const cm_Type * et); /* kopie x, stary prvek se uvolni */

/*************************** DYNAMIC_CAST ***************************/

/* popis tridy - prvni polozka kazde tabulky virtualnich metod */
typedef struct cm_ClassInfo cm_ClassInfo;
struct cm_ClassInfo
{
   const char *         name;
   const cm_ClassInfo * base;
};

/* p = objekt, vptr_offset = posun ukazatele na tabulku virtualnich metod,
   target = hledana trida;  tridy se porovnavaji podle jmena (vice souboru) */
void * cm_dyncast (void * p, size_t vptr_offset, const cm_ClassInfo * target);
void * cm_dyncast_ref (void * p, size_t vptr_offset, const cm_ClassInfo * target);
void cm_bad_cast (void);   /* cmm --plain-classes: neuspesny dynamic_cast <T &> */

#endif
