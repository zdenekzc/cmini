
/* EXPR.C */

#include <string.h>
#include <stdlib.h> /* calloc */

#include "decl.h"

/******************************* UTILITIES ********************************/

void ErrorId (char * msg, char * id)
{
  char txt [256];
  strmov (txt, sizeof (txt), msg);
  stradd (txt, sizeof (txt), " ");
  stradd (txt, sizeof (txt), id);
  Error (txt);
}

void Bug (void)
{
  Error ("Internal error");
}

void Check (bool b)
{
  if (!b) Bug ();
}

void * New (unsigned int size)
/* allocate and clear memory */
{
  return calloc (1, size);
}

char * NewStr (char * s)
/* allocate memory for string */
{
  char * p;
  p = (char *) New (strlen(s)+1);
  strcpy (p, s);
  return p;
}

IDENT_PTR ReadIdentPtr (void)
/* read any identifier from the input */
{
  if (sy!=IDENT) Error ("Identifier expected");
  char * id = NewStr (IdentVal);
  NextSymbol ();
  return id;
}

void ReadIdent (IDENT_TYP id)
/* read any identifier from the input */
{
  if (sy!=IDENT) Error ("Identifier expected");
  strmov (id, sizeof (IDENT_TYP), IdentVal);
  NextSymbol ();
}

/****************************** TYPE LEVELS *******************************/

#define IntLev          0
#define UnsignedLev     1
#define LongLev         2
#define UnsignedLongLev 3
#define FloatLev        4
#define DoubleLev       5
#define LongDoubleLev   6
#define OtherLev        7

Type IntDef          = { Int, Signed },
     UnsignedDef     = { Int, Unsigned },
     LongDef         = { Int, Signed, Long },
     UnsignedLongDef = { Int, Unsigned, Long },
     FloatDef        = { Float },
     DoubleDef       = { Double },
     LongDoubleDef   = { Double, NoSign, Long },

     VoidDef         = { Void },
     BoolDef         = { Bool },
     CharDef         = { Char },

     StringDef       = { Pointer, NoSign, NoLength, false, false, NULL },   /* from = &CharDef (InitTypes) */
     NullPtrDef      = { Pointer, NoSign, NoLength, false, false, NULL };   /* from = &VoidDef (InitTypes) */

/* cmini (compiling itself): in the initializer of a global variable the address of another
   variable must not appear - pointers are set only at initialization (InitDecl) */
void InitTypes (void)
{
  StringDef.from = &CharDef;
  NullPtrDef.from = &VoidDef;
}

static TypePtr Std (int level)
{
  switch (level)
  {
     case 0:  return &IntDef;
     case 1:  return &UnsignedDef;
     case 2:  return &LongDef;
     case 3:  return &UnsignedLongDef;
     case 4:  return &FloatDef;
     case 5:  return &DoubleDef;
     default: return &LongDoubleDef;
  }
}

int CalcLevel (TypePtr typ)
/* Determine the category to which the given type belongs */
{
  switch (typ->sort)
  {
     case Char:
     case Bool:     /* bool is promoted to int in arithmetic */
     case Enum:
        return IntLev;

     case NoSort:
     case Int:
        if (typ->length!=Long)
           if (typ->sign!=Unsigned)
              return IntLev;
           else
              return UnsignedLev;
        else
           if (typ->sign!=Unsigned)
              return LongLev;
           else
              return UnsignedLongLev;

     case Float:
        return FloatLev;

     case Double:
        if (typ->length!=Long)
           return DoubleLev;
        else
           return LongDoubleLev;

     default:
        return OtherLev;
  }
}

/***************************** TYPE CHECKING ******************************/

bool IsInteger (TypePtr t)
/* test whether t is an integer type - Char, Enum, Int */
{
  return IsIntegerSort (t->sort);
}

bool IsFloat (TypePtr t)
/* test whether t is a real type */
{
  return IsFloatSort (t->sort);
}

bool IsNumeric (TypePtr t)
/* test whether t is an integer or real type */
{
  return IsNumericSort (t->sort);
}

bool IsPointer (TypePtr t)
/* test whether t is a pointer - Pointer */
{
  return t->sort==Pointer;
}

bool IsPtr (TypePtr t)
/* test whether t is a pointer, reference or array - Pointer, Reference, Array */
{
  return IsPtrSort (t->sort);
}

bool IsOperand (TypePtr t)
/* test whether t is an integer type, real type or pointer */
/* Char, Enum, Int, Float, Double, Pointer */
{
  while (t->sort == Reference)
     t = t->from;
  return IsOperandSort (t->sort);
}

bool IsString (TypePtr t)
/* test whether t is a string */
{
  sort_typ sort;
  TypePtr from;

  sort = t->sort;
  from = t->from;
  return (sort==Pointer || sort==Array) && from->sort==Char;
}

bool IsText (TypePtr t)
{
  sort_typ sort;
  TypePtr from;

  sort = t->sort;
  from = t->from;
  return sort==Array && from->sort==Char;
}

/* ---------------------------------------------------------------------- */

void InvOp (void)
{
  Error ("Invalid operand type");
}

void CheckInteger (TypePtr t)
/* check whether t is an integer type - Char, Enum, Int */
{
  if (!IsInteger (t)) InvOp ();
}

static inline void CheckNumeric (TypePtr t)
/* check whether t is an integer or real type */
/* Char, Enum, Int, Float, Double */
{
  if (!IsNumeric (t)) InvOp ();
}

void CheckOperand (TypePtr t)
/* check whether t is an integer type, real type or pointer */
/* Char, Enum, Int, Float, Double, Pointer */
{
  if (!IsOperand (t)) InvOp ();
}

void CheckPtr (TypePtr t)
/* check whether t is a pointer, reference or array - Pointer, Reference, Array */
{
  if (!IsPtr (t)) InvOp ();
}

void CheckString (TypePtr t)
/* check whether t is a string */
{
  if (!IsString (t)) InvOp ();
}

/* ---------------------------------------------------------------------- */

TypePtr integers (TypePtr t1, TypePtr t2)
/* operation on integer operands */
{
  int a, b;

  a = CalcLevel (t1);
  b = CalcLevel (t2);
  if (a<b) a=b;

  if (a>UnsignedLongLev) InvOp ();
  return Std (a);
}

TypePtr arit (TypePtr t1, TypePtr t2)
/* operation on numeric operands */
{
  int a, b;

  a = CalcLevel (t1);
  b = CalcLevel (t2);
  if (a<b) a=b;

  if (a==OtherLev) InvOp ();
  return Std (a);
}

TypePtr pointers (TypePtr t1, TypePtr t2)
/* check whether t1 and t2 are pointers of the same type */
{
  if (IsInteger (t1))
  {
     CheckPtr (t2);
     return (t2);
  }
  if (IsInteger (t2))
  {
     CheckPtr (t1);
     return (t1);
  }

  CheckPtr (t1);
  CheckPtr (t2);
  return (t1);
}

void compare (TypePtr t1, TypePtr t2)
/* check whether types t1 and t2 can be compared */
{
  if (IsInteger (t1))
     CheckOperand (t2);
  else if (IsFloat (t1))
     CheckNumeric (t2);
  else if (IsPtr (t1))
     pointers (t1, t2);
  else InvOp ();
}

/******************************* STD::STRING ******************************/

bool IsStdString (TypePtr t)
{
  return t != NULL && t->sort == StdString;
}

bool IsContainer (TypePtr t)
{
  return t != NULL && (t->sort == StdSequence || t->sort == StdDictionary);
}

