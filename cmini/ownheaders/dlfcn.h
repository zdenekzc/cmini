/* dlfcn.h - cparser / C-- (v C <dlfcn.h>; RTLD_DEFAULT je v C makro) */
#ifndef CMM_DLFCN_H
#define CMM_DLFCN_H
#define RTLD_DEFAULT ((void *) 0)   /* jako glibc - plati i v nasm, pasm, asm64, llvm */
void * dlsym (void * handle, const char * name);
#define RTLD_NOW 2
#define RTLD_GLOBAL 256
void * dlopen (const char * file, int mode);
#endif
