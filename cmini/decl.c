
/* DECL.C */

#include "decl.h"
#include <string.h>
#include <stdio.h>    /* snprintf */

Scope     gscope;
Queue     glist;
Queue     printlist;   /* poradi pro vypis -p: objekty jako v textu + znacky */
Queue     ctorlist;     /* definice konstruktoru */
Queue     methodlist;   /* definice metod (s telem) */
Queue     classlist;    /* definovane struktury a tridy */
TypePtr   MethodClass = NULL;   /* trida prekladane metody nebo konstruktoru (this) */

static TypePtr QualClass = NULL; /* T::f - trida z kvalifikovaneho jmena v deklaratoru */

/*************************** RECORDED SYMBOLS *****************************/

/* Tela metod a konstruktoru uvnitr tridy mohou pouzivat polozky deklarovane
   az za nimi.  Symboly tela se proto pri cteni tridy jen zaznamenaji
   a prelozi se (prehraji pres ReplaySymbol) az na konci tridy. */

void (*ReplaySymbol) (void) = NULL;

typedef struct
{
  SYMBOL sy;
  char * text;      /* IdentVal, NumVal nebo StrVal */
  char   chr;
  bool   l, u, f;
  int    line, col;
  const char * file;  /* vlozeny soubor (preprocesor) nebo NULL */
  int    fline;       /* radka ve vlozenem souboru */
}
  TOKEN;

const char * ReplayFile = NULL;   /* lex.h */
int          ReplayLine = 0;

typedef struct
{
  TOKEN * items;
  int     cnt;
  int     cap;
}
  TOKENLIST;

static void SaveToken (TOKEN * t)
/* aktualni symbol => t */
{
  t->sy = sy;
  t->text = NULL;
  if (sy==IDENT) t->text = NewStr (IdentVal);
  else if (sy==NUM || sy==FLT) t->text = NewStr (NumVal);
  else if (sy==STR) t->text = NewStr (StrVal);
  t->chr = ChrVal;
  t->l = l_suffix;
  t->u = u_suffix;
  t->f = f_suffix;
  t->line = InpLineNum;
  t->col = InpColNum;
  const char * f = LexCurrentFile ();
  t->file = (f == NULL) ? NULL : NewStr ((char *) f);
  t->fline = LexCurrentLine ();
}

static void LoadToken (TOKEN * t)
/* t => aktualni symbol */
{
  sy = t->sy;
  if (t->text != NULL)
  {
     if (sy==IDENT) strmov (IdentVal, sizeof (IDENT_TYP), t->text);
     else if (sy==NUM || sy==FLT) strmov (NumVal, sizeof (NUM_TYP), t->text);
     else if (sy==STR) strmov (StrVal, sizeof (STR_TYP), t->text);
  }
  ChrVal = t->chr;
  l_suffix = t->l;
  u_suffix = t->u;
  f_suffix = t->f;
  InpLineNum = t->line;
  InpColNum = t->col;
  ReplayFile = t->file;
  ReplayLine = t->fline;
}

static void RecordBlock (TOKENLIST * list)
/* zaznamenej symboly od aktualniho (: nebo {) az po odpovidajici } vcetne */
{
  int depth = 0;
  list->items = NULL;
  list->cnt = 0;
  list->cap = 0;

  for (;;)
  {
     if (sy==EOS) Error ("Unexpected end of source in method body");
     if (list->cnt >= list->cap)
     {
        TOKEN * old = list->items;
        list->cap = (list->cap == 0) ? 64 : 2 * list->cap;
        list->items = (TOKEN *) New (list->cap * sizeof (TOKEN));
        if (old != NULL) memcpy (list->items, old, list->cnt * sizeof (TOKEN));
     }
     SaveToken (&list->items [list->cnt ++]);

     if (sy==LBRACE)
        depth ++;
     else if (sy==RBRACE)
     {
        depth --;
        if (depth == 0)
        {
           NextSymbol ();
           return;
        }
     }
     NextSymbol ();
  }
}

typedef struct REPLAY_REC REPLAY;   /* predem - ukazatel na sebe (cmm nezna struct X * v polozce) */
struct REPLAY_REC
{
  TOKENLIST *         list;
  int                 pos;
  TOKEN               saved;   /* symbol aktualni pred prehravanim */
  REPLAY *            prev;
};

static REPLAY * Replay = NULL;

static void ReplayNext (void)
{
  REPLAY * r = Replay;
  if (r->pos < r->list->cnt)
  {
     LoadToken (&r->list->items [r->pos ++]);
     return;
  }

  /* konec zaznamu: obnov symbol, ktery byl aktualni pred prehravanim */
  LoadToken (&r->saved);
  Replay = r->prev;
  ReplaySymbol = (Replay != NULL) ? ReplayNext : NULL;
}

static void StartReplay (TOKENLIST * list)
{
  REPLAY * r = NEW (REPLAY);
  r->list = list;
  r->pos = 0;
  SaveToken (&r->saved);
  r->prev = Replay;
  Replay = r;
  ReplaySymbol = ReplayNext;
  ReplayNext ();              /* prvni symbol zaznamu */
}

/* nahled na nasledujici symbol (aktualni symbol se nemeni) */
static TOKEN PeekNext;
static void (*PeekSaved) (void) = NULL;

static void PeekReplay (void)
{
  ReplaySymbol = PeekSaved;     /* puvodni zdroj symbolu (vstup nebo zaznam) */
  LoadToken (&PeekNext);
}

/* prostory jmen:  namespace X { ... }  - deklarace jsou globalni, kvalifikace X::
   se pri cteni symbolu vynecha (NamespaceQual, volaji ho lexery) */
static char * Namespaces [64];
static int NamespaceCnt = 0;
static bool InPeek = false;

static bool IsNamespaceName (const char * s)
{
  for (int i = 0; i < NamespaceCnt; i++)
     if (strcmp (Namespaces [i], s) == 0) return true;
  return false;
}

void NamespaceQual (void)
{
  if (InPeek || NamespaceCnt == 0 || !IsNamespaceName (IdentVal)) return;
  if (PeekSymbol () != DCOLON) return;    /* namespace X {, using namespace X; */
  NextSymbol ();                           /* :: */
  NextSymbol ();                           /* jmeno (i vnoreny prostor) */
}

SYMBOL PeekSymbol (void)
{
  TOKEN cur;
  SaveToken (&cur);
  InPeek = true;
  NextSymbol ();
  InPeek = false;
  SaveToken (&PeekNext);
  SYMBOL result = sy;
  PeekSaved = ReplaySymbol;
  ReplaySymbol = PeekReplay;    /* pristi NextSymbol vrati PeekNext */
  LoadToken (&cur);
  return result;
}

bool IsType (ObjectPtr obj);
extern bool DclSym [];

static bool PeekIsTypeStart (void)
/* symbol za aktualnim ( je ) nebo zacina typ => deklarace funkce,
   jinak  string s (p, n)  - prima inicializace */
{
  SYMBOL s = PeekSymbol ();
  if (s == RPAR) return true;
  if (s == IDENT)
  {
     const char * n = PeekNext.text;
     ObjectPtr o = Lookup ((char *) n);
     if (o == NULL)
        return strcmp (n, "string") == 0 || strcmp (n, "std") == 0 ||
               strcmp (n, "Sequence") == 0 || strcmp (n, "Dictionary") == 0;
     return IsType (o);
  }
  return DclSym [s];
}

bool IsQualifiedExpr (void)
/* T::f (...),  T::x  - jmeno tridy a :: na zacatku vyrazu (ne deklarace) */
{
  if (sy!=IDENT) return false;
  ObjectPtr obj = Lookup (IdentVal);
  if (obj == NULL || !IsType (obj)) return false;
  TypePtr t = Resolve (obj->type);
  if (t == NULL || !IsClsSort (t->sort)) return false;
  return PeekSymbol () == DCOLON;
}

/* odlozena tela metod a konstruktoru prave ctene tridy */
typedef struct DEFERRED_REC DEFERRED;
struct DEFERRED_REC
{
  ObjectPtr             obj;
  bool                  ctor;
  TOKENLIST             tokens;
  DEFERRED *            next;
};

static DEFERRED * * DeferTail = NULL;   /* != NULL => tela se odkladaji */

static void DeferBody (ObjectPtr obj, bool ctor)
{
  DEFERRED * d = NEW (DEFERRED);
  d->obj = obj;
  d->ctor = ctor;
  obj->deferred = true;
  RecordBlock (&d->tokens);
  if (sy==SEMICOLON) NextSymbol ();      /* void f () { ... }; */
  *DeferTail = d;
  DeferTail = &d->next;
}

void FunctionBody (ObjectPtr object);
void ConstructorBody (TypePtr cls, ObjectPtr obj);

void ConstructorDecl (TypePtr cls);
void DestructorDecl (TypePtr cls, bool virt);
static void ImplicitInitCtor (TypePtr cls);

/* stejne parametry (prekryti virtualni metody); jine - metoda virtualni metodu
   baze jen skryva (C++: void f (int, bool = false) ve tride odvozene) */
static bool SameParams (TypePtr f1, TypePtr f2)
{
  f1 = Resolve (f1);
  f2 = Resolve (f2);
  if (f1 == NULL || f2 == NULL) return true;
  NodePtr a = f1->list.first, b = f2->list.first;
  while (a != NULL && a->item->type != NULL && a->item->type->sort == Void) a = a->next;
  while (b != NULL && b->item->type != NULL && b->item->type->sort == Void) b = b->next;
  while (a != NULL && b != NULL)
  {
     if (a->item->type == NULL || b->item->type == NULL)
     {
        if (a->item->type != b->item->type) return false;   /* ... */
     }
     else if (!SameType (a->item->type, b->item->type))
        return false;
     a = a->next;
     b = b->next;
  }
  return a == NULL && b == NULL;
}
ExprPtr calc (ExprPtr e1, ExprPtr e2, SYMBOL op);

ObjectPtr Fce;

