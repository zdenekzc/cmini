
/* STAT.C */

#include <string.h>

#include "decl.h"

/******************************* STATEMENTS *******************************/

ExprPtr BoolExpr (void)
{
  ExprPtr e;
  e = expression ();
  CheckOperand (e->type);     /* cele cislo, float, double, ukazatel */
  return e;
}

ExprPtr IntExpr (void)
{
  ExprPtr e;
  e = expression ();
  CheckInteger (e->type);
  return e;
}

ExprPtr StringExpr (void)
{
  ExprPtr e;
  e = expression ();
  CheckString (e->type);
  return e;
}

ExprPtr CallExpr (void)
{
  ExprPtr e;
  SYMBOL op;

  e = expression ();
  op = e->op;

  if (op != DPLUS &&
      op != DMINUS &&
      op != FCE &&
      op != ASSIGN &&
      op != MULASSIGN &&
      op != DIVASSIGN &&
      op != MODASSIGN &&
      op != ADDASSIGN &&
      op != SUBASSIGN &&
      op != SHLASSIGN &&
      op != SHRASSIGN &&
      op != ANDASSIGN &&
      op != XORASSIGN &&
      op != ORASSIGN)
      Error ("Strange statement");

  return e;
}

/* ---------------------------------------------------------------------- */

/* deklarace v podmince:  if (T x = e), while (T x = e), switch (T x = e) - jedna
   promenna s inicializaci; vytvori blok { T x = e; } s vlastnim rozsahem (otevreny -
   zavre ho volajici po vnitrnim prikazu) a do *cond da vyraz x */
/* podminka zacina deklaraci (ne funkcni pretypovani string (x), T::f ()) */
static bool IsCondDecl (void)
{
  return IsDecl () && ! IsQualifiedExpr () && PeekSymbol () != LPAR;
}

static StatPtr CondDecl (ExprPtr * cond)
{
  StatPtr blk = NEW (Stat);
  blk->st = LBRACE;
  OpenScope (&blk->scope);
  StatPtr d = NEW (Stat);
  d->st = DECL;
  declaration (&d->decl, Init | CondInit);
  if (d->decl.first == NULL || d->decl.first != d->decl.last || d->decl.first->item->init == NULL)
     Error ("One initialized variable expected in condition");
  ObjectPtr var = d->decl.first->item;
  ExprPtr e = NEW (Expr);
  e->op = IDENT;
  e->id = var->name;
  e->obj = var;
  e->type = var->type;
  *cond = e;
  blk->s1 = d;
  return blk;
}

