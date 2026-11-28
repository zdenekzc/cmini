/* stdbool.h - cparser: bool, true and false are built into c-mini
   (lex.c: the keyword BOOL_); for a C compiler that is given this directory
   the standard macros are provided */

#ifndef CPARSER_STDBOOL
#define CPARSER_STDBOOL

#ifndef __CPARSER__
#ifndef __cplusplus
   #define bool  _Bool
   #define true  1
   #define false 0
#endif
#endif

#define __bool_true_false_are_defined 1

#endif
