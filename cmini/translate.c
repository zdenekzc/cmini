/* TRANSLATE.CC */

/*
   Preklad z cmini do jazyka C.

   cmini = podmnozina C++, kterou zpracuje cparser (decl.c, expr.c,
   stat.c, lexer.c): tridy s jednoduchou dedicnosti, konstruktory,
   destruktory, virtualni metody, dynamic_cast, reference, implicitni
   hodnoty parametru, new / delete a vestavene typy std::string,
   Sequence <T>, Dictionary <K, V>.  Prekladac precte zdrojovy text
   stejne jako generatory instr*.cc (strom v glist, printlist, classlist)
   a vypise program v C (C99), ktery pouziva behovou knihovnu cminirt.h / cminirt.c.

   Pouziti:
      cmini.bin -C [-I adresar] [-D jmeno[=hodnota]] [-o vystup.c] soubor.cc

      -I  adresar nahradnich systemovych hlavicek (cmini/ownheaders/); deklarace
          z techto souboru se do vystupu neprepisuji, misto nich se
          vlozi #include <...> (cstdio => stdio.h, ...)
      -iquote  adresar hlavicek programu (napr. ../gram pro zdroje cmm) -
          hledaji se v nem #include "...", deklarace se prekladaji

   Preklad:
      tridy        struct se vsemi polozkami (polozky baze na zacatku, takze
                   B * lze pretypovat na A *), metoda  T::f  =>  T_f (T * this, ...),
                   konstruktor T_ctor (T * this, ...) vraci this,
                   destruktor T_dtor, new T (...) => T_new (...),
                   delete p => T_delete (p)
      virtualni    ukazatel vptr na tabulku T_vtbl (polozka info = jmeno tridy
      metody       a baze pro dynamic_cast, potom ukazatele na metody),
                   volani pres T_f_virtual (this, ...)
      std::string  cm_string (char *, retezec se nemeni, NULL = "")
      Sequence     cm_Seq, makra CM_SEQ_AT, CM_SEQ_ADD, ...
      Dictionary   cm_Dict, makra CM_DICT_REF, CM_DICT_GET, ...
      reference    ukazatel (T & x => T * x, pouziti (*x))
      lokalni objekty s destruktorem - volani destruktoru na konci bloku,
                   pred return, break a continue
      globalni objekty s konstruktorem - funkce cm_global_init (pred main),
                   destruktory pres atexit
*/

#include "decl.h"
#include "lexer.h"
#include "util.h"   /* Dict, StrEq, ReadFile (spolecne s instr.c) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/******************************* UTILITIES ********************************/

/* text podle formatu (printf): Fmt (...) v nove alokovane pameti, PutF (...) na vystup;
   makra misto funkci s promennym poctem parametru (va_list) - ty cmini nezvlada
   (translate.c se prelozi i sam sebou, test_instr_lexer.bin -0);
   argumenty se vyhodnoti jen jednou, vnorene Fmt vraci kopii => FmtBuf lze sdilet */
#define FMT_MAX 262144   /* pasm: segment dat < 512 kB */
static char FmtBuf [FMT_MAX];

static int FmtCheck (int n)
{
  if (n < 0 || n >= FMT_MAX)
  {
     fprintf (stderr, "translate: text too long (Fmt)\n");
     exit (2);
  }
  return n;
}

static char * FmtDup (int n)
{
  n = FmtCheck (n);
  char * s = (char *) malloc (n + 1);
  memcpy (s, FmtBuf, n + 1);
  return s;
}

#define Fmt(...) FmtDup (snprintf (FmtBuf, FMT_MAX, __VA_ARGS__))

static void TrError (const char * msg)
/* chyba prekladu do C */
{
  fprintf (stderr, "translate: error: %s\n", msg);
  exit (1);
}

/* ---------------------------------------------------------------------- */

/* tabulka ukazatel => ukazatel (Dict z util.c; chybejici klic => NULL) */

static void * PGet (Dict * m, void * k)
{
  return (void *) DictGet (m, (long) k);
}

static void PSet (Dict * m, void * k, void * v)
{
  DictStore (m, (long) k, (long) v);
}

/******************************** OUTPUT **********************************/

typedef struct
{
  char * text;
  int    len;
  int    cap;
  int    ind;     /* odsazeni (pri prepnuti casti) */
}
  BUF;

static void BufAddChr (BUF * b, char c)
{
  if (b->len + 2 > b->cap)
  {
     b->cap = (b->cap == 0) ? 256 : 2 * b->cap;
     b->text = (char *) realloc (b->text, b->cap);
  }
  b->text [b->len ++] = c;
  b->text [b->len] = 0;
}

static const char * BufText (BUF * b)
{
  return (b->text == NULL) ? "" : b->text;
}

/* vystup je rozdelen na casti, na konci se spoji v tomto poradi */
static BUF SecTypedef;   /* typedef struct T T; */
static BUF SecTypes;     /* definice typu, konstanty */
static BUF SecProto;     /* prototypy funkci */
static BUF SecVars;      /* globalni promenne, tabulky virtualnich metod */
static BUF SecFuncs;     /* funkce ze zdrojoveho textu */
static BUF SecGen;       /* vygenerovane funkce (konstruktory, new, delete, ...) */

static BUF * Out = NULL;   /* &SecFuncs - nastavi main (cmini: adresa v inicializatoru) */
static int   Ind = 0;       /* odsazeni */
static bool  Bol = true;    /* zacatek radky */

static void Put (const char * s)
{
  for (; *s != 0; s++)
  {
     if (Bol && *s != '\n')
        for (int i = 0; i < 4 * Ind; i++) BufAddChr (Out, ' ');
     BufAddChr (Out, *s);
     Bol = (*s == '\n');
  }
}

static void PutFmtBuf (int n)
{
  FmtCheck (n);
  Put (FmtBuf);
}

#define PutF(...) PutFmtBuf (snprintf (FmtBuf, FMT_MAX, __VA_ARGS__))

static void PutLn (const char * s)
{
  Put (s);
  Put ("\n");
}

/* zachyceni vystupu do textu */

typedef struct
{
  BUF * out;
  int   ind;
  bool  bol;
  BUF   buf;
}
  CAPTURE;

static void BeginCapture (CAPTURE * c)
{
  c->out = Out;
  c->ind = Ind;
  c->bol = Bol;
  c->buf.text = NULL;
  c->buf.len = 0;
  c->buf.cap = 0;
  c->buf.ind = 0;
  Out = &c->buf;
  Ind = 0;
  Bol = false;
}

static char * EndCapture (CAPTURE * c)
{
  Out = c->out;
  Ind = c->ind;
  Bol = c->bol;
  if (c->buf.text == NULL) return Fmt ("%s", "");
  return c->buf.text;
}

/* prepnuti vystupu do jine casti */
static BUF * Section (BUF * sec)
{
  BUF * old = Out;
  Out->ind = Ind;
  Out = sec;
  Ind = sec->ind;
  Bol = sec->len == 0 || sec->text [sec->len - 1] == '\n';
  return old;
}

/***************************** SOURCE FILES *******************************/

static char * IncDirs [32];      /* -I: adresare nahradnich systemovych hlavicek */
static int    IncCnt = 0;

static Dict   Origin;            /* objekt z printlist => soubor (NULL = hlavni) */
static Dict   SysTypes;          /* typy definovane v systemovych hlavickach */

static char * Headers [64];      /* #include <...> pro vystup */
static int    HeaderCnt = 0;

static char * InternFile (const char * name)
/* stejna jmena souboru => stejny ukazatel (LexCurrentFile se uvolnuje) */
{
  static char * names [256];
  static int cnt = 0;
  if (name == NULL) return NULL;
  for (int i = 0; i < cnt; i++)
     if (StrEq (names [i], name)) return names [i];
  if (cnt >= 256) return Fmt ("%s", name);
  names [cnt] = Fmt ("%s", name);
  return names [cnt ++];
}

static const char * SystemHeader (const char * file)
/* soubor z adresare -I => jmeno hlavicky (cstdio), jinak NULL */
{
  if (file == NULL) return NULL;
  for (int i = 0; i < IncCnt; i++)
  {
     int n = strlen (IncDirs [i]);
     if (strncmp (file, IncDirs [i], n) == 0)
     {
        const char * rest = file + n;
        while (*rest == '/') rest ++;
        return rest;
     }
  }
  return NULL;
}

static void AddHeader (const char * name)
{
  static const char * map [] =
  {
     "cstdio", "stdio.h",     "cstdlib", "stdlib.h",   "cstring", "string.h",
     "cstdint", "stdint.h",   "climits", "limits.h",   "cmath", "math.h",
     "cassert", "assert.h",   "cstddef", "stddef.h",   "csetjmp", "setjmp.h",
     "csignal", "signal.h",   "ctime", "time.h",       "cctype", "ctype.h",
     "string", NULL,          "containers.h", NULL,
     NULL
  };
  for (int i = 0; map [i] != NULL; i += 2)
     if (StrEq (name, map [i]))
     {
        name = map [i + 1];
        break;
     }
  if (name == NULL) return;
  for (int i = 0; i < HeaderCnt; i++)
     if (StrEq (Headers [i], name)) return;
  if (HeaderCnt < 64) Headers [HeaderCnt ++] = Fmt ("%s", name);
}

static bool IsSystemObj (ObjectPtr obj)
{
  return SystemHeader ((const char *) PGet (&Origin, obj)) != NULL;
}

static bool IsSysType (TypePtr t)
{
  return PGet (&SysTypes, t) != NULL;
}

static void MarkSysTypes (TypePtr t, TypePtr desc)
/* typy definovane v deklaraci ze systemove hlavicky */
{
  while (t != NULL || desc != NULL)
  {
     TypePtr n = (desc != NULL) ? desc : t;
     if (n->sort == Name || n->sort == Tag) return;
     if (IsClsSort (n->sort) || n->sort == Enum)
     {
        PSet (&SysTypes, n, n);
        return;
     }
     desc = n->desc;
     t = n->from;
  }
}

/****************************** TYPE HELPERS ******************************/

static bool IsStrT (TypePtr t)  { t = VT (t); return t != NULL && t->sort == StdString; }
static bool IsRefT (TypePtr t)  { t = Resolve (t); return t != NULL && t->sort == Reference; }

static int AnonCount = 0;

static const char * ClsName (TypePtr cls)
{
  if (cls->tag == NULL) cls->tag = Fmt ("cm_anon%d", ++ AnonCount);
  return cls->tag;
}

static char * MethodName (ObjectPtr m)
/* T::f => T_f, konstruktor T_ctor, destruktor T_dtor */
{
  const char * cls = ClsName (m->owner);
  if (IsCtorDecl (m)) return Fmt ("%s_ctor", cls);
  if (IsDtorDecl (m)) return Fmt ("%s_dtor", cls);
  return Fmt ("%s_%s", cls, m->name);
}

/******************************** CLASSES *********************************/

typedef struct
{
  char *    name;      /* jmeno metody ("~" = destruktor) */
  ObjectPtr decl;      /* metoda, ktera slot zavedla (typ slotu) */
  TypePtr   intro;     /* trida, ktera slot zavedla */
  ObjectPtr impl;      /* implementace (NULL = implicitni destruktor tridy impl_cls) */
  TypePtr   impl_cls;
}
  SLOT;

typedef struct
{
  TypePtr  type;
  bool     poly;           /* ma virtualni metody (i zdedene) */
  bool     vptr_here;      /* zavadi ukazatel na tabulku virtualnich metod */
  SLOT *   slots;
  int      nslots;
  bool     emitted;        /* definice struktury je ve vystupu */

  /* pozadavky na vygenerovani (na konci prekladu) */
  bool     want_ctor, done_ctor;      /* implicitni konstruktor */
  bool     want_dtor, done_dtor;      /* implicitni destruktor */
  bool     want_new, done_new;
  bool     want_newarr, done_newarr;
  bool     want_delete, done_delete;
  bool     want_vtable, done_vtable;
  bool     want_info, done_info;
  bool     want_copy, done_copy;      /* kopie objektu s kontejnery */
  int      has_cont;                  /* -1 = neurceno */
  bool *   want_disp;                 /* dispatcher slotu */
  bool *   done_disp;
}
  CLS;

static Dict Classes;

static CLS * GetCls (TypePtr t);

static void ComputeSlots (CLS * c)
/* sloty ze spolecne tabulky (sem.c - VtGet) */
{
  TypePtr t = c->type;
  c->nslots = VtCount (t);
  c->slots = (SLOT *) calloc (c->nslots + 1, sizeof (SLOT));
  for (int i = 0; i < c->nslots; i++)
  {
     VtSlot * v = VtGet (t, i);
     c->slots [i].name = v->name;
     c->slots [i].decl = v->decl;
     c->slots [i].intro = v->intro;
     c->slots [i].impl = v->impl;
     c->slots [i].impl_cls = v->impl_cls;
  }

  /* zdedeny virtualni destruktor => implicitni destruktor teto tridy */
  for (int i = 0; i < c->nslots; i++)
     if (StrEq (c->slots [i].name, "~") && c->slots [i].impl_cls != t)
     {
        c->slots [i].impl = NULL;
        c->slots [i].impl_cls = t;
     }

  c->want_disp = (bool *) calloc (c->nslots + 1, sizeof (bool));
  c->done_disp = (bool *) calloc (c->nslots + 1, sizeof (bool));
}

