#!/bin/bash
# mkramdisk.sh - ToaruOS bez ISO: ramdisk s balikem a kernel pro qemu -kernel/-initrd
#
#   ./mkramdisk.sh        (potrebuje _import/toaruos-2.3.2.iso a _build/push/ z mkcross.sh)
#
# Vystup (toaruos-tools/_build/):
#   ramdisk.tgz   ramdisk.igz ze ISO se vsim, co je v _build/push/ (cecko-toaru.tar.gz,
#                 cmini-src.tar.gz), pod /extras; stejna cesta jako mkiso.sh u /abc/new
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

echo "== copying payload $PUSH into /extras"
mkdir -p "$WORK/ramdisk/extras"
cp -a "$PUSH"/. "$WORK/ramdisk/extras/"

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
