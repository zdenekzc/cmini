
/* INSTR4.CC */

/*
   Varianta instr4.cc: generator strojoveho kodu pro x86-64 pomoci asmjit::x86::Builder,
   s prehazovanim poradi vypoctu operandu (Sethi-Ullman),
   s predavanim argumentu funkci pres registry
   a navic s promennymi v registrech.

   Promenne v registrech:
     - kandidati: promenne deklarovane jako  register  a ridici promenne for-cyklu
       (for (int i = ...) nebo  for (i = ...)  s lokalni promennou i),
     - jen lokalni promenne a parametry typu int / char / bool, u kterych se nebere adresa
       (predani do parametru  int & , inicializace reference, &x),
     - priorita: register, potom for-cykly - vnorenejsi cyklus ma prednost,
     - registry: rbx, r15, r14, r13 (nejvyse 4 promenne); r13 - r15 se tim
       odeberou registrum pro mezivysledky (max_reg_limit), rbx se pro mezivysledky nepouziva.

   Struktury, ukazatele, konstruktory:
     - kazda polozka struktury zabira 8 bajtu (pole v polozce N * 8 bajtu),
     - p->x, s.x, *p, &x, p [i] (ukazatel na 8-bajtove promenne; ukazatele na char
       a aritmetika s ukazateli zatim nejsou),
     - new T  =>  calloc (1, velikost T), potom se zavola konstruktor,
     - konstruktor je funkce se skrytym parametrem this (rdi), vraci this;
       uvnitr konstruktoru se polozky adresuji pres this,
     - assert (podminka) je vestavena funkce (pri nesplneni vypise hlaseni a zavola abort).

   Uvod funkce:
     Pouzite callee-saved registry (rbx, r12 - r15) se ulozi instrukci push pred  push rbp
     a obnovi instrukci pop po  pop rbp. Instrukce push a  sub rsp, N  se vlozi zpetne
     na zacatek funkce, az je znam nejvyssi pouzity registr. Pri lichem poctu push
     se ramec zvetsi o 8 bajtu, aby rsp zustal zarovnany na 16 bajtu.

   Volani funkci (codeCall):
     1. kazdy argument se vygeneruje jako samostatny blok instrukci
        a zmeri se jeho potreba registru (nejvyssi pouzity registr),
        argument obsahujici volani funkce potrebuje vsechny registry az do r11,
     2. bloky se seradi sestupne podle potreby registru
        (poradi vyhodnoceni argumentu jazyk C neurcuje),
     3. vysledek kazdeho bloku zustane v registru nad potrebou vsech nasledujicich bloku,
        pokud mozno rovnou v cilovem registru (rdi, rsi, rdx, rcx, r8, r9),
        jinak v pomocnem registru, posledni argument muze zustat v rax,
        pokud registr nezbyva, vlozi se  sub rsp, 8 / push rax,
     4. pred instrukci call se hodnoty presunou do cilovych registru
        (paralelni presun - cykly se resi instrukci xchg).

   Binarni operatory (codeBinary):
     Builder uchovava instrukce ve spojovem seznamu uzlu.
       1. vygeneruje se blok levého operandu, zmeri se nejvyssi pouzity registr L,
       2. vygeneruje se blok praveho operandu, zmeri se nejvyssi pouzity registr R
          (poradi registru: rax, rcx, rdx, rsi, rdi, r8, r9, r10, r11, r12, r13, r14, r15),
       3. pokud R > L (pravy operand potrebuje vice registru), presune se blok levého
          operandu za blok praveho - nejdrive se pocita pravy operand,
       4. vysledek prvniho bloku se ulozi do prvniho registru nad potrebou druheho bloku:
          na hranici bloku se zpetne vlozi  mov <registr>, rax,
          pokud registr nezbyva, vlozi se  sub rsp, 8 / push rax.
     Celkova potreba registru je tak max (R, L+1) misto max (L, R+1).
     Jazyk C poradi vyhodnoceni operandu aritmetickych operatoru neurcuje,
     prehozeni je proto pripustne (&&, || a carka se neprehazuji).

     Registry r12 - r15 musi funkce zachovat (callee-saved) - viz Uvod funkce.

   Zasobnikovy model:
     - vysledek kazdeho (pod)vyrazu je v registru rax,
     - levy operand binarniho operatoru se uklada do volneho registru (viz vyse),
       ostatni mezivysledky (indexy, adresy, argumenty volani) na zasobnik (push / pop),
     - rcx, rdx a r11 jsou pomocne registry.

   Pamet:
     - kazda promenna (int, bool, char, ukazatel, ...) zabira 8 bajtu,
       pole N prvku zabira N * 8 bajtu,
     - globalni promenne lezi v pameti, kterou alokuje generator,
     - parametry a lokalni promenne lezi v ramci funkce: [rbp - offset],
     - reference (int & x) je ulozena jako adresa.

   Volani funkci - konvence System V AMD64:
     - parametry v rdi, rsi, rdx, rcx, r8, r9 (nejvyse 6),
     - vysledek v rax,
     - pred instrukci call je rsp zarovnan na 16 bajtu
       (proto si pocitame hloubku zasobniku - promenna depth),
     - al = 0 (pocet vektorovych registru pro funkce s promennym poctem parametru),
     - funkce bez tela (printf, rand, ...) se hledaji v knihovnach pres dlsym.

   Kazda funkce s telem ma vlastni navesti a ramec:
       [push rbx / push r12 ...]  push rbp / mov rbp, rsp / sub rsp, N
       ...
       mov rsp, rbp / pop rbp  [... pop r12 / pop rbx]  ret

   Na zacatku kodu je skok na main, takze vstupni bod je zacatek kodu.

   Zatim nepodporovano: aritmetika s ukazateli, ukazatele na char (krome predani
   do knihovnich funkci), struktury predavane hodnotou, float / double, switch, goto,
   vice nez 6 parametru.
*/

#include "instr.h"
#include "decl.h"

#include "asmjit/x86.h"    /* asmjit.h je v nove verzi prazdny */
using namespace asmjit;
using namespace asmjit::x86;

/* Podporovane verze asmjit:
     - 1.24 a novejsi (_library/asmjit) - API ve stylu snake_case,
     - 1.11 (balik asmjit-devel ve Fedore) - API ve stylu camelCase.
   Mezilehle verze nebyly zkouseny. */

#if ASMJIT_LIBRARY_VERSION >= ASMJIT_LIBRARY_MAKE_VERSION(1, 24, 0)
   #define ASMJIT_SNAKE_CASE_API 1
#else
   #define ASMJIT_SNAKE_CASE_API 0
#endif

#include <dlfcn.h>     /* dlsym */
#include <stdlib.h>
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <algorithm>  /* stable_sort */
#include <set>
#include <iostream>
using namespace std;

/* ---------------------------------------------------------------------- */

static void InstrError (string msg)
{
    ::Error ((char *) msg.c_str ());
}

/* ---------------------------------------------------------------------- */

/* volano z vygenerovaneho kodu pri nesplnenem assert */
static void instrAssertFail (const char * fce_name)
{
    fflush (stdout);
    fprintf (stderr, "Assertion failed in function %s\n", fce_name);
    abort ();
}

