/* CMINIRT.C - runtime library for programs compiled by translate.c (see cminirt.h) */

#include "cminirt.h"
#include <stdio.h>

static void cm_fatal (const char * what)
{
   fflush (stdout);
   fprintf (stderr, "Assertion failed in function %s\n", what);
   abort ();
}

/****************************** MEMORY *******************************/

void * cm_new (size_t size)
{
   void * p = calloc (1, size > 0 ? size : 1);
   if (p == NULL) cm_fatal ("new (out of memory)");
   return p;
}

void * cm_new_array (long n, size_t size)
{
   if (n < 0) cm_fatal ("new (negative size)");
   return cm_new (n * size);
}

void cm_free (void * p)
{
   free (p);
}

void cm_pure_virtual (void)
{
   fflush (stdout);
   fprintf (stderr, "Pure virtual method called\n");
   abort ();
}

/***************************** EXCEPTIONS ******************************/

/* stack of try blocks: jmp_buf and list of handler types ("|int|Fault|...|"); "" = cleanup block
   (cmm-classes.cc exc_lower: destructors of local objects, end of handler) */
#define CM_TRY_MAX 1024
#define CM_EXC_MAX 64

static jmp_buf      cm_try_buf [CM_TRY_MAX];
static const char * cm_try_catches [CM_TRY_MAX];
static int          cm_try_top = 0;

/* propagating exception (between throw and catch) */
static void *       cm_exc_ptr = NULL;
static const char * cm_exc_types = NULL;
static void      (* cm_exc_drop) (void *) = NULL;

/* exceptions being handled (nested handlers) */
static void *       cm_hnd_ptr [CM_EXC_MAX];
static const char * cm_hnd_types [CM_EXC_MAX];
static void      (* cm_hnd_drop [CM_EXC_MAX]) (void *);
static int          cm_hnd_top = 0;

/* handler left by return with a value (cm_exc_end (2)) - the exception is cleared only at the
   next call (the value of the expression after return may still be read from it) */
static void *       cm_dead_ptr = NULL;
static void      (* cm_dead_drop) (void *) = NULL;

static void cm_exc_bury (void)
{
   void * p = cm_dead_ptr;
   if (p == NULL) return;
   cm_dead_ptr = NULL;
   if (cm_dead_drop != NULL) cm_dead_drop (p);
   else free (p);
}

void * cm_try_push (const char * catches)
{
   cm_exc_bury ();
   if (cm_try_top >= CM_TRY_MAX)
   {
      fflush (stdout);
      fprintf (stderr, "Too many nested try blocks\n");
      abort ();
   }
   cm_try_catches [cm_try_top] = catches != NULL ? catches : "";
   return & cm_try_buf [cm_try_top ++];
}

void cm_try_pop (void)
{
   if (cm_try_top > 0) cm_try_top --;
}

void cm_exc_keep (void * p)
{
   (void) p;
}

/* type (name without |) is in the list "|a|b|" */
static int cm_exc_has (const char * list, const char * type, size_t n)
{
   const char * p;
   if (list == NULL) return 0;
   for (p = strstr (list, "|"); p != NULL; p = strstr (p + 1, "|"))
      if (strncmp (p + 1, type, n) == 0 && p [n + 1] == '|') return 1;
   return 0;
}

/* some try block catches the propagating exception (otherwise as g++ terminate without cleanup) */
static int cm_exc_caught (void)
{
   int k;
   for (k = cm_try_top - 1; k >= 0; k --)
   {
      const char * c = cm_try_catches [k];
      while (* c == '|')
      {
         const char * e = strchr (c + 1, '|');
         size_t n;
         if (e == NULL) break;
         n = (size_t) (e - c - 1);
         if (n > 0 && (strncmp (c + 1, "...", n) == 0 || cm_exc_has (cm_exc_types, c + 1, n))) return 1;
         c = e;
      }
   }
   return 0;
}