static CLS * GetCls (TypePtr t)
{
  t = Resolve (t);
  CLS * c = (CLS *) PGet (&Classes, t);
  if (c != NULL) return c;

  c = (CLS *) calloc (1, sizeof (CLS));
  c->type = t;
  c->has_cont = -1;
  PSet (&Classes, t, c);

  c->poly = IsPolymorphic (t);
  c->vptr_here = VptrHere (t);
  if (c->poly) ComputeSlots (c);
  else
  {
     c->want_disp = (bool *) calloc (1, sizeof (bool));
     c->done_disp = (bool *) calloc (1, sizeof (bool));
  }
  return c;
}

static bool HasContainers (TypePtr t)
/* trida obsahuje Sequence / Dictionary (kopie objektu musi kopirovat i je) */
{
  CLS * c = GetCls (t);
  if (c->has_cont >= 0) return c->has_cont;
  c->has_cont = 0;
  bool r = t->base != NULL && HasContainers (t->base);
  for (NodePtr p = t->list.first; p != NULL && !r; p = p->next)
  {
     ObjectPtr f = p->item;
     if (!IsField (f)) continue;
     TypePtr ft = Resolve (f->type);
     if (ft->sort == StdSequence || ft->sort == StdDictionary) r = true;
     else if (IsClsSort (ft->sort) && HasContainers (ft)) r = true;
  }
  c->has_cont = r;
  return r;
}

/***************************** TYPE PRINTING ******************************/

static char * ExprText (ExprPtr e);
static char * EnumBody (TypePtr t);
static char * StructBody (TypePtr t);
static void EmitTypeDef (TypePtr t);

static ObjectPtr CurFce = NULL;      /* prekladana funkce */

static Dict TypedefDone;

static bool TypedefName (TypePtr t)
/* trida nebo vycet ze zdrojoveho textu => typedef (jmeno bez struct / enum);
   tridy i jen deklarovane (class X;) - typedef struct X X; na zacatku */
{
  if (t->tag == NULL) return false;
  if (IsSysType (t)) return t->typedef_tag;   /* typedef struct { ... } ffi_cif; ze systemove hlavicky */
  if (IsClsSort (t->sort))
  {
     if (PGet (&TypedefDone, t) == NULL)
     {
        PSet (&TypedefDone, t, t);
        BUF * old = Section (&SecTypedef);
        PutF ("typedef %s %s %s;\n", t->sort == Union ? "union" : "struct", t->tag, t->tag);
        Section (old);
     }
     return true;
  }
  return t->defined;
}

static const char * AliasName (TypePtr n);

static char * BaseSpec (TypePtr n)
/* zakladni typ deklarace */
{
  const char * cnst = n->Const ? "const " : "";

  switch (n->sort)
  {
     case Name:
     {
        TypePtr r = Resolve (n);
        if (r != NULL && IsClsSort (r->sort) && StrEq (r->tag, n->tag) && !TypedefName (r))
           return Fmt ("%s %s", r->sort == Union ? "union" : "struct", n->tag);
        if (r != NULL && (IsClsSort (r->sort) || r->sort == StdString ||
                          r->sort == StdSequence || r->sort == StdDictionary))
           cnst = "";
        return Fmt ("%s%s", cnst, AliasName (n));
     }

     case Tag:
     {
        TypePtr r = n->from;
        if (TypedefName (r)) return Fmt ("%s", r->tag);
        if (r->sort == Enum) return Fmt ("%senum %s", cnst, n->tag);
        if (r->sort == Union) return Fmt ("union %s", n->tag);
        return Fmt ("struct %s", n->tag);
     }

     case Struct:
     case Union:
     case Class:
        if (n->tag != NULL)
        {
           if (TypedefName (n)) return Fmt ("%s", n->tag);
           return Fmt ("%s %s", n->sort == Union ? "union" : "struct", n->tag);
        }
        return StructBody (n);

     case Enum:
        if (n->tag != NULL)
        {
           if (TypedefName (n)) return Fmt ("%s%s", cnst, n->tag);
           return Fmt ("%senum %s", cnst, n->tag);
        }
        return EnumBody (n);

     case StdString:   return Fmt ("cm_string");
     case StdSequence: return Fmt ("cm_Seq");
     case StdDictionary: return Fmt ("cm_Dict");

     default:
        break;
  }

  const char * sign = (n->sign == Signed) ? "signed " : (n->sign == Unsigned) ? "unsigned " : "";
  const char * len = (n->length == Short) ? "short " : (n->length == Long) ? (n->long_long ? "long long " : "long ") : "";
  const char * word = "";
  switch (n->sort)
  {
     case Char:   word = "char";   break;
     case Bool:   word = "bool";   break;
     case Int:    word = "int";    break;
     case Float:  word = "float";  break;
     case Double: word = "double"; break;
     case Void:   word = "void";   break;
     default:     word = "";       break;
  }
  char * s = Fmt ("%s%s%s%s", cnst, sign, len, word);
  int l = strlen (s);
  if (l == 0) return Fmt ("int");
  if (s [l - 1] == ' ') s [l - 1] = 0;
  return s;
}

static char * ParamsText (TypePtr ft, TypePtr this_cls, bool defn);

static char * Declarator (TypePtr t, TypePtr desc, const char * inner, char * * spec)
/* deklarator k objektu typu t (desc = typ s aliasy), inner = jmeno;
   *spec = zakladni typ */
{
  char * d = Fmt ("%s", inner);
  bool ptr = false;

  for (;;)
  {
     TypePtr n = (desc != NULL) ? desc : t;
     if (n == NULL)
     {
        *spec = Fmt ("int");
        return d;
     }
     switch (n->sort)
     {
        case Pointer:
        case Reference:
           d = Fmt ("*%s%s%s", n->Const ? " const" : "", d [0] != 0 ? " " : "", d);
           ptr = true;
           break;

        case Array:
           if (ptr) d = Fmt ("(%s)", d);
           d = Fmt ("%s [%s]", d, n->range != NULL ? ExprText (n->range) : "");
           ptr = false;
           break;

        case Function:
        case Constructor:
        case Destructor:
           if (ptr) d = Fmt ("(%s)", d);
           d = Fmt ("%s (%s)", d, ParamsText (n, NULL, false));
           ptr = false;
           break;

        default:
           *spec = BaseSpec (n);
           return d;
     }
     desc = n->desc;
     t = n->from;
  }
}

static char * TypeDecl (TypePtr t, TypePtr desc, const char * name)
/* deklarace:  typ jmeno */
{
  char * spec;
  char * d = Declarator (t, desc, name, &spec);
  if (d [0] == 0) return spec;
  return Fmt ("%s %s", spec, d);
}

static char * TypeName (TypePtr t)
/* typ bez jmena (pretypovani, sizeof, makra CM_SEQ_...) */
{
  return TypeDecl (t, NULL, "");
}

static char * ObjDecl (ObjectPtr obj, const char * name)
{
  return TypeDecl (obj->type, obj->desc, name);
}

static char * ParamsText (TypePtr ft, TypePtr this_cls, bool defn)
/* parametry funkce;  this_cls != NULL => prvni parametr  T * this;
   defn = definice (parametry bez jmena dostanou jmeno) */
{
  char * s = Fmt ("%s", "");
  int cnt = 0;
  if (this_cls != NULL)
  {
     s = Fmt ("%s * this", ClsName (this_cls));
     cnt ++;
  }
  int i = 0;
  for (NodePtr p = ft->list.first; p != NULL; p = p->next)
  {
     ObjectPtr par = p->item;
     i ++;
     if (par->type != NULL && par->type->sort == Void) continue;
     char * text;
     if (par->type == NULL)
        text = Fmt ("...");
     else
     {
        const char * name = par->name;
        if (name [0] == 0 && defn) name = Fmt ("cm_unused%d", i);
        text = ObjDecl (par, name);
     }
     s = Fmt ("%s%s%s", s, cnt > 0 ? ", " : "", text);
     cnt ++;
  }
  if (cnt == 0) return Fmt ("void");
  return s;
}

static char * FuncHead (ObjectPtr f, const char * name, TypePtr this_cls, bool defn)
/* hlavicka funkce bez storage:  int f (int a, char * b) */
{
  TypePtr ft = f->type;
  char * d = Fmt ("%s (%s)", name, ParamsText (ft, this_cls, defn));
  TypePtr rdesc = (f->desc != NULL && f->desc->sort == Function) ? f->desc->desc : NULL;
  if (IsCtorDecl (f)) return Fmt ("%s * %s", ClsName (f->owner), d);
  if (IsDtorDecl (f)) return Fmt ("void %s", d);
  return TypeDecl (ft->from, rdesc, d);
}

/***************************** TYPE DEFINITIONS ***************************/

static char * EnumBody (TypePtr t)
{
  char * s = Fmt ("enum%s%s\n{\n", t->tag != NULL ? " " : "", t->tag != NULL ? t->tag : "");
  for (NodePtr p = t->list.first; p != NULL; p = p->next)
  {
     ObjectPtr e = p->item;
     if (e->init != NULL)
        s = Fmt ("%s    %s = %s", s, e->name, ExprText (e->init));
     else
        s = Fmt ("%s    %s", s, e->name);
     s = Fmt ("%s%s\n", s, p->next != NULL ? "," : "");
  }
  return Fmt ("%s}", s);
}

static void EnsureTypes (TypePtr t, TypePtr desc)
/* definice trid a vyctu, ktere jsou soucasti typu (struct X { ... } x;) */
{
  while (t != NULL || desc != NULL)
  {
     TypePtr n = (desc != NULL) ? desc : t;
     if (n->sort == Name || n->sort == Tag) return;
     if ((IsClsSort (n->sort) || n->sort == Enum) && n->tag != NULL)
     {
        EmitTypeDef (n);
        return;
     }
     if (IsClsSort (n->sort) && n->tag == NULL)
     {
        for (NodePtr p = n->list.first; p != NULL; p = p->next)
           EnsureTypes (p->item->type, p->item->desc);
        return;
     }
     if (n->sort == Function)
        for (NodePtr p = n->list.first; p != NULL; p = p->next)
           if (p->item->type != NULL) EnsureTypes (p->item->type, p->item->desc);
     desc = n->desc;
     t = n->from;
  }
}

/* polozka odvozene tridy se stejnym jmenem jako polozka baze (skryva ji, C++) - ve
   strukture C jsou polozky baze i tridy vedle sebe => jmeno_Trida (jako c2c.cc) */
static Dict FieldNames;

/* typedef ve tride (typedef ToCpp super; ve vice tridach) => Trida_jmeno (jako c2c.cc) */
static Dict NestedTypedefs;

static const char * AliasName (TypePtr n)
{
  const char * r = n->tdef != NULL ? (const char *) PGet (&NestedTypedefs, n->tdef) : NULL;
  return r != NULL ? r : n->tag;
}

static void RenameHiddenFields (void)
{
  for (NodePtr c = classlist.first; c != NULL; c = c->next)
  {
     TypePtr t = Resolve (c->item->type);
     if (t == NULL) continue;
     for (NodePtr p = t->list.first; p != NULL; p = p->next)
        if (p->item->storage == Typedef && p->item->name [0] != 0)
           PSet (&NestedTypedefs, p->item, Fmt ("%s_%s", ClsName (t), p->item->name));
  }

  for (NodePtr c = classlist.first; c != NULL; c = c->next)
  {
     TypePtr t = Resolve (c->item->type);
     if (t == NULL || t->base == NULL) continue;
     for (NodePtr p = t->list.first; p != NULL; p = p->next)
        if (IsField (p->item) && BaseHasField (t->base, p->item->name))
           PSet (&FieldNames, p->item, Fmt ("%s_%s", p->item->name, ClsName (t)));
  }
}

static const char * FName (ObjectPtr f)
{
  const char * n = (const char *) PGet (&FieldNames, f);
  return n != NULL ? n : f->name;
}

static void FieldsText (TypePtr t, char * * s)
/* polozky struktury vcetne bazovych trid */
{
  CLS * c = GetCls (t);
  if (t->base != NULL) FieldsText (t->base, s);
  if (c->vptr_here)
     *s = Fmt ("%s    const %s_vtbl * vptr;\n", *s, ClsName (t));
  for (NodePtr p = t->list.first; p != NULL; p = p->next)
  {
     ObjectPtr f = p->item;
     if (!IsField (f)) continue;
     char * d = ObjDecl (f, (char *) FName (f));
     if (f->width != NULL) d = Fmt ("%s : %s", d, ExprText (f->width));
     *s = Fmt ("%s    %s;\n", *s, d);
  }
}

static char * StructBody (TypePtr t)
/* struct T { polozky } */
{
  char * fields = Fmt ("%s", "");
  FieldsText (t, &fields);
  if (fields [0] == 0) fields = Fmt ("    char cm_empty;\n");
  return Fmt ("%s%s%s\n{\n%s}", t->sort == Union ? "union" : "struct",
              t->tag != NULL ? " " : "", t->tag != NULL ? t->tag : "", fields);
}

static bool IsIntConstObj (ObjectPtr obj);
static void EmitMethods (TypePtr t);
static void EmitStaticMembers (TypePtr t);

static Dict TypeDone;

