# toaruos-common.sh - spolecna cast testu ToaruOS (tests/04-toaruos-tcc.sh):
#   do bezici VM s ToaruOS posle krome prikladu i PREKLADACE a ZDROJE
#   (output.sh ho samostatne nespousti - jmeno nezacina cislem)
#
#   cecko_prepare           krizove prelozene prekladace (toaruos-tools/mkcross.sh):
#                           _build/push/cecko-toaru.tar.gz (cmini.bin, pasm.bin,
#                           cminirt.o, hlavicky, priklady) a zdroje cmini-src.tar.gz;
#                           mkramdisk.sh je vlozi do ramdisku pod /extras (boot bez ISO)
#   cecko_guest <adresar>   v bezicim hostovi ($VM, toaru-vm.sh): rozbali balik z
#                           /extras do /tmp/cecko, zdroje do /tmp/src, a prekladace PRIMO
#                           V ToaruOS prelozi a spusti:
#                             guest-cmini-T    cmini -T dama.c (vlastni knihovna)
#                             guest-cmini-L    cmini -L /lib classes.c (libc ToaruOS)
#                             guest-pasm-x     pasm -x libc64.asm (assembler + linker)
#                           vystupy <adresar>/guest-*.out, check proti vzorum
#   cecko_stop              (bez HTTP serveru - nic k zastaveni)
#
# Zdroje cmini-src se v hostovi nepreklada (gcc z msk install
# build-essential potrebuje sit) - viz toaruos-tools/guest-cmini.sh.

CECKO_OK=false

cecko_prepare ()
{
   if "$TOP/toaruos-tools/mkcross.sh" > $T/mkcross.log 2>&1 &&
      "$TOP/toaruos-tools/mkramdisk.sh" > $T/mkramdisk.log 2>&1; then
      CECKO_OK=true
      # vzor pro libc64.asm: tentyz program na Linuxu (pasm -c, gcc, glibc)
      mkdir -p $T/cecko-ref
      cp asm/tests/libc64.asm $T/cecko-ref/
      (cd $T/cecko-ref && $TOP/asm/pasm.bin -c libc64.asm > /dev/null 2>&1 &&
         gcc -no-pie libc64.o -o libc64 2> /dev/null && ./libc64 > $T/exp/guest-pasm-x.txt 2>&1)
      cp $EX/expected/dama.txt $T/exp/guest-cmini-T.txt
      cp $EX/expected/classes.txt $T/exp/guest-cmini-L.txt
   else
      echo "  krizovy preklad prekladacu pro ToaruOS selhal (mkcross.sh, $T/mkcross.log)" >&2
   fi
}

cecko_run ()   # cecko_run <jmeno> <prikazy pred behem> <program> - jeden test v hostovi
{
   $VM send "$2; echo ==BEG-$1-$RUN==; $3; echo; echo ==END-$1-$RUN=="
   for i in $(seq 1 120); do
      $VM log 5 | grep -qx "==END-$1-$RUN==" && return
      sleep 1
   done
}

cecko_guest ()
{
   $CECKO_OK || return
   G=$1
   mkdir -p $G
   $VM send "mkdir -p /tmp/src; cd /tmp; tar xzf /extras/cecko-toaru.tar.gz; cp /extras/cmini-src.tar.gz /tmp/src/; echo ==PKG-$RUN=="
   for i in $(seq 1 60); do
      $VM log 5 | grep -qx "==PKG-$RUN==" && break
      sleep 1
   done
   c="cd /tmp/cecko; export PASM_LIBDIR=/lib"
   cecko_run guest-cmini-T  "$c; bin/cmini.bin -T -o /tmp/g1 tests/examples/dama.cc" /tmp/g1
   cecko_run guest-cmini-L  "$c; bin/cmini.bin -L /lib -I include -o /tmp/g2 tests/examples/classes.cc" /tmp/g2
   cecko_run guest-pasm-x   "$c; cp tests/libc64.asm /tmp/g3.asm; bin/pasm.bin -x /tmp/g3.asm" /tmp/g3.bin
   $VM log 1000000 > $T/cecko.log
   for p in guest-cmini-T guest-cmini-L guest-pasm-x; do
      # echo pred koncovou znackou => posledni prazdny radek pryc
      awk -v b="==BEG-$p-$RUN==" -v e="==END-$p-$RUN==" '$0==b{f=1;next} $0==e{f=0} f && !/^root@/' \
         $T/cecko.log | sed '$ { /^$/ d }' > $G/$p.out
      check $G/$p.out $p
   done
}

cecko_stop ()
{
   :
}