namespace {   /* stejne nazvy trid jako v instr1.cc, instr2.cc, instr3.cc a instr4.cc */

/* asmjit hlasi chyby pres ErrorHandler.
   Vyjimku zde vyhodit nelze (asmjit je noexcept),
   proto si prvni chybu zapamatujeme a ohlasime ji po vygenerovani kodu. */

class AsmErrorHandler : public ErrorHandler
{
public:
    bool   failed = false;
    string message;

#if ASMJIT_SNAKE_CASE_API
    void handle_error (asmjit::Error err, const char * msg, BaseEmitter * origin) override
#else
    void handleError (asmjit::Error err, const char * msg, BaseEmitter * origin) override
#endif
    {
        (void) err;
        (void) origin;
        if (!failed)
        {
            failed = true;
            message = msg;
        }
    }
};

/* ---------------------------------------------------------------------- */

/* umisteni promenne */
struct Variable
{
    bool      global = false;
    int64_t * addr = nullptr;  /* globalni promenna */
    int       offset = 0;      /* lokalni promenna: [rbp + offset] */
    bool      ref = false;     /* reference - v pameti je adresa */
    const Gp * reg = nullptr;  /* promenna v registru */
};

struct LoopLabels
{
    Label break_label;
    Label continue_label;
};

static const int VAR_SIZE = 8; /* velikost jedne promenne v bajtech */

/* registry v poradi, ve kterem se prideluji pro mezivysledky */
static const Gp * const REGS [] =
{
    &rax, &rcx, &rdx, &rsi, &rdi, &r8, &r9, &r10, &r11, &r12, &r13, &r14, &r15
};

enum
{
    R_RAX = 0, R_RCX = 1, R_RDX = 2,
    R_R11 = 8,               /* nejvyssi registr, ktery muze zmenit volana funkce */
    R_R12 = 9,               /* r12 - r15 musi funkce zachovat (callee-saved) */
    R_LAST = 12              /* r15 */
};

/* registry pro promenne: rbx a odshora r15, r14, r13 (r12 zustava pro mezivysledky) */
static const Gp * const VAR_REGS [] = { &rbx, &r15, &r14, &r13 };
static const int VAR_REGS_MAX = 4;

/* ---------------------------------------------------------------------- */

class AsmModule
{
public:
    JitRuntime      jit_runtime;
    CodeHolder      code_holder;
    StringLogger    logger;
    AsmErrorHandler error_handler;
    x86::Builder    a;

    vector <unique_ptr <int64_t []>> storage;  /* pamet globalnich promennych */
    deque <string>                   strings;  /* retezcove konstanty */
    map <ObjectPtr, Variable>        vars;
    map <ObjectPtr, Label>           fce_labels;

    ObjectPtr current_fce = nullptr;
    Label     epilogue_label;
    int       frame_size = 0;          /* velikost lokalnich promennych */
    int       depth = 0;               /* pocet 8-bajtovych polozek na zasobniku (push) */
    int       max_reg = R_RAX;         /* nejvyssi pouzity registr (index do REGS) */
    int       fce_max_reg = R_RAX;     /* nejvyssi pouzity registr v cele funkci */
    int       max_reg_limit = R_LAST;  /* nejvyssi registr pro mezivysledky */
    map <ObjectPtr, const Gp *> reg_vars;   /* promenne zvolene do registru (aktualni funkce) */

    map <TypePtr, long>    struct_sizes;     /* velikost struktury (pocet 8-bajtovych polozek) */
    map <ObjectPtr, long>  member_offsets;   /* polozka struktury -> posun v bajtech */
    TypePtr   current_class = nullptr;       /* konstruktor: jeho struktura */
    ObjectPtr this_obj = nullptr;            /* konstruktor: skryty parametr this */
    vector <LoopLabels> loops;         /* pro break a continue */

    typedef long (*MainType) (int, char * *);

    /* ------------------------------------------------------------------ */

    long run (bool verbose)
    {
#if ASMJIT_SNAKE_CASE_API
        code_holder.init (jit_runtime.environment (), jit_runtime.cpu_features ());
        code_holder.set_error_handler (&error_handler);
        if (verbose)
            code_holder.set_logger (&logger);
#else
        code_holder.init (jit_runtime.environment (), jit_runtime.cpuFeatures ());
        code_holder.setErrorHandler (&error_handler);
        if (verbose)
            code_holder.setLogger (&logger);
#endif
        code_holder.attach (&a);

        allocGlobals ();

        ObjectPtr main_fce = nullptr;
        for (NodePtr p = glist.first; p != nullptr; p = p->next)
        {
            ObjectPtr obj = p->item;
            if (obj->fce && obj->body != nullptr)
            {
                fce_labels [obj] = newLabel ();
                if (string (obj->name) == "main")
                    main_fce = obj;
            }
        }

        for (NodePtr p = ctorlist.first; p != nullptr; p = p->next)
            fce_labels [p->item] = newLabel ();     /* konstruktory */

        if (main_fce == nullptr)
            InstrError ("Function main not found");

        a.comment ("entry point");
        a.jmp (fce_labels [main_fce]);

        for (NodePtr p = glist.first; p != nullptr; p = p->next)
            codeDecl (p->item);

        for (NodePtr p = ctorlist.first; p != nullptr; p = p->next)
            codeFunction (p->item);

        a.finalize ();   /* Builder => strojovy kod */

        if (error_handler.failed)
            InstrError ("asmjit: " + error_handler.message);

        if (verbose)
            cout << logger.data () << endl;

        MainType fn;
        asmjit::Error err = jit_runtime.add (&fn, &code_holder);
        if (err != kErrorOk)
            InstrError (string ("asmjit: ") + errorText (err));

        char arg0 [] = "program_name";
        char * argv [] = { arg0, nullptr };
        long result = fn (1, argv);
        fflush (stdout);   /* vystup printf z prekladaneho programu */

        jit_runtime.release (fn);
        return result;
    }

    /* ------------------------------------------------------------------ */

    /* rozdily v API mezi verzemi asmjit */

    Label newLabel ()
    {
#if ASMJIT_SNAKE_CASE_API
        return a.new_label ();
#else
        return a.newLabel ();
#endif
    }

    void setCursor (BaseNode * node)
    {
#if ASMJIT_SNAKE_CASE_API
        a.set_cursor (node);
#else
        a.setCursor (node);
#endif
    }

    /* presun uzly first .. last (souvisly usek seznamu) za uzel ref */
    void moveNodesAfter (BaseNode * first, BaseNode * last, BaseNode * ref)
    {
        vector <BaseNode *> nodes;
        for (BaseNode * n = first; ; n = n->next ())
        {
            nodes.push_back (n);
            if (n == last) break;
        }

        for (BaseNode * n : nodes)
        {
#if ASMJIT_SNAKE_CASE_API
            a.remove_node (n);
            a.add_after (n, ref);
#else
            a.removeNode (n);
            a.addAfter (n, ref);
#endif
            ref = n;
        }
    }

    static const char * errorText (asmjit::Error err)
    {
#if ASMJIT_SNAKE_CASE_API
        return stringify_error (err);
#else
        return DebugUtils::errorAsString (err);
#endif
    }

    /* ------------------------------------------------------------------ */

    /* zaznamenej pouziti registru - staci si pamatovat nejvyssi */
    void use (int reg)
    {
        if (reg > max_reg)
            max_reg = reg;
    }

    /* push / pop s pocitanim hloubky zasobniku */

    void push (const Gp & reg)
    {
        a.push (reg);
        depth ++;
    }

    void pop (const Gp & reg)
    {
        a.pop (reg);
        depth --;
    }

    /* ------------------------------------------------------------------ */

    static ExprPtr skipParens (ExprPtr expr)
    {
        while (expr != nullptr && expr->op == LPAR)
            expr = expr->e1;
        return expr;
    }

    static bool constValue (ExprPtr expr, long & value)
    {
        if (expr == nullptr) return false;

        long v1, v2;
        switch (expr->op)
        {
            case NUM:
                value = strtol (expr->id, nullptr, 0);
                return true;

            case CHR:
                value = (signed char) expr->id [0];
                return true;

            case TRUE_:    value = 1; return true;
            case FALSE_:   value = 0; return true;
            case NULLPTR_: value = 0; return true;

            case LPAR:
                return constValue (expr->e1, value);

            case IDENT:   /* const int N = 100; */
                if (expr->obj != nullptr && expr->obj->type->Const && expr->obj->init != nullptr)
                    return constValue (expr->obj->init, value);
                return false;

            case UNARYPLUS:
                return constValue (expr->e2, value);

            case UNARYMINUS:
                if (!constValue (expr->e2, value)) return false;
                value = -value;
                return true;

            case PLUS: case MINUS: case ASTERISK: case SLASH: case MOD:
                if (!constValue (expr->e1, v1) || !constValue (expr->e2, v2)) return false;
                if ((expr->op == SLASH || expr->op == MOD) && v2 == 0) return false;
                switch (expr->op)
                {
                    case PLUS:     value = v1 + v2; break;
                    case MINUS:    value = v1 - v2; break;
                    case ASTERISK: value = v1 * v2; break;
                    case SLASH:    value = v1 / v2; break;
                    default:       value = v1 % v2; break;
                }
                return true;

            default:
                return false;
        }
    }

