/* INSTR.H */

#ifndef INSTR_H
#define INSTR_H

/* instr.c - generator kodu x86-64 v C (seznam instrukci asm/asmlist.h,
   assembler pasm jako knihovna asm/libpasm.a); hlavicku ctou gcc, tcc,
   cmini (instr_drv.c, instr.c) i g++ (test_instr.cc, instr1.cc .. instr5.cc) */

#ifdef __cplusplus
extern "C" {   /* instr.c je C */
#endif

/* InstrOutputFile != NULL => runInstructions program nespusti, ulozi
   spustitelny soubor pod timto jmenem (text pro assembler jmeno.asm
   a vypis jmeno.lst) a vrati 0 */
extern const char * InstrOutputFile;
/* InstrAsmFile != NULL => vygenerovany text pro assembler (AsmPrint) se ulozi do tohoto souboru
   (i pri chybe assembleru; rezim pasm, bez -C) */
extern const char * InstrAsmFile;

/* InstrObjectFile != NULL => runInstructions vytvori jen objekt ELF64
   (verejne funkce a promenne PUBLIC, chybejici EXTRN), main neni povinna */
extern const char * InstrObjectFile;

/* InstrToaru != 0 (nebo prostredi PASM_TARGET=toaru) => samostatny program
   (vlastni behova knihovna, pasm -e) pro ToaruOS: cisla syscallu ToaruOS,
   halda pres sbrk, obraz od 0x3F000000 (pasm -T); s glibc / objektem bez vlivu */
extern int InstrToaru;

/* InstrToaruLib != NULL (cmini -L DIR) => program pro ToaruOS s libc ToaruOS:
   rezim libc (pasm -c), ale misto gcc staticky linker pasm (PasmLink) s
   DIR/crt0.o crti.o libc.a crtn.o (z /lib ToaruOS); vyzaduje -o */
extern const char * InstrToaruLib;

/* prelozi globalni deklarace (glist), spusti main a vrati jeho vysledek;
   verbose => vytiskne vypis assembleru (prog.lst) */
long runInstructions (int verbose);

/* prelozi, vytiskne vypis, spusti main a vytiskne vysledek */
void instructions (void);

/* slinkuje objekty (a knihovny -l..., soubory .a) gcc; out != NULL =>
   ulozi program, jinak ho spusti a vrati vysledek main */
long InstrLink (char * * inputs, int n_inputs, const char * out);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

/* generatory s knihovnou asmjit (C++): instr1.cc .. instr5.cc */

/* prelozi globalni deklarace (glist), spusti main a vrati jeho vysledek;
   verbose => vytiskne vygenerovany assembler */
long runInstructions1 (bool verbose);

/* prelozi, vytiskne assembler, spusti main a vytiskne vysledek */
void instructions1 ();

/* totez, varianta instr2.cc (x86::Builder, mezivysledky v registrech misto push / pop) */
long runInstructions2 (bool verbose);
void instructions2 ();

/* totez, varianta instr3.cc (jako instr2.cc + prehazovani operandu podle potreby registru) */
long runInstructions3 (bool verbose);
void instructions3 ();

/* totez, varianta instr4.cc (jako instr3.cc + argumenty volani v registrech misto zasobniku) */
long runInstructions4 (bool verbose);
void instructions4 ();

/* totez, varianta instr5.cc (jako instr4.cc + promenne v registrech, push/pop callee-saved registru) */
long runInstructions5 (bool verbose);
void instructions5 ();

#endif

#endif
