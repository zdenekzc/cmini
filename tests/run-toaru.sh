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
# /extras of the ramdisk (/extras/cmini-toaru, /extras/cmini-src, /extras/cstencils).
# The tests of cmini and cstencils (the steps of tests/04-toaru.sh) are in the ToaruOS
# terminal:   /extras/cmini-tests.sh   (the results go to /extras/tmp).
#
# Stop the VM:  WORK=_build/vm-run toaruos-tools/toaru-vm.sh stop

set -eu

HERE=$(cd "$(dirname "$0")" && pwd)
TOP=$(cd "$HERE/.." && pwd)
TOOLS=$TOP/toaruos-tools
ISO=$TOP/_import/toaruos-2.3.2.iso
ISO_URL=https://github.com/klange/toaruos/releases/download/v2.3.2/image.iso
ISO_MD5=0043dba93a361b908814a03be2ba7ab2
WORK=$TOP/_build/vm-run
INITRD=$TOP/_build/ramdisk.tgz
KERNEL=$TOP/_build/kernel.mb

MC_BIN=$TOP/_build/mini-commander/cmini-toaru/mc-toaru.bin
TIDE_BIN=$TOP/_build/tiny-ide/cmini-toaru/tiny-ide-toaru.bin

die () { echo "run-toaru: $*" >&2; exit 1; }

mkdir -p "$TOP/_build"

[ -n "${DISPLAY:-}${WAYLAND_DISPLAY:-}" ] || die "no graphical display (DISPLAY is not set)"
command -v qemu-system-x86_64 > /dev/null || die "missing qemu-system-x86_64 (package qemu-system-x86: sudo dnf install qemu-system-x86)"

# 1) ISO
if [ ! -f "$ISO" ]; then
   mkdir -p "$(dirname "$ISO")"
   echo "== downloading ToaruOS 2.3.2 ISO (about 7.5 MB) ..."
   curl -fL --retry 2 --progress-bar -o "$ISO.part" "$ISO_URL" || die "download failed: $ISO_URL"
   [ "$(md5sum "$ISO.part" | cut -d' ' -f1)" = "$ISO_MD5" ] || { rm -f "$ISO.part"; die "md5 does not match"; }
   mv "$ISO.part" "$ISO"
fi

# 2) tools and ramdisk (the ramdisk is rebuilt on every run: its /extras payload changes)
if [ ! -f "$TOP/_build/push/cmini-toaru.tar.gz" ]; then
   echo "== building the tools for ToaruOS (mkcross.sh) ..."
   "$TOOLS/mkcross.sh" > "$TOP/_build/mkcross.log" 2>&1 || die "mkcross.sh failed (see $TOP/_build/mkcross.log)"
fi
if [ ! -f "$MC_BIN" ]; then
   echo "== building mini-commander for ToaruOS (build-toaru-cmini.sh) ..."
   "$TOP/mini-commander/build-toaru-cmini.sh" > "$TOP/_build/mc-toaru.log" 2>&1 || die "mini-commander build failed (see $TOP/_build/mc-toaru.log)"
fi
if [ ! -f "$TIDE_BIN" ]; then
   echo "== building tiny-ide for ToaruOS (build-toaru-cmini.sh) ..."
   "$TOP/tiny-ide/build-toaru-cmini.sh" > "$TOP/_build/tiny-ide-toaru.log" 2>&1 || die "tiny-ide build failed (see $TOP/_build/tiny-ide-toaru.log)"
fi
echo "== building the ramdisk (mkramdisk.sh) ..."
"$TOOLS/mkramdisk.sh" > "$TOP/_build/mkramdisk.log" || die "mkramdisk.sh failed (see $TOP/_build/mkramdisk.log)"

# 3) the VM with the desktop
mkdir -p "$WORK"
echo "== starting ToaruOS in a QEMU window (desktop) ..."
env WORK="$WORK" INITRD="$INITRD" KERNEL="$KERNEL" START=live \
    "$TOOLS/toaru-vm.sh" start
echo "The window shows the desktop after about 15 s. The VM keeps running after this"
echo "script ends."
echo "In the ToaruOS terminal run the cmini and cstencils tests with:"
echo "    /extras/cmini-tests.sh"
echo "Stop the VM with:  WORK=$WORK $TOOLS/toaru-vm.sh stop"
