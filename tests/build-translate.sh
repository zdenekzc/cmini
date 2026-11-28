#!/bin/sh
# translate.c - preklad cmini => C a jeho kontroly (bez Qt, bez asmjit)
#
#   tests/build-translate.sh          cmini-translate.bin -C, testy tests/examples/*.cc, cmini cely v C99
#
#   1. tests/examples/*.cc: cmini-translate.bin -C => C => gcc -Wall, vystup programu proti g++
#   2. prekladac cely v C99 (instr_drv.c, front.c, front-end, generator instr.c, cminirt.c -
#      gcc -std=c99 -pedantic, jako TinyCC) musi dat stejny text pro assembler
#      a stejny vystup programu jako cmini/cmini.bin (gnu11) (tests/examples/*.cc)
#      a prelozit instr.c do stejneho objektu instr.o
#   3. translate.c + front-end => C a zpet (pevny bod translate-c.bin)
#   4. c-mini sam sebou (test_instr_lexer.bin -0): cmini-asm.bin (instr_drv.c, front.c,
#      front-end, instr.c) a translate-asm.bin (translate_drv.c, front.c, translate.c) -
#      stejne vystupy jako tytez programy z gcc;
#      assembler pasm (asm/*.c) prelozeny c-mini - testy asm/tests/run.sh

cd "$(dirname "$0")/.." || exit 1
# docasne soubory v _build/tmp (ne v /tmp)
mkdir -p _build/tmp || exit 1
TMPDIR=$(pwd)/_build/tmp
export TMPDIR
SRC=$PWD
TMP=$(mktemp -d) || exit 1
trap 'rm -rf "$TMP"' EXIT
status=0

PROGRAMS="dama jezdec avl btree veze classes strings defaults chars dtors floats objcopy pointers sequence dictionary dyncast"

# compilers (pure C - as in cmini/Makefile); the translator is not in "make all"
make -s -C cmini cmini.bin cmini-translate.bin || exit 1
gcc -std=gnu11 -Wall -c cminirt/cminirt.c -o "$TMP/cminirt.o" || exit 1

echo "=== tests/examples/*.cc => C"
for p in $PROGRAMS
do
    g++ -x c++ -w tests/examples/$p.cc -o "$TMP/$p.gpp" -lm && "$TMP/$p.gpp" > "$TMP/$p.exp" 2>&1
    if ./cmini/cmini-translate.bin -C -I cmini/ownheaders -o "$TMP/$p.c" tests/examples/$p.cc 2> "$TMP/$p.err" &&
       gcc -std=gnu11 -Wall -Werror -Wno-unused -I cmini -I cminirt "$TMP/$p.c" "$TMP/cminirt.o" -o "$TMP/$p" -lm 2>> "$TMP/$p.err" &&
       "$TMP/$p" 2>&1 | cmp -s - "$TMP/$p.exp"
    then echo "  OK    tests/examples/$p.cc"
    else echo "  FAIL  tests/examples/$p.cc"; head -3 "$TMP/$p.err"; status=1
    fi
done

# assembler pasm jako knihovna (asm/pasmlib.h)
make -s -C asm libpasm.a || exit 1

# referencni prekladac: cmini/cmini.bin (gcc -std=gnu11, cmini/Makefile)
echo "=== cmini cely v C99 (instr_drv.c, front.c, front-end, instr.c, cminirt.c)"
D="-DUSE_LEXER"
cp cmini/cmini.bin "$TMP/ref.bin" || exit 1
# prekladac cely v C (jako cmini/cmini.bin, TinyCC): C99 bez rozsireni GNU
for f in decl expr stat sem util lexer instr_drv front
do
    gcc -std=c99 -pedantic -w $D -I cmini -c cmini/$f.c -o "$TMP/c_$f.o" || { echo "  FAIL  $f.c (gcc -std=c99)"; exit 1; }
done
gcc -std=c99 -pedantic -w $D -I cmini -c cminirt/cminirt.c -o "$TMP/c_cminirt.o" ||
    { echo "  FAIL  cminirt/cminirt.c (gcc -std=c99)"; exit 1; }
gcc -std=c99 -pedantic -Wall -Werror -Wno-unused $D -I cmini -c cmini/instr.c -o "$TMP/c_instr.o" ||
    { echo "  FAIL  cmini/instr.c (gcc -std=c99 -pedantic -Wall)"; exit 1; }
gcc "$TMP/c_instr_drv.o" "$TMP/c_front.o" "$TMP/c_decl.o" "$TMP/c_expr.o" "$TMP/c_stat.o" "$TMP/c_sem.o" "$TMP/c_util.o" "$TMP/c_lexer.o" \
    "$TMP/c_instr.o" "$TMP/c_cminirt.o" asm/libpasm.a -o "$TMP/new.bin" || exit 1

