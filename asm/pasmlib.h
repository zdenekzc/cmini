/* PASMLIB.H - assembler pasm jako knihovna */

/*
   PasmAssemble (src, base, flags, msg, msg_size)

      src    zdrojovy text (soubor .asm)
      base   vystupni soubory bez pripony:  base.obj, base.bin, base.o,
             base.exe, base.lst, base.inl  (NULL => jmeno src bez pripony)
      flags  PASM_OBJ | PASM_EXE | PASM_LST | PASM_INL | PASM_ELFOBJ | PASM_TOARU
             | PASM_PE | PASM_COFF
             (stejne jako prepinace -o -e -l -i -c -T -w -m programu pasm;
             PASM_OBJ a PASM_COFF zaroven nelze - oba pisi base.obj)
      msg    chybove hlaseni jako u programu pasm:
             "soubor.asm(radka,sloupec) Error: text"  (NULL => nevraci se)

   vysledek: 0 = bez chyby, 1 = chyba (msg)

   PasmAssembleText (name, parts, count, base, flags, msg, msg_size)

      totez, zdrojovy text je v pameti: parts [0] .. parts [count - 1]
      (casti textu za sebou, ukoncene nulou - napr. radky nebo bloky
      generovaneho programu); name = jmeno pro chybova hlaseni
      ("prog.asm"), soubor se necte; base nesmi byt NULL

   PasmAssembleList (name, prog, base, flags, msg, msg_size)

      totez, program je spojovy seznam uzlu (asmlist.h, faze 2):
      instrukce a navesti cte pasm primo z uzlu, ostatni uzly (direktivy,
      data, text) z jejich tisku;  radka v hlaseni = poradi uzlu

   Lze volat opakovane v jednom procesu (kazde volani zacina od zacatku);
   pamet z predchozich prekladu se neuvolnuje.

   Hlavicku cte g++ a cparser (../cmini/instr.cc) i gcc (prelozene C).
*/

#ifndef PASMLIB_H
#define PASMLIB_H

#define PASM_OBJ     1     /* -o  objekt OMF (.obj) */
#define PASM_EXE     2     /* -e  spustitelny soubor ELF / ELF64 (.bin) */
#define PASM_LST     4     /* -l  vypis (.lst) */
#define PASM_INL     8     /* -i  Turbo Pascal inline (.inl) */
#define PASM_ELFOBJ 16     /* -c  relokovatelny objekt ELF64 (.o) */
#define PASM_TOARU  32     /* -T  cil ToaruOS: jina zakladna obrazce (s -e) */
#define PASM_PE     64     /* -w  spustitelny soubor PE pro Windows (.exe) */
#define PASM_COFF  128     /* -m  objekt COFF x64 pro Windows (.obj, ne s -o) */

#ifdef __cplusplus
extern "C"
#endif
int PasmAssemble (const char * src, const char * base, int flags, char * msg, int msg_size);

#ifdef __cplusplus
extern "C"
#endif
int PasmAssembleText (const char * name, const char * const * parts, int count,
                      const char * base, int flags, char * msg, int msg_size);

/*
   PasmLink (out, inputs, count, msg, msg_size)

      staticky linker ELF64 x86-64 pro ToaruOS (code.c, oddil LINKER):
      inputs [0..count-1] = objekty (.o) a archivy (.a) v poradi linkeru,
      typicky  crt0.o crti.o program.o libc.a crtn.o  z /lib ToaruOS
      => out: samostatny ELF64 ET_EXEC od 0x3F000000 (bez ld.so v hostovi);
      0 = v poradku, jinak "link: ..." v msg
*/
#ifdef __cplusplus
extern "C"
#endif
int PasmLink (const char * out, const char * const * inputs, int count, char * msg, int msg_size);

/* prog = const AsmProgram * (asmlist.h); void - hlavicku cte i cparser */
#ifdef __cplusplus
extern "C"
#endif
int PasmAssembleList (const char * name, const void * prog,
                      const char * base, int flags, char * msg, int msg_size);

#endif
