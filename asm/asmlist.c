/* ASMLIST.C - program pro assembler pasm jako spojovy seznam (viz asmlist.h) */

#include "asmlist.h"
#include "pasmlib.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

static void * AsmAlloc (size_t size)
{
   void * p = calloc (1, size > 0 ? size : 1);
   if (p == NULL)
   {
      fprintf (stderr, "asmlist: out of memory\n");
      abort ();
   }
   return p;
}

static char * AsmStrDup (const char * s)
{
   char * r = (char *) AsmAlloc (strlen (s) + 1);
   strcpy (r, s);
   return r;
}

/******************************** OPERANDY ********************************/

static AsmOp NoOp (AsmOpKind kind)
{
   AsmOp o;
   memset (&o, 0, sizeof (o));
   o.kind = kind;
   o.reg = None;
   o.index = None;
   o.scale = 1;
   return o;
}

AsmOp AsmReg (Register r)
{
   AsmOp o = NoOp (AO_REG);
   o.reg = r;
   return o;
}

AsmOp AsmXmm (int n)
{
   AsmOp o = NoOp (AO_XMM);
   o.num = n;
   return o;
}

AsmOp AsmImm (int64_t v)
{
   AsmOp o = NoOp (AO_IMM);
   o.val = v;
   return o;
}

AsmOp AsmImmHex (int64_t v)
{
   AsmOp o = AsmImm (v);
   o.num = 1;                 /* tisk 0...h */
   return o;
}

AsmOp AsmImmHexLit (int64_t v)
{
   AsmOp o = AsmImm (v);
   o.num = 3;                 /* tisk ...h (bez uvodni 0, cislo zacina cislici) */
   return o;
}

AsmOp AsmImmU (uint64_t v)
{
   AsmOp o = AsmImm ((int64_t) v);
   o.num = 2;                 /* tisk bez znamenka */
   return o;
}

AsmOp AsmMem (int size, Register base, int64_t disp)
{
   AsmOp o = NoOp (AO_MEM);
   o.size = size;
   o.reg = base;
   o.val = disp;
   return o;
}

AsmOp AsmMemIdx (int size, Register base, Register index, int scale, int64_t disp)
{
   AsmOp o = AsmMem (size, base, disp);
   o.index = index;
   o.scale = scale;
   return o;
}

AsmOp AsmMemLab (int size, AsmLabel * label, int64_t disp)
{
   AsmOp o = NoOp (AO_MEM);
   o.size = size;
   o.label = label;
   o.val = disp;
   return o;
}

AsmOp AsmLab (AsmLabel * label)
{
   AsmOp o = NoOp (AO_LABEL);
   o.label = label;
   return o;
}

AsmOp AsmOffset (AsmLabel * label)
{
   AsmOp o = NoOp (AO_OFFSET);
   o.label = label;
   return o;
}

AsmOp AsmStr (const char * s)
{
   AsmOp o = NoOp (AO_STR);
   o.str = AsmStrDup (s);
   return o;
}

AsmOp AsmDup (int count, int64_t value)
{
   AsmOp o = NoOp (AO_DUP);
   o.num = count;
   o.val = value;
   return o;
}

/********************************* SEZNAM *********************************/

/* operand z rozhrani s ukazateli (AsmPReg, ...) - seznam v programu */
typedef struct OpBlock OpBlock;
struct OpBlock
{
   OpBlock * next;
   AsmOp     op;
};

void AsmInit (AsmProgram * p)
{
   memset (p, 0, sizeof (*p));
}

void AsmFree (AsmProgram * p)
{
   AsmNode * n = p->first;
   while (n != NULL)
   {
      AsmNode * next = n->next;
      free (n->items);
      free (n);
      n = next;
   }
   AsmLabel * l = p->labels;
   while (l != NULL)
   {
      AsmLabel * next = l->next;
      free (l->name);
      free (l);
      l = next;
   }
   OpBlock * b = (OpBlock *) p->op_blocks;
   while (b != NULL)
   {
      OpBlock * next = b->next;
      free (b);
      b = next;
   }
   AsmInit (p);
}