StatPtr statement (void)
{
  StatPtr s = NULL;

  if (sy==GOTO_)                               /* goto */
  {
     NextSymbol ();
     s = NEW (Stat);
     s->st = GOTO_;
     s->id = ReadIdentPtr ();
     /* !! kontrola navesti */
     CheckSymbol (SEMICOLON);
  }

  else if (sy==CONTINUE_)                      /* continue */
  {
     NextSymbol ();
     s = NEW (Stat);
     s->st = CONTINUE_;
     CheckSymbol (SEMICOLON);
  }

  else if (sy==BREAK_)                         /* break */
  {
     NextSymbol ();
     s = NEW (Stat);
     s->st = BREAK_;
     CheckSymbol (SEMICOLON);
  }

  else if (sy==RETURN_)                        /* return */
  {
     NextSymbol ();
     s = NEW (Stat);
     s->st = RETURN_;

     if (sy!=SEMICOLON)
     {
        ExprPtr e;
        e = expression ();
        CheckAssign (Fce->type->from, e->type);
        s->e1 = e;
     }

     CheckSymbol (SEMICOLON);
  }

  else if (sy==WHILE_)                         /* while */
  {
     NextSymbol ();
     s = NEW (Stat);
     s->st = WHILE_;

     CheckSymbol (LPAR);
     if (IsCondDecl ())
     {
        /* while (T x = e) S  =>  for (;;) { T x = e; if (x) S else break; } */
        ExprPtr c;
        StatPtr blk = CondDecl (&c);
        CheckOperand (c->type);
        CheckSymbol (RPAR);
        StatPtr i = NEW (Stat);
        i->st = IF_;
        i->e1 = c;
        i->s1 = statement ();
        i->s2 = NEW (Stat);
        i->s2->st = BREAK_;
        blk->s1->next = i;
        CloseScope ();
        s->st = FOR_;
        s->s1 = blk;
        return s;
     }
     s->e1 = BoolExpr ();
     CheckSymbol (RPAR);

     s->s1 = statement ();
  }

  else if (sy==DO_)                            /* do */
  {
     NextSymbol ();
     s = NEW (Stat);
     s->st = DO_;
     s->s1 = statement ();

     CheckSymbol (WHILE_);
     CheckSymbol (LPAR);
     s->e1 = BoolExpr ();
     CheckSymbol (RPAR);
     CheckSymbol (SEMICOLON);
  }

  else if (sy==FOR_)                           /* for */
  {
     NextSymbol ();
     s = NEW (Stat);
     s->st = FOR_;

     CheckSymbol (LPAR);
     OpenScope (&s->scope);   /* for (int i = ...) */

     if (IsDecl ())
     {
        RangeForDecl = false;
        declaration (&s->decl, Init | RangeFor); /* vcetne stredniku */
        if (RangeForDecl)
        {
           /* for (T x : seq),  for (T & x : seq)  -  s->st = COLON,
              decl = promenna cyklu, e1 = posloupnost, s1 = telo */
           RangeForDecl = false;
           NextSymbol ();                     /* : */
           s->st = COLON;
           s->e1 = expression ();
           TypePtr ct = Resolve (s->e1->type);
           if (ct->sort == Reference) ct = Resolve (ct->from);
           if (ct->sort != StdSequence)
              Error ("Sequence expected in for (T x : ...)");
           if (s->decl.first == NULL || s->decl.first != s->decl.last)
              Error ("One variable expected in for (T x : ...)");
           ObjectPtr var = s->decl.first->item;
           TypePtr vt = var->type;
           if (vt->sort == Reference)
           {
              if (! SameType (vt->from, ct->elem))
                 Error ("Reference to element type expected in for (T & x : ...)");
           }
           else
              CheckAssign (vt, ct->elem);
           CheckSymbol (RPAR);
           s->s1 = statement ();
           CloseScope ();
           return s;
        }
     }
     else
     {
        if (sy!=SEMICOLON) s->e1 = expression ();
        CheckSymbol (SEMICOLON);
     }

     if (sy!=SEMICOLON) s->e2 = BoolExpr ();
     CheckSymbol (SEMICOLON);

     if (sy!=RPAR) s->e3 = expression ();
     CheckSymbol (RPAR);

     s->s1 = statement ();
     CloseScope ();
  }

  else if (sy==IF_)                            /* if */
  {
     NextSymbol ();
     s = NEW (Stat);
     s->st = IF_;

     CheckSymbol (LPAR);
     StatPtr blk = NULL;
     if (IsCondDecl ())
     {
        /* if (T x = e) S1 else S2  =>  { T x = e; if (x) S1 else S2 } */
        blk = CondDecl (&s->e1);
        CheckOperand (s->e1->type);
     }
     else
        s->e1 = BoolExpr ();
     CheckSymbol (RPAR);

     s->s1 = statement ();

     if (sy==ELSE_)
     {
        NextSymbol ();
        s->s2 = statement ();
     }
     if (blk != NULL)
     {
        blk->s1->next = s;
        CloseScope ();
        s = blk;
     }
  }

  else if (sy==SWITCH_)                        /* switch */
  {
     NextSymbol ();
     s = NEW (Stat);
     s->st = SWITCH_;

     CheckSymbol (LPAR);
     StatPtr blk = NULL;
     if (IsCondDecl ())
     {
        /* switch (T x = e) S  =>  { T x = e; switch (x) S } */
        blk = CondDecl (&s->e1);
        CheckInteger (s->e1->type);
     }
     else
        s->e1 = IntExpr ();
     CheckSymbol (RPAR);

     s->s1 = statement ();
     if (blk != NULL)
     {
        blk->s1->next = s;
        CloseScope ();
        s = blk;
     }
  }

  else if (sy==LBRACE)                         /* { } */
  {
     StatPtr p, first, last;

     NextSymbol ();
     s = NEW (Stat);
     s->st = LBRACE;
     OpenScope (&s->scope);

     first = NULL;
     last = NULL;
     while (sy!=RBRACE)
     {
        p = statement ();

        /* pripojeni do fronty */
        if (first==NULL)
           first = p;
        else
           last->next = p;

        last = p;
        Check (p->next == NULL);
     }

     s->s1 = first;
     CloseScope ();
     NextSymbol ();
  }

  else if (sy==CASE_)                          /* case */
  {
     NextSymbol ();
     s = NEW (Stat);
     s->st = CASE_;
     s->e1 = const_expr ();
     CheckSymbol (COLON);
  }

  else if (sy==DEFAULT_)                       /* default */
  {
     NextSymbol ();
     s = NEW (Stat);
     s->st = DEFAULT_;
     CheckSymbol (COLON);
  }

  else if (IsDecl () && ! IsQualifiedExpr ())  /* declaration (ne  Base::f ();) */
  {
     s = NEW (Stat);
     s->st = DECL;
     declaration (&s->decl, Init);
  }

  else if (sy==IDENT && PeekSymbol () == COLON)  /* navesti:  again: ... */
  {
     s = NEW (Stat);
     s->st = LAB;
     s->id = ReadIdentPtr ();
     CheckSymbol (COLON);
     if (sy==RBRACE)
        s->s1 = NULL;                          /* navesti na konci bloku */
     else
        s->s1 = statement ();
  }

  else                                         /* expression */
  {
     s = NEW (Stat);
     s->st = SIM;
     if (sy != SEMICOLON)
        s->e1 = expression ();
     CheckSymbol (SEMICOLON);
  }

  return s;
}

