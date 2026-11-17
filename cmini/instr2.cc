
/* INSTR1.CC */

/*
   Variant of instr1.cc: machine code generator for x86-64 using asmjit::x86::Builder.

   Difference from instr1.cc - binary operators (codeBinary):
     The left operand is not saved by a push instruction, but into a free register.
     Builder keeps the instructions as a list of nodes, into which instructions can also be inserted backwards:
       1. the left operand is generated (result in rax) and its position (cursor) is remembered,
       2. the right operand is generated, while tracking the HIGHEST used register
          in the order  rax, rcx, rdx, rsi, rdi, r8, r9, r10, r11, r12, r13, r14, r15
          (no register sets, just one number - max_reg),
       3. at the remembered position  mov <first register above max_reg>, rax  is inserted,
          and if no register is left, then  sub rsp, 8 / push rax  is inserted
          (16 bytes - stack alignment for function calls does not change).
     Registers r12 - r15 must be preserved by the function (callee-saved). If the function uses them,
     it saves them into reserved places in the frame - the save instructions are again inserted
     backwards to the start of the function, once the highest used register is known.

   Stack model:
     - the result of each (sub)expression is in register rax,
     - the left operand of a binary operator is stored in a free register (see above),
     - other intermediate results (indexes, addresses, call arguments) on the stack (push / pop),
     - rcx, rdx and r11 are auxiliary registers.

   Memory:
     - each variable (int, bool, char, pointer, ...) takes 8 bytes,
       an array of N elements takes N * 8 bytes,
     - global variables lie in memory allocated by the generator,
     - parameters and local variables lie in the function frame: [rbp - offset],
     - a reference (int & x) is stored as an address.

   Function calls - System V AMD64 convention:
     - parameters in rdi, rsi, rdx, rcx, r8, r9 (at most 6),
     - result in rax,
     - before the call instruction rsp is aligned to 16 bytes
       (that is why we count the stack depth - variable depth),
     - al = 0 (number of vector registers for functions with a variable number of parameters),
     - functions without a body (printf, rand, ...) are looked up in libraries via dlsym.

   Every function with a body has its own label and frame:
       push rbp / mov rbp, rsp / sub rsp, N  [mov [rbp-k], r12 ...]
       ...
       [mov r12, [rbp-k] ...]  mov rsp, rbp / pop rbp / ret

   At the start of the code there is a jump to main, so the entry point is the start of the code.

   Not supported yet: pointers (except passing them to library functions),
   structs, float / double, switch, goto, more than 6 parameters.
   */

#include "instr.h"
#include "decl.h"

#include "asmjit/x86.h"    /* asmjit.h is empty in the new version */
using namespace asmjit;
using namespace asmjit::x86;

/* Supported asmjit versions:
     - 1.24 and newer (_library/asmjit) - API in snake_case style,
     - 1.11 (asmjit-devel package in Fedora) - API in camelCase style.
   Intermediate versions were not tested. */

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
#include <iostream>
using namespace std;

/* ---------------------------------------------------------------------- */

static void InstrError (string msg)
{
    ::Error ((char *) msg.c_str ());
}

/* ---------------------------------------------------------------------- */

namespace {   /* the same class names as in instr1.cc */

/* asmjit reports errors via ErrorHandler.
   An exception cannot be thrown here (asmjit is noexcept),
   so we remember the first error and report it after the code has been generated. */

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

/* variable location */
struct Variable
{
    bool      global = false;
    int64_t * addr = nullptr;  /* global variable */
    int       offset = 0;      /* local variable: [rbp + offset] */
    bool      ref = false;     /* reference - in memory is the address */
};

struct LoopLabels
{
    Label break_label;
    Label continue_label;
};

static const int VAR_SIZE = 8; /* size of one variable in bytes */

/* registers in the order in which they are allocated for intermediate results */
static const Gp * const REGS [] =
{
    &rax, &rcx, &rdx, &rsi, &rdi, &r8, &r9, &r10, &r11, &r12, &r13, &r14, &r15
};

enum
{
    R_RAX = 0, R_RCX = 1, R_RDX = 2,
    R_R11 = 8,               /* highest register that the called function may change */
    R_R12 = 9,               /* r12 - r15 must be preserved by the function (callee-saved) */
    R_LAST = 12              /* r15 */
};

static const int CALLEE_SAVED_SLOTS = R_LAST - R_R12 + 1;   /* r12 .. r15 */

/* ---------------------------------------------------------------------- */

class AsmModule
{
public:
    JitRuntime      jit_runtime;
    CodeHolder      code_holder;
    StringLogger    logger;
    AsmErrorHandler error_handler;
    x86::Builder    a;

