/* SEM.C - pomocne funkce nad stromem deklaraci (decl.h), spolecne pro oba generatory
   c-mini: translate.c (preklad do C) a instr.c (x86-64); ciste C jako front-end
   (prelozi ho gcc, tcc, chibicc i c-mini sam). Vypoctene vlastnosti trid (potreba
   konstruktoru, destruktoru, tabulka virtualnich metod) jsou ulozene v struct Type. */

#include "decl.h"

#include <stdlib.h>
#include <string.h>

ExprPtr SkipParens (ExprPtr e)
{
  while (e != NULL && e->op == LPAR) e = e->e1;
  return e;
}

bool IsSequence (TypePtr t)
{
  return t != NULL && t->sort == StdSequence;
}

bool IsDictionary (TypePtr t)
{
  return t != NULL && t->sort == StdDictionary;
}

bool IsStringKey (TypePtr dict)
{
  dict = Resolve (dict);
  return dict != NULL && IsStdString (Resolve (dict->key));
}

TypePtr ArrayElem (TypePtr t)
{
  while (t != NULL && t->sort == Array) t = t->from;
  return t;
}

TypePtr ElemClass (TypePtr t)
{
  t = Resolve (t);
  while (t != NULL && t->sort == Array) t = Resolve (t->from);
  return (t != NULL && IsClsSort (t->sort)) ? t : NULL;
}

bool IsEnumConst (ObjectPtr obj)
{
  if (obj == NULL) return false;
  TypePtr t = obj->type;
  if (t == NULL || t->sort != Enum) return false;
  for (NodePtr p = t->list.first; p != NULL; p = p->next)
     if (p->item == obj) return true;
  return false;
}

bool IsField (ObjectPtr obj)
{
  return obj != NULL && !obj->fce && obj->storage != Static && obj->storage != Typedef &&
         obj->name [0] != 0 && !IsEnumConst (obj);
}

bool IsStaticMember (ObjectPtr obj)
{
  return obj != NULL && !obj->fce && obj->storage == Static && obj->owner != NULL;
}

bool IsCtorDecl (ObjectPtr m)
{
  return m != NULL && m->owner != NULL && m->owner->tag != NULL && strcmp (m->name, m->owner->tag) == 0;
}

bool IsDtorDecl (ObjectPtr m)
{
  return m != NULL && m->owner != NULL && strcmp (m->name, "~") == 0;
}

TypePtr CalleeType (ExprPtr f)
{
  TypePtr t = Resolve (f->type);
  if (t != NULL && t->sort == Reference) t = Resolve (t->from);
  if (t != NULL && t->sort == Pointer) t = Resolve (t->from);
  return t;
}

bool CallReturnsRef (ExprPtr e)
{
  if (e == NULL || e->op != FCE) return false;
  TypePtr ft = CalleeType (e->e1);
  if (ft == NULL || ft->sort != Function) return false;
  TypePtr r = Resolve (ft->from);
  return r != NULL && r->sort == Reference;
}

/****************************** KONSTRUKTOR, DESTRUKTOR *******************************/

/* polozka (i pole) vyzaduje konstruktor / destruktor: trida, ktera ho potrebuje;
   zero_fields - i retezec a kontejner (nuluji se, generator instr.cc je uvolnuje) */
static bool FieldNeeds (ObjectPtr f, bool ctor, bool zero_fields)
{
  TypePtr c = ElemClass (f->type);
  if (c != NULL && (ctor ? NeedsCtor (c, zero_fields) : NeedsDtor (c, zero_fields))) return true;
  if (zero_fields)
  {
     TypePtr e = Resolve (ArrayElem (f->type));
     if (IsStdString (e) || IsContainer (e)) return true;
  }
  return false;
}

bool NeedsCtor (TypePtr cls, bool zero_fields)
{
  cls = Resolve (cls);
  if (cls == NULL || !IsClsSort (cls->sort)) return false;
  int flag = zero_fields ? cls->need_ctor_z : cls->need_ctor;
  if (flag != 0) return flag == 2;
  if (zero_fields) cls->need_ctor_z = 1; else cls->need_ctor = 1;   /* rekurze: zatim ne */
  bool r = cls->ctor != NULL || IsPolymorphic (cls) || NeedsCtor (cls->base, zero_fields);
  for (NodePtr p = cls->list.first; p != NULL && !r; p = p->next)
     if (IsField (p->item) && FieldNeeds (p->item, true, zero_fields)) r = true;
  if (zero_fields) cls->need_ctor_z = r ? 2 : 1; else cls->need_ctor = r ? 2 : 1;
  return r;
}

