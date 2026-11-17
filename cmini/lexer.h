/* LEXER.H */

/**************************************************************************/
/*                                                                        */
/*               Lexical Analysis with Preprocessor                       */
/*                                                                        */
/**************************************************************************/

/*
   Alternative to lex.c - the same interface (lex.h: sy, IdentVal, NumVal, ...,
   InitLex, NextSymbol, CheckSymbol, ...), and in addition handles the preprocessor
   directives:

      #define, #undef               macros with and without parameters,
                                    operators # and ##, __VA_ARGS__
      #if, #ifdef, #ifndef,
      #elif, #else, #endif          conditional compilation, expressions with defined
      #include "file"               searches in the directory of the source file
                                    and in the directories from LexIncludeDir
      #include <file>               only in the directories from LexIncludeDir,
                                    files not found are ignored
                                    (e.g. <stdio.h>, as in lex.c)
      #error                        compilation error
      #pragma, #line, #warning      ignored

      __LINE__, __FILE__            predefined macros

   Either lex.c or lexer.c is linked into the program.
*/

#ifndef LEXER_H
#define LEXER_H

#include <stddef.h> /* size_t (in lex.h) */
#include "lex.h"

#ifdef __cplusplus
extern "C" {   /* front-end is plain C (decl.c, ...); g++ (instr.cc, test_instr.cc) */
#endif

void LexFileName   (const char * name);   /* name of the source file (for #include "..." and __FILE__) */
void LexIncludeDir (const char * dir);    /* add a directory for #include (like -I) */
void LexDefine     (const char * name, const char * value); /* macro without parameters (like -D) */
void LexUndefine   (const char * name);
void LexClear      (void);                /* remove directories and macros from LexDefine */

/* LexCurrentFile, LexCurrentLine - see lex.h */

#ifdef __cplusplus
}
#endif

#endif