int DisplayLevel;
ScopePtr Display [DISPLAY_MAX];

/********************************** INIT **********************************/

void InitDclSym (void);

void InitDecl (void)
{
  InitTypes ();
  InitDclSym ();

  gscope.root = NULL;
  Clear (&glist);
  Clear (&printlist);
  UsesContainers = false;
  Clear (&ctorlist);
  Clear (&methodlist);
  Clear (&classlist);
  Fce = NULL;
  MethodClass = NULL;
  QualClass = NULL;
  NamespaceCnt = 0;

  DisplayLevel = 0;
  CLR_ARR (Display);
  Display [0] = &gscope;
}

/********************************* QUEUE **********************************/

void Clear (QueuePtr queue)
{
   queue->first = NULL;
   queue->last = NULL;
}

void Add (QueuePtr queue, NodePtr node)
{
  node->prev = queue->last;
  node->next = NULL;

  if (queue->first==NULL)
     queue->first = node;
  else
     queue->last->next = node;

  queue->last = node;
}

void Append (QueuePtr queue, ObjectPtr obj)
{
  NodePtr decl;

  Check (obj!=NULL);
  decl = NEW (Node);
  decl->item = obj;
  Add (queue, decl);
}

void Join (QueuePtr dst, QueuePtr add)
/* Pridej prvky fronty 'add' do fronty 'dst' */
{
  if (add->first!=NULL)
     if (dst->first==NULL)
         *dst = *add;
     else
     {
         dst->last->next = add->first;
         add->first->prev = dst->last;
         dst->last = add->last;
     }
}

/********************************* SCOPE **********************************/

void OpenScope (ScopePtr s)
{
  DisplayLevel ++;
  if (DisplayLevel >= DISPLAY_MAX) Error ("Too many nested scope levels");

  Display [DisplayLevel] = s;
}

void CloseScope (void)
{
  Check (DisplayLevel > 0);
  DisplayLevel --;
}

/********************************** TREE ***********************************/

void EnterPrim (ObjectPtr * root, ObjectPtr add)
/* Vloz identifikator do stromu */
{
  int cmp;

  for (;;)
     if (*root==NULL)
     {
        *root = add;
        add->left = NULL;
        add->right = NULL;
        return;
     }
     else
     {
        cmp = strcmp (add->name, (*root)->name);
        if (cmp < 0)
           root = &(*root)->left;
        else if (cmp>0)
           root = &(*root)->right;
        else
           ErrorId ("Duplicated identifier", add->name);
     }
}

void Enter (ScopePtr scope, ObjectPtr add)
/* Vloz identifikator do stromu */
{
  EnterPrim (&scope->root, add);
}

/* ---------------------------------------------------------------------- */

ObjectPtr SearchPrim (ObjectPtr root, IDENT_TYP name)
/* Hledej zadany identifikator ve stromu */
{
  int cmp;

  for (;;)
     if (root==NULL)
        return (NULL);
     else
     {
        cmp = strcmp (name, root->name);
        if (cmp<0)
           root = root->left;
        else if (cmp>0)
           root = root->right;
        else
           return (root);
     }
}

ObjectPtr Search (ScopePtr scope, IDENT_TYP name)
/* Hledej zadany identifikator ve stromu */
{
   return SearchPrim (scope->root, name);
}

/* ---------------------------------------------------------------------- */

ObjectPtr Lookup (IDENT_TYP name)
/* Hledej zadany identifikator ve vsech stromech */
{
  int i;
  ObjectPtr obj;

  i = DisplayLevel;
  obj = Search (Display[i], name);
  while (obj==NULL && i>0)
  {
     i--;
     obj = Search (Display[i], name);
  }

  return (obj);
}

ObjectPtr Find (IDENT_TYP name)
/* Hledej zadany identifikator. Ohlas chybu, pokud nebyl nalezen. */
{
  ObjectPtr obj;

  obj = Lookup (name);
  if (obj==NULL) ErrorId ("Unknown identifier", name);
  return (obj);
}

/* ---------------------------------------------------------------------- */

ObjectPtr LookupMember (TypePtr typ, IDENT_TYP name)
/* Hledej polozku struktury, potom v bazovych tridach */
{
  while (typ != NULL)
  {
     ObjectPtr obj = Search (&typ->scope, name);
     if (obj != NULL) return obj;
     typ = typ->base;
  }
  return NULL;
}

bool IsMemberOf (ObjectPtr obj, TypePtr typ)
/* obj je polozka (nebo metoda) struktury typ nebo jejich bazovych trid */
{
  if (obj == NULL) return false;
  while (typ != NULL)
  {
     if (Search (&typ->scope, obj->name) == obj) return true;
     typ = typ->base;
  }
  return false;
}

int OpenClassScopes (TypePtr cls)
/* zpristupni polozky tridy a jejich bazovych trid (baze nejdrive),
   vraci pocet otevrenych oboru */
{
  int n = 0;
  if (cls->base != NULL) n = OpenClassScopes (cls->base);
  OpenScope (&cls->scope);
  return n + 1;
}

void CloseScopes (int n)
{
  while (n-- > 0) CloseScope ();
}

ObjectPtr FindMember (TypePtr typ, IDENT_TYP name)
/* Hledej polozku struktury. Ohlas chybu, pokud nebyla nalezena. */
{
  ObjectPtr obj;

  obj = LookupMember (typ, name);
  if (obj==NULL)
     ErrorId ("Member of structure expected", name);
  return (obj);
}

/************************ CREATE TYPES AND OBJECTS ************************/

TypePtr NewType (sort_typ sort)
/* Vytvor novy typ, nastav polozku sort */
{
  TypePtr typ;

  typ = NEW (Type);
  typ->sort = sort;

  return typ;
}

TypePtr Resolve (TypePtr typ)
{
  if (typ==NULL)
     return NULL;

  if (typ->sort==Name || typ->sort==Tag)
     typ = typ->from;

  return typ;
}

TypePtr Link (sort_typ sort, TypePtr desc)
{
  TypePtr typ;

  typ = NewType (sort);
  typ->desc = desc;
  typ->from = Resolve (desc);

  return typ;
}

TypePtr Alias (IDENT_TYP name, sort_typ sort, TypePtr desc)
{
  TypePtr typ;

  Check (sort==Name || sort==Tag);
  typ = Link (sort, desc);
  typ->tag = NewStr (name);

  return typ;
}

ObjectPtr NewObject (IDENT_TYP name, TypePtr desc)
/* Vytvor objekt daneho jmena a typu */
{
  ObjectPtr obj;

  obj = NEW (Object);
  obj->name = NewStr (name);
  obj->desc = desc;
  obj->type = Resolve (desc);

  return (obj);
}

/* ---------------------------------------------------------------------- */

TypePtr CreateType (IDENT_TYP id, sort_typ sort, bool def)
{
  ObjectPtr obj;
  TypePtr   typ;

  if (id==NULL || id[0]==0)
  {
     /* anonymous */
     typ = NewType (sort);
     if (!def) Error ("{ expected");
  }
  else
  {
     obj = Search (Display[DisplayLevel], id);
     if (obj==NULL)
     {
        /* new type */
        typ = NewType (sort);
        obj = NewObject (id, typ);
        typ->tag = obj->name;
        Enter (Display[DisplayLevel], obj);
     }
     else
     {
        /* type already exists */
        typ = obj->type;

        if (typ->sort!=sort || obj->storage==Typedef)
           ErrorId ("Type mismatch in redeclaration", id);

        if (def && typ->defined)
           ErrorId ("Multiple declaration", id);
     }
  }

  if (!def)
     typ = Alias (id, Tag, typ);

  return typ;
}

/******************************** ENUM ************************************/

TypePtr enum_specifier (void)
{
  IDENT_TYP id;
  TypePtr typ;
  ObjectPtr elem;

  CheckSymbol (ENUM_);

  if (sy==IDENT)
     ReadIdent (id);
  else
     id[0] = 0;

  typ = CreateType (id, Enum, sy==LBRACE);

  if (sy==LBRACE)
  {
     if (typ->defined) ErrorId ("Multiple declaration", id);
     typ->defined = true;
     NextSymbol ();

     /* enumerator_list (prazdny vycet enum X { } - gram ho generuje) */
     if (sy != RBRACE) for (;;)
     {
        /* enumerator */
        ReadIdent (id);
        elem = NewObject (id, typ);
        Enter (Display[DisplayLevel], elem);
        Append (&typ->list, elem);

        if (sy==ASSIGN)
        {
           NextSymbol ();
           elem->init = const_expr ();
        }
        if (sy!=COMMA) break;
        NextSymbol ();
     }
     CheckSymbol (RBRACE);
  }

  return typ;
}

/******************************* STRUCTURE ********************************/

TypePtr CurrentClass = NULL;   /* prave zpracovavana struktura */

bool IsType (ObjectPtr obj);

