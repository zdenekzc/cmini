#!/bin/sh
# 02-cmini-pasm.sh - _output/cmini-pasm/: c-mini (cmini/instr.c) => assembler pasm (bez gcc
# u prekladu programu, s vlastni knihovnou; glibc jen tam, kde se linkuje gcc)
#
# CESTA PREKLADU (kazdy krok je v tomto souboru, pod nim je jeho vystup):
#   0. make -C cmini: test_instr.bin, test_instr_lexer.bin, cmini.bin (i s prekladem do C, -C);
#      asm/libpasm.a (pasm jako knihovna). Bez toho se nic nesestavi.
#   1. cmini-pasm-libc (examples/): tests/examples/<p>.cc => cmini/test_instr.bin -0 -o
#      - instr.c vygeneruje seznam uzlu programu, pasm z nej udela .asm a .lst;
#        ve vychozim rezimu (CPARSER_RTL neni) pasm -c => prog.o a gcc -no-pie => prog.bin
#        (glibc: printf, malloc, ...). Spusti se run (vystup v .out, porovna se s vzorem
#        tests/examples/expected/<p>.txt; quicksort, heapsort jen posledni radek "O.K.").
#   2. cmini-pasm-ownlib (examples-ownlib/): CPARSER_RTL=own => vlastni behova knihovna
#      (RTL_DATA/RTL_CODE v cmini/instr.c) => pasm -e => samostatny ELF64 BEZ gcc a glibc.
#      Overeni: readelf -d bez NEEDED. Program se take spusti a porovna se vzorem.
#   3. zdroje cmini po souborech (SOURCES v common.sh: cmini/*.cc|*.c, asm/*.c): cmini_pasm =
#      test_instr_lexer.bin -0 -c -S (preprocesor cparseru, -I ownheaders)
#      => objekt .o (ukladá se do $T) a text pro assembler $V/cmini-pasm/<dir>/<zdroj>.asm.
#   4. cmini sam sebou: cmini.bin z pasm (instr_drv.c translate.c front-end instr.c libpasm.a)
#      => shoda .asm s referencnim cmini z gcc (dama, instr.c); translate.c => C shodne.
#   5. asm/pasm.bin z pasm zdroju (asm/*.c) => testy asm/tests/run.sh (pasm z gcc a z pasm).
#   6. cmini.bin -S soubor.asm (prepinac -S): ulozeni vygenerovaneho .asm (dama-S.asm).
#
# VYSTUP: _output/cmini-pasm/{examples,examples-ownlib,cmini,asm}/ (.asm, .lst, .bin, .out)
# POZOR: jeden NN-*.sh se nesmi spustit dvakrat soucasne (sdileji _output a _build/vystup).

. "$(dirname "$0")/common.sh"
# preklad cmini (test_instr, test_instr_lexer, translate, cmini.bin; make si preklad libpasm.a sam)
make -s -C "$TOP/cmini" || exit 1
begin cmini-pasm

# cmini-pasm-libc: programy s glibc (vychozi CPARSER_RTL, pasm -c + gcc)
D=$V/cmini-pasm/examples; mkdir -p $D
for p in $PROGRAMS
do
   ./cmini/test_instr.bin -0 -o $T/$p $EX/$p.cc > /dev/null 2>&1 && cp $T/$p.asm $T/$p.lst $D/ && cp $T/$p $D/$p.bin
   run $D $p
done
# cout / cerr (C++ iostream): cmini => printf, dprintf (fflush pred cerr); vzor = g++ s iostream
for p in cout
do
   ./cmini/test_instr.bin -0 -o $T/$p $EX/$p.cc > /dev/null 2>&1 && cp $T/$p.asm $T/$p.lst $D/ && cp $T/$p $D/$p.bin
   run $D $p
done
# cmini-pasm-ownlib: vlastni behova knihovna (CPARSER_RTL=own), pasm -e, samostatny ELF
# bez gcc a bez libc - zadna zavislost na knihovnach (DT_NEEDED)
D2=$V/cmini-pasm/examples-ownlib; mkdir -p $D2
for p in dama jezdec avl classes strings defaults chars dtors
do
   CPARSER_RTL=own ./cmini/test_instr.bin -0 -o $T/own-$p $EX/$p.cc > /dev/null 2>&1 && cp $T/own-$p.asm $D2/$p.asm && cp $T/own-$p.lst $D2/$p.lst && cp $T/own-$p $D2/$p.bin
   [ -x $D2/$p.bin ] && ! readelf -d $D2/$p.bin 2>/dev/null | grep -q NEEDED || { echo "$p-ownlib" >> $T/bad; continue; }
   run $D2 $p