static void cm_unwind (void)
{
   if (! cm_exc_caught ())
   {
      /* as g++: terminate called after throwing an instance of 'int' */
      const char * t = cm_exc_types != NULL ? cm_exc_types + 1 : "?";
      const char * e = strchr (t, '|');
      fflush (stdout);
      fprintf (stderr, "terminate called after throwing an instance of '%.*s'\n",
               (int) (e != NULL ? e - t : (long) strlen (t)), t);
      abort ();
   }
   cm_try_top --;
   longjmp (cm_try_buf [cm_try_top], 1);
}

/* throw x; - copy of value (drop: destructor of the copy, delete; NULL => free) */
void cm_throw (const char * types, void * data, int size, void (* drop) (void *))
{
   void * p;
   cm_exc_bury ();
   p = malloc (size > 0 ? size : 1);
   memcpy (p, data, size);
   cm_exc_ptr = p;
   cm_exc_types = types;
   cm_exc_drop = drop;
   cm_unwind ();
}

/* throw; - rethrows the exception being handled */
void cm_rethrow (void)
{
   if (cm_hnd_top == 0)
   {
      fflush (stdout);
      fprintf (stderr, "terminate called without an active exception\n");
      abort ();
   }
   cm_exc_ptr = cm_hnd_ptr [cm_hnd_top - 1];
   cm_exc_types = cm_hnd_types [cm_hnd_top - 1];
   cm_exc_drop = cm_hnd_drop [cm_hnd_top - 1];
   cm_unwind ();
}

/* end of cleanup block - exception propagates further */
void cm_exc_resume (void)
{
   if (cm_exc_types == NULL)
   {
      fflush (stdout);
      fprintf (stderr, "terminate called without an active exception\n");
      abort ();
   }
   cm_unwind ();
}

int cm_catch (const char * type)
{
   if (strcmp (type, "...") == 0) return 1;
   return cm_exc_has (cm_exc_types, type, strlen (type));
}

/* start of handler: propagating exception => being handled */
int cm_exc_begin (void)
{
   cm_exc_bury ();
   if (cm_hnd_top >= CM_EXC_MAX)
   {
      fflush (stdout);
      fprintf (stderr, "Too many nested exception handlers\n");
      abort ();
   }
   cm_hnd_ptr [cm_hnd_top] = cm_exc_ptr;
   cm_hnd_types [cm_hnd_top] = cm_exc_types;
   cm_hnd_drop [cm_hnd_top] = cm_exc_drop;
   cm_hnd_top ++;
   cm_exc_ptr = NULL;
   cm_exc_types = NULL;
   cm_exc_drop = NULL;
   return 0;
}

void * cm_exc_data (void)
{
   return cm_hnd_top > 0 ? cm_hnd_ptr [cm_hnd_top - 1] : NULL;
}

/* end of handler: escaped 0 - normally (the exception is cleared), 1 - the exception left the handler
   (throw; => the same exception propagates further, otherwise the handled one is cleared), 2 - return with a value */
void cm_exc_end (int escaped)
{
   void * p;
   void (* drop) (void *);
   cm_exc_bury ();
   if (cm_hnd_top == 0) return;
   cm_hnd_top --;
   p = cm_hnd_ptr [cm_hnd_top];
   drop = cm_hnd_drop [cm_hnd_top];
   if (escaped == 2)
   {
      cm_dead_ptr = p;
      cm_dead_drop = drop;
      return;
   }
   if (! (escaped && p == cm_exc_ptr))
   {
      if (drop != NULL) drop (p);
      else free (p);
   }
   if (escaped) cm_exc_resume ();
}

/****************************** STRING ******************************/

/* header before the string characters; magic distinguishes a string from cm_alloc_str from another pointer
   (string literal, char * from the C library) - for those cm_retain and cm_release are empty */
typedef struct
{
   unsigned int magic;
   int          refs;
}
   cm_strhdr;

#define CM_STR_MAGIC 0x52545343u

static long        cm_str_alive = 0;      /* live strings (CM_STR_STATS) */
static int         cm_rc_on = 0;          /* stack enabled (first cm_tmp_mark) */
static cm_string * cm_pool = NULL;        /* temporary strings (each holds 1 reference) */
static long        cm_pool_n = 0;
static long        cm_pool_cap = 0;

