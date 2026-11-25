/* CMINIRT.H - runtime library for programs compiled by translate.c (cmini => C) */

/*
   std::string    cm_string = pointer to characters terminated by zero; strings are
                  not changed (+, += create a new string), so it is enough to copy
                  the pointer; NULL is an empty string (zeroed memory).
                  Reference counting (programs from cmm): a header precedes the characters
                  (magic, refs); the created string is put on the stack of temporary
                  values (cm_tmp_mark / cm_tmp_end - end of the whole expression), storing
                  into a variable, member or element increases the count (cm_retain, cm_store),
                  end of the variable decreases it (cm_release, cm_assign); the stack is
                  enabled by the first cm_tmp_mark - programs from cmini (translate.c, instr.cc)
                  do not call it and memory is not freed for them as before
   Sequence <T>   cm_Seq  - array of elements of size sizeof (T)
   Dictionary <K, V>  cm_Dict - hash table, key intptr_t (integer
                  number, pointer) or string (kind = CM_KSTR)
   A zeroed cm_Seq / cm_Dict structure is an empty container.
   Assigning a container copies it (cm_seq_copy, cm_dict_copy).
*/

#ifndef CMINIRT_H
#define CMINIRT_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* function called before main (global objects with a constructor) */
#if defined (__GNUC__) || defined (__TINYC__)
   #define CM_CONSTRUCTOR __attribute__ ((constructor))
#else
   #define CM_CONSTRUCTOR
#endif

/* variable need not be used (const from header - in C++ only in one file) */
#if defined (__GNUC__) || defined (__TINYC__)
   #define CM_UNUSED __attribute__ ((unused))
#else
   #define CM_UNUSED
#endif

/****************************** MEMORY *******************************/

void * cm_new (size_t size);                  /* zeroed memory */
void * cm_new_array (long n, size_t size);
void   cm_free (void * p);

#define CM_NEW(T)            ((T *) cm_new (sizeof (T)))
#define CM_NEW_ARRAY(T, n)   ((T *) cm_new_array ((n), sizeof (T)))

void cm_pure_virtual (void);                  /* call of a pure virtual method */

/***************************** EXCEPTIONS ******************************/

/* cmm: try / catch / throw (cmm/cmm-classes.cc exc_lower) => setjmp / longjmp
   if (_setjmp (cm_try_push ("|T|")) == 0) { body; cm_try_pop (); }
   else if (cm_catch ("T")) { int h = cm_exc_begin (); { T e = * (T *) cm_exc_data ();
      if (_setjmp (cm_try_push ("")) == 0) { { handler } cm_try_pop (); } else h = 1; }
      cm_exc_end (h); }
   else cm_exc_resume ();
   throw x;  =>  { T cm_exc_v = x; cm_throw ("|T|Base|", & cm_exc_v, sizeof (T), drop); }
   local object with destructor: { int u = 0; { T a; if (_setjmp (cm_try_push ("")) == 0)
      { rest of block cm_try_pop (); } else u = 1; } if (u) cm_exc_resume (); } */
#include <setjmp.h>
void * cm_try_push (const char * catches);    /* jmp_buf of try block ("|T|...|"; "" cleanup) */
void   cm_try_pop (void);                     /* end of try block (also return, break) */
void   cm_throw (const char * types, void * data, int size, void (* drop) (void *));
                                              /* copy of the value, longjmp; drop = delete the copy */
void   cm_rethrow (void);                     /* throw; in a handler */
void   cm_exc_resume (void);                  /* end of cleanup - exception propagates further */
int    cm_catch (const char * type);          /* exception type (also base class), "..." all */
int    cm_exc_begin (void);                   /* start of handler (returns 0) */
void * cm_exc_data (void);                    /* value of the exception being handled */
void   cm_exc_end (int escaped);              /* end of handler (1 - the exception left it) */
void   cm_exc_keep (void * p);                /* nothing: variable in memory (setjmp) */

/****************************** STRING ******************************/

typedef char * cm_string;

