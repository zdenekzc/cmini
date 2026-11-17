#!/bin/sh
# Preklad a spusteni testu generatoru kodu (bez Qt)
#
#   tests/build-test.sh [-v]                  bez asmjit: jen generator instr.c (-0) s assemblerem
#                                         pasm jako knihovnou
#   ASMJIT=local tests/build-test.sh [-v]     i instr1.cc ... instr5.cc, lokalni asmjit z _library/asmjit
#   ASMJIT=system tests/build-test.sh [-v]    totez, asmjit nainstalovany v systemu (/usr/include)
#   LEXER=lexer tests/build-test.sh [-v]      lexer.c (s preprocesorem) misto lex.c
#   generator instr.c (-0) vola assembler jako knihovnu asm/libpasm.a
#   (asm/asmlist.h, asm/pasmlib.h) - prelozi se zde (make -C asm)
#   tests/build-test.sh -e                    znovu vygenerovat tests/examples/expected/*.txt
#                                         (preklad tests/examples/*.cc pomoci g++ a ulozeni vystupu)

cd "$(dirname "$0")/.." || exit 1
# docasne soubory v _build/tmp (ne v /tmp)
mkdir -p _build/tmp || exit 1
TMPDIR=$(pwd)/_build/tmp
export TMPDIR

PROGRAMS="dama jezdec avl btree veze preproc classes strings defaults chars dtors floats objcopy pointers sequence dictionary dyncast"

if [ "$1" = "-e" ]
then
    mkdir -p tests/examples/expected || exit 1
    TMP=$(mktemp -d) || exit 1
    trap 'rm -rf "$TMP"' EXIT
    for p in $PROGRAMS
    do
        g++ -x c++ -w tests/examples/$p.cc -o "$TMP/$p" -lm || exit 1
        "$TMP/$p" > tests/examples/expected/$p.txt || { echo "  FAIL  tests/examples/$p.cc (g++)"; exit 1; }
        echo "  GEN   tests/examples/expected/$p.txt"
    done
    # program z vice souboru (tests/examples/multi): main.c, shapes.c (C++), util.c (C)
    g++ -w -iquote cmini/ownheaders -x c++ tests/examples/multi/main.c tests/examples/multi/shapes.c -x c tests/examples/multi/util.c \
        -o "$TMP/multi" || exit 1
    "$TMP/multi" > tests/examples/expected/multi.txt || { echo "  FAIL  tests/examples/multi (g++)"; exit 1; }
    echo "  GEN   tests/examples/expected/multi.txt"
    exit 0
fi

if [ "$ASMJIT" = "system" ]
then
    ASMJIT_FLAGS="-lasmjit"
    GENS="-0 -1 -2 -3 -4 -5"
elif [ "$ASMJIT" = "local" ]
then
    LIB=_library/asmjit
    ASMJIT_FLAGS="-I $LIB -L $LIB -lasmjit -Wl,-rpath,$PWD/$LIB"
    GENS="-0 -1 -2 -3 -4 -5"
else
    ASMJIT_FLAGS="-DNO_ASMJIT"
    GENS="-0"
fi
if [ "$GENS" = "-0" ]
then
    INSTR_SRC=""
else
    INSTR_SRC="cmini/instr1.cc cmini/instr2.cc cmini/instr3.cc cmini/instr4.cc cmini/instr5.cc"
fi
echo "=== generatory: $GENS"

if [ "$LEXER" = "lexer" ]
then
    LEX_NAME="cmini/lexer.c"; LEX_DEF="-DUSE_LEXER"
else
    LEX_NAME="cmini/lex.c"; LEX_DEF=""
fi
echo "=== lexer: ${LEX_NAME#cmini/}"

make -s -C asm all || exit 1