TypePtr struct_specifier (void)
{
  IDENT_TYP id;
  sort_typ sort;
  TypePtr typ;
  Queue queue;

  if (sy==STRUCT_) sort = Struct;
  else if (sy==UNION_) sort = Union;
  else if (sy==CLASS_) sort = Class;
  else Bug ();

  NextSymbol ();

  if (sy==IDENT)
     ReadIdent (id);
  else
     id[0] = 0;

  typ = CreateType (id, sort, sy==LBRACE || sy==COLON);

  /* bazova trida:  class B : public A { ... } */
  if (sy==COLON)
  {
     NextSymbol ();
     if (sy==PUBLIC_ || sy==PRIVATE_ || sy==PROTECTED_) NextSymbol ();
     if (sy!=IDENT) Error ("Base class expected");
     ObjectPtr b = Lookup (IdentVal);
     if (!IsType (b) || !IsClsSort (Resolve (b->type)->sort) || Resolve (b->type)->sort == Union)
        ErrorId ("Base class expected", IdentVal);
     TypePtr bt = Resolve (b->type);
     if (!bt->defined) ErrorId ("Base class is not defined", IdentVal);
     if (bt == typ) ErrorId ("Class cannot be derived from itself", IdentVal);
     typ->base = bt;
     NextSymbol ();
     if (sy==COMMA) Error ("Multiple inheritance is not supported");
     if (sy!=LBRACE) Error ("{ expected");
  }

  if (sy==LBRACE)
  {
     if (typ->defined) ErrorId ("Multiple declaration", id);
     typ->defined = true;
     NextSymbol ();

     /* struct_declaration_list */
     /* struct_declaration */
     /* specifier_qualifier_list */
     /* struct_declarator_list */
     /* struct_declarator */

     int scopes = 0;
     if (typ->base != NULL) scopes = OpenClassScopes (typ->base);  /* polozky baze */
     OpenScope (&typ->scope);
     TypePtr save_class = CurrentClass;
     CurrentClass = typ;

     DEFERRED * deferred = NULL;
     DEFERRED * * save_tail = DeferTail;
     DeferTail = &deferred;

     while (sy!=RBRACE)
     {
        /* public:  private:  protected:  (pristup se nekontroluje) */
        if (sy==PUBLIC_ || sy==PRIVATE_ || sy==PROTECTED_)
        {
           NextSymbol ();
           CheckSymbol (COLON);
           continue;
        }
        /* friend class X;  friend int f (...);  - pristup se nekontroluje => preskocit */
        if (sy==FRIEND_)
        {
           while (sy!=SEMICOLON && sy!=EOS)
              NextSymbol ();
           CheckSymbol (SEMICOLON);
           continue;
        }
        declaration (&queue, Width | Body);
        Join (&typ->list, &queue);
     }

     /* tela metod a konstruktoru - az jsou zname vsechny polozky tridy */
     DeferTail = save_tail;
     for (DEFERRED * d = deferred; d != NULL; d = d->next)
     {
        StartReplay (&d->tokens);
        if (d->ctor)
           ConstructorBody (typ, d->obj);
        else
           FunctionBody (d->obj);
        if (sy!=RBRACE) Bug ();             /* zaznam musi byt cely zpracovan */
     }
     NextSymbol ();
     ImplicitInitCtor (typ);   /* polozky s inicializaci ve tride, bez konstruktoru */

     CurrentClass = save_class;
     CloseScope ();
     CloseScopes (scopes);

     Append (&classlist, NewObject (id, typ));
  }

  return typ;
}

/************************* DECLARATION SPECIFIER **************************/

void CheckSort (TypePtr t)
{
  if (t->sort!=NoSort)
     Error ("Too many types in declaration");
}

void CheckLength (TypePtr t)
{
  if (t->length!=NoLength)
     Error ("Too many types in declaration");
}

void CheckSign (TypePtr t)
{
  if (t->sign!=NoSign)
     Error ("Too many types in declaration");
}

static void CheckNumeric (TypePtr t)
{
  if (! IsNumericSort (t->sort))
     Error ("Too many types in declaration");
}

void CheckInt (TypePtr t)
{
  if (! IsIntegerSort (t->sort))
     Error ("Too many types in declaration");
}

void CheckAll (TypePtr t)
{
  if (t->sort!=NoSort ||
      t->length!=NoLength ||
      t->sign!=NoSign)
     Error ("Too many types in declaration");
}

/* ---------------------------------------------------------------------- */

TypePtr sim_declaration_specifiers (void)
{
  TypePtr typ;

  typ = NewType (NoSort);

  for (;;)
     /* simple_type_name */
     if (sy==CHAR_)
     {
        CheckSort (typ);
        typ->sort = Char;
        NextSymbol ();
     }
     else if (sy==INT_)
     {
        CheckSort (typ);
        typ->sort = Int;
        NextSymbol ();
     }
     else if (sy==FLOAT_)
     {
        CheckSort (typ);
        CheckSign (typ);
        typ->sort = Float;
        NextSymbol ();
     }
     else if (sy==DOUBLE_)
     {
        CheckSort (typ);
        CheckSign (typ);
        typ->sort = Double;
        NextSymbol ();
     }
     else if (sy==VOID_)
     {
        CheckSort (typ);
        CheckSign (typ);
        CheckLength (typ);
        typ->sort = Void;
        NextSymbol ();
     }
     else if (sy==BOOL_)
     {
        CheckAll (typ);
        typ->sort = Bool;
        NextSymbol ();
     }

     /* type_qualifier */
     else if (sy==CONST_)
     {
        typ->Const = true;
        NextSymbol ();
     }
     else if (sy==VOLATILE_)
     {
        typ->Volatile = true;
        NextSymbol ();
     }

     else if (sy==SHORT_)
     {
        CheckLength (typ);
        CheckNumeric (typ);
        typ->length = Short;
        NextSymbol ();
     }
     else if (sy==LONG_ && typ->length == Long)
     {
        typ->long_long = true;
        NextSymbol ();                 /* long long = long (64 bitu, x86-64) */
     }
     else if (sy==LONG_)
     {
        CheckLength (typ);
        CheckNumeric (typ);
        typ->length = Long;
        NextSymbol ();
     }

     else if (sy==SIGNED_)
     {
        CheckSign (typ);
        CheckInt (typ);
        typ->sign = Signed;
        NextSymbol ();
     }
     else if (sy==UNSIGNED_)
     {
        CheckSign (typ);
        CheckInt (typ);
        typ->sign = Unsigned;
        NextSymbol ();
     }
     else break;

  if (typ->sort==NoSort &&
      typ->sign==NoSign &&
      typ->length==NoLength)
     Error ("Type name expected");

  return typ;
}

bool IsType (ObjectPtr obj)
{
  sort_typ sort;

  if (obj==NULL) return false;
  if (obj->storage==Typedef) return true;
  sort = obj->type->sort;
  /* jmeno struktury (tag), ne promenna typu struktura:
     CreateType nastavi typ->tag = obj->name */
  if ((IsClsSort (sort) || sort==Enum) && obj->type->tag == obj->name) return true;
  return false;
}

/* Sequence <T>, Dictionary <K, V>  (pokud nejsou deklarovany jinak) */
static bool IsContainerName (void)
{
  return sy==IDENT && (strcmp (IdentVal, "Sequence") == 0 || strcmp (IdentVal, "Dictionary") == 0) &&
         Lookup (IdentVal) == NULL;
}

bool UsesContainers = false;
bool RangeForDecl = false;

ObjectPtr declarator (TypePtr typ);
TypePtr declaration_specifiers (void);

static TypePtr ContainerArg (bool key)
/* parametr typu:  int,  GrmRule *,  string  (prvek: hodnota v 8bajtovem slotu) */
{
  TypePtr t = declaration_specifiers ();
  ObjectPtr o = declarator (t);
  if (o->name [0] != 0) ErrorId ("Type expected, found identifier", o->name);
  TypePtr r = Resolve (o->type);
  if (r->sort == StdString || r->sort == Pointer || r->sort == Char || r->sort == Bool ||
      r->sort == Enum || r->sort == Int)
     return o->type;
  if (r->sort == NoSort && (r->sign != NoSign || r->length != NoLength))
     return o->type;                     /* long, unsigned, unsigned long, short */
  if (!key && (r->sort == Float || r->sort == Double))
     return o->type;
  /* prvek struktura bez retezcu, kontejneru a destruktoru (kopie po bajtech):
     Sequence <DtorVar> (zdroje cmm) */
  if (!key && IsClsSort (r->sort) && r->defined && !NeedsDtor (r, true) && !IsPolymorphic (r))
     return o->type;
  Error (key ? "Dictionary key must be integer, enum, pointer or string"
             : "Sequence / Dictionary element must be number, pointer or string");
  return NULL;
}

static void CloseAngle (void)
/* > na konci parametru typu;  >> (vnorene typy) se rozdeli na dva symboly > */
{
  if (sy==GREATER) NextSymbol ();
  else if (sy==SHR) sy = GREATER;        /* prvni > je zpracovan, druhy zustava */
  else Error ("> expected");
}

static TypePtr ContainerType (void)
/* Sequence <T>,  Dictionary <K, V> */
{
  bool dict = strcmp (IdentVal, "Dictionary") == 0;
  NextSymbol ();
  CheckSymbol (LESS);
  TypePtr typ = NewType (dict ? StdDictionary : StdSequence);
  if (dict)
  {
     typ->key = ContainerArg (true);
     CheckSymbol (COMMA);
  }
  typ->elem = ContainerArg (false);
  CloseAngle ();
  UsesContainers = true;
  return typ;
}

/* std::string:  string  (pokud neni deklarovan jinak)  nebo  std::string */
static bool IsStringName (void)
{
  return sy==IDENT && (strcmp (IdentVal, "string") == 0 || strcmp (IdentVal, "std") == 0) &&
         Lookup (IdentVal) == NULL;
}

TypePtr declaration_specifiers (void)
{
  TypePtr typ;
  ObjectPtr obj;

  /* const pred jmenem typu:  const string & s,  const T * p */
  bool cnst = false;
  while (sy==CONST_ || sy==VOLATILE_)
  {
     if (sy==CONST_) cnst = true;
     NextSymbol ();
     if (sy!=IDENT && sy!=STRUCT_ && sy!=UNION_ && sy!=CLASS_ && sy!=ENUM_)
     {
        /* const int ... - zbytek precte sim_declaration_specifiers */
        typ = sim_declaration_specifiers ();
        if (cnst) typ->Const = true;
        return typ;
     }
  }

  if (sy==STRUCT_ || sy==UNION_ || sy==CLASS_)
     typ = struct_specifier ();
  else if (sy==ENUM_)
     typ = enum_specifier ();
  else if (IsContainerName ())
     typ = ContainerType ();
  else if (IsStringName ())
  {
     if (strcmp (IdentVal, "std") == 0)
     {
        NextSymbol ();
        CheckSymbol (DCOLON);
        if (sy!=IDENT || strcmp (IdentVal, "string") != 0)
           Error ("Only std::string is supported from namespace std");
     }
     NextSymbol ();
     typ = NewType (StdString);

     /* string::size_type  =>  unsigned long */
     if (sy==DCOLON)
     {
        NextSymbol ();
        if (sy!=IDENT || strcmp (IdentVal, "size_type") != 0)
           Error ("string::size_type expected");
        NextSymbol ();
        typ = NewType (Int);
        typ->sign = Unsigned;
        typ->length = Long;
     }
     if (cnst) typ->Const = true;
  }
  else
  {
     typ = NULL;
     if (sy==IDENT)
     {
        obj = Lookup (IdentVal);
	if (IsType (obj))
        {
	   NextSymbol ();
           typ = Alias (obj->name, Name, obj->type);
           typ->tdef = obj;
        }
     }

     if (typ==NULL)
        typ = sim_declaration_specifiers ();
     if (cnst) typ->Const = true;
  }

  return typ;
}