/***************************** PUT STATEMENT ******************************/

void SubStat (StatPtr s)
{
  Check (s!=NULL);

  if (s->st == LBRACE) /* graficka uprava */
     PutStat (s);
  else
  {
     Indent ();
     PutStat (s);
     Unindent ();
  }
}

void PutStat (StatPtr s)
{
  Check (s!=NULL);

  switch (s->st)
  {
     case GOTO_:
        PutStr ("goto ");
        PutStr (s->id);
        PutStr (";");
        PutEol ();
        break;

     case CONTINUE_:
        PutStr ("continue;");
        PutEol ();
        break;

     case BREAK_:
        PutStr ("break;");
        PutEol ();
        break;

     case RETURN_:
        PutStr ("return");
        if (s->e1 != NULL)
        {
           PutChr (' ');
           PutExpr (s->e1);
        }
        PutChr (';');
        PutEol ();
        break;

     case WHILE_:
        PutStr ("while (");
        PutExpr (s->e1);
        PutStr (")");
        PutEol ();
        SubStat (s->s1);
        break;

     case DO_:
        PutStr ("do");
        PutEol ();
        SubStat (s->s1);
        PutStr ("while (");
        PutExpr (s->e1);
        PutStr (");");
        PutEol ();
        break;

     case COLON:                       /* for (T x : seq) */
        PutStr ("for (");
        PutParamDecl (s->decl.first->item);
        PutStr (" : ");
        PutExpr (s->e1);
        PutStr (")");
        PutEol ();
        SubStat (s->s1);
        break;

     case FOR_:
        PutStr ("for (");
        for (NodePtr p = s->decl.first; p != NULL; p = p->next)
        {
           PutParamDecl (p->item);
           if (p->next != NULL) PutStr (", ");
        }
        if (s->e1!=NULL) PutExpr (s->e1);
        PutStr ("; ");
        if (s->e2!=NULL) PutExpr (s->e2);
        PutStr ("; ");
        if (s->e3!=NULL) PutExpr (s->e3);
        PutStr (")");
        PutEol ();
        SubStat (s->s1);
        break;

     case IF_:
        PutStr ("if (");
        PutExpr (s->e1);
        PutStr (")");
        PutEol ();

        SubStat (s->s1);

        if (s->s2 != NULL)
        {
           StatPtr t;
           t = s->s2;

           if (t->st == IF_) /* graficka uprava */
           {
              PutStr ("else ");
              PutStat (t);
           }
           else
           {
              PutStr ("else");
              PutEol ();
              SubStat (t);
           }
        }
        break;

     case SWITCH_:
        PutStr ("switch (");
        PutExpr (s->e1);
        PutStr (")");
        PutEol ();
        SubStat (s->s1);
        break;

     case LBRACE:
        {
           StatPtr p;

           PutChr ('{');
           PutEol ();
           Indent ();

           for (p=s->s1; p!=NULL; p=p->next)
              PutStat (p);

           Unindent ();
           PutChr ('}');
           PutEol ();
        }
        break;

     case CASE_:
        PutStr ("case ");
        PutExpr (s->e1);
        PutChr (':');
        PutEol ();
        break;

     case DEFAULT_:
        PutStr ("default:");
        PutEol ();
        break;

     case LAB:
        PutStr (s->id);
        PutChr (':');
        if (s->s1 != NULL) PutStat (s->s1); else PutChr (';');
        PutEol ();
        break;

     case SIM:
        if (s->e1 != NULL) PutExpr (s->e1);
        PutChr (';');
        PutEol ();
        break;

     case DECL:
        {
           NodePtr p;
           for (p=s->decl.first; p!=NULL; p=p->next)
              PutDecl (p->item);
        }
        break;
  }
}