# front-end jako ciste C bez .eh_frame (jako TinyCC): showError (test_instr.cc) nesmi
# prochazet ramci C jako vyjimka C++ - jinak by chybove testy skoncily std::terminate
OTMP=$(mktemp -d)
for f in cmini/inp.c cmini/out.c $LEX_NAME cmini/expr.c cmini/stat.c cmini/sem.c cmini/util.c cmini/decl.c cmini/instr.c
do
    gcc -g -std=gnu11 -w -fno-asynchronous-unwind-tables $LEX_DEF -c $f -o "$OTMP/$(basename $f .c).o" || exit 1
done
g++ -g -std=c++20 -fpermissive -w $LEX_DEF \
    cmini/test_instr.cc "$OTMP"/*.o $INSTR_SRC \
    asm/libpasm.a $ASMJIT_FLAGS \
    -o cmini/test_instr.bin || exit 1
rm -rf "$OTMP"
./cmini/test_instr.bin "$@" || exit 1
[ $# -gt 0 ] && exit 0   # s parametry jen testy z test_instr.cc

# programy v tests/examples/ s deterministickym vystupem: porovnani s vystupem g++ (tests/examples/expected)
#   dama, jezdec - vsechny generatory; avl, btree, veze (struktury) - instr5.cc a instr.c
#   classes (tridy, dedicnost, virtualni metody), strings (std::string),
#   defaults (prototypy, implicitni hodnoty parametru, vyctove konstanty),
#   chars (pole znaku, char *, aritmetika ukazatelu, metody std::string),
#   dtors (destruktory), floats (float, double - SSE2; jen s glibc),
#   pointers (ukazatele, ukazatele na funkce, qsort - jen s glibc),
#   sequence, dictionary (vestavene typy Sequence <T>, Dictionary <K, V>),
#   dyncast (dynamic_cast) - instr.c
#   instr.c: vychozi je glibc (pasm -c, gcc), CPARSER_RTL=own - vlastni behova knihovna
#   preproc (preprocesor) - vsechny generatory, jen s LEXER=lexer
status=0
compare ()   # compare <program> <prepinac generatoru>
{
    if ./cmini/test_instr.bin $2 tests/examples/$1.cc 2>&1 | grep -v '^main returned' | cmp -s - tests/examples/expected/$1.txt
    then echo "  OK    tests/examples/$1.cc $2"
    else echo "  FAIL  tests/examples/$1.cc $2"; status=1
    fi
}
echo "=== tests/examples/*.cc"
for g in $GENS; do compare dama "$g"; compare jezdec "$g"; done
for p in avl btree veze; do [ "$GENS" != "-0" ] && compare $p -5; compare $p -0; done
compare classes -0
compare strings -0
compare defaults -0
compare chars -0
compare dtors -0
compare floats -0
compare pointers -0
compare sequence -0
compare dictionary -0
compare dyncast -0

# instr.c sklada program jako seznam instrukci (asm/asmlist.h): pasm cte uzly
# primo (faze 2) - program a vypis musi byt shodne s prekladem pres text
# (CPARSER_ASMLIST=text, faze 1), i s vlastni behovou knihovnou
echo "=== instr.c: uzly (faze 2) == text (faze 1)"
ATMP=$(mktemp -d) || exit 1
for p in dama jezdec avl btree veze classes strings defaults chars dtors floats pointers sequence dictionary dyncast
do
    for rtl in glibc own
    do
        CPARSER_RTL=$rtl ./cmini/test_instr.bin -0 -o "$ATMP/b" tests/examples/$p.cc > /dev/null 2>&1
        CPARSER_RTL=$rtl CPARSER_ASMLIST=text ./cmini/test_instr.bin -0 -o "$ATMP/t" tests/examples/$p.cc > /dev/null 2>&1
        # bez glibc nektere programy nelze (chyba pred assemblerem, zadne soubory)
        if [ -f "$ATMP/b" ] || [ -f "$ATMP/t" ]
        then
            if cmp -s "$ATMP/b" "$ATMP/t" && cmp -s "$ATMP/b.lst" "$ATMP/t.lst" && cmp -s "$ATMP/b.asm" "$ATMP/t.asm"
            then echo "  OK    tests/examples/$p.cc ($rtl)"
            else echo "  FAIL  tests/examples/$p.cc ($rtl: uzly != text)"; status=1
            fi
        fi
        rm -f "$ATMP"/*
    done
done
rm -rf "$ATMP"

# instr.c s vlastni behovou knihovnou (bez glibc, CPARSER_RTL=own)
echo "=== instr.c, CPARSER_RTL=own"
CPARSER_RTL=own ./cmini/test_instr.bin -0 | grep -E 'FAIL|passed' || status=1
CPARSER_RTL=own ./cmini/test_instr.bin -0 | grep -q ' 0 failed' || status=1
for p in dama jezdec avl classes strings defaults chars dtors; do CPARSER_RTL=own compare $p -0; done
if [ "$LEXER" = "lexer" ]
then
    for g in $GENS; do compare preproc "$g"; done

    # program z vice souboru (tests/examples/multi): main.c, shapes.c (cparser), util.c (gcc)
    MTMP=$(mktemp -d) || exit 1
    gcc -c tests/examples/multi/util.c -o "$MTMP/util.o" || status=1
    if ./cmini/test_instr.bin -0 -I cmini/ownheaders tests/examples/multi/main.c tests/examples/multi/shapes.c "$MTMP/util.o" 2>&1 |
       grep -v '^main returned' | cmp -s - tests/examples/expected/multi.txt
    then echo "  OK    tests/examples/multi (zdrojove soubory + util.o)"
    else echo "  FAIL  tests/examples/multi (zdrojove soubory + util.o)"; status=1
    fi
    # -c: objekty zvlast, potom slinkovani objektu
    if (cd "$MTMP" && "$OLDPWD/cmini/test_instr.bin" -0 -c -I "$OLDPWD/cmini/ownheaders" "$OLDPWD/tests/examples/multi/main.c" "$OLDPWD/tests/examples/multi/shapes.c" &&
        "$OLDPWD/cmini/test_instr.bin" -0 -o prog main.o shapes.o util.o && ./prog) 2>&1 |
       cmp -s - tests/examples/expected/multi.txt
    then echo "  OK    tests/examples/multi (-c, objekty .o)"
    else echo "  FAIL  tests/examples/multi (-c, objekty .o)"; status=1
    fi
    rm -rf "$MTMP"

    # instr.c (generator v C v podmnozine c-mini) musi jit prelozit cparserem do objektu
    STMP=$(mktemp -d) || exit 1
    if ./cmini/test_instr.bin -0 -c -I cmini -I cmini/ownheaders -o "$STMP/instr.o" cmini/instr.c > "$STMP/err" 2>&1 && [ -s "$STMP/instr.o" ]
    then echo "  OK    instr.c => instr.o (cparser)"
    else echo "  FAIL  instr.c => instr.o (cparser)"; head -3 "$STMP/err"; status=1
    fi
    rm -rf "$STMP"
fi

# vypis -p musi byt platne C++: preklad g++ a stejny vystup programu
echo "=== vypis -p (g++)"
PTMP=$(mktemp -d) || exit 1
compare_p ()   # compare_p <program>
{
    if ./cmini/test_instr.bin -p -0 -o "$PTMP/x.bin" tests/examples/$1.cc > "$PTMP/$1.cc" 2> "$PTMP/$1.err" &&
       g++ -std=c++17 -w -iquote cmini/ownheaders "$PTMP/$1.cc" -o "$PTMP/$1" -lm 2>> "$PTMP/$1.err" &&
       "$PTMP/$1" | cmp -s - tests/examples/expected/$1.txt
    then echo "  OK    tests/examples/$1.cc -p"
    else echo "  FAIL  tests/examples/$1.cc -p"; head -3 "$PTMP/$1.err"; status=1
    fi
}
for p in dama jezdec avl btree veze classes strings defaults chars dtors floats pointers sequence dictionary dyncast; do compare_p $p; done
[ "$LEXER" = "lexer" ] && compare_p preproc
rm -rf "$PTMP"
exit $status