    /* pocet 8-bajtovych promennych, ktere zabira typ (pole => pocet prvku) */
    long typeCount (TypePtr type)
    {
        if (type != nullptr && type->sort == Array)
        {
            long n;
            if (!constValue (type->range, n) || n <= 0)
                InstrError ("Array size must be a positive constant");
            return n * typeCount (type->from);
        }
        if (type != nullptr && IsClsSort (type->sort))
            return structCount (type);
        if (type != nullptr && (type->sort == Float || type->sort == Double))
            InstrError ("Floating point types are not supported yet");
        if (type != nullptr && type->sort == StdString)
            InstrError ("std::string is supported only by instr.cc");
        if (type != nullptr && (type->sort == StdSequence || type->sort == StdDictionary))
            InstrError ("Sequence and Dictionary are supported only by instr.cc");
        return 1;
    }

    /* rozlozeni struktury: posuny polozek, velikost */
    long structCount (TypePtr type)
    {
        auto it = struct_sizes.find (type);
        if (it != struct_sizes.end ())
        {
            if (it->second < 0)
                InstrError ("Structure contains itself");
            return it->second;
        }
        struct_sizes [type] = -1;      /* rozpracovano */

        long n = 0;
        for (NodePtr p = type->list.first; p != nullptr; p = p->next)
        {
            ObjectPtr m = p->item;
            if (m->fce || m->storage == Typedef || m->name [0] == 0)
                continue;
            if (m->width != nullptr)
                InstrError (string ("Bit fields are not supported: ") + m->name);
            if (m->type->sort == Reference)
                InstrError (string ("Reference members are not supported: ") + m->name);

            long cnt = typeCount (m->type);
            if (type->sort == Union)
            {
                member_offsets [m] = 0;
                n = max (n, cnt);
            }
            else
            {
                member_offsets [m] = n * VAR_SIZE;
                n += cnt;
            }
        }
        if (n == 0) n = 1;
        struct_sizes [type] = n;
        return n;
    }

    long memberOffset (ObjectPtr member, TypePtr cls)
    {
        structCount (cls);
        auto it = member_offsets.find (member);
        if (member == nullptr || it == member_offsets.end ())
            InstrError ("Unknown structure member");
        return it->second;
    }

    void checkNoConstructor (ObjectPtr obj)
    {
        TypePtr t = obj->type;
        while (t != nullptr && t->sort == Array) t = t->from;
        if (t != nullptr && IsClsSort (t->sort) && t->ctor != nullptr)
            InstrError (string ("Constructors are supported only with new: ") + obj->name);
    }

    /* ------------------------------------------------------------------ */

    void allocGlobals ()
    {
        for (NodePtr p = glist.first; p != nullptr; p = p->next)
        {
            ObjectPtr obj = p->item;
            if (obj->fce || obj->storage == Typedef || obj->name [0] == 0)
                continue;          /* funkce, typedef, struct X { ... }; */
            checkNoConstructor (obj);

            if (obj->type->sort == Reference)
                InstrError (string ("Global references are not supported: ") + obj->name);

            long count = typeCount (obj->type);
            int64_t * mem = new int64_t [count] ();   /* vynulovano */
            storage.emplace_back (mem);

            if (obj->init != nullptr)
            {
                long value;
                if (count != 1 || !constValue (obj->init, value))
                    InstrError (string ("Only constant initializers are supported: ") + obj->name);
                mem [0] = value;
            }

            Variable var;
            var.global = true;
            var.addr = mem;
            vars [obj] = var;
        }
    }

    void allocLocal (ObjectPtr obj)
    {
        Variable var;

        auto r = reg_vars.find (obj);
        if (r != reg_vars.end ())
        {
            var.reg = r->second;       /* promenna v registru - v ramci nezabira misto */
            vars [obj] = var;
            return;
        }

        var.ref = obj->type->sort == Reference;

        long count = var.ref ? 1 : typeCount (obj->type);
        frame_size += count * VAR_SIZE;
        var.offset = - frame_size;     /* pole: prvky od [rbp+offset] smerem nahoru */
        vars [obj] = var;
    }

    void collectLocalDecls (Queue & decl)
    {
        for (NodePtr p = decl.first; p != nullptr; p = p->next)
        {
            ObjectPtr obj = p->item;
            if (obj->fce || obj->storage == Typedef || obj->storage == Extern)
                continue;
            if (obj->storage == Static)
                InstrError (string ("Static local variables are not supported yet: ") + obj->name);
            if (obj->name [0] == 0)
                continue;
            checkNoConstructor (obj);
            allocLocal (obj);
        }
    }

    /* projdi telo funkce a prirad lokalnim promennym misto v ramci */
    void collectLocals (StatPtr stat)
    {
        if (stat == nullptr) return;

        switch (stat->st)
        {
            case DECL:
                collectLocalDecls (stat->decl);
                break;
            case LBRACE:
                for (StatPtr s = stat->s1; s != nullptr; s = s->next)
                    collectLocals (s);
                break;
            case FOR_:
                collectLocalDecls (stat->decl);
                collectLocals (stat->s1);
                break;
            case IF_:
                collectLocals (stat->s1);
                collectLocals (stat->s2);
                break;
            default:
                collectLocals (stat->s1);
                break;
        }
    }

    const Variable & variable (ExprPtr expr)
    {
        auto it = vars.find (expr->obj);
        if (expr->obj == nullptr || it == vars.end ())
            InstrError (string ("Unknown variable: ") + expr->id);
        return it->second;
    }

    /* ------------------------------------------------------------------ */

    void codeDecl (ObjectPtr obj)
    {
        if (obj->fce && obj->body != nullptr)
            codeFunction (obj);
    }

    /* ---------------- vyber promennych do registru ---------------- */

    struct RegCandidate
    {
        ObjectPtr obj;
        int       score;      /* register => 1000, for-cyklus => hloubka vnoreni */
        int       order;      /* poradi vyskytu */
    };

    set <ObjectPtr>        locals_seen;    /* parametry a lokalni promenne funkce */
    set <ObjectPtr>        address_taken;  /* promenne, jejichz adresa se pouziva */
    vector <RegCandidate>  candidates;

    static bool regEligibleType (ObjectPtr obj)
    {
        TypePtr t = obj->type;
        return t != nullptr && (t->sort == Int || t->sort == Char || t->sort == Bool || t->sort == Enum ||
                                t->sort == Pointer)
               && obj->storage != Static && obj->storage != Extern && obj->storage != Typedef
               && !obj->fce;
    }

    void addCandidate (ObjectPtr obj, int score)
    {
        if (obj == nullptr || !regEligibleType (obj) || locals_seen.count (obj) == 0)
            return;
        for (RegCandidate & c : candidates)
            if (c.obj == obj)
            {
                c.score = max (c.score, score);
                return;
            }
        candidates.push_back ({ obj, score, (int) candidates.size () });
    }

    void markAddress (ExprPtr expr)
    {
        expr = skipParens (expr);
        if (expr != nullptr && expr->op == IDENT && expr->obj != nullptr)
            address_taken.insert (expr->obj);
    }