bool SameType (TypePtr t1, TypePtr t2)
/* same type: basic types by kind, sign and length, pointers
   and containers by element type; classes and enums by identity */
{
  t1 = Resolve (t1);
  t2 = Resolve (t2);
  if (t1 == t2) return true;
  if (t1 == NULL || t2 == NULL || t1->sort != t2->sort) return false;
  switch (t1->sort)
  {
     case StdSequence:
        return SameType (t1->elem, t2->elem);
     case StdDictionary:
        return SameType (t1->key, t2->key) && SameType (t1->elem, t2->elem);
     case Pointer:
     case Reference:
        if (Resolve (t1->from)->sort == Void || Resolve (t2->from)->sort == Void) return true;
        return SameType (t1->from, t2->from);
     case Struct: case Union: case Class: case Enum: case Function:
        return false;
     default:
        return t1->sign == t2->sign && t1->length == t2->length;
  }
}

static void MethodParam (TypePtr fn, TypePtr type, ExprPtr init);

/* built-in methods of Sequence <T> and Dictionary <K, V> */
static ObjectPtr ContainerMethod (IDENT_TYP id, TypePtr typ)
{
  TypePtr T = typ->elem;
  TypePtr fn = NULL;
  if (typ->sort == StdSequence)
  {
     if (strcmp (id, "add") == 0 || strcmp (id, "push") == 0 || strcmp (id, "insertFirst") == 0)
     {
        fn = Link (Function, &VoidDef);
        MethodParam (fn, T, NULL);
     }
     else if (strcmp (id, "get") == 0)
     {
        fn = Link (Function, T);
        MethodParam (fn, &IntDef, NULL);
     }
     else if (strcmp (id, "change") == 0)
     {
        fn = Link (Function, &VoidDef);
        MethodParam (fn, &IntDef, NULL);
        MethodParam (fn, T, NULL);
     }
     else if (strcmp (id, "contains") == 0)
     {
        fn = Link (Function, &BoolDef);
        MethodParam (fn, T, NULL);
     }
     else if (strcmp (id, "last") == 0 || strcmp (id, "top") == 0)
        fn = Link (Function, T);
     else if (strcmp (id, "pop") == 0 || strcmp (id, "clear") == 0)
        fn = Link (Function, &VoidDef);
  }
  else
  {
     TypePtr K = typ->key;
     if (strcmp (id, "store") == 0)
     {
        fn = Link (Function, &VoidDef);
        MethodParam (fn, K, NULL);
        MethodParam (fn, T, NULL);
     }
     else if (strcmp (id, "get") == 0)
     {
        fn = Link (Function, T);
        MethodParam (fn, K, NULL);
     }
     else if (strcmp (id, "contains") == 0)
     {
        fn = Link (Function, &BoolDef);
        MethodParam (fn, K, NULL);
     }
     else if (strcmp (id, "clear") == 0)
        fn = Link (Function, &VoidDef);
  }
  if (strcmp (id, "size") == 0)
     fn = Link (Function, &IntDef);
  else if (strcmp (id, "empty") == 0)
     fn = Link (Function, &BoolDef);

  if (fn == NULL)
     ErrorId (typ->sort == StdSequence ? "Unknown method of Sequence" : "Unknown method of Dictionary", id);

  ObjectPtr obj = NewObject (id, fn);
  obj->fce = true;
  obj->method = true;
  return obj;
}

bool IsStringLike (TypePtr t)
/* std::string, char * or string constant */
{
  if (t == NULL) return false;
  if (t->sort == StdString) return true;
  return (t->sort == Pointer || t->sort == Array) && t->from != NULL && t->from->sort == Char;   /* also char msg [512] */
}

static void CheckStringOperand (TypePtr t, bool allow_char)
/* allow_char: character (also an integer value as a character - s += 'a' + i) */
{
  if (IsStringLike (t)) return;
  if (allow_char && t != NULL && IsIntegerSort (t->sort)) return;
  Error ("String expected");
}

static TypePtr StringResult (ExprPtr e1, SYMBOL op, TypePtr t1, TypePtr t2)
/* operations with std::string:  +  =  +=  ==  !=  <  <=  >  >=  */
{
  switch (op)
  {
     case PLUS:
        CheckStringOperand (t1, true);
        CheckStringOperand (t2, true);
        return NewType (StdString);

     case ASSIGN:
     case ADDASSIGN:
        CheckLValue (e1);
        if (! IsStdString (t1)) Error ("Invalid assignment of string");
        CheckStringOperand (t2, true);
        return t1;

     case EQUAL:
     case UNEQUAL:
     case LESS:
     case GREATER:
     case LESSEQUAL:
     case GREATEREQUAL:
        CheckStringOperand (t1, false);
        CheckStringOperand (t2, false);
        return &IntDef;

     default:
        Error ("Invalid operation with string");
  }
  return NULL;
}

static ExprPtr NumConst (const char * value, TypePtr type)
{
  ExprPtr e = NEW (Expr);
  e->op = NUM;
  e->id = NewStr ((char *) value);
  e->type = type;
  return e;
}

static void MethodParam (TypePtr fn, TypePtr type, ExprPtr init)
/* parameter of a built-in method (no name), init = default value */
{
  ObjectPtr p = NewObject ((char *) "", type);
  p->init = init;
  Append (&fn->list, p);
}

/* built-in methods of std::string:  c_str (), length (), size (), empty (),
   substr (pos = 0, n = npos), find (s, pos = 0), rfind (s, pos = npos), clear ()
   (argument s of the methods find and rfind: std::string, constant, char * or character) */
static ObjectPtr StringMethod (IDENT_TYP id)
{
  TypePtr res;
  TypePtr fn = NULL;
  if (strcmp (id, "c_str") == 0) res = Link (Pointer, &CharDef);
  else if (strcmp (id, "length") == 0 || strcmp (id, "size") == 0) res = &IntDef;
  else if (strcmp (id, "empty") == 0) res = &BoolDef;
  else if (strcmp (id, "clear") == 0) res = &VoidDef;
  else if (strcmp (id, "reserve") == 0)       /* no effect - strings are not changed */
  {
     fn = Link (Function, &VoidDef);
     MethodParam (fn, &UnsignedLongDef, NULL);
  }
  else if (strcmp (id, "substr") == 0)
  {
     fn = Link (Function, NewType (StdString));
     MethodParam (fn, &UnsignedLongDef, NumConst ("0", &IntDef));
     MethodParam (fn, &UnsignedLongDef, NumConst ("-1", &UnsignedLongDef));
  }
  else if (strcmp (id, "find") == 0 || strcmp (id, "rfind") == 0)
  {
     fn = Link (Function, &UnsignedLongDef);
     MethodParam (fn, NewType (StdString), NULL);
     MethodParam (fn, &UnsignedLongDef,
                  strcmp (id, "find") == 0 ? NumConst ("0", &IntDef) : NumConst ("-1", &UnsignedLongDef));
  }
  else ErrorId ("Unknown method of std::string", id);

  if (fn != NULL)
  {
     ObjectPtr obj = NewObject (id, fn);
     obj->fce = true;
     obj->method = true;
     return obj;
  }

  ObjectPtr obj = NewObject (id, Link (Function, res));
  obj->fce = true;
  obj->method = true;
  return obj;
}

/* ---------------------------------------------------------------------- */

void CheckLValue (ExprPtr e)
{
  bool ok;
  SYMBOL op;
  TypePtr t;
  sort_typ sort;

  ok = true;

  while (e->op == LPAR)
     e = e->e1;

  op = e->op;
  if (op!=IDENT &&
      op!=LBRACK &&
      op!=PERIOD &&
      op!=ARROW &&
      op!=DEREF &&
      ! (op==DYNAMIC_CAST_ && IsClsSort (e->type->sort)))   /* dynamic_cast <T &> */
      ok = false;

  t = e->type;
  if (t->Const) ok = false;

  if (!IsOperand (t)) /* char, number, pointer */
  {
     sort = t->sort;
     if (sort!=Struct && sort!=Union && sort!=Class && sort!=StdString &&
         sort!=StdSequence && sort!=StdDictionary)
        ok = false;
  }

  if (!ok) Error ("lvalue expected");
}

