#!/bin/sh
# 06-toaru-arm64.sh - _output/toaruos-arm64/: cmini (varianta translate) v ToaruOS aarch64 v QEMU
#
# CESTA (hostitel: toaruos-tools/mktcc-aarch64.sh, mkarm64.sh; VM: toaruos-tools/arm64-common.sh):
#   1. release ToaruOS 2.3.2 aarch64 (_import/download/aarch64-virt.zip, stahne se)
#   2. cross TinyCC pro aarch64 (_build/tcc-aarch64), guest tcc (/extras/tcc/bin/tcc)
#   3. cmini -C (translate) tests/examples/*.cc => C; cminirt.o pro aarch64
#      => ramdisk _build/ramdisk-a64.tgz (/extras/examples, /extras/build-examples.sh)
#   4. QEMU bez okna, seriova linka: VM se prelozi a spusti /extras/build-examples.sh
#      (tcc v ToaruOS prelozi kazdy .c s cminirt.o a program se spusti)
#   5. vystup kazdeho programu se porovna se vzorem tests/examples/expected (quicksort,
#      heapsort jen posledni radek O.K.); navratovy kod musi byt 0
#
# tiny-ide a mini-commander zatim netestujeme (az po prvnim kroku).
# Vystupy: _output/toaruos-arm64/{<program>.out,serial.txt,qemu.log}
# Vyzaduje qemu-system-aarch64, unzip, fakeroot, curl (stazeni zipu); bez nich se preskoci.

. "$(dirname "$0")/common.sh"
A64_TOP=$TOP
export A64_TOP
. "$TOP/toaruos-tools/arm64-common.sh"
begin toaruos-arm64
# programy v payloadu (toaruos-tools/mkarm64.sh: PROGRAMS); ostatni testy tests/examples se tu netestuji
ARM_PROGRAMS="dama classes avl btree jezdec veze heapsort quicksort strings dyncast"

for c in qemu-system-aarch64 unzip fakeroot gzip python3
do
   command -v $c > /dev/null || { echo "  toaruos-arm64: chybi $c - preskoceno" >&2; finish; }
done
# cmini -C (translate) a zkontrolovane vzory; navratovy kod programu se porovna s RC=0
for p in $ARM_PROGRAMS; do echo "RC=0" > $T/exp/rc-$p.txt; done
echo "RC=7" > $T/exp/rc-rc_check.txt

if ! a64_prepare
then
   echo "  toaruos-arm64: priprava (zip, TinyCC, payload) selhala" >&2
   FAILED=1; finish
fi

a64_start 0 $T/qemu.log
# the guest is up when the login shell prompt appears (as in /abc/tiny/tests/run-arm64.sh)
if ! a64_wait_for 'local@livecd' 300
then
   echo "  toaruos-arm64: VM nenastartovala (prompt local@livecd, log: $T/qemu.log)" >&2
   a64_stop; FAILED=1; finish
fi
up=false
for attempt in 1 2 3
do
   a64_send "echo ==READY=="
   if a64_wait "==READY==" 60; then up=true; break; fi
done
if ! $up
then
   echo "  toaruos-arm64: shell neodpovida (log: $T/qemu.log)" >&2
   a64_stop; FAILED=1; finish
fi

a64_send "sh /extras/build-examples.sh"
last=rc_check
a64_wait "==END-$last==" 900 || echo "  toaruos-arm64: run_examples.sh neskoncil (cast $last)" >&2
# tiny-ide a mini-commander (preloženy translate cestou v hostiteli): -v musi odpovedet
for app in tide mc
do
   case $app in
      tide) bin=/extras/tiny-ide/tiny-ide-toaru.bin ;;
      mc)   bin=/extras/mini-commander/mc-toaru.bin ;;
   esac
   a64_send "echo ==BEG-$app-v=="
   a64_send "$bin -v"
   a64_send "echo RC=\$?"
   a64_send "echo ==END-$app-v=="
   a64_wait "==END-$app-v==" 120 || echo "  toaruos-arm64: $app -v neodpovedel" >&2
done
a64_log 200000 > $V/toaruos-arm64/serial.txt 2> /dev/null || true
a64_stop

for p in $ARM_PROGRAMS
do
   awk -v b="==BEG-$p==" -v e="==END-$p==" '$0==b{f=1;next} $0==e{f=0} f' $V/toaruos-arm64/serial.txt \
      | grep -v '^RC=' > $V/toaruos-arm64/$p.out
   rc=$(awk -v b="==BEG-$p==" -v e="==END-$p==" '$0==b{f=1;next} $0==e{f=0} f && /^RC=/' $V/toaruos-arm64/serial.txt | head -1)
   echo "$rc" > $T/rc-$p.txt
   check $V/toaruos-arm64/$p.out $p
   if [ "$rc" != "RC=0" ]
   then echo "$p" >> $T/bad; echo "  $p: navratovy kod '$rc' (ocekavano RC=0)" >&2
   fi
done
# -v: tiny-ide a mini-commander se spusti a vypisi verzi (navratovy kod jen zaznamenavame)
echo "Tiny IDE 1.0" > $T/exp/app-tide-v.txt
echo "Version 2.0" > $T/exp/app-mc-v.txt
for app in tide mc
do
   # the terminal redraws the prompt and the typed command: keep only the version line itself
   awk -v b="==BEG-$app-v==" -v e="==END-$app-v==" '$0==b{f=1;next} $0==e{f=0} f' $V/toaruos-arm64/serial.txt \
      | grep -o 'Tiny IDE 1\.0\|Version 2\.0' | head -1 > $V/toaruos-arm64/$app-v.out
   check $V/toaruos-arm64/$app-v.out app-$app-v
done
# exit-code path: rc_check vraci 7; RC=7 na serial lince potvrdi, ze navratovy kod dojde do shellu
rc=$(awk '$0=="==BEG-rc_check=="{f=1;next} $0=="==END-rc_check=="{f=0} f && /^RC=/' $V/toaruos-arm64/serial.txt | head -1)
echo "$rc" > $T/rc-rc_check.txt
if [ "$rc" = "RC=7" ]; then echo "rc_check" >> $T/ok; else echo "rc_check" >> $T/bad; echo "  rc_check: navratovy kod '$rc' (ocekavano RC=7)" >&2; fi
report toaruos-arm64 "cmini (translate) v ToaruOS aarch64: programy z C prelozene tcc v ToaruOS, vystup = vzor"
finish