static AsmNode * NewNode (AsmProgram * p, AsmNodeKind kind)
/* novy uzel za kurzor, stane se kurzorem */
{
   AsmNode * n = (AsmNode *) AsmAlloc (sizeof (AsmNode));
   n->kind = kind;
   n->prefix = iOther;

   AsmNode * after = p->cursor;
   n->prev = after;
   n->next = (after != NULL) ? after->next : p->first;
   if (n->prev != NULL) n->prev->next = n; else p->first = n;
   if (n->next != NULL) n->next->prev = n; else p->last = n;

   p->cursor = n;
   p->node_count ++;
   return n;
}

AsmNode * AsmCursor (AsmProgram * p)
{
   return p->cursor;
}

AsmNode * AsmSetCursor (AsmProgram * p, AsmNode * n)
{
   AsmNode * old = p->cursor;
   p->cursor = n;
   return old;
}

void AsmRemove (AsmProgram * p, AsmNode * n)
{
   if (n->prev != NULL) n->prev->next = n->next; else p->first = n->next;
   if (n->next != NULL) n->next->prev = n->prev; else p->last = n->prev;
   if (p->cursor == n) p->cursor = n->prev;
   p->node_count --;
   free (n->items);
   free (n);
}

/********************************* NAVESTI ********************************/

AsmLabel * AsmNamedLabel (AsmProgram * p, const char * name)
{
   AsmLabel * l = (AsmLabel *) AsmAlloc (sizeof (AsmLabel));
   l->name = AsmStrDup (name);
   l->next = p->labels;
   p->labels = l;
   return l;
}

AsmLabel * AsmNewLabel (AsmProgram * p, const char * prefix)
{
   char buf [64];
   snprintf (buf, sizeof (buf), "%s%d", prefix, ++ p->label_count);
   return AsmNamedLabel (p, buf);
}

AsmNode * AsmBind (AsmProgram * p, AsmLabel * label)
{
   AsmNode * n = NewNode (p, AN_LABEL);
   n->label = label;
   return n;
}

/******************************** INSTRUKCE *******************************/

AsmNode * AsmIns (AsmProgram * p, keyword inst, int nops, const AsmOp * ops)
{
   AsmNode * n = NewNode (p, AN_INST);
   n->inst = inst;
   if (nops > ASM_MAX_OPS) nops = ASM_MAX_OPS;
   n->nops = nops;
   for (int i = 0; i < nops; i++) n->op [i] = ops [i];
   return n;
}

AsmNode * AsmIns0 (AsmProgram * p, keyword inst)
{
   return AsmIns (p, inst, 0, NULL);
}

AsmNode * AsmIns1 (AsmProgram * p, keyword inst, AsmOp a)
{
   return AsmIns (p, inst, 1, &a);
}

AsmNode * AsmIns2 (AsmProgram * p, keyword inst, AsmOp a, AsmOp b)
{
   AsmOp ops [2];
   ops [0] = a;
   ops [1] = b;
   return AsmIns (p, inst, 2, ops);
}

AsmNode * AsmIns3 (AsmProgram * p, keyword inst, AsmOp a, AsmOp b, AsmOp c)
{
   AsmOp ops [3];
   ops [0] = a;
   ops [1] = b;
   ops [2] = c;
   return AsmIns (p, inst, 3, ops);
}

AsmNode * AsmPrefixed (AsmProgram * p, keyword prefix, keyword inst)
{
   AsmNode * n = AsmIns0 (p, inst);
   n->prefix = prefix;
   return n;
}

/******************************** DIREKTIVY *******************************/

AsmNode * AsmData (AsmProgram * p, AsmLabel * name, keyword dir, int nitems, const AsmOp * items)
{
   AsmNode * n = NewNode (p, AN_DATA);
   n->inst = dir;
   n->label = name;
   n->nitems = nitems;
   n->items = (AsmOp *) AsmAlloc (nitems * sizeof (AsmOp));
   memcpy (n->items, items, nitems * sizeof (AsmOp));
   return n;
}

