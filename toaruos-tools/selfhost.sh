#!/bin/bash
# selfhost.sh - cmini cross-compiles cmini (and pasm) FOR ToaruOS, using only cmini
# (cmini.bin -L /lib -c per source file, then pasm -k as the static linker).
# No C-to-C translation (-C) and no gcc is used for building the ToaruOS compilers.
#
#   ./selfhost.sh          => _build/selfhost/push/cmini-toaru.tar.gz  (bin/cmini.bin, bin/pasm.bin,
#                             include/, lib/cminirt.o, tests/examples/)
#                             _build/selfhost/push/cmini-src.tar.gz   (sources for a rebuild in ToaruOS)
#                             _build/ramdisk.tgz                      (mkramdisk.sh with those archives)
#
# Needs: _build/toaru-lib (mklibc.sh), cmini.bin and asm/pasm.bin built on the host (make).
set -euo pipefail

DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
TOP=$(cd "$DIR/.." && pwd)
L=$TOP/_build/toaru-lib
B=$TOP/_build/selfhost
CMINI=${CMINI:-$TOP/cmini/cmini.bin}
PASM=${PASM:-$TOP/asm/pasm.bin}
CINC="-I $TOP/cmini/ownheaders"

die() { echo "error: $*" >&2; exit 1; }
[[ -x $CMINI ]] || die "missing $CMINI (make -C cmini)"
[[ -x $PASM ]] || die "missing $PASM (make -C asm)"
[[ -f $L/libc.a && -f $L/crt0.o ]] || "$DIR/mklibc.sh" > /dev/null || die "mklibc.sh"

O=$B/obj
rm -rf "$B"; mkdir -p "$O" "$B/out/cmini/bin" "$B/out/cmini/lib" "$B/out/cmini/tests/examples" "$B/push"

# 1. objects for ToaruOS: cmini compiles each source (pasm backend, relocatable, -L /lib headers)
cc() { "$CMINI" -L "$L" $CINC -I "$TOP/cmini" -I "$TOP/asm" -D calloc=cm_calloc -c "$1" "$2" || die "cmini -c $1"; }
CM=""
for f in instr_drv translate decl expr stat sem util lexer instr; do
    cc "$O/c_$f.o" "$TOP/cmini/$f.c"; CM="$CM $O/c_$f.o"
done
cc "$O/c_cminirt.o" "$TOP/cmini/cminirt.c"
PL=""
for f in pasmlib code def expr inst key oth asmlist; do
    cc "$O/a_$f.o" "$TOP/asm/$f.c"; PL="$PL $O/a_$f.o"
done
cc "$O/a_pasm.o" "$TOP/asm/pasm.c"
# calloc that zeroes memory (ToaruOS libc calloc does not; see selfhost/calloc_zero.c)
"$CMINI" -L "$L" $CINC -c "$O/calloc_zero.o" "$DIR/selfhost/calloc_zero.c" || die "cmini calloc_zero"
# _setjmp alias: cmini calls _setjmp (ownheaders), ToaruOS libc has setjmp only
cp "$DIR/selfhost/setjmp_alias.asm" "$O/"
(cd "$O" && "$PASM" -c setjmp_alias.asm > /dev/null) || die "pasm setjmp alias"
[[ -f $O/setjmp_alias.o ]] || die "setjmp alias"

# 2. static link with pasm -k (ToaruOS crt + libc.a)
"$PASM" -k "$B/out/cmini/bin/cmini.bin" $L/crt0.o $L/crti.o $CM $PL $O/setjmp_alias.o $O/calloc_zero.o $L/libc.a $L/crtn.o || die "link cmini"
"$PASM" -k "$B/out/cmini/bin/pasm.bin" $L/crt0.o $L/crti.o $O/a_pasm.o $PL $O/setjmp_alias.o $O/calloc_zero.o $L/libc.a $L/crtn.o || die "link pasm"
cp "$O/c_cminirt.o" "$B/out/cmini/lib/cminirt.o"

# 2b. runtime check of ToaruOS libc as seen by cmini-compiled code (selfhost/rt.c):
#     malloc/realloc/strdup/snprintf/qsort/fopen - the output is compared in the VM
"$CMINI" -L "$L" $CINC -c "$O/rt.o" "$DIR/selfhost/rt.c" || die "cmini rt"
"$PASM" -k "$O/rt.bin" $L/crt0.o $L/crti.o "$O/rt.o" $L/libc.a $L/crtn.o || die "link rt"
"$CMINI" -L "$L" $CINC -c "$O/realloc.o" "$DIR/selfhost/realloc.c" && "$PASM" -k "$O/realloc.bin" $L/crt0.o $L/crti.o "$O/realloc.o" $L/libc.a $L/crtn.o || die "link realloc"
"$CMINI" -L "$L" $CINC -c "$O/init.o" "$DIR/selfhost/init.c" && "$PASM" -k "$O/init.bin" $L/crt0.o $L/crti.o "$O/init.o" $L/libc.a $L/crtn.o || die "link init"
"$CMINI" -L "$L" $CINC -c "$O/heap.o" "$DIR/selfhost/heap.c" && "$PASM" -k "$O/heap.bin" $L/crt0.o $L/crti.o "$O/heap.o" $L/libc.a $L/crtn.o || die "link heap"
"$CMINI" -L "$L" $CINC -c "$O/textadd.o" "$DIR/selfhost/textadd.c" && "$PASM" -k "$O/textadd.bin" $L/crt0.o $L/crti.o "$O/textadd.o" $L/libc.a $L/crtn.o || die "link textadd"

