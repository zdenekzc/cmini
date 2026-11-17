/* INSTR.H */

#ifndef INSTR_H
#define INSTR_H

/* instr.c - x86-64 code generator in C (list of instructions asm/asmlist.h,
   assembler pasm as library asm/libpasm.a); this header is read by gcc, tcc,
   cmini (instr_drv.c, instr.c) and g++ (test_instr.cc, instr1.cc .. instr5.cc) */

#ifdef __cplusplus
extern "C" {   /* instr.c is C */
#endif

/* InstrOutputFile != NULL => runInstructions does not run the program, but saves the
   executable under this name (text for the assembler name.asm
   and listing name.lst) and returns 0 */
extern const char * InstrOutputFile;
/* InstrAsmFile != NULL => the generated text for the assembler (AsmPrint) is saved to this file
   (also on an assembler error; pasm mode, without -C) */
extern const char * InstrAsmFile;

/* InstrObjectFile != NULL => runInstructions creates only an ELF64 object
   (public functions and variables PUBLIC, missing ones EXTRN), main is not required */
extern const char * InstrObjectFile;

/* InstrToaru != 0 (or environment PASM_TARGET=toaru) => standalone program
   (own runtime library, pasm -e) for ToaruOS: ToaruOS syscall numbers,
   heap via sbrk, image from 0x3F000000 (pasm -T); no glibc involved, object mode has no effect */
extern int InstrToaru;

/* InstrToaruLib != NULL (cmini -L DIR) => program for ToaruOS with the ToaruOS libc:
   libc mode (pasm -c), but instead of gcc the static linker of pasm (PasmLink) with
   DIR/crt0.o crti.o libc.a crtn.o (from ToaruOS /lib); requires -o */
extern const char * InstrToaruLib;

/* translates the global declarations (glist), runs main and returns its result;
   verbose => prints the assembler listing (prog.lst) */
long runInstructions (int verbose);

/* translates, prints the listing, runs main and prints the result */
void instructions (void);

/* links the objects (and libraries -l..., .a files) with gcc; out != NULL =>
   saves the program, otherwise runs it and returns the result of main */
long InstrLink (char * * inputs, int n_inputs, const char * out);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

/* generators using the asmjit library (C++): instr1.cc .. instr5.cc */

/* translates the global declarations (glist), runs main and returns its result;
   verbose => prints the generated assembler */
long runInstructions1 (bool verbose);

/* translates, prints the assembler, runs main and prints the result */
void instructions1 ();

/* the same, variant instr2.cc (x86::Builder, intermediate results in registers instead of push / pop) */
long runInstructions2 (bool verbose);
void instructions2 ();

/* the same, variant instr3.cc (like instr2.cc + swapping operands as needed by registers) */
long runInstructions3 (bool verbose);
void instructions3 ();

/* the same, variant instr4.cc (like instr3.cc + call arguments in registers instead of the stack) */
long runInstructions4 (bool verbose);
void instructions4 ();

/* the same, variant instr5.cc (like instr4.cc + variables in registers, push/pop of callee-saved registers) */
long runInstructions5 (bool verbose);
void instructions5 ();

#endif

#endif