    vector <unique_ptr <int64_t []>> storage;  /* memory of global variables */
    deque <string>                   strings;  /* string constants */
    map <ObjectPtr, Variable>        vars;
    map <ObjectPtr, Label>           fce_labels;

    ObjectPtr current_fce = nullptr;
    Label     epilogue_label;
    int       frame_size = 0;          /* size of local variables */
    int       depth = 0;               /* number of 8-byte items on the stack (push) */
    int       max_reg = R_RAX;         /* highest used register (index into REGS) */
    int       fce_max_reg = R_RAX;     /* highest used register in the whole function */
    int       callee_saved_offset = 0; /* place in the frame for r12 .. r15 */
    vector <LoopLabels> loops;         /* for break and continue */

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

        if (main_fce == nullptr)
            InstrError ("Function main not found");

        a.comment ("entry point");
        a.jmp (fce_labels [main_fce]);

        for (NodePtr p = glist.first; p != nullptr; p = p->next)
            codeDecl (p->item);

        a.finalize ();   /* Builder => machine code */

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
        fflush (stdout);   /* printf output of the compiled program */

        jit_runtime.release (fn);
        return result;
    }

    /* ------------------------------------------------------------------ */

    /* differences in the API between asmjit versions */

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

    static const char * errorText (asmjit::Error err)
    {
#if ASMJIT_SNAKE_CASE_API
        return stringify_error (err);
#else
        return DebugUtils::errorAsString (err);
#endif
    }

    /* ------------------------------------------------------------------ */

    /* record the register usage - it is enough to remember the highest */
    void use (int reg)
    {
        if (reg > max_reg)
            max_reg = reg;
    }

    /* push / pop with counting of the stack depth */

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

    /* number of 8-byte variables occupied by the type (array => number of elements) */
    static long typeCount (TypePtr type)
    {
        if (type != nullptr && type->sort == Array)
        {
            long n;
            if (!constValue (type->range, n) || n <= 0)
                InstrError ("Array size must be a positive constant");
            return n * typeCount (type->from);
        }
        if (type != nullptr && (type->sort == Struct || type->sort == Union || type->sort == Class))
            InstrError ("Structures are not supported yet");
        if (type != nullptr && (type->sort == Float || type->sort == Double))
            InstrError ("Floating point types are not supported yet");
        if (type != nullptr && type->sort == StdString)
            InstrError ("std::string is supported only by instr.cc");
        if (type != nullptr && (type->sort == StdSequence || type->sort == StdDictionary))
            InstrError ("Sequence and Dictionary are supported only by instr.cc");
        return 1;
    }

    /* ------------------------------------------------------------------ */

    void allocGlobals ()
    {
        for (NodePtr p = glist.first; p != nullptr; p = p->next)
        {
            ObjectPtr obj = p->item;
            if (obj->fce || obj->storage == Typedef)
                continue;

            if (obj->type->sort == Reference)
                InstrError (string ("Global references are not supported: ") + obj->name);

            long count = typeCount (obj->type);
            int64_t * mem = new int64_t [count] ();   /* zeroed */
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
        var.ref = obj->type->sort == Reference;

        long count = var.ref ? 1 : typeCount (obj->type);
        frame_size += count * VAR_SIZE;
        var.offset = - frame_size;     /* array: elements from [rbp+offset] upward */
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
            allocLocal (obj);
        }
    }

    /* walk the function body and assign the local variables their place in the frame */
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