done
cmini_pasm ()   # zdroj prekladace => objekt (cparser s preprocesorem, -c) a text pro assembler (-S)
{
   rm -f $T/src.asm $T/src.o
   d=$(dirname $1); (cd $d && $TOP/cmini/test_instr_lexer.bin -0 -c -S -I $TOP/cmini/ownheaders -o $T/src.o $(basename $1)) &&
      [ -s $T/src.o ] && cp $T/src.asm $2.asm
}
sources cmini-pasm cmini_pasm
# c-mini sam sebou: prekladac cmini.bin (instr_drv.c + translate.c + front-end + instr.c + asm/libpasm.a)
# - cmini.bin -C (preklad do C) prelozeny pasmem se shoduje s cmini.bin z gcc
D=$V/cmini-pasm/cmini; mkdir -p $D
(cd cmini && $TOP/cmini/test_instr_lexer.bin -0 -I . -I $TOP/cmini/ownheaders -o $D/cmini.bin \
    instr_drv.c translate.c decl.c expr.c stat.c sem.c util.c lexer.c instr.c $TOP/asm/libpasm.a > /dev/null 2>&1) &&
   $D/cmini.bin -I cmini/ownheaders -o $T/self-dama $EX/dama.cc > /dev/null 2>&1 &&
   ./cmini/test_instr_lexer.bin -0 -I cmini/ownheaders -o $T/ref-dama $EX/dama.cc > /dev/null 2>&1 &&
   cmp -s $T/self-dama.asm $T/ref-dama.asm &&
   $D/cmini.bin -I cmini/ownheaders -I cmini -o $T/self-instr -c $T/self-instr.o cmini/instr.c > /dev/null 2>&1 &&
   cmp -s $T/self-instr.asm $D/instr.asm && echo "cmini" >> $T/ok
(cd cmini && $D/cmini.bin -C -I ownheaders -o $T/self-tr.c translate.c) &&
   (cd cmini && $TOP/cmini/cmini.bin -C -I ownheaders -o $T/self-tr0.c translate.c) &&
   cmp -s $T/self-tr.c $T/self-tr0.c && echo "translate" >> $T/ok
rm -f $D/*.bin.asm $D/*.bin.lst
# assembler pasm (asm/*.c) prelozeny c-mini: asm/pasm.bin, testy asm/tests/run.sh jako pasm z gcc
mkdir -p $V/cmini-pasm/asm
(cd asm && $TOP/cmini/test_instr_lexer.bin -0 -I $TOP/cmini/ownheaders -o $V/cmini-pasm/asm/pasm.bin \
    pasm.c pasmlib.c code.c def.c expr.c inst.c key.c oth.c asmlist.c > /dev/null 2>&1) &&
   ASM_OUTPUT=$T/pasm-tests sh asm/tests/run.sh $V/cmini-pasm/asm/pasm.bin > $T/pasm-tests.log 2>&1 &&
   echo "pasm" >> $T/ok
rm -f $V/cmini-pasm/asm/pasm.bin.asm $V/cmini-pasm/asm/pasm.bin.lst
# -S: ulozeni vygenerovaneho assembleru (prepinac cmini.bin -S soubor.asm)
if $TOP/cmini/cmini.bin -S $V/cmini-pasm/examples/dama-S.asm -I $TOP/cmini/ownheaders $EX/dama.cc > /dev/null 2>&1 &&
   [ -s $V/cmini-pasm/examples/dama-S.asm ]
then echo "  OK    cmini.bin -S: $V/cmini-pasm/examples/dama-S.asm"
else echo "  FAIL  cmini.bin -S (dama)"; FAILED=1
fi
report cmini-pasm "c-mini => pasm (.asm), vypis (.lst); cmini.bin (i -C) a asm/pasm.bin (sam sebou)"
finish
