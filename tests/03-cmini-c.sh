#!/bin/sh
# 03-cmini-c.sh - _output/cmini-c/: c-mini => C (cmini.bin -C, translate.c) => gcc => binarky
#
# CESTA PREKLADU (kazdy krok je v tomto souboru, pod nim je jeho vystup):
#   0. begin: _output/cmini-c se smaze, pracovni adresar $T=_build/vystup/cmini-c; gcc
#      prelozi cminirt.c (behova knihovna C) do $T/cminirt.o; vzorove vystupy se zkopiruji.
#   1. examples/ (tests/examples/<p>.cc): cmini.bin -C -I ownheaders -o examples/<p>.c
#      (translate.c: C-mini => C; na stdout, kdyz neni -o) => gcc CMINI_CFLAGS + cminirt.o
#      => <p>.bin; run porovna vystup s tests/examples/expected/<p>.txt.
#   2. zdroje cmini/asm po souborech (SOURCES v common.sh): cmini_c = cmini.bin -C
#      => <zdroj>.c (ponecha se v _output/cmini-c/<dir>/) => gcc -c => objekt v $T.
#   3. cmini.bin z C: objekty instr_drv, decl, expr, stat, sem, util, lexer, translate, instr
#      + cminirt.o + asm/libpasm.a => cmini.bin; ten preklada translate.c znovu do C a
#      vysledek se shoduje s cmini/translate.c z gcc (sam sebe = pevny bod).
#
# VYSTUP: _output/cmini-c/{examples,cmini,asm}/ (.c, .o, .bin, .out)
# POZOR: jeden NN-*.sh se nesmi spustit dvakrat soucasne (sdileji _output a _build/vystup).

. "$(dirname "$0")/common.sh"
begin cmini-c

D=$V/cmini-c/examples; mkdir -p $D
for p in $PROGRAMS
do
   ./cmini/cmini.bin -C -I cmini/ownheaders -o $D/$p.c $EX/$p.cc > /dev/null 2>&1 &&
      gcc $CMINI_CFLAGS $D/$p.c $T/cminirt.o -o $D/$p.bin -lm 2> /dev/null
   run $D $p
done
# cout / cerr (C++ iostream => printf, dprintf; viz tests/02 a kap. 86 v CHANGELOG.txt)
for p in cout
do
   ./cmini/cmini.bin -C -I cmini/ownheaders -o $D/$p.c $EX/$p.cc > /dev/null 2>&1 &&
      gcc $CMINI_CFLAGS $D/$p.c $T/cminirt.o -o $D/$p.bin -lm 2> /dev/null
   run $D $p
done
cmini_c ()   # zdroj => C, ponecha se, kdyz ho prelozi gcc
{
   d=$(dirname $1); (cd $d && $TOP/cmini/cmini.bin -C -I $TOP/cmini/ownheaders -o $2.c $(basename $1)) &&
      gcc $CMINI_CFLAGS -c $2.c -o $2.o && mv $2.o $T/tr-$(basename $(dirname $2))-$(basename $2).o
}
sources cmini-c cmini_c
ok=true; objs=""
for f in instr_drv decl expr stat sem util lexer translate instr; do o=$T/tr-cmini-$f.o; [ -f $o ] || ok=false; objs="$objs $o"; done
$ok && gcc $objs $T/cminirt.o $TOP/asm/libpasm.a -o $V/cmini-c/cmini/cmini.bin &&
   (cd cmini && $V/cmini-c/cmini/cmini.bin -C -I ownheaders -o $T/self.c translate.c) &&
   cmp -s $T/self.c $V/cmini-c/cmini/translate.c && echo "translate" >> $T/ok
report cmini-c "c-mini => C (gcc s cmini/cminirt.c); cmini.bin -C (sam sebou)"
finish
