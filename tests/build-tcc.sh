#!/bin/sh
# bootstrap prekladace c-mini bez gcc: TinyCC (a chibicc)
#
#   tests/build-tcc.sh                TCC=/opt/tinycc/bin/tcc, CHIBICC=chibicc (kdyz jsou)
#   TCC=... CHIBICC=... tests/build-tcc.sh
#
#   1. TinyCC prelozi cmini.bin (instr_drv.c, translate.c, front-end, instr.c, cminirt.c) a assembler
#      pasm jako knihovnu (asm/*.c) - vse ciste C bez .eh_frame
#   2. cmini.bin z TinyCC prelozi tests/examples/*.cc: stejny text pro assembler jako
#      cmini/cmini.bin z gcc, vystup programu proti vzoru
#   3. samopreklad: cmini.bin z TinyCC prelozi sve zdroje (instr_drv.c, front-end,
#      instr.c) po souborech (-c) => objekty slinkuje tcc => stupen 2; stupen 2 totez
#      => stupen 3; text pro assembler stupnu 2 a 3 shodny (pevny bod) a shodny s cmini
#      z gcc; stupen 3 prelozi dama.c
#   4. chibicc (je-li): cmini.bin (linkuje gcc), body 2 a 3 jen proti cmini z gcc
#      (chibicc hleda ./include - spousti se z pracovniho adresare s absolutnimi cestami)

cd "$(dirname "$0")/.." || exit 1
TOP=$PWD
TCC=${TCC:-/opt/tinycc/bin/tcc}
CHIBICC=${CHIBICC:-$(command -v chibicc)}
mkdir -p _build/tmp || exit 1
TMP=$(mktemp -d "$TOP/_build/tmp/tcc.XXXXXX") || exit 1
trap 'rm -rf "$TMP"' EXIT
status=0

if [ ! -x "$TCC" ]
then
   echo "  SKIP  TinyCC nenalezen ($TCC)"
   exit 0
fi
make -s -C cmini cmini.bin > /dev/null || exit 1   # referencni cmini.bin z gcc

CMINI_SRC="cmini/instr_drv.c cmini/translate.c cmini/decl.c cmini/expr.c cmini/stat.c cmini/sem.c cmini/util.c cmini/lexer.c cmini/instr.c cminirt/cminirt.c"
PASM_SRC="pasmlib code def expr inst key oth asmlist"
SELF_SRC="instr_drv.c translate.c decl.c expr.c stat.c sem.c util.c lexer.c instr.c"

result ()   # $1 = popis, $2 = 0 / 1
{
   if [ "$2" = 0 ]; then echo "  OK    $1"; else echo "  FAIL  $1"; status=1; fi
}