AsmNode * AsmEqu (AsmProgram * p, AsmLabel * name, int64_t value)
{
   AsmNode * n = NewNode (p, AN_EQU);
   n->label = name;
   n->value = value;
   return n;
}

AsmNode * AsmDir (AsmProgram * p, keyword dir, AsmLabel * name, const char * text)
{
   AsmNode * n = NewNode (p, AN_DIR);
   n->inst = dir;
   n->label = name;
   n->text = (text != NULL) ? AsmStrDup (text) : NULL;
   return n;
}

AsmNode * AsmAlign (AsmProgram * p, int a)
{
   AsmNode * n = AsmDir (p, iAlign, NULL, NULL);
   n->value = a;
   return n;
}

AsmNode * AsmTextLine (AsmProgram * p, const char * line)
{
   AsmNode * n = NewNode (p, AN_TEXT);
   n->text = AsmStrDup (line);
   return n;
}

AsmNode * AsmCfi (AsmProgram * p, int cfi, Register reg, int64_t value)
{
   AsmNode * n = NewNode (p, AN_CFI);
   n->cfi = cfi;
   n->cfi_reg = reg;
   n->value = value;
   return n;
}

const char * AsmCfiName (int cfi)
{
   switch (cfi)
   {
      case ASM_CFI_STARTPROC:        return ".cfi_startproc";
      case ASM_CFI_ENDPROC:          return ".cfi_endproc";
      case ASM_CFI_DEF_CFA_OFFSET:   return ".cfi_def_cfa_offset";
      case ASM_CFI_DEF_CFA_REGISTER: return ".cfi_def_cfa_register";
      case ASM_CFI_DEF_CFA:          return ".cfi_def_cfa";
      case ASM_CFI_OFFSET:           return ".cfi_offset";
      case ASM_CFI_RESTORE:          return ".cfi_restore";
      case ASM_CFI_REMEMBER_STATE:   return ".cfi_remember_state";
      case ASM_CFI_RESTORE_STATE:    return ".cfi_restore_state";
      default:                       return ".cfi_?";
   }
}

AsmNode * AsmComment (AsmNode * n, const char * comment)
{
   n->comment = (comment != NULL) ? AsmStrDup (comment) : NULL;
   return n;
}

/********************************** TEXT **********************************/

/* text se sklada do pomocneho bufferu (AsmAssemble - casti pro pasm) */

typedef struct
{
   char * text;
   int    len;
   int    cap;
}
   TEXT;

static void TextAdd (TEXT * t, const char * s)
{
   int n = (int) strlen (s);
   if (t->len + n + 1 > t->cap)
   {
      t->cap = 2 * (t->len + n + 1) + 64;
      t->text = (char *) realloc (t->text, t->cap);
      if (t->text == NULL) abort ();
   }
   memcpy (t->text + t->len, s, n + 1);
   t->len += n;
}

/* formatovany text - funkce s pevnymi parametry (ne ...): asmlist.c
   preklada i cparser, ktery funkce s promennym poctem parametru neumi
   definovat (volat printf, snprintf ano) */
static void TextNum (TEXT * t, const char * fmt, long v)      /* jedno cislo: %ld, %lX, %lu */
{
   char buf [512];
   snprintf (buf, sizeof (buf), fmt, v);
   TextAdd (t, buf);
}

static void TextStr (TEXT * t, const char * fmt, const char * a)   /* jeden retezec */
{
   char buf [512];
   snprintf (buf, sizeof (buf), fmt, a);
   TextAdd (t, buf);
}

/* retezec doplneny mezerami na sirku w (delsi zustane cely) - ne printf "%-8s":
   libc ToaruOS 2.3.2 delsi retezec na sirku orizne (init_array => init_ar) */
static void TextPad (TEXT * t, const char * s, int w)
{
   int n = (int) strlen (s);
   TextAdd (t, s);
   for (; n < w; n++) TextAdd (t, " ");
}