    void scanExpr (ExprPtr expr)
    {
        if (expr == nullptr) return;

        if (expr->op == ADR)
            markAddress (expr->e2);

        if (expr->op == FCE)
        {
            ExprPtr f = skipParens (expr->e1);
            vector <ExprPtr> args;
            collectArgs (expr->e2, args);
            NodePtr par = (f != nullptr && f->obj != nullptr && f->obj->type != nullptr)
                          ? f->obj->type->list.first : nullptr;
            for (ExprPtr arg : args)
            {
                if (par != nullptr && par->item->type != nullptr && par->item->type->sort == Reference)
                    markAddress (arg);           /* int & => potrebuje adresu */
                if (par != nullptr && par->item->type != nullptr) par = par->next;
            }
        }

        scanExpr (expr->e1);
        scanExpr (expr->e2);
        scanExpr (expr->e3);
    }

    void scanDecls (Queue & decl)
    {
        for (NodePtr p = decl.first; p != nullptr; p = p->next)
        {
            ObjectPtr obj = p->item;
            locals_seen.insert (obj);
            if (obj->storage == Register_)
                addCandidate (obj, 1000);
            if (obj->init != nullptr)
            {
                if (obj->type != nullptr && obj->type->sort == Reference)
                    markAddress (obj->init);
                scanExpr (obj->init);
            }
        }
    }

    void scanStat (StatPtr stat, int for_depth)
    {
        if (stat == nullptr) return;

        switch (stat->st)
        {
            case DECL:
                scanDecls (stat->decl);
                break;

            case LBRACE:
                for (StatPtr s = stat->s1; s != nullptr; s = s->next)
                    scanStat (s, for_depth);
                break;

            case FOR_:
            {
                scanDecls (stat->decl);
                for (NodePtr p = stat->decl.first; p != nullptr; p = p->next)
                    addCandidate (p->item, for_depth + 1);      /* for (int i = ...) */

                ExprPtr init = skipParens (stat->e1);
                if (init != nullptr && init->op == ASSIGN)
                {
                    ExprPtr v = skipParens (init->e1);
                    if (v->op == IDENT)
                        addCandidate (v->obj, for_depth + 1);   /* for (i = ...) */
                }

                scanExpr (stat->e1);
                scanExpr (stat->e2);
                scanExpr (stat->e3);
                scanStat (stat->s1, for_depth + 1);
                break;
            }

            default:
                scanExpr (stat->e1);
                scanExpr (stat->e2);
                scanExpr (stat->e3);
                scanStat (stat->s1, for_depth);
                scanStat (stat->s2, for_depth);
                break;
        }
    }

    void chooseRegisterVariables (ObjectPtr decl, vector <ObjectPtr> & params)
    {
        locals_seen.clear ();
        address_taken.clear ();
        candidates.clear ();
        reg_vars.clear ();

        for (ObjectPtr par : params)
        {
            locals_seen.insert (par);
            if (par->storage == Register_)
                addCandidate (par, 1000);         /* int f (register int n) */
        }

        scanStat (decl->body, 0);

        vector <RegCandidate> sorted;
        for (RegCandidate & c : candidates)
            if (address_taken.count (c.obj) == 0)
                sorted.push_back (c);
        stable_sort (sorted.begin (), sorted.end (),
                     [] (const RegCandidate & x, const RegCandidate & y) { return x.score > y.score; });

        int n = min ((int) sorted.size (), VAR_REGS_MAX);
        string info;
        for (int i = 0; i < n; i++)
        {
            reg_vars [sorted [i].obj] = VAR_REGS [i];
            info += string (" ") + sorted [i].obj->name;
        }

        /* r15, r14, r13 obsazene promennymi uz nejsou pro mezivysledky */
        max_reg_limit = R_LAST - max (0, n - 1);

        if (n > 0)
            a.comment (("register variables:" + info).c_str ());
    }

    /* ---------------- funkce ---------------- */

    void codeFunction (ObjectPtr decl)
    {
        static const Gp * arg_regs [] = { &rdi, &rsi, &rdx, &rcx, &r8, &r9 };

        current_fce = decl;
        epilogue_label = newLabel ();
        frame_size = 0;
        depth = 0;

        /* parametry */
        vector <ObjectPtr> params;
        for (NodePtr p = decl->type->list.first; p != nullptr; p = p->next)
        {
            ObjectPtr par = p->item;
            if (par->type == nullptr)
                InstrError (string ("Variable number of parameters is not supported: ") + decl->name);
            if (par->type->sort == Void)
                break;          /* int f (void) */
            params.push_back (par);
        }

        /* konstruktor: skryty parametr this */
        current_class = decl->owner;
        this_obj = nullptr;
        if (current_class != nullptr)
        {
            this_obj = NewObject ((char *) "this", Link (Pointer, current_class));
            params.insert (params.begin (), this_obj);
        }

        if (params.size () > 6)
            InstrError (string ("More than 6 parameters are not supported: ") + decl->name);

        a.comment ((string ("function ") + decl->name).c_str ());
        chooseRegisterVariables (decl, params);

        for (ObjectPtr par : params)
            allocLocal (par);
        collectLocals (decl->body);               /* lokalni promenne */

        a.bind (fce_labels [decl]);
        BaseNode * entry = a.cursor ();           /* sem se pozdeji vlozi push callee-saved registru */
        a.push (rbp);
        a.mov (rbp, rsp);
        BaseNode * frame_pos = a.cursor ();       /* sem se pozdeji vlozi sub rsp, N */

        for (size_t i = 0; i < params.size (); i++)
        {
            Variable & var = vars [params [i]];
            if (var.reg != nullptr)
                a.mov (*var.reg, *arg_regs [i]);
            else
                a.mov (qword_ptr (rbp, var.offset), *arg_regs [i]);
        }

        fce_max_reg = R_RAX;
        max_reg = R_RAX;
        codeStat (decl->body);

        if (current_class != nullptr)
            loadThis ();          /* konstruktor vraci this */
        else
            a.xor_ (eax, eax);    /* bez return => vysledek 0 */
        a.bind (epilogue_label);

        /* pouzite callee-saved registry: rbx, r12 .. r15 */
        vector <const Gp *> saved;
        bool uses_rbx = false;
        for (auto & rv : reg_vars)
            if (rv.second == &rbx) uses_rbx = true;
        if (uses_rbx)
            saved.push_back (&rbx);
        for (int r = R_R12; r <= R_LAST; r++)
        {
            bool var_reg = false;
            for (auto & rv : reg_vars)
                if (rv.second == REGS [r]) var_reg = true;
            if (r <= fce_max_reg || var_reg)
                saved.push_back (REGS [r]);
        }

        a.mov (rsp, rbp);
        a.pop (rbp);
        for (int i = (int) saved.size () - 1; i >= 0; i--)
            a.pop (*saved [i]);
        a.ret ();

        BaseNode * end = a.cursor ();

        /* zpetne: push callee-saved registru pred push rbp */
        setCursor (entry);
        for (const Gp * r : saved)
            a.push (*r);

        /* zpetne: ramec, pri lichem poctu push o 8 bajtu vetsi (zarovnani na 16) */
        int frame = (frame_size + 15) & ~15;
        if (saved.size () % 2 != 0)
            frame += 8;
        if (frame > 0)
        {
            setCursor (frame_pos);
            a.sub (rsp, frame);
        }

        setCursor (end);
        current_fce = nullptr;
        current_class = nullptr;
        this_obj = nullptr;
    }

    /* rax := this */
    void loadThis ()
    {
        const Variable & var = vars [this_obj];
        if (var.reg != nullptr)
            a.mov (rax, *var.reg);
        else
            a.mov (rax, qword_ptr (rbp, var.offset));
    }

    /* rax := (rax != 0) ? 1 : 0 */
    void normalizeBool ()
    {
        a.test (rax, rax);
        a.setne (al);
        a.movzx (rax, al);
    }

