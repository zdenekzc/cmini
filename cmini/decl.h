
/* DECL.H */

#ifndef DECL_H
#define DECL_H

#include "inp.h"
#include "out.h"
#include "lex.h"

#ifdef __cplusplus
extern "C" {   /* front-end je v cistem C (decl.c, ...); g++ (instr.cc, test_instr.cc) */
#endif

struct Node;
struct Queue;
struct Scope;
struct Object;
struct Type;
struct Expr;
struct Stat;
struct VtSlot;

/* C: struct/enum tagy nejsou implicitni typova jmena (na rozdil od C++) */
#ifndef __cplusplus
typedef struct Node  Node;
typedef struct Queue Queue;
typedef struct Scope Scope;
typedef struct Object Object;
typedef struct Type  Type;
typedef struct Expr  Expr;
typedef struct Stat  Stat;
typedef struct VtSlot VtSlot;
#endif

typedef Node *   NodePtr;
typedef Queue *  QueuePtr;
typedef Scope *  ScopePtr;
typedef Object * ObjectPtr;
typedef Type *   TypePtr;
typedef Expr *   ExprPtr;
typedef Stat *   StatPtr;

/******************************* UTILITIES ********************************/

#define NEW(type) (type *) New (sizeof (type))
void * New (unsigned int size); /* allocate and clear memory */
char * NewStr (char * s);   /* allocate memory for string */

typedef char * IDENT_PTR;
IDENT_PTR ReadIdentPtr (void);
void ReadIdent (IDENT_TYP id);

void ErrorId (char * msg, char * id);
void Bug (void);
void Check (bool b);

/********************************** INIT **********************************/

void InitDecl (void);

/********************************* QUEUE **********************************/

struct Node                    /* Element of queue of objects */
{
   NodePtr    next;
   NodePtr    prev;
   ObjectPtr  item;

};

struct Queue                   /* Queue of objects */
{
   NodePtr first;
   NodePtr last;
};

void Clear (Queue * queue);
/* Initialize queue */

void Add (Queue * queue, NodePtr node);
/* Add node to queue */

void Append (Queue * queue, ObjectPtr obj);
/* Add object to queue */

void Join (Queue * dst, Queue * add);
/* Join two queues */

/********************************* SCOPE **********************************/

struct Scope
{
   ObjectPtr root;                          /* tree root */
};

extern Scope gscope;                        /* tree of global identifiers */
extern Queue glist;                         /* queue of global identifiers */
extern Queue printlist;                     /* order for printing (-p): glist + markers */
extern Queue ctorlist;                      /* queue of constructor definitions */
extern Queue methodlist;                    /* queue of method definitions (with body) */
extern Queue classlist;                     /* queue of defined structures / classes (tag objects) */
extern TypePtr CurrentClass;                /* structure being declared */
extern TypePtr MethodClass;                 /* class of method being compiled (this) */
extern ObjectPtr Fce;                       /* current function */

#define DISPLAY_MAX 64
extern int DisplayLevel;                    /* number of current scopes */
extern ScopePtr DisplayArray [DISPLAY_MAX]; /* array of scopes */

void OpenScope (ScopePtr s);                /* add new scope */
void CloseScope (void);                     /* remove last scope */

/********************************** TREE ***********************************/

void Enter (ScopePtr scope, ObjectPtr add);
/* Enter object into tree */

ObjectPtr Search (ScopePtr scope, IDENT_TYP name);
/* Search for identifier in tree, return NULL if not found */

ObjectPtr Lookup (IDENT_TYP name);
/* Search for identifier in all scopes, return NULL if not found */

ObjectPtr Find (IDENT_TYP name);
/* Search for identifier in all scopes, error if not found */

ObjectPtr LookupMember (TypePtr typ, IDENT_TYP name);
/* Search for identifier in structure and its base classes, return NULL if not found */

bool IsMemberOf (ObjectPtr obj, TypePtr typ);
/* obj is member (field or method) of structure typ or of its base classes */

ObjectPtr FindMember (TypePtr typ, IDENT_TYP name);
/* Search for identifier in structure, error if not found */

/********************************* OBJECT *********************************/

/* Register_ - ne Register: v C koliduje s typem Register z ../asm/reg.h (instr.cc) */
enum storage_typ { NoStorage, Typedef,
                   Auto, Register_, Static, Extern };
#ifndef __cplusplus
typedef enum storage_typ storage_typ;
#endif

struct Object                  /* Object = typedef, variable, function, ... */
{
   ObjectPtr   left;           /* pointers for tree */
   ObjectPtr   right;

   char *      name;           /* identifier */
   TypePtr     type;           /* type */
   TypePtr     desc;           /* detail type description */

   storage_typ storage;        /* storage class */
   ExprPtr     init;           /* initializer */
   ExprPtr     width;          /* width of bit field */
   ExprPtr     member_init;    /* polozka tridy s inicializaci ve tride:  bool x = false; */

   StatPtr     body;           /* function definition */
   bool        fce;            /* function */
   bool        Inline;         /* inline function */
   TypePtr     owner;          /* constructor or method - its class (structure) */
   bool        implicit;       /* implicitly declared function (call of undeclared function) */
   bool        method;         /* member function (hidden parameter this) */
   bool        Virtual;        /* virtual method */
   bool        pure;           /* pure virtual method:  virtual void f () = 0; */
   bool        deferred;       /* body inside class - parsed at the end of class */

   /* vypis -p (printlist) */
   int         pkind;          /* znacka v printlist: PK_PROTO, PK_OUTDEF (0 = objekt) */
   ObjectPtr   pref;           /* znacka: funkce, metoda nebo konstruktor */
   TypePtr     ptype;          /* PK_PROTO: typ z deklarace (jmena, implicitni hodnoty) */
   bool        outdef;         /* metoda / konstruktor definovany mimo tridu,
                                  staticka polozka definovana mimo tridu */
   bool        merged;         /* parametr: implicitni hodnota prevzata z deklarace */
   bool        extern_decl;    /* globalni promenna deklarovana i jako extern - vnejsi
                                  vazba i pro const (extern const char T []; v hlavicce) */

   /* konstruktor:  T (...) : Base (args) */
   bool        base_init;      /* konstruktor baze s argumenty */
   ExprPtr     base_args;      /* argumenty (NULL = zadne) */
};

enum { PK_PROTO = 1, PK_OUTDEF = 2 };

/********************************** TYPE **********************************/

enum sort_typ  { NoSort,
                 Char, Bool, Enum, Int, Float, Double, Pointer, Reference,
                 Array, Function, Struct, Union, Class, Void,
                 Constructor, Destructor,
                 StdString,     /* std::string - jen generator instr.cc */
                 StdSequence,   /* Sequence <T> - vestaveny typ (instr.cc) */
                 StdDictionary, /* Dictionary <K, V> - vestaveny typ (instr.cc) */
                 Name, Tag };
#ifndef __cplusplus
typedef enum sort_typ sort_typ;
#endif


#ifdef __cplusplus
inline bool IsIntegerSort (sort_typ sort)
{ return sort<=Int; }

inline bool IsFloatSort (sort_typ sort)
{ return sort==Float || sort==Double; }

inline bool IsNumericSort (sort_typ sort)
{ return sort<=Double; }

inline bool IsOperandSort (sort_typ sort)
{ return sort<=Pointer; }

inline bool IsPtrSort (sort_typ sort)
{ return sort==Pointer || sort==Reference || sort==Array; }

inline bool IsClsSort (sort_typ sort)
{ return sort==Struct || sort==Union || sort==Class; }
#else
static inline int IsIntegerSort (sort_typ sort) { return sort<=Int; }
static inline int IsFloatSort   (sort_typ sort) { return sort==Float || sort==Double; }
static inline int IsNumericSort (sort_typ sort) { return sort<=Double; }
static inline int IsOperandSort (sort_typ sort) { return sort<=Pointer; }
static inline int IsPtrSort     (sort_typ sort) { return sort==Pointer || sort==Reference || sort==Array; }
static inline int IsClsSort     (sort_typ sort) { return sort==Struct || sort==Union || sort==Class; }
#endif

enum length_typ  { NoLength, Short, Long };
enum sign_typ    { NoSign, Signed, Unsigned };
#ifndef __cplusplus
typedef enum length_typ length_typ;
typedef enum sign_typ   sign_typ;
#endif

struct Type
{
   sort_typ    sort;    /* NoSort, Char, Enum, Int, ... */
   sign_typ    sign;    /* NoSign, Signed, Unsigned */
   length_typ  length;  /* NoLength, Short, Long */
   bool        Const;
   bool        Volatile;

   TypePtr     from;    /* for arrays - type of element */
                        /* pointers - base type */
                        /* functions - type of result */

   TypePtr     desc;    /* detail type description */

   ExprPtr     range;   /* for arrays - number of elements */

   Scope       scope;   /* for structures - tree of fields */
                        /* for fuctions - tree of local identifiers */

   Queue       list;    /* for enums - list of elements */
                        /* for structures - fields */
                        /* for functions - parameters */

                        /* for structures and enums */
   char *      tag;     /* tag name */
   bool        defined; /* true => already defined */

   ObjectPtr   ctor;    /* for structures - constructor */
   ObjectPtr   dtor;    /* for classes - destructor (method named "~") */
   TypePtr     base;    /* for classes - base class (single inheritance) */
   TypePtr     elem;    /* Sequence <T>: T,  Dictionary <K, V>: V */
   TypePtr     key;     /* Dictionary <K, V>: K */
   ObjectPtr   tdef;    /* Name: typedef, jehoz jmeno alias pouziva (typedef ve tride - translate.c) */
   bool        long_long;     /* long long (v c-mini = long; translate.c vypise long long) */
   bool        typedef_tag;   /* typedef struct { ... } X; - jmeno X z typedef (v C jen X, ne struct X) */

                        /* sem.c - vypoctene vlastnosti tridy (0 = zatim neurceno, 1 = ne, 2 = ano) */
   int         need_ctor;     /* konstruktor (vlastni, vptr, baze, polozky) */
   int         need_ctor_z;   /*   i kvuli retezcum a kontejnerum v polozkach (nulovani - instr.cc) */
   int         need_dtor;
   int         need_dtor_z;
   bool        vt_done;       /* tabulka virtualnich metod spocitana (VtCount) */
   int         vt_count;
   VtSlot *    vt;
};

/* slot tabulky virtualnich metod (sem.c) */
struct VtSlot
{
   char *      name;      /* jmeno metody ("~" = destruktor) */
   ObjectPtr   decl;      /* metoda, ktera slot zavedla (typ slotu) */
   TypePtr     intro;     /* trida, ktera slot zavedla */
   ObjectPtr   impl;      /* posledni prekryti (v teto tride nebo bazi) */
   TypePtr     impl_cls;  /*   jeho trida */
};

TypePtr   NewType (sort_typ sort);
TypePtr   Resolve (TypePtr typ);
TypePtr   Link (sort_typ sort, TypePtr desc);
TypePtr   Alias (IDENT_TYP name, sort_typ sort, TypePtr desc);
ObjectPtr NewObject (IDENT_TYP name, TypePtr desc);
TypePtr   CreateType (IDENT_TYP id, sort_typ sort, bool def);

/****************************** DECLARATIONS ******************************/

#ifdef __cplusplus
const int Body    = 1;   /* function body { ... } */
const int Init    = 2;   /* variable initializer */
const int Width   = 4;   /* field width */
const int RangeFor = 8;  /* for (T x : seq) - deklarace konci pred : */
const int CondInit = 16; /* if (T x = e) - deklarace v podmince konci pred ) */
#else
#define Body     1
#define Init     2
#define Width    4
#define RangeFor 8
#define CondInit 16
#endif
extern bool RangeForDecl; /* declaration nasla  for (T x : ... */

bool IsDecl (void);                        /* is it declaration */
SYMBOL PeekSymbol (void);                  /* next symbol (current symbol is unchanged) */
bool IsQualifiedExpr (void);               /* T::f (...) - expression, not declaration */
void declaration (Queue * queue, int opt); /* read declaration */
void GlobalDecl (void);                    /* read global declaration */

void PutDecl (ObjectPtr obj);              /* print declaration */
void PutParamDecl (ObjectPtr obj);
void PutTypeName (TypePtr type);           /* type in expression (only class name) */         /* print declaration without ; (for parameters, for (...)) */

void PutGlobalDecl (void);                 /* print global decl. */
void PutGlobalHeader (void);               /* print headers */

/******************** EXPRESSION - AUXILIARY ROUTINES *********************/

extern Type IntDef,                        /* standard types */
            UnsignedDef,
            LongDef,
            UnsignedLongDef,
            FloatDef,
            DoubleDef,
            LongDoubleDef,
            VoidDef,
            BoolDef,
            CharDef,
            StringDef,
            NullPtrDef;

void InitTypes (void);                      /* expr.c - StringDef, NullPtrDef (InitDecl) */
bool IsInteger (TypePtr t);
bool IsFloat (TypePtr t);
bool IsNumeric (TypePtr t);
bool IsPointer (TypePtr t);
bool IsPtr (TypePtr t);
bool IsOperand (TypePtr t);
bool IsString (TypePtr t);
bool IsStdString (TypePtr t);              /* std::string */
bool IsContainer (TypePtr t);              /* Sequence <T>, Dictionary <K, V> */
bool IsPolymorphic (TypePtr cls);          /* trida s virtualnimi metodami (i zdedenymi) */
bool SameType (TypePtr t1, TypePtr t2);    /* stejny typ (i kontejnery) */
extern bool UsesContainers;                /* program pouziva Sequence / Dictionary (vypis -p) */
bool IsStringLike (TypePtr t);             /* std::string, char *, retezcova konstanta */

/* sem.c - pomocne funkce nad stromem, spolecne pro translate.c a instr.cc */
ExprPtr SkipParens (ExprPtr e);            /* bez zavorek (LPAR) */
bool    IsSequence (TypePtr t);            /* Sequence <T> */
bool    IsDictionary (TypePtr t);          /* Dictionary <K, V> */
bool    IsStringKey (TypePtr dict);        /* Dictionary <string, V> - klic CM_KSTR */
TypePtr ArrayElem (TypePtr t);             /* typ prvku pole (i vicerozmerneho), jinak t */
TypePtr ElemClass (TypePtr t);             /* trida nebo pole trid => trida, jinak NULL */
bool    IsEnumConst (ObjectPtr obj);       /* vyctova konstanta */
bool    IsField (ObjectPtr obj);           /* nestaticka datova polozka tridy */
bool    IsStaticMember (ObjectPtr obj);    /* staticka datova polozka tridy */
bool    IsCtorDecl (ObjectPtr m);          /* konstruktor (jmeno tridy) */
bool    IsDtorDecl (ObjectPtr m);          /* destruktor ("~") */
TypePtr CalleeType (ExprPtr f);            /* typ volane funkce (i pres ukazatel, referenci) */
bool    CallReturnsRef (ExprPtr e);        /* volani funkce vracejici referenci */
bool    NeedsCtor (TypePtr cls, bool zero_fields);   /* zero_fields: i retezce, kontejnery v polozkach */
bool    NeedsDtor (TypePtr cls, bool zero_fields);
int     VtCount (TypePtr cls);             /* pocet slotu tabulky virtualnich metod (0 - neni) */
VtSlot * VtGet (TypePtr cls, int i);       /* slot i */
int     VtIndex (TypePtr cls, const char * name);   /* slot metody podle jmena, -1 - neni */
bool    BaseIsPolymorphic (TypePtr cls);   /* baze s virtualnimi metodami */
bool    VptrHere (TypePtr cls);            /* trida zavadi ukazatel na tabulku virtualnich metod */
TypePtr VptrRoot (TypePtr cls);            /* trida, ktera zavedla ukazatel na tabulku (cls polymorfni) */
bool    IsConstInit (ObjectPtr obj);       /* const s inicializaci (const int N = 10;) */

void CheckInteger (TypePtr t);
void CheckOperand (TypePtr t);
void CheckOperandExt (TypePtr t);
void CheckString (TypePtr t);
void CheckLValue (ExprPtr e);
void CheckAssign (TypePtr t1, TypePtr t2);
void CheckCall (TypePtr t, ExprPtr exp);

/******************************* EXPRESSION *******************************/

struct Expr
{
  SYMBOL   op;                 /* operator */
  char *   id;                 /* identifier */
  ObjectPtr obj;               /* IDENT - declared object */
  ExprPtr  e1, e2, e3;         /* sub-expressions */
  TypePtr  type;               /* type of result */
};

/* expression encoding:

   IDENT         id
   NUM           id
   FLT           id
   STR           id
   CHR           id
   TRUE_, FALSE_, NULLPTR_   id
   THIS_         this      type = pointer to MethodClass
   DCOLON        T::id     obj = member function of class T (non-virtual call)

   LBRACK        e1 [e2]
   LPAR          (e1)
   FCE           e1 (e2)
   PERIOD        e1.id
   ARROW         e1->id

   DPLUS         e1 ++     (e2==NULL)
   DMINUS        e1 --

   DPLUS         ++ e2     (e1==NULL)
   DMINUS        -- e2

   PLUS          + e2
   MINUS         - e2
   NOT           ~ e2
   LOGNOT        ! e2
   ADR           & e2
   DEREF         * e2

   SIZEOF_       sizeof (e1)
   NEW_          new T (e1)   type = pointer to T, e1 = constructor arguments or NULL
                 new T [e2]   pole (e2 = pocet prvku)

   ASTERISK      e1 * e2
   SLASH         e1 / e2
   MOD           e1 % e2

   PLUS          e1 + e2
   MINUS         e1 - e2

   SHL           e1 << e2
   SHR           e1 >> e2

   LESS          e1 < e2
   GREATER       e1 > e2
   LESSEQUAL     e1 <= e2
   GREATEREQUAL  e1 >= e2

   EQUAL         e1 == e2
   UNEQUAL       e1 != e2

   AND           e1 & e2
   XOR           e1 ^ e2
   OR            e1 | e2
   LOGAND        e1 && e2
   LOGOR         e1 || e2
   QUESTION      e1 ? e2 : e3

   ASSIGN        e1 = e2
   MULASSIGN     e1 *= e2
   DIVASSIGN     e1 /= e2
   MODASSIGN     e1 %= e2
   ADDASSIGN     e1 += e2
   SUBASSIGN     e1 -= e2
   SHLASSIGN     e1 <<= e2
   SHRASSIGN     e1 >>= e2
   ANDASSIGN     e1 &= e2
   XORASSIGN     e1 ^= e2
   ORASSIGN      e1 |= e2

   COMMA         e1, e2

   LBRACE        {e1}
*/

ExprPtr expression (void);   /* read any expression */
ExprPtr const_expr (void);   /* read constant expression */
ExprPtr simple_expr (void);  /* read expression without , */
ExprPtr initializer (void);  /* read initializer */
ExprPtr StringConstruct (ExprPtr args);   /* string (p, n), string (n, c) */

void PutExpr (ExprPtr t);    /* print expression */

/******************************* STATEMENT ********************************/

struct Stat
{
  SYMBOL    st;          /* type of statement */

  char *    id;          /* label identifier */
  ExprPtr   e1, e2, e3;  /* expressions in statement */
  StatPtr   s1, s2;      /* inner statements */
  StatPtr   next;        /* next statement in { } */

  Scope     scope;       /* tree of local identifiers in { } */
  Queue     decl;        /* queue of declarations (when st==DECL) */
};

/* statement encoding:

   GOTO_      goto id;
   CONTINUE_  continue;
   BREAK_     break;
   RETURN_    return e1;
   WHILE_     while (e1) s1
   DO_        do s1 while (e1)
   FOR_       for (e1;e2;e3) s1
   IF_        if (e1) s1 else s2
   SWITCH_    switch (e1) s1
   LBRACE     { s1 s1->next ... }
   CASE_      case e1:
   DEFAULT_   default:

   LAB        e1:s1
   SIM        e1;
   DECL       decl.first ...
*/

StatPtr statement (void);       /* read statement */
void    PutStat (StatPtr s);    /* print statement */

/* z sem.c (presunuto z instr.cc / translate.c) */
bool isIntegralType (TypePtr t);
bool isUnsignedType (TypePtr t);
long scalarSize (TypePtr t);
bool isStructVal (TypePtr t);
bool isZeroInit (TypePtr t);
int fpKind (TypePtr t)       /* 0 - neni, 1 - float, 2 - double */;
bool hasFloatMember (TypePtr t);
bool hasUserDtor (TypePtr t);
bool hasCopyCtor (TypePtr t);
bool IsInClass (ObjectPtr f) /* metoda / konstruktor definovany uvnitr tridy => static inline */;
bool BaseHasField (TypePtr base, const char * name);
TypePtr VT (TypePtr t) /* typ hodnoty: bez aliasu a reference */;
TypePtr ET (ExprPtr e);
bool IsCharPtrT (TypePtr t) /* char *, char [] */;
TypePtr PtrClass (TypePtr t) /* ukazatel na tridu => trida, jinak NULL */;

#ifdef __cplusplus
}
#endif

#endif
