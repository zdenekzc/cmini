#!/bin/sh
# 05-cmini-translate.sh - _output/cmini-translate/: c-mini => C (cmini-translate.bin -C, translate.c) => gcc => binarky
#
# CESTA PREKLADU (kazdy krok je v tomto souboru, pod nim je jeho vystup):
#   0. begin: _output/cmini-translate se smaze, pracovni adresar $T=_build/vystup/cmini-translate; gcc
#      prelozi cminirt.c (behova knihovna C) do $T/cminirt.o; vzorove vystupy se zkopiruji.
#      cmini-translate.bin se prelozi (make -C cmini cmini-translate.bin - neni v make all).
#   1. examples/ (tests/examples/<p>.cc): cmini-translate.bin -C -I ownheaders -o examples/<p>.c
#      (translate.c: C-mini => C; na stdout, kdyz neni -o) => gcc CMINI_CFLAGS + cminirt.o
#      => <p>.bin; run porovna vystup s tests/examples/expected/<p>.txt.
#   2. zdroje cmini/asm po souborech (SOURCES v common.sh): cmini_translate = cmini-translate.bin -C
#      => <zdroj>.c (ponecha se v _output/cmini-translate/<dir>/) => gcc -c => objekt v $T.
#   3. cmini-translate.bin z C: objekty translate_drv, front, decl, expr, stat, sem, util,
#      lexer, translate => cmini-translate.bin; ten preklada translate.c znovu do C a
#      vysledek se shoduje s cmini/translate.c z gcc (sam sebe = pevny bod).
#
# VYSTUP: _output/cmini-translate/{examples,cmini,asm}/ (.c, .o, .bin, .out)
# POZOR: jeden NN-*.sh se nesmi spustit dvakrat soucasne (sdileji _output a _build/vystup).

. "$(dirname "$0")/common.sh"
begin cmini-translate

# the translator is not part of "make all": build it only for this test
make -s -C "$TOP/cmini" cmini-translate.bin || exit 1
# every <...> must have a replacement in ownheaders (nothing silently ignored)
CMINI_MISSING_HEADERS=error
export CMINI_MISSING_HEADERS

D=$V/cmini-translate/examples; mkdir -p $D
for p in $PROGRAMS
do
   ./cmini/cmini-translate.bin -C -I cmini/ownheaders -o $D/$p.c $EX/$p.cc > /dev/null 2>&1 &&
      gcc $CMINI_CFLAGS $D/$p.c $T/cminirt.o -o $D/$p.bin -lm 2> /dev/null
   run $D $p
done
# cout / cerr (C++ iostream => printf, dprintf; viz tests/02 a kap. 86 v CHANGELOG.txt)
for p in cout
do
   ./cmini/cmini-translate.bin -C -I cmini/ownheaders -o $D/$p.c $EX/$p.cc > /dev/null 2>&1 &&
      gcc $CMINI_CFLAGS $D/$p.c $T/cminirt.o -o $D/$p.bin -lm 2> /dev/null
   run $D $p
done
cmini_translate ()   # zdroj => C, ponecha se, kdyz ho prelozi gcc
{
   d=$(dirname $1); (cd $d && $TOP/cmini/cmini-translate.bin -C -I $TOP/cmini/ownheaders -o $2.c $(basename $1)) &&
      gcc $CMINI_CFLAGS -c $2.c -o $2.o && mv $2.o $T/tr-$(basename $(dirname $2))-$(basename $2).o
}
sources cmini-translate cmini_translate
ok=true; objs=""
for f in translate_drv front decl expr stat sem util lexer translate; do o=$T/tr-cmini-$f.o; [ -f $o ] || ok=false; objs="$objs $o"; done
$ok && gcc $objs -o $V/cmini-translate/cmini/cmini-translate.bin &&
   (cd cmini && $V/cmini-translate/cmini/cmini-translate.bin -C -I ownheaders -o $T/self.c translate.c) &&
   cmp -s $T/self.c $V/cmini-translate/cmini/translate.c && echo "translate" >> $T/ok
report cmini-translate "c-mini => C (gcc s cminirt/cminirt.c); cmini-translate.bin -C (sam sebou)"
finish