/******************************* MODIFIERS ********************************/

void type_qualifier_list (TypePtr typ)
{
  for (;;)
    /* type_qualifier */
    if (sy==CONST_)
    {
       typ->Const = true;
       NextSymbol ();
    }
    else if (sy==VOLATILE_)
    {
       typ->Volatile = true;
       NextSymbol ();
    }

    else break;
}

/******************************* DECLARATOR *******************************/

#define DIMENSION_MAX 16

ObjectPtr declarator (TypePtr typ);

void ParameterList (TypePtr typ)
/* seznam parametru funkce typu 'typ', symbol ( jiz byl precten, cte se vcetne ) */
{
  int cnt = 0;

  /* parameter_type_list, parameter_list */
  for (;;)
     if (sy==RPAR)
        break;
     else if (sy==DOTS)
     {
        ObjectPtr pobj;

        pobj = NewObject ("...", NULL);
        Append (&typ->list, pobj);

        NextSymbol ();
        break;
     }
     else
     {
        TypePtr ptyp;
        ObjectPtr pobj;

        bool reg = false;
        if (sy==REGISTER_)          /* int f (register int n) */
        {
           reg = true;
           NextSymbol ();
        }

        ptyp = declaration_specifiers ();
        pobj = declarator (ptyp);
        if (reg) pobj->storage = Register_;

        /* parametr typu pole je ukazatel:  char * argv []  =>  char * * argv */
        if (pobj->type != NULL && pobj->type->sort == Array)
        {
           pobj->desc = Link (Pointer, pobj->type->desc);
           pobj->type = Resolve (pobj->desc);
        }

        /* implicitni hodnota:  int level = 0 */
        if (sy==ASSIGN)
        {
           NextSymbol ();
           pobj->init = simple_expr ();
        }

        /* add parameter */
        if (pobj->name[0] != 0)
           Enter (&typ->scope, pobj);
        Append (&typ->list, pobj);

        cnt ++;
        if (pobj->type != NULL && pobj->type->sort==Void)   /* f (void), ne f (void * p) */
        {
           if (cnt!=1)
              Error ("Invalid parameter type (void)");
           break;
        }

        if (sy!=COMMA) break;
        NextSymbol ();
     }

  CheckSymbol (RPAR);
}

ObjectPtr declarator (TypePtr typ)
{
  ObjectPtr obj;
  IDENT_TYP id;
  TypePtr ref;

  /* pointer or reference */
  while (sy==ASTERISK || sy==AND)
  {
     /* declaration_specifiers */
     if (sy==ASTERISK)
     {
        NextSymbol ();
        typ = Link (Pointer, typ);
     }
     else if (sy==AND)
     {
        NextSymbol ();
        typ = Link (Reference, typ);
     }
     else Bug ();

     type_qualifier_list (typ); /* !!! */
  }

  /* direct_declarator */
  if (sy==LPAR)
  {
      NextSymbol ();
      obj = declarator (NULL);
      CheckSymbol (RPAR);
  }
  else if (sy==IDENT)
  {
     ReadIdent (id);

     /* metoda definovana mimo tridu:  int T::f (...) { ... } */
     if (sy==DCOLON)
     {
        ObjectPtr c = Lookup (id);
        if (!IsType (c) || !IsClsSort (Resolve (c->type)->sort))
           ErrorId ("Class name expected", id);
        QualClass = Resolve (c->type);
        NextSymbol ();
        ReadIdent (id);
     }

     obj = NewObject (id, NULL);
  }
  else
  {
     /* abstract declarator */
     obj = NewObject ("", NULL);
  }

  for (;;)
     if (sy==LBRACK)
     {
        /* a [N] [M] = pole N prvku, kazdy je pole M prvku
           => posledni rozmer musi byt nejvnitrnejsi typ */
        ExprPtr ranges [DIMENSION_MAX];
        int cnt = 0;
        while (sy==LBRACK)
        {
           NextSymbol ();
           if (cnt >= DIMENSION_MAX) Error ("Too many array dimensions");
           ranges [cnt++] = (sy==RBRACK) ? NULL : const_expr ();
           CheckSymbol (RBRACK);
        }
        for (int i = cnt-1; i >= 0; i--)
        {
           typ = Link (Array, typ);
           typ->range = ranges [i];
        }
     }

     else if (sy==LPAR && Fce != NULL && typ != NULL && Resolve (typ)->sort == StdString &&
              obj->name [0] != 0 && ! PeekIsTypeStart ())
        break;      /* lokalni  string s (p, n);  - inicializace, ne deklarace funkce */

     else if (sy==LPAR)
     {
        NextSymbol ();
        typ = Link (Function, typ);
        /* int T::f (TType * p) - jmena typu ze tridy T jsou v parametrech viditelna */
        int qscopes = (QualClass != NULL) ? OpenClassScopes (QualClass) : 0;
        ParameterList (typ);
        CloseScopes (qscopes);

        /* metoda:  int f () const   void f () override */
        for (;;)
           if (sy==CONST_) NextSymbol ();
           else if (sy==IDENT && (strcmp (IdentVal, "override") == 0 ||
                                  strcmp (IdentVal, "final") == 0)) NextSymbol ();
           else break;
     }
     else break;

  /* link 'obj' and 'typ' */
  Check (obj!=NULL);

  if (obj->type==NULL)
  {
     obj->desc = typ;
     obj->type = Resolve (typ);
  }
  else
  {
     ref = obj->type;
     while (ref->from!=NULL) ref=ref->from;
     ref->desc = typ;
     ref->from = Resolve (typ);
  }

  return obj;
}

/******************************** IS DECL *********************************/

bool DclSym [SYMBOL_MAX+1];

void InitDclSym (void)
{
  DclSym [CHAR_] = 1;
  DclSym [INT_] = 1;
  DclSym [FLOAT_] = 1;
  DclSym [DOUBLE_] = 1;
  DclSym [VOID_] = 1;
  DclSym [BOOL_] = 1;
  DclSym [INLINE_] = 1;

  DclSym [SHORT_] = 1;
  DclSym [LONG_] = 1;

  DclSym [SIGNED_] = 1;
  DclSym [UNSIGNED_] = 1;

  DclSym [CONST_] = 1;
  DclSym [VOLATILE_] = 1;

  DclSym [ENUM_] = 1;
  DclSym [STRUCT_] = 1;
  DclSym [UNION_] = 1;
  DclSym [CLASS_] = 1;
  DclSym [VIRTUAL_] = 1;

  DclSym [TYPEDEF_] = 1;
  DclSym [AUTO_] = 1;
  DclSym [REGISTER_] = 1;
  DclSym [STATIC_] = 1;
  DclSym [EXTERN_] = 1;
}

bool IsDecl (void)
{
  ObjectPtr obj;
  TypePtr   typ;
  sort_typ  sort;

  if (sy==IDENT)
  {
     if (IsStringName ()) return true;     /* string s;  std::string s; */
     if (IsContainerName ()) return true;  /* Sequence <T> s; */
     obj = Lookup (IdentVal);
     if (obj==NULL) return (false);

     typ = obj->type;
     if (typ==NULL) return (false);

     sort=typ->sort;

     return IsType (obj);
  }
  else
  {
     return (DclSym [sy]);
  }
}

/******************************* FUNCTIONS ********************************/

void FunctionBody (ObjectPtr object)
{
  /* metoda: polozky tridy (a bazovych trid) jsou viditelne, this */
  int scopes = 0;
  TypePtr save_method = MethodClass;
  if (object->owner != NULL)
  {
     if (object->owner != CurrentClass)
        scopes = OpenClassScopes (object->owner);   /* definice mimo tridu */
     if (object->method) MethodClass = object->owner;
  }

  OpenScope (&object->type->scope);
  Fce = object;

  object->body = statement ();
  if (sy==SEMICOLON) NextSymbol ();

  Fce = NULL;
  CloseScope ();

  MethodClass = save_method;
  CloseScopes (scopes);

  if (object->owner != NULL)
     Append (&methodlist, object);
}

/****************************** DECLARATION *******************************/

void SetStorage (storage_typ * var, storage_typ value)
{
  if (*var!=NoStorage)
     Error ("Too many storage classes in declaration");
  *var = value;
  NextSymbol ();
}

storage_typ storage_class_specifier (void)
{
  storage_typ storage = NoStorage;

  if (DisplayLevel==0)
     if (sy==AUTO_ || sy==REGISTER_)
        Error ("Invalid storage class");

  if (sy==TYPEDEF_)
     SetStorage (&storage, Typedef);
  else if (sy==EXTERN_)
     SetStorage (&storage, Extern);
  else if (sy==AUTO_)
     SetStorage (&storage, Auto);
  else if (sy==REGISTER_)
     SetStorage (&storage, Register_);
  else if (sy==STATIC_)
     SetStorage (&storage, Static);

  /* extern "C" int main (...) - jen zpusob linkovani, ne extern */
  if (storage==Extern && sy==STR && strcmp (StrVal, "C") == 0)
  {
     NextSymbol ();
     storage = NoStorage;
  }

  return (storage);
}

#define OPT(n) ((opt&(n))!=0)