#define CM_NPOS ((unsigned long) -1)

cm_string     cm_str (const char * p);                 /* copy */
cm_string     cm_str_n (const char * p, long n);       /* string (p, n) */
cm_string     cm_str_rep (long n, int c);              /* string (n, c) */
cm_string     cm_str_char (int c);                     /* string (1, c) */
cm_string     cm_str_long (long long v);               /* std::to_string (v) */
cm_string     cm_str_ulong (unsigned long long v);
cm_string     cm_str_double (double v);
cm_string     cm_cat (const char * a, const char * b); /* a + b */
cm_string     cm_append (cm_string * s, const char * b); /* s += b */
int           cm_str_set (cm_string * s, long i, int c);  /* s [i] = c (copy of string) */
cm_string     cm_substr (const char * s, unsigned long pos, unsigned long n);
unsigned long cm_find (const char * s, const char * x, unsigned long pos);
unsigned long cm_rfind (const char * s, const char * x, unsigned long pos);
int           cm_strcmp (const char * a, const char * b);

/* reference counting (cmm generators) */
long          cm_tmp_mark (void);                       /* start of the whole expression (enables the stack) */
int           cm_tmp_active (void);                     /* stack enabled (the new string belongs to it) */
void          cm_tmp_end (long mark);                   /* end: free temporary strings */
cm_string     cm_tmp_own (cm_string s);                 /* +1 reference on the stack (result of return) */
cm_string     cm_retain (cm_string s);                  /* +1 (store) */
void          cm_release (cm_string s);                 /* -1 (end of variable) */
cm_string     cm_store (cm_string * p, cm_string v);    /* *p = v, +1 (member, element - old value stays) */
cm_string     cm_assign (cm_string * p, cm_string v);   /* *p = v, +1, old -1 (own variable) */
cm_string     cm_append_own (cm_string * s, const char * b);   /* s += b (own variable) */
int           cm_str_set_own (cm_string * s, long i, int c);   /* s [i] = c (own variable) */

static inline const char * cm_cstr (const char * s) { return s != NULL ? s : ""; }
static inline int  cm_len (const char * s)   { return s != NULL ? (int) strlen (s) : 0; }
static inline bool cm_empty (const char * s) { return s == NULL || s [0] == 0; }

/***************************** SEQUENCE *****************************/

typedef struct
{
   char * data;
   int    size;
   int    cap;
   int    esize;    /* element size */
}
   cm_Seq;

void * cm_seq_at (cm_Seq * s, long i, size_t esize);    /* index check */
void   cm_seq_add (cm_Seq * s, const void * x, size_t esize);
void   cm_seq_insert_first (cm_Seq * s, const void * x, size_t esize);
bool   cm_seq_contains (cm_Seq * s, const void * x, size_t esize);
bool   cm_seq_contains_str (cm_Seq * s, const char * x);
void   cm_seq_pop (cm_Seq * s);
cm_Seq cm_seq_copy (cm_Seq * s);
cm_Seq * cm_seq_box (cm_Seq s);                         /* temporary value => address */

static inline int  cm_seq_size (cm_Seq * s)  { return s->size; }
static inline bool cm_seq_empty (cm_Seq * s) { return s->size == 0; }
static inline void cm_seq_clear (cm_Seq * s) { s->size = 0; }

#define CM_SEQ_AT(T, s, i)          (* (T *) cm_seq_at ((s), (i), sizeof (T)))
#define CM_SEQ_LAST(T, s)           (* (T *) cm_seq_at ((s), (s)->size - 1, sizeof (T)))
/* (T []) { x }: also struct x (Sequence <AsmOp>) - (T) { x } would initialize only the first member */
#define CM_SEQ_ADD(T, s, x)         cm_seq_add ((s), (T []) { x }, sizeof (T))
#define CM_SEQ_INSERT_FIRST(T, s, x) cm_seq_insert_first ((s), (T []) { x }, sizeof (T))
#define CM_SEQ_CONTAINS(T, s, x)    cm_seq_contains ((s), (T []) { x }, sizeof (T))