static void EmitTypeDef (TypePtr t)
/* definice tridy nebo vyctu do SecTypes (jednou) */
{
  if (PGet (&TypeDone, t) != NULL) return;
  if (!t->defined) return;
  PSet (&TypeDone, t, t);
  if (IsSysType (t)) return;

  if (t->sort == Enum && CurFce != NULL)
  {
     /* vycet uvnitr funkce - zustane lokalni (jmena konstant) */
     PutF ("%s;\n", EnumBody (t));
     PutF ("typedef enum %s %s;\n", t->tag, t->tag);
     return;
  }

  if (t->sort == Enum)
  {
     BUF * old = Section (&SecTypes);
     PutF ("%s;\n", EnumBody (t));
     PutF ("typedef enum %s %s;\n\n", t->tag, t->tag);
     Section (old);
     return;
  }

  /* vnorene typy a typy polozek, typedef ve tride => globalni typedef */
  if (t->base != NULL) EmitTypeDef (t->base);
  for (NodePtr p = t->list.first; p != NULL; p = p->next)
  {
     ObjectPtr f = p->item;
     if (f->type != NULL && !f->fce) EnsureTypes (f->type, f->desc);
     if (f->storage == Typedef && f->name [0] != 0)
     {
        TypePtr r = Resolve (f->type);
        if (r != NULL && (IsClsSort (r->sort) || r->sort == Enum) && StrEq (r->tag, f->name)) continue;
        BUF * old = Section (&SecTypes);
        const char * tn = (const char *) PGet (&NestedTypedefs, f);
        PutF ("typedef %s;\n\n", ObjDecl (f, tn != NULL ? tn : f->name));
        Section (old);
     }
  }
  EmitStaticMembers (t);

  CLS * c = GetCls (t);
  BUF * old = Section (&SecTypes);
  PutF ("%s;\n\n", StructBody (t));

  if (c->poly)
  {
     /* tabulka virtualnich metod:  info, potom sloty (baze nejdrive) */
     PutF ("struct %s_vtbl\n{\n", ClsName (t));
     PutLn ("    const cm_ClassInfo * info;");
     for (int i = 0; i < c->nslots; i++)
     {
        SLOT * s = &c->slots [i];
        if (StrEq (s->name, "~"))
           PutF ("    void (* cm_dtor) (%s * this);\n", ClsName (s->intro));
        else
        {
           char * d = Fmt ("(* %s) (%s)", s->name, ParamsText (s->decl->type, s->intro, false));
           PutF ("    %s;\n", TypeDecl (s->decl->type->from, NULL, d));
        }
     }
     PutLn ("};");
     Put ("\n");
  }
  Section (old);

  TypedefName (t);
  BUF * oldt = Section (&SecTypedef);
  if (c->poly) PutF ("typedef struct %s_vtbl %s_vtbl;\n", ClsName (t), ClsName (t));
  Section (oldt);

  c->emitted = true;
  EmitMethods (t);
}

/****************************** REQUESTS *********************************/

/* funkce a data generovane na konci prekladu (SecGen), prototypy hned */

static void Proto (const char * text)
{
  BUF * old = Section (&SecProto);
  PutF ("%s;\n", text);
  Section (old);
}

static char * CtorParams (TypePtr t, bool defn)
{
  if (t->ctor == NULL) return Fmt ("void");
  return ParamsText (t->ctor->type, NULL, defn);
}

static char * CtorRef (TypePtr t)
/* konstruktor tridy (implicitni => vygenerovat) */
{
  CLS * c = GetCls (t);
  if (t->ctor == NULL && !c->want_ctor)
  {
     c->want_ctor = true;
     Proto (Fmt ("static %s * %s_ctor (%s * this)", ClsName (t), ClsName (t), ClsName (t)));
  }
  return Fmt ("%s_ctor", ClsName (t));
}

static char * DtorRef (TypePtr t)
{
  CLS * c = GetCls (t);
  if (t->dtor == NULL && !c->want_dtor)
  {
     c->want_dtor = true;
     Proto (Fmt ("static void %s_dtor (%s * this)", ClsName (t), ClsName (t)));
  }
  return Fmt ("%s_dtor", ClsName (t));
}

static char * NewRef (TypePtr t)
{
  CLS * c = GetCls (t);
  if (!c->want_new)
  {
     c->want_new = true;
     Proto (Fmt ("static %s * %s_new (%s)", ClsName (t), ClsName (t), CtorParams (t, false)));
  }
  return Fmt ("%s_new", ClsName (t));
}

static char * NewArrayRef (TypePtr t)
{
  CLS * c = GetCls (t);
  if (!c->want_newarr)
  {
     c->want_newarr = true;
     Proto (Fmt ("static %s * %s_new_array (long n)", ClsName (t), ClsName (t)));
  }
  return Fmt ("%s_new_array", ClsName (t));
}

static char * DeleteRef (TypePtr t)
{
  CLS * c = GetCls (t);
  if (!c->want_delete)
  {
     c->want_delete = true;
     Proto (Fmt ("static void %s_delete (%s * p)", ClsName (t), ClsName (t)));
  }
  return Fmt ("%s_delete", ClsName (t));
}

static char * CopyRef (TypePtr t)
{
  CLS * c = GetCls (t);
  if (!c->want_copy)
  {
     c->want_copy = true;
     Proto (Fmt ("static %s %s_copy (%s * src)", ClsName (t), ClsName (t), ClsName (t)));
  }
  return Fmt ("%s_copy", ClsName (t));
}

static char * VtableRef (TypePtr t)
{
  GetCls (t)->want_vtable = true;
  return Fmt ("%s_vtable", ClsName (t));
}

static char * InfoRef (TypePtr t)
{
  CLS * c = GetCls (t);
  if (!c->want_info)
  {
     c->want_info = true;
     if (t->base != NULL) InfoRef (t->base);
  }
  return Fmt ("%s_cm_info", ClsName (t));
}

static char * DispatchRef (TypePtr cls, ObjectPtr m)
/* virtualni volani metody m (objekt tridy cls):  Intro_f_virtual */
{
  int i = VtIndex (cls, m->name);
  if (i < 0) TrError (Fmt ("Virtual method %s not found", m->name));
  SLOT * s = &GetCls (cls)->slots [i];
  TypePtr intro = s->intro;
  int j = VtIndex (intro, m->name);
  CLS * ci = GetCls (intro);
  const char * name = StrEq (m->name, "~") ? "dtor" : m->name;
  if (!ci->want_disp [j])
  {
     ci->want_disp [j] = true;
     ObjectPtr d = ci->slots [j].decl;
     if (StrEq (m->name, "~"))
        Proto (Fmt ("static void %s_dtor_virtual (%s * this)", ClsName (intro), ClsName (intro)));
     else
        Proto (Fmt ("static %s", FuncHead (d, Fmt ("%s_%s_virtual", ClsName (intro), name), intro, false)));
  }
  return Fmt ("%s_%s_virtual", ClsName (intro), name);
}

/****************************** EXPRESSIONS *******************************/

static TypePtr   CurClass = NULL;    /* trida prekladane metody (this) */
static TypePtr   CurRet = NULL;      /* typ vysledku */
static int       CurKind = 0;        /* 0 = funkce, 1 = konstruktor, 2 = destruktor */
static bool      DtorEndUsed = false;
static int       TmpCount = 0;       /* pomocne promenne (for (T x : seq)) */

static void Ex (ExprPtr e);
static void ExTo (ExprPtr e, TypePtr target);
static void Addr (ExprPtr e);

static char * ExprText (ExprPtr e)
{
  CAPTURE c;
  BeginCapture (&c);
  Ex (e);
  return EndCapture (&c);
}

static const char * OpText (SYMBOL op)
{
  switch (op)
  {
     case PLUS:         return "+";
     case MINUS:        return "-";
     case ASTERISK:     return "*";
     case SLASH:        return "/";
     case MOD:          return "%";
     case SHL:          return "<<";
     case SHR:          return ">>";
     case LESS:         return "<";
     case GREATER:      return ">";
     case LESSEQUAL:    return "<=";
     case GREATEREQUAL: return ">=";
     case EQUAL:        return "==";
     case UNEQUAL:      return "!=";
     case AND:          return "&";
     case XOR:          return "^";
     case OR:           return "|";
     case LOGAND:       return "&&";
     case LOGOR:        return "||";
     case ASSIGN:       return "=";
     case MULASSIGN:    return "*=";
     case DIVASSIGN:    return "/=";
     case MODASSIGN:    return "%=";
     case ADDASSIGN:    return "+=";
     case SUBASSIGN:    return "-=";
     case SHLASSIGN:    return "<<=";
     case SHRASSIGN:    return ">>=";
     case ANDASSIGN:    return "&=";
     case XORASSIGN:    return "^=";
     case ORASSIGN:     return "|=";
     case DPLUS:        return "++";
     case DMINUS:       return "--";
     case COMMA:        return ",";
     default:           return "?";
  }
}

static void PutQuoted (const char * txt, char quote)
{
  char buf [8];
  buf [1] = 0;
  buf [0] = quote;
  Put (buf);
  for (; *txt != 0; txt++)
  {
     char c = *txt;
     if (c >= 0 && c < 32)
        switch (c)
        {
           case '\a': Put ("\\a"); break;
           case '\b': Put ("\\b"); break;
           case '\f': Put ("\\f"); break;
           case '\n': Put ("\\n"); break;
           case '\r': Put ("\\r"); break;
           case '\t': Put ("\\t"); break;
           case '\v': Put ("\\v"); break;
           default:
              snprintf (buf, sizeof (buf), "\\%03o", c);
              Put (buf);
              buf [1] = 0;   /* dale opet jeden znak */
              break;
        }
     else
     {
        if (c == '\'' || c == '"' || c == '\\') Put ("\\");
        buf [0] = c;
        Put (buf);
     }
  }
  buf [0] = quote;
  Put (buf);
}

static bool IsPrimary (ExprPtr e)
/* vyraz neni treba zavorkovat pred postfixovym operatorem */
{
  switch (e->op)
  {
     case IDENT: case NUM: case FLT: case STR: case CHR:
     case TRUE_: case FALSE_: case NULLPTR_: case THIS_:
     case LPAR: case FCE: case LBRACK: case PERIOD: case ARROW:
        return true;
     case DYNAMIC_CAST_:
        return e->id != NULL && e->id [0] == '&';
     default:
        return false;
  }
}

static void ExP (ExprPtr e)
{
  if (IsPrimary (e))
     Ex (e);
  else
  {
     Put ("(");
     Ex (e);
     Put (")");
  }
}

static bool IsLValue (ExprPtr e)
{
  e = SkipParens (e);
  switch (e->op)
  {
     case IDENT:
        return e->obj != NULL && !e->obj->fce && !IsEnumConst (e->obj);
     case LBRACK:
     case DEREF:
        return true;
     case PERIOD:
     case ARROW:
        return e->obj != NULL && !e->obj->fce;
     case DYNAMIC_CAST_:
        return e->id != NULL && e->id [0] == '&';
     case FCE:
        return CallReturnsRef (e);
     default:
        return false;
  }
}

static Dict Renamed;                 /* staticke lokalni objekty => globalni jmeno */

static const char * VarName (ObjectPtr obj)
{
  const char * r = (const char *) PGet (&Renamed, obj);
  return (r != NULL) ? r : obj->name;
}

static void ThisExpr (TypePtr owner)
/* this pretypovany na tridu owner */
{
  if (CurClass != NULL && owner != NULL && owner != CurClass)
     PutF ("(%s *) this", ClsName (owner));
  else
     Put ("this");
}

static void IdentExpr (ExprPtr e, bool addr)
/* identifikator;  addr = adresa (jen reference: bez dereference) */
{
  ObjectPtr obj = e->obj;
  if (obj == NULL)
  {
     Put (e->id);
     return;
  }
  if (obj->fce)
  {
     if (obj->owner != NULL) Put (MethodName (obj));
     else Put (obj->name);
     return;
  }
  if (IsEnumConst (obj))
  {
     Put (obj->name);
     return;
  }

  char * text;
  if (obj->owner != NULL && obj->storage == Static)
     text = Fmt ("%s_%s", ClsName (obj->owner), obj->name);       /* staticka polozka */
  else if (CurClass != NULL && IsField (obj) && IsMemberOf (obj, CurClass))
     text = Fmt ("this->%s", FName (obj));                        /* polozka this */
  else
     text = Fmt ("%s", VarName (obj));

  if (IsRefT (obj->type) && !addr)
     PutF ("(*%s)", text);
  else
     Put (text);
}

/* ---------------------------------------------------------------------- */

static void Addr (ExprPtr e)
/* adresa l-hodnoty (docasne hodnoty kontejneru a retezce: kopie v pameti) */
{
  ExprPtr x = SkipParens (e);
  if (x->op == IDENT && x->obj != NULL && IsRefT (x->obj->type))
  {
     IdentExpr (x, true);
     return;
  }
  if (x->op == DEREF)
  {
     Ex (x->e2);
     return;
  }
  if (x->op == THIS_)
  {
     Put ("this");
     return;
  }
  if (x->op == FCE && CallReturnsRef (x))
  {
     /* volani bez dereference */
     ExprPtr save = x;
     ExprPtr tmp = NEW (Expr);
     *tmp = *save;
     tmp->type = NULL;          /* priznak: nedereferencovat */
     Ex (tmp);
     return;
  }
  if (!IsLValue (x))
  {
     TypePtr t = ET (x);
     if (t != NULL && t->sort == StdSequence) { Put ("cm_seq_box ("); Ex (x); Put (")"); return; }
     if (t != NULL && t->sort == StdDictionary) { Put ("cm_dict_box ("); Ex (x); Put (")"); return; }
     PutF ("& (%s) { ", TypeName (t));
     Ex (x);
     Put (" }");
     return;
  }
  Put ("&");
  ExP (x);
}

static void ArgList (TypePtr ft, ExprPtr args, bool first);