    void codeFunction (ObjectPtr decl)
    {
        static const Gp * arg_regs [] = { &rdi, &rsi, &rdx, &rcx, &r8, &r9 };

        current_fce = decl;
        epilogue_label = newLabel ();
        frame_size = 0;
        depth = 0;

        /* parameters */
        vector <ObjectPtr> params;
        for (NodePtr p = decl->type->list.first; p != nullptr; p = p->next)
        {
            ObjectPtr par = p->item;
            if (par->type == nullptr)
                InstrError (string ("Variable number of parameters is not supported: ") + decl->name);
            if (par->type->sort == Void)
                break;          /* int f (void) */
            if (par->type->sort == Array)
                InstrError (string ("Array parameters are not supported yet: ") + decl->name);
            params.push_back (par);
            allocLocal (par);
        }
        if (params.size () > 6)
            InstrError (string ("More than 6 parameters are not supported: ") + decl->name);

        /* local variables */
        collectLocals (decl->body);

        /* place for saving r12 .. r15 */
        frame_size += CALLEE_SAVED_SLOTS * VAR_SIZE;
        callee_saved_offset = - frame_size;
        int frame = (frame_size + 15) & ~15;   /* rsp stays aligned to 16 */

        a.comment ((string ("function ") + decl->name).c_str ());
        a.bind (fce_labels [decl]);
        a.push (rbp);
        a.mov (rbp, rsp);
        a.sub (rsp, frame);
        BaseNode * prologue_end = a.cursor ();  /* the saving of r12 .. r15 is inserted here later */

        for (size_t i = 0; i < params.size (); i++)
            a.mov (qword_ptr (rbp, vars [params [i]].offset), *arg_regs [i]);

        fce_max_reg = R_RAX;
        max_reg = R_RAX;
        codeStat (decl->body);

        a.xor_ (eax, eax);        /* no return => result 0 */
        a.bind (epilogue_label);

        /* restore the used callee-saved registers */
        for (int r = R_R12; r <= fce_max_reg; r++)
            a.mov (*REGS [r], calleeSavedSlot (r));

        a.mov (rsp, rbp);
        a.pop (rbp);
        a.ret ();

        /* saving of the callee-saved registers - insert backwards into the prologue */
        if (fce_max_reg >= R_R12)
        {
            BaseNode * end = a.cursor ();
            setCursor (prologue_end);
            for (int r = R_R12; r <= fce_max_reg; r++)
                a.mov (calleeSavedSlot (r), *REGS [r]);
            setCursor (end);
        }

        current_fce = nullptr;
    }

    /* ------------------------------------------------------------------ */

    Mem calleeSavedSlot (int reg)
    {
        return qword_ptr (rbp, callee_saved_offset + (reg - R_R12) * VAR_SIZE);
    }

    /* rax := (rax != 0) ? 1 : 0 */
    void normalizeBool ()
    {
        a.test (rax, rax);
        a.setne (al);
        a.movzx (rax, al);
    }

    /* rax := address of the variable (for a reference the address is stored in the variable) */
    void codeVarAddress (ExprPtr expr)
    {
        const Variable & var = variable (expr);
        if (var.global)
            a.mov (rax, imm ((intptr_t) var.addr));
        else
            a.lea (rax, qword_ptr (rbp, var.offset));

        if (var.ref)
            a.mov (rax, qword_ptr (rax));
    }

    /* rax := address of the l-value */
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
                if (arr->type->sort != Array)
                    InstrError ("Pointers are not supported yet");

                codeAddress (arr);
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
                InstrError ("Pointers are not supported yet");
                break;