static ObjectPtr PrintMarker (int kind, ObjectPtr ref, TypePtr type);
static void ReplaceInPrintList (ObjectPtr old_item, ObjectPtr new_item);

static void ArraySizeFromInit (ObjectPtr obj)
/* pole bez velikosti:  int a [] = { 1, 2, 3 };  char s [] = "abc"; */
{
  TypePtr t = obj->type;
  if (t == NULL || t->sort != Array || t->range != NULL || obj->init == NULL) return;

  long n = 0;
  ExprPtr e = obj->init;
  if (e->op == LBRACE)
  {
     n = 1;
     for (ExprPtr x = e->e1; x != NULL && x->op == COMMA; x = x->e1)
        n ++;
     if (e->e1 == NULL) n = 0;
  }
  else if (e->op == STR)
     n = strlen (e->id) + 1;
  else
     return;

  char buf [32];
  snprintf (buf, sizeof (buf), "%ld", n);
  ExprPtr r = NEW (Expr);
  r->op = NUM;
  r->id = NewStr (buf);
  r->type = &IntDef;

  /* novy typ pole (typ muze byt sdileny s jinymi deklaracemi) */
  TypePtr a = NewType (Array);
  *a = *t;
  a->range = r;
  obj->type = a;
  if (obj->desc == t) obj->desc = a;
}

static void RemoveFromQueue (QueuePtr queue, ObjectPtr obj)
/* odeber objekt z fronty (nenalezeny => nic) */
{
  NodePtr p = queue->first;
  while (p != NULL && p->item != obj)
     p = p->next;
  if (p == NULL) return;

  if (p->prev == NULL) queue->first = p->next; else p->prev->next = p->next;
  if (p->next == NULL) queue->last = p->prev;  else p->next->prev = p->prev;
}

static NodePtr NextParam (NodePtr p)
/* preskoc parametr typu void:  int f (void) */
{
  while (p != NULL && p->item->type != NULL && p->item->type->sort == Void)
     p = p->next;
  return p;
}

static void MergeDefaults (TypePtr decl, TypePtr def, char * name)
/* deklarace (prototyp, metoda ve tride) a definice teze funkce:
   stejny pocet parametru, chybejici implicitni hodnoty v 'def'
   se prevezmou z 'decl'  (void f (int a = 1);  void f (int a) { ... }) */
{
  NodePtr a = NextParam (decl->list.first);
  NodePtr b = NextParam (def->list.first);
  while (a != NULL && b != NULL)
  {
     if ((a->item->type == NULL) != (b->item->type == NULL))  /* ... */
        break;
     if (b->item->init == NULL && a->item->init != NULL)
     {
        b->item->init = a->item->init;
        b->item->merged = true;      /* vypis: implicitni hodnota jen v deklaraci */
     }
     a = NextParam (a->next);
     b = NextParam (b->next);
  }
  if (a != NULL || b != NULL)
     ErrorId ("Parameters do not match previous declaration of", name);
}

void declaration (QueuePtr queue, int opt)
{
  storage_typ storage;
  ObjectPtr   obj;
  TypePtr     type;
  sort_typ    sort;

  bool        inl = false;
  bool        virt = false;

  Clear (queue);

  for (;;)
     if (sy==INLINE_) { inl = true; NextSymbol (); }
     else if (sy==VIRTUAL_)
     {
        if (CurrentClass == NULL) Error ("virtual is allowed only inside class");
        virt = true;
        NextSymbol ();
     }
     else break;
  storage = storage_class_specifier ();
  if (sy==INLINE_) { inl = true; NextSymbol (); }

  /* destruktor uvnitr tridy:  ~T () { ... } */
  if (sy==TILDA && CurrentClass != NULL)
  {
     DestructorDecl (CurrentClass, virt);
     return;
  }

  type = declaration_specifiers ();

  /* konstruktor uvnitr struktury:  Item (...) ... */
  if (sy==LPAR && CurrentClass != NULL && Resolve (type) == CurrentClass)
  {
     ConstructorDecl (CurrentClass);
     return;
  }

  /* konstruktor mimo strukturu:  Item::Item (...) { ... } */
  if (sy==DCOLON)
  {
     TypePtr cls = Resolve (type);
     NextSymbol ();

     /* destruktor mimo tridu:  T::~T () { ... } */
     if (sy==TILDA && cls != NULL && IsClsSort (cls->sort))
     {
        DestructorDecl (cls, false);
        return;
     }

     if (cls == NULL || !IsClsSort (cls->sort) || sy!=IDENT || cls->tag == NULL ||
         strcmp (IdentVal, cls->tag) != 0)
        Error ("Constructor name expected after ::  (methods: type T::name (...))");
     NextSymbol ();

     int scopes = OpenClassScopes (cls);   /* polozky tridy a baze jsou viditelne */
     ConstructorDecl (cls);
     CloseScopes (scopes);
     return;
  }

  /* init_declarator_list */
  for (;;)
  {
     /* init_declarator */
     obj = declarator (type);

     /* definice metody mimo tridu:  int T::f (...) { ... } */
     if (QualClass != NULL)
     {
        TypePtr cls = QualClass;
        QualClass = NULL;
        ObjectPtr m = Search (&cls->scope, obj->name);

        /* definice staticke polozky:  const int T::x = 0; */
        if (m != NULL && !m->fce && m->storage == Static && m->owner == cls)
        {
           m->outdef = true;          /* definice je v tomto souboru (translate.c) */
           if (sy==ASSIGN)
           {
              NextSymbol ();
              if (m->init != NULL) ErrorId ("Multiple initialization of", obj->name);
              m->init = const_expr ();
           }
           CheckSymbol (SEMICOLON);
           return;
        }

        if (m == NULL || !m->fce || m->owner != cls)
           ErrorId ("Method is not declared in class", obj->name);
        if (m->body != NULL)
           ErrorId ("Multiple definition of method", obj->name);
        if (sy!=LBRACE) Error ("{ expected");
        MergeDefaults (m->type, obj->type, obj->name);  /* implicitni hodnoty z deklarace ve tride */
        m->ptype = m->type;           /* vypis: deklarace ve tride (jmena, implicitni hodnoty) */
        m->type = obj->type;          /* parametry s jmeny z definice */
        m->desc = obj->desc;
        m->outdef = true;
        Append (&printlist, PrintMarker (PK_OUTDEF, m, m->type));
        FunctionBody (m);
        return;
     }

     /* globalni funkce: prototyp a pozdejsi definice (nebo dalsi prototyp)
          void f (int a, int b = 0);   ...   void f (int a, int b) { ... }
        pouzije se objekt z prvni deklarace, implicitni hodnoty se prevezmou */
     if (obj->name[0] != 0 && storage != Typedef && CurrentClass == NULL &&
         Display[DisplayLevel] == &gscope && obj->type->sort == Function)
     {
        ObjectPtr prev = Search (&gscope, obj->name);
        if (prev != NULL && prev->fce && !prev->implicit && prev->owner == NULL &&
            prev->type->sort == Function)
        {
           if (inl) prev->Inline = true;
           if (prev->body != NULL)
           {
              /* prototyp za definici - jen implicitni hodnoty */
              if (sy==LBRACE)
                 ErrorId ("Multiple definition of function", obj->name);
              MergeDefaults (obj->type, prev->type, obj->name);
              CheckSymbol (SEMICOLON);
              return;
           }

           MergeDefaults (prev->type, obj->type, obj->name);
           if (sy==LBRACE)            /* vypis: na miste prototypu jen prototyp */
              ReplaceInPrintList (prev, PrintMarker (PK_PROTO, prev, prev->type));
           prev->type = obj->type;    /* parametry s jmeny z posledni deklarace */
           prev->desc = obj->desc;

           if (sy==LBRACE)
           {
              if (! OPT (Body))
                 Error ("Function definition is not allowed here");
              /* definice na miste definice (vypis -p, poradi v glist) */
              RemoveFromQueue (&glist, prev);
              Append (queue, prev);
              FunctionBody (prev);
           }
           else
           {
              CheckSymbol (SEMICOLON);
           }
           return;
        }
     }

     /* globalni promenna:  extern int x;  ...  int x = 1;  (i v opacnem poradi)
        - jeden objekt, definice nahradi deklaraci extern */
     bool skip_enter = false;
     if (obj->name[0] != 0 && storage != Typedef && CurrentClass == NULL &&
         Display[DisplayLevel] == &gscope && obj->type->sort != Function)
     {
        ObjectPtr prev = Search (&gscope, obj->name);
        if (prev != NULL && !prev->fce && prev->storage != Typedef &&
            (prev->storage == Extern || storage == Extern))
        {
           prev->extern_decl = true;
           if (storage == Extern)
              skip_enter = true;              /* deklarace po definici (nebo dalsi extern) */
           else
           {
              /* definice po deklaraci extern */
              ReplaceInPrintList (prev, PrintMarker (PK_PROTO, prev, prev->type));
              RemoveFromQueue (&glist, prev);
              prev->type = obj->type;
              prev->desc = obj->desc;
              prev->storage = storage;
              obj = prev;
              skip_enter = true;
           }
        }
     }

     /* typedef struct X X;  typedef enum X X;  - jmeno X uz je typ (jako v C++) */
     if (obj->name[0] != 0 && storage == Typedef && !skip_enter)
     {
        ObjectPtr prev = Search (Display[DisplayLevel], obj->name);
        if (prev != NULL && IsType (prev) && Resolve (prev->type) == Resolve (obj->type))
           skip_enter = true;
     }

     if (obj->name[0] != 0 && !skip_enter)  /* struct X { ... };  nema jmeno */
        Enter (Display[DisplayLevel], obj);
     else if (skip_enter && storage == Extern)
     {
        /* jen deklarace - do fronty se nepridava */
        if (OPT (Init) && sy==ASSIGN) Error ("extern declaration with initializer");
        if (sy!=COMMA) { CheckSymbol (SEMICOLON); return; }
        NextSymbol ();
        continue;
     }

     Append (queue, obj);
     obj->storage = storage;
     if (storage==Typedef && obj->type->tag == NULL)
     {
        obj->type->tag = obj->name;   /* typedef struct { ... } X;  (ne typedef A B;) */
        obj->type->typedef_tag = true;
     }

     sort = obj->type->sort;
     if (sort==Void)
        Error ("Invalid type");

     obj->fce = storage!=Typedef &&
                (sort==Function || sort==Constructor || sort==Destructor);
     obj->Inline = inl && obj->fce;

     /* metoda tridy (staticka metoda nema this) */
     if (obj->fce && CurrentClass != NULL)
     {
        obj->owner = CurrentClass;
        obj->method = storage != Static;
        if (storage == Static) obj->storage = NoStorage;

        /* virtualni: oznacena virtual, nebo prekryva virtualni metodu baze */
        ObjectPtr inh = CurrentClass->base != NULL ? LookupMember (CurrentClass->base, obj->name) : NULL;
        obj->Virtual = virt || (inh != NULL && inh->fce && inh->Virtual && SameParams (inh->type, obj->type));
        if (virt && !obj->method) Error ("Static method cannot be virtual");

        /* ciste virtualni metoda:  virtual void f () = 0; */
        if (sy==ASSIGN)
        {
           NextSymbol ();
           if (!obj->Virtual || sy!=NUM || strcmp (NumVal, "0") != 0)
              Error ("Pure virtual method expected:  virtual ... = 0");
           NextSymbol ();
           obj->pure = true;
        }
     }
     else if (virt)
        Error ("virtual is allowed only for methods");

     if (obj->fce) break;

     /* for (T x : seq) - deklarace promenne cyklu konci pred : */
     if (OPT (RangeFor) && sy==COLON)
     {
        RangeForDecl = true;
        return;
     }

     if (OPT (Width) && sy==COLON)
     {
        NextSymbol ();
        obj->width = const_expr ();
     }

     if (OPT (Init) && sy==LPAR && IsStdString (obj->type))
     {
        /* string s (p, n);  string s (n, c);  string s (x); */
        NextSymbol ();
        obj->init = StringConstruct (expression ());
        CheckSymbol (RPAR);
     }
     else if (OPT (Init) && sy==ASSIGN)
     {
        NextSymbol ();
        obj->init = initializer ();
        ArraySizeFromInit (obj);
        if (obj->init->type != NULL && obj->init->type->sort == Function)
           CheckAssign (obj->type, obj->init->type);    /* int (*f) (int) = g;  (ne int k = g;) */
     }
     else if (CurrentClass != NULL && storage != Static && storage != Typedef && sy==ASSIGN)
     {
        /* polozka s inicializaci ve tride:  bool x = false;  - prirazeni na zacatku
           kazdeho konstruktoru, ktery ji neinicializuje (MemberInits) */
        NextSymbol ();
        obj->member_init = simple_expr ();
     }
     else if (CurrentClass != NULL && storage == Static)
     {
        /* staticka polozka tridy:  static const int max = 4 * 1024; */
        obj->owner = CurrentClass;
        if (sy==ASSIGN)
        {
           NextSymbol ();
           obj->init = const_expr ();
        }
     }

     if (OPT (CondInit) && sy==RPAR) return;   /* if (T x = e) - ) precte volajici */
     if (sy!=COMMA) break;
     NextSymbol ();
  }

  if (obj->fce && sy==LBRACE)
  {
     if (OPT (Body) && obj->owner != NULL && obj->owner == CurrentClass && DeferTail != NULL)
        DeferBody (obj, false);           /* telo metody az na konci tridy */
     else if (OPT (Body))
        FunctionBody (obj);
     else
        Error ("Function definition is not allowed here");
  }
  else
  {
     CheckSymbol (SEMICOLON);
  }
}