static void cm_str_stats (void)
{
   fprintf (stderr, "cm strings alive: %ld\n", cm_str_alive);
}

static cm_strhdr * cm_hdr (const char * s)
{
   if (s == NULL) return NULL;
   cm_strhdr * h = ((cm_strhdr *) s) - 1;
   return h->magic == CM_STR_MAGIC ? h : NULL;
}

/* new string - 1 reference belongs to the stack (if enabled) */
static cm_string cm_pool_push (cm_string s)
{
   if (! cm_rc_on || s == NULL) return s;
   if (cm_pool_n == cm_pool_cap)
   {
      cm_pool_cap = cm_pool_cap == 0 ? 256 : 2 * cm_pool_cap;
      cm_pool = (cm_string *) realloc (cm_pool, cm_pool_cap * sizeof (cm_string));
      if (cm_pool == NULL) cm_fatal ("new (out of memory)");
   }
   cm_pool [cm_pool_n ++] = s;
   return s;
}

static cm_string cm_alloc_str (size_t n)
{
   static int first = 1;
   if (first)
   {
      first = 0;
      if (getenv ("CM_STR_STATS") != NULL) atexit (cm_str_stats);
   }
   cm_strhdr * h = (cm_strhdr *) cm_new (sizeof (cm_strhdr) + n + 1);
   h->magic = CM_STR_MAGIC;
   h->refs = 1;
   cm_str_alive ++;
   return cm_pool_push ((cm_string) (h + 1));
}

long cm_tmp_mark (void)
{
   cm_rc_on = 1;
   return cm_pool_n;
}

int cm_tmp_active (void)
{
   return cm_rc_on;
}

void cm_tmp_end (long mark)
{
   while (cm_pool_n > mark)
      cm_release (cm_pool [-- cm_pool_n]);
}

cm_string cm_tmp_own (cm_string s)
{
   if (! cm_rc_on) return s;
   if (cm_hdr (s) == NULL) return s;
   return cm_pool_push (s);
}

cm_string cm_retain (cm_string s)
{
   cm_strhdr * h = cm_hdr (s);
   if (h != NULL && h->refs > 0) h->refs ++;
   return s;
}

void cm_release (cm_string s)
{
   cm_strhdr * h = cm_hdr (s);
   if (h != NULL && h->refs > 0 && -- h->refs == 0)
   {
      h->magic = 0;
      cm_str_alive --;
      free (h);
   }
}

cm_string cm_store (cm_string * p, cm_string v)
{
   cm_retain (v);
   * p = v;
   return v;
}

cm_string cm_assign (cm_string * p, cm_string v)
{
   cm_string old = * p;
   cm_retain (v);
   * p = v;
   cm_release (old);
   return v;
}

cm_string cm_str (const char * p)
{
   if (p == NULL) return NULL;
   size_t n = strlen (p);
   cm_string r = cm_alloc_str (n);
   memcpy (r, p, n);
   return r;
}

cm_string cm_str_n (const char * p, long n)
{
   if (n < 0) n = 0;
   cm_string r = cm_alloc_str (n);
   if (p != NULL) memcpy (r, p, n);
   return r;
}

cm_string cm_str_rep (long n, int c)
{
   if (n < 0) n = 0;
   cm_string r = cm_alloc_str (n);
   memset (r, c, n);
   return r;
}

cm_string cm_str_char (int c)
{
   cm_string r = cm_alloc_str (1);
   r [0] = (char) c;
   return r;
}

/* std::to_string (v) - by argument type (cmm generators) */
cm_string cm_str_long (long long v)
{
   char buf [32];
   snprintf (buf, sizeof (buf), "%lld", v);
   return cm_str (buf);
}

cm_string cm_str_ulong (unsigned long long v)
{
   char buf [32];
   snprintf (buf, sizeof (buf), "%llu", v);
   return cm_str (buf);
}

cm_string cm_str_double (double v)
{
   int n = snprintf (NULL, 0, "%f", v);   /* as g++: %f (1e300 => 301 digits) */
   cm_string r = cm_alloc_str (n);
   snprintf (r, n + 1, "%f", v);
   return r;
}

