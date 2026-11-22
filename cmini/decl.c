
/* DECL.C */

#include "decl.h"
#include <string.h>
#include <stdio.h>    /* snprintf */

Scope     gscope;
Queue     glist;
Queue     printlist;   /* order for printing -p: objects as in the text + markers */
Queue     ctorlist;     /* definition of constructors */
Queue     methodlist;   /* definitions of methods (with body) */
Queue     classlist;    /* defined structures and classes */
TypePtr   MethodClass = NULL;   /* class of the method or constructor being translated (this) */

static TypePtr QualClass = NULL; /* T::f - class from the qualified name in the declarator */

/*************************** RECORDED SYMBOLS *****************************/

/* The bodies of methods and constructors inside a class may use members declared
   only after them. Therefore, when the class is read, the symbols of the body are only recorded
   and translated (replayed via ReplaySymbol) only at the end of the class. */

void (*ReplaySymbol) (void) = NULL;

typedef struct
{
  SYMBOL sy;
  char * text;      /* IdentVal, NumVal or StrVal */
  char   chr;
  bool   l, u, f;
  int    line, col;
  const char * file;  /* included file (preprocessor) or NULL */
  int    fline;       /* line in the included file */
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
/* current symbol => t */
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
/* t => current symbol */
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
/* record the symbols from the current one (: or {) up to the corresponding } inclusive */
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

typedef struct REPLAY_REC REPLAY;   /* in advance - pointer to itself (cmm does not know struct X * in a member) */
struct REPLAY_REC
{
  TOKENLIST *         list;
  int                 pos;
  TOKEN               saved;   /* current symbol before replaying */
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

  /* end of the record: restore the symbol that was current before replaying */
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
  ReplayNext ();              /* first symbol of the record */
}

/* peek at the following symbol (the current symbol does not change) */
static TOKEN PeekNext;
static void (*PeekSaved) (void) = NULL;

static void PeekReplay (void)
{
  ReplaySymbol = PeekSaved;     /* original source of the symbol (input or record) */
  LoadToken (&PeekNext);
}

/* namespaces:  namespace X { ... }  - declarations are global, the qualification X::
   is skipped when reading symbols (NamespaceQual, called by the lexers) */
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
  NextSymbol ();                           /* name (also a nested namespace) */
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
  ReplaySymbol = PeekReplay;    /* the next NextSymbol returns PeekNext */
  LoadToken (&cur);
  return result;
}

bool IsType (ObjectPtr obj);
extern bool DclSym [];

static bool PeekIsTypeStart (void)
/* symbol after the current one is ) or starts a type => function declaration,
   otherwise  string s (p, n)  - direct initialization */
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
/* T::f (...),  T::x  - class name and :: at the start of an expression (not a declaration) */
{
  if (sy!=IDENT) return false;
  ObjectPtr obj = Lookup (IdentVal);
  if (obj == NULL || !IsType (obj)) return false;
  TypePtr t = Resolve (obj->type);
  if (t == NULL || !IsClsSort (t->sort)) return false;
  return PeekSymbol () == DCOLON;
}

/* postponed bodies of methods and constructors of the class just being read */
typedef struct DEFERRED_REC DEFERRED;
struct DEFERRED_REC
{
  ObjectPtr             obj;
  bool                  ctor;
  TOKENLIST             tokens;
  DEFERRED *            next;
};

static DEFERRED * * DeferTail = NULL;   /* != NULL => bodies are postponed */

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

/* same parameters (override of a virtual method); different - the method only hides
   the virtual method of the base (C++: void f (int, bool = false) in the derived class) */
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
/* Add the elements of queue 'add' to queue 'dst' */
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
/* Insert identifier into the tree */
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
/* Insert identifier into the tree */
{
  EnterPrim (&scope->root, add);
}

/* ---------------------------------------------------------------------- */

ObjectPtr SearchPrim (ObjectPtr root, IDENT_TYP name)
/* Search the given identifier in the tree */
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
/* Search the given identifier in the tree */
{
   return SearchPrim (scope->root, name);
}

/* ---------------------------------------------------------------------- */

ObjectPtr Lookup (IDENT_TYP name)
/* Search the given identifier in all trees */
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
/* Search the given identifier. Report an error if it was not found. */
{
  ObjectPtr obj;

  obj = Lookup (name);
  if (obj==NULL) ErrorId ("Unknown identifier", name);
  return (obj);
}