static void TextStr2 (TEXT * t, const char * fmt, const char * a, const char * b)
{
   char buf [512];
   snprintf (buf, sizeof (buf), fmt, a, b);
   TextAdd (t, buf);
}

const char * AsmInstName (keyword k)
/* jmeno instrukce nebo direktivy malymi pismeny (KeyTable z key.c) */
{
   static char names [KeyMax + 64][12];
   static bool ready = false;
   if (! ready)
   {
      for (int i = 1; i <= KeyMax; i++)
      {
         int k2 = (int) KeyTable [i].k;
         if (k2 < 0 || k2 >= KeyMax + 64) continue;
         for (int j = 0; j < 11 && KeyTable [i].n [j] != 0; j++)
            names [k2][j] = (char) tolower ((unsigned char) KeyTable [i].n [j]);
      }
      ready = true;
   }
   if ((int) k < 0 || (int) k >= KeyMax + 64) return "?";
   return names [k];
}

const char * AsmRegName (Register r)
{
   static char names [None + 1][8];
   static bool ready = false;
   if (! ready)
   {
      for (int k = 0; k <= (int) None; k++)
      {
         int i = 0;
         for (; RegTable [k][i] != 0 && i < 7; i++)
            names [k][i] = (char) tolower ((unsigned char) RegTable [k][i]);
         names [k][i] = 0;
      }
      ready = true;
   }
   if ((int) r < 0 || (int) r > (int) None) return "?";
   return names [r];
}

static void RegName (TEXT * t, Register r)
{
   char buf [8];
   int i = 0;
   for (; RegTable [r][i] != 0 && i < 7; i++)
      buf [i] = (char) tolower ((unsigned char) RegTable [r][i]);
   buf [i] = 0;
   TextAdd (t, buf);
}

static void Number (TEXT * t, int64_t v)
/* desitkove (pasm cte i 64bitova cisla) */
{
   TextNum (t, "%ld", (long) v);   /* int64_t = long (LP64) - bez long long (cparser) */
}

static void Disp (TEXT * t, int64_t v, bool first)
/* posun v adrese:  +16, -8 */
{
   if (v < 0)
   {
      TextAdd (t, "-");
      Number (t, - v);
   }
   else
   {
      if (! first) TextAdd (t, "+");
      Number (t, v);
   }
}

static const char * SizeName (int size)
{
   switch (size)
   {
      case 1:  return "byte ptr ";
      case 2:  return "word ptr ";
      case 4:  return "dword ptr ";
      case 8:  return "qword ptr ";
      case 10: return "tbyte ptr ";
      default: return "";
   }
}

static void Operand (TEXT * t, const AsmOp * o)
{
   switch (o->kind)
   {
      case AO_REG:
         RegName (t, o->reg);
         break;

      case AO_XMM:
         TextNum (t, "xmm%ld", (long) o->num);
         break;

      case AO_IMM:
         if (o->num == 1) TextNum (t, "0%lXh", (long) o->val);
         else if (o->num == 2) TextNum (t, "%lu", (long) o->val);
         else if (o->num == 3) TextNum (t, "%lXh", (long) o->val);
         else Number (t, o->val);
         break;

      case AO_MEM:
      {
         TextAdd (t, SizeName (o->size));
         if (o->label != NULL && o->reg == None && o->index == None)
         {
            /* navesti:  msg  (lea),  dword ptr [count+4] */
            if (o->size == 0 && o->val == 0)
               TextAdd (t, o->label->name);
            else
            {
               TextAdd (t, "[");
               TextAdd (t, o->label->name);
               if (o->val != 0) Disp (t, o->val, false);
               TextAdd (t, "]");
            }
            break;
         }
         bool first = true;
         TextAdd (t, "[");
         if (o->label != NULL)
         {
            TextAdd (t, o->label->name);
            first = false;
         }
         if (o->reg != None)
         {
            if (! first) TextAdd (t, "+");
            RegName (t, o->reg);
            first = false;
         }
         if (o->index != None)
         {
            if (! first) TextAdd (t, "+");
            RegName (t, o->index);
            if (o->scale != 1) TextNum (t, "*%ld", (long) o->scale);
            first = false;
         }
         if (o->val != 0 || first) Disp (t, o->val, first);
         TextAdd (t, "]");
         break;
      }

      case AO_LABEL:
         TextAdd (t, o->label->name);
         if (o->val != 0) Disp (t, o->val, false);
         break;

      case AO_OFFSET:
         TextAdd (t, "offset ");
         TextAdd (t, o->label->name);
         break;

      case AO_STR:
         TextAdd (t, "'");
         for (const char * s = o->str; *s != 0; s++)
         {
            char c [2] = { *s, 0 };
            if (*s == '\'') TextAdd (t, "'");     /* '' uvnitr retezce */
            TextAdd (t, c);
         }
         TextAdd (t, "'");
         break;

      case AO_DUP:
         TextNum (t, "%ld dup (", (long) o->num);
         Number (t, o->val);
         TextAdd (t, ")");
         break;

      default:
         break;
   }
}

