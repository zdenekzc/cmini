#!/bin/bash
# mkarm64.sh - payload for ToaruOS aarch64 (x86_64 host): the cmini TRANSLATE variant.
#
#   ./mkarm64.sh      => _build/arm64/ramdisk.tgz (+ kernel-a64, bootstub-a64)
#
# The release ramdisk (_import/download/aarch64-virt/ramdisk.igz) is repacked with
#   /extras/tcc/bin/tcc        TinyCC that runs INSIDE ToaruOS aarch64 (mktcc-aarch64.sh)
#   /extras/tcc/lib, include   its sysroot (crt0.o, crti.o, crtn.o, libc.a, libtcc1.a)
#   /extras/cmini-c/*.c        cmini -C output (translate.c) of tests/examples/*.cc
#   /extras/cmini-c/cminirt.h  runtime header of the translated programs
#   /extras/cmini-c/cminirt.o  runtime, compiled for aarch64 by the cross TinyCC
#   /extras/cmini-c/run_examples.sh  compiles and runs the examples inside ToaruOS
#
# Needs: _build/tcc-aarch64 (toaruos-tools/mktcc-aarch64.sh), ./cmini/cmini.bin (make),
#        fakeroot (owners and setuid bits of the release ramdisk stay as they are).
set -euo pipefail

DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
TOP=$(cd "$DIR/.." && pwd)
. "$TOP/scripts/need.sh"
need fakeroot
need gzip
need python3

B=$TOP/_build
TCC=$B/tcc-aarch64
OUT=$B/arm64
REL=$TOP/_import/download/aarch64-virt
BASE=$REL/ramdisk.igz
W=$OUT/push                     # payload directory (copied over the ramdisk)
CMINI=$TOP/cmini/cmini.bin
[[ -x $TCC/bin/aarch64-toaru-tcc ]] || { echo "error: missing $TCC (toaruos-tools/mktcc-aarch64.sh)" >&2; exit 1; }
[[ -f $BASE ]] || { echo "error: missing $BASE (unzip _import/download/aarch64-virt.zip)" >&2; exit 1; }
[[ -x $CMINI ]] || { echo "error: missing $CMINI (make -C cmini)" >&2; exit 1; }

rm -rf "$OUT"
mkdir -p "$W/extras/tcc/bin" "$W/extras/tcc/lib" "$W/extras/cmini-c" "$W/bin"

echo "== guest tcc and its sysroot"
install -m 755 "$TCC/tcc-guest-aarch64" "$W/extras/tcc/bin/tcc"
ln -s /extras/tcc/bin/tcc "$W/bin/tcc"
install -m 644 "$TCC/lib/crt0.o" "$TCC/lib/crti.o" "$TCC/lib/crtn.o" \
               "$TCC/lib/libc.a" "$TCC/lib/libtcc1.a" "$W/extras/tcc/lib/"
cp -a "$TCC/lib/include" "$W/extras/tcc/include"

echo "== cmini -C (translate) of tests/examples"
PROGRAMS="dama classes avl btree jezdec veze heapsort quicksort strings dyncast"
for p in $PROGRAMS
do
    "$CMINI" -C -I "$TOP/cmini/ownheaders" -o "$W/extras/cmini-c/$p.c" "$TOP/tests/examples/$p.cc" \
        || { echo "error: cmini -C $p" >&2; exit 1; }
done
cp "$TOP/cmini/cminirt.h" "$W/extras/cmini-c/"

echo "== cminirt.o for aarch64 (cross TinyCC)"
"$TCC/bin/aarch64-toaru-tcc" -B"$TCC/lib" -I "$TOP/cmini" -c "$TOP/cmini/cminirt.c" \
    -o "$W/extras/cmini-c/cminirt.o" 2> "$OUT/cminirt.log" || { cat "$OUT/cminirt.log" >&2; exit 1; }

echo "== rc_check.c: main returns 7 (checks the exit code path in ToaruOS aarch64)"
printf '/* rc_check.c - main returns 7: the exit status must come back to the shell as RC=7 */\nint main (void)\n{\n   return 7;\n}\n' > "$W/extras/cmini-c/rc_check.c"

echo "== /extras/cmini-c/run_examples.sh (esh-safe: no loops, one line per command)"
{
    cat <<'GUEST_EOF'
#!/bin/sh
# run_examples.sh - the translated examples compiled and run by TinyCC inside ToaruOS.
# Output goes to /tmp; each program is framed by ==BEG-<name>== and ==END-<name>==.
GUEST_EOF
    for p in $PROGRAMS
    do
        printf 'echo ==BEG-%s==\n' "$p"
        printf '/extras/tcc/bin/tcc -w -I /extras/cmini-c -o /tmp/%s /extras/cmini-c/%s.c /extras/cmini-c/cminirt.o\n' "$p" "$p"
        printf 'if stat -q "/tmp/%s" then /tmp/%s\n' "$p" "$p"
        printf 'echo RC=$?\n'
        printf 'echo ==END-%s==\n' "$p"
    done
    # exit-code check: main returns 7, the shell must print RC=7
    printf 'echo ==BEG-rc_check==\n'
    printf '/extras/tcc/bin/tcc -w -o /tmp/rc_check /extras/cmini-c/rc_check.c\n'
    printf 'if stat -q "/tmp/rc_check" then /tmp/rc_check\n'
    printf 'echo RC=$?\n'
    printf 'echo ==END-rc_check==\n'
} > "$W/extras/cmini-c/run_examples.sh"
chmod 755 "$W/extras/cmini-c/run_examples.sh"

echo "== repacking the release ramdisk under fakeroot"
fakeroot -- bash -c '
    set -eu
    W="$1"; BASE="$2"; PUSH="$3"; OUT="$4"
    gzip -dc "$BASE" | tar --same-owner -xf - -C "$W"
    cp -a "$PUSH/." "$W/"
    find "$W/extras" -exec chown 1000:1000 {} +
    ( cd "$W" && find . -mindepth 1 -maxdepth 1 -printf "%P\0" |
        tar --null --numeric-owner -cf - -T - ) | gzip -9 -n > "$OUT"
' repack "$(mktemp -d "$OUT/unpack.XXXXXX")" "$BASE" "$W" "$OUT/ramdisk.tgz"
rm -rf "$OUT"/unpack.*

install -m 644 "$REL/misaka-kernel" "$OUT/kernel-a64"
install -m 755 "$REL/bootstub" "$OUT/bootstub-a64"
rm -rf "$W"
echo "== done: $OUT/ramdisk.tgz, $OUT/kernel-a64, $OUT/bootstub-a64"