cm_string cm_cat (const char * a, const char * b)
{
   size_t la = (a != NULL) ? strlen (a) : 0;
   size_t lb = (b != NULL) ? strlen (b) : 0;
   cm_string r = cm_alloc_str (la + lb);
   if (la > 0) memcpy (r, a, la);
   if (lb > 0) memcpy (r + la, b, lb);
   return r;
}

/* new string already stored (cm_store, cm_assign): without the stack (cmini, basic v cmm.bin
   z g++) nobody owns the reference from creation - free it */
static void cm_drop_new (cm_string r)
{
   if (! cm_rc_on) cm_release (r);
}

/* s += b: member, element (old value stays - may be shared by a copy of the object) */
cm_string cm_append (cm_string * s, const char * b)
{
   cm_string r = cm_cat (*s, b);
   cm_store (s, r);
   cm_drop_new (r);
   return r;
}

cm_string cm_append_own (cm_string * s, const char * b)
{
   cm_string r = cm_cat (*s, b);
   cm_assign (s, r);
   cm_drop_new (r);
   return r;
}

int cm_str_set (cm_string * s, long i, int c)
{
   long len = cm_len (*s);
   if (i < 0 || i >= len) cm_fatal ("basic_string (index out of range)");
   cm_string r = cm_str_n (*s, len);
   r [i] = (char) c;
   cm_store (s, r);
   cm_drop_new (r);
   return c;
}

int cm_str_set_own (cm_string * s, long i, int c)
{
   long len = cm_len (*s);
   if (i < 0 || i >= len) cm_fatal ("basic_string (index out of range)");
   cm_string r = cm_str_n (*s, len);
   r [i] = (char) c;
   cm_assign (s, r);
   cm_drop_new (r);
   return c;
}

cm_string cm_substr (const char * s, unsigned long pos, unsigned long n)
{
   unsigned long len = cm_len (s);
   if (pos > len) cm_fatal ("basic_string::substr (out of range)");
   if (n > len - pos) n = len - pos;
   return cm_str_n (s + pos, n);
}

unsigned long cm_find (const char * s, const char * x, unsigned long pos)
{
   s = cm_cstr (s);
   x = cm_cstr (x);
   unsigned long len = strlen (s);
   if (pos > len) return CM_NPOS;
   const char * p = strstr (s + pos, x);
   return (p == NULL) ? CM_NPOS : (unsigned long) (p - s);
}

unsigned long cm_rfind (const char * s, const char * x, unsigned long pos)
{
   s = cm_cstr (s);
   x = cm_cstr (x);
   unsigned long len = strlen (s), n = strlen (x);
   if (n > len) return CM_NPOS;
   unsigned long i = len - n;
   if (pos < i) i = pos;
   for (;;)
   {
      if (strncmp (s + i, x, n) == 0) return i;
      if (i == 0) return CM_NPOS;
      i --;
   }
}

int cm_strcmp (const char * a, const char * b)
{
   return strcmp (cm_cstr (a), cm_cstr (b));
}

/***************************** SEQUENCE *****************************/

static void cm_seq_grow (cm_Seq * s, size_t esize)
{
   s->esize = (int) esize;
   if (s->size >= s->cap)
   {
      int cap = (s->cap == 0) ? 8 : 2 * s->cap;
      char * data = (char *) cm_new ((size_t) cap * esize);
      if (s->size > 0) memcpy (data, s->data, (size_t) s->size * esize);
      free (s->data);
      s->data = data;
      s->cap = cap;
   }
}

void * cm_seq_at (cm_Seq * s, long i, size_t esize)
{
   if (i < 0 || i >= s->size) cm_fatal ("Sequence (index out of range)");
   return s->data + i * esize;
}

/* x may be an element of the same container (s.add (s.get (0))) - cm_seq_grow frees the old data */
static long cm_seq_inside (cm_Seq * s, const void * x, size_t esize)
{
   const char * p = (const char *) x;
   if (s->data != NULL && p >= s->data && p < s->data + (size_t) s->size * esize)
      return (long) (p - s->data);
   return -1;
}