static void StrOperand (ExprPtr e)
/* operand retezcove operace:  std::string, char *, znak */
{
  TypePtr t = ET (e);
  if (t != NULL && t->sort != StdString && t->sort != Pointer && t->sort != Array && IsIntegerSort (t->sort))
  {
     Put ("cm_str_char (");
     Ex (e);
     Put (")");
  }
  else
     Ex (e);
}

static void ExTo (ExprPtr e, TypePtr target)
/* vyraz e prevedeny na typ target (prirazeni, parametr, return, inicializace) */
{
  TypePtr tt = Resolve (target);
  if (tt == NULL)
  {
     Ex (e);
     return;
  }

  if (tt->sort == Reference)
  {
     TypePtr rt = VT (tt->from);
     if (IsLValue (e))
     {
        TypePtr src = ET (e);
        if (rt != NULL && IsClsSort (rt->sort) && src != rt)
           PutF ("(%s *) ", ClsName (rt));
        Addr (e);
     }
     else
     {
        PutF ("& (%s) { ", TypeName (rt));
        ExTo (e, rt);
        Put (" }");
     }
     return;
  }

  TypePtr st = ET (e);

  if (tt->sort == StdString)
  {
     if (st != NULL && st->sort == StdString) Ex (e);
     else if (SkipParens (e)->op == STR) Ex (e);
     else if (IsCharPtrT (st)) { Put ("cm_str ("); Ex (e); Put (")"); }
     else { Put ("cm_str_char ("); Ex (e); Put (")"); }
     return;
  }

  if ((tt->sort == StdSequence || tt->sort == StdDictionary) && IsLValue (e))
  {
     Put (tt->sort == StdSequence ? "cm_seq_copy (" : "cm_dict_copy (");
     Addr (e);
     Put (")");
     return;
  }

  if (IsClsSort (tt->sort) && HasContainers (tt) && IsLValue (e))
  {
     PutF ("%s (", CopyRef (tt));
     Addr (e);
     Put (")");
     return;
  }

  if (tt->sort == Pointer)
  {
     TypePtr tc = PtrClass (tt);
     TypePtr sc = PtrClass (st);
     if (tc != NULL && sc != NULL && tc != sc)
     {
        PutF ("(%s *) ", ClsName (tc));
        ExP (e);
        return;
     }
  }

  Ex (e);
}

/* ---------------------------------------------------------------------- */

#define MAX_ARGS 64

static int SplitArgs (ExprPtr args, ExprPtr * list)
/* argumenty volani (a, b, c) => pole */
{
  int n = 0;
  while (args != NULL)
  {
     if (n >= MAX_ARGS) TrError ("Too many arguments");
     if (args->op == COMMA)
     {
        list [n ++] = args->e1;
        args = args->e2;
     }
     else
     {
        list [n ++] = args;
        args = NULL;
     }
  }
  return n;
}

static void ArgList (TypePtr ft, ExprPtr args, bool first)
/* argumenty podle parametru funkce ft (implicitni hodnoty);
   first = pred argumenty uz neco je (this) => carka */
{
  ExprPtr list [MAX_ARGS];
  int n = SplitArgs (args, list);
  int i = 0;
  NodePtr p = (ft != NULL) ? ft->list.first : NULL;

  for (;;)
  {
     while (p != NULL && p->item->type != NULL && p->item->type->sort == Void) p = p->next;
     if (p == NULL)
     {
        /* bez prototypu */
        for (; i < n; i++)
        {
           if (first) Put (", ");
           first = true;
           Ex (list [i]);
        }
        return;
     }
     ObjectPtr par = p->item;
     if (par->type == NULL)              /* ... */
     {
        for (; i < n; i++)
        {
           if (first) Put (", ");
           first = true;
           if (IsStrT (ET (list [i]))) { Put ("cm_cstr ("); Ex (list [i]); Put (")"); }
           else Ex (list [i]);
        }
        return;
     }
     if (i < n)
     {
        if (first) Put (", ");
        first = true;
        ExTo (list [i ++], par->desc != NULL ? par->desc : par->type);
     }
     else if (par->init != NULL)
     {
        if (first) Put (", ");
        first = true;
        ExTo (par->init, par->desc != NULL ? par->desc : par->type);
     }
     else
        return;
     p = p->next;
  }
}

static void StringMethod (ExprPtr f, ExprPtr args)
/* s.c_str (), s.length (), s.substr (p, n), ... */
{
  const char * name = f->id;
  ExprPtr list [MAX_ARGS];
  int n = SplitArgs (args, list);

  CAPTURE c;
  BeginCapture (&c);
  if (f->op == ARROW) { Put ("(*"); Ex (f->e1); Put (")"); }
  else Ex (f->e1);
  char * s = EndCapture (&c);

  if (StrEq (name, "c_str")) PutF ("cm_cstr (%s)", s);
  else if (StrEq (name, "length") || StrEq (name, "size")) PutF ("cm_len (%s)", s);
  else if (StrEq (name, "empty")) PutF ("cm_empty (%s)", s);
  else if (StrEq (name, "clear")) PutF ("(%s = NULL)", s);
  else if (StrEq (name, "reserve")) Put ("((void) 0)");
  else if (StrEq (name, "substr"))
  {
     PutF ("cm_substr (%s, ", s);
     if (n > 0) Ex (list [0]); else Put ("0");
     Put (", ");
     if (n > 1) Ex (list [1]); else Put ("CM_NPOS");
     Put (")");
  }
  else if (StrEq (name, "find") || StrEq (name, "rfind"))
  {
     PutF ("cm_%s (%s, ", name, s);
     StrOperand (list [0]);
     Put (", ");
     if (n > 1) Ex (list [1]); else Put (StrEq (name, "find") ? "0" : "CM_NPOS");
     Put (")");
  }
  else
     TrError (Fmt ("Unknown method of std::string: %s", name));
}

static void ContAddr (ExprPtr f)
/* adresa kontejneru v s.f () / p->f () */
{
  if (f->op == ARROW) Ex (f->e1);
  else Addr (f->e1);
}

static void SeqMethod (ExprPtr f, ExprPtr args, TypePtr seq)
{
  const char * name = f->id;
  TypePtr T = seq->elem;
  char * tn = TypeName (T);
  ExprPtr list [MAX_ARGS];
  SplitArgs (args, list);

  CAPTURE c;
  BeginCapture (&c);
  ContAddr (f);
  char * a = EndCapture (&c);

  if (StrEq (name, "add") || StrEq (name, "push") || StrEq (name, "insertFirst"))
  {
     PutF ("%s (%s, %s, ", StrEq (name, "insertFirst") ? "CM_SEQ_INSERT_FIRST" : "CM_SEQ_ADD", tn, a);
     ExTo (list [0], T);
     Put (")");
  }
  else if (StrEq (name, "get"))
  {
     PutF ("CM_SEQ_AT (%s, %s, ", tn, a);
     Ex (list [0]);
     Put (")");
  }
  else if (StrEq (name, "change"))
  {
     PutF ("(CM_SEQ_AT (%s, %s, ", tn, a);
     Ex (list [0]);
     Put (") = ");
     ExTo (list [1], T);
     Put (")");
  }
  else if (StrEq (name, "contains"))
  {
     if (IsStrT (T)) PutF ("cm_seq_contains_str (%s, ", a);
     else PutF ("CM_SEQ_CONTAINS (%s, %s, ", tn, a);
     ExTo (list [0], T);
     Put (")");
  }
  else if (StrEq (name, "last") || StrEq (name, "top")) PutF ("CM_SEQ_LAST (%s, %s)", tn, a);
  else if (StrEq (name, "pop")) PutF ("cm_seq_pop (%s)", a);
  else if (StrEq (name, "clear")) PutF ("cm_seq_clear (%s)", a);
  else if (StrEq (name, "size")) PutF ("cm_seq_size (%s)", a);
  else if (StrEq (name, "empty")) PutF ("cm_seq_empty (%s)", a);
  else TrError (Fmt ("Unknown method of Sequence: %s", name));
}

static const char * KeyKind (TypePtr dict)
{
  return IsStringKey (dict) ? "CM_KSTR" : "CM_KINT";
}

static void DictMethod (ExprPtr f, ExprPtr args, TypePtr dict)
{
  const char * name = f->id;
  char * vn = TypeName (dict->elem);
  ExprPtr list [MAX_ARGS];
  SplitArgs (args, list);

  CAPTURE c;
  BeginCapture (&c);
  ContAddr (f);
  char * a = EndCapture (&c);

  if (StrEq (name, "store"))
  {
     PutF ("(CM_DICT_REF (%s, %s, %s, ", vn, a, KeyKind (dict));
     ExTo (list [0], dict->key);
     Put (") = ");
     ExTo (list [1], dict->elem);
     Put (")");
  }
  else if (StrEq (name, "get"))
  {
     PutF ("CM_DICT_GET (%s, %s, %s, ", vn, a, KeyKind (dict));
     ExTo (list [0], dict->key);
     Put (")");
  }
  else if (StrEq (name, "contains"))
  {
     PutF ("CM_DICT_CONTAINS (%s, %s, ", a, KeyKind (dict));
     ExTo (list [0], dict->key);
     Put (")");
  }
  else if (StrEq (name, "clear")) PutF ("cm_dict_clear (%s)", a);
  else if (StrEq (name, "size")) PutF ("cm_dict_size (%s)", a);
  else if (StrEq (name, "empty")) PutF ("cm_dict_empty (%s)", a);
  else TrError (Fmt ("Unknown method of Dictionary: %s", name));
}

static void MethodCall (ObjectPtr m, TypePtr recv_cls, const char * recv, bool virt, ExprPtr args)
/* volani metody m, recv = ukazatel na objekt tridy recv_cls */
{
  TypePtr owner = m->owner;
  if (virt && m->Virtual)
  {
     char * d = DispatchRef (recv_cls, m);
     TypePtr intro = GetCls (recv_cls)->slots [VtIndex (recv_cls, m->name)].intro;
     if (intro != recv_cls) PutF ("%s ((%s *) %s", d, ClsName (intro), recv);
     else PutF ("%s (%s", d, recv);
  }
  else
  {
     if (owner != recv_cls) PutF ("%s ((%s *) %s", MethodName (m), ClsName (owner), recv);
     else PutF ("%s (%s", MethodName (m), recv);
  }
  ArgList (m->type, args, true);
  Put (")");
}

static void Call (ExprPtr e)
/* volani funkce nebo metody */
{
  ExprPtr f = SkipParens (e->e1);
  ExprPtr args = e->e2;

  if (f->op == IDENT && f->obj != NULL && f->obj->fce)
  {
     ObjectPtr m = f->obj;
     if (m->owner != NULL && m->method)
     {
        MethodCall (m, CurClass, "this", true, args);       /* f () v metode */
        return;
     }
     if (m->owner != NULL)
        Put (MethodName (m));                                /* staticka metoda */
     else if ((StrEq (m->name, "cparser_stderr") || StrEq (m->name, "cparser_stdout")) && IsSystemObj (m))
     {
        Put (m->name + 8);                                   /* cmini/ownheaders/cstdio: stderr, stdout */
        return;
     }
     else
        Put (m->name);
     Put (" (");
     ArgList (m->type, args, false);
     Put (")");
     return;
  }

  if (f->op == DCOLON)                                       /* T::f () - bez virtualniho volani */
  {
     ObjectPtr m = f->obj;
     CAPTURE c;
     BeginCapture (&c);
     ThisExpr (m->owner);
     char * recv = EndCapture (&c);
     PutF ("%s (%s", MethodName (m), recv);
     ArgList (m->type, args, true);
     Put (")");
     return;
  }

  if (f->op == PERIOD || f->op == ARROW)
  {
     TypePtr rt = ET (f->e1);
     if (f->op == ARROW && rt != NULL && rt->sort == Pointer) rt = VT (rt->from);

     if (rt != NULL && rt->sort == StdString) { StringMethod (f, args); return; }
     if (rt != NULL && rt->sort == StdSequence) { SeqMethod (f, args, rt); return; }
     if (rt != NULL && rt->sort == StdDictionary) { DictMethod (f, args, rt); return; }

     ObjectPtr m = f->obj;
     if (m != NULL && m->fce)
     {
        if (!m->method)                                      /* staticka metoda pres objekt */
        {
           PutF ("%s (", MethodName (m));
           ArgList (m->type, args, false);
           Put (")");
           return;
        }
        CAPTURE c;
        BeginCapture (&c);
        if (f->op == ARROW) Ex (f->e1);
        else Addr (f->e1);
        char * recv = EndCapture (&c);
        MethodCall (m, rt, recv, true, args);
        return;
     }
  }

  /* ukazatel na funkci */
  ExP (f);
  Put (" (");
  ArgList (CalleeType (f), args, false);
  Put (")");
}

/* ---------------------------------------------------------------------- */

static void NewExpr (ExprPtr e)
{
  TypePtr t = Resolve (e->type->from);
  if (e->e2 != NULL)                         /* new T [n] */
  {
     if (IsClsSort (t->sort) && NeedsCtor (t, false))
        PutF ("%s (", NewArrayRef (t));
     else
        PutF ("CM_NEW_ARRAY (%s, ", TypeName (e->type->from));
     Ex (e->e2);
     Put (")");
     return;
  }
  if (IsClsSort (t->sort) && NeedsCtor (t, false))
  {
     PutF ("%s (", NewRef (t));
     if (t->ctor != NULL) ArgList (t->ctor->type, e->e1, false);
     Put (")");
     return;
  }
  if (e->e1 != NULL && !IsClsSort (t->sort))
  {
     /* new int (5) */
     PutF ("((%s) memcpy (CM_NEW (%s), & (%s) { ", TypeName (e->type), TypeName (e->type->from),
           TypeName (e->type->from));
     ExTo (e->e1, e->type->from);
     PutF (" }, sizeof (%s)))", TypeName (e->type->from));
     return;
  }
  PutF ("CM_NEW (%s)", TypeName (e->type->from));
}

