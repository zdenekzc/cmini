#!/bin/bash
# mkcross.sh — compilers FOR ToaruOS built ON THE HOST (cross build):
#   _build/cross/cecko/          bin/cmini.bin pasm.bin, include/, lib/cminirt.o,
#                                tests/examples, README.txt
#   _build/push/cecko-toaru.tar.gz   the same as an archive (tests/15, tests/16 send it to the host)
#
#   ./mkcross.sh          (tree built - make all; ISO in _import/)
#
# On ToaruOS, gcc (package build-essential) and ld.so are NOT needed:
#   - compilation: host gcc with ToaruOS headers (_build/toaru-include from ISO,
#     mklibc.sh), -nostdinc, without Linux macros (__linux__ ...), with ToaruOS macros
#     (__toaru__, toaru - as the ToaruOS gcc does), -fno-pie -fno-stack-protector
#     -fcf-protection=none; implicit declarations only warn (gcc 14+ would treat
#     them as errors; the ToaruOS headers e.g. do not declare chmod)
#   - linking: static linker pasm (asm/pasm.bin -k, asm/code.c LINKER)
#     with ToaruOS crt0.o crti.o libc.a crtn.o (_build/toaru-lib)
#   - sources: cmini and pasm from mkguest.sh (_build/guest-src)
#
# On ToaruOS (tar xzf cecko-toaru.tar.gz; cd cecko; see README.txt):
#   bin/cmini.bin -T -o p p.c                 own library, ToaruOS syscalls
#   bin/cmini.bin -L /lib -o p p.c            ToaruOS libc from /lib, pasm linker
#   export PASM_LIBDIR=/lib; bin/pasm.bin -x p.asm
set -euo pipefail

DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
TOP=$(cd "$DIR/.." && pwd)
B=$DIR/_build
OUT=${OUT:-$B/cross}
TAR=${TAR:-$B/push/cecko-toaru.tar.gz}
PASM=$TOP/asm/pasm.bin

die() { echo "error: $*" >&2; exit 1; }
[[ -x $PASM ]] || die "missing $PASM (make all)"
if [[ ! -f $B/toaru-lib/libc.a || ! -f $B/toaru-include/stdio.h ]]; then
    "$DIR/mklibc.sh" > /dev/null || die "mklibc.sh"
fi
# sources (and at the same time archive for the host: _build/push/cmini-src.tar.gz)
OUT= SRC= "$DIR/mkpkg.sh" > /dev/null || die "mkpkg.sh"         # calls mkguest.sh

L=$B/toaru-lib
CF="-O1 -std=gnu11 -w -Wno-error=implicit-function-declaration -Wno-error=int-conversion
    -Wno-error=incompatible-pointer-types -nostdinc -isystem $B/toaru-include
    -isystem $(gcc -print-file-name=include) -fno-pie -fno-pic -fno-stack-protector
    -fcf-protection=none -fno-asynchronous-unwind-tables -U__linux__ -U__linux -Ulinux
    -U__gnu_linux__ -U__unix__ -D__toaru__=1 -D__toaru=1 -Dtoaru=1"
O=$OUT/obj
rm -rf "$OUT"
mkdir -p "$O" "$OUT/cecko/bin" "$OUT/cecko/lib" "$OUT/cecko/tests/examples"

cc() { gcc $CF "$@" 2> "$O/cc.log" || { cat "$O/cc.log" >&2; die "gcc $*"; }; }

# pasm (library and program) and cmini - sources as in guest/build.sh
G=$B/guest-src
PASMLIB=""
for f in pasmlib code def expr inst key oth asmlist; do
    cc -I"$G/cmini" -I"$G/asm" -c "$G/asm/$f.c" -o "$O/a_$f.o"
    PASMLIB="$PASMLIB $O/a_$f.o"
done
cc -I"$G/cmini" -I"$G/asm" -c "$G/asm/pasm.c" -o "$O/a_pasm.o"
CMINI=""
for f in instr_drv translate decl expr stat sem util lexer instr cminirt; do
    cc -DUSE_LEXER -I"$G/cmini" -I"$G/asm" -c "$G/cmini/$f.c" -o "$O/c_$f.o"
    CMINI="$CMINI $O/c_$f.o"
done

# runtime library of programs from cmini (and gcc on the host)
cc -I"$TOP/cmini" -c "$TOP/cmini/cminirt.c" -o "$OUT/cecko/lib/cminirt.o"

link() { local out=$1; shift; "$PASM" -k "$out" "$L/crt0.o" "$L/crti.o" "$@" "$L/libc.a" "$L/crtn.o" || die "pasm -k $out"; }
link "$OUT/cecko/bin/pasm.bin" "$O/a_pasm.o" $PASMLIB
link "$OUT/cecko/bin/cmini.bin" $CMINI $PASMLIB

# headers for programs compiled by cmini, examples (../../include/containers.h)
cp -a "$TOP/cmini/ownheaders" "$OUT/cecko/include"
cp "$TOP"/tests/examples/*.cc "$OUT/cecko/tests/examples/"
cp "$TOP/asm/tests/libc64.asm" "$TOP/asm/tests/sse64.asm" "$OUT/cecko/tests/"
cat > "$OUT/cecko/README.txt" <<'EOF'
cecko for ToaruOS - compilers built on the host (toaruos-tools/mkcross.sh),
statically linked with ToaruOS libc: no gcc or ld.so needed. In ToaruOS:

  cd /tmp/cecko
  bin/cmini.bin -T -o /tmp/dama tests/examples/dama.cc       c-mini, own library
  /tmp/dama
  bin/cmini.bin -L /lib -o /tmp/classes tests/examples/classes.cc
                                       c-mini, ToaruOS libc from /lib (pasm linker)
  export PASM_LIBDIR=/lib
  bin/pasm.bin -x tests/libc64.asm     assembler + linker => tests/libc64.bin

Source texts (gcc from "msk install build-essential" compiles them, network):
  cmini-src.tar.gz  - tar xzf ...; cd cmini-src; sh build.sh      => cmini.bin, pasm.bin
EOF

mkdir -p "$(dirname "$TAR")"
tar --format=ustar --sort=name --owner=0 --group=0 --numeric-owner --mtime=@0 \
    -cf - -C "$OUT" cecko | gzip -n > "$TAR"
echo "cecko-toaru: $OUT/cecko, $TAR ($(wc -c < "$TAR") B)"
ls -l "$OUT/cecko/bin"/*.bin "$OUT/cecko/lib/cminirt.o" | awk '{print "  " $5 "  " $9}'