bool NeedsDtor (TypePtr cls, bool zero_fields)
{
  cls = Resolve (cls);
  if (cls == NULL || !IsClsSort (cls->sort)) return false;
  int flag = zero_fields ? cls->need_dtor_z : cls->need_dtor;
  if (flag != 0) return flag == 2;
  if (zero_fields) cls->need_dtor_z = 1; else cls->need_dtor = 1;
  bool r = cls->dtor != NULL || NeedsDtor (cls->base, zero_fields);
  for (NodePtr p = cls->list.first; p != NULL && !r; p = p->next)
     if (IsField (p->item) && FieldNeeds (p->item, false, zero_fields)) r = true;
  if (zero_fields) cls->need_dtor_z = r ? 2 : 1; else cls->need_dtor = r ? 2 : 1;
  return r;
}

/****************************** TABULKA VIRTUALNICH METOD *******************************/

/* sloty baze (je-li polymorfni), virtualni metoda se stejnym jmenem slot prekryje,
   nova se prida na konec */
static void VtCompute (TypePtr cls)
{
  cls->vt_done = true;
  cls->vt_count = 0;
  cls->vt = NULL;
  if (!IsPolymorphic (cls)) return;
  int cap = 16;
  if (BaseIsPolymorphic (cls))
  {
     TypePtr b = Resolve (cls->base);
     int n = VtCount (b);
     cap = n + 16;
     cls->vt = (VtSlot *) calloc (cap, sizeof (VtSlot));
     memcpy (cls->vt, b->vt, n * sizeof (VtSlot));
     cls->vt_count = n;
  }
  else
     cls->vt = (VtSlot *) calloc (cap, sizeof (VtSlot));

  for (NodePtr p = cls->list.first; p != NULL; p = p->next)
  {
     ObjectPtr m = p->item;
     if (!m->fce || !m->Virtual) continue;
     int i = 0;
     while (i < cls->vt_count && strcmp (cls->vt [i].name, m->name) != 0) i ++;
     if (i == cls->vt_count)
     {
        if (cls->vt_count >= cap)
        {
           cap *= 2;
           cls->vt = (VtSlot *) realloc (cls->vt, cap * sizeof (VtSlot));
        }
        cls->vt [i].name = m->name;
        cls->vt [i].decl = m;
        cls->vt [i].intro = cls;
        cls->vt_count ++;
     }
     cls->vt [i].impl = m;
     cls->vt [i].impl_cls = cls;
  }
}

int VtCount (TypePtr cls)
{
  cls = Resolve (cls);
  if (cls == NULL) return 0;
  if (!cls->vt_done) VtCompute (cls);
  return cls->vt_count;
}

VtSlot * VtGet (TypePtr cls, int i)
{
  cls = Resolve (cls);
  if (i < 0 || i >= VtCount (cls)) return NULL;
  return &cls->vt [i];
}

int VtIndex (TypePtr cls, const char * name)
{
  cls = Resolve (cls);
  int n = VtCount (cls);
  for (int i = 0; i < n; i++)
     if (strcmp (cls->vt [i].name, name) == 0) return i;
  return -1;
}

/* ukazatel na tabulku virtualnich metod (2026-11-16, drive zvlast v instr.c structBytes
   a translate.c GetCls / VptrRoot): zavadi ho prvni polymorfni trida v hierarchii */
bool BaseIsPolymorphic (TypePtr cls)
{
  return cls->base != NULL && IsPolymorphic (cls->base);
}

bool VptrHere (TypePtr cls)
{
  return IsPolymorphic (cls) && !BaseIsPolymorphic (cls);
}

TypePtr VptrRoot (TypePtr cls)
{
  while (!VptrHere (cls)) cls = cls->base;
  return cls;
}