void CheckAssign (TypePtr t1, TypePtr t2)
/* check whether variables of type t1 can be assigned an expression of type t2 */
{
  if (IsStdString (t1))
  {
     CheckStringOperand (t2, true);
     return;
  }
  if (IsStdString (t2))
     Error ("Invalid conversion of std::string");
  if (IsContainer (t1) || IsContainer (t2))
  {
     /* Sequence <T>, Dictionary <K, V> - same type (assignment is a copy) */
     if (! SameType (t1, t2)) Error ("Incompatible Sequence / Dictionary types");
     return;
  }
  if (t1 != NULL && t2 != NULL && IsClsSort (t1->sort) && IsClsSort (t2->sort))
  {
     /* object of a class to a reference to the same class or to its base:  f (*p) */
     for (TypePtr t = t2; t != NULL; t = t->base)
        if (t == t1) return;
     Error ("Incompatible structure types");
  }
  /* function name => pointer to function:  int (*f) (int) = g; */
  if (t2 != NULL && t2->sort == Function)
  {
     if (t1 == NULL || t1->sort != Pointer || t1->from == NULL || Resolve (t1->from)->sort != Function)
        Error ("Pointer to function expected");
     return;
  }
  if (t1!=t2)
     compare (t1, t2);
}

void CheckCall (TypePtr t, ExprPtr exp)
/* check whether a function of type t can be called with parameters exp */
{
  NodePtr par; /* formal parameter */
  ExprPtr arg; /* argument expression */

  if (t->sort != Function) Error ("Call of nonfunction");
  par = t->list.first;

  for (;;)
  {
     if (par!=NULL)
     {
        TypePtr ptyp = par->item->type;
        if (ptyp==NULL)
           return; /* ... => O.K. */
        if (ptyp->sort==Void)
           par = NULL;
     }

     /* get argument expression */
     /* prepare next expression */
     if (exp!=NULL && exp->op==COMMA)
     {
        arg = exp->e1;
        exp = exp->e2;
     }
     else
     {
        arg = exp;
        exp = NULL;
     }

     if (par==NULL || arg==NULL) break;
     if (par->item->type->sort == Reference)
     {
        TypePtr rt = par->item->type->from;
        if (! (rt->Const && IsStdString (rt)))   /* const string & => also a value */
           CheckLValue (arg);                     /* T & <= lvalue of type T */
        CheckAssign (rt, arg->type);
     }
     else
        CheckAssign (par->item->type, arg->type);

     /* next formal parameter */
     par = par->next;
  }

  /* parameters with default value:  int level = 0 */
  while (par!=NULL && par->item->type!=NULL && par->item->init!=NULL)
     par = par->next;

  if (par!=NULL)
     Error ("Too few parameters in function call");
  if (arg!=NULL)
     Error ("Extra parameter in function call");
}

TypePtr add (TypePtr t1, TypePtr t2)
/* check whether expressions of types t1 and t2 can be added */
/* t1 and t2 may be NULL */
{
  TypePtr res;

  if (t1==NULL) t1 = &IntDef;
  if (t2==NULL) t2 = &IntDef;

  if (IsInteger (t1))
  {
     if (IsPtr (t2))
        res = t2;
     else
        res = arit (t1, t2);
  }
  else if (IsFloat (t1))
  {
     res = arit (t1, t2);
  }
  else if (IsPtr (t1))
  {
     CheckInteger (t2);
     res = t1;
  }
  else InvOp ();

  return (res);
}

TypePtr sub (TypePtr t1, TypePtr t2)
/* check whether expressions of types t1 and t2 can be subtracted */
/* t1 and t2 may be NULL */
{
  TypePtr res;

  if (t1==NULL) t1 = &IntDef;
  if (t2==NULL) t2 = &IntDef;

  if (IsNumeric (t1))
     res = arit (t1, t2);
  else if (IsPtr (t1))
  {
     res = t1;
     if (IsPtr (t2))
        pointers (t1, t2);
     else
        CheckInteger (t2);
  }
  else InvOp ();

  return (res);
}

/********************************** CALC **********************************/

ExprPtr calc (ExprPtr e1, ExprPtr e2, SYMBOL op)
/* Create an expression from operands 'e1', 'e2' and operator 'op', determine the type of the result */
{
  ExprPtr ans;
  TypePtr t1 = NULL;
  TypePtr t2 = NULL;
  TypePtr res = NULL;

  if (e1!=NULL) t1 = e1->type;
  if (e2!=NULL) t2 = e2->type;

  /* std::string */
  if ((IsStdString (t1) || IsStdString (t2)) &&
      op != LPAR && op != COMMA && op != FCE && op != ADR && op != LBRACK && op != SIZEOF_)
  {
     ans = NEW (Expr);
     ans->op = op;
     ans->e1 = e1;
     ans->e2 = e2;
     ans->type = StringResult (e1, op, t1, t2);
     return ans;
  }

  switch (op)
  {
     case LPAR:    /* ( ) */
        res = t1;
        break;

     case LBRACK:  /* [ ] */
        if (t1->sort == StdSequence)     /* seq [i] - element (read and write) */
        {
           CheckAssign (&IntDef, t2);
           res = t1->elem;
           break;
        }
        if (t1->sort == StdDictionary)   /* dict [k] - value (a missing key is inserted) */
        {
           CheckAssign (t1->key, t2);
           res = t1->elem;
           break;
        }
        if (IsStdString (t1))            /* s [i] - character of the string (read only) */
        {
           CheckAssign (&IntDef, t2);
           res = &CharDef;
           break;
        }
        if (t1->sort!=Array && t1->sort!=Pointer)
           Error ("Invalid indirection");
        CheckAssign (&IntDef, t2);

        res = t1->from;
        break;

     case FCE:     /* ( ) */
        if (t1->sort == Pointer && t1->from != NULL && Resolve (t1->from)->sort == Function)
           t1 = Resolve (t1->from);         /* call via pointer to function:  f (a, b) */
        CheckCall (t1, e2);
        res = t1->from;
        break;

     case ADR:     /* & */
        res = Link (Pointer, t2);
        break;

     case DEREF:   /* * */
        if (t2->sort != Pointer) Error ("Invalid indirection");
        res = t2->from;
        break;

     case SIZEOF_:  /* sizeof */
        res = &UnsignedLongDef;
        break;

     case NOT:     /* ~ */
        CheckNumeric (t2);
        res = t2;
        break;

     case LOGNOT:  /* ! */
        CheckOperand (t2);
        res = &IntDef;
        break;

     case UNARYPLUS:   /* +e2 */
     case UNARYMINUS:  /* -e2 */
        CheckNumeric (t2);
        res = t2;
        break;

     case UNARYNOT:    /* ~e2 */
        CheckInteger (t2);
        res = t2;
        break;

     case DPLUS:   /* ++e2 and e1++ */
     case PLUS:    /* +e2 and e1+e2 */
        res = add (t1, t2);
        break;

     case DMINUS:  /* --e2 and e1-- */
        res = (t1 == NULL) ? sub (t2, NULL) : sub (t1, t2);   /* --p  (ukazatel) */
        break;

     case MINUS:   /* -e2 and e1-e2 */
        res = sub (t1, t2);
        break;

     case ASTERISK:
     case SLASH:
        res = arit (t1, t2);
        break;

     case MOD:
     case SHL:
     case SHR:
     case AND:
     case XOR:
     case OR:
        res = integers (t1, t2);
        break;

     case LESS:
     case GREATER:
     case LESSEQUAL:
     case GREATEREQUAL:
        compare (t1, t2);
        res = &IntDef;
        break;

     case EQUAL:
     case UNEQUAL:
        compare (t1, t2);
        res = &IntDef;
        break;

     case LOGAND:
     case LOGOR:
        CheckOperand (t1);
        CheckOperand (t2);
        res = &IntDef;
        break;

     case ASSIGN:
        CheckLValue (e1);
        CheckAssign (t1, t2);
        res = t1;
        break;

     case ADDASSIGN:
        CheckLValue (e1);
        add (t1, t2);
        CheckAssign (t1, t2);
        res = t1;
        break;

     case SUBASSIGN:
        sub (t1, t2);
        CheckAssign (t1, t2);
        res = t1;
        break;

     case MULASSIGN:
     case DIVASSIGN:
        CheckLValue (e1);
        arit (t1, t2);
        CheckAssign (t1, t2);
        res = t1;
        break;

     case MODASSIGN:
     case SHLASSIGN:
     case SHRASSIGN:
     case ANDASSIGN:
     case XORASSIGN:
     case ORASSIGN:
        CheckLValue (e1);
        integers (t1, t2);
        CheckAssign (t1, t2);
        res = t1;
        break;

     case COMMA:
        res = t2;
        break;

     case LBRACE:
        res = &VoidDef; /* !!! */
        break;

     default:
        Bug ();
  }

  ans = NEW (Expr);
  ans->op = op;
  ans->e1 = e1;
  ans->e2 = e2;
  ans->type = res;

  return (ans);
}

