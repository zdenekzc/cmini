#!/bin/bash
# mkramdisk.sh - ToaruOS bez ISO: ramdisk s balikem a kernel pro qemu -kernel/-initrd
#
#   ./mkramdisk.sh        (potrebuje _import/toaruos-2.3.2.iso a _build/push/ z mkcross.sh)
#
# Vystup (toaruos-tools/_build/):
#   ramdisk.tgz   ramdisk.igz ze ISO se vsim pod /extras; z _build/push/ se pridaji
#                 rozbalene adresare /extras/cmini-toaru (z cmini-toaru.tar.gz) a
#                 /extras/cmini-src (z cmini-src.tar.gz); archivy se nekopiruji
#                 a /extras/cstencils (zdroje generatoru + unity.cc pro cmini),
#                 /extras/tests/cmini-tests.sh (kroky testu pro terminal ToaruOS)
#   kernel.mb     kernel z ISO, v multiboot hlavicce vypnuty pozadavek na VBE (QEMU
#                 multiboot loader by jinak odmitl "multiboot knows VBE")
#
# Spusteni (toaru-vm.sh): INITRD=_build/ramdisk.tgz KERNEL=_build/kernel.mb ./toaru-vm.sh start
set -euo pipefail

# The whole script runs in ONE fakeroot session (fakeroot keeps its owner database per
# invocation): the stock owners and setuid bits of the ramdisk survive the unpack and
# repack, so /bin/sudo stays setuid root (otherwise "effective uid is not 0") and
# /home/local stays local:local. Without fakeroot the owners get flattened to 1000.
if [[ -z ${MKRAMDISK_UNDER_FAKEROOT:-} ]]; then
    if command -v fakeroot > /dev/null 2>&1; then
        exec fakeroot -- env MKRAMDISK_UNDER_FAKEROOT=1 bash "$0" "$@"
    fi
    echo "warning: fakeroot missing - sudo in ToaruOS loses its setuid bit (sudo dnf install fakeroot)" >&2
fi

DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
TOP=$(cd "$DIR/.." && pwd)
ISO=${ISO:-$TOP/_import/toaruos-2.3.2.iso}
PUSH=${PUSH:-$DIR/_build/push}
B=$DIR/_build
WORK=$B/ramdisk-work

die() { echo "error: $*" >&2; exit 1; }
for t in isoinfo gzip tar python3; do
    command -v $t > /dev/null || die "missing $t"
done
[[ -f "$ISO" ]] || die "missing ISO: $ISO"
[[ -d "$PUSH" ]] || die "missing payload directory $PUSH (run mkcross.sh)"

rm -rf "$WORK"
mkdir -p "$WORK/base" "$WORK/ramdisk" "$B"

echo "== extracting kernel and ramdisk.igz from $ISO"
for f in kernel ramdisk.igz; do
    isoinfo -R -i "$ISO" -x "/$f" > "$WORK/base/$f" 2> /dev/null
    [[ -s "$WORK/base/$f" ]] || die "$f not found in $ISO"
done

echo "== unpacking ramdisk.igz"
gzip -dc "$WORK/base/ramdisk.igz" | tar --same-owner -xf - -C "$WORK/ramdisk"

echo "== copying payload $PUSH into /extras (archives are unpacked as directories)"
mkdir -p "$WORK/ramdisk/extras"
for f in "$PUSH"/*; do
    case "$f" in
        *.tar.gz) ;;                          # unpacked below
        *) cp -a "$f" "$WORK/ramdisk/extras/" ;;
    esac
done
# /extras/tests/cmini-tests.sh: the steps of tests/05-toaru.sh for the ToaruOS terminal
mkdir -p "$WORK/ramdisk/extras/tests"
cp "$DIR/guest/cmini-tests.sh" "$WORK/ramdisk/extras/tests/cmini-tests.sh"
# /extras/cstencils: sources of the stencil generator (without rtl/) and unity.cc,
# one translation unit for cmini (cmini -o makes one program from one source file)
rm -rf "$WORK/ramdisk/extras/cstencils"
(cd "$TOP" && tar --exclude='cstencils/rtl' -cf - cstencils) | tar -xf - -C "$WORK/ramdisk/extras"
cat > "$WORK/ramdisk/extras/cstencils/unity.cc" <<'UNITY'
/* unity.cc - cstencils as one translation unit (cmini: one source file = one program) */
#include "utils/inf.cc"
#include "utils/str.cc"
#include "utils/conv.cc"
#include "utils/envio.cc"
#include "utils/textio.cc"
#include "elem.cc"
#include "fact.cc"
#include "field.cc"
#include "flexible.cc"
#include "stencil.cc"
#include "code.cc"
#include "main-code.cc"
UNITY
# /extras/cmini-toaru: programs (bin/, lib/, include/) from cmini-toaru.tar.gz (top dir cmini/ dropped)
mkdir -p "$WORK/ramdisk/extras/cmini-toaru"
tar -xzf "$PUSH/cmini-toaru.tar.gz" --strip-components=1 -C "$WORK/ramdisk/extras/cmini-toaru"
# /extras/cmini-src: compiler sources from cmini-src.tar.gz (top dir cmini-src/ dropped)
mkdir -p "$WORK/ramdisk/extras/cmini-src"
tar -xzf "$PUSH/cmini-src.tar.gz" --strip-components=1 -C "$WORK/ramdisk/extras/cmini-src"
# /extras/build-new-cmini.sh: cmini compiles cmini in ToaruOS => /extras/new-cmini;
# its helpers (as in selfhost.sh) go to /extras/cmini-src/selfhost
mkdir -p "$WORK/ramdisk/extras/cmini-src/selfhost"
cp "$DIR/selfhost/calloc_zero.c" "$DIR/selfhost/setjmp_alias.asm" "$WORK/ramdisk/extras/cmini-src/selfhost/"
cp "$DIR/guest/build-new-cmini.sh" "$WORK/ramdisk/extras/build-new-cmini.sh"

