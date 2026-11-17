
/* LEX.H */

/**************************************************************************/
/*                                                                        */
/*                            Lexical Analysis                            */
/*                                                                        */
/**************************************************************************/

#ifndef LEX_H
#define LEX_H

#include <stddef.h>   /* size_t */
#ifndef __cplusplus
#include <stdbool.h>  /* bool pro C */
#endif

#ifdef __cplusplus
extern "C" {   /* front-end je v cistem C (decl.c, ...); g++ (instr.cc, test_instr.cc) */
#endif

#define QUOTE1    '\''
#define QUOTE2    '\"'
#define BACKSLASH '\\'

#define IDENT_SIZE 80
typedef char IDENT_TYP [IDENT_SIZE]; /* identifikatory */

#define STR_SIZE 256
typedef char STR_TYP [STR_SIZE]; /* retezce znaku */

#define NUM_SIZE 32
typedef char NUM_TYP [NUM_SIZE]; /* ciselne konstanty */

enum SYMBOL
{
    AUTO_,
    BREAK_,
    CASE_,
    CHAR_,
    CONST_,
    CONTINUE_,
    DEFAULT_,
    DO_,
    DOUBLE_,
    ELSE_,
    ENUM_,
    EXTERN_,
    FLOAT_,
    FOR_,
    GOTO_,
    IF_,
    INT_,
    LONG_,
    REGISTER_,
    RETURN_,
    SHORT_,
    SIGNED_,
    SIZEOF_,
    STATIC_,
    STRUCT_,
    SWITCH_,
    TYPEDEF_,
    UNION_,
    UNSIGNED_,
    VOID_,
    VOLATILE_,
    WHILE_,

    /* C++ */
    BOOL_,
    FALSE_,
    INLINE_,
    NEW_,
    NULLPTR_,
    TRUE_,
    CLASS_,
    PRIVATE_,
    PROTECTED_,
    PUBLIC_,
    THIS_,
    VIRTUAL_,
    STATIC_CAST_,       /* static_cast <T> (e), (T) e - uzel pretypovani */
    REINTERPRET_CAST_,
    CONST_CAST_,
    DYNAMIC_CAST_,
    FRIEND_,
    DELETE_,

    PLUS,         /* +  */
    MINUS,        /* -   */
    ASTERISK,     /* *   */
    SLASH,        /* /   */
    PERIOD,       /* .   */
    COMMA,        /* ,   */
    COLON,        /* :   */
    SEMICOLON,    /* ;   */
    EQUAL,        /* ==  */
    LESS,         /* <   */
    GREATER,      /* >   */
    UNEQUAL,      /* !=  */
    LESSEQUAL,    /* <=  */
    GREATEREQUAL, /* >=  */
    ARROW,        /* ->  */
    ASSIGN,       /* =   */
    LPAR,         /* (   */
    RPAR,         /* )   */
    LBRACK,       /* [   */
    RBRACK,       /* ]   */

    LBRACE,       /* {   */
    RBRACE,       /* }   */
    TILDA,        /* ~   */
    PERCENT,      /* %   */
    CARET,        /* ^   */
    AND,          /* &   */
    BAR,          /* |   */
    QUESTION,     /* ?   */

    DPLUS,        /* ++  */
    DMINUS,       /* --  */
    DCOLON,       /* ::  */
    DOTS,         /* ... */

    SHL,          /* <<  */
    SHR,          /* >>  */
    LOGNOT,       /* !   */
    LOGAND,       /* &&  */
    LOGOR,        /* ||  */

    MULASSIGN,    /* *=  */
    DIVASSIGN,    /* /=  */
    MODASSIGN,    /* %=  */
    ADDASSIGN,    /* +=  */
    SUBASSIGN,    /* -=  */
    SHLASSIGN,    /* <<= */
    SHRASSIGN,    /* >>= */
    ANDASSIGN,    /* &=  */
    ORASSIGN,     /* |=  */
    XORASSIGN,    /* ^=  */

    IDENT,        /* identifier */
    NUM,          /* number */
    FLT,          /* floating point number */
    STR,          /* string    "" */
    CHR,          /* character '' */

    /* expressions */

    FCE,          /* function call */
    ADR,          /* unary & */
    DEREF,        /* unary * */
    UNARYPLUS,    /* unary + */
    UNARYMINUS,   /* unary - */
    UNARYNOT,     /* unary ~ */

    /* statements */

    SIM,          /* simple statement: expression or null */
    LAB,          /* label */
    DECL,         /* declaration */

    EOS           /* end of source */
};

#ifndef __cplusplus
typedef enum SYMBOL SYMBOL;  /* C: enum tagy nejsou implicitni typy */
#endif

#define SYMBOL_MAX EOS /* posledni symbol */

#define OR       BAR
#define XOR      CARET
#define NOT      TILDA
#define MOD      PERCENT

extern SYMBOL    sy;           /* zpracovavany symbol */

extern IDENT_TYP IdentVal;     /* identifikator */
extern NUM_TYP   NumVal;       /* cislo (jako posloupnost znaku) */
extern STR_TYP   StrVal;       /* retezec (bez uvozovek) */
extern char      ChrVal;       /* znak */

extern bool      l_suffix;     /* znacky na konci cisla */
extern bool      u_suffix;
extern bool      f_suffix;     

/* prehravani ulozenych symbolu (decl.c: tela metod uvnitr tridy se prekladaji
   az na konci tridy); != NULL => NextSymbol zavola tuto funkci misto cteni vstupu */
extern void (*ReplaySymbol) (void);
void NamespaceQual (void);   /* decl.c: X::jmeno => jmeno (prostor jmen X) */

/* pozice pro chybova hlaseni: aktualni (vlozeny) soubor a radka v nem;
   LexCurrentFile vraci NULL, pokud se cte hlavni text (lex.c vzdy NULL) */
const char * LexCurrentFile (void);
int          LexCurrentLine (void);

/* pozice prave prehravaneho symbolu (decl.c - tela metod uvnitr tridy) */
extern const char * ReplayFile;
extern int          ReplayLine;

void InitLex     (void);       /* inicializace - cte prvni symbol */
void NextSymbol  (void);       /* precti dalsi symbol */
void CheckSymbol (SYMBOL par); /* zkontroluj, zda je na vstupu zadany symbol */

void PutSymbol (SYMBOL par);   /* zobraz symbol na vystupu */
void PutCurrentSymbol (void);  /* zobraz prave zpracovavany symbol na vystupu */

/******************************* UTILITIES ********************************/

#define CLR_ARR(x) memset (x, 0, sizeof (x));
#define CLR_REC(x) memset (&(x), 0, sizeof (x));

void strmov (char * a, size_t size, char * b);
void stradd (char * a, size_t size, char * b);

#ifdef __cplusplus
}
#endif

#endif