    /* rax := adresa promenne (u reference adresa ulozena v promenne) */
    void codeVarAddress (ExprPtr expr)
    {
        /* polozka struktury uvnitr konstruktoru:  this + posun */
        if (current_class != nullptr && expr->obj != nullptr && vars.find (expr->obj) == vars.end () &&
            LookupMember (current_class, expr->obj->name) == expr->obj)
        {
            long off = memberOffset (expr->obj, current_class);
            loadThis ();
            if (off != 0) a.add (rax, off);
            return;
        }

        const Variable & var = variable (expr);
        if (var.reg != nullptr)
            InstrError (string ("Address of register variable: ") + expr->id);
        if (var.global)
            a.mov (rax, imm ((intptr_t) var.addr));
        else
            a.lea (rax, qword_ptr (rbp, var.offset));

        if (var.ref)
            a.mov (rax, qword_ptr (rax));
    }

    /* rax := adresa l-hodnoty */
    void codeAddress (ExprPtr expr)
    {
        expr = skipParens (expr);

        switch (expr->op)
        {
            case IDENT:
                if (expr->obj != nullptr && expr->obj->fce)
                    InstrError (string ("Function used as a variable: ") + expr->id);
                codeVarAddress (expr);
                break;

            case LBRACK:
            {
                ExprPtr arr = skipParens (expr->e1);
                if (arr->type->sort == Pointer)
                {
                    checkPointee (arr->type);
                    codeExpr (arr);               /* ukazatel => jeho hodnota */
                }
                else if (arr->type->sort == Array)
                    codeAddress (arr);
                else
                    InstrError ("Array or pointer expected");
                push (rax);
                codeExpr (expr->e2);
                use (R_RCX);
                a.mov (rcx, rax);
                pop (rax);

                long elem_size = typeCount (expr->type) * VAR_SIZE;
                if (elem_size == VAR_SIZE)
                    a.lea (rax, qword_ptr (rax, rcx, 3));   /* rax + rcx * 8 */
                else
                {
                    a.imul (rcx, rcx, imm (elem_size));
                    a.add (rax, rcx);
                }
                break;
            }

            case DEREF:
                checkPointee (expr->e2->type);
                codeExpr (expr->e2);
                break;

            case PERIOD:                          /* s.x */
            {
                long off = memberOffset (expr->obj, skipParens (expr->e1)->type);
                codeAddress (expr->e1);
                if (off != 0) a.add (rax, off);
                break;
            }

            case ARROW:                           /* p->x */
            {
                long off = memberOffset (expr->obj, skipParens (expr->e1)->type->from);
                codeExpr (expr->e1);
                if (off != 0) a.add (rax, off);
                break;
            }

            default:
                InstrError ("lvalue expected");
        }
    }

    /* rax := hodnota promenne nebo prvku pole */
    /* promenna v registru, jinak nullptr */
    const Gp * regVar (ExprPtr expr)
    {
        expr = skipParens (expr);
        if (expr == nullptr || expr->op != IDENT || expr->obj == nullptr)
            return nullptr;
        auto it = vars.find (expr->obj);
        return it == vars.end () ? nullptr : it->second.reg;
    }

    /* ukazatel musi ukazovat na 8-bajtove promenne */
    static void checkPointee (TypePtr ptr)
    {
        if (ptr == nullptr || ptr->from == nullptr || ptr->from->sort == Void)
            InstrError ("Dereferencing of void pointer");
        if (ptr->from->sort == Char)
            InstrError ("Pointers to char are not supported yet");
    }

    void codeLoad (ExprPtr expr)
    {
        if (IsClsSort (expr->type->sort))
            InstrError ("Structures as values are not supported yet (use pointer or reference)");

        if (const Gp * r = regVar (expr))
            a.mov (rax, *r);
        else if (expr->type->sort == Array)
            codeAddress (expr);           /* pole => adresa prvniho prvku */
        else
        {
            codeAddress (expr);
            a.mov (rax, qword_ptr (rax));
        }
    }

    /* rax op= rcx */
    void codeOperation (SYMBOL kind)
    {
        use (R_RCX);
        if (kind == SLASH || kind == MOD)
            use (R_RDX);

        switch (kind)
        {
            case PLUS:     a.add (rax, rcx);  break;
            case MINUS:    a.sub (rax, rcx);  break;
            case ASTERISK: a.imul (rax, rcx); break;

            case SLASH:
                a.cqo ();         /* rdx:rax = znamenkove rozsireny rax */
                a.idiv (rcx);
                break;

            case MOD:
                a.cqo ();
                a.idiv (rcx);
                a.mov (rax, rdx);
                break;

            case SHL: a.shl (rax, cl); break;
            case SHR: a.sar (rax, cl); break;

            case AND: a.and_ (rax, rcx); break;
            case OR:  a.or_  (rax, rcx); break;
            case XOR: a.xor_ (rax, rcx); break;

            default:
                a.cmp (rax, rcx);
                switch (kind)
                {
                    case LESS:         a.setl  (al); break;
                    case LESSEQUAL:    a.setle (al); break;
                    case GREATER:      a.setg  (al); break;
                    case GREATEREQUAL: a.setge (al); break;
                    case EQUAL:        a.sete  (al); break;
                    case UNEQUAL:      a.setne (al); break;
                    default:           Bug ();
                }
                a.movzx (rax, al);
                break;
        }
    }

    void codeBinary (ExprPtr expr)
    {
        if ((expr->op == PLUS || expr->op == MINUS) &&
            (expr->e1->type->sort == Pointer || expr->e2->type->sort == Pointer ||
             expr->e1->type->sort == Array || expr->e2->type->sort == Array))
            InstrError ("Pointer arithmetic is not supported yet");

        int outer_max = max_reg;

        /* blok leveho operandu */
        BaseNode * before = a.cursor ();       /* uzel pred obema bloky */
        max_reg = R_RAX;
        codeExpr (expr->e1);
        int left_max = max_reg;
        BaseNode * left_end = a.cursor ();

        /* blok praveho operandu */
        max_reg = R_RAX;
        codeExpr (expr->e2);
        int right_max = max_reg;
        BaseNode * right_end = a.cursor ();

        /* pravy operand potrebuje vice registru => pocitat ho jako prvni */
        bool swap = right_max > left_max;
        BaseNode * first_end;      /* konec prvniho bloku - sem se ulozi jeho vysledek */
        BaseNode * end;            /* konec obou bloku */
        int second_max;            /* potreba registru druheho bloku */

        if (swap)
        {
            moveNodesAfter (before->next (), left_end, right_end);
            first_end = right_end;
            end = left_end;
            second_max = left_max;
        }
        else
        {
            first_end = left_end;
            end = right_end;
            second_max = right_max;
        }

        int save = second_max + 1;             /* prvni registr, ktery druhy blok nepouzil */
        if (save < R_RCX) save = R_RCX;

        setCursor (first_end);
        if (swap)
            a.comment ("operands swapped");
        if (save <= max_reg_limit)
            a.mov (*REGS [save], rax);
        else
        {
            a.sub (rsp, 8);                    /* 16 bajtu => zarovnani se nezmeni */
            a.push (rax);
        }
        setCursor (end);

        /* rax = levy operand, rcx = pravy operand */
        if (swap)
        {
            /* rax = levy, ulozeny = pravy */
            if (save > max_reg_limit)
            {
                a.pop (rcx);
                a.add (rsp, 8);
            }
            else if (save != R_RCX)
                a.mov (rcx, *REGS [save]);
        }
        else
        {
            /* rax = pravy, ulozeny = levy */
            if (save > max_reg_limit)
            {
                a.mov (rcx, rax);
                a.pop (rax);
                a.add (rsp, 8);
            }
            else if (save == R_RCX)
                a.xchg (rax, rcx);
            else
            {
                a.mov (rcx, rax);
                a.mov (rax, *REGS [save]);
            }
        }

        max_reg = max (left_max, right_max);
        use (save <= max_reg_limit ? save : R_RCX);
        codeOperation (expr->op);
        if (max_reg > fce_max_reg)
            fce_max_reg = max_reg;
        use (outer_max);
    }