static void Mnemonic (TEXT * t, keyword k)
/* jmeno instrukce na 8 znaku, delsi + 1 mezera (cvtsi2sd xmm0, rax) */
{
   const char * n = AsmInstName (k);
   if (strlen (n) >= 8) TextStr (t, "%s ", n);
   else TextPad (t, n, 8);
}

static void Column (TEXT * t, const char * name)
/* jmeno v prvnim sloupci (8 znaku) */
{
   if (name == NULL) TextAdd (t, "        ");
   else { TextPad (t, name, 7); TextAdd (t, " "); }
}

static void CfiText (TEXT * t, const AsmNode * n)
/* .cfi_offset rbx, -24 */
{
   TextAdd (t, AsmCfiName (n->cfi));
   switch (n->cfi)
   {
      case ASM_CFI_DEF_CFA_OFFSET:
         TextAdd (t, " ");
         Number (t, n->value);
         break;
      case ASM_CFI_DEF_CFA_REGISTER:
      case ASM_CFI_RESTORE:
         TextAdd (t, " ");
         RegName (t, n->cfi_reg);
         break;
      case ASM_CFI_DEF_CFA:
      case ASM_CFI_OFFSET:
         TextAdd (t, " ");
         RegName (t, n->cfi_reg);
         TextAdd (t, ", ");
         Number (t, n->value);
         break;
      default:
         break;
   }
}

int AsmCfiText (const AsmNode * n, char * buf, int size)
{
   TEXT t = { NULL, 0, 0 };
   CfiText (&t, n);
   int len = t.len;
   if (size > 0)
   {
      int k = (len < size - 1) ? len : size - 1;
      if (k > 0) memcpy (buf, t.text, k);
      buf [k] = 0;
   }
   free (t.text);
   return len;
}

