#!/bin/bash
# mktcc.sh - TinyCC (cross build for ToaruOS): cmini and pasm (library)
#   _build/tcc/cmini-tcc.bin    cmini compiled by TinyCC, linked with pasm -k against ToaruOS libc
#   _build/tcc/obj/*.o          TinyCC objects: cmini (cmini/*.c, cminirt.o), pasm (asm/*.c)
#
#   ./mktcc.sh                  needs: _build/toaru-lib (mklibc.sh), TinyCC (TCC=...)
#
# Headers: tcc/include/stdint.h (fixed-width types; TinyCC has no __GNUC__, so the
# ToaruOS stdint.h does not work) + _build/toaru-include (ToaruOS headers) + TinyCC headers.
set -euo pipefail

DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
TOP=$(cd "$DIR/.." && pwd)
TCC=${TCC:-/opt/tinycc/bin/tcc}
B=$TOP/_build
L=$B/toaru-lib
OUT=$B/tcc
O=$OUT/obj
PASM=$TOP/asm/pasm.bin

die() { echo "error: $*" >&2; exit 1; }
[[ -x $TCC ]] || die "missing TinyCC ($TCC)"
[[ -f $L/libc.a ]] || die "missing ToaruOS libc ($L/libc.a) - mklibc.sh"
[[ -x $PASM ]] || die "missing $PASM (make all)"

rm -rf "$OUT"
mkdir -p "$O"
TI="-nostdinc -isystem $DIR/tcc/include -isystem $B/toaru-include -isystem /opt/tinycc/lib/tcc/include -D__toaru__=1 -D__toaru=1 -Dtoaru=1"
tc() { "$TCC" -c $TI "$@" || die "tcc $*"; }

# cmini (c-mini) and pasm (library) - same sources as mkcross.sh (CMINI, PASMLIB)
for f in instr_drv translate decl expr stat sem util lexer instr cminirt; do
    tc -DUSE_LEXER -I "$TOP/cmini" -I "$TOP/asm" "$TOP/cmini/$f.c" -o "$O/c_$f.o"
done
for f in pasmlib code def expr inst key oth asmlist pasm; do
    tc -I "$TOP/asm" "$TOP/asm/$f.c" -o "$O/a_$f.o"
done
# cmini-tcc.bin: all objects + ToaruOS libc, static, pasm -k
L_CMINI=""
for f in instr_drv translate decl expr stat sem util lexer instr cminirt; do L_CMINI="$L_CMINI $O/c_$f.o"; done
ASMOBJ=""
for f in pasmlib code def expr inst key oth asmlist; do ASMOBJ="$ASMOBJ $O/a_$f.o"; done
"$PASM" -k "$OUT/cmini-tcc.bin" "$L/crt0.o" "$L/crti.o" $L_CMINI $ASMOBJ "$L/libc.a" "$L/crtn.o" \
    || die "pasm -k cmini-tcc.bin"
echo "tcc: $OUT/cmini-tcc.bin ($(stat -c %s "$OUT/cmini-tcc.bin") B), objects in $O"