            default:
                InstrError ("lvalue expected");
        }
    }

    /* rax := value of the variable or array element */
    void codeLoad (ExprPtr expr)
    {
        if (expr->type->sort == Array)
            codeAddress (expr);           /* array => address of the first element */
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
        codeExpr (expr->e1);                 /* rax = left operand */
        int left_max = max_reg;
        BaseNode * save_pos = a.cursor ();   /* the saving of rax is inserted here later */

        max_reg = R_RAX;
        codeExpr (expr->e2);                 /* rax = right operand */
        int right_max = max_reg;

        int save = right_max + 1;            /* first register that the right operand did not use */
        if (save < R_RCX) save = R_RCX;

        BaseNode * end = a.cursor ();
        setCursor (save_pos);
        if (save <= R_LAST)
            a.mov (*REGS [save], rax);
        else
        {
            a.sub (rsp, 8);                  /* 16 bytes => stack alignment does not change */
            a.push (rax);
        }
        setCursor (end);

        /* rax = left operand, rcx = right operand */
        if (save == R_RCX)
            a.xchg (rax, rcx);
        else if (save <= R_LAST)
        {
            a.mov (rcx, rax);
            a.mov (rax, *REGS [save]);
        }
        else
        {
            a.mov (rcx, rax);
            a.pop (rax);
            a.add (rsp, 8);
        }

        max_reg = max (left_max, right_max);
        use (save <= R_LAST ? save : R_RCX);
        codeOperation (expr->op);
        if (max_reg > fce_max_reg)
            fce_max_reg = max_reg;
    }

    void codeAndOr (ExprPtr expr)
    {
        Label end_label = newLabel ();

        codeExpr (expr->e1);
        a.test (rax, rax);

        if (expr->op == LOGAND)
            a.jz (end_label);    /* left = 0 => result 0 */
        else
            a.jnz (end_label);   /* left != 0 => result 1 */

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

        codeAddress (expr->e1);
        push (rax);
        codeExpr (expr->e2);
        use (R_R11);
        pop (r11);                /* r11 = address */

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

        codeAddress (operand);
        use (R_RDX);
        a.mov (rcx, rax);
        a.mov (rax, qword_ptr (rcx));

        if (postfix)
        {
            a.mov (rdx, rax);                   /* result = original value */
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

    void codeCall (ExprPtr expr)
    {
        static const Gp * arg_regs [] = { &rdi, &rsi, &rdx, &rcx, &r8, &r9 };

        ExprPtr f = skipParens (expr->e1);
        if (f->op != IDENT || f->obj == nullptr || !f->obj->fce)
            InstrError ("Only direct function calls are supported");
        ObjectPtr fce = f->obj;

        vector <ExprPtr> args;
        collectArgs (expr->e2, args);

        /* missing arguments => default values of parameters:  int level = 0 */
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

        if (args.size () > 6)
            InstrError (string ("More than 6 arguments are not supported: ") + fce->name);

        /* arguments one after another onto the stack */
        NodePtr par = fce->type->list.first;
        for (ExprPtr arg : args)
        {
            TypePtr ptype = (par != nullptr) ? par->item->type : nullptr;   /* nullptr => ... */

            if (ptype != nullptr && ptype->sort == Reference)
                codeAddress (arg);                 /* reference => address */
            else
                codeExpr (arg);
            push (rax);

            if (ptype != nullptr) par = par->next;
        }

        /* from the stack into registers */
        for (int i = (int) args.size () - 1; i >= 0; i--)
            pop (*arg_regs [i]);

        use (R_R11);                               /* the called function may change rax .. r11 */

        bool pad = (depth % 2) != 0;               /* alignment of rsp to 16 bytes */
        if (pad) a.sub (rsp, 8);

        a.xor_ (eax, eax);                         /* al = 0 vector registers */

        if (fce->body != nullptr)
        {
            a.call (fce_labels [fce]);
        }
        else
        {
            void * addr = dlsym (RTLD_DEFAULT, fce->name);
            if (addr == nullptr)
                InstrError (string ("Undefined function: ") + fce->name);
            a.mov (r11, imm ((intptr_t) addr));
            a.call (r11);
        }

        if (pad) a.add (rsp, 8);

        /* library functions return int, bool, char only in the low part of rax */
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
                codeLoad (expr);
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

    /* evaluate the condition and set the flags (ZF=1 <=> false) */
    void codeCond (ExprPtr expr)
    {
        codeExpr (expr);
        a.test (rax, rax);
    }

    /* declarations of local variables - initialization */
    void codeLocalDecls (Queue & decl)
    {
        for (NodePtr p = decl.first; p != nullptr; p = p->next)
        {
            ObjectPtr obj = p->item;
            if (vars.find (obj) == vars.end ())
                continue;         /* typedef, extern, function declaration */

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
        // for (e1;e2;e3) s1     or  for (declaration; e2; e3) s1
        a.comment ("for statement");
        Label begin_label = newLabel ();
        Label next_label = newLabel ();
        Label end_label = newLabel ();

        codeLocalDecls (stat->decl);
        codeExpr (stat->e1);              /* initialization */

        a.bind (begin_label);
        if (stat->e2 != nullptr)          /* for (;;) => without test */
        {
            codeCond (stat->e2);
            a.jz (end_label);
        }

        loops.push_back ({ end_label, next_label });
        codeStat (stat->s1);
        loops.pop_back ();

        a.bind (next_label);
        codeExpr (stat->e3);              /* increment */
        a.jmp (begin_label);

        a.bind (end_label);
    }

    void codeReturn (StatPtr stat)
    {
        if (stat->e1 != nullptr)
            codeExpr (stat->e1);
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

long runInstructions2 (bool verbose)
{
    AsmModule module;
    return module.run (verbose);
}

void instructions2 (void)
{
    long result = runInstructions2 (true);
    cout << "main returned " << result << endl;
}
