#!/bin/sh
# build-own.sh - cstencils (code) translated by cmini, linked with the own runtime
# rtl/rtl.c (no glibc): cmini -c (object via pasm) for each unit, cminirt.c and rtl.c
# compiled freestanding, static link with gcc -nostdlib.
#
#   sh rtl/build-own.sh [OUT_DIR]     default OUT_DIR = ${TMPDIR:-/tmp}/cstencils-own
#   CMINI=path/to/cmini.bin           default ../cmini/cmini.bin (relative to this directory)

set -e
D=$(cd "$(dirname "$0")/.." && pwd)              # cstencils directory
CMINI=${CMINI:-$D/../cmini/cmini.bin}
CMINIRT=$D/../cminirt/cminirt.c
OUT=${1:-${TMPDIR:-/tmp}/cstencils-own}
mkdir -p "$OUT"
cd "$D"

FREE="-std=gnu11 -w -O2 -ffreestanding -fno-builtin -fno-stack-protector \
      -fno-tree-loop-distribute-patterns -fno-pic -fno-asynchronous-unwind-tables -U_FORTIFY_SOURCE"

gcc $FREE -c rtl/rtl.c -o "$OUT/rtl.o"
gcc $FREE -c "$CMINIRT" -o "$OUT/cminirt.o"

objs="$OUT/rtl.o $OUT/cminirt.o"
for u in main-code elem fact field flexible stencil code utils/conv utils/envio utils/inf utils/str utils/textio
do
   b=$(echo "$u" | tr / _)
   "$CMINI" -I . -I utils -I "$D/../cmini/ownheaders" -c "$OUT/$b.o" -S "$OUT/$b.asm" "$u.cc"
   objs="$objs $OUT/$b.o"
done

gcc -nostdlib -static -no-pie -o "$OUT/code" $objs -lgcc
echo "built: $OUT/code"