void cm_seq_add (cm_Seq * s, const void * x, size_t esize)
{
   long inside = cm_seq_inside (s, x, esize);
   cm_seq_grow (s, esize);
   if (inside >= 0) x = s->data + inside;
   memcpy (s->data + (size_t) s->size * esize, x, esize);
   s->size ++;
}

void cm_seq_insert_first (cm_Seq * s, const void * x, size_t esize)
{
   long inside = cm_seq_inside (s, x, esize);
   cm_seq_grow (s, esize);
   memmove (s->data + esize, s->data, (size_t) s->size * esize);
   if (inside >= 0) x = s->data + inside + esize;
   memcpy (s->data, x, esize);
   s->size ++;
}

bool cm_seq_contains (cm_Seq * s, const void * x, size_t esize)
{
   for (int i = 0; i < s->size; i++)
      if (memcmp (s->data + i * esize, x, esize) == 0) return true;
   return false;
}

bool cm_seq_contains_str (cm_Seq * s, const char * x)
{
   for (int i = 0; i < s->size; i++)
      if (cm_strcmp (((cm_string *) s->data) [i], x) == 0) return true;
   return false;
}

void cm_seq_pop (cm_Seq * s)
{
   if (s->size <= 0) cm_fatal ("Sequence (index out of range)");
   s->size --;
}

cm_Seq cm_seq_copy (cm_Seq * s)
{
   cm_Seq r = *s;
   r.cap = s->size;
   r.data = NULL;
   if (s->size > 0)
   {
      r.data = (char *) cm_new ((size_t) s->size * s->esize);
      memcpy (r.data, s->data, (size_t) s->size * s->esize);
   }
   return r;
}

cm_Seq * cm_seq_box (cm_Seq s)
{
   cm_Seq * p = CM_NEW (cm_Seq);
   *p = s;
   return p;
}

/**************************** DICTIONARY ****************************/

static unsigned long cm_hash (int kind, intptr_t key)
{
   unsigned long h;
   if (kind == CM_KSTR)
   {
      h = 5381;
      for (const char * p = cm_cstr ((const char *) key); *p != 0; p++)
         h = h * 33 + (unsigned char) *p;
   }
   else
   {
      h = (unsigned long) key;
      h ^= h >> 33;
      h *= 0xff51afd7ed558ccdUL;
      h ^= h >> 33;
   }
   return h;
}

static bool cm_key_eq (int kind, intptr_t a, intptr_t b)
{
   if (kind == CM_KSTR) return cm_strcmp ((const char *) a, (const char *) b) == 0;
   return a == b;
}

/* key index, -1 = none */
static int cm_dict_find (cm_Dict * d, int kind, intptr_t key)
{
   if (d->cap == 0) return -1;
   int i = (int) (cm_hash (kind, key) & (d->cap - 1));
   while (d->used [i])
   {
      if (cm_key_eq (kind, d->keys [i], key)) return i;
      i = (i + 1) & (d->cap - 1);
   }
   return -1;
}

static void cm_dict_insert (cm_Dict * d, intptr_t key, const void * val)
{
   int i = (int) (cm_hash (d->kind, key) & (d->cap - 1));
   while (d->used [i]) i = (i + 1) & (d->cap - 1);
   d->used [i] = 1;
   d->keys [i] = key;
   if (val != NULL) memcpy (d->vals + (size_t) i * d->vsize, val, d->vsize);
   else memset (d->vals + (size_t) i * d->vsize, 0, d->vsize);   /* new value: zeros */
   d->size ++;
}

static void cm_dict_rehash (cm_Dict * d, int cap)
{
   cm_Dict old = *d;
   d->cap = cap;
   d->size = 0;
   d->keys = (intptr_t *) cm_new ((size_t) cap * sizeof (intptr_t));
   d->vals = (char *) cm_new ((size_t) cap * d->vsize);
   d->used = (char *) cm_new ((size_t) cap);
   for (int i = 0; i < old.cap; i++)
      if (old.used [i])
         cm_dict_insert (d, old.keys [i], old.vals + (size_t) i * old.vsize);
   free (old.keys);
   free (old.vals);
   free (old.used);
}