"$CMINI" -L "$L" $CINC -c "$O/stack.o" "$DIR/selfhost/stack.c" && "$PASM" -k "$O/stack.bin" $L/crt0.o $L/crti.o "$O/stack.o" $L/libc.a $L/crtn.o || die "stack test"
"$CMINI" -L "$L" $CINC -c "$O/jmp.o" "$DIR/selfhost/jmp.c" && "$PASM" -k "$O/jmp.bin" $L/crt0.o $L/crti.o "$O/jmp.o" "$O/setjmp_alias.o" $L/libc.a $L/crtn.o || die "jmp test"

# 2c. pasm check: host cmini writes the asm text of a small program, host pasm makes the
#     ToaruOS binary; the same asm is assembled by pasm-toaru in the VM and compared (cmp)
mkdir -p "$O/pasmcheck"
cp "$DIR/selfhost/triv.c" "$O/pasmcheck/"
(cd "$O/pasmcheck" && "$TOP/cmini/cmini.bin" -T -S triv.asm -o triv triv.c > /dev/null 2>&1; "$PASM" -e -T triv.asm > /dev/null) || die "host pasm check"
[[ -s $O/pasmcheck/triv.bin ]] || die "host pasm check: no triv.bin"

# 3. package layout as mkcross.sh: include/, tests/examples/ and their expected outputs
cp -a "$TOP/cmini/ownheaders" "$B/out/cmini/include"
mkdir -p "$B/out/cmini/tests/pasmcheck" && cp "$O/pasmcheck/triv.asm" "$O/pasmcheck/triv.bin" "$B/out/cmini/tests/pasmcheck/"
mkdir -p "$B/out/cmini/tests" && cp "$O/rt.bin" "$B/out/cmini/tests/rt.bin"
# optional: tiny-ide built by cmini for ToaruOS (tiny-ide/build-toaru-cmini.sh) and its demo
if [[ -n ${TIDE_BIN:-} && -f $TIDE_BIN ]]; then
    cp "$TIDE_BIN" "$B/out/cmini/tests/tiny-ide-toaru.bin"
    mkdir -p "$B/out/cmini/tests/tide-demo" && cp "$TOP/tiny-ide/demo/hello.c" "$B/out/cmini/tests/tide-demo/"
fi
# optional: mini-commander built by cmini for ToaruOS (mini-commander/build-toaru-cmini.sh)
if [[ -n ${MC_BIN:-} && -f $MC_BIN ]]; then
    cp "$MC_BIN" "$B/out/cmini/tests/mc-toaru.bin"
fi
# optional: a test program for the curses shim (cmini-curses/tests/keys.c)
if [[ -n ${KEYS_BIN:-} && -f $KEYS_BIN ]]; then cp "$KEYS_BIN" "$B/out/cmini/tests/keys.bin"; fi
# optional debugging build (instrumented cmini, see the trace notes): TRACE_BIN=/path/cmini-trace.bin
if [[ -n ${TRACE_BIN:-} && -f $TRACE_BIN ]]; then cp "$TRACE_BIN" "$B/out/cmini/tests/cmini-trace.bin"; fi && cp "$O/stack.bin" "$B/out/cmini/tests/stack.bin" && cp "$O/jmp.bin" "$B/out/cmini/tests/jmp.bin" && cp "$O/realloc.bin" "$B/out/cmini/tests/realloc.bin" && cp "$O/init.bin" "$B/out/cmini/tests/init.bin" && cp "$O/textadd.bin" "$B/out/cmini/tests/textadd.bin" && cp "$O/heap.bin" "$B/out/cmini/tests/heap.bin"
cp "$TOP"/tests/examples/*.cc "$B/out/cmini/tests/examples/"
cp -a "$TOP/tests/examples/expected" "$B/out/cmini/tests/examples/"
cat > "$B/out/cmini/README.txt" <<'TXT'
cmini cross-compiled for ToaruOS by cmini itself (toaruos-tools/selfhost.sh).
  bin/cmini.bin -L /lib -I include -o /tmp/p tests/examples/dama.cc
  export PASM_LIBDIR=/lib; bin/pasm.bin -x prog.asm
TXT
tar --format=ustar --sort=name --owner=0 --group=0 --numeric-owner --mtime=@0 \
    -cf - -C "$B/out" cmini | gzip -n > "$B/push/cmini-toaru.tar.gz"

# 3b. control (only for debugging the port): the gcc-linked compilers of mkcross.sh
if [[ -n ${OLD_PKG:-} && -f $OLD_PKG ]]; then
    mkdir -p "$B/out/cmini/old-gcc"
    tar -xzf "$OLD_PKG" -C "$O" cmini/bin
    cp "$O/cmini/bin/cmini.bin" "$O/cmini/bin/pasm.bin" "$B/out/cmini/old-gcc/"
fi

# 4. sources for a rebuild inside ToaruOS (cmini and pasm, headers, ownheaders)
S=$B/src/cmini-src
mkdir -p "$S/cmini" "$S/asm" "$S/cmini/ownheaders"
cp -a "$TOP/cmini/ownheaders/." "$S/cmini/ownheaders/"
cp "$TOP"/cmini/*.c "$TOP"/cmini/*.h "$S/cmini/" 2>/dev/null || true
cp "$TOP"/asm/*.c "$TOP"/asm/*.h "$S/asm/" 2>/dev/null || true
mkdir -p "$S/selfhost" && cp "$DIR/selfhost/setjmp_alias.asm" "$DIR/selfhost/calloc_zero.c" "$DIR/selfhost/rt.c" "$S/selfhost/"
tar --format=ustar --sort=name --owner=0 --group=0 --numeric-owner --mtime=@0 \
    -cf - -C "$B/src" cmini-src | gzip -n > "$B/push/cmini-src.tar.gz"

echo "selfhost: $B/push ($(ls "$B/push" | tr '\n' ' '))"