for p in $PROGRAMS
do
    "$TMP/ref.bin" -I cmini/ownheaders -o "$TMP/r_$p" tests/examples/$p.cc > /dev/null 2>&1
    "$TMP/new.bin" -I cmini/ownheaders -o "$TMP/n_$p" tests/examples/$p.cc > /dev/null 2>&1
    "$TMP/ref.bin" -I cmini/ownheaders tests/examples/$p.cc > "$TMP/r_$p.out" 2>&1
    "$TMP/new.bin" -I cmini/ownheaders tests/examples/$p.cc > "$TMP/n_$p.out" 2>&1
    if [ -s "$TMP/r_$p.asm" ] && cmp -s "$TMP/r_$p.asm" "$TMP/n_$p.asm" && cmp -s "$TMP/r_$p.out" "$TMP/n_$p.out"
    then echo "  OK    tests/examples/$p.cc (C99: asm a vystup == cmini.bin)"
    else echo "  FAIL  tests/examples/$p.cc (instr.c)"; status=1
    fi
done

"$TMP/ref.bin" -I cmini/ownheaders -I cmini -c "$TMP/r.o" cmini/instr.c > /dev/null 2>&1
"$TMP/new.bin" -I cmini/ownheaders -I cmini -c "$TMP/n.o" cmini/instr.c > /dev/null 2>&1
if [ -s "$TMP/r.o" ] && cmp -s "$TMP/r.o" "$TMP/n.o"
then echo "  OK    instr.c => instr.o (C99 == cmini.bin)"
else echo "  FAIL  instr.c => instr.o"; status=1
fi

# front-end a translate.c => C:  translate-c.bin (gcc) musi dat stejne vystupy
# jako cmini-translate.bin -C (gcc ze zdroju);  pevny bod - prelozi sve zdrojove texty na tentyz C
echo "=== translate.c + front-end => C (self)"
SELF="decl expr stat sem util lexer translate"
mkdir -p "$TMP/self"
ok=true
for f in $SELF
do
    ./cmini/cmini-translate.bin -C -I cmini/ownheaders -o "$TMP/self/$f.c" cmini/$f.c &&
    gcc -std=gnu11 -Wall -Wno-unused -Wno-switch -Wno-dangling-else -Werror -I cmini -I cminirt -c "$TMP/self/$f.c" \
        -o "$TMP/self/$f.o" || { echo "  FAIL  $f.c => C"; ok=false; }
done
# the translator driver (translate_drv.c) and the shared front.c, compiled by gcc
gcc -std=gnu11 -w $D -I cmini -c cmini/translate_drv.c -o "$TMP/self/translate_drv.o" || ok=false
gcc -std=gnu11 -w $D -I cmini -c cmini/front.c -o "$TMP/self/front.o" || ok=false
if $ok && gcc $(for f in $SELF; do echo "$TMP/self/$f.o"; done) "$TMP/self/translate_drv.o" "$TMP/self/front.o" \
       -o "$TMP/translate-c.bin"