# priklady: $1 = prekladac cmini, $2 = adresar
examples ()
{
   mkdir -p $2; ok=0; bad=""
   for f in tests/examples/*.cc
   do
      p=$(basename $f .c)
      [ -f tests/examples/expected/$p.txt ] || continue
      if $1 -I cmini/ownheaders -o $2/$p $f > $2/$p.log 2>&1 &&
         ./cmini/cmini.bin -I cmini/ownheaders -o $2/ref-$p $f > /dev/null 2>&1 &&
         cmp -s $2/$p.asm $2/ref-$p.asm &&
         (cd $2 && ./$p) 2>&1 | cmp -s - tests/examples/expected/$p.txt
      then ok=$((ok+1))
      else bad="$bad $p"
      fi
   done
   [ -z "$bad" ] || echo "        chyby:$bad"
   echo "$ok"
   [ -z "$bad" ]
}

# zdroje cmini po souborech: $1 = prekladac, $2 = adresar (objekty $2/*.o, text $2/*.asm)
self_objects ()
{
   mkdir -p $2
   for f in $SELF_SRC
   do
      (cd cmini && $1 -I . -I ownheaders -o $2/${f%.*} -c $2/${f%.*}.o $f > $2/${f%.*}.log 2>&1) ||
         { echo "        $f: $(tail -1 $2/${f%.*}.log)"; return 1; }
   done
}

same_asm ()   # $1, $2 = adresare s *.asm
{
   for f in $SELF_SRC
   do
      cmp -s $1/${f%.*}.asm $2/${f%.*}.asm || { echo "        lisi se ${f%.*}.asm"; return 1; }
   done
}

echo "=== TinyCC: $($TCC -v 2>&1 | head -1)"
for f in $PASM_SRC
do
   $TCC -w -I asm -c asm/$f.c -o $TMP/asm_$f.o || status=1
done
$TCC -ar rcs $TMP/libpasm.a $TMP/asm_*.o
$TCC -w -DUSE_LEXER -I cmini -I cminirt -I asm $CMINI_SRC $TMP/libpasm.a -o $TMP/cmini-tcc.bin
result "tcc: cmini.bin a libpasm.a (asm/*.c)" $?
[ -x $TMP/cmini-tcc.bin ] || exit 1

n=$(examples $TMP/cmini-tcc.bin $TMP/ex1); r=$?
result "cmini z tcc: tests/examples ($n programu, .asm jako cmini z gcc, vystup = vzor)" $r

self_objects $TMP/cmini-tcc.bin $TMP/s2 && $TCC -o $TMP/s2/cmini.bin $TMP/s2/*.o $TMP/libpasm.a -lm
result "stupen 2: cmini z tcc prelozi sve zdroje (-c), slinkuje tcc" $?
self_objects $TMP/s2/cmini.bin $TMP/s3 && $TCC -o $TMP/s3/cmini.bin $TMP/s3/*.o $TMP/libpasm.a -lm
result "stupen 3: stupen 2 prelozi sve zdroje" $?
same_asm $TMP/s2 $TMP/s3
result "pevny bod: text pro assembler stupnu 2 a 3 shodny" $?
self_objects $TOP/cmini/cmini.bin $TMP/gcc > /dev/null && same_asm $TMP/s3 $TMP/gcc
result "stupen 3 = cmini z gcc (text pro assembler)" $?
$TMP/s3/cmini.bin -I cmini/ownheaders -o $TMP/dama tests/examples/dama.cc > /dev/null 2>&1 &&
   (cd $TMP && ./dama) | cmp -s - tests/examples/expected/dama.txt
result "stupen 3: dama.c" $?

if [ -n "$CHIBICC" ] && [ -x "$CHIBICC" ]
then
   echo "=== chibicc: $CHIBICC"
   mkdir -p $TMP/chibi
   ok=0
   for f in $CMINI_SRC $(for a in $PASM_SRC; do echo asm/$a.c; done)
   do
      (cd $TMP/chibi && $CHIBICC -I/usr/include/chibicc -DUSE_LEXER -I$TOP/cmini -I$TOP/cminirt -I$TOP/asm \
         -c $TOP/$f -o $TMP/chibi/$(echo $f | tr / _).o > $TMP/chibi/err.log 2>&1) ||
         { echo "        $f: $(head -1 $TMP/chibi/err.log)"; ok=1; }
   done
   [ $ok = 0 ] && gcc $TMP/chibi/*.o -o $TMP/cmini-chibi.bin -lm
   result "chibicc: cmini.bin (linkuje gcc)" $?
   if [ -x $TMP/cmini-chibi.bin ]
   then
      n=$(examples $TMP/cmini-chibi.bin $TMP/ex2); r=$?
      result "cmini z chibicc: tests/examples ($n programu)" $r
      self_objects $TMP/cmini-chibi.bin $TMP/cs && same_asm $TMP/cs $TMP/gcc
      result "cmini z chibicc: vlastni zdroje = cmini z gcc (text pro assembler)" $?
   fi
else
   echo "  SKIP  chibicc nenalezen"
fi
exit $status