static void DeleteExpr (ExprPtr e)
{
  TypePtr t = PtrClass (ET (e->e2));
  bool arr = e->id != NULL && e->id [0] != 0;
  if (t != NULL && !arr && NeedsDtor (t, false))
  {
     PutF ("%s (", DeleteRef (t));
     Ex (e->e2);
     Put (")");
     return;
  }
  Put ("cm_free (");
  Ex (e->e2);
  Put (")");
}

static void CastExpr (ExprPtr e)
{
  TypePtr t = Resolve (e->type);
  if (t->sort == StdString)
  {
     ExprPtr a = e->e1;
     if (a->op == COMMA)
     {
        if (IsCharPtrT (ET (a->e1)))
        {
           Put ("cm_str_n (");             /* string (p, n) */
           Ex (a->e1);
        }
        else
        {
           Put ("cm_str_rep (");           /* string (n, c) */
           Ex (a->e1);
        }
        Put (", ");
        Ex (a->e2);
        Put (")");
        return;
     }
     ExTo (a, t);
     return;
  }
  if (t->sort == Reference)
  {
     PutF ("(* (%s *) ", TypeName (t->from));
     Addr (e->e1);
     Put (")");
     return;
  }
  PutF ("(%s) ", TypeName (e->obj != NULL ? e->obj->desc : e->type));
  ExP (e->e1);
}

static void DynCastExpr (ExprPtr e)
{
  bool ref = e->id != NULL && e->id [0] == '&';
  bool up = e->id != NULL && e->id [0] == '^';
  TypePtr target = ref ? Resolve (e->type) : PtrClass (e->type);
  TypePtr src = ref ? ET (e->e1) : PtrClass (ET (e->e1));
  if (up)
  {
     if (ref || IsClsSort (Resolve (e->type)->sort))
     {
        PutF ("(* (%s *) ", ClsName (Resolve (e->type)));
        Addr (e->e1);
        Put (")");
     }
     else
     {
        PutF ("(%s *) ", ClsName (target));
        ExP (e->e1);
     }
     return;
  }
  if (ref)
  {
     PutF ("(* (%s *) cm_dyncast_ref (", ClsName (target));
     Addr (e->e1);
  }
  else
  {
     PutF ("((%s *) cm_dyncast (", ClsName (target));
     Ex (e->e1);
  }
  PutF (", offsetof (%s, vptr), &%s))", ClsName (src), InfoRef (target));
}

static void InitList (ExprPtr e, TypePtr t);

static void AssignExpr (ExprPtr e)
{
  TypePtr t1 = ET (e->e1);
  ExprPtr l = SkipParens (e->e1);

  /* reference v konstruktoru:  : r (x) */
  if (e->op == ASSIGN && l->op == IDENT && l->obj != NULL && IsRefT (l->obj->type) && IsRefT (l->type))
  {
     IdentExpr (l, true);
     Put (" = ");
     ExTo (e->e2, l->obj->type);
     return;
  }

  /* s [i] = c  =>  cm_str_set (&s, i, c)  (kopie retezce) */
  if (e->op == ASSIGN && l->op == LBRACK && IsStrT (ET (l->e1)))
  {
     Put ("cm_str_set (");
     Addr (l->e1);
     Put (", ");
     Ex (l->e2);
     Put (", ");
     Ex (e->e2);
     Put (")");
     return;
  }

  if (t1 != NULL && t1->sort == StdString && e->op == ADDASSIGN)
  {
     Put ("cm_append (");
     Addr (e->e1);
     Put (", ");
     StrOperand (e->e2);
     Put (")");
     return;
  }

  /* rez:  base = derived  =>  (x = * (Base *) & (d), x.vptr = &Base_vtable, x)
     - kopie polozek baze, cil si ponecha tabulku virtualnich metod sveho typu */
  TypePtr c1 = t1 != NULL ? Resolve (t1) : NULL;
  TypePtr c2 = ET (e->e2) != NULL ? Resolve (ET (e->e2)) : NULL;
  if (e->op == ASSIGN && c1 != NULL && c2 != NULL && IsClsSort (c1->sort) && IsClsSort (c2->sort) && c1 != c2)
  {
     bool poly = GetCls (c1)->poly;
     if (poly) Put ("(");
     Ex (e->e1);
     PutF (" = * (%s *) & (", ClsName (c1));
     Ex (e->e2);
     Put (")");
     if (poly)
     {
        Put (", ");
        Ex (e->e1);
        PutF (".vptr = (const %s_vtbl *) &%s, ", ClsName (VptrRoot (c1)), VtableRef (c1));
        Ex (e->e1);
        Put (")");
     }
     return;
  }

  Ex (e->e1);
  PutF (" %s ", OpText (e->op));
  if (e->op == ASSIGN) ExTo (e->e2, t1);
  else Ex (e->e2);
}

static void Ex (ExprPtr e)
{
  if (e == NULL) return;

  switch (e->op)
  {
     case IDENT:
        IdentExpr (e, false);
        break;

     case NUM:
        Put (e->id);
        if (e->type != NULL && e->type->sort == Int &&
            strchr ("uUlL", e->id [strlen (e->id) - 1]) == NULL && strchr (e->id, '.') == NULL)
        {
           if (e->type->sign == Unsigned) Put ("U");
           if (e->type->length == Long) Put ("L");
        }
        break;

     case FLT:
        Put (e->id);
        break;

     case TRUE_:    Put ("true");  break;
     case FALSE_:   Put ("false"); break;
     case NULLPTR_: Put ("NULL");  break;
     case THIS_:    Put ("this");  break;

     case DCOLON:
        Put (MethodName (e->obj));
        break;

     case CHR:
        PutQuoted (e->id, '\'');
        break;

     case STR:
        PutQuoted (e->id, '"');
        break;

     case LPAR:
        Put ("(");
        Ex (e->e1);
        Put (")");
        break;

     case LBRACE:
        InitList (e, NULL);
        break;

     case FCE:
        if (e->type != NULL && CallReturnsRef (e))
        {
           Put ("(*");
           Call (e);
           Put (")");
        }
        else
           Call (e);
        break;

     case LBRACK:
     {
        TypePtr t = ET (e->e1);
        if (t != NULL && t->sort == StdSequence)
        {
           PutF ("CM_SEQ_AT (%s, ", TypeName (t->elem));
           Addr (e->e1);
           Put (", ");
           Ex (e->e2);
           Put (")");
        }
        else if (t != NULL && t->sort == StdDictionary)
        {
           PutF ("CM_DICT_REF (%s, ", TypeName (t->elem));
           Addr (e->e1);
           PutF (", %s, ", KeyKind (t));
           ExTo (e->e2, t->key);
           Put (")");
        }
        else if (t != NULL && t->sort == StdString)
        {
           Put ("cm_cstr (");
           Ex (e->e1);
           Put (") [");
           Ex (e->e2);
           Put ("]");
        }
        else
        {
           ExP (e->e1);
           Put (" [");
           Ex (e->e2);
           Put ("]");
        }
        break;
     }

     case PERIOD:
     case ARROW:
     {
        ObjectPtr obj = e->obj;
        if (obj != NULL && obj->owner != NULL && obj->storage == Static && !obj->fce)
        {
           PutF ("%s_%s", ClsName (obj->owner), obj->name);
           break;
        }
        bool ref = obj != NULL && IsRefT (obj->type);
        if (ref) Put ("(*");
        ExP (e->e1);
        Put (e->op == PERIOD ? "." : "->");
        Put (obj != NULL && IsField (obj) ? FName (obj) : e->id);
        if (ref) Put (")");
        break;
     }

     case ADR:
        Put ("&");
        ExP (e->e2);
        break;

     case DEREF:
        Put ("*");
        ExP (e->e2);
        break;

     case UNARYPLUS:
     case UNARYMINUS:
     {
        Put (e->op == UNARYPLUS ? "+" : "-");
        ExprPtr x = e->e2;
        if (x->op == UNARYPLUS || x->op == UNARYMINUS || x->op == DPLUS || x->op == DMINUS)
           Put (" ");
        ExP (x);
        break;
     }

     case UNARYNOT:
        Put ("~");
        ExP (e->e2);
        break;

     case LOGNOT:
        Put ("!");
        ExP (e->e2);
        break;

     case DPLUS:
     case DMINUS:
        if (e->e1 != NULL)
        {
           ExP (e->e1);
           Put (OpText (e->op));
        }
        else
        {
           Put (OpText (e->op));
           ExP (e->e2);
        }
        break;

     case SIZEOF_:
        Put ("sizeof (");
        if (e->e2 == NULL) Put (TypeName (e->obj->desc != NULL ? e->obj->desc : e->obj->type));
        else Ex (e->e2);
        Put (")");
        break;

     case NEW_:
        NewExpr (e);
        break;

     case DELETE_:
        DeleteExpr (e);
        break;

     case STATIC_CAST_:
        CastExpr (e);
        break;

     case DYNAMIC_CAST_:
        DynCastExpr (e);
        break;

     case QUESTION:
        Ex (e->e1);
        Put (" ? ");
        ExTo (e->e2, e->type);
        Put (" : ");
        ExTo (e->e3, e->type);
        break;

     case COMMA:
        Ex (e->e1);
        Put (", ");
        Ex (e->e2);
        break;

     case ASSIGN:
     case MULASSIGN:
     case DIVASSIGN:
     case MODASSIGN:
     case ADDASSIGN:
     case SUBASSIGN:
     case SHLASSIGN:
     case SHRASSIGN:
     case ANDASSIGN:
     case XORASSIGN:
     case ORASSIGN:
        AssignExpr (e);
        break;

     default:
     {
        /* binarni operatory */
        bool str = IsStrT (ET (e->e1)) || IsStrT (ET (e->e2));
        if (str && e->op == PLUS)
        {
           Put ("cm_cat (");
           StrOperand (e->e1);
           Put (", ");
           StrOperand (e->e2);
           Put (")");
           break;
        }
        if (str)
        {
           Put ("(cm_strcmp (");
           Ex (e->e1);
           Put (", ");
           Ex (e->e2);
           PutF (") %s 0)", OpText (e->op));
           break;
        }
        /* ukazatele na ruzne tridy (baze a odvozena):  p == q */
        TypePtr c1 = PtrClass (ET (e->e1));
        TypePtr c2 = PtrClass (ET (e->e2));
        if (c1 != NULL && c2 != NULL && c1 != c2)
        {
           Ex (e->e1);
           PutF (" %s (%s *) ", OpText (e->op), ClsName (c1));
           ExP (e->e2);
           break;
        }
        Ex (e->e1);
        PutF (" %s ", OpText (e->op));
        Ex (e->e2);
        break;
     }
  }
}

static void InitList (ExprPtr e, TypePtr t)
/* { a, b, c } - prvky prevedene na typ prvku pole t */
{
  TypePtr elem = NULL;
  t = Resolve (t);
  if (t != NULL && t->sort == Array) elem = t->from;

  Put ("{");
  ExprPtr list [4096];
  int n = 0;
  ExprPtr x = e->e1;
  /* calc (t1, t2, COMMA) vytvari levou rekurzi:  ((a, b), c) */
  while (x != NULL && x->op == COMMA && n < 4096)
  {
     list [n ++] = x->e2;
     x = x->e1;
  }
  if (x != NULL) list [n ++] = x;
  for (int i = n - 1; i >= 0; i--)
  {
     if (list [i]->op == LBRACE) InitList (list [i], elem);
     else if (elem != NULL) ExTo (list [i], elem);
     else Ex (list [i]);
     if (i > 0) Put (", ");
  }
  Put ("}");
}

/******************************* STATEMENTS *******************************/

/* lokalni objekty s destruktorem v otevrenych blocich */

enum { SC_BLOCK, SC_LOOP, SC_SWITCH };

typedef struct SCOPE_REC SCOPE;   /* predem - ukazatel na sebe (cmm nezna struct X * v polozce) */
struct SCOPE_REC
{
  int                kind;
  ObjectPtr *        objs;
  int                cnt;
  int                cap;
  SCOPE *            up;
};

static SCOPE * Scopes = NULL;

static void PushScope (int kind)
{
  SCOPE * s = (SCOPE *) calloc (1, sizeof (SCOPE));
  s->kind = kind;
  s->up = Scopes;
  Scopes = s;
}

static void PopScope (void)
{
  Scopes = Scopes->up;
}

static void AddDtorObj (ObjectPtr obj)
{
  SCOPE * s = Scopes;
  if (s->cnt >= s->cap)
  {
     s->cap = (s->cap == 0) ? 8 : 2 * s->cap;
     s->objs = (ObjectPtr *) realloc (s->objs, s->cap * sizeof (ObjectPtr));
  }
  s->objs [s->cnt ++] = obj;
}

static void DtorCall (TypePtr cls, const char * addr)
/* destruktor objektu na adrese addr (virtualni neni treba - presny typ) */
{
  PutF ("%s (%s);\n", DtorRef (cls), addr);
}

