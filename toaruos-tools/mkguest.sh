#!/bin/bash
# mkguest.sh — assembles the sources for building inside ToaruOS: _build/guest-src/
#
#   ./mkguest.sh                  # _build/guest-src/ from the current tree
#   OUT=/somewhere/else ./mkguest.sh
#
# Package = copies from the main tree (asm/, cmini/, include/) +
# hand-written files from guest/ (build.sh, pokus.c, README-toaru.txt, regenerate/).
# It is regenerated every time so it cannot go stale (formerly a frozen snapshot _preklad/,
# later guest-src/, that was patched by hand: -T, ToaruTarget, jump before the data).
# cmini/instr.c (generator in C) and asm/*.c - run make all first
# (make all in the root of the tree).
#
# Sequence: ./mkguest.sh ; ./mkpkg.sh ; ./mkshare.sh ; ./toaru-vm.sh start
set -euo pipefail

DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
TOP=$(cd "$DIR/.." && pwd)
OUT=${OUT:-${TOP}/_build/guest-src}

die() { echo "error: $*" >&2; exit 1; }

[[ -f $TOP/asm/code.c && -f $TOP/cmini/instr.c ]] || die "in $TOP missing asm/code.c or cmini/instr.c (make all)"
[[ -f $DIR/guest/build.sh ]] || die "missing $DIR/guest/build.sh"

rm -rf "$OUT"
mkdir -p "$OUT/asm" "$OUT/cmini"

# pasm assembler: sources, headers, description, tests
cp -a "$TOP"/asm/*.c "$TOP"/asm/*.h "$TOP"/asm/*.txt "$TOP"/asm/Makefile "$OUT/asm/"
cp -a "$TOP/asm/tests" "$OUT/asm/"
rm -rf "$OUT/asm/tests/_"*                 # any test outputs

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
