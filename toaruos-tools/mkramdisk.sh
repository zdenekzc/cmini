#!/bin/bash
# mkramdisk.sh - ToaruOS bez ISO: ramdisk s balikem a kernel pro qemu -kernel/-initrd
#
#   ./mkramdisk.sh        (potrebuje _import/toaruos-2.3.2.iso a _build/push/ z mkcross.sh)
#
# Vystup (toaruos-tools/_build/):
#   ramdisk.tgz   ramdisk.igz ze ISO se vsim pod /extras; z _build/push/ se pridaji
#                 rozbalene adresare /extras/cmini-toaru (z cecko-toaru.tar.gz) a
#                 /extras/cmini-src (z cmini-src.tar.gz); archivy se nekopiruji
#                 a /extras/cstencils (zdroje generatoru + unity.cc pro cmini),
#                 /extras/tests/cmini-tests.sh (kroky testu pro terminal ToaruOS)
#   kernel.mb     kernel z ISO, v multiboot hlavicce vypnuty pozadavek na VBE (QEMU
#                 multiboot loader by jinak odmitl "multiboot knows VBE")
#
# Spusteni (toaru-vm.sh): INITRD=_build/ramdisk.tgz KERNEL=_build/kernel.mb ./toaru-vm.sh start
set -euo pipefail

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
gzip -dc "$WORK/base/ramdisk.igz" | tar -xf - -C "$WORK/ramdisk"

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
# /extras/cmini-toaru: programs (bin/, lib/, include/) from cecko-toaru.tar.gz (top dir cecko/ dropped)
mkdir -p "$WORK/ramdisk/extras/cmini-toaru"
tar -xzf "$PUSH/cecko-toaru.tar.gz" --strip-components=1 -C "$WORK/ramdisk/extras/cmini-toaru"
# /extras/cmini-src: compiler sources from cmini-src.tar.gz (top dir cmini-src/ dropped)
mkdir -p "$WORK/ramdisk/extras/cmini-src"
tar -xzf "$PUSH/cmini-src.tar.gz" --strip-components=1 -C "$WORK/ramdisk/extras/cmini-src"

echo "== repacking ramdisk.tgz"
# owner 1000 (user local), the same as mkiso.sh: root-owned /home/local would be unreadable
(
    cd "$WORK/ramdisk"
    find . -mindepth 1 -maxdepth 1 -printf '%P\0' |
        tar --null --owner=1000 --group=1000 -cf - -T -
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