void * cm_dict_ref (cm_Dict * d, int kind, intptr_t key, size_t vsize)
{
   int i = cm_dict_find (d, kind, key);
   if (i < 0)
   {
      d->kind = kind;
      d->vsize = (int) vsize;
      if (2 * (d->size + 1) > d->cap)
         cm_dict_rehash (d, d->cap == 0 ? 16 : 2 * d->cap);
      if (kind == CM_KSTR)
      {
         /* key: own copy (a temporary string would be freed; a constant may be without
            header) - outside the stack of temporary values */
         int on = cm_rc_on;
         cm_rc_on = 0;
         key = (intptr_t) cm_str ((const char *) key);
         cm_rc_on = on;
      }
      cm_dict_insert (d, key, NULL);
      i = cm_dict_find (d, kind, key);
   }
   return d->vals + (size_t) i * vsize;
}

void * cm_dict_get (cm_Dict * d, int kind, intptr_t key, size_t vsize)
{
   static double small [8];   /* zero value for missing key (also object) */
   static void * zero = small;
   static size_t zero_size = sizeof (small);
   int i = cm_dict_find (d, kind, key);
   if (i < 0)
   {
      if (vsize > zero_size)
      {
         zero = cm_new (vsize);
         zero_size = vsize;
      }
      memset (zero, 0, zero_size);
      return zero;
   }
   return d->vals + (size_t) i * vsize;
}

bool cm_dict_contains (cm_Dict * d, int kind, intptr_t key)
{
   return cm_dict_find (d, kind, key) >= 0;
}

void cm_dict_clear (cm_Dict * d)
{
   if (d->cap > 0)
      memset (d->used, 0, d->cap);
   d->size = 0;
}

cm_Dict cm_dict_copy (cm_Dict * d)
{
   cm_Dict r = *d;
   if (d->cap > 0)
   {
      r.keys = (intptr_t *) cm_new ((size_t) d->cap * sizeof (intptr_t));
      r.vals = (char *) cm_new ((size_t) d->cap * d->vsize);
      r.used = (char *) cm_new ((size_t) d->cap);
      memcpy (r.keys, d->keys, (size_t) d->cap * sizeof (intptr_t));
      memcpy (r.vals, d->vals, (size_t) d->cap * d->vsize);
      memcpy (r.used, d->used, (size_t) d->cap);
      if (d->kind == CM_KSTR)   /* keys: own copies (cm_dict_ref) - each dictionary +1 */
         for (int i = 0; i < d->cap; i++)
            if (d->used [i]) cm_retain ((cm_string) d->keys [i]);
   }
   return r;
}

cm_Dict * cm_dict_box (cm_Dict d)
{
   cm_Dict * p = CM_NEW (cm_Dict);
   *p = d;
   return p;
}

/************************ TYPE DESCRIPTION (cm_Type) ***********************/

const cm_Type cm_type_str = { CM_T_STR, sizeof (cm_string), NULL, NULL, 0, NULL };

/* bitwise copy of value => own copy (strings +1, new container data) */
/* copy of value: p is a bitwise copy of src, adjusted according to the description (strings +1, containers with their own
   copy, object with copy constructor - CM_T_UOBJ - that one is called with the source src) */
typedef void (* cm_copy_fn) (void * dst, const void * src);

