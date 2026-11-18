#!/bin/sh
# run-toaru.sh - ToaruOS 2.3.2 with the CMini and PAsm tools in a QEMU window (desktop).
#
#   sh tests/run-toaru.sh
#
# Nothing is tested here; it only shows the system to the user. The steps:
#   1. the ISO ToaruOS 2.3.2 into _import/ (downloaded if missing, md5 checked),
#   2. the tools for ToaruOS (mkcross.sh) and the ramdisk with them (mkramdisk.sh),
#      built only when they are missing,
#   3. QEMU with a window and the desktop (START=live in toaru-vm.sh).
#
# Needs a graphical display (DISPLAY or WAYLAND_DISPLAY). The tools are in
# /extras of the ramdisk: in the terminal of ToaruOS use /extras/cmini-toaru/
# (the test tests/05-toaru.sh copies it to /tmp/cecko).
#
# Stop the VM:  WORK=toaruos-tools/_build/vm-run toaruos-tools/toaru-vm.sh stop

set -eu

HERE=$(cd "$(dirname "$0")" && pwd)
TOP=$(cd "$HERE/.." && pwd)
TOOLS=$TOP/toaruos-tools
ISO=$TOP/_import/toaruos-2.3.2.iso
ISO_URL=https://github.com/klange/toaruos/releases/download/v2.3.2/image.iso
ISO_MD5=0043dba93a361b908814a03be2ba7ab2
WORK=$TOOLS/_build/vm-run
INITRD=$TOOLS/_build/ramdisk.tgz
KERNEL=$TOOLS/_build/kernel.mb

die () { echo "run-toaru: $*" >&2; exit 1; }

[ -n "${DISPLAY:-}${WAYLAND_DISPLAY:-}" ] || die "no graphical display (DISPLAY is not set)"
command -v qemu-system-x86_64 > /dev/null || die "missing qemu-system-x86_64"

# 1) ISO
if [ ! -f "$ISO" ]; then
   mkdir -p "$(dirname "$ISO")"
   echo "== downloading ToaruOS 2.3.2 ISO (about 7.5 MB) ..."
   curl -fL --retry 2 --progress-bar -o "$ISO.part" "$ISO_URL" || die "download failed: $ISO_URL"
   [ "$(md5sum "$ISO.part" | cut -d' ' -f1)" = "$ISO_MD5" ] || { rm -f "$ISO.part"; die "md5 does not match"; }
   mv "$ISO.part" "$ISO"
fi

# 2) tools and ramdisk
if [ ! -f "$INITRD" ] || [ ! -f "$KERNEL" ]; then
   echo "== building the tools for ToaruOS (mkcross.sh) ..."
   "$TOOLS/mkcross.sh" > "$TOOLS/_build/mkcross.log" 2>&1 || die "mkcross.sh failed (see $TOOLS/_build/mkcross.log)"
   echo "== building the ramdisk (mkramdisk.sh) ..."
   "$TOOLS/mkramdisk.sh" > "$TOOLS/_build/mkramdisk.log" 2>&1 || die "mkramdisk.sh failed (see $TOOLS/_build/mkramdisk.log)"
fi

# 3) the VM with the desktop
mkdir -p "$WORK"
[ -f "$WORK/share.img" ] || truncate -s 1M "$WORK/share.img"
echo "== starting ToaruOS in a QEMU window (desktop) ..."
env WORK="$WORK" SHARE="$WORK/share.img" INITRD="$INITRD" KERNEL="$KERNEL" START=live \
    "$TOOLS/toaru-vm.sh" start
echo "The window shows the desktop after about 15 s. The VM keeps running after this"
echo "script ends. Stop it with:  WORK=$WORK $TOOLS/toaru-vm.sh stop"