/* objekt deklarovany const s inicializaci (const int N = 10; i const u aliasu typu) -
   hodnota je konstanta (instr.c constValue, translate.c IsIntConstObj) */
bool IsConstInit (ObjectPtr obj)
{
  if (obj == NULL || obj->init == NULL) return false;
  TypePtr t = Resolve (obj->type);
  return t != NULL && (t->Const || (obj->desc != NULL && obj->desc->Const));
}

/* presunuto z instr.cc / translate.c: bezstavove predikaty spolecne pro oba generatory */

bool isIntegralType (TypePtr t)
{
    t = Resolve (t);
    return t != NULL && (t->sort == Char || t->sort == Bool || t->sort == Int ||
                            t->sort == Enum || t->sort == NoSort);
}


bool isUnsignedType (TypePtr t)
{
    t = Resolve (t);
    return t != NULL && (t->sign == Unsigned || t->sort == Bool);
}


long scalarSize (TypePtr t)
{
    t = Resolve (t);
    if (t == NULL) return 8;
    switch (t->sort)
    {
        case Char:
        case Bool:   return 1;
        case Float:  return 4;
        case Double: return 8;
        case Int:
        case Enum:
        case NoSort: return t->length == Short ? 2 : t->length == Long ? 8 : 4;
        default:     return 8;    /* ukazatel, reference, std::string, kontejner, funkce */
    }
}


bool isStructVal (TypePtr t)
{
    return t != NULL && IsClsSort (t->sort);
}


bool isZeroInit (TypePtr t) { return IsStdString (t) || IsContainer (t); }


int fpKind (TypePtr t)       /* 0 - neni, 1 - float, 2 - double */
{
    if (t == NULL) return 0;
    if (t->sort == Double) return 2;
    if (t->sort == Float) return 1;
    return 0;
}


bool hasFloatMember (TypePtr t)
{
    for (TypePtr c = t; c != NULL; c = c->base)
        for (NodePtr p = c->list.first; p != NULL; p = p->next)
        {
            ObjectPtr m = p->item;
            if (! IsField (m)) continue;
            TypePtr mt = ArrayElem (m->type);
            if (fpKind (mt) != 0) return true;
            if (mt != NULL && IsClsSort (mt->sort) && hasFloatMember (mt)) return true;
        }
    return false;
}


bool hasUserDtor (TypePtr t)
{
    for (TypePtr c = t; c != NULL; c = c->base)
        if (c->dtor != NULL) return true;
    return false;
}


bool hasCopyCtor (TypePtr t)
{
    for (TypePtr c = t; c != NULL; c = c->base)
    {
        if (c->ctor == NULL) continue;
        NodePtr p = c->ctor->type->list.first;
        if (p != NULL && p->item->type != NULL && p->item->type->sort == Reference &&
            p->item->type->from == c)
            return true;
    }
    return false;
}


bool IsInClass (ObjectPtr f)
/* metoda / konstruktor definovany uvnitr tridy => static inline */
{
  return f->owner != NULL && !f->outdef;
}


bool BaseHasField (TypePtr base, const char * name)
{
  for (TypePtr b = base; b != NULL; b = b->base)
     for (NodePtr p = b->list.first; p != NULL; p = p->next)
        if (IsField (p->item) && strcmp (p->item->name, name) == 0) return true;
  return false;
}


TypePtr VT (TypePtr t)
/* typ hodnoty: bez aliasu a reference */
{
  t = Resolve (t);
  if (t != NULL && t->sort == Reference) t = Resolve (t->from);
  return t;
}


TypePtr ET (ExprPtr e)
{
  if (e == NULL) return NULL;
  return VT (e->type);
}


bool IsCharPtrT (TypePtr t)
/* char *, char [] */
{
  t = VT (t);
  return t != NULL && (t->sort == Pointer || t->sort == Array) &&
         t->from != NULL && Resolve (t->from)->sort == Char;
}


TypePtr PtrClass (TypePtr t)
/* ukazatel na tridu => trida, jinak NULL */
{
  t = VT (t);
  if (t == NULL || (t->sort != Pointer && t->sort != Array) || t->from == NULL) return NULL;
  t = Resolve (t->from);
  return (t != NULL && IsClsSort (t->sort)) ? t : NULL;
}

