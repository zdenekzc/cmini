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

echo "== tiny-ide and mini-commander: cmini -C (translate), compiled by the cross TinyCC"
# the same sources and the same curses layer (toaruos-tools/cmini-curses) as the x86_64 builds;
# uintmax_t: cmini_libc.h (mini-commander) declares it as long unsigned int, the sysroot as tcc's type
CTCC="$TCC/bin/aarch64-toaru-tcc"
CMINI=$TOP/cmini/cmini.bin
TCCFLAGS=(-B"$TCC/lib" -w -I "$TOP/cminirt" -include "$TOP/toaruos-tools/tcc/cmini-compat.h" "-D__UINTMAX_TYPE__=long unsigned int" -D TOARU_CMINI=1 -D calloc=cm_calloc)
A=$B/arm64/apps
rm -rf "$A"; mkdir -p "$A/tide" "$A/mc"
SHIM=$TOP/toaruos-tools/cmini-curses
(
    cd "$TOP/tiny-ide"
    for f in main app editor runner ui dialogs
    do "$CMINI" -C -I "$TOP/cmini/ownheaders" -I "$SHIM" -D TOARU_CMINI=1 -D calloc=cm_calloc -o "$A/tide/$f.c" "$f.c" \
          || { echo "error: cmini -C tiny-ide/$f.c" >&2; exit 1; }
    done
)
(
    cd "$TOP/mini-commander"
    for f in mc cmd operations dialog filelist init panel ui view_edit progress
    do "$CMINI" -C -I "$TOP/mini-commander/cmini-toaru" -I "$SHIM" -D TOARU_CMINI=1 -D calloc=cm_calloc -o "$A/mc/$f.c" "$f.c" \
          || { echo "error: cmini -C mini-commander/$f.c" >&2; exit 1; }
    done
)
for f in curses8 glue
do "$CTCC" "${TCCFLAGS[@]}" -I "$SHIM" -c "$SHIM/$f.c" -o "$A/tide/$f.o" || { echo "error: tcc $f.c" >&2; exit 1; }
done
"$CTCC" -B"$TCC/lib" -w -c "$TOP/toaruos-tools/selfhost/calloc_zero.c" -o "$A/tide/calloc_zero.o" || exit 1
"$CTCC" -B"$TCC/lib" -w -c "$TOP/cminirt/cminirt.c" -I "$TOP/cminirt" -o "$A/tide/cminirt.o" || exit 1
for f in main app editor runner ui dialogs
do "$CTCC" "${TCCFLAGS[@]}" -I "$SHIM" -I "$TOP/tiny-ide/toaru-cmini" -c "$A/tide/$f.c" -o "$A/tide/$f.o" || { echo "error: tcc tiny-ide/$f.c" >&2; exit 1; }
done
"$CTCC" "${TCCFLAGS[@]}" -o "$A/tide/tiny-ide-toaru.bin" "$A/tide"/{main,app,editor,runner,ui,dialogs,curses8,glue,calloc_zero}.o "$A/tide/cminirt.o" \
      || { echo "error: link tiny-ide" >&2; exit 1; }
for f in mc cmd operations dialog filelist init panel ui view_edit progress
do "$CTCC" "${TCCFLAGS[@]}" -I "$TOP/mini-commander/cmini-toaru" -I "$SHIM" -c "$A/mc/$f.c" -o "$A/mc/$f.o" || { echo "error: tcc mc/$f.c" >&2; exit 1; }
done
"$CTCC" "${TCCFLAGS[@]}" -o "$A/mc/mc-toaru.bin" "$A/mc"/{mc,cmd,operations,dialog,filelist,init,panel,ui,view_edit,progress}.o \
      "$A/tide/curses8.o" "$A/tide/glue.o" "$A/tide/calloc_zero.o" "$A/tide/cminirt.o" \
      || { echo "error: link mini-commander" >&2; exit 1; }
mkdir -p "$W/extras/tiny-ide"; install -m 755 "$A/tide/tiny-ide-toaru.bin" "$W/extras/tiny-ide/tiny-ide-toaru.bin"
mkdir -p "$W/extras/mini-commander"; install -m 755 "$A/mc/mc-toaru.bin" "$W/extras/mini-commander/mc-toaru.bin"

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
