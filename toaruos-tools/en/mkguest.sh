#!/bin/bash
# mkguest.sh — assembles the sources for compilation inside ToaruOS: _build/guest-src/
#
#   ./mkguest.sh                  # _build/guest-src/ from the current cecko tree
#   OUT=/somewhere/else ./mkguest.sh
#
# Package = a copy from the main tree (cecko/asm, cecko/cmini, cecko/include) +
# hand-written files from guest/ (build.sh, pokus.c, README-toaru.txt, regenerate/).
# It is generated anew every time so it does not go stale (formerly a frozen snapshot,
# later guest-src/, was brought up to date by hand: -T, ToaruTarget, jump before the data).
# cmini/instr.c is generated from cmini/instr.cc (make -C cmini) - run
# make all in the tree root first.
#
# Sequence: ./mkguest.sh ; ./mkpkg.sh ; ./mkshare.sh ; ./toaru-vm.sh start
set -euo pipefail

DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# the main cecko tree is not always two levels up — see toaru-top.sh
. "$DIR/toaru-top.sh"
TOP=$(find_top "$DIR") \
    || { echo "error: no main cecko tree (asm/code.c + cmini/instr.c) — set TOP=/path" >&2; exit 1; }
OUT=${OUT:-${DIR}/_build/guest-src}

die() { echo "error: $*" >&2; exit 1; }

[[ -f $TOP/asm/code.c && -f $TOP/cmini/instr.c ]] || die "$TOP is missing asm/code.c or cmini/instr.c (make all)"
[[ -f $DIR/guest/build.sh ]] || die "missing $DIR/guest/build.sh"

rm -rf "$OUT"
mkdir -p "$OUT/asm" "$OUT/cmini"

# assembler pasm: sources, headers, description, tests
cp -a "$TOP"/asm/*.c "$TOP"/asm/*.h "$TOP"/asm/*.txt "$TOP"/asm/Makefile "$OUT/asm/"
cp -a "$TOP/asm/tests" "$OUT/asm/"
rm -rf "$OUT/asm/tests/_"*                 # possible test outputs

# cmini compiler in C (build.sh compiles instr_drv decl expr stat sem lexer instr cminirt)
for f in cminirt.c cminirt.h decl.c decl.h expr.c ifc.h inp.c inp.h instr.c instr_drv.c translate.c \
         instr.h lex.c lexer.c lexer.h lex.h out.c out.h sem.c stat.c translate.c util.c util.h; do
    cp -a "$TOP/cmini/$f" "$OUT/cmini/"
done

# headers for programs compiled by cmini
cp -a "$TOP/cmini/ownheaders" "$OUT/include"

# hand-written parts of the package
cp -a "$DIR"/guest/. "$OUT"/

echo "guest-src: $OUT ($(find "$OUT" -type f | wc -l) files)"