static void cm_fix_copy (void * p, const void * src, const cm_Type * t)
{
   if (t == NULL) return;
   switch (t->kind)
   {
      case CM_T_STR:
         cm_retain (* (cm_string *) p);
         break;
      case CM_T_SEQ:
      {
         cm_Seq * s = (cm_Seq *) p;
         char * old = s->data;
         s->data = NULL;
         s->cap = s->size;
         if (s->size > 0)
         {
            s->data = (char *) cm_new ((size_t) s->size * s->esize);
            memcpy (s->data, old, (size_t) s->size * s->esize);
            if (t->elem != NULL)
               for (int i = 0; i < s->size; i++)
                  cm_fix_copy (s->data + (size_t) i * s->esize, old + (size_t) i * s->esize, t->elem);
         }
         break;
      }
      case CM_T_DICT:
      {
         cm_Dict * d = (cm_Dict *) p;
         char * old = d->vals;
         * d = cm_dict_copy (d);   /* keys +1, values at the same indexes */
         if (t->elem != NULL)
            for (int i = 0; i < d->cap; i++)
               if (d->used [i]) cm_fix_copy (d->vals + (size_t) i * d->vsize, old + (size_t) i * d->vsize, t->elem);
         break;
      }
      case CM_T_OBJ:
         for (long i = 0; i < t->nfields; i++)
            cm_fix_copy ((char *) p + t->fields [i].offset, (const char *) src + t->fields [i].offset, t->fields [i].type);
         break;
      case CM_T_UOBJ:
         if (p == src)
         {
            /* copy in place (cm_copy (x, x, t)): the source is a temporary bitwise copy */
            void * tmp = cm_new (t->size);
            memcpy (tmp, p, t->size);
            ((cm_copy_fn) (void *) t->elem) (p, tmp);   /* copy constructor T (const T &) */
            free (tmp);
         }
         else
            ((cm_copy_fn) (void *) t->elem) (p, src);
         break;
   }
}

/* mode: 0 - end of value (object: class destructor), 1 - end of destructor (only strings
   and containers; nested objects and bases run their destructors), 2 - old value on assignment
   (strings and containers also in nested objects, without destructor - as operator = in C++) */
static void cm_drop_value (void * p, const cm_Type * t, int fields_only)
{
   if (t == NULL) return;
   switch (t->kind)
   {
      case CM_T_STR:
         cm_release (* (cm_string *) p);
         * (cm_string *) p = NULL;
         break;
      case CM_T_SEQ:
      {
         cm_Seq * s = (cm_Seq *) p;
         if (t->elem != NULL)
            for (int i = s->size - 1; i >= 0; i--)
               cm_drop (s->data + (size_t) i * s->esize, t->elem);
         free (s->data);
         s->data = NULL;
         s->size = 0;
         s->cap = 0;
         break;
      }
      case CM_T_DICT:
      {
         cm_Dict * d = (cm_Dict *) p;
         cm_dict_clear_t (d, t->elem);
         free (d->keys);
         free (d->vals);
         free (d->used);
         d->keys = NULL;
         d->vals = NULL;
         d->used = NULL;
         d->cap = 0;
         break;
      }
      case CM_T_UOBJ:
         /* object with copy constructor owns resources - destructor always (also old
            value on assignment); end of destructor of the enclosing class skips it (above) */
         if (t->dtor != NULL) t->dtor (p);
         break;
      case CM_T_OBJ:
         if (t->dtor != NULL && fields_only == 0)
            t->dtor (p);   /* class destructor also frees members */
         else
            for (long i = t->nfields - 1; i >= 0; i--)
            {
               const cm_Type * ft = t->fields [i].type;
               /* class destructor: nested objects and bases free themselves (their destructors) */
               if (fields_only == 1 && ft != NULL && (ft->kind == CM_T_OBJ || ft->kind == CM_T_UOBJ)) continue;
               cm_drop_value ((char *) p + t->fields [i].offset, ft, fields_only == 2 ? 2 : 0);
            }
         break;
   }
}

void cm_drop (void * p, const cm_Type * t)
{
   cm_drop_value (p, t, 0);
}

void cm_drop_fields (void * p, const cm_Type * t)
{
   cm_drop_value (p, t, 1);
}

void cm_copy (void * dst, const void * src, const cm_Type * t)
{
   if (dst != src) memmove (dst, src, t->size);
   cm_fix_copy (dst, src, t);
}

void cm_set (void * dst, const void * src, const cm_Type * t)
{
   if (dst == src) return;
   cm_drop_value (dst, t, 2);   /* old value - without destructor (operator =) */
   memmove (dst, src, t->size);
}