/* ---------------------------------------------------------------------- */

ObjectPtr LookupMember (TypePtr typ, IDENT_TYP name)
/* Search the structure member, then in the base classes */
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
/* obj is a member (or method) of structure typ or of its base classes */
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
/* make the members of the class and its base classes accessible (base first),
   returns the number of opened scopes */
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
/* Search the structure member. Report an error if it was not found. */
{
  ObjectPtr obj;

  obj = LookupMember (typ, name);
  if (obj==NULL)
     ErrorId ("Member of structure expected", name);
  return (obj);
}

/************************ CREATE TYPES AND OBJECTS ************************/

TypePtr NewType (sort_typ sort)
/* Create a new type, set the sort field */
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
/* Create an object of the given name and type */
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
     if (obj==NULL && !def)
     {
        /* struct X v;  struct X * p;  in a function or in another structure: X declared
           in an enclosing scope (otherwise a new empty local type X hid it and its
           members were not found - tests/examples/structtag.cc) */
        ObjectPtr outer = Lookup (id);
        if (outer!=NULL && IsType (outer) && outer->storage!=Typedef && outer->type->sort==sort)
           obj = outer;
     }
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

     /* enumerator_list (empty enumeration enum X { } - the grammar generates it) */
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

TypePtr CurrentClass = NULL;   /* structure being processed */

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

  /* base class:  class B : public A { ... } */
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
     if (typ->base != NULL) scopes = OpenClassScopes (typ->base);  /* members of the base */
     OpenScope (&typ->scope);
     TypePtr save_class = CurrentClass;
     CurrentClass = typ;

     DEFERRED * deferred = NULL;
     DEFERRED * * save_tail = DeferTail;
     DeferTail = &deferred;

     while (sy!=RBRACE)
     {
        /* public:  private:  protected:  (access is not checked) */
        if (sy==PUBLIC_ || sy==PRIVATE_ || sy==PROTECTED_)
        {
           NextSymbol ();
           CheckSymbol (COLON);
           continue;
        }
        /* friend class X;  friend int f (...);  - access is not checked => skip */
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

     /* bodies of methods and constructors - once all members of the class are known */
     DeferTail = save_tail;
     for (DEFERRED * d = deferred; d != NULL; d = d->next)
     {
        StartReplay (&d->tokens);
        if (d->ctor)
           ConstructorBody (typ, d->obj);
        else
           FunctionBody (d->obj);
        if (sy!=RBRACE) Bug ();             /* the record must be fully processed */
     }
     NextSymbol ();
     ImplicitInitCtor (typ);   /* members with initialization in the class, without constructor */

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
        NextSymbol ();                 /* long long = long (64 bits, x86-64) */
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
  /* name of the structure (tag), not a variable of type structure:
     CreateType sets typ->tag = obj->name */
  if ((IsClsSort (sort) || sort==Enum) && obj->type->tag == obj->name) return true;
  return false;
}

/* Sequence <T>, Dictionary <K, V>  (unless declared otherwise) */
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
/* parameter of a type:  int,  GrmRule *,  string  (element: value in an 8-byte slot) */
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
  /* element struct without strings, containers and destructor (copy byte by byte):
     Sequence <DtorVar> (sources of cmm) */
  if (!key && IsClsSort (r->sort) && r->defined && !NeedsDtor (r, true) && !IsPolymorphic (r))
     return o->type;
  Error (key ? "Dictionary key must be integer, enum, pointer or string"
             : "Sequence / Dictionary element must be number, pointer or string");
  return NULL;
}