/****************************** CONSTRUCTOR *******************************/

void ConstructorDecl (TypePtr cls)
/* konstruktor struktury 'cls', symbol ( je na vstupu
     Item ();                                  deklarace
     Item () : key (0), left (nullptr) { ... }  definice */
{
  TypePtr typ = Link (Function, &VoidDef);
  CheckSymbol (LPAR);
  ParameterList (typ);

  ObjectPtr obj = NewObject (cls->tag, typ);
  obj->fce = true;
  obj->owner = cls;

  if (sy==SEMICOLON)
  {
     NextSymbol ();
     if (cls->ctor == NULL) cls->ctor = obj;
     return;
  }

  if (cls->ctor != NULL && (cls->ctor->body != NULL || cls->ctor->deferred))
     ErrorId ("Multiple definition of constructor", cls->tag);

  /* deklarace ve tride, definice mimo tridu - implicitni hodnoty z deklarace */
  if (cls->ctor != NULL)
     MergeDefaults (cls->ctor->type, typ, cls->tag);

  /* konstruktor uvnitr tridy: inicializace a telo az na konci tridy */
  if (cls == CurrentClass && DeferTail != NULL)
  {
     cls->ctor = obj;
     DeferBody (obj, true);
     return;
  }

  if (cls != CurrentClass)
  {
     /* definice mimo tridu - vypis: ve tride deklarace, definice na tomto miste */
     obj->outdef = true;
     obj->ptype = (cls->ctor != NULL) ? cls->ctor->type : typ;
     Append (&printlist, PrintMarker (PK_OUTDEF, obj, typ));
  }
  ConstructorBody (cls, obj);
}

/* prirazeni polozek s inicializaci ve tride (bool x = false;), ktere nejsou
   v inicializacnim seznamu konstruktoru (done, n) - pripoji se za *last */
static void MemberInits (TypePtr cls, ObjectPtr * done, int n, StatPtr * first, StatPtr * last)
{
  for (NodePtr p = cls->list.first; p != NULL; p = p->next)
  {
     ObjectPtr f = p->item;
     if (f->member_init == NULL) continue;
     bool skip = false;
     for (int i = 0; i < n; i++)
        if (done [i] == f) skip = true;
     if (skip) continue;
     ExprPtr m = NEW (Expr);
     m->op = IDENT;
     m->id = f->name;
     m->obj = f;
     m->type = f->type;
     StatPtr s = NEW (Stat);
     s->st = SIM;
     s->e1 = calc (m, f->member_init, ASSIGN);
     if (*first == NULL) *first = s; else (*last)->next = s;
     *last = s;
  }
}

/* trida bez konstruktoru s polozkami s inicializaci ve tride - implicitni T () { } */
static void ImplicitInitCtor (TypePtr cls)
{
  bool any = false;
  for (NodePtr p = cls->list.first; p != NULL; p = p->next)
     if (p->item->member_init != NULL) any = true;
  if (!any || cls->ctor != NULL) return;
  ObjectPtr obj = NewObject (cls->tag, Link (Function, &VoidDef));
  obj->fce = true;
  obj->owner = cls;
  StatPtr first = NULL, last = NULL;
  MemberInits (cls, NULL, 0, &first, &last);
  StatPtr s = NEW (Stat);
  s->st = LBRACE;
  s->s1 = first;
  obj->body = s;
  cls->ctor = obj;
  Append (&ctorlist, obj);
}

void ConstructorBody (TypePtr cls, ObjectPtr obj)
/* inicializace polozek a telo konstruktoru, na vstupu je : nebo { */
{
  TypePtr typ = obj->type;

  OpenScope (&typ->scope);     /* parametry */
  Fce = obj;
  TypePtr save_method = MethodClass;
  MethodClass = cls;           /* this v konstruktoru */

  /* inicializace polozek  : key (0), left (nullptr)  =>  prirazeni na zacatku tela */
  StatPtr first = NULL, last = NULL;
  ObjectPtr done [256];   /* polozky z inicializacniho seznamu (MemberInits) */
  int ndone = 0;
  if (sy==COLON)
  {
     NextSymbol ();
     for (;;)
     {
        if (sy!=IDENT) Error ("Member name expected");

        /* konstruktor baze:  : Base (args) */
        if (cls->base != NULL && cls->base->tag != NULL && strcmp (IdentVal, cls->base->tag) == 0)
        {
           NextSymbol ();
           CheckSymbol (LPAR);
           ExprPtr args = (sy==RPAR) ? NULL : expression ();
           CheckSymbol (RPAR);
           if (cls->base->ctor != NULL)
              CheckCall (cls->base->ctor->type, args);
           else if (args != NULL)
              ErrorId ("Constructor with arguments is not defined", cls->base->tag);
           obj->base_init = true;
           obj->base_args = args;
           if (sy!=COMMA) break;
           NextSymbol ();
           continue;
        }

        ObjectPtr member = FindMember (cls, IdentVal);
        if (ndone < 256) done [ndone ++] = member;
        ExprPtr m = NEW (Expr);
        m->op = IDENT;
        m->id = member->name;
        m->obj = member;
        m->type = member->type;
        NextSymbol ();

        CheckSymbol (LPAR);
        if (sy==RPAR)
        {
           /* buf ()  - inicializace hodnotou: skalar => 0, pole a objekty
              se nemeni (new => calloc, pamet je vynulovana) */
           NextSymbol ();
           TypePtr mt = member->type;
           if (mt->sort == Array || IsClsSort (mt->sort) || mt->sort == StdString ||
               mt->sort == StdSequence || mt->sort == StdDictionary)
           {
              if (sy!=COMMA) break;
              NextSymbol ();
              continue;
           }
           ExprPtr zero = NEW (Expr);
           zero->op = (mt->sort == Pointer) ? NULLPTR_ : NUM;
           zero->id = NewStr ((char *) "0");
           zero->type = (mt->sort == Pointer) ? &NullPtrDef : &IntDef;
           StatPtr s = NEW (Stat);
           s->st = SIM;
           s->e1 = calc (m, zero, ASSIGN);
           if (first == NULL) first = s; else last->next = s;
           last = s;
           if (sy!=COMMA) break;
           NextSymbol ();
           continue;
        }
        ExprPtr v = expression ();
        CheckSymbol (RPAR);

        StatPtr s = NEW (Stat);
        s->st = SIM;
        s->e1 = calc (m, v, ASSIGN);
        if (first == NULL) first = s; else last->next = s;
        last = s;

        if (sy!=COMMA) break;
        NextSymbol ();
     }
  }

  MemberInits (cls, done, ndone, &first, &last);   /* bool x = false; ve tride */

  if (sy!=LBRACE) Error ("{ expected");
  StatPtr body = statement ();
  if (sy==SEMICOLON) NextSymbol ();

  Fce = NULL;
  MethodClass = save_method;
  CloseScope ();

  /* { inicializace ... telo } */
  StatPtr s = NEW (Stat);
  s->st = LBRACE;
  if (first == NULL) first = body; else last->next = body;
  s->s1 = first;

  obj->body = s;
  cls->ctor = obj;
  Append (&ctorlist, obj);
}

