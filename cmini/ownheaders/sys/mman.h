/* sys/mman.h - C-- (v C <sys/mman.h>; PROT_..., MAP_... jsou v C makra) */
#ifndef CMM_SYS_MMAN_H
#define CMM_SYS_MMAN_H
#include <stddef.h>
/* hodnoty jako makra glibc (Linux x86-64) - plati i v nasm, pasm, asm64, llvm */
enum { PROT_READ = 1, PROT_WRITE = 2, PROT_EXEC = 4, MAP_PRIVATE = 2, MAP_ANONYMOUS = 0x20 };
#define MAP_FAILED ((void *) -1)
void * mmap (void * addr, size_t len, int prot, int flags, int fd, long offset);
int mprotect (void * addr, size_t len, int prot);
int munmap (void * addr, size_t len);
#endif
