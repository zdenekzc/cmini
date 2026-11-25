#!/bin/sh
# 04-cstencils.sh - _output/cstencils/: generator cstencils (.t => .hpp, .cpp) prelozeny cmini
#   (cmini -c => objekt pres pasm, glibc, gcc slinkuje s cmini/cminirt.c);
#   vystup cmini se porovna bajtove s vystupem stejneho generatoru prelozeneho g++
#
# CESTA PREKLADU (kazdy krok je v tomto souboru):
#   0. make -C cmini: cmini.bin (cmini se prelozi sam)
#   1. begin cstencils: _output/cstencils/, pracovni adresar $T, cminirt.o
#   2. jednotky: cstencils main-code, elem, fact, field, flexible, stencil, code (ostatni .cc
#      jsou fragmenty, ktere code.cc a elem.cc includuji) a utils conv, envio, inf, str, textio:
#      cmini.bin -c x.o -S x.asm => $T (objekt pres pasm)
#   3. gcc slinkuje objekty => $T/code-cmini
#   4. referencni build: g++ z cstencils => $T/code-gpp
#   5. kazdy cstencils/example/*.t: code-cmini --output-dir _output/cstencils => <base>.hpp, <base>.cpp
#      a totez code-gpp do $T/ref; soubory se musi shodovat bajtove
#
# VYSTUP: _output/cstencils/ (<base>.hpp, <base>.cpp pro kazdy example/*.t)

. "$(dirname "$0")/common.sh"
make -s -C "$TOP/cmini" || exit 1
begin cstencils
need g++ "referencni build (g++ -std=c++17) nebude, vystup nelze porovnat" || FAILED=1
need gcc "linkovani cmini objektu (gcc -no-pie) nebude" || FAILED=1
need cmp "porovnani vystupu" || FAILED=1

CST=$TOP/cstencils
CMINI=$TOP/cmini/cmini.bin
UNITS="main-code elem fact field flexible stencil code utils/conv utils/envio utils/inf utils/str utils/textio"
INC="-I $CST -I $CST/utils"

# 2. cmini: objekt (pasm) a text pro assembler
objs=""; ok_cmini=true
for u in $UNITS
do
   b=$(echo $u | tr / _)
   if (cd $CST && $CMINI $INC -I $TOP/cmini/ownheaders -c $T/cm-$b.o -S $T/cm-$b.asm $u.cc > $T/cm-$b.log 2>&1)
   then objs="$objs $T/cm-$b.o"
   else ok_cmini=false; echo "  cmini: $u.cc selhal: $(grep -m1 -i error $T/cm-$b.log | cut -c1-120)"
   fi
done

# 3. slinkovani (cmini objekty jsou ELF64 pro glibc)
$ok_cmini && gcc -no-pie $objs $T/cminirt.o -o $T/code-cmini > $T/link-cmini.log 2>&1

# 4. referencni build g++
gpp_objs=""
for u in $UNITS
do
   b=$(echo $u | tr / _)
   g++ -std=c++17 -w $INC -c $CST/$u.cc -o $T/gpp-$b.o && gpp_objs="$gpp_objs $T/gpp-$b.o"
done
g++ $gpp_objs -o $T/code-gpp > $T/link-gpp.log 2>&1

# 5. porovnani vystupu na vsech prikladech .t
[ -x $T/code-cmini ] || { echo "  cmini: program nebyl slinkovan (viz $T/link-cmini.log)"; FAILED=1; }
[ -x $T/code-gpp ] || { echo "  g++: program nebyl slinkovan (viz $T/link-gpp.log)"; FAILED=1; }
mkdir -p $T/ref
for t in $CST/example/*.t
do
   n=$(basename $t .t)
   cp $t $V/cstencils/                              # zdroj prikladu vedle vystupu
   if [ -x $T/code-cmini ] && $T/code-cmini --output-dir $V/cstencils $t > $T/$n-cmini.log 2>&1 &&
      [ -s $V/cstencils/$n.hpp ] && [ -s $V/cstencils/$n.cpp ] &&
      $T/code-gpp --output-dir $T/ref $t > $T/$n-gpp.log 2>&1 &&
      cmp -s $V/cstencils/$n.hpp $T/ref/$n.hpp && cmp -s $V/cstencils/$n.cpp $T/ref/$n.cpp
   then echo "$n" >> $T/ok
   else echo "$n" >> $T/bad
   fi
done

report cstencils "cstencils: cmini (-c, pasm) => .hpp, .cpp shodne s g++ build"
finish
