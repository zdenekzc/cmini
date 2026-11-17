#!/bin/sh
# Generates a FOREIGN version of cmini: .cc -> .c via cmini.bin -C (runs on the
# host; the resulting .c files are packaged for ToaruOS, where any other C compiler is enough).
set -u
# SRC = the earlier development tree (with cmini/*.cc, asm/*.cc, include/). Not part of this
# project: it must be set in the environment, e.g.  SRC=/path/to/old/cecko sh gen.sh
SRC=${SRC:?set SRC to the earlier development tree (see README.md §0)}
B=/tmp/cmini-bundle/cmini-src
T=/tmp/cmini-bundle/bin/cmini.bin
E=/tmp/cecko-ctest
mkdir -p "$E" "$B/cmini" "$B/asm" || exit 1
cd "$SRC" || exit 1
status=0

for f in decl expr stat lexer instr0; do
    if "$T" -C -I include -o "$B/cmini/$f.c" "cmini/$f.cc" 2>"$E/tr_fe_$f.err"; then
        echo "OK   cmini/$f.c  $(wc -l < "$B/cmini/$f.c") radku"
    else
        echo "FAIL cmini/$f.cc"; head -4 "$E/tr_fe_$f.err"; status=1
    fi
done

for f in pasmlib code def expr inst key oth pasm; do
    if "$T" -C -I include -I asm -o "$B/asm/$f.c" "asm/$f.cc" 2>"$E/tr_asm_$f.err"; then
        echo "OK   asm/$f.c  $(wc -l < "$B/asm/$f.c") radku"
    else
        echo "FAIL asm/$f.cc"; head -4 "$E/tr_asm_$f.err"; status=1
    fi
done

exit $status
