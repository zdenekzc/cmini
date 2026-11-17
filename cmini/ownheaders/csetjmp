/* csetjmp - cparser (glibc, x86-64) */

#ifndef CPARSER_CSETJMP
#define CPARSER_CSETJMP

typedef long jmp_buf [25];          /* glibc: struct __jmp_buf_tag [1] - 200 bajtu */

int  _setjmp (jmp_buf env);         /* setjmp je v glibc makro */
void longjmp (jmp_buf env, int value);

#define setjmp(env) _setjmp (env)

#endif
