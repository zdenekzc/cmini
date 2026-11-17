/* PASMLIB.H - assembler pasm as a library */

/* PASMLIB.H - assembler pasm as a library */

#ifndef PASMLIB_H
#define PASMLIB_H

#define PASM_OBJ     1     /*
   PasmAssemble (src, base, flags, msg, msg_size)

      src    source text (.asm file)
      base   output files without extension:  base.obj, base.bin, base.o,
             base.exe, base.lst, base.inl  (NULL => source name without extension)
      flags  PASM_OBJ | PASM_EXE | PASM_LST | PASM_INL | PASM_ELFOBJ | PASM_TOARU
             | PASM_PE | PASM_COFF
             (the same as the switches -o -e -l -i -c -T -w -m of the pasm program;
             PASM_OBJ and PASM_COFF cannot be combined - both write base.obj)
      msg    error message as in the pasm program:
             "file.asm(line,column) Error: text"  (NULL => not returned)

   result: 0 = no error, 1 = error (msg)

   PasmAssembleText (name, parts, count, base, flags, msg, msg_size)

      the same, but the source text is in memory: parts [0] .. parts [count - 1]
      (pieces of text one after another, each zero-terminated - e.g. lines or blocks
      of the generated program); name = name for error messages
      ("prog.asm"), the file is not read; base must not be NULL

   PasmAssembleList (name, prog, base, flags, msg, msg_size)

      the same, the program is a linked list of nodes (asmlist.h, phase 2):
      instructions and labels are read by pasm directly from the nodes, other nodes
      (directives, data, text) from their printed form; the line in a message = order of the node

   It can be called repeatedly in one process (each call starts from the beginning);
   memory from previous translations is not freed.

   The header is read by g++ and cparser (../cmini/instr.cc) and by gcc (compiled C).
*/
#define PASM_EXE     2     /* -o  OMF object (.obj) */
#define PASM_LST     4     /* -e  executable ELF / ELF64 (.bin) */
#define PASM_INL     8     /* -l  listing (.lst) */
#define PASM_ELFOBJ 16     /* -i  Turbo Pascal inline (.inl) */
#define PASM_TOARU  32     /* -c  relocatable ELF64 object (.o) */
#define PASM_PE     64     /* -T  ToaruOS target: different base of the image (with -e) */
#define PASM_COFF  128     /* -w  executable PE for Windows (.exe) */

#ifdef __cplusplus
extern "C"
#endif
int PasmAssemble (const char * src, const char * base, int flags, char * msg, int msg_size);

#ifdef __cplusplus
extern "C"
#endif
int PasmAssembleText (const char * name, const char * const * parts, int count,
                      const char * base, int flags, char * msg, int msg_size);

/* -m  COFF x64 object for Windows (.obj, not with -o) */
#ifdef __cplusplus
extern "C"
#endif
int PasmLink (const char * out, const char * const * inputs, int count, char * msg, int msg_size);

/*
   PasmLink (out, inputs, count, msg, msg_size)

      static linker ELF64 x86-64 for ToaruOS (code.c, section LINKER):
      inputs [0..count-1] = objects (.o) and archives (.a) in linker order,
      typically  crt0.o crti.o program.o libc.a crtn.o  from /lib of ToaruOS
      => out: standalone ELF64 ET_EXEC from 0x3F000000 (without ld.so on the host);
      0 = OK, otherwise "link: ..." in msg
*/
#ifdef __cplusplus
extern "C"
#endif
int PasmAssembleList (const char * name, const void * prog,
                      const char * base, int flags, char * msg, int msg_size);

#endif