void cm_copy_set (void * dst, const void * src, const cm_Type * t)
{
   if (dst == src) return;
   void * tmp = t->size <= 64 ? NULL : cm_new (t->size);
   char small [64];
   void * v = tmp != NULL ? tmp : small;
   cm_copy (v, src, t);   /* copy first - src may be part of the old value of dst */
   cm_drop_value (dst, t, 2);
   memcpy (dst, v, t->size);
   free (tmp);
}

cm_Seq cm_seq_copy_t (cm_Seq * s, const cm_Type * t)
{
   cm_Seq r;
   cm_copy (&r, s, t);
   return r;
}

cm_Dict cm_dict_copy_t (cm_Dict * d, const cm_Type * t)
{
   cm_Dict r;
   cm_copy (&r, d, t);
   return r;
}

void cm_seq_pop_t (cm_Seq * s, const cm_Type * et)
{
   cm_seq_pop (s);
   if (et != NULL) cm_drop (s->data + (size_t) s->size * s->esize, et);
}

void cm_seq_clear_t (cm_Seq * s, const cm_Type * et)
{
   if (et != NULL)
      for (int i = s->size - 1; i >= 0; i--)
         cm_drop (s->data + (size_t) i * s->esize, et);
   s->size = 0;
}

void cm_dict_clear_t (cm_Dict * d, const cm_Type * vt)
{
   for (int i = 0; i < d->cap; i++)
      if (d->used [i])
      {
         if (vt != NULL) cm_drop (d->vals + (size_t) i * d->vsize, vt);
         if (d->kind == CM_KSTR) cm_release ((cm_string) d->keys [i]);
      }
   cm_dict_clear (d);
}

void cm_seq_add_t (cm_Seq * s, const void * x, size_t esize, const cm_Type * et)
{
   cm_seq_add (s, x, esize);
   cm_fix_copy (s->data + (size_t) (s->size - 1) * esize, x, et);
}

void cm_seq_insert_first_t (cm_Seq * s, const void * x, size_t esize, const cm_Type * et)
{
   cm_seq_insert_first (s, x, esize);
   cm_fix_copy (s->data, x, et);
}

void cm_seq_change_t (cm_Seq * s, long i, const void * x, size_t esize, const cm_Type * et)
{
   void * p = cm_seq_at (s, i, esize);
   if (et != NULL) cm_copy_set (p, x, et);
   else memmove (p, x, esize);
}

/*************************** DYNAMIC_CAST ***************************/

void * cm_dyncast (void * p, size_t vptr_offset, const cm_ClassInfo * target)
{
   if (p == NULL) return NULL;
   const cm_ClassInfo * const * vt = * (const cm_ClassInfo * const * const *) ((char *) p + vptr_offset);
   for (const cm_ClassInfo * c = vt [0]; c != NULL; c = c->base)
      if (c == target || strcmp (c->name, target->name) == 0)
         return p;
   return NULL;
}

void * cm_dyncast_ref (void * p, size_t vptr_offset, const cm_ClassInfo * target)
{
   void * r = cm_dyncast (p, vptr_offset, target);
   if (r == NULL) cm_fatal ("dynamic_cast (bad_cast)");
   return r;
}

/* failed dynamic_cast <T &> for classes converted to structs (cmm --plain-classes) */
void cm_bad_cast (void)
{
   cm_fatal ("dynamic_cast (bad_cast)");
}

/*************************** GENERATORS cmm ***************************/

/* instr-asm.cc: element by value (up to 8 bytes, little endian), copy to the place */

void cm_seq_add_v (cm_Seq * s, intptr_t x, size_t esize)
{
   cm_seq_add (s, &x, esize);
}

void cm_seq_insert_first_v (cm_Seq * s, intptr_t x, size_t esize)
{
   cm_seq_insert_first (s, &x, esize);
}

long cm_seq_contains_v (cm_Seq * s, intptr_t x, size_t esize)
{
   return cm_seq_contains (s, &x, esize);
}

void cm_seq_copy_to (cm_Seq * dst, cm_Seq * src)
{
   * dst = cm_seq_copy (src);
}

void cm_dict_copy_to (cm_Dict * dst, cm_Dict * src)
{
   * dst = cm_dict_copy (src);
}