ExprPtr calc3 (ExprPtr e1, ExprPtr e2, ExprPtr e3, SYMBOL op)
/* Operation with three operands */
{
  ExprPtr ans;
  TypePtr t1, t2, t3, res;

  t1 = e1->type;
  t2 = e2->type;
  t3 = e3->type;

  CheckOperand (t1);

  /* function name => pointer to function:  c ? f : NULL */
  if (t2 != NULL && t2->sort == Function) t2 = Link (Pointer, t2);
  if (t3 != NULL && t3->sort == Function) t3 = Link (Pointer, t3);

  if (t2==t3)
     res = t2;
  else if (IsStdString (t2) || IsStdString (t3))
  {
     /* c ? s : "abc"  =>  std::string */
     CheckStringOperand (t2, true);
     CheckStringOperand (t3, true);
     res = NewType (StdString);
  }
  else if (IsPtr (t2) || IsPtr (t3))
     res = pointers (t2, t3);
  else
     res = arit (t2, t3);

  ans = NEW (Expr);
  ans->op = op;
  ans->e1 = e1;
  ans->e2 = e2;
  ans->e3 = e3;
  ans->type = res;

  return ans;
}

ExprPtr calci (ExprPtr e1, IDENT_TYP id, SYMBOL op)
/* Create an expression from operands 'e1', 'id' and operator 'op', determine the type of the result */
{
  ExprPtr ans;
  TypePtr res;

  TypePtr   typ;
  ObjectPtr obj;
  sort_typ  sort;

  res = NULL;
  typ = e1->type;
  sort = typ->sort;

  switch (op)
  {
     case PERIOD:  /* . */
        if (sort == StdString)
        {
           obj = StringMethod (id);
           res = obj->type;
           break;
        }
        if (sort == StdSequence || sort == StdDictionary)
        {
           obj = ContainerMethod (id, typ);
           res = obj->type;
           break;
        }
        if (! IsClsSort (sort))
           Error ("Structure required on left side of .");

        obj = FindMember (typ, id);
        res = obj->type;
        break;

     case ARROW:   /* -> */
        if (sort != Pointer)
           Error ("Pointer to structure required on left side of ->");

        typ = typ->from;
        sort = typ->sort;

        if (sort == StdString)
        {
           obj = StringMethod (id);
           res = obj->type;
           break;
        }
        if (sort == StdSequence || sort == StdDictionary)
        {
           obj = ContainerMethod (id, typ);
           res = obj->type;
           break;
        }
        if (! IsClsSort (sort))
           Error ("Pointer to structure required on left side of ->");

        obj = FindMember (typ, id);
        res = obj->type;
        break;

     default:
        Bug ();
  }

  ans = NEW (Expr);
  ans->op = op;
  ans->e1 = e1;
  ans->id = id;
  ans->obj = obj;      /* structure member */
  ans->type = res;

  return ans;
}

/******************************* EXPRESSION *******************************/

TypePtr declaration_specifiers (void);   /* decl.c */
bool IsType (ObjectPtr obj);              /* decl.c */

ObjectPtr ImplicitFunction (char * name)
/* call of an undeclared function => implicit declaration  int name (...)
   (e.g. printf, rand, ... - we do not process header files) */
{
  TypePtr typ = Link (Function, &IntDef);
  Append (&typ->list, NewObject ("...", NULL));

  ObjectPtr obj = NewObject (name, typ);
  obj->storage = Extern;
  obj->fce = true;
  obj->implicit = true;
  Enter (&gscope, obj);
  return obj;
}

/* cout, cerr, endl (take std::cout, std::cerr, std::endl):  cout << a << b << endl;
   In primary only a MARK (IDENT without an object); shift_expr converts it into a call of cm_out_*
   (cminirt.c). An undeclared name => mark; a declared variable takes precedence. */
static bool IsStreamName (char * s)
{
   return strcmp (s, "cout") == 0 || strcmp (s, "cerr") == 0 || strcmp (s, "endl") == 0;
}

static ExprPtr StreamMarker (char * name)
{
   ExprPtr e = NEW (Expr);
   e->op = IDENT;
   e->id = name;
   e->obj = NULL;
   e->type = &VoidDef;
   return e;
}

static bool IsStreamMarker (ExprPtr e)
{
   return e->op == IDENT && e->obj == NULL && e->id != NULL && IsStreamName (e->id);
}

