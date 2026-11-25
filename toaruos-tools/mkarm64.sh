#!/bin/bash
# mkarm64.sh - payload for ToaruOS aarch64 (x86_64 host): the cmini TRANSLATE variant.
#
#   ./mkarm64.sh      => _build/ramdisk-a64.tgz, _build/kernel-a64 (+ _build/arm64/bootstub-a64)
#
# The release ramdisk (_import/download/aarch64-virt/ramdisk.igz) is repacked with
#   /extras/tcc/bin/tcc         TinyCC that runs INSIDE ToaruOS aarch64 (mktcc-aarch64.sh)
#   /extras/tcc/lib, include    its sysroot (crt0.o, crti.o, crtn.o, libc.a, libtcc1.a)
#   /extras/examples, /extras/build-examples.sh   (toaruos-tools/mkexamples.sh a64)
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
OUT=$B/arm64                    # bootstub and the unpacked payload live here
REL=$TOP/_import/download/aarch64-virt
BASE=$REL/ramdisk.igz
W=$OUT/push                     # payload directory (copied over the ramdisk)
[[ -x $TCC/bin/aarch64-toaru-tcc ]] || { echo "error: missing $TCC (toaruos-tools/mktcc-aarch64.sh)" >&2; exit 1; }
[[ -f $BASE ]] || { echo "error: missing $BASE (unzip _import/download/aarch64-virt.zip)" >&2; exit 1; }

rm -rf "$OUT"
mkdir -p "$W/extras/tcc/bin" "$W/extras/tcc/lib" "$W/bin"

echo "== guest tcc and its sysroot"
install -m 755 "$TCC/tcc-guest-aarch64" "$W/extras/tcc/bin/tcc"
ln -s /extras/tcc/bin/tcc "$W/bin/tcc"
install -m 644 "$TCC/lib/crt0.o" "$TCC/lib/crti.o" "$TCC/lib/crtn.o" \
               "$TCC/lib/libc.a" "$TCC/lib/libtcc1.a" "$W/extras/tcc/lib/"
cp -a "$TCC/lib/include" "$W/extras/tcc/include"

echo "== /extras/examples (cmini -C, cminirt.o for aarch64) and /extras/build-examples.sh"
TCC="$TCC/bin/aarch64-toaru-tcc" "$TOP/toaruos-tools/mkexamples.sh" "$W/extras" a64 > /dev/null

echo "== repacking the release ramdisk under fakeroot"
UNPACK=$(mktemp -d "$OUT/unpack.XXXXXX")
fakeroot -- bash -c '
    set -eu
    W="$1"; BASE="$2"; PUSH="$3"; OUT="$4"
    gzip -dc "$BASE" | tar --same-owner -xf - -C "$W"
    cp -a "$PUSH/." "$W/"
    find "$W/extras" -exec chown 1000:1000 {} +
    ( cd "$W" && find . -mindepth 1 -maxdepth 1 -printf "%P\0" |
        tar --null --numeric-owner -cf - -T - ) | gzip -9 -n > "$OUT"
' repack "$UNPACK" "$BASE" "$W" "$B/ramdisk-a64.tgz"
rm -rf "$UNPACK"

install -m 644 "$REL/misaka-kernel" "$B/kernel-a64"
install -m 755 "$REL/bootstub" "$OUT/bootstub-a64"
rm -rf "$W"
echo "== done: $B/ramdisk-a64.tgz, $B/kernel-a64, $OUT/bootstub-a64"
