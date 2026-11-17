#!/bin/sh
# 04-toaruos-tcc.sh - _output/toaruos-tcc/: cmini a pasm v ToaruOS (bez gcc, v bezici VM)
#
# CESTA (kroky 1-2 na hostiteli, 3-4 v bezici VM ToaruOS 2.3.2; funkce cecko_* v toaruos-common.sh):
#   1. mkcross.sh (toaruos-tools): cmini.bin, pasm.bin a cminirt.o pro ToaruOS
#      + zdroje cmini; mkramdisk.sh je vlozi do ramdisku (/extras) a VM se boot-ne
#      z ramdisku a kernelu (qemu -kernel/-initrd, bez ISO)
#   2. VM ToaruOS: balik se rozbali v /tmp/cecko a cmini / pasm se prelozi a spusti:
#        guest-cmini-T  cmini -T dama.c (vlastni behova knihovna)
#        guest-cmini-L  cmini -L /lib classes.c (libc ToaruOS)
#        guest-pasm-x   pasm -x libc64.asm (assembler + linker)
#   3. vystupy programu se porovnaji se vzory tests/examples/expected (dama, classes)
#      a s vystupem libc64 slinkovaneho na hostiteli (pasm -c, gcc)
#   4. VM se zastavi; vystupy: _output/toaruos-tcc/{guest-*.out,guest.log}
#
# Vyzaduje qemu-system-x86_64, ISO ToaruOS (_import/) a
# toaru-vm.sh. Bez nich se test jen preskoci (vypise se proc).
# POZOR: jeden NN-*.sh se nesmi spustit dvakrat soucasne (sdileji _output a _build/vystup).

HERE=$(cd "$(dirname "$0")" && pwd)   # common.sh changes the directory, so remember the script's own
. "$HERE/common.sh"
. "$HERE/toaruos-common.sh"
begin toaruos-tcc

TOOLS=$TOP/toaruos-tools
ISO=$TOP/_import/toaruos-2.3.2.iso
D=$V/toaruos-tcc; mkdir -p $D

# ISO ToaruOS 2.3.2: kdyz chybi, stahne se z vydani na GitHubu (curl) a overi se md5
ISO_URL=https://github.com/klange/toaruos/releases/download/v2.3.2/image.iso
ISO_MD5=0043dba93a361b908814a03be2ba7ab2
if [ ! -f "$ISO" ] && command -v curl > /dev/null
then
   mkdir -p "$(dirname "$ISO")"
   echo "  toaruos-tcc: stahuji ISO ToaruOS 2.3.2 (~7.5 MB) z $ISO_URL" >&2
   if curl -fL --retry 2 --progress-bar -o "$ISO.part" "$ISO_URL" && \
      [ "$(md5sum "$ISO.part" | cut -d' ' -f1)" = "$ISO_MD5" ]
   then
      mv "$ISO.part" "$ISO"
   else
      rm -f "$ISO.part"
      echo "  toaruos-tcc: stazeni $ISO_URL selhalo nebo md5 nesouhlasi" >&2
   fi
fi

if ! { command -v qemu-system-x86_64 > /dev/null && [ -f "$ISO" ] && command -v python3 > /dev/null; }
then
   echo "  toaruos-tcc: chybi qemu-system-x86_64 nebo $ISO - preskoceno" >&2
   finish
fi

# ------------------------------------------------------------- hostitel: prekladace a balik
cecko_prepare
$CECKO_OK || { echo "  toaruos-tcc: mkcross.sh selhal ($T/mkcross.log)" >&2; cecko_stop; finish; }

# ------------------------------------------------------------- VM
WORKV=$T/vm; mkdir -p $WORKV
truncate -s 1M $WORKV/share.img
VM="env WORK=$WORKV SHARE=$WORKV/share.img INITRD=$TOOLS/_build/ramdisk.tgz KERNEL=$TOOLS/_build/kernel.mb $TOOLS/toaru-vm.sh"
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
   echo "  toaruos-tcc: VM nenastartovala ($WORKV)" >&2
   cecko_stop
   finish
fi

# ------------------------------------------------------------- v ToaruOS: prelozeni a beh
cecko_guest $D
$VM log 1000000 > $D/guest.log
cp $T/cecko.log $D/guest.log 2> /dev/null
cecko_stop
$VM stop > /dev/null

report toaruos-tcc "cmini a pasm v ToaruOS: programy prelozene v ToaruOS, vystup = vzor"
finish