ExprPtr primary (void)
{
  ExprPtr e;
  char * ans;
  TypePtr res;
  ObjectPtr obj = NULL;

  switch (sy)
  {
     case IDENT:
        ans = NewStr (IdentVal);
        obj = Lookup (IdentVal);

        /* qualified method name:  Base::f (...)  - call without the virtual mechanism */
        if (obj != NULL && IsType (obj) && IsClsSort (Resolve (obj->type)->sort))
        {
           TypePtr cls = Resolve (obj->type);
           NextSymbol ();
           if (sy != DCOLON) Error ("Syntax error in expression");
           NextSymbol ();
           if (sy != IDENT) Error ("Identifier expected");
           ObjectPtr m = FindMember (cls, IdentVal);

           /* T::max (static member),  T::f (...) (static method) */
           if (!m->method)
           {
              if (!m->fce && m->storage != Static) ErrorId ("Static member expected", IdentVal);
              e = NEW (Expr);
              e->op = IDENT;
              e->id = m->name;
              e->obj = m;
              e->type = m->type;
              NextSymbol ();
              return e;
           }

           e = NEW (Expr);
           e->op = DCOLON;
           e->id = m->name;
           e->obj = m;
           e->type = m->type;
           NextSymbol ();
           return e;
        }

        if (obj == NULL)
        {
           NextSymbol ();

           /* string::npos,  std::string::npos  (= (size_t) -1) */
           if (sy == DCOLON && (strcmp (ans, "string") == 0 || strcmp (ans, "std") == 0))
           {
              NextSymbol ();
              if (strcmp (ans, "std") == 0)
              {
                 if (sy == IDENT && IsStreamName (IdentVal))     /* std::cout, std::cerr, std::endl */
                 {
                    ans = NewStr (IdentVal);
                    NextSymbol ();
                    return StreamMarker (ans);
                 }
                 if (sy != IDENT || strcmp (IdentVal, "string") != 0)
                    Error ("Only std::string is supported from namespace std");
                 NextSymbol ();
                 CheckSymbol (DCOLON);
              }
              if (sy != IDENT || strcmp (IdentVal, "npos") != 0)
                 Error ("string::npos expected");
              NextSymbol ();
              e = NEW (Expr);
              e->op = NUM;
              e->id = NewStr ("-1");
              e->type = &UnsignedLongDef;
              return e;
           }

           if (IsStreamName (ans)) return StreamMarker (ans);      /* cout, cerr, endl */

           if (sy != LPAR) ErrorId ("Unknown identifier", ans);
           obj = ImplicitFunction (ans);

           e = NEW (Expr);
           e->op = IDENT;
           e->id = ans;
           e->obj = obj;
           e->type = obj->type;
           return e; /* NextSymbol has already been called */
        }
        res = obj->type;
        if (res->sort == Reference)  /* reference behaves as a variable of type T */
           res = res->from;
        break;

     case TRUE_:
        ans = NewStr ("true");
        res = &BoolDef;
        break;

     case FALSE_:
        ans = NewStr ("false");
        res = &BoolDef;
        break;

     case NULLPTR_:
        ans = NewStr ("nullptr");
        res = &NullPtrDef;
        break;

     case THIS_:
        if (MethodClass == NULL) Error ("this is allowed only in methods and constructors");
        ans = NewStr ("this");
        res = Link (Pointer, MethodClass);
        break;

     case NUM:
        ans = NewStr (NumVal);
        if (u_suffix)
        {
           if (l_suffix) res = &UnsignedLongDef;
                     else res = &UnsignedDef;
        }
        else
        {
           if (l_suffix) res = &LongDef;
                    else res = &IntDef;
        }
        break;

     case FLT:
        ans = NewStr (NumVal);
        if (f_suffix) res = &FloatDef;
        else if (l_suffix) res = &LongDoubleDef;
        else res = &DoubleDef;
        break;

     case STR:
        ans = NewStr (StrVal);
        res = &StringDef;
        break;

     case CHR:
        {   /* C99: a declaration must not directly follow a label */
           char tmp [2];
           tmp [0] = ChrVal;
           tmp [1] = 0;
           ans = NewStr (tmp);
           res = &CharDef;
        }
        break;

     default:
        Error ("Syntax error in expression");
  }

  e = NEW (Expr);
  e->op = sy;
  e->id = ans;
  e->obj = obj;
  e->type = res;

  NextSymbol ();
  return (e);
}

ObjectPtr declarator (TypePtr typ);      /* decl.c */
bool IsDecl (void);

static TypePtr TypeName (void)
/* type name in a cast:  int,  char *,  const T *,  unsigned char */
{
  TypePtr t = declaration_specifiers ();
  ObjectPtr o = declarator (t);
  if (o->name [0] != 0) ErrorId ("Type name expected, found identifier", o->name);
  return o->type;
}

static ExprPtr CastNode (TypePtr type, ExprPtr e)
/* cast node:  op = STATIC_CAST_, type = target type, e1 = expression,
   obj = object without name with the target type (print -p) */
{
  type = Resolve (type);
  if (IsClsSort (type->sort))
     Error ("Cast to structure is not supported");
  if (type->sort == StdString && e->op == COMMA)
  {
     /* string (p, n) - n characters from pointer p,  string (n, c) - n characters c */
     TypePtr a = Resolve (e->e1->type);
     TypePtr b = Resolve (e->e2->type);
     if (e->e1->op == COMMA || b == NULL || !IsIntegerSort (b->sort))
        Error ("string (p, n) or string (n, c) expected");
     bool chars = (a->sort == Pointer || a->sort == Array) && a->from != NULL &&
                  Resolve (a->from)->sort == Char;
     if (!chars && !IsIntegerSort (a->sort))
        Error ("string (p, n) or string (n, c) expected");
  }
  ExprPtr c = NEW (Expr);
  c->op = STATIC_CAST_;
  c->e1 = e;
  c->type = type;
  c->obj = NewObject ((char *) "", type);
  return c;
}

bool IsPolymorphic (TypePtr cls)
/* class with virtual methods (also inherited) */
{
  for (TypePtr t = Resolve (cls); t != NULL; t = t->base)
     for (NodePtr p = t->list.first; p != NULL; p = p->next)
        if (p->item->fce && p->item->Virtual) return true;
  return false;
}

static bool IsBaseOf (TypePtr base, TypePtr cls)
{
  for (TypePtr t = cls; t != NULL; t = t->base)
     if (t == base) return true;
  return false;
}

static ExprPtr DynamicCastNode (TypePtr type, ExprPtr e)
/* DYNAMIC_CAST_ node:  type = T * (result) or T (reference - l-value),
   e1 = expression, obj = object without name with the target type (print -p),
   id = "&" for a reference, "^" for a cast to the base (without a check at runtime) */
{
  type = Resolve (type);
  bool ref = type->sort == Reference;
  TypePtr target = ref ? Resolve (type->from) : (type->sort == Pointer ? Resolve (type->from) : NULL);
  if (target == NULL || !IsClsSort (target->sort))
     Error ("dynamic_cast: pointer or reference to class expected");

  TypePtr src = Resolve (e->type);
  if (!ref)
  {
     if (src->sort != Pointer || !IsClsSort (Resolve (src->from)->sort))
        Error ("dynamic_cast: pointer to class expected");
     src = Resolve (src->from);
  }
  else if (!IsClsSort (src->sort))
     Error ("dynamic_cast: class object expected");

  bool up = IsBaseOf (target, src);
  if (!up && !IsPolymorphic (src))
     Error ("dynamic_cast: source class must be polymorphic");

  ExprPtr c = NEW (Expr);
  c->op = DYNAMIC_CAST_;
  c->e1 = e;
  c->type = ref ? target : type;
  c->obj = NewObject ((char *) "", type);
  c->id = NewStr ((char *) (up ? "^" : ref ? "&" : ""));
  return c;
}

ExprPtr StringConstruct (ExprPtr args)
/* string (args) - direct initialization  string s (p, n); */
{
  return CastNode (NewType (StdString), args);
}

ExprPtr primary_expr (void)
{
  ExprPtr t1;

  /* dynamic_cast <T *> (p),  dynamic_cast <T &> (x) - type check at runtime */
  if (sy==DYNAMIC_CAST_)
  {
     NextSymbol ();
     CheckSymbol (LESS);
     TypePtr type = TypeName ();
     CheckSymbol (GREATER);
     CheckSymbol (LPAR);
     ExprPtr e = expression ();
     CheckSymbol (RPAR);
     return DynamicCastNode (type, e);
  }

  /* static_cast <T> (e),  reinterpret_cast, const_cast */
  if (sy==STATIC_CAST_ || sy==REINTERPRET_CAST_ || sy==CONST_CAST_)
  {
     NextSymbol ();
     CheckSymbol (LESS);
     TypePtr type = TypeName ();
     CheckSymbol (GREATER);
     CheckSymbol (LPAR);
     ExprPtr e = expression ();
     CheckSymbol (RPAR);
     return CastNode (type, e);
  }

  /* functional cast to a type name:  Kind (0),  string (p)  (not classes) */
  if (sy==IDENT && IsDecl () && PeekSymbol () == LPAR)
  {
     ObjectPtr tobj = Lookup (IdentVal);
     TypePtr tt = (tobj != NULL) ? Resolve (tobj->type) : NULL;
     if (tobj == NULL || (tt != NULL && !IsClsSort (tt->sort)))
     {
        TypePtr type = declaration_specifiers ();
        CheckSymbol (LPAR);
        ExprPtr e = expression ();
        CheckSymbol (RPAR);
        return CastNode (type, e);
     }
  }

  if (sy==IDENT ||
      sy==NUM ||
      sy==FLT ||
      sy==STR ||
      sy==CHR ||
      sy==TRUE_ ||
      sy==FALSE_ ||
      sy==NULLPTR_ ||
      sy==THIS_)
  {
     t1 = primary ();
  }
  else if (sy==LPAR)
  {
     NextSymbol ();
     t1 = expression ();
     t1 = calc (t1, NULL, LPAR);
     CheckSymbol (RPAR);
  }
  else if (sy==CHAR_ || sy==INT_ || sy==BOOL_ || sy==LONG_ || sy==SHORT_ ||
           sy==UNSIGNED_ || sy==SIGNED_)
  {
     /* functional cast:  char (n),  int (c) */
     TypePtr type = declaration_specifiers ();
     CheckSymbol (LPAR);
     ExprPtr e = expression ();
     CheckSymbol (RPAR);
     t1 = CastNode (type, e);
  }
  else Error ("Syntax error in expression");

  return (t1);
}