# /extras/mini-commander/mc-toaru.bin and /extras/tiny-ide/tiny-ide-toaru.bin: built by
# cmini (build-toaru-cmini.sh of each tool); the links in /bin: mc, tiny-ide, cmini
MC_BIN=${MC_BIN:-$TOP/mini-commander/_build/cmini-toaru/mc-toaru.bin}
TIDE_BIN=${TIDE_BIN:-$TOP/tiny-ide/_build/cmini-toaru/tiny-ide-toaru.bin}
for f in "$MC_BIN" "$TIDE_BIN"; do
    [[ -f "$f" ]] || die "missing $f (run the build-toaru-cmini.sh of the tool)"
done
mkdir -p "$WORK/ramdisk/extras/mini-commander" "$WORK/ramdisk/extras/tiny-ide" "$WORK/ramdisk/bin"
cp "$MC_BIN" "$WORK/ramdisk/extras/mini-commander/mc-toaru.bin"
cp "$TIDE_BIN" "$WORK/ramdisk/extras/tiny-ide/tiny-ide-toaru.bin"
chmod 755 "$WORK/ramdisk/extras/mini-commander/mc-toaru.bin" "$WORK/ramdisk/extras/tiny-ide/tiny-ide-toaru.bin"
ln -sfn /extras/mini-commander/mc-toaru.bin "$WORK/ramdisk/bin/mc"
ln -sfn /extras/tiny-ide/tiny-ide-toaru.bin "$WORK/ramdisk/bin/tiny-ide"
ln -sfn /extras/cmini-toaru/bin/cmini.bin "$WORK/ramdisk/bin/cmini"

echo "== repacking ramdisk.tgz"
if [[ -n ${MKRAMDISK_UNDER_FAKEROOT:-} ]]; then
    # stock owners from the archive (root for system files, setuid kept); only the
    # payload copied from the host belongs to the user "local" (uid/gid 1000)
    chown -R 1000:1000 "$WORK/ramdisk/extras"
    OWNER=(--numeric-owner)
else
    # no fakeroot: everything owner 1000, otherwise root-owned /home/local would be
    # unreadable for the session (but /bin/sudo is not setuid root then)
    OWNER=(--owner=1000 --group=1000)
fi
(
    cd "$WORK/ramdisk"
    find . -mindepth 1 -maxdepth 1 -printf '%P\0' |
        tar --null "${OWNER[@]}" -cf - -T -
) | gzip -9 -n > "$B/ramdisk.tgz"

echo "== writing kernel.mb"
python3 -I - "$WORK/base/kernel" "$B/kernel.mb" <<'PY'
import struct, sys
data = bytearray(open(sys.argv[1], "rb").read())
idx = data.find(struct.pack("<I", 0x1BADB002))
if idx < 0:
    sys.exit("multiboot1 header not found in the kernel")
magic, flags, checksum = struct.unpack_from("<III", data, idx)
flags &= ~0x4                      # bit 2: request graphics mode (VBE)
checksum = (-(magic + flags)) & 0xFFFFFFFF
struct.pack_into("<III", data, idx, magic, flags, checksum)
open(sys.argv[2], "wb").write(bytes(data))
PY

rm -rf "$WORK"
echo "== done: $B/ramdisk.tgz, $B/kernel.mb"