/******************************* DESTRUCTOR *******************************/

void DestructorDecl (TypePtr cls, bool virt)
/* destruktor tridy 'cls', symbol ~ je na vstupu
     ~T ();   virtual ~T ();   virtual ~T () = 0;   ~T () { ... }   (ve tride)
     T::~T () { ... }                                                (mimo tridu)
   Destruktor je metoda se jmenem "~" (stejne jmeno ve vsech tridach -
   prekryti virtualniho destruktoru baze), ulozena v cls->dtor a mezi
   polozkami tridy. */
{
  CheckSymbol (TILDA);
  if (sy!=IDENT || cls->tag == NULL || strcmp (IdentVal, cls->tag) != 0)
     Error ("Destructor name expected");
  NextSymbol ();
  CheckSymbol (LPAR);
  if (sy==VOID_) NextSymbol ();
  CheckSymbol (RPAR);

  if (cls != CurrentClass)
  {
     /* definice mimo tridu */
     ObjectPtr d = cls->dtor;
     if (d == NULL)
        ErrorId ("Destructor is not declared in class", cls->tag);
     if (d->body != NULL || d->deferred)
        ErrorId ("Multiple definition of destructor", cls->tag);
     if (sy!=LBRACE) Error ("{ expected");
     d->outdef = true;
     Append (&printlist, PrintMarker (PK_OUTDEF, d, d->type));
     FunctionBody (d);
     return;
  }

  if (cls->dtor != NULL)
     ErrorId ("Multiple declaration of destructor", cls->tag);

  TypePtr typ = Link (Function, &VoidDef);
  ObjectPtr obj = NewObject ((char *) "~", typ);
  obj->fce = true;
  obj->method = true;
  obj->owner = cls;

  /* virtualni: oznaceny virtual, nebo baze ma virtualni destruktor */
  ObjectPtr inh = (cls->base != NULL) ? LookupMember (cls->base, (char *) "~") : NULL;
  obj->Virtual = virt || (inh != NULL && inh->fce && inh->Virtual);

  cls->dtor = obj;
  Enter (&cls->scope, obj);
  Append (&cls->list, obj);

  if (sy==ASSIGN)                       /* virtual ~T () = 0; */
  {
     NextSymbol ();
     if (!obj->Virtual || sy!=NUM || strcmp (NumVal, "0") != 0)
        Error ("Pure virtual destructor expected:  virtual ~T () = 0");
     NextSymbol ();
     obj->pure = true;
  }

  if (sy==SEMICOLON)
  {
     NextSymbol ();
     return;
  }

  if (sy!=LBRACE) Error ("{ expected");
  if (DeferTail != NULL)
     DeferBody (obj, false);            /* telo az na konci tridy */
  else
     FunctionBody (obj);
}

/************************* GLOBAL DECLARATION **************************/

void GlobalDecl (void)
{
  Queue queue;

  /* using namespace std;  - jmenne prostory se ignoruji */
  if (sy==IDENT && strcmp (IdentVal, "using") == 0)
  {
     while (sy!=SEMICOLON && sy!=EOS) NextSymbol ();
     CheckSymbol (SEMICOLON);
     return;
  }

  /* extern "C" { ... },  extern "C" deklarace;  - vazba se nerozlisuje */
  if (sy==EXTERN_ && PeekSymbol () == STR)
  {
     NextSymbol ();                      /* extern */
     NextSymbol ();                      /* "C" */
     if (sy==LBRACE)
     {
        NextSymbol ();
        while (sy!=RBRACE && sy!=EOS) GlobalDecl ();
        CheckSymbol (RBRACE);
        return;
     }
  }

  /* namespace X { ... } - deklarace globalne, X:: se vynechava (NamespaceQual) */
  if (sy==IDENT && strcmp (IdentVal, "namespace") == 0)
  {
     NextSymbol ();
     if (sy!=IDENT) Error ("Identifier expected");
     if (!IsNamespaceName (IdentVal))
     {
        if (NamespaceCnt >= 64) Error ("Too many namespaces");
        Namespaces [NamespaceCnt ++] = NewStr (IdentVal);
     }
     NextSymbol ();
     CheckSymbol (LBRACE);
     while (sy!=RBRACE && sy!=EOS) GlobalDecl ();
     CheckSymbol (RBRACE);
     return;
  }

  /* read declaration */
  declaration (&queue, Body | Init);
  for (NodePtr p = queue.first; p != NULL; p = p->next)
     Append (&printlist, p->item);
  Join (&glist, &queue);
}

/* znacka v printlist */
static ObjectPtr PrintMarker (int kind, ObjectPtr ref, TypePtr type)
{
  ObjectPtr m = NewObject (ref->name, type);
  m->pkind = kind;
  m->pref = ref;
  m->ptype = type;
  m->desc = ref->desc;          /* typ s aliasy (typedef) v okamziku deklarace */
  return m;
}

static void ReplaceInPrintList (ObjectPtr old_item, ObjectPtr new_item)
{
  for (NodePtr p = printlist.first; p != NULL; p = p->next)
     if (p->item == old_item)
     {
        p->item = new_item;
        return;
     }
}

/******************************** PUT DECL ********************************/

enum mode_typ { header, full, param };
#ifndef __cplusplus
typedef enum mode_typ mode_typ;
#endif
void SubDecl (ObjectPtr obj, mode_typ mode);

void Separator (TypePtr t, bool * ptr, bool * mark)
{
  *mark = false;

  switch (t->sort)
  {
     case Pointer:
     case Reference:
        *ptr = true;
        break;

     case Array:
     case Function:
        if (*ptr) *mark = true;
        *ptr = false;
        break;
  }
}

bool spc = false;

void PutSpc (char *s)
{
  if (spc) PutChr (' ');
  PutStr (s);
  spc = true;
}

void Postfix (TypePtr t, TypePtr desc, bool ptr);

static void PutBaseInit (ObjectPtr ctor);

bool PutTypeNameOnly = false;   /* tisk typu ve vyrazu (new T, (T) e, sizeof (T)): jen jmeno tridy */

void Prefix (TypePtr t, TypePtr desc, bool ptr)
{
  bool mark;
  NodePtr p;

  Check (t!=NULL);
  if (desc!=NULL) t=desc;

  if (PutTypeNameOnly && t->tag != NULL &&
      (IsClsSort (t->sort) || t->sort == Enum))
  {
     if (t->Const) PutSpc ("const");
     PutSpc (t->tag);
     return;
  }

  if (t->from!=NULL && t->sort!=Name && t->sort!=Tag)
  {
     Separator (t, &ptr, &mark);
     Prefix (t->from, t->desc, ptr);
     if (mark) PutSpc ("(");
  }

  if (t->Const) PutSpc ("const");
  if (t->Volatile) PutSpc ("volatile");

  switch (t->sign)
  {
     case Signed:   PutSpc ("signed"); break;
     case Unsigned: PutSpc ("unsigned"); break;
  }

  switch (t->length)
  {
     case Short: PutSpc ("short"); break;
     case Long:  PutSpc ("long");  break;
  }

  switch (t->sort)
  {
     case Char:   PutSpc ("char");   break;
     case Bool:   PutSpc ("bool");   break;
     case Int:    PutSpc ("int");    break;
     case Float:  PutSpc ("float");  break;
     case Double: PutSpc ("double"); break;
     case Void:   PutSpc ("void");   break;
     case StdString: PutSpc ("string"); break;   /* std::string */

     case StdSequence:                            /* Sequence <T> */
     case StdDictionary:                          /* Dictionary <K, V> */
        PutSpc (t->sort == StdSequence ? "Sequence <" : "Dictionary <");
        if (t->key != NULL)
        {
           PutTypeName (t->key);
           PutStr (", ");
        }
        PutTypeName (t->elem);
        PutStr (">");
        spc = true;
        break;

     case Pointer:
        PutSpc ("*");
        break;

     case Reference:
        PutSpc ("&");
        break;

     case Struct:
     case Union:
     case Class:

        if (t->sort==Struct) PutSpc ("struct");
        else if (t->sort==Union) PutSpc ("union");
        else PutSpc ("class");

        if (t->tag!=NULL)
        {
           PutChr (' ');
           PutStr (t->tag);
        }

        if (t->base!=NULL && t->base->tag!=NULL)   /* bazova trida */
        {
           PutStr (" : public ");
           PutStr (t->base->tag);
        }

        PutEol ();

        PutChr ('{');
        PutEol ();
        if (t->sort==Class)
        {
           PutStr ("public:");     /* pristup se nekontroluje - vse verejne */
           PutEol ();
        }
        Indent ();

        for (p=t->list.first; p!=NULL; p=p->next)
        {
           Check (p->item!=NULL);
           /* metody s telem uvnitr tridy, definovane mimo tridu jen deklarace */
           SubDecl (p->item, (p->item->fce && p->item->body != NULL && !p->item->outdef) ? full : header);
        }

        if (t->ctor != NULL && t->ctor->outdef)   /* konstruktor definovany mimo tridu */
        {
           PutStr (t->ctor->name);
           Postfix (t->ctor->ptype, NULL, false);
           PutChr (';');
           PutEol ();
        }
        else if (t->ctor != NULL)     /* konstruktor */
        {
           PutStr (t->ctor->name);
           Postfix (t->ctor->type, t->ctor->desc, false);
           if (t->ctor->body != NULL) PutBaseInit (t->ctor);
           if (t->ctor->body == NULL)
           {
              PutChr (';');
              PutEol ();
           }
           else
           {
              PutEol ();
              PutStat (t->ctor->body);
           }
        }

        Unindent ();
        PutChr ('}');
        PutChr (' ');
        break;

     case Enum:
        PutSpc ("enum");
        if (t->tag)
        {
           PutChr (' ');
           PutStr (t->tag);
        }

        PutEol ();
        PutChr ('{');
        PutEol ();
        Indent ();

        for (p=t->list.first; p!=NULL; p=p->next)
        {
           PutStr (p->item->name);
           if (p->item->init!=NULL)        /* c = 10 */
           {
              PutStr (" = ");
              PutExpr (p->item->init);
           }
           if (p->next!=NULL) PutStr (", ");
           PutEol ();
        }

        Unindent ();
        PutChr ('}');
        PutChr (' ');
        break;

     case Name:
        PutSpc (t->tag);
        break;

     case Tag:
        switch (t->from->sort)
        {
           case Struct: PutSpc ("struct"); break;
           case Union:  PutSpc ("union"); break;
           case Enum:   PutSpc ("enum"); break;
           case Class:  PutSpc ("class"); break;
           default:     Bug ();
        }
        PutSpc (t->tag);
        break;
  }
}