ExprPtr postfix_tail (ExprPtr t1);

ExprPtr postfix_expr (void)
{
  return postfix_tail (primary_expr ());
}

ExprPtr postfix_tail (ExprPtr t1)
/* postfix operators following expression t1:  [ ]  ( )  .  ->  ++  -- */
{
  ExprPtr t2;

  for (;;)
     if (sy==LBRACK)
     {
        NextSymbol ();
        t2 = expression ();
        t1 = calc (t1, t2, LBRACK);
        CheckSymbol (RBRACK);
     }

     else if (sy==LPAR)
     {
        NextSymbol ();
        /* argument-expression-list */
        if (sy==RPAR)
           t2 = NULL;
        else
           t2 = expression ();
        CheckSymbol (RPAR);
        t1 = calc (t1, t2, FCE);
     }

     else if (sy==PERIOD)
     {
        IDENT_PTR id;
        NextSymbol ();
        id = ReadIdentPtr ();
        t1 = calci (t1, id, PERIOD);
     }

     else if (sy==ARROW)
     {
        IDENT_PTR id;
        NextSymbol ();
        id = ReadIdentPtr ();
        t1 = calci (t1, id, ARROW);
     }

     else if (sy==DPLUS || sy==DMINUS)
     {
        t1 = calc (t1, NULL, sy);
        NextSymbol ();
     }
     else break;

  return (t1);
 }

ExprPtr cast_expr (void);

ExprPtr unary_expr (void)
{
  ExprPtr t1;
  SYMBOL op;

  if (sy==DPLUS || sy==DMINUS)
  {
     op = sy;
     NextSymbol ();
     t1 = unary_expr ();
     t1 = calc (NULL, t1, op);
  }

  else if (sy==PLUS)
  {
     NextSymbol ();
     t1 = cast_expr ();
     t1 = calc (NULL, t1, UNARYPLUS);
  }

  else if (sy==MINUS)
  {
     NextSymbol ();
     t1 = cast_expr ();
     t1 = calc (NULL, t1, UNARYMINUS);
  }

  else if (sy==LOGNOT)                         /* ! */
  {
     NextSymbol ();
     t1 = cast_expr ();
     t1 = calc (NULL, t1, LOGNOT);
  }

  else if (sy==TILDA)                          /* ~ */
  {
     NextSymbol ();
     t1 = cast_expr ();
     t1 = calc (NULL, t1, UNARYNOT);
  }

  else if (sy==AND)
  {
     NextSymbol ();
     t1 = cast_expr ();
     t1 = calc (NULL, t1, ADR);
  }

  else if (sy==ASTERISK)
  {
     NextSymbol ();
     t1 = cast_expr ();
     if (t1->type->sort!=Pointer) Error ("Invalid indirection");
     t1 = calc (NULL, t1, DEREF);
  }

  else if (sy==NEW_)                           /* new T, new T (args) */
  {
     NextSymbol ();
     TypePtr t = declaration_specifiers ();
     while (sy==ASTERISK)                    /* new T * [n] */
     {
        NextSymbol ();
        t = Link (Pointer, t);
     }
     ExprPtr args = NULL;
     ExprPtr count = NULL;
     if (sy==LBRACK)                         /* new T [n] - array */
     {
        NextSymbol ();
        count = expression ();
        CheckAssign (&LongDef, count->type);
        CheckSymbol (RBRACK);
     }
     else if (sy==LPAR)
     {
        NextSymbol ();
        if (sy!=RPAR) args = expression ();
        CheckSymbol (RPAR);
     }

     const char * name = "?";
     if (t->tag != NULL) name = t->tag;
     else if (t->sort == Int) name = "int";
     else if (t->sort == Char) name = "char";
     else if (t->sort == Bool) name = "bool";

     t1 = NEW (Expr);
     t1->op = NEW_;
     t1->id = NewStr ((char *) name);
     t1->e1 = args;
     t1->e2 = count;
     t1->type = Link (Pointer, t);
  }

  else if (sy==DELETE_)                        /* delete p,  delete [] p */
  {
     NextSymbol ();
     bool arr = false;
     if (sy==LBRACK)
     {
        NextSymbol ();
        CheckSymbol (RBRACK);
        arr = true;
     }
     ExprPtr p = cast_expr ();
     if (p->type->sort != Pointer) Error ("Pointer expected after delete");
     t1 = NEW (Expr);
     t1->op = DELETE_;
     t1->id = NewStr ((char *) (arr ? "[]" : ""));   /* delete [] p */
     t1->e2 = p;
     t1->type = &VoidDef;
  }

  else if (sy==SIZEOF_)
  {
     /* sizeof (T),  sizeof expression;  node SIZEOF_: e2 = expression, or obj = object
        without name with type T (e2 == NULL) */
     NextSymbol ();
     if (sy==LPAR)
     {
        NextSymbol ();
        if (IsDecl ())
        {
           TypePtr type = TypeName ();
           CheckSymbol (RPAR);
           t1 = NEW (Expr);
           t1->op = SIZEOF_;
           t1->obj = NewObject ((char *) "", type);
           t1->type = &UnsignedLongDef;
           return t1;
        }
        t1 = expression ();
        t1 = calc (t1, NULL, LPAR);
        CheckSymbol (RPAR);
        t1 = postfix_tail (t1);
     }
     else
        t1 = unary_expr ();
     t1 = calc (NULL, t1, SIZEOF_);
     /*
     if (sy==LPAR)
     {
        NextSymbol ();
        !!!
        CheckSymbol (RPAR);
     }
     */
  }

  else t1 = postfix_expr ();

  return (t1);
}

ExprPtr cast_expr (void)
{
  /* (T) e - cast, otherwise (e) and postfix operators */
  if (sy==LPAR)
  {
     NextSymbol ();
     if (IsDecl ())
     {
        TypePtr type = TypeName ();
        CheckSymbol (RPAR);
        return CastNode (type, cast_expr ());
     }
     ExprPtr t1 = expression ();
     t1 = calc (t1, NULL, LPAR);
     CheckSymbol (RPAR);
     return postfix_tail (t1);
  }
  return unary_expr ();
}

ExprPtr mul_expr (void)
{
  ExprPtr t1, t2;
  SYMBOL op;

  t1 = cast_expr ();
  while (sy==ASTERISK ||
         sy==SLASH ||
         sy==MOD)
  {
     op = sy;
     NextSymbol ();
     t2 = cast_expr ();
     t1 = calc (t1, t2, op);
  }
  return (t1);
}

