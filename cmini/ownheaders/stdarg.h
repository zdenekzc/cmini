/* cstdarg - cparser: jen typ va_list, aby se dal precist zdrojovy text
   (va_start, va_end, vsnprintf jsou implicitni funkce); generatory instr*.cc
   promenny pocet parametru nepodporuji, translate.c vlozi skutecny <stdarg.h> */

#ifndef CPARSER_CSTDARG
#define CPARSER_CSTDARG

typedef char * va_list;

#endif