static void CloseAngle (void)
/* > at the end of the type parameter;  >> (nested types) is split into two symbols > */
{
  if (sy==GREATER) NextSymbol ();
  else if (sy==SHR) sy = GREATER;        /* the first > is processed, the second remains */
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

/* std::string:  string  (unless declared otherwise)  or  std::string */
static bool IsStringName (void)
{
  return sy==IDENT && (strcmp (IdentVal, "string") == 0 || strcmp (IdentVal, "std") == 0) &&
         Lookup (IdentVal) == NULL;
}

TypePtr declaration_specifiers (void)
{
  TypePtr typ;
  ObjectPtr obj;

  /* const before the type name:  const string & s,  const T * p */
  bool cnst = false;
  while (sy==CONST_ || sy==VOLATILE_)
  {
     if (sy==CONST_) cnst = true;
     NextSymbol ();
     if (sy!=IDENT && sy!=STRUCT_ && sy!=UNION_ && sy!=CLASS_ && sy!=ENUM_)
     {
        /* const int ... - the rest is read by sim_declaration_specifiers */
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
/* list of parameters of the function of type 'typ', symbol ( already read, read including ) */
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

        /* parameter of array type is a pointer:  char * argv []  =>  char * * argv */
        if (pobj->type != NULL && pobj->type->sort == Array)
        {
           pobj->desc = Link (Pointer, pobj->type->desc);
           pobj->type = Resolve (pobj->desc);
        }

        /* default value:  int level = 0 */
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
        if (pobj->type != NULL && pobj->type->sort==Void)   /* f (void), not f (void * p) */
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

     /* method defined outside the class:  int T::f (...) { ... } */
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
        /* a [N] [M] = array of N elements, each is an array of M elements
           => the last dimension must be the innermost type */
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
        break;      /* local  string s (p, n);  - initialization, not a function declaration */

     else if (sy==LPAR)
     {
        NextSymbol ();
        typ = Link (Function, typ);
        /* int T::f (TType * p) - type names from class T are visible in the parameters */
        int qscopes = (QualClass != NULL) ? OpenClassScopes (QualClass) : 0;
        ParameterList (typ);
        CloseScopes (qscopes);

        /* method:  int f () const   void f () override */
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
  /* method: members of the class (and base classes) are visible, this */
  int scopes = 0;
  TypePtr save_method = MethodClass;
  if (object->owner != NULL)
  {
     if (object->owner != CurrentClass)
        scopes = OpenClassScopes (object->owner);   /* definition outside the class */
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

  /* extern "C" int main (...) - only the linkage, not extern */
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
/* array without size:  int a [] = { 1, 2, 3 };  char s [] = "abc"; */
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

  /* new array type (the type may be shared with other declarations) */
  TypePtr a = NewType (Array);
  *a = *t;
  a->range = r;
  obj->type = a;
  if (obj->desc == t) obj->desc = a;
}

static void RemoveFromQueue (QueuePtr queue, ObjectPtr obj)
/* remove object from queue (not found => nothing) */
{
  NodePtr p = queue->first;
  while (p != NULL && p->item != obj)
     p = p->next;
  if (p == NULL) return;

  if (p->prev == NULL) queue->first = p->next; else p->prev->next = p->next;
  if (p->next == NULL) queue->last = p->prev;  else p->next->prev = p->prev;
}

static NodePtr NextParam (NodePtr p)
/* skip parameter of type void:  int f (void) */
{
  while (p != NULL && p->item->type != NULL && p->item->type->sort == Void)
     p = p->next;
  return p;
}

static void MergeDefaults (TypePtr decl, TypePtr def, char * name)
/* declaration (prototype, method in a class) and definition of the same function:
   same number of parameters, missing default values in 'def'
   are taken from 'decl'  (void f (int a = 1);  void f (int a) { ... }) */
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
        b->item->merged = true;      /* print: default value only in the declaration */
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

  /* destructor inside a class:  ~T () { ... } */
  if (sy==TILDA && CurrentClass != NULL)
  {
     DestructorDecl (CurrentClass, virt);
     return;
  }

  type = declaration_specifiers ();

  /* constructor inside a structure:  Item (...) ... */
  if (sy==LPAR && CurrentClass != NULL && Resolve (type) == CurrentClass)
  {
     ConstructorDecl (CurrentClass);
     return;
  }

  /* constructor outside the structure:  Item::Item (...) { ... } */
  if (sy==DCOLON)
  {
     TypePtr cls = Resolve (type);
     NextSymbol ();

     /* destructor outside the class:  T::~T () { ... } */
     if (sy==TILDA && cls != NULL && IsClsSort (cls->sort))
     {
        DestructorDecl (cls, false);
        return;
     }

     if (cls == NULL || !IsClsSort (cls->sort) || sy!=IDENT || cls->tag == NULL ||
         strcmp (IdentVal, cls->tag) != 0)
        Error ("Constructor name expected after ::  (methods: type T::name (...))");
     NextSymbol ();

     int scopes = OpenClassScopes (cls);   /* members of the class and its bases are visible */
     ConstructorDecl (cls);
     CloseScopes (scopes);
     return;
  }

  /* init_declarator_list */
  for (;;)
  {
     /* init_declarator */
     obj = declarator (type);

     /* method definition outside the class:  int T::f (...) { ... } */
     if (QualClass != NULL)
     {
        TypePtr cls = QualClass;
        QualClass = NULL;
        ObjectPtr m = Search (&cls->scope, obj->name);

        /* definition of a static member:  const int T::x = 0; */
        if (m != NULL && !m->fce && m->storage == Static && m->owner == cls)
        {
           m->outdef = true;          /* the definition is in this file (translate.c) */
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
        MergeDefaults (m->type, obj->type, obj->name);  /* default values from the declaration in the class */
        m->ptype = m->type;           /* print: declaration in the class (names, default values) */
        m->type = obj->type;          /* parameters with names from the definition */
        m->desc = obj->desc;
        m->outdef = true;
        Append (&printlist, PrintMarker (PK_OUTDEF, m, m->type));
        FunctionBody (m);
        return;
     }

     /* global function: prototype and later definition (or another prototype)
          void f (int a, int b = 0);   ...   void f (int a, int b) { ... }
        the object of the first declaration is used, default values are taken over */
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
              /* prototype after the definition - only default values */
              if (sy==LBRACE)
                 ErrorId ("Multiple definition of function", obj->name);
              MergeDefaults (obj->type, prev->type, obj->name);
              CheckSymbol (SEMICOLON);
              return;
           }

           MergeDefaults (prev->type, obj->type, obj->name);
           if (sy==LBRACE)            /* print: at the place of the prototype only the prototype */
              ReplaceInPrintList (prev, PrintMarker (PK_PROTO, prev, prev->type));
           prev->type = obj->type;    /* parameters with names from the last declaration */
           prev->desc = obj->desc;

           if (sy==LBRACE)
           {
              if (! OPT (Body))
                 Error ("Function definition is not allowed here");
              /* definition at the place of the definition (print -p, order in glist) */
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

     /* global variable:  extern int x;  ...  int x = 1;  (also in the opposite order)
          - one object, the definition replaces the extern declaration */
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
              skip_enter = true;              /* declaration after definition (or another extern) */
           else
           {
              /* definition after extern declaration */
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

     /* typedef struct X X;  typedef enum X X;  - name X is already a type (as in C++) */
     if (obj->name[0] != 0 && storage == Typedef && !skip_enter)
     {
        ObjectPtr prev = Search (Display[DisplayLevel], obj->name);
        if (prev != NULL && IsType (prev) && Resolve (prev->type) == Resolve (obj->type))
           skip_enter = true;
     }

     if (obj->name[0] != 0 && !skip_enter)  /* struct X { ... };  has no name */
        Enter (Display[DisplayLevel], obj);
     else if (skip_enter && storage == Extern)
     {
        /* only declaration - not added to the queue */
        if (OPT (Init) && sy==ASSIGN) Error ("extern declaration with initializer");
        if (sy!=COMMA) { CheckSymbol (SEMICOLON); return; }
        NextSymbol ();
        continue;
     }

     Append (queue, obj);
     obj->storage = storage;
     if (storage==Typedef && obj->type->tag == NULL)
     {
        obj->type->tag = obj->name;   /* typedef struct { ... } X;  (not typedef A B;) */
        obj->type->typedef_tag = true;
     }

     sort = obj->type->sort;
     if (sort==Void)
        Error ("Invalid type");

     obj->fce = storage!=Typedef &&
                (sort==Function || sort==Constructor || sort==Destructor);
     obj->Inline = inl && obj->fce;

     /* method of a class (static method has no this) */
     if (obj->fce && CurrentClass != NULL)
     {
        obj->owner = CurrentClass;
        obj->method = storage != Static;
        if (storage == Static) obj->storage = NoStorage;

        /* virtual: marked virtual, or overrides a virtual method of the base */
        ObjectPtr inh = CurrentClass->base != NULL ? LookupMember (CurrentClass->base, obj->name) : NULL;
        obj->Virtual = virt || (inh != NULL && inh->fce && inh->Virtual && SameParams (inh->type, obj->type));
        if (virt && !obj->method) Error ("Static method cannot be virtual");

        /* pure virtual method:  virtual void f () = 0; */
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

     /* for (T x : seq) - the declaration of the loop variable ends before : */
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
        /* member with initialization in the class:  bool x = false;  - assignment at the start of
           each constructor that does not initialize it (MemberInits) */
        NextSymbol ();
        obj->member_init = simple_expr ();
     }
     else if (CurrentClass != NULL && storage == Static)
     {
        /* static member of a class:  static const int max = 4 * 1024; */
        obj->owner = CurrentClass;
        if (sy==ASSIGN)
        {
           NextSymbol ();
           obj->init = const_expr ();
        }
     }

     if (OPT (CondInit) && sy==RPAR) return;   /* if (T x = e) - ) is read by the caller */
     if (sy!=COMMA) break;
     NextSymbol ();
  }

  if (obj->fce && sy==LBRACE)
  {
     if (OPT (Body) && obj->owner != NULL && obj->owner == CurrentClass && DeferTail != NULL)
        DeferBody (obj, false);           /* body of a method until the end of the class */
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
/* constructor of the structure 'cls', symbol ( is at the input
     Item ();                                  declaration
     Item () : key (0), left (nullptr) { ... }  definition */
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

  /* declaration in the class, definition outside the class - default values from the declaration */
  if (cls->ctor != NULL)
     MergeDefaults (cls->ctor->type, typ, cls->tag);

  /* constructor inside the class: initialization and body at the end of the class */
  if (cls == CurrentClass && DeferTail != NULL)
  {
     cls->ctor = obj;
     DeferBody (obj, true);
     return;
  }

  if (cls != CurrentClass)
  {
     /* definition outside the class - print: declaration in the class, definition at this place */
     obj->outdef = true;
     obj->ptype = (cls->ctor != NULL) ? cls->ctor->type : typ;
     Append (&printlist, PrintMarker (PK_OUTDEF, obj, typ));
  }
  ConstructorBody (cls, obj);
}

/* assignment of members with initialization in the class (bool x = false;) that are not
   in the constructor's initializer list (done, n) - appended after *last */
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

/* class without constructor with members having initialization in the class - implicit T () { } */
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
/* initialization of members and the body of the constructor, at the input is : or { */
{
  TypePtr typ = obj->type;

  OpenScope (&typ->scope);     /* parameters */
  Fce = obj;
  TypePtr save_method = MethodClass;
  MethodClass = cls;           /* this in the constructor */

  /* initialization of members  : key (0), left (nullptr)  =>  assignment at the start of the body */
  StatPtr first = NULL, last = NULL;
  ObjectPtr done [256];   /* members from the initializer list (MemberInits) */
  int ndone = 0;
  if (sy==COLON)
  {
     NextSymbol ();
     for (;;)
     {
        if (sy!=IDENT) Error ("Member name expected");

        /* base constructor:  : Base (args) */
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
           /* buf ()  - initialization by value: scalar => 0, arrays and objects
              are not changed (new => calloc, memory is zeroed) */
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

  MemberInits (cls, done, ndone, &first, &last);   /* bool x = false; in the class */

  if (sy!=LBRACE) Error ("{ expected");
  StatPtr body = statement ();
  if (sy==SEMICOLON) NextSymbol ();

  Fce = NULL;
  MethodClass = save_method;
  CloseScope ();

  /* { initialization ... body } */
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
/* destructor of the class 'cls', symbol ~ is at the input
     ~T ();   virtual ~T ();   virtual ~T () = 0;   ~T () { ... }   (in the class)
     T::~T () { ... }                                                (outside the class)
   Destructor is a method named "~" (the same name in all classes -
   overriding the virtual destructor of the base), stored in cls->dtor and among
   the members of the class. */
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
     /* definition outside the class */
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

  /* virtual: marked virtual, or the base has a virtual destructor */
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
     DeferBody (obj, false);            /* body at the end of the class */
  else
     FunctionBody (obj);
}

/************************* GLOBAL DECLARATION **************************/

void GlobalDecl (void)
{
  Queue queue;

  /* using namespace std;  - namespaces are ignored */
  if (sy==IDENT && strcmp (IdentVal, "using") == 0)
  {
     while (sy!=SEMICOLON && sy!=EOS) NextSymbol ();
     CheckSymbol (SEMICOLON);
     return;
  }

  /* extern "C" { ... },  extern "C" declarations;  - linkage is not distinguished */
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

  /* namespace X { ... } - declarations are global, X:: is skipped (NamespaceQual) */
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

/* marker in printlist */
static ObjectPtr PrintMarker (int kind, ObjectPtr ref, TypePtr type)
{
  ObjectPtr m = NewObject (ref->name, type);
  m->pkind = kind;
  m->pref = ref;
  m->ptype = type;
  m->desc = ref->desc;          /* type with aliases (typedef) at the moment of declaration */
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

bool PutTypeNameOnly = false;   /* printing of a type in an expression (new T, (T) e, sizeof (T)): only the class name */

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

        if (t->base!=NULL && t->base->tag!=NULL)   /* base class */
        {
           PutStr (" : public ");
           PutStr (t->base->tag);
        }

        PutEol ();

        PutChr ('{');
        PutEol ();
        if (t->sort==Class)
        {
           PutStr ("public:");     /* access is not checked - everything is public */
           PutEol ();
        }
        Indent ();

        for (p=t->list.first; p!=NULL; p=p->next)
        {
           Check (p->item!=NULL);
           /* methods with body inside the class, defined outside the class only as declarations */
           SubDecl (p->item, (p->item->fce && p->item->body != NULL && !p->item->outdef) ? full : header);
        }

        if (t->ctor != NULL && t->ctor->outdef)   /* constructor defined outside the class */
        {
           PutStr (t->ctor->name);
           Postfix (t->ctor->ptype, NULL, false);
           PutChr (';');
           PutEol ();
        }
        else if (t->ctor != NULL)     /* constructor */
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
/* constructor:  : Base (args) */
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

static TypePtr PutQualifier = NULL;   /* definition outside the class:  int T::f (...) */
static bool PutSkipMerged = false;    /* definition: default values only in the declaration */

void SubDecl (ObjectPtr obj, mode_typ mode)
{
  if (obj->type == NULL)            /* parameter ... */
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
  /* function without body at the end of translation = library function (glibc) */
  if (mode==full && obj->fce && obj->body==NULL && obj->owner==NULL && obj->storage!=Static)
     PutStr ("extern \"C\" ");
  if (! (obj->owner != NULL && obj->storage == Static))
     PutStorage (obj->storage, mode);

  spc = false;
  bool dtor = obj->fce && obj->owner != NULL && obj->owner->dtor == obj;
  if (dtor)
  {
     /* destructor:  ~T ()  /  T::~T ()  - without result type */
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
           PutChr (';');              /* function declaration (prototype) */
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
/* type in an expression:  new T,  static_cast <T *>,  sizeof (T) */
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
/* implicitly declared function in the global identifier tree */
{
  if (root == NULL) return false;
  return root->implicit || HasImplicit (root->left) || HasImplicit (root->right);
}

/* C library functions that C++ overloads in <cstring> (const char * strchr
   (const char *, int) and char * strchr (char *, int)) - the extern "C" prototype
   would clash with them; in the -p listing they are declared by #include <cstring> */
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
  /* called undeclared functions (printf, rand, ...) - C language headers */
  if (HasImplicit (gscope.root))
  {
     PutStr ("#include <cstdio>"); PutEol ();
     PutStr ("#include <cstdlib>"); PutEol ();
     PutStr ("#include <ctime>"); PutEol ();
  }
  PutStr ("#include <cstring>"); PutEol ();   /* strchr, strstr, ... - viz CppOverloaded */

  /* std::string and assert are built in - for g++ headers */
  if (UsesContainers)       /* Sequence, Dictionary - templates for g++ (cmini/ownheaders/containers.h) */
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
        /* function prototype (definition is later), declaration of extern variable */
        Object tmp;
        tmp = *obj->pref;   /* cmini: initialization of a struct only { ... } */
        tmp.type = obj->ptype;
        tmp.desc = obj->desc;
        tmp.body = NULL;
        tmp.init = NULL;
        if (!tmp.fce) tmp.storage = Extern;
        SubDecl (&tmp, header);
     }
     else if (obj->pkind == PK_OUTDEF)
     {
        /* method or constructor defined outside the class:  int T::f (...) { ... } */
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