static void NodeText (TEXT * t, const AsmNode * n)
/* jeden radek textu bez konce radky */
{
   switch (n->kind)
   {
      case AN_INST:
         Column (t, NULL);
         if (n->prefix != iOther)
            Mnemonic (t, n->prefix);
         if (n->nops == 0)
            TextAdd (t, AsmInstName (n->inst));
         else
            Mnemonic (t, n->inst);
         for (int i = 0; i < n->nops; i++)
         {
            if (i > 0) TextAdd (t, ", ");
            Operand (t, &n->op [i]);
         }
         break;

      case AN_LABEL:
         TextStr (t, "%s:", n->label->name);
         break;

      case AN_DATA:
         Column (t, n->label != NULL ? n->label->name : NULL);
         TextPad (t, AsmInstName (n->inst), 8);
         for (int i = 0; i < n->nitems; i++)
         {
            if (i > 0) TextAdd (t, ", ");
            Operand (t, &n->items [i]);
         }
         break;

      case AN_EQU:
         Column (t, n->label->name);
         TextAdd (t, "equ     ");
         Number (t, n->value);
         break;

      case AN_DIR:
         switch (n->inst)
         {
            case iSegment:
            case iEnds:
            case iProc:
            case iEndp:
               Column (t, n->label->name);
               if (n->text != NULL) { TextPad (t, AsmInstName (n->inst), 8); TextAdd (t, n->text); }
               else TextAdd (t, AsmInstName (n->inst));
               break;

            case iExtrn:          /* extrn  jmeno: near 'printf' */
               Column (t, NULL);
               TextStr2 (t, "%-8s%s:", "extrn", n->label->name);
               if (n->text != NULL) TextStr (t, " %s", n->text);
               break;

            case iPublic:         /* public  jmeno 'main' */
               Column (t, NULL);
               TextStr2 (t, "%-8s%s", "public", n->label->name);
               if (n->text != NULL) TextStr (t, " %s", n->text);
               break;

            case iAlign:          /* align 8,  align qword (text) */
               Column (t, NULL);
               if (n->text != NULL) TextStr2 (t, "%-8s%s", "align", n->text);
               else { TextStr (t, "%-8s", "align"); TextNum (t, "%ld", (long) (int) n->value); }
               break;

            case iEnd:
               Column (t, NULL);
               if (n->label != NULL) TextStr2 (t, "%-8s%s", "end", n->label->name);
               else TextAdd (t, "end");
               break;

            default:              /* .x64 a dalsi bez parametru */
               Column (t, NULL);
               TextAdd (t, AsmInstName (n->inst));
               if (n->text != NULL) TextStr (t, " %s", n->text);
               break;
         }
         break;

      case AN_TEXT:
         TextAdd (t, n->text);
         break;

      case AN_CFI:          /* pasm: poznamka */
         Column (t, NULL);
         TextAdd (t, "; ");
         CfiText (t, n);
         break;
   }

   if (n->comment != NULL)
      TextStr (t, "  ; %s", n->comment);
}

void AsmPrintNode (const AsmNode * n, FILE * f)
{
   TEXT t = { NULL, 0, 0 };
   NodeText (&t, n);
   fprintf (f, "%s\n", t.text != NULL ? t.text : "");
   free (t.text);
}

void AsmPrint (const AsmProgram * p, FILE * f)
{
   for (const AsmNode * n = p->first; n != NULL; n = n->next)
      AsmPrintNode (n, f);
}

AsmNode * AsmNodeAtLine (const AsmProgram * p, int line)
{
   AsmNode * n = p->first;
   for (int i = 1; n != NULL && i < line; i++)
      n = n->next;
   return n;
}

/********************************* PREKLAD ********************************/

int AsmNodeLine (const AsmNode * n, char * buf, int size)
{
   TEXT t = { NULL, 0, 0 };
   NodeText (&t, n);
   int len = t.len;
   if (size > 0)
   {
      int k = (len < size - 1) ? len : size - 1;
      if (k > 0) memcpy (buf, t.text, k);
      buf [k] = 0;
   }
   free (t.text);
   return len;
}

static void SourceName (char * name, int size, const char * base)
/* jmeno pro hlaseni:  base.asm (bez adresare) */
{
   const char * slash = strrchr (base, '/');
   snprintf (name, size, "%s.asm", slash != NULL ? slash + 1 : base);
}

int AsmAssemble (const AsmProgram * p, const char * base, int flags, char * msg, int msg_size)
/* faze 2: pasm cte uzly primo */
{
   char name [256];
   SourceName (name, sizeof (name), base);
   return PasmAssembleList (name, p, base, flags, msg, msg_size);
}

int AsmAssembleText (const AsmProgram * p, const char * base, int flags, char * msg, int msg_size)
/* faze 1: text po radcich (jeden uzel = jedna radka) => PasmAssembleText */
{
   int count = p->node_count;
   const char * * parts = (const char * *) AsmAlloc ((count + 1) * sizeof (char *));
   int i = 0;
   for (const AsmNode * n = p->first; n != NULL && i < count; n = n->next)
   {
      TEXT t = { NULL, 0, 0 };
      NodeText (&t, n);
      TextAdd (&t, "\n");
      parts [i ++] = t.text;
   }

   char name [256];
   SourceName (name, sizeof (name), base);
   int r = PasmAssembleText (name, parts, i, base, flags, msg, msg_size);

   for (int j = 0; j < i; j++) free ((void *) parts [j]);
   free (parts);
   return r;
}