static void DestroyObj (ObjectPtr obj)
{
  TypePtr t = Resolve (obj->type);
  if (t->sort == Array)
  {
     TypePtr c = ElemClass (t);
     PutF ("for (long cm_k = %s - 1; cm_k >= 0; cm_k --) ", ExprText (t->range));
     DtorCall (c, Fmt ("&%s [cm_k]", VarName (obj)));
  }
  else
     DtorCall (t, Fmt ("&%s", VarName (obj)));
}

static void DestroyScope (SCOPE * s)
{
  for (int i = s->cnt - 1; i >= 0; i--)
     DestroyObj (s->objs [i]);
}

static bool PendingDtors (SCOPE * upto, bool through)
/* jsou objekty k zruseni v blocich nad upto (through = vcetne upto) */
{
  for (SCOPE * s = Scopes; s != NULL; s = s->up)
  {
     if (s == upto && !through) return false;
     if (s->cnt > 0) return true;
     if (s == upto) return false;
  }
  return false;
}

static void DestroyUpTo (SCOPE * upto)
{
  for (SCOPE * s = Scopes; s != NULL && s != upto; s = s->up)
     DestroyScope (s);
}

static SCOPE * FindLoop (bool for_break)
{
  for (SCOPE * s = Scopes; s != NULL; s = s->up)
     if (s->kind == SC_LOOP || (for_break && s->kind == SC_SWITCH)) return s;
  return NULL;
}

/* ---------------------------------------------------------------------- */

static void St (StatPtr s);
static void LocalDecl (ObjectPtr obj);
static char * FuncName (ObjectPtr f);

static void SubStat (StatPtr s)
/* telo if, while, for */
{
  if (s->st == LBRACE)
     St (s);
  else
  {
     Ind ++;
     St (s);
     Ind --;
  }
}

static void LoopBody (StatPtr s)
{
  PushScope (SC_LOOP);
  SubStat (s);
  PopScope ();
}

static bool IsJump (StatPtr s)
{
  return s != NULL && (s->st == RETURN_ || s->st == BREAK_ || s->st == CONTINUE_ || s->st == GOTO_);
}

static void Block (StatPtr first, StatPtr prefix_decl)
/* prikazy bloku (bez { }) - destruktory na konci */
{
  (void) prefix_decl;
  StatPtr last = NULL;
  for (StatPtr p = first; p != NULL; p = p->next)
  {
     St (p);
     /* navesti pred deklaraci (C pred C23 nedovoluje) */
     if ((p->st == CASE_ || p->st == DEFAULT_) && p->next != NULL && p->next->st == DECL)
        PutLn (";");
     last = p;
  }
  if (!IsJump (last)) DestroyScope (Scopes);
}

static void ReturnStat (StatPtr s)
{
  if (CurKind == 2)                       /* destruktor: zbytek destruktoru */
  {
     DtorEndUsed = true;
     PutLn ("goto cm_dtor_end;");
     return;
  }
  const char * value = (CurKind == 1) ? "this" : NULL;
  bool dtors = PendingDtors (NULL, true);

  if (!dtors)
  {
     if (value != NULL) PutF ("return %s;\n", value);
     else if (s->e1 == NULL) PutLn ("return;");
     else
     {
        Put ("return ");
        ExTo (s->e1, CurRet);
        PutLn (";");
     }
     return;
  }

  /* return e;  =>  { T cm_ret = e; destruktory; return cm_ret; } */
  PutLn ("{");
  Ind ++;
  if (s->e1 != NULL && value == NULL)
  {
     PutF ("%s = ", TypeDecl (CurRet, NULL, "cm_ret"));
     ExTo (s->e1, CurRet);
     PutLn (";");
     value = "cm_ret";
  }
  else if (s->e1 != NULL)
  {
     Ex (s->e1);
     PutLn (";");
  }
  DestroyUpTo (NULL);
  if (value != NULL) PutF ("return %s;\n", value);
  else PutLn ("return;");
  Ind --;
  PutLn ("}");
}

static void JumpStat (StatPtr s)
/* break, continue - destruktory objektu z bloku uvnitr cyklu */
{
  const char * text = (s->st == BREAK_) ? "break;" : "continue;";
  SCOPE * loop = FindLoop (s->st == BREAK_);
  if (loop == NULL || !PendingDtors (loop, false))
  {
     PutLn (text);
     return;
  }
  PutLn ("{");
  Ind ++;
  DestroyUpTo (loop);
  PutLn (text);
  Ind --;
  PutLn ("}");
}

static void RangeForStat (StatPtr s)
/* for (T x : seq)  =>  for (int i = 0; i < size; i++) { T x = seq [i]; ... } */
{
  ObjectPtr var = s->decl.first->item;
  TypePtr seq = ET (s->e1);
  TypePtr T = seq->elem;
  int k = ++ TmpCount;

  CAPTURE c;
  BeginCapture (&c);
  Addr (s->e1);
  char * a = EndCapture (&c);

  bool tmp = !IsLValue (s->e1);
  if (tmp)
  {
     /* posloupnost je vysledek volani - pomocna promenna */
     PutLn ("{");
     Ind ++;
     PutF ("cm_Seq * cm_r%d = %s;\n", k, a);
     a = Fmt ("cm_r%d", k);
  }

  PutF ("for (int cm_i%d = 0; cm_i%d < cm_seq_size (%s); cm_i%d ++)\n", k, k, a, k);
  PutLn ("{");
  Ind ++;
  PushScope (SC_LOOP);
  PushScope (SC_BLOCK);
  char * elem = Fmt ("CM_SEQ_AT (%s, %s, cm_i%d)", TypeName (T), a, k);
  if (IsRefT (var->type))
     PutF ("%s = &%s;\n", ObjDecl (var, var->name), elem);
  else
     PutF ("%s = %s;\n", ObjDecl (var, var->name), elem);

  if (s->s1->st == LBRACE)
  {
     PushScope (SC_BLOCK);
     Block (s->s1->s1, NULL);
     PopScope ();
  }
  else
  {
     St (s->s1);
  }
  PopScope ();
  PopScope ();
  Ind --;
  PutLn ("}");

  if (tmp)
  {
     Ind --;
     PutLn ("}");
  }
}

static void ForInit (StatPtr s)
/* for (int i = 0, j = 1; ...) */
{
  bool first = true;
  for (NodePtr p = s->decl.first; p != NULL; p = p->next)
  {
     ObjectPtr obj = p->item;
     char * spec;
     char * d = Declarator (obj->type, obj->desc, obj->name, &spec);
     if (first) PutF ("%s %s", spec, d);
     else PutF (", %s", d);
     first = false;
     if (obj->init != NULL)
     {
        Put (" = ");
        ExTo (obj->init, obj->desc != NULL ? obj->desc : obj->type);
     }
  }
  if (s->e1 != NULL) Ex (s->e1);
}

static void St (StatPtr s)
{
  switch (s->st)
  {
     case GOTO_:
        PutF ("goto %s;\n", s->id);
        break;

     case CONTINUE_:
     case BREAK_:
        JumpStat (s);
        break;

     case RETURN_:
        ReturnStat (s);
        break;

     case WHILE_:
        Put ("while (");
        Ex (s->e1);
        PutLn (")");
        LoopBody (s->s1);
        break;

     case DO_:
        PutLn ("do");
        LoopBody (s->s1);
        Put ("while (");
        Ex (s->e1);
        PutLn (");");
        break;

     case COLON:
        RangeForStat (s);
        break;

     case FOR_:
        Put ("for (");
        ForInit (s);
        Put ("; ");
        if (s->e2 != NULL) Ex (s->e2);
        Put ("; ");
        if (s->e3 != NULL) Ex (s->e3);
        PutLn (")");
        LoopBody (s->s1);
        break;

     case IF_:
        Put ("if (");
        Ex (s->e1);
        PutLn (")");
        SubStat (s->s1);
        if (s->s2 != NULL)
        {
           if (s->s2->st == IF_)
           {
              Put ("else ");
              St (s->s2);
           }
           else
           {
              PutLn ("else");
              SubStat (s->s2);
           }
        }
        break;

     case SWITCH_:
        Put ("switch (");
        Ex (s->e1);
        PutLn (")");
        PushScope (SC_SWITCH);
        SubStat (s->s1);
        PopScope ();
        break;

     case LBRACE:
        PutLn ("{");
        Ind ++;
        PushScope (SC_BLOCK);
        Block (s->s1, NULL);
        PopScope ();
        Ind --;
        PutLn ("}");
        break;

     case CASE_:
        Ind --;
        Put ("case ");
        Ex (s->e1);
        PutLn (":");
        Ind ++;
        break;

     case DEFAULT_:
        Ind --;
        PutLn ("default:");
        Ind ++;
        break;

     case LAB:
        PutF ("%s:", s->id);
        if (s->s1 != NULL)
        {
           Put ("\n");
           St (s->s1);
        }
        else
           PutLn (" ;");
        break;

     case SIM:
        if (s->e1 != NULL) Ex (s->e1);
        PutLn (";");
        break;

     case DECL:
        for (NodePtr p = s->decl.first; p != NULL; p = p->next)
           LocalDecl (p->item);
        break;

     default:
        TrError ("Unsupported statement");
  }
}

/****************************** DECLARATIONS ******************************/

static void ConstructObj (TypePtr cls, const char * addr)
/* volani konstruktoru (bez argumentu) */
{
  PutF ("%s (%s", CtorRef (cls), addr);
  if (cls->ctor != NULL) ArgList (cls->ctor->type, NULL, true);
  PutLn (");");
}

static void ConstructVar (ObjectPtr obj, const char * name)
/* konstruktory objektu (i pole objektu) */
{
  TypePtr t = Resolve (obj->type);
  if (t->sort == Array)
  {
     TypePtr c = ElemClass (t);
     if (c == NULL || !NeedsCtor (c, false)) return;
     PutF ("for (long cm_k = 0; cm_k < %s; cm_k ++) ", ExprText (t->range));
     ConstructObj (c, Fmt ("&%s [cm_k]", name));
     return;
  }
  if (IsClsSort (t->sort) && NeedsCtor (t, false))
     ConstructObj (t, Fmt ("&%s", name));
}

static bool HasDtor (ObjectPtr obj)
{
  TypePtr c = ElemClass (obj->type);
  return c != NULL && NeedsDtor (c, false);
}

static void LocalDecl (ObjectPtr obj)
{
  if (obj->fce) return;                       /* prototyp - prototypy jsou na zacatku */

  if (obj->storage == Typedef)
  {
     EnsureTypes (obj->type, obj->desc);
     PutF ("typedef %s;\n", ObjDecl (obj, obj->name));
     return;
  }
  if (obj->name [0] == 0)
  {
     TypePtr t = Resolve (obj->type);
     if (t != NULL && t->sort == Enum && t->tag == NULL)
        PutF ("%s;\n", EnumBody (t));          /* enum { A, B }; */
     else
        EnsureTypes (obj->type, obj->desc);    /* struct X { ... }; */
     return;
  }

  const char * storage = (obj->storage == Static) ? "static " :
                         (obj->storage == Extern) ? "extern " : "";
  TypePtr t = Resolve (obj->type);

  TypePtr sc = ElemClass (t);
  if (obj->storage == Static && sc != NULL && (NeedsCtor (sc, false) || NeedsDtor (sc, false)))
  {
     /* staticky lokalni objekt: globalni promenna, konstruktor pri prvnim
        pruchodu, destruktor na konci programu (atexit) */
     static int cnt = 0;
     int k = ++ cnt;
     char * name = Fmt ("cm_%s_%s", CurFce != NULL ? FuncName (CurFce) : "", obj->name);
     PSet (&Renamed, obj, name);
     BUF * old = Section (&SecVars);
     PutF ("static %s = {0};\n", ObjDecl (obj, name));
     Section (old);
     if (NeedsDtor (sc, false))
     {
        old = Section (&SecGen);
        PutF ("static void cm_fini%d (void)\n{\n", k);
        Ind ++;
        DestroyObj (obj);
        Ind --;
        PutLn ("}\n");
        Section (old);
        Proto (Fmt ("static void cm_fini%d (void)", k));
     }
     PutF ("static bool cm_once%d = false;\n", k);
     PutF ("if (!cm_once%d)\n{\n", k);
     Ind ++;
     PutF ("cm_once%d = true;\n", k);
     ConstructVar (obj, name);
     if (NeedsDtor (sc, false)) PutF ("atexit (cm_fini%d);\n", k);
     Ind --;
     PutLn ("}");
     return;
  }
  TypePtr target = obj->desc != NULL ? obj->desc : obj->type;

  if (t->sort == Reference)
  {
     PutF ("%s%s = ", storage, ObjDecl (obj, obj->name));
     if (obj->init != NULL) ExTo (obj->init, target);
     else Put ("NULL");
     PutLn (";");
     return;
  }

  PutF ("%s%s", storage, ObjDecl (obj, obj->name));
  if (obj->init != NULL)
  {
     Put (" = ");
     if (obj->init->op == LBRACE) InitList (obj->init, t);
     else ExTo (obj->init, target);
  }
  else if (IsClsSort (t->sort) || (t->sort == Array && ElemClass (t) != NULL) ||
           t->sort == StdSequence || t->sort == StdDictionary)
     Put (" = {0}");
  else if (t->sort == StdString)
     Put (" = NULL");
  PutLn (";");

  if (obj->init == NULL && obj->storage != Static && obj->storage != Extern)
     ConstructVar (obj, obj->name);
  if (obj->storage != Static && obj->storage != Extern && HasDtor (obj))
     AddDtorObj (obj);
}

/******************************* FUNCTIONS ********************************/