    void codeAndOr (ExprPtr expr)
    {
        Label end_label = newLabel ();

        codeExpr (expr->e1);
        a.test (rax, rax);

        if (expr->op == LOGAND)
            a.jz (end_label);    /* levy = 0 => vysledek 0 */
        else
            a.jnz (end_label);   /* levy != 0 => vysledek 1 */

        codeExpr (expr->e2);
        a.bind (end_label);
        normalizeBool ();
    }

    void codeConditional (ExprPtr expr)
    {
        // e1 ? e2 : e3
        Label else_label = newLabel ();
        Label end_label = newLabel ();

        codeCond (expr->e1);
        a.jz (else_label);
        codeExpr (expr->e2);
        a.jmp (end_label);
        a.bind (else_label);
        codeExpr (expr->e3);
        a.bind (end_label);
    }

    /* =, +=, -=, ... */
    void codeAssign (ExprPtr expr)
    {
        static const map <SYMBOL, SYMBOL> ops =
        {
            { ADDASSIGN, PLUS }, { SUBASSIGN, MINUS }, { MULASSIGN, ASTERISK },
            { DIVASSIGN, SLASH }, { MODASSIGN, MOD }, { SHLASSIGN, SHL },
            { SHRASSIGN, SHR }, { ANDASSIGN, AND }, { ORASSIGN, OR }, { XORASSIGN, XOR }
        };

        if (IsClsSort (expr->e1->type->sort))
            InstrError ("Structure assignment is not supported yet");
        if (expr->op != ASSIGN && expr->e1->type->sort == Pointer)
            InstrError ("Pointer arithmetic is not supported yet");

        if (const Gp * r = regVar (expr->e1))   /* promenna v registru */
        {
            codeExpr (expr->e2);
            if (expr->op != ASSIGN)
            {
                use (R_RCX);
                a.mov (rcx, rax);
                a.mov (rax, *r);
                codeOperation (ops.at (expr->op));
            }
            if (expr->e1->type->sort == Bool)
                normalizeBool ();
            a.mov (*r, rax);
            return;
        }

        codeAddress (expr->e1);
        push (rax);
        codeExpr (expr->e2);
        use (R_R11);
        pop (r11);                /* r11 = adresa */

        if (expr->op != ASSIGN)
        {
            a.mov (rcx, rax);
            a.mov (rax, qword_ptr (r11));
            codeOperation (ops.at (expr->op));
        }

        if (expr->e1->type->sort == Bool)
            normalizeBool ();
        a.mov (qword_ptr (r11), rax);
    }

    /* ++ a -- */
    void codeIncDec (ExprPtr expr)
    {
        bool postfix = expr->e1 != nullptr;     /* e1++  versus  ++e2 */
        ExprPtr operand = postfix ? expr->e1 : expr->e2;

        if (operand->type->sort == Pointer)
            InstrError ("Pointers are not supported yet");

        if (const Gp * r = regVar (operand))    /* promenna v registru */
        {
            if (postfix)
            {
                a.mov (rax, *r);
                if (expr->op == DPLUS) a.add (*r, 1); else a.sub (*r, 1);
            }
            else
            {
                if (expr->op == DPLUS) a.add (*r, 1); else a.sub (*r, 1);
                a.mov (rax, *r);
            }
            return;
        }

        codeAddress (operand);
        use (R_RDX);
        a.mov (rcx, rax);
        a.mov (rax, qword_ptr (rcx));

        if (postfix)
        {
            a.mov (rdx, rax);                   /* vysledek = puvodni hodnota */
            if (expr->op == DPLUS) a.add (rdx, 1); else a.sub (rdx, 1);
            a.mov (qword_ptr (rcx), rdx);
        }
        else
        {
            if (expr->op == DPLUS) a.add (rax, 1); else a.sub (rax, 1);
            a.mov (qword_ptr (rcx), rax);
        }
    }

    static void collectArgs (ExprPtr expr, vector <ExprPtr> & args)
    {
        if (expr == nullptr) return;
        if (expr->op == COMMA)
        {
            collectArgs (expr->e1, args);
            collectArgs (expr->e2, args);
        }
        else
            args.push_back (expr);
    }

    /* jeden argument volani funkce */
    struct ArgBlock
    {
        int        target;     /* cilovy registr (index do REGS) */
        BaseNode * first;      /* prvni a posledni uzel bloku instrukci */
        BaseNode * last;
        int        need;       /* nejvyssi registr pouzity pri vypoctu */
        int        hold;       /* kde ceka hodnota do volani: registr, nebo NO_REG = zasobnik */
    };

    static const int NO_REG = -1;

    /* zvol registr, ve kterem pocka hodnota argumentu blocks [k] */
    int chooseHoldRegister (vector <ArgBlock> & blocks, size_t k, int lowest)
    {
        auto taken = [&] (int r)
        {
            for (size_t j = 0; j < k; j++)
                if (blocks [j].hold == r) return true;
            return false;
        };
        auto target_of_other = [&] (int r)
        {
            for (size_t j = k + 1; j < blocks.size (); j++)
                if (blocks [j].target == r) return true;
            return false;
        };

        /* nejlepe rovnou cilovy registr */
        if (blocks [k].target >= lowest && !taken (blocks [k].target))
            return blocks [k].target;

        /* volny registr, ktery nepotrebuje jiny argument */
        for (int r = lowest; r <= max_reg_limit; r++)
            if (!taken (r) && !target_of_other (r))
                return r;

        for (int r = lowest; r <= max_reg_limit; r++)
            if (!taken (r))
                return r;

        return NO_REG;
    }

    /* paralelni presun registru:  dst [i] := src [i]  pro vsechna i najednou */
    void parallelMove (vector <pair <int, int>> moves)   /* (src, dst) */
    {
        for (;;)
        {
            /* odstran presuny, ktere nic nedelaji */
            for (size_t i = 0; i < moves.size (); )
                if (moves [i].first == moves [i].second)
                    moves.erase (moves.begin () + i);
                else
                    i++;

            if (moves.empty ()) break;

            /* presun do registru, ktery uz neni zdrojem jineho presunu */
            bool done = false;
            for (size_t i = 0; i < moves.size () && !done; i++)
            {
                int dst = moves [i].second;
                bool is_source = false;
                for (size_t j = 0; j < moves.size (); j++)
                    if (j != i && moves [j].first == dst)
                        is_source = true;

                if (!is_source)
                {
                    a.mov (*REGS [dst], *REGS [moves [i].first]);
                    moves.erase (moves.begin () + i);
                    done = true;
                }
            }

            /* zbyvaji jen cykly => vymena */
            if (!done)
            {
                int src = moves [0].first;
                int dst = moves [0].second;
                a.xchg (*REGS [dst], *REGS [src]);
                moves.erase (moves.begin ());
                for (auto & m : moves)       /* puvodni obsah dst je nyni v src */
                    if (m.first == dst)
                        m.first = src;
            }
        }
    }

    /* instrukce call (zarovnani zasobniku, al = 0) */
    void emitCall (const Label * label, void * addr)
    {
        use (R_R11);                               /* volana funkce muze zmenit rax .. r11 */

        bool pad = (depth % 2) != 0;               /* zarovnani rsp na 16 bajtu */
        if (pad) a.sub (rsp, 8);

        a.xor_ (eax, eax);                         /* al = 0 vektorovych registru */

        if (label != nullptr)
            a.call (*label);
        else
        {
            a.mov (r11, imm ((intptr_t) addr));
            a.call (r11);
        }

        if (pad) a.add (rsp, 8);
    }

