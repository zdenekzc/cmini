#!/bin/sh
# 04-toaru.sh - _output/toaruos/: cmini (varianta pasm) v ToaruOS x86_64 (bez gcc, v bezici VM)
#
# CESTA (kroky 1-2 na hostiteli, 3-4 v bezici VM ToaruOS 2.3.2; funkce cmini_* v toaruos-common.sh):
#   1. na hostiteli (_build/ v hlavnim adresari):
#      mkcross.sh        cmini.bin a pasm.bin pro ToaruOS (staticky, ToaruOS libc), zdroje cmini,
#                        CROSS-PRELOZENE PRIKLADY (_build/cross/cmini/examples-bin/)
#      mini-commander/build-toaru-cmini.sh, tiny-ide/build-toaru-cmini.sh
#                        mc-toaru.bin a tiny-ide-toaru.bin (cross, cmini -L toaru-lib, pasm -k)
#      mkramdisk.sh      vse se vlozi do ramdisku pod /extras, VM se boot-ne z ramdisku a kernelu
#                        (qemu -kernel/-initrd, bez ISO)
#   2. v bezici VM ToaruOS:
#        guest-cmini-T / -L / -L-<p>   prekladani cmini uvnitr ToaruOS (/extras/tmp)
#        guest-pasm-x                  pasm -x libc64.asm (assembler + linker)
#        cst-build / cst-run           cstencils (unity.cc) prelozeny v ToaruOS
#        cross-<p>                     programy cross-prelozene na hostiteli (/extras/cmini-toaru/examples-bin)
#        cross-tiny-ide-v, cross-mc-v  -v (verze) cross-prelozenych tiny-ide a mc
#        build-new-tiny-ide, build-new-mc  /extras/build-new-*.sh: tiny-ide a mc prelozene cmini UVNITR ToaruOS
#        guest-tiny-ide-v, guest-mc-v  -v programu prelozenych uvnitr ToaruOS
#   3. vystupy se porovnaji se vzory tests/examples/expected (dama, classes, ...), u -v s
#      ocekavanym radkem, u cstencils CRC32 a velikosti (g++ build na hostiteli).
#   4. VM se zastavi (s oknem, DISPLAY nastaveno, zustava bezet); vystupy:
#      _output/toaruos/{guest-*.out,cross-*.out,build-new-*.out,cst-*.out,guest.log}
#
# Translate (cmini-translate.bin -C) se tu netestuje - patri k tests/05-cmini-translate.sh;
# cmini.bin je jen prekladac s pasm backendem.
# cout se neprekladá krizove: ToaruOS libc nema dprintf (cerr).
# Pro ToaruOS ARM64 je tests/06-toaru-arm64.sh.
# Vyzaduje qemu-system-x86_64, ISO ToaruOS (_import/) a
# toaru-vm.sh. Bez nich se test jen preskoci (vypise se proc).
# POZOR: jeden NN-*.sh se nesmi spustit dvakrat soucasne (sdileji _output a _build/vystup).

HERE=$(cd "$(dirname "$0")" && pwd)   # common.sh changes the directory, so remember the script's own
. "$HERE/common.sh"
. "$HERE/toaruos-common.sh"
begin toaruos

TOOLS=$TOP/toaruos-tools
ISO=$TOP/_import/toaruos-2.3.2.iso
D=$V/toaruos; mkdir -p $D

# ISO ToaruOS 2.3.2: kdyz chybi, stahne se z vydani na GitHubu (curl) a overi se md5
ISO_URL=https://github.com/klange/toaruos/releases/download/v2.3.2/image.iso
ISO_MD5=0043dba93a361b908814a03be2ba7ab2
if [ ! -f "$ISO" ] && command -v curl > /dev/null
then
   mkdir -p "$(dirname "$ISO")"
   echo "  toaruos: stahuji ISO ToaruOS 2.3.2 (~7.5 MB) z $ISO_URL" >&2
   if curl -fL --retry 2 --progress-bar -o "$ISO.part" "$ISO_URL" && \
      [ "$(md5sum "$ISO.part" | cut -d' ' -f1)" = "$ISO_MD5" ]
   then
      mv "$ISO.part" "$ISO"
   else
      rm -f "$ISO.part"
      echo "  toaruos: stazeni $ISO_URL selhalo nebo md5 nesouhlasi" >&2
   fi
fi

if ! { command -v qemu-system-x86_64 > /dev/null && [ -f "$ISO" ] && command -v python3 > /dev/null; }
then
   echo "  toaruos: chybi qemu-system-x86_64 nebo $ISO - preskoceno" >&2
   finish
fi

# ------------------------------------------------------------- hostitel: prekladace a balik
cmini_prepare
$CMINI_OK || { echo "  toaruos: mkcross.sh selhal ($T/mkcross.log)" >&2; cmini_stop; finish; }

# ------------------------------------------------------------- VM
# with a window (manual run) the VM stays up after the test: its directory is kept
KEEP_VM=0
if [ -n "${DISPLAY:-}${WAYLAND_DISPLAY:-}" ] && [ "${TOARU_GUI:-1}" != 0 ]; then KEEP_VM=1; fi
if [ "$KEEP_VM" = 1 ]; then WORKV=$TOP/_build/vm-keep; else WORKV=$T/vm; fi
mkdir -p $WORKV
VM="env WORK=$WORKV INITRD=$TOP/_build/ramdisk.tgz KERNEL=$TOP/_build/kernel.mb $TOOLS/toaru-vm.sh"
up=false
for attempt in 1 2 3; do
   $VM start > /dev/null
   for i in $(seq 1 90); do
      $VM log 3 2> /dev/null | grep -q 'root@livecd' && { up=true; break; }
      sleep 1
   done
   $up && break
   $VM stop > /dev/null
done
if ! $up
then
   echo "  toaruos: VM nenastartovala ($WORKV)" >&2
   cmini_stop
   finish
fi

# ------------------------------------------------------------- v ToaruOS: prelozeni a beh
cmini_guest $D
$VM log 1000000 > $D/guest.log
cp $T/cmini.log $D/guest.log 2> /dev/null
cmini_stop
# Manual run with a window (DISPLAY set, TOARU_GUI not 0): QEMU stays up so that the
# ToaruOS desktop can be used. Its working directory is kept (not under $T, which
# finish removes); stop it with:  WORK=$WORKV $TOOLS/toaru-vm.sh stop
if [ "$KEEP_VM" = 1 ]; then
   echo "  toaruos: QEMU zustava bezet (okno). Zastaveni: WORK=$WORKV $TOOLS/toaru-vm.sh stop" >&2
else
   $VM stop > /dev/null
fi

report toaruos "cmini a pasm v ToaruOS: programy prelozene v ToaruOS, vystup = vzor"
finish