ExprPtr add_expr (void)
{
  ExprPtr t1, t2;
  SYMBOL op;

  t1 = mul_expr ();
  while (sy==PLUS ||
         sy==MINUS)
  {
     op = sy;
     NextSymbol ();
     t2 = mul_expr ();
     t1 = calc (t1, t2, op);
  }

  return (t1);
}

/* output cout / cerr => libc:  printf (fmt, v)  stdout (same buffer as printf),
   dprintf (2, fmt, v)  stderr.  The format string is a literal (STR). */
static ExprPtr OutCall (bool err, char * fmt, ExprPtr value)
{
   ObjectPtr obj;
   ExprPtr lit, args, fn, call;

   obj = Lookup (err ? "dprintf" : "printf");
   if (obj == NULL) obj = ImplicitFunction (err ? "dprintf" : "printf");

   lit = NEW (Expr);
   lit->op = STR;
   lit->id = NewStr (fmt);
   lit->type = &StringDef;

   args = (value == NULL) ? lit : calc (lit, value, COMMA);
   if (err) args = calc (NumConst ("2", &IntDef), args, COMMA);

   fn = NEW (Expr);
   fn->op = IDENT;
   fn->id = obj->name;
   fn->obj = obj;
   fn->type = obj->type;

   call = NEW (Expr);
   call->op = FCE;
   call->e1 = fn;
   call->e2 = args;
   call->type = &IntDef;

   if (err)     /* cerr is tied to cout (cerr.tie): first stdout is flushed => fflush (0) */
   {
      ExprPtr fl, fn2;
      ObjectPtr fo = Lookup ("fflush");
      if (fo == NULL) fo = ImplicitFunction ("fflush");
      fn2 = NEW (Expr);
      fn2->op = IDENT;
      fn2->id = fo->name;
      fn2->obj = fo;
      fn2->type = fo->type;
      fl = NEW (Expr);
      fl->op = FCE;
      fl->e1 = fn2;
      fl->e2 = NumConst ("0", &IntDef);
      fl->type = &IntDef;
      call = calc (fl, call, COMMA);
   }
   return call;
}

/* one element  cout << t2  (type of the operand determines the printf format) */
static ExprPtr StreamCall (bool err, ExprPtr t2)
{
   TypePtr t;

   if (IsStreamMarker (t2))
   {
      if (strcmp (t2->id, "endl") != 0) Error ("Only endl can follow << on cout / cerr");
      return OutCall (err, "\n", NULL);
   }

   t = Resolve (t2->type);
   if (IsStdString (t) || IsCharPtrT (t))
      return OutCall (err, "%s", t2);
   if (t->sort == Char)
      return OutCall (err, "%c", CastNode (&IntDef, t2));
   if (IsFloat (t))
      return OutCall (err, "%g", CastNode (&DoubleDef, t2));     /* as cout: precision 6 */
   if (IsInteger (t) && isUnsignedType (t))
      return OutCall (err, "%lu", CastNode (&UnsignedLongDef, t2));
   if (IsInteger (t))
      return OutCall (err, "%ld", CastNode (&LongDef, t2));

   Error ("Unsupported operand type for cout / cerr");
   return NULL;
}

/* cout << a << b << endl;  =>  printf (...), printf (...), ...  (COMMA chain of calls) */
static ExprPtr StreamOut (ExprPtr t1)
{
   ExprPtr out, t2, call;
   bool err;

   err = strcmp (t1->id, "cerr") == 0;
   out = NULL;
   if (sy != SHL) Error ("Expected << after cout / cerr / endl");
   while (sy==SHL)
   {
      NextSymbol ();
      t2 = add_expr ();
      call = StreamCall (err, t2);
      out = (out == NULL) ? call : calc (out, call, COMMA);
   }
   return out;
}

ExprPtr shift_expr (void)
{
  ExprPtr t1, t2;
  SYMBOL op;

  t1 = add_expr ();
  if (IsStreamMarker (t1)) return StreamOut (t1);     /* cout << ... */
  while (sy==SHL ||
         sy==SHR)
  {
     op = sy;
     NextSymbol ();
     t2 = add_expr ();
     t1 = calc (t1, t2, op);
  }

  return (t1);
}

ExprPtr rel_expr (void)
{
  ExprPtr t1, t2;
  SYMBOL op;

  t1 = shift_expr ();
  while (sy==LESS ||
         sy==GREATER ||
         sy==LESSEQUAL ||
         sy==GREATEREQUAL)
  {
     op = sy;
     NextSymbol ();
     t2 = shift_expr ();
     t1 = calc (t1, t2, op);
  }

  return (t1);
}

ExprPtr equ_expr (void)
{
  ExprPtr t1, t2;
  SYMBOL op;

  t1 = rel_expr ();
  while (sy==EQUAL ||
         sy==UNEQUAL)
  {
     op = sy;
     NextSymbol ();
     t2 = rel_expr ();
     t1 = calc (t1, t2, op);
  }

  return (t1);
}

ExprPtr and_expr (void)
{
  ExprPtr t1, t2;

  t1 = equ_expr ();
  while (sy==AND)
  {
     NextSymbol ();
     t2 = equ_expr ();
     t1 = calc (t1, t2, AND);
  }

  return (t1);
}

ExprPtr xor_expr (void)
{
  ExprPtr t1, t2;

  t1 = and_expr ();
  while (sy==XOR)
  {
     NextSymbol ();
     t2 = and_expr ();
     t1 = calc (t1, t2, XOR);
  }

  return (t1);
}

ExprPtr or_expr (void)
{
  ExprPtr t1, t2;

  t1 = xor_expr ();
  while (sy==OR)
  {
     NextSymbol ();
     t2 = xor_expr ();
     t1 = calc (t1, t2, OR);
  }

  return (t1);
}

ExprPtr land_expr (void)
{
  ExprPtr t1, t2;

  t1 = or_expr ();
  while (sy==LOGAND)
  {
     NextSymbol ();
     t2 = or_expr ();
     t1 = calc (t1, t2, LOGAND);
  }

  return (t1);
}

ExprPtr lor_expr (void)
{
  ExprPtr t1, t2;

  t1 = land_expr ();
  while (sy==LOGOR)
  {
     NextSymbol ();
     t2 = land_expr ();
     t1 = calc (t1, t2, LOGOR);
  }

  return (t1);
}

ExprPtr cond_expr (void)
{
  ExprPtr t1, t2, t3;

  t1 = lor_expr ();
  if (sy==QUESTION)
  {
     NextSymbol ();
     t2 = expression ();
     CheckSymbol (COLON);
     t3 = cond_expr ();
     t1 = calc3 (t1, t2, t3, QUESTION);
  }

  return (t1);
}

ExprPtr const_expr (void)
{
  /* !!! */
  return cond_expr ();
}

ExprPtr simple_expr (void)
{
  return cond_expr ();
}

ExprPtr assignment_expr (void)
{
  ExprPtr t1, t2;
  SYMBOL op;

  t1 = cond_expr ();
  if (sy==ASSIGN ||
         sy==MULASSIGN ||
         sy==DIVASSIGN ||
         sy==MODASSIGN ||
         sy==ADDASSIGN ||
         sy==SUBASSIGN ||
         sy==SHLASSIGN ||
         sy==SHRASSIGN ||
         sy==ANDASSIGN ||
         sy==XORASSIGN ||
         sy==ORASSIGN)
  {
     op = sy;
     NextSymbol ();
     t2 = assignment_expr ();   /* a = b = c  =>  a = (b = c) */
     t1 = calc (t1, t2, op);
  }

  return (t1);
}

ExprPtr expression (void)
{
  ExprPtr t1, t2;

  t1 = assignment_expr ();
  if (sy==COMMA)
  {
     NextSymbol ();
     t2 = expression ();
     t1 = calc (t1, t2, COMMA);
  }

  return (t1);
}