void Postfix (TypePtr t, TypePtr desc, bool ptr)
{
  bool mark;
  NodePtr p;

  Check (t!=NULL);
  if (desc!=NULL)t=desc;

  if (t->sort==Name || t->sort==Tag) return;

  if (t->from!=NULL)
  {
     Separator (t, &ptr, &mark);
     if (mark) PutChr (')');
  }

  switch (t->sort)
  {
     case Array:
        PutStr (" [");
        if (t->range!=NULL) PutExpr (t->range);
        PutStr ("]");
        break;

     case Function:
     case Constructor:
     case Destructor:
        PutStr (" (");
        for (p=t->list.first; p!=NULL; p=p->next)
        {
           SubDecl (p->item, param);
           if (p->next!=NULL) PutStr (", ");
        }
        PutStr (")");
        break;
  }

  if (t->from!=NULL)
  {
     Postfix (t->from, t->desc, ptr);
  }
}

void PutStorage (storage_typ storage, mode_typ mode)
{
  switch (storage)
  {
     case Typedef:  PutStr ("typedef "); break;
     case Auto:     PutStr ("auto "); break;
     case Register_: PutStr ("register "); break;
     case Static:   PutStr ("static "); break;
     case Extern:   if (mode!=full) PutStr ("extern "); break;
  }
}

static void PutBaseInit (ObjectPtr ctor)
/* konstruktor:  : Base (args) */
{
  if (ctor != NULL && ctor->base_init && ctor->owner != NULL && ctor->owner->base != NULL)
  {
     PutStr (" : ");
     PutStr (ctor->owner->base->tag);
     PutStr (" (");
     if (ctor->base_args != NULL) PutExpr (ctor->base_args);
     PutChr (')');
  }
}

static TypePtr PutQualifier = NULL;   /* definice mimo tridu:  int T::f (...) */
static bool PutSkipMerged = false;    /* definice: implicitni hodnoty jen v deklaraci */

void SubDecl (ObjectPtr obj, mode_typ mode)
{
  if (obj->type == NULL)            /* parametr ... */
  {
     PutStr (obj->name);
     return;
  }
  if (obj->Inline) PutStr ("inline ");
  if (PutQualifier == NULL)
  {
     if (obj->fce && obj->Virtual) PutStr ("virtual ");
     if (obj->fce && obj->owner != NULL && !obj->method) PutStr ("static ");
     if (!obj->fce && obj->owner != NULL && obj->storage == Static) PutStr ("static ");
  }
  /* funkce bez tela na konci prekladu = knihovni funkce (glibc) */
  if (mode==full && obj->fce && obj->body==NULL && obj->owner==NULL && obj->storage!=Static)
     PutStr ("extern \"C\" ");
  if (! (obj->owner != NULL && obj->storage == Static))
     PutStorage (obj->storage, mode);

  spc = false;
  bool dtor = obj->fce && obj->owner != NULL && obj->owner->dtor == obj;
  if (dtor)
  {
     /* destruktor:  ~T ()  /  T::~T ()  - bez typu vysledku */
     if (PutQualifier != NULL)
     {
        PutSpc (PutQualifier->tag);
        PutStr ("::");
     }
     else
        PutSpc ("");
     PutChr ('~');
     PutStr (obj->owner->tag);
  }
  else
  {
     Prefix (obj->type, obj->desc, false);
     if (PutQualifier != NULL && obj->fce)
     {
        PutSpc (PutQualifier->tag);
        PutStr ("::");
        PutStr (obj->name);
     }
     else
        PutSpc (obj->name);
  }
  TypePtr save_qual = PutQualifier;
  bool save_skip = PutSkipMerged;
  PutQualifier = NULL;
  PutSkipMerged = (mode == full && obj->fce);
  Postfix (obj->type, obj->desc, false);
  PutQualifier = save_qual;
  PutSkipMerged = save_skip;

  if (obj->pure) PutStr (" = 0");

  if (obj->width!=NULL)
  {
     PutChr (':');
     PutExpr (obj->width);
  }

  if (obj->init!=NULL && ! (mode==param && PutSkipMerged && obj->merged))
  {
     PutChr ('=');
     PutExpr (obj->init);
  }

  if (mode==header)
  {
     PutChr (';');
     PutEol ();
  }

  if (mode==full)
  {
     if (obj->fce)
     {
        if (obj->body==NULL)
           PutChr (';');              /* deklarace funkce (prototyp) */
        PutEol ();
        if (obj->body!=NULL)
           PutStat (obj->body);
     }
     else
     {
        PutChr (';');
        PutEol ();
     }
  }
}

void PutDecl (ObjectPtr obj)
{
  SubDecl (obj, full);
}

void PutParamDecl (ObjectPtr obj)
{
  SubDecl (obj, param);
}

void PutTypeName (TypePtr type)
/* typ ve vyrazu:  new T,  static_cast <T *>,  sizeof (T) */
{
  bool save = PutTypeNameOnly;
  PutTypeNameOnly = true;
  PutParamDecl (NewObject ((char *) "", type));
  PutTypeNameOnly = save;
}

void PutHeader (ObjectPtr obj)
{
  SubDecl (obj, header);
}

static bool HasImplicit (ObjectPtr root)
/* implicitne deklarovana funkce v globalnim stromu identifikatoru */
{
  if (root == NULL) return false;
  return root->implicit || HasImplicit (root->left) || HasImplicit (root->right);
}

/* knihovni funkce C, ktere ma C++ v <cstring> pretizene (const char * strchr
   (const char *, int) a char * strchr (char *, int)) - prototyp extern "C"
   by s nimi koliduje; ve vypisu -p je deklaruje #include <cstring> */
static bool CppOverloaded (ObjectPtr obj)
{
  static const char * names [] = { "strchr", "strrchr", "strstr", "strpbrk", "memchr", NULL };
  if (! obj->fce || obj->body != NULL || obj->owner != NULL) return false;
  for (int i = 0; names [i] != NULL; i++)
     if (strcmp (obj->name, names [i]) == 0) return true;
  return false;
}

void PutGlobalDecl (void)
{
  /* volane nedeklarovane funkce (printf, rand, ...) - hlavicky jazyka C */
  if (HasImplicit (gscope.root))
  {
     PutStr ("#include <cstdio>"); PutEol ();
     PutStr ("#include <cstdlib>"); PutEol ();
     PutStr ("#include <ctime>"); PutEol ();
  }
  PutStr ("#include <cstring>"); PutEol ();   /* strchr, strstr, ... - viz CppOverloaded */

  /* std::string a assert jsou vestavene - pro g++ hlavicky */
  if (UsesContainers)       /* Sequence, Dictionary - sablony pro g++ (cmini/ownheaders/containers.h) */
  {
     PutStr ("#include \"containers.h\""); PutEol ();
  }
  PutStr ("#include <cassert>"); PutEol ();
  PutStr ("#include <string>"); PutEol ();
  PutStr ("using namespace std;"); PutEol ();
  PutEol ();

  NodePtr p;
  for (p=printlist.first; p!=NULL; p=p->next)
  {
     ObjectPtr obj = p->item;
     if (obj->pkind == PK_PROTO)
     {
        /* prototyp funkce (definice je dale), deklarace extern promenne */
        Object tmp;
        tmp = *obj->pref;   /* cmini: inicializace struktury jen { ... } */
        tmp.type = obj->ptype;
        tmp.desc = obj->desc;
        tmp.body = NULL;
        tmp.init = NULL;
        if (!tmp.fce) tmp.storage = Extern;
        SubDecl (&tmp, header);
     }
     else if (obj->pkind == PK_OUTDEF)
     {
        /* metoda nebo konstruktor definovany mimo tridu:  int T::f (...) { ... } */
        ObjectPtr m = obj->pref;
        if (m->owner != NULL && m->owner->ctor == m)
        {
           PutStr (m->owner->tag);
           PutStr ("::");
           PutStr (m->name);
           bool save_skip = PutSkipMerged;
           PutSkipMerged = true;
           Postfix (m->type, NULL, false);
           PutSkipMerged = save_skip;
           PutBaseInit (m);
           PutEol ();
           PutStat (m->body);
        }
        else
        {
           PutQualifier = m->owner;
           SubDecl (m, full);
           PutQualifier = NULL;
        }
     }
     else if (! CppOverloaded (obj))
        PutDecl (obj);
  }
}

void PutGlobalHeader (void)
{
  NodePtr p;
  for (p=glist.first; p!=NULL; p=p->next)
     PutHeader (p->item);
}