static char * StorageOf (ObjectPtr f)
{
  if (IsInClass (f) && f->body != NULL) return Fmt ("static inline ");
  if (f->owner == NULL && f->storage == Static) return Fmt (f->Inline ? "static inline " : "static ");
  if (f->owner == NULL && f->Inline) return Fmt ("static inline ");
  return Fmt ("%s", "");
}

static char * FuncName (ObjectPtr f)
{
  if (f->owner != NULL) return MethodName (f);
  return Fmt ("%s", f->name);
}

static char * FullHead (ObjectPtr f, bool defn)
{
  TypePtr this_cls = (f->owner != NULL && (f->method || IsCtorDecl (f) || IsDtorDecl (f))) ? f->owner : NULL;
  return Fmt ("%s%s", StorageOf (f), FuncHead (f, FuncName (f), this_cls, defn));
}

static Dict ProtoDone;

static void FuncProto (ObjectPtr f)
{
  if (PGet (&ProtoDone, f) != NULL) return;
  PSet (&ProtoDone, f, f);
  if (f->pure && f->body == NULL) return;
  for (NodePtr p = f->type->list.first; p != NULL; p = p->next)
     if (p->item->type != NULL) EnsureTypes (p->item->type, p->item->desc);
  Proto (FullHead (f, false));
}

static void CtorPrologue (TypePtr cls, ObjectPtr ctor)
/* zacatek konstruktoru: baze, tabulka virtualnich metod, vnorene objekty */
{
  CLS * c = GetCls (cls);
  if (cls->base != NULL && NeedsCtor (cls->base, false))
  {
     PutF ("%s ((%s *) this", CtorRef (cls->base), ClsName (cls->base));
     if (cls->base->ctor != NULL)
        ArgList (cls->base->ctor->type, (ctor != NULL && ctor->base_init) ? ctor->base_args : NULL, true);
     PutLn (");");
  }
  if (c->poly)
     PutF ("this->vptr = (const %s_vtbl *) &%s;\n", ClsName (VptrRoot (cls)), VtableRef (cls));
  for (NodePtr p = cls->list.first; p != NULL; p = p->next)
  {
     ObjectPtr f = p->item;
     if (!IsField (f)) continue;
     TypePtr ec = ElemClass (f->type);
     if (ec == NULL || !NeedsCtor (ec, false)) continue;
     ConstructVar (f, Fmt ("this->%s", FName (f)));
  }
}

static void DtorEpilogue (TypePtr cls)
/* konec destruktoru: vnorene objekty (pozpatku), baze */
{
  int n = 0;
  ObjectPtr fields [1024];
  for (NodePtr p = cls->list.first; p != NULL; p = p->next)
     if (IsField (p->item) && HasDtor (p->item) && n < 1024) fields [n ++] = p->item;
  for (int i = n - 1; i >= 0; i--)
  {
     ObjectPtr f = fields [i];
     TypePtr t = Resolve (f->type);
     if (t->sort == Array)
     {
        PutF ("for (long cm_k = %s - 1; cm_k >= 0; cm_k --) ", ExprText (t->range));
        DtorCall (ElemClass (t), Fmt ("&this->%s [cm_k]", FName (f)));
     }
     else
        DtorCall (t, Fmt ("&this->%s", FName (f)));
  }
  if (cls->base != NULL && NeedsDtor (cls->base, false))
     DtorCall (cls->base, Fmt ("(%s *) this", ClsName (cls->base)));
}

static void FuncBody (ObjectPtr f)
/* definice funkce, metody, konstruktoru nebo destruktoru */
{
  FuncProto (f);

  CurFce = f;
  CurClass = (f->owner != NULL && (f->method || IsCtorDecl (f) || IsDtorDecl (f))) ? f->owner : NULL;
  CurRet = f->type->from;
  CurKind = IsCtorDecl (f) ? 1 : IsDtorDecl (f) ? 2 : 0;
  DtorEndUsed = false;
  TmpCount = 0;

  BUF * old = Section (&SecFuncs);
  PutLn (FullHead (f, true));
  PutLn ("{");
  Ind ++;

  if (CurKind == 1) CtorPrologue (f->owner, f);
  if (CurKind == 2 && GetCls (f->owner)->poly)
     PutF ("this->vptr = (const %s_vtbl *) &%s;\n", ClsName (VptrRoot (f->owner)), VtableRef (f->owner));

  /* telo:  { ... } */
  StatPtr body = f->body;
  PushScope (SC_BLOCK);
  /* parametry - objekty s destruktorem hodnotou: kopii zrusi volany (na konci tela, i return) */
  for (NodePtr p = f->type->list.first; p != NULL; p = p->next)
  {
     ObjectPtr par = p->item;
     if (par->type == NULL || par->name == NULL || par->name [0] == 0) continue;
     TypePtr pt = Resolve (par->type);
     if (pt != NULL && IsClsSort (pt->sort) && HasDtor (par)) AddDtorObj (par);
  }
  if (body->st == LBRACE)
     Block (body->s1, NULL);
  else
     St (body);
  PopScope ();

  if (CurKind == 1) PutLn ("return this;");
  if (CurKind == 2)
  {
     CAPTURE c;
     BeginCapture (&c);
     DtorEpilogue (f->owner);
     char * epi = EndCapture (&c);
     if (DtorEndUsed)
     {
        Ind --;
        PutLn ("cm_dtor_end:");
        Ind ++;
        if (epi [0] == 0) PutLn (";");
     }
     Put (epi);
  }

  Ind --;
  PutLn ("}");
  Put ("\n");
  Section (old);

  CurFce = NULL;
  CurClass = NULL;
  CurKind = 0;
}

/* ---------------------------------------------------------------------- */

static void EmitStaticMembers (TypePtr t)
/* staticke polozky:  celociselne konstanty => enum, ostatni => promenne */
{
  for (NodePtr p = t->list.first; p != NULL; p = p->next)
  {
     ObjectPtr m = p->item;
     if (m->fce || m->storage != Static || m->name [0] == 0) continue;
     char * name = Fmt ("%s_%s", ClsName (t), m->name);
     if (IsIntConstObj (m))
     {
        BUF * old = Section (&SecTypes);
        PutF ("enum { %s = %s };\n\n", name, ExprText (m->init));
        Section (old);
     }
     else
     {
        /* definice mimo tridu (T::x) => definice, jinak extern (definice
           v jinem souboru);  inicializace ve tride => static v kazdem souboru */
        EnsureTypes (m->type, m->desc);
        BUF * old = Section (&SecVars);
        const char * storage = m->outdef ? "" : (m->init != NULL) ? "CM_UNUSED static " : "extern ";
        PutF ("%s%s", storage, ObjDecl (m, name));
        if (m->init != NULL)
        {
           Put (" = ");
           ExTo (m->init, m->desc != NULL ? m->desc : m->type);
        }
        PutLn (";");
        Section (old);
     }
  }
}

static void EmitMethods (TypePtr t)
/* prototypy metod, definice metod uvnitr tridy */
{
  for (NodePtr p = t->list.first; p != NULL; p = p->next)
  {
     ObjectPtr m = p->item;
     if (!m->fce) continue;
     FuncProto (m);
  }
  if (t->ctor != NULL) FuncProto (t->ctor);
}

static void EmitInClassBodies (TypePtr t)
{
  for (NodePtr p = t->list.first; p != NULL; p = p->next)
  {
     ObjectPtr m = p->item;
     if (m->fce && m->body != NULL && !m->outdef) FuncBody (m);
  }
  if (t->ctor != NULL && t->ctor->body != NULL && !t->ctor->outdef) FuncBody (t->ctor);
}

/* ---------------------------------------------------------------------- */

/* globalni objekty s konstruktorem / destruktorem */
static ObjectPtr GlobalObjs [4096];
static int       GlobalObjCnt = 0;
static BUF       GlobalInit = { NULL, 0, 0, 1 };   /* prikazy pred main */

static bool IsIntConstObj (ObjectPtr obj);

static bool IsConstExpr (ExprPtr e)
/* konstantni vyraz (inicializace globalni promenne v C) */
{
  if (e == NULL) return true;
  switch (e->op)
  {
     case NUM: case FLT: case CHR: case STR: case TRUE_: case FALSE_: case NULLPTR_:
     case SIZEOF_:
        return true;
     case IDENT:
        return e->obj == NULL || IsEnumConst (e->obj) || e->obj->fce || IsIntConstObj (e->obj);
     case ADR:
        return true;
     case LPAR: case UNARYPLUS: case UNARYMINUS: case UNARYNOT: case LOGNOT:
        return IsConstExpr (e->e1) && IsConstExpr (e->e2);
     case STATIC_CAST_:
        return !IsStrT (e->type) && IsConstExpr (e->e1);
     case LBRACE: case COMMA:
     case PLUS: case MINUS: case ASTERISK: case SLASH: case MOD: case SHL: case SHR:
     case AND: case OR: case XOR: case LOGAND: case LOGOR:
     case LESS: case GREATER: case LESSEQUAL: case GREATEREQUAL: case EQUAL: case UNEQUAL:
        if (IsStrT (ET (e->e1)) || IsStrT (ET (e->e2))) return false;
        return IsConstExpr (e->e1) && IsConstExpr (e->e2);
     case QUESTION:
        return IsConstExpr (e->e1) && IsConstExpr (e->e2) && IsConstExpr (e->e3);
     default:
        return false;
  }
}

static bool IsIntConstObj (ObjectPtr obj)
/* const int X = 10;  => enum (v C lze pouzit jako velikost pole) */
{
  if (!IsConstInit (obj)) return false;
  TypePtr t = Resolve (obj->type);
  if (!(t->sort == Int || t->sort == Char || t->sort == Enum || t->sort == NoSort)) return false;
  if (t->long_long) return false;    /* const long long - hodnota mimo int (enum by zmenil typ) */
  return IsConstExpr (obj->init) && obj->init->op != STR;
}

static void GlobalVar (ObjectPtr obj)
{
  EnsureTypes (obj->type, obj->desc);
  TypePtr t = Resolve (obj->type);
  TypePtr target = obj->desc != NULL ? obj->desc : obj->type;

  if (IsIntConstObj (obj) && obj->storage != Extern)
  {
     BUF * old = Section (&SecTypes);
     PutF ("enum { %s = %s };\n\n", obj->name, ExprText (obj->init));
     Section (old);
     return;
  }

  BUF * old = Section (&SecVars);
  const char * storage = "";
  if (obj->storage == Static) storage = "static ";
  else if (obj->storage == Extern) storage = "extern ";
  else if (t->Const) storage = "CM_UNUSED static ";     /* const v C++: jen v tomto souboru */

  PutF ("%s%s", storage, ObjDecl (obj, obj->name));
  if (obj->init != NULL && obj->storage != Extern)
  {
     if (IsConstExpr (obj->init))
     {
        Put (" = ");
        if (obj->init->op == LBRACE) InitList (obj->init, t);
        else ExTo (obj->init, target);
     }
     else
     {
        /* inicializace az pred main */
        BUF * o = Section (&GlobalInit);
        PutF ("%s = ", obj->name);
        ExTo (obj->init, target);
        PutLn (";");
        Section (o);
     }
  }
  PutLn (";");
  Section (old);

  if (obj->storage != Extern && obj->init == NULL)
  {
     TypePtr c = ElemClass (t);
     if (c != NULL && (NeedsCtor (c, false) || NeedsDtor (c, false)) && GlobalObjCnt < 4096)
        GlobalObjs [GlobalObjCnt ++] = obj;
  }
}

static void GlobalItem (ObjectPtr obj)
{
  if (obj->pkind == PK_PROTO) return;               /* prototyp - prototypy jsou na zacatku */
  if (obj->pkind == PK_OUTDEF)
  {
     ObjectPtr m = obj->pref;
     EmitTypeDef (m->owner);
     FuncBody (m);
     return;
  }

  if (obj->storage == Typedef)
  {
     EnsureTypes (obj->type, obj->desc);
     TypePtr t = Resolve (obj->type);
     /* typedef struct { ... } X;  - struktura uz ma jmeno X */
     if (t != NULL && (IsClsSort (t->sort) || t->sort == Enum) && StrEq (t->tag, obj->name))
        return;
     BUF * old = Section (&SecTypes);
     PutF ("typedef %s;\n\n", ObjDecl (obj, obj->name));
     Section (old);
     return;
  }

  if (obj->name [0] == 0)                           /* struct X { ... }; */
  {
     TypePtr t = Resolve (obj->type);
     if (t != NULL && t->sort == Enum && t->tag == NULL && PGet (&TypeDone, t) == NULL)
     {
        PSet (&TypeDone, t, t);                       /* enum { A, B };  (bez jmena) */
        BUF * old = Section (&SecTypes);
        PutF ("%s;\n\n", EnumBody (t));
        Section (old);
        return;
     }
     EnsureTypes (obj->type, obj->desc);
     return;
  }

  if (obj->fce)
  {
     FuncProto (obj);
     if (obj->body != NULL) FuncBody (obj);
     return;
  }

  GlobalVar (obj);
}

/****************************** GENERATED *********************************/