ExprPtr initializer (void)
{
  ExprPtr t1, t2;

  if (sy==LBRACE)
  {
     NextSymbol ();

     /* initializer_list */
     t1 = initializer ();

     while (sy==COMMA)
     {
        NextSymbol ();
        if (sy==RBRACE) break;          /* comma after the last element:  { a, b, } */
        t2 = initializer ();
        t1 = calc (t1, t2, COMMA);
     }

     t1 = calc (t1, NULL, LBRACE);
     CheckSymbol (RBRACE);
  }
  else
  {
     t1 = assignment_expr ();
  }

  return (t1);
}

/******************************** PUT EXPR ********************************/

void PutInit (ExprPtr t)
{
  switch (t->op)
  {
     case LBRACE:
        PutChr ('{');
        PutInit (t->e1);
        PutChr ('}');
        break;

     case COMMA:
        PutInit (t->e1);

        PutChr (',');
        PutChr (' ');

        PutInit (t->e2);
        break;

     default:
        PutExpr (t);
  }
}

void PutString (char * txt, char quote)
{
   PutChr (quote);

   for (;;)
   {
      char c = *(txt++);
      if (c==0) break;

      if (c>=0 && c<32)
         switch (c)
         {
            case '\a': PutChr (BACKSLASH); PutChr ('a'); break;
            case '\b': PutChr (BACKSLASH); PutChr ('b'); break;
            case '\f': PutChr (BACKSLASH); PutChr ('f'); break;
            case '\n': PutChr (BACKSLASH); PutChr ('n'); break;
            case '\r': PutChr (BACKSLASH); PutChr ('r'); break;
            case '\t': PutChr (BACKSLASH); PutChr ('t'); break;
            case '\v': PutChr (BACKSLASH); PutChr ('v'); break;

            default:
               PutChr (BACKSLASH);
               PutChr ((char) ('0' + c/64));
               PutChr ((char) ('0' + ((c/8) & 7)));
               PutChr ((char) ('0' + (c & 7)));
               break;
         }
      else
      {
         if (c==QUOTE1 || c==QUOTE2 || c==BACKSLASH) PutChr (BACKSLASH);
         PutChr (c);
      }
   }

   PutChr (quote);
}

void PutExpr (ExprPtr t)
{
  if (t==NULL) return;

  switch (t->op)
  {
     case IDENT:
        /* static member or method of a class:  T::x  (valid also inside the class) */
        if (t->obj != NULL && t->obj->owner != NULL && t->obj->owner->tag != NULL &&
            ((!t->obj->fce && t->obj->storage == Static) || (t->obj->fce && !t->obj->method &&
                                                              t->obj->owner->ctor != t->obj)))
        {
           PutStr (t->obj->owner->tag);
           PutStr ("::");
        }
        PutStr (t->id);
        break;

     case NUM:
        PutStr (t->id);
        if (t->type != NULL && t->type->sort == Int &&   /* suffix:  10U  10L  10UL */
            strchr ("uUlL", t->id [strlen (t->id) - 1]) == NULL)   /* (lexer.c leaves it in the text) */
        {
           if (t->type->sign == Unsigned) PutChr ('U');
           if (t->type->length == Long) PutChr ('L');
        }
        break;

     case FLT:
     case TRUE_:
     case FALSE_:
     case NULLPTR_:
     case THIS_:
        PutStr (t->id);
        break;

     case DCOLON:                 /* T::f */
        if (t->obj != NULL && t->obj->owner != NULL && t->obj->owner->tag != NULL)
        {
           PutStr (t->obj->owner->tag);
           PutStr ("::");
        }
        PutStr (t->id);
        break;

     case CHR:
        PutString (t->id, QUOTE1);
        break;

     case STR:
        PutString (t->id, QUOTE2);
        break;

     case LPAR:
        PutChr ('(');
        PutExpr (t->e1);
        PutChr (')');
        break;

     case DELETE_:
        PutStr ("delete ");
        if (t->id != NULL && t->id [0] != 0) PutStr ("[] ");
        PutExpr (t->e2);
        break;

     case SIZEOF_:
        PutStr ("sizeof (");
        if (t->e2 == NULL)
           PutTypeName (t->obj->type);   /* sizeof (T) */
        else
           PutExpr (t->e2);
        PutChr (')');
        break;

     case DYNAMIC_CAST_:
        PutStr ("dynamic_cast <");
        PutTypeName (t->obj->type);
        PutStr ("> (");
        PutExpr (t->e1);
        PutChr (')');
        break;

     case STATIC_CAST_:            /* (T) e  =>  static_cast <T> (e) */
        if (t->type != NULL && t->type->sort == StdString)
        {
           /* string (1, c),  string (p),  string (p, n),  string (n, c) */
           bool chr = t->e1->op != COMMA && t->e1->type != NULL && IsIntegerSort (t->e1->type->sort);
           PutStr (chr ? "string (1, " : "string (");
           PutExpr (t->e1);
           PutChr (')');
           break;
        }
        PutStr ("static_cast <");
        PutTypeName (t->type);
        PutStr ("> (");
        PutExpr (t->e1);
        PutChr (')');
        break;

     case LBRACK:
        PutExpr (t->e1);
        PutChr ('[');
        PutExpr (t->e2);
        PutChr (']');
        break;

     case LBRACE:
        PutInit (t);
        break;

     case FCE:
        PutExpr (t->e1);
        PutStr (" (");
        PutExpr (t->e2);
        PutChr (')');
        break;

     case PERIOD:
        PutExpr (t->e1);
        PutChr ('.');
        PutStr (t->id);
        break;

     case ARROW:
        PutExpr (t->e1);
        PutStr ("->");
        PutStr (t->id);
        break;

     case ADR:
        PutChr ('&');
        PutExpr (t->e2);
        break;

     case NEW_:
        PutStr ("new ");
        PutTypeName (t->type->from);   /* type:  new T *  [n] */
        if (t->e2 != NULL)
        {
           PutStr (" [");
           PutExpr (t->e2);
           PutChr (']');
        }
        else if (t->e1 != NULL)
        {
           PutStr (" (");
           PutExpr (t->e1);
           PutChr (')');
        }
        break;

     case DEREF:
        PutChr ('*');
        PutExpr (t->e2);
        break;

     case UNARYPLUS:
        PutChr ('+');
        PutExpr (t->e2);
        break;

     case UNARYMINUS:
        PutChr ('-');
        PutExpr (t->e2);
        break;

     case UNARYNOT:
        PutChr ('~');
        PutExpr (t->e2);
        break;

     case DPLUS:
     case DMINUS:
        if (t->e1==NULL) PutChr (' ');
        PutExpr (t->e1);
        PutSymbol (t->op);
        PutExpr (t->e2);
        break;

     case QUESTION:
        PutExpr (t->e1);
        PutChr ('?');
        PutExpr (t->e2);
        PutChr (':');
        PutExpr (t->e3);
        break;

     case COMMA:
        PutExpr (t->e1);
        PutStr (", ");
        PutExpr (t->e2);
        break;

     case ASSIGN:
        PutExpr (t->e1);
        PutSymbol (t->op);
        PutExpr (t->e2);
        break;

     case LOGAND:
     case LOGOR:
        PutExpr (t->e1);
        PutChr (' ');
        PutSymbol (t->op);
        PutChr (' ');
        PutExpr (t->e2);
        break;

     case LOGNOT:
        PutSymbol (t->op);
        PutExpr (t->e2);
        break;

     default:
        PutExpr (t->e1);
        PutSymbol (t->op);
        PutExpr (t->e2);
        break;
  }
}
