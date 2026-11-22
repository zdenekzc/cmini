# common.sh - spolecna cast skriptu tests/NN-*.sh: vznikle soubory pro studenty
# v adresari _output/<generator>/ (vola tests/output.sh, make vystup)
#
#   . "$(dirname "$0")/common.sh"
#   begin <generator>         smaze _output/<generator>, pracovni adresar $T
#   ...                       preklad, run, check, sources
#   report <generator> <popis>
#   finish                    smaze pracovni adresar
#
# Kazdy skript lze spustit i samostatne (prekladace musi byt prelozene - make).
#
#   _output/<generator>/examples/   testovaci programy (tests/examples/*.cc):
#                                   vygenerovany text (.asm, .c, ...), spustitelny
#                                   soubor (.bin) a jeho vystup (.out),
#                                   porovnani se vzorem (tests/examples/expected,
#                                   quicksort a heapsort - nahodna data - jen posledni
#                                   radek O.K.)
#   _output/<generator>/cmini/      zdrojove texty prekladace, ktere generator zvladne
#   _output/<generator>/asm/        (podadresar vznikne, jen kdyz se aspon neco povede;
#                                   soubor se ponecha, jen kdyz ho prelozi i gcc)

TOP=$(cd "$(dirname "$0")/.." && pwd)
cd "$TOP" || exit 1
V=$TOP/_output
EX=tests/examples

PROGRAMS="dama jezdec avl btree veze classes strings defaults chars dtors floats objcopy pointers sequence dictionary dyncast quicksort heapsort initsize"
# zdroje prekladace (cmini a pasm); cmini.bin preklada sam sebe po souborech
SOURCES="$(ls cmini/*.cc cmini/*.c) $(ls asm/*.c 2> /dev/null)"
CMINI_CFLAGS="-std=gnu11 -w -I $TOP/cmini"

begin ()   # begin <generator> - novy _output/<generator>, pracovni adresar $T, vzory, behova knihovna
{
   T=$TOP/_build/vystup/$1
   rm -rf "$V/$1" "$T"
   mkdir -p "$V/$1" "$T" || exit 1
   TMPDIR=$T; export TMPDIR
   gcc -std=gnu11 -w -c cmini/cminirt.c -o $T/cminirt.o || exit 1
   # vzorovy vystup: tests/examples/expected/p.txt;
   # quicksort, heapsort (nahodna data, srand (time (NULL))) - posledni radek O.K.
   mkdir -p $T/exp
   for p in $PROGRAMS; do [ -f $EX/expected/$p.txt ] && cp $EX/expected/$p.txt $T/exp/; done
   cp $EX/expected/cout.txt $T/exp/ 2> /dev/null      # cout (tests/02, 03 zvlast, kap. 86 CHANGELOG.txt)
}

finish ()
{
   rm -rf "$T"
   rmdir "$TOP/_build/vystup" 2> /dev/null
   exit ${FAILED:-0}   # 1 => vystup se lisi od vzoru nebo FAIL (output.sh, make test-all)
}

check ()   # check <vystup> <program> - souhlasi se vzorem?
{
   if [ -f $T/exp/$2.txt ]
   then cmp -s "$1" $T/exp/$2.txt
   else [ "$(tail -n 1 "$1")" = "O.K." ]
   fi
   if [ $? = 0 ]; then echo "$2" >> $T/ok; else echo "$2" >> $T/bad; fi
}

run ()   # run <adresar> <program> - spusti <adresar>/<program>.bin => .out, porovna se vzorem
{
   [ -x "$1/$2.bin" ] || return
   (cd "$1" && timeout 10 ./$2.bin > $2.out 2>&1)
   check "$1/$2.out" $2
}

report ()   # report <adresar> <popis>
{
   d=$V/$1
   [ -d $d ] || return
   n=$(find $d -type f | wc -l)
   b=$(find $d -name '*.bin' | wc -l)
   ok=$(cat $T/ok 2>/dev/null | wc -l); bad=$(cat $T/bad 2>/dev/null | sort | tr '\n' ' ')
   subs=$(cd $d && ls -d */ 2>/dev/null | tr -d / | tr '\n' ' ')
   printf "  %-17s %3d souboru, %2d spustitelnych, vystup = vzor: %2d  [%s]  %s\n" "$1/" $n $b $ok "$subs" "$2"
   [ -n "$bad" ] && { printf "  %-17s     vystup se lisi od vzoru: %s\n" "" "$bad"; FAILED=1; }
   rm -f $T/ok $T/bad
}

sources ()   # sources <generator> <prikaz> - zdroje prekladace: <prikaz> <zdroj> <vystup bez pripony>
{            # vraci 0 => soubor se ponecha
   for f in $SOURCES
   do
      dir=$(dirname $f); base=$(basename $f); stem=${base%.*}
      [ "$base" = "$stem.c" ] && [ -f $dir/$stem.cc ] && continue   # x.c vedle x.cc - jen jeden
      mkdir -p $V/$1/$dir
      $2 $f $V/$1/$dir/$stem > /dev/null 2>&1 || rm -f $V/$1/$dir/$stem.*
   done
   for dir in cmini asm
   do
      [ -d $V/$1/$dir ] && find $V/$1/$dir -type f ! -name '*.c' ! -name '*.asm' ! -name '*.s' ! -name '*.bin' -delete
      rmdir $V/$1/$dir 2> /dev/null
   done
}