then
    n=0; bad=0
    for f in tests/examples/*.cc cmini/instr.c cmini/decl.c cmini/expr.c cmini/stat.c cmini/sem.c cmini/util.c cmini/lexer.c cmini/translate.c
    do
        n=$((n+1))
        ./cmini/cmini-translate.bin -C -I cmini/ownheaders -o "$TMP/a.c" $f 2> /dev/null
        "$TMP/translate-c.bin" -C -I cmini/ownheaders -o "$TMP/b.c" $f 2> /dev/null
        cmp -s "$TMP/a.c" "$TMP/b.c" || { echo "  DIFF  $f"; bad=$((bad+1)); }
    done
    if [ $bad = 0 ]
    then echo "  OK    translate-c.bin == cmini-translate.bin -C ($n souboru, i vlastni zdroje - pevny bod)"
    else echo "  FAIL  translate-c.bin ($bad z $n souboru)"; status=1
    fi
else
    echo "  FAIL  translate-c.bin"; status=1
fi

# c-mini prelozeny sam sebou (test_instr_lexer.bin -0 => pasm => ELF):
#   cmini-asm.bin     = instr_drv.c + front.c + front-end + instr.c: stejny text pro assembler
#                       a stejne objekty jako ref.bin (gcc), i na vlastnich zdrojich
#   translate-asm.bin = translate_drv.c + front.c + front-end + translate.c: stejny C
#                       jako cmini-translate.bin -C
echo "=== c-mini sam sebou (test_instr_lexer.bin -0)"
SELFASM="front.c decl.c expr.c stat.c sem.c util.c lexer.c instr.c"
SELFTR="translate_drv.c front.c translate.c decl.c expr.c stat.c sem.c util.c lexer.c"
if [ ! -x ./cmini/test_instr_lexer.bin ]
then
    echo "  SKIP  (chybi test_instr_lexer.bin - make)"
else
    if (cd cmini && ./test_instr_lexer.bin -0 -I . -I ownheaders -o "$TMP/cmini-asm.bin" \
            instr_drv.c $SELFASM ../asm/libpasm.a) > "$TMP/cmini-asm.log" 2>&1
    then
        n=0; bad=0
        for p in $PROGRAMS
        do
            n=$((n+1))
            "$TMP/ref.bin" -I cmini/ownheaders -o "$TMP/r_$p" tests/examples/$p.cc > /dev/null 2>&1
            "$TMP/cmini-asm.bin" -I cmini/ownheaders -o "$TMP/s_$p" tests/examples/$p.cc > /dev/null 2>&1
            [ -s "$TMP/r_$p.asm" ] && cmp -s "$TMP/r_$p.asm" "$TMP/s_$p.asm" || { echo "  DIFF  tests/examples/$p.cc"; bad=$((bad+1)); }
        done
        for f in $SELFASM translate.c
        do
            n=$((n+1))
            "$TMP/ref.bin" -I cmini/ownheaders -I cmini -c "$TMP/r.o" cmini/$f > /dev/null 2>&1
            "$TMP/cmini-asm.bin" -I cmini/ownheaders -I cmini -c "$TMP/s.o" cmini/$f > /dev/null 2>&1
            [ -s "$TMP/r.o" ] && cmp -s "$TMP/r.o" "$TMP/s.o" || { echo "  DIFF  cmini/$f"; bad=$((bad+1)); }
        done
        if [ $bad = 0 ]
        then echo "  OK    cmini-asm.bin == gcc ($n souboru: text pro assembler a objekty, i vlastni zdroje)"
        else echo "  FAIL  cmini-asm.bin ($bad z $n souboru)"; status=1
        fi
    else
        echo "  FAIL  cmini-asm.bin: $(grep -m1 error "$TMP/cmini-asm.log")"; status=1
    fi

    if (cd cmini && ./test_instr_lexer.bin -0 -I ownheaders -o "$TMP/translate-asm.bin" \
            $SELFTR) > "$TMP/translate-asm.log" 2>&1
    then
        n=0; bad=0
        for f in tests/examples/*.cc cmini/instr.c cmini/decl.c cmini/expr.c cmini/stat.c cmini/sem.c cmini/util.c cmini/lexer.c cmini/translate.c
        do
            n=$((n+1))
            ./cmini/cmini-translate.bin -C -I cmini/ownheaders -o "$TMP/a.c" $f 2> /dev/null
            "$TMP/translate-asm.bin" -C -I cmini/ownheaders -o "$TMP/b.c" $f 2> /dev/null
            cmp -s "$TMP/a.c" "$TMP/b.c" || { echo "  DIFF  $f"; bad=$((bad+1)); }
        done
        if [ $bad = 0 ]
        then echo "  OK    translate-asm.bin == cmini-translate.bin -C ($n souboru, i vlastni zdroje)"
        else echo "  FAIL  translate-asm.bin ($bad z $n souboru)"; status=1
        fi
    else
        echo "  FAIL  translate-asm.bin: $(grep -m1 error "$TMP/translate-asm.log")"; status=1
    fi
fi

# assembler pasm cely prelozeny c-mini (struktury hodnotou: ExprRec, ResultRec, AsmOp)
# - stejne vysledky testu asm/tests/run.sh jako pasm z gcc
if [ -x ./cmini/test_instr_lexer.bin ]
then
    if (cd asm && ../cmini/test_instr_lexer.bin -0 -I ../cmini/ownheaders -o "$TMP/pasm-asm.bin" \
            pasm.c pasmlib.c code.c def.c expr.c inst.c key.c oth.c asmlist.c) > "$TMP/pasm-asm.log" 2>&1 &&
       ASM_OUTPUT="$TMP/pasm-tests" sh asm/tests/run.sh "$TMP/pasm-asm.bin" > "$TMP/pasm-tests.log" 2>&1
    then echo "  OK    pasm-asm.bin (asm/*.c prelozene c-mini) - asm/tests/run.sh: $(grep -c '^ok' "$TMP/pasm-tests.log") ok"
    else echo "  FAIL  pasm-asm.bin: $(grep -m1 -i 'error\|FAIL' "$TMP/pasm-asm.log" "$TMP/pasm-tests.log" 2> /dev/null)"; status=1
    fi
fi

exit $status