    /* chybejici argumenty => implicitni hodnoty parametru:  int level = 0 */
    static void addDefaultArgs (ObjectPtr fce, vector <ExprPtr> & args)
    {
        size_t i = 0;
        for (NodePtr p = fce->type->list.first; p != nullptr; p = p->next, i++)
        {
            ObjectPtr par = p->item;
            if (par->type == nullptr || par->type->sort == Void) break;
            if (i >= args.size ())
            {
                if (par->init == nullptr)
                    InstrError (string ("Too few arguments: ") + fce->name);
                args.push_back (par->init);
            }
        }
    }

    /* assert (podminka) */
    void codeAssert (vector <ExprPtr> & args)
    {
        if (args.size () != 1)
            InstrError ("assert expects one argument");

        int outer_max = max_reg;
        Label ok_label = newLabel ();

        codeCond (args [0]);
        a.jnz (ok_label);
        strings.push_back (current_fce != nullptr ? current_fce->name : "?");
        a.mov (rdi, imm ((intptr_t) strings.back ().c_str ()));
        emitCall (nullptr, (void *) &instrAssertFail);
        a.bind (ok_label);
        a.xor_ (eax, eax);

        max_reg = max (outer_max, (int) R_R11);
        if (max_reg > fce_max_reg)
            fce_max_reg = max_reg;
    }

    /* new T  /  new T (argumenty konstruktoru) */
    void codeNew (ExprPtr expr)
    {
        static const Gp * arg_regs [] = { &rdi, &rsi, &rdx, &rcx, &r8, &r9 };

        TypePtr T = expr->type->from;
        long size = typeCount (T) * VAR_SIZE;
        ObjectPtr ctor = IsClsSort (T->sort) ? T->ctor : nullptr;

        vector <ExprPtr> args;
        collectArgs (expr->e1, args);
        if (ctor == nullptr && !args.empty ())
            InstrError ("new T (...) is supported only for structures with constructor");
        if (ctor != nullptr && ctor->body == nullptr)
            InstrError (string ("Constructor is not defined: ") + ctor->name);
        if (ctor != nullptr)
            addDefaultArgs (ctor, args);
        if (args.size () > 5)
            InstrError ("More than 5 constructor arguments are not supported");

        int outer_max = max_reg;

        /* calloc (1, size) - pamet je vynulovana */
        a.mov (edi, 1);
        a.mov (rsi, imm (size));
        emitCall (nullptr, (void *) &calloc);

        if (ctor != nullptr)
        {
            /* konstruktor (this, argumenty...) - argumenty jednoduse pres zasobnik */
            push (rax);
            NodePtr par = ctor->type->list.first;
            for (ExprPtr arg : args)
            {
                TypePtr ptype = (par != nullptr) ? par->item->type : nullptr;
                if (ptype != nullptr && ptype->sort == Reference)
                    codeAddress (arg);
                else
                    codeExpr (arg);
                push (rax);
                if (ptype != nullptr) par = par->next;
            }
            for (int i = (int) args.size (); i >= 0; i--)
                pop (*arg_regs [i]);

            emitCall (&fce_labels [ctor], nullptr);   /* vraci this */
        }

        max_reg = max (outer_max, (int) R_R11);
        if (max_reg > fce_max_reg)
            fce_max_reg = max_reg;
    }

    void codeCall (ExprPtr expr)
    {
        /* rdi, rsi, rdx, rcx, r8, r9 jako indexy do REGS */
        static const int arg_regs [] = { 4, 3, 2, 1, 5, 6 };

        ExprPtr f = skipParens (expr->e1);
        if (f->op != IDENT || f->obj == nullptr || !f->obj->fce)
            InstrError ("Only direct function calls are supported");
        ObjectPtr fce = f->obj;

        vector <ExprPtr> args;
        collectArgs (expr->e2, args);

        if (fce->implicit && string (fce->name) == "assert")
        {
            codeAssert (args);
            return;
        }

        addDefaultArgs (fce, args);
        if (args.size () > 6)
            InstrError (string ("More than 6 arguments are not supported: ") + fce->name);

        int outer_max = max_reg;
        int call_max = R_R11;                      /* volana funkce muze zmenit rax .. r11 */

        /* 1. kazdy argument jako samostatny blok */
        vector <ArgBlock> blocks;
        BaseNode * before = a.cursor ();
        NodePtr par = fce->type->list.first;
        for (size_t i = 0; i < args.size (); i++)
        {
            TypePtr ptype = (par != nullptr) ? par->item->type : nullptr;   /* nullptr => ... */

            BaseNode * start = a.cursor ();
            max_reg = R_RAX;
            if (ptype != nullptr && IsClsSort (ptype->sort))
                InstrError ("Structures passed by value are not supported yet (use reference)");
            if (ptype != nullptr && ptype->sort == Reference)
                codeAddress (args [i]);            /* reference => adresa */
            else
                codeExpr (args [i]);

            ArgBlock blk;
            blk.target = arg_regs [i];
            blk.first = start->next ();
            blk.last = a.cursor ();
            blk.need = max_reg;
            blk.hold = NO_REG;
            blocks.push_back (blk);

            if (ptype != nullptr) par = par->next;
        }

        if (!blocks.empty ())
        {
            /* 2. serad bloky sestupne podle potreby registru */
            vector <ArgBlock> sorted = blocks;
            stable_sort (sorted.begin (), sorted.end (),
                         [] (const ArgBlock & x, const ArgBlock & y) { return x.need > y.need; });

            bool reordered = false;
            for (size_t k = 0; k < blocks.size (); k++)
                if (sorted [k].first != blocks [k].first)
                    reordered = true;
            blocks = sorted;

            if (reordered)
            {
                BaseNode * ref = before;
                for (ArgBlock & blk : blocks)
                {
                    moveNodesAfter (blk.first, blk.last, ref);
                    ref = blk.last;
                }
            }

            /* 3. kde pocka hodnota kazdeho argumentu */
            for (size_t k = 0; k < blocks.size (); k++)
            {
                int later_need = -1;               /* posledni blok => muze zustat v rax */
                for (size_t j = k + 1; j < blocks.size (); j++)
                    later_need = max (later_need, blocks [j].need);
                if (k + 1 < blocks.size () && later_need < R_RAX)
                    later_need = R_RAX;

                blocks [k].hold = chooseHoldRegister (blocks, k, later_need + 1);

                setCursor (blocks [k].last);
                if (k == 0 && reordered)
                    a.comment ("arguments reordered");
                if (blocks [k].hold == NO_REG)
                {
                    a.sub (rsp, 8);                /* 16 bajtu => zarovnani se nezmeni */
                    a.push (rax);
                }
                else if (blocks [k].hold != R_RAX)
                    a.mov (*REGS [blocks [k].hold], rax);

                call_max = max (call_max, max (blocks [k].need, blocks [k].hold));
            }
            /* kurzor je ted za poslednim blokem */

            /* 4. hodnoty do cilovych registru */
            vector <pair <int, int>> moves;
            for (ArgBlock & blk : blocks)
                if (blk.hold != NO_REG)
                    moves.push_back ({ blk.hold, blk.target });
            parallelMove (moves);

            for (int k = (int) blocks.size () - 1; k >= 0; k--)   /* zasobnik v opacnem poradi */
                if (blocks [k].hold == NO_REG)
                {
                    a.pop (*REGS [blocks [k].target]);
                    a.add (rsp, 8);
                }
        }

        max_reg = max (outer_max, call_max);
        if (max_reg > fce_max_reg)
            fce_max_reg = max_reg;

        if (fce->body != nullptr)
            emitCall (&fce_labels [fce], nullptr);
        else
        {
            void * addr = dlsym (RTLD_DEFAULT, fce->name);
            if (addr == nullptr)
                InstrError (string ("Undefined function: ") + fce->name);
            emitCall (nullptr, addr);
        }

        /* knihovni funkce vraci int, bool, char jen v dolni casti rax */
        TypePtr res = fce->type->from;
        if (fce->body == nullptr && res != nullptr && res->length != Long)
        {
            if (res->sort == Int || res->sort == Enum)
                a.movsxd (rax, eax);
            else if (res->sort == Char)
                a.movsx (rax, al);
            else if (res->sort == Bool)
                a.movzx (rax, al);
        }
    }