/**************************** DICTIONARY ****************************/

enum { CM_KINT = 0, CM_KSTR = 1 };   /* key kind */

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

void * cm_dict_ref (cm_Dict * d, int kind, intptr_t key, size_t vsize);  /* inserts missing key */
void * cm_dict_get (cm_Dict * d, int kind, intptr_t key, size_t vsize);  /* missing => 0 */
bool   cm_dict_contains (cm_Dict * d, int kind, intptr_t key);
void   cm_dict_clear (cm_Dict * d);
cm_Dict cm_dict_copy (cm_Dict * d);
cm_Dict * cm_dict_box (cm_Dict d);

/* for cmm generators (instr-asm.cc): element by value (up to 8 bytes), copy to the place */
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

/************************ TYPE DESCRIPTION (cm_Type) ***********************/

/* freeing and copying values with strings and containers (cmm generators): type description of
   the value - string, container (description of the element; for a Dictionary value; NULL = element
   without freeing), class object (destructor also frees members; members with strings,
   containers and nested objects for copy). All members are 8 bytes
   (asm generators print them as dq). */

enum { CM_T_STR = 1, CM_T_SEQ = 2, CM_T_DICT = 3, CM_T_OBJ = 4,
       CM_T_UOBJ = 5 };   /* UOBJ: class with copy constructor T (const T &) - elem is the address of the
                             constructor (void (*) (void * dst, const void * src)), dtor destructor */

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
   long             size;      /* value size */
   const cm_Type *  elem;      /* SEQ element, DICT value */
   void          (* dtor) (void *);   /* OBJ: class destructor (NULL = members only) */
   long             nfields;   /* OBJ: members (also base, nested objects) */
   const cm_Field * fields;
};

extern const cm_Type cm_type_str;

void cm_drop (void * p, const cm_Type * t);                      /* end of value (zeroes it) */
void cm_drop_fields (void * p, const cm_Type * t);               /* OBJ: only strings and containers (end of destructor) */
void cm_copy (void * dst, const void * src, const cm_Type * t);  /* dst = copy of src (dst without value) */
void cm_set (void * dst, const void * src, const cm_Type * t);   /* dst = src (src owns the value - move), old value of dst is freed */
void cm_copy_set (void * dst, const void * src, const cm_Type * t); /* dst = copy of src, old value of dst is freed */

/* containers with element description (et: NULL = without freeing); t = container description */
cm_Seq  cm_seq_copy_t (cm_Seq * s, const cm_Type * t);
cm_Dict cm_dict_copy_t (cm_Dict * d, const cm_Type * t);
void cm_seq_pop_t (cm_Seq * s, const cm_Type * et);
void cm_seq_clear_t (cm_Seq * s, const cm_Type * et);
void cm_dict_clear_t (cm_Dict * d, const cm_Type * vt);
void cm_seq_add_t (cm_Seq * s, const void * x, size_t esize, const cm_Type * et);         /* copy x */
void cm_seq_insert_first_t (cm_Seq * s, const void * x, size_t esize, const cm_Type * et);
void cm_seq_change_t (cm_Seq * s, long i, const void * x, size_t esize, const cm_Type * et); /* copy x, old element is freed */

/*************************** DYNAMIC_CAST ***************************/

/* class description - first member of each virtual method table */
typedef struct cm_ClassInfo cm_ClassInfo;
struct cm_ClassInfo
{
   const char *         name;
   const cm_ClassInfo * base;
};

/* p = object, vptr_offset = offset of the pointer to the virtual method table,
   target = class sought;  classes are compared by name (multiple files) */
void * cm_dyncast (void * p, size_t vptr_offset, const cm_ClassInfo * target);
void * cm_dyncast_ref (void * p, size_t vptr_offset, const cm_ClassInfo * target);
void cm_bad_cast (void);   /* cmm --plain-classes: failed dynamic_cast <T &> */

#endif