static void GenClass (TypePtr t, bool * more)
{
  CLS * c = GetCls (t);
  const char * n = ClsName (t);
  BUF * old = Section (&SecGen);

  if (c->want_ctor && !c->done_ctor)
  {
     c->done_ctor = true;
     *more = true;
     PutF ("static %s * %s_ctor (%s * this)\n{\n", n, n, n);
     Ind ++;
     CurClass = t;
     CtorPrologue (t, NULL);
     CurClass = NULL;
     PutLn ("return this;");
     Ind --;
     PutLn ("}\n");
  }

  if (c->want_dtor && !c->done_dtor)
  {
     c->done_dtor = true;
     *more = true;
     PutF ("static void %s_dtor (%s * this)\n{\n", n, n);
     Ind ++;
     if (c->poly)
        PutF ("this->vptr = (const %s_vtbl *) &%s;\n", ClsName (VptrRoot (t)), VtableRef (t));
     DtorEpilogue (t);
     Ind --;
     PutLn ("}\n");
  }

  if (c->want_new && !c->done_new)
  {
     c->done_new = true;
     *more = true;
     PutF ("static %s * %s_new (%s)\n{\n", n, n, CtorParams (t, true));
     Ind ++;
     PutF ("return %s (CM_NEW (%s)", CtorRef (t), n);
     if (t->ctor != NULL)
     {
        int i = 0;
        for (NodePtr p = t->ctor->type->list.first; p != NULL; p = p->next)
        {
           i ++;
           ObjectPtr par = p->item;
           if (par->type == NULL || par->type->sort == Void) continue;
           PutF (", %s", par->name [0] != 0 ? par->name : Fmt ("cm_unused%d", i));
        }
     }
     PutLn (");");
     Ind --;
     PutLn ("}\n");
  }

  if (c->want_newarr && !c->done_newarr)
  {
     c->done_newarr = true;
     *more = true;
     PutF ("static %s * %s_new_array (long n)\n{\n", n, n);
     Ind ++;
     PutF ("%s * p = CM_NEW_ARRAY (%s, n);\n", n, n);
     PutF ("for (long i = 0; i < n; i++) ");
     ConstructObj (t, "&p [i]");
     PutLn ("return p;");
     Ind --;
     PutLn ("}\n");
  }

  if (c->want_delete && !c->done_delete)
  {
     c->done_delete = true;
     *more = true;
     PutF ("static void %s_delete (%s * p)\n{\n", n, n);
     Ind ++;
     PutLn ("if (p == NULL) return;");
     int slot = c->poly ? VtIndex (t, "~") : -1;
     if (slot >= 0)
     {
        SLOT * sl = &c->slots [slot];
        if (sl->intro != t) PutF ("%s ((%s *) p);\n", DispatchRef (t, sl->decl), ClsName (sl->intro));
        else PutF ("%s (p);\n", DispatchRef (t, sl->decl));
     }
     else
        PutF ("%s (p);\n", DtorRef (t));
     PutLn ("cm_free (p);");
     Ind --;
     PutLn ("}\n");
  }

  if (c->want_copy && !c->done_copy)
  {
     c->done_copy = true;
     *more = true;
     PutF ("static %s %s_copy (%s * src)\n{\n", n, n, n);
     Ind ++;
     PutF ("%s r = *src;\n", n);
     for (TypePtr b = t; b != NULL; b = b->base)
        for (NodePtr p = b->list.first; p != NULL; p = p->next)
        {
           ObjectPtr f = p->item;
           if (!IsField (f)) continue;
           TypePtr ft = Resolve (f->type);
           if (ft->sort == StdSequence) PutF ("r.%s = cm_seq_copy (&src->%s);\n", FName (f), FName (f));
           else if (ft->sort == StdDictionary) PutF ("r.%s = cm_dict_copy (&src->%s);\n", FName (f), FName (f));
           else if (IsClsSort (ft->sort) && HasContainers (ft))
              PutF ("r.%s = %s (&src->%s);\n", FName (f), CopyRef (ft), FName (f));
        }
     PutLn ("return r;");
     Ind --;
     PutLn ("}\n");
  }

  for (int i = 0; i < c->nslots; i++)
     if (c->want_disp [i] && !c->done_disp [i])
     {
        c->done_disp [i] = true;
        *more = true;
        SLOT * s = &c->slots [i];
        const char * vt = Fmt ("((const %s_vtbl *) this->vptr)", n);
        if (StrEq (s->name, "~"))
        {
           PutF ("static void %s_dtor_virtual (%s * this)\n{\n", n, n);
           PutF ("    %s->cm_dtor (this);\n}\n\n", vt);
           continue;
        }
        ObjectPtr d = s->decl;
        PutF ("static %s\n{\n", FuncHead (d, Fmt ("%s_%s_virtual", n, s->name), t, true));
        bool is_void = Resolve (d->type->from)->sort == Void;
        PutF ("    %s%s->%s (this", is_void ? "" : "return ", vt, s->name);
        int k = 0;
        for (NodePtr p = d->type->list.first; p != NULL; p = p->next)
        {
           k ++;
           ObjectPtr par = p->item;
           if (par->type == NULL || par->type->sort == Void) continue;
           PutF (", %s", par->name [0] != 0 ? par->name : Fmt ("cm_unused%d", k));
        }
        PutLn (");\n}\n");
     }

  Section (old);

  if (c->want_info && !c->done_info)
  {
     c->done_info = true;
     *more = true;
     old = Section (&SecTypes);
     if (t->base != NULL)
        PutF ("static const cm_ClassInfo %s_cm_info = { \"%s\", &%s_cm_info };\n\n", n, n, ClsName (t->base));
     else
        PutF ("static const cm_ClassInfo %s_cm_info = { \"%s\", NULL };\n\n", n, n);
     Section (old);
  }

  if (c->want_vtable && !c->done_vtable)
  {
     c->done_vtable = true;
     *more = true;
     InfoRef (t);
     old = Section (&SecVars);
     PutF ("static const %s_vtbl %s_vtable =\n{\n", n, n);
     PutF ("    &%s_cm_info", n);
     for (int i = 0; i < c->nslots; i++)
     {
        SLOT * s = &c->slots [i];
        char * slot_type;
        if (StrEq (s->name, "~"))
           slot_type = Fmt ("void (*) (%s *)", ClsName (s->intro));
        else
           slot_type = TypeDecl (s->decl->type->from, NULL,
                                 Fmt ("(*) (%s)", ParamsText (s->decl->type, s->intro, false)));
        char * impl;
        if (s->impl == NULL) impl = DtorRef (s->impl_cls);
        else if (s->impl->pure) impl = Fmt ("cm_pure_virtual");
        else if (StrEq (s->name, "~")) impl = DtorRef (s->impl_cls);
        else impl = MethodName (s->impl);
        if (s->impl_cls == s->intro && (s->impl == NULL || !s->impl->pure))
           PutF (",\n    %s", impl);
        else
           PutF (",\n    (%s) %s", slot_type, impl);
     }
     PutLn ("\n};\n");
     Section (old);
  }
}

static void GenGlobalInit (void)
{
  if (GlobalObjCnt == 0 && GlobalInit.len == 0) return;

  BUF * old = Section (&SecGen);
  bool dtors = false;
  for (int i = 0; i < GlobalObjCnt; i++)
     if (HasDtor (GlobalObjs [i])) dtors = true;

  if (dtors)
  {
     PutLn ("static void cm_global_fini (void)");
     PutLn ("{");
     Ind ++;
     for (int i = GlobalObjCnt - 1; i >= 0; i--)
        if (HasDtor (GlobalObjs [i])) DestroyObj (GlobalObjs [i]);
     Ind --;
     PutLn ("}\n");
  }

  PutLn ("CM_CONSTRUCTOR static void cm_global_init (void)");
  PutLn ("{");
  Put (BufText (&GlobalInit));
  Ind ++;
  for (int i = 0; i < GlobalObjCnt; i++)
  {
     ObjectPtr obj = GlobalObjs [i];
     TypePtr c = ElemClass (obj->type);
     if (NeedsCtor (c, false)) ConstructVar (obj, obj->name);
  }
  if (dtors) PutLn ("atexit (cm_global_fini);");
  Ind --;
  PutLn ("}\n");
  Section (old);
}

static void Generate (void)
/* pozadovane funkce a tabulky (mohou vyvolat dalsi pozadavky) */
{
  bool more = true;
  while (more)
  {
     more = false;
     for (NodePtr p = classlist.first; p != NULL; p = p->next)
        GenClass (Resolve (p->item->type), &more);
  }
}

/******************************* TRANSLATE ********************************/

static void Translate (FILE * f, const char * src_name)
{
  RenameHiddenFields ();

  /* systemove hlavicky => #include */
  for (NodePtr p = printlist.first; p != NULL; p = p->next)
  {
     const char * h = SystemHeader ((const char *) PGet (&Origin, p->item));
     if (h != NULL)
     {
        AddHeader (h);
        MarkSysTypes (p->item->type, p->item->desc);
     }
  }

  /* typy a funkce ze zdrojoveho textu (poradi jako v textu) */
  for (NodePtr p = printlist.first; p != NULL; p = p->next)
  {
     ObjectPtr obj = p->item;
     if (IsSystemObj (obj))
     {
        /* jen funkce s telem a staticke promenne (napr. cmini/ownheaders/cstdio) */
        if (strncmp (obj->name, "cparser_std", 11) == 0) continue;
        if (!(obj->fce && obj->body != NULL) && !(obj->storage == Static && !obj->fce)) continue;
     }
     if (obj->pkind == 0 && obj->type != NULL)
        EnsureTypes (obj->type, obj->desc);
     GlobalItem (obj);
  }

  /* tridy: definice (i nepouzite) a metody definovane uvnitr tridy */
  for (NodePtr p = classlist.first; p != NULL; p = p->next)
  {
     TypePtr t = Resolve (p->item->type);
     if (IsSysType (t)) continue;
     EmitTypeDef (t);
  }
  for (NodePtr p = classlist.first; p != NULL; p = p->next)
  {
     TypePtr t = Resolve (p->item->type);
     if (IsSysType (t)) continue;
     EmitInClassBodies (t);
  }

  Generate ();
  GenGlobalInit ();
  Generate ();

  /* vystup */
  fprintf (f, "/* %s - prelozeno z %s (translate.c) */\n\n", "C", src_name);
  /* program pouziva POSIX (unistd.h: fork, mkdtemp, fileno, setenv, ...) -
     i pri prekladu s -std=c99 (tcc, gcc -std=c99) */
  bool posix = false;
  for (int i = 0; i < HeaderCnt; i++)
     if (StrEq (Headers [i], "unistd.h")) posix = true;
  if (posix)
     fprintf (f, "#ifndef _POSIX_C_SOURCE\n#define _POSIX_C_SOURCE 200809L\n#endif\n\n");
  static const char * std_headers [] =
  {
     "stdio.h", "stdlib.h", "string.h", "stddef.h", "stdint.h", "stdbool.h", "ctype.h", "assert.h", NULL
  };
  for (int i = 0; std_headers [i] != NULL; i++)
     fprintf (f, "#include <%s>\n", std_headers [i]);
  for (int i = 0; i < HeaderCnt; i++)
  {
     bool dup = false;
     for (int j = 0; std_headers [j] != NULL; j++)
        if (StrEq (Headers [i], std_headers [j])) dup = true;
     if (!dup) fprintf (f, "#include <%s>\n", Headers [i]);
  }
  fprintf (f, "#include \"cminirt.h\"\n\n");

  const BUF * secs [] = { &SecTypedef, &SecTypes, &SecProto, &SecVars, &SecFuncs, &SecGen };
  for (int i = 0; i < 6; i++)
     if (secs [i]->len > 0)
     {
        fputs (secs [i]->text, f);
        if (i == 0 || i == 2 || i == 3) fputs ("\n", f);
     }
}

/*************************** INPUT AND OUTPUT *****************************/

/* vstup a chybova hlaseni pro cparser (inp.h, out.h) - samostatny program;
   (soucast cmini.bin; vstup a vystup pro -C i pro generator pasm) */


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

/* volano z cmini.bin (instr_drv.c): -I, -iquote, -D a -C (preklad do C) */

void TrIncludeDir (char * dir)
{
  LexIncludeDir (dir);
  if (IncCnt < 32) IncDirs [IncCnt ++] = dir;
}

void TrDefine (char * arg)
{
  char * def = Fmt ("%s", arg);
  char * eq = strchr (def, '=');
  if (eq != NULL) *eq = 0;
  LexDefine (def, eq != NULL ? eq + 1 : "1");
}

/* nacte zdroj src a pripravi lexer a deklarace; 0 = ok */
int TrStart (const char * src)
{
  long len = 0;
  InpText = ReadFile (src, &len, true);   /* bez nulovych bajtu */
  InpLen = (int) len;
  if (InpText == NULL)
  {
     fprintf (stderr, "cannot open %s\n", src);
     return 2;
  }
  InpName = src;
  LexFileName (src);
  LexDefine ("__CPARSER__", "1");
  Out = &SecFuncs;
  OpenInp ();
  OpenOut ();
  InitLex ();
  InitDecl ();
  return 0;
}

/* -C: cely zdroj src => C na out_name (NULL = stdout); 0 = ok */
int TrTranslate (const char * src, const char * out_name)
{
  if (TrStart (src) != 0) return 2;

  while (sy != EOS)
  {
     char * file = InternFile (LexCurrentFile ());
     NodePtr last = printlist.last;
     GlobalDecl ();
     for (NodePtr p = (last == NULL) ? printlist.first : last->next; p != NULL; p = p->next)
        PSet (&Origin, p->item, file);
  }

  FILE * f = stdout;
  if (out_name != NULL)
  {
     f = fopen (out_name, "w");
     if (f == NULL)
     {
        fprintf (stderr, "cannot write %s\n", out_name);
        return 2;
     }
  }
  Translate (f, src);
  if (f != stdout) fclose (f);
  return 0;
}

#ifdef __cplusplus
}
#endif