/************************* ROZHRANI S UKAZATELI ***************************/

AsmProgram * AsmCreate (void)
{
   AsmProgram * p = (AsmProgram *) AsmAlloc (sizeof (AsmProgram));
   AsmInit (p);
   return p;
}

void AsmDestroy (AsmProgram * p)
{
   if (p == NULL) return;
   AsmFree (p);
   free (p);
}

static AsmOp * KeepOp (AsmProgram * p, AsmOp o)
{
   OpBlock * b = (OpBlock *) AsmAlloc (sizeof (OpBlock));
   b->op = o;
   b->next = (OpBlock *) p->op_blocks;
   p->op_blocks = b;
   return &b->op;
}

AsmOp * AsmPReg (AsmProgram * p, Register r)            { return KeepOp (p, AsmReg (r)); }
AsmOp * AsmPXmm (AsmProgram * p, int n)                 { return KeepOp (p, AsmXmm (n)); }
AsmOp * AsmPImm (AsmProgram * p, int64_t v)             { return KeepOp (p, AsmImm (v)); }
AsmOp * AsmPImmHex (AsmProgram * p, int64_t v)          { return KeepOp (p, AsmImmHex (v)); }
AsmOp * AsmPImmU (AsmProgram * p, uint64_t v)           { return KeepOp (p, AsmImmU (v)); }
AsmOp * AsmPImmHexLit (AsmProgram * p, int64_t v)       { return KeepOp (p, AsmImmHexLit (v)); }
AsmOp * AsmPMem (AsmProgram * p, int size, Register base, int64_t disp)
                                                        { return KeepOp (p, AsmMem (size, base, disp)); }
AsmOp * AsmPMemIdx (AsmProgram * p, int size, Register base, Register index, int scale, int64_t disp)
                                                        { return KeepOp (p, AsmMemIdx (size, base, index, scale, disp)); }
AsmOp * AsmPMemLab (AsmProgram * p, int size, AsmLabel * label, int64_t disp)
                                                        { return KeepOp (p, AsmMemLab (size, label, disp)); }
AsmOp * AsmPLab (AsmProgram * p, AsmLabel * label)      { return KeepOp (p, AsmLab (label)); }
AsmOp * AsmPOffset (AsmProgram * p, AsmLabel * label)   { return KeepOp (p, AsmOffset (label)); }
AsmOp * AsmPStr (AsmProgram * p, const char * s)        { return KeepOp (p, AsmStr (s)); }
AsmOp * AsmPDup (AsmProgram * p, int count, int64_t value) { return KeepOp (p, AsmDup (count, value)); }

AsmNode * AsmInsP (AsmProgram * p, keyword inst, const AsmOp * a, const AsmOp * b, const AsmOp * c)
{
   AsmOp ops [ASM_MAX_OPS];
   int n = 0;
   if (a != NULL)
   {
      ops [n++] = *a;
      if (b != NULL)
      {
         ops [n++] = *b;
         if (c != NULL) ops [n++] = *c;
      }
   }
   return AsmIns (p, inst, n, ops);
}

AsmNode * AsmDataP (AsmProgram * p, AsmLabel * name, keyword dir, const AsmOp * item)
{
   return AsmData (p, name, dir, item != NULL ? 1 : 0, item);
}

AsmNode * AsmDataAdd (AsmNode * n, const AsmOp * item)
{
   AsmOp * items = (AsmOp *) AsmAlloc ((n->nitems + 1) * sizeof (AsmOp));
   if (n->nitems > 0) memcpy (items, n->items, n->nitems * sizeof (AsmOp));
   items [n->nitems] = *item;
   free (n->items);
   n->items = items;
   n->nitems ++;
   return n;
}