    void codeExpr (ExprPtr expr)
    {
        if (expr == nullptr) return;

        SYMBOL kind = expr->op;
        // see struct Expr in decl.h

        switch (kind)
        {
            case NUM:
                a.mov (rax, imm (strtol (expr->id, nullptr, 0)));
                break;

            case CHR:
                a.mov (rax, imm ((signed char) expr->id [0]));
                break;

            case TRUE_:
                a.mov (rax, 1);
                break;

            case FALSE_:
            case NULLPTR_:
                a.xor_ (eax, eax);
                break;

            case STR:
                strings.push_back (expr->id);
                a.mov (rax, imm ((intptr_t) strings.back ().c_str ()));
                break;

            case IDENT:
            case LBRACK:
            case PERIOD:
            case ARROW:
            case DEREF:
                codeLoad (expr);
                break;

            case ADR:
                codeAddress (expr->e2);
                break;

            case NEW_:
                codeNew (expr);
                break;

            case LPAR:
                codeExpr (expr->e1);
                break;

            case FCE:
                codeCall (expr);
                break;

            case ASSIGN:
            case ADDASSIGN: case SUBASSIGN: case MULASSIGN: case DIVASSIGN: case MODASSIGN:
            case SHLASSIGN: case SHRASSIGN: case ANDASSIGN: case ORASSIGN: case XORASSIGN:
                codeAssign (expr);
                break;

            case DPLUS:
            case DMINUS:
                codeIncDec (expr);
                break;

            case LOGAND:
            case LOGOR:
                codeAndOr (expr);
                break;

            case QUESTION:
                codeConditional (expr);
                break;

            case COMMA:
                codeExpr (expr->e1);
                codeExpr (expr->e2);
                break;

            case PLUS: case MINUS: case ASTERISK: case SLASH: case MOD:
            case SHL: case SHR: case AND: case OR: case XOR:
            case LESS: case LESSEQUAL: case GREATER: case GREATEREQUAL:
            case EQUAL: case UNEQUAL:
                codeBinary (expr);
                break;

            case UNARYPLUS:
                codeExpr (expr->e2);
                break;

            case UNARYMINUS:
                codeExpr (expr->e2);
                a.neg (rax);
                break;

            case UNARYNOT:            /* ~ */
                codeExpr (expr->e2);
                a.not_ (rax);
                break;

            case LOGNOT:              /* ! */
                codeExpr (expr->e2);
                a.test (rax, rax);
                a.sete (al);
                a.movzx (rax, al);
                break;

            default:
                InstrError ("Unsupported expression");
        }
    }

    /* ------------------------------------------------------------------ */

    /* vyhodnot podminku a nastav priznaky (ZF=1 <=> nepravda) */
    void codeCond (ExprPtr expr)
    {
        codeExpr (expr);
        a.test (rax, rax);
    }

    /* deklarace lokalnich promennych - inicializace */
    void codeLocalDecls (Queue & decl)
    {
        for (NodePtr p = decl.first; p != nullptr; p = p->next)
        {
            ObjectPtr obj = p->item;
            if (vars.find (obj) == vars.end ())
                continue;         /* typedef, extern, deklarace funkce */

            Variable & var = vars [obj];

            if (obj->init == nullptr)
            {
                if (var.ref)
                    InstrError (string ("Reference must be initialized: ") + obj->name);
                continue;
            }

            if (obj->init->op == LBRACE)
                InstrError (string ("Array initializers are not supported yet: ") + obj->name);

            if (var.ref)
                codeAddress (obj->init);
            else
            {
                codeExpr (obj->init);
                if (obj->type->sort == Bool)
                    normalizeBool ();
            }

            if (var.reg != nullptr)
                a.mov (*var.reg, rax);
            else
                a.mov (qword_ptr (rbp, var.offset), rax);
        }
    }

    void codeIf (StatPtr stat)
    {
        // if (e1) s1 else s2
        a.comment ("if statement");
        Label end_label = newLabel ();
        codeCond (stat->e1);

        if (stat->s2 == nullptr)
        {
            a.jz (end_label);
            codeStat (stat->s1);
        }
        else
        {
            Label else_label = newLabel ();
            a.jz (else_label);
            codeStat (stat->s1);
            a.jmp (end_label);

            a.comment ("else");
            a.bind (else_label);
            codeStat (stat->s2);
        }

        a.bind (end_label);
    }

    void codeWhile (StatPtr stat)
    {
        // while (e1) s1
        a.comment ("while statement");
        Label begin_label = newLabel ();
        Label end_label = newLabel ();

        a.bind (begin_label);
        codeCond (stat->e1);
        a.jz (end_label);

        loops.push_back ({ end_label, begin_label });
        codeStat (stat->s1);
        loops.pop_back ();
        a.jmp (begin_label);

        a.bind (end_label);
    }

    void codeDo (StatPtr stat)
    {
        // do s1 while (e1)
        a.comment ("do statement");
        Label begin_label = newLabel ();
        Label cond_label = newLabel ();
        Label end_label = newLabel ();

        a.bind (begin_label);
        loops.push_back ({ end_label, cond_label });
        codeStat (stat->s1);
        loops.pop_back ();

        a.bind (cond_label);
        codeCond (stat->e1);
        a.jnz (begin_label);

        a.bind (end_label);
    }

    void codeFor (StatPtr stat)
    {
        // for (e1;e2;e3) s1     nebo  for (deklarace; e2; e3) s1
        a.comment ("for statement");
        Label begin_label = newLabel ();
        Label next_label = newLabel ();
        Label end_label = newLabel ();

        codeLocalDecls (stat->decl);
        codeExpr (stat->e1);              /* inicializace */

        a.bind (begin_label);
        if (stat->e2 != nullptr)          /* for (;;) => bez testu */
        {
            codeCond (stat->e2);
            a.jz (end_label);
        }

        loops.push_back ({ end_label, next_label });
        codeStat (stat->s1);
        loops.pop_back ();

        a.bind (next_label);
        codeExpr (stat->e3);              /* inkrement */
        a.jmp (begin_label);

        a.bind (end_label);
    }

    void codeReturn (StatPtr stat)
    {
        if (stat->e1 != nullptr)
            codeExpr (stat->e1);
        else if (current_class != nullptr)
            loadThis ();              /* konstruktor vraci this */
        else
            a.xor_ (eax, eax);
        a.jmp (epilogue_label);
    }

    void codeCompoundStat (StatPtr stat)
    {
        // { s1 s1->next ... }
        for (StatPtr body = stat->s1; body != nullptr; body = body->next)
            codeStat (body);
    }

    void codeStat (StatPtr stat)
    {
        if (stat == nullptr) return;

        switch (stat->st)
        {
            case SIM:     codeExpr (stat->e1);           break;
            case LBRACE:  codeCompoundStat (stat);       break;
            case DECL:    codeLocalDecls (stat->decl);   break;
            case IF_:     codeIf (stat);                 break;
            case WHILE_:  codeWhile (stat);              break;
            case DO_:     codeDo (stat);                 break;
            case FOR_:    codeFor (stat);                break;
            case RETURN_: codeReturn (stat);             break;

            case BREAK_:
                if (loops.empty ()) InstrError ("break outside of loop");
                a.jmp (loops.back ().break_label);
                break;

            case CONTINUE_:
                if (loops.empty ()) InstrError ("continue outside of loop");
                a.jmp (loops.back ().continue_label);
                break;

            default:
                InstrError ("Unsupported statement");
        }
    }
};

} // namespace

/* ---------------------------------------------------------------------- */

long runInstructions5 (bool verbose)
{
    AsmModule module;
    return module.run (verbose);
}

void instructions5 (void)
{
    long result = runInstructions5 (true);
    cout << "main returned " << result << endl;
}
