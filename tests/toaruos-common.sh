# toaruos-common.sh - spolecna cast testu ToaruOS (tests/05-toaru.sh):
#   do bezici VM s ToaruOS posle krome prikladu i PREKLADACE a ZDROJE
#   (output.sh ho samostatne nespousti - jmeno nezacina cislem)
#
#   cmini_prepare           krizove prelozene prekladace (toaruos-tools/mkcross.sh):
#                           _build/push/cmini-toaru.tar.gz (cmini.bin, pasm.bin,
#                           cminirt.o, hlavicky, priklady) a zdroje cmini-src.tar.gz;
#                           mkramdisk.sh je vlozi do ramdisku pod /extras (boot bez ISO)
#   cmini_guest <adresar>   v bezicim hostovi ($VM, toaru-vm.sh): zkopiruje balik z
#                           /extras/cmini-toaru do /extras/tmp/cmini, /extras/cmini-src do /extras/tmp/src, a prekladace PRIMO
#                           V ToaruOS prelozi a spusti:
#                             guest-cmini-T    cmini -T dama.c (vlastni knihovna)
#                             guest-cmini-L    cmini -L /lib classes.c (libc ToaruOS)
#                             guest-pasm-x     pasm -x libc64.asm (assembler + linker)
#                           vystupy <adresar>/guest-*.out, check proti vzorum
#   cmini_stop              (bez HTTP serveru - nic k zastaveni)
#
# Zdroje cmini-src se v hostovi nepreklada (gcc z msk install
# build-essential potrebuje sit) - viz toaruos-tools/guest-cmini.sh.

CMINI_OK=false
# programy cross-prelozene na hostiteli (mkcross.sh => /extras/cmini-toaru/examples-bin); cout ne:
# ToaruOS libc nema dprintf (cerr)
CROSS_PROGRAMS="dama classes avl btree jezdec veze heapsort quicksort"
# /extras/examples + /extras/build-examples.sh (toaruos-tools/mkexamples.sh): stejne programy jako tests/06
EX_PROGRAMS="dama classes avl btree jezdec veze heapsort quicksort strings dyncast rc_check"
CROSS_NAMES="cross-dama cross-classes cross-avl cross-btree cross-jezdec cross-veze cross-heapsort cross-quicksort"

cmini_prepare ()
{
   # tiny-ide a mini-commander: cmini (pasm) pro ToaruOS, do _build/<nastroj>/cmini-toaru/
   if "$TOP/toaruos-tools/mkcross.sh" > $T/mkcross.log 2>&1 &&
      "$TOP/mini-commander/build-toaru-cmini.sh" > $T/mc-toaru.log 2>&1 &&
      "$TOP/tiny-ide/build-toaru-cmini.sh" > $T/tide-toaru.log 2>&1 &&
      "$TOP/toaruos-tools/mkramdisk.sh" > $T/mkramdisk.log 2>&1; then
      CMINI_OK=true
      # vzor pro libc64.asm: tentyz program na Linuxu (pasm -c, gcc, glibc)
      mkdir -p $T/cmini-ref
      cp asm/tests/libc64.asm $T/cmini-ref/
      (cd $T/cmini-ref && $TOP/asm/pasm.bin -c libc64.asm > /dev/null 2>&1 &&
         gcc -no-pie libc64.o -o libc64 2> /dev/null && ./libc64 > $T/exp/guest-pasm-x.txt 2>&1)
      cp $EX/expected/dama.txt $T/exp/guest-cmini-T.txt
      cp $EX/expected/classes.txt $T/exp/guest-cmini-L.txt
      # dalsi prikazy: cmini -L (libc ToaruOS) - vzor tests/examples/expected/<p>.txt, heapsort a quicksort jen O.K.
      for p in avl btree jezdec veze; do cp $EX/expected/$p.txt $T/exp/guest-cmini-L-$p.txt; done
      for p in $CROSS_PROGRAMS; do cp $EX/expected/$p.txt $T/exp/cross-$p.txt 2> /dev/null; done
      for p in $EX_PROGRAMS; do [ -f $EX/expected/$p.txt ] && cp $EX/expected/$p.txt $T/exp/ex-$p.txt; done
      : > $T/exp/ex-rc_check.txt      # rc_check nema vystup; jeho navratovy kod se kontroluje zvlast
      # -v: tiny-ide a mini-commander (version, ocekavany radek)
      echo "Tiny IDE 1.0" > $T/exp/cross-tiny-ide-v.txt; cp $T/exp/cross-tiny-ide-v.txt $T/exp/guest-tiny-ide-v.txt
      echo "Version 2.0" > $T/exp/cross-mc-v.txt; cp $T/exp/cross-mc-v.txt $T/exp/guest-mc-v.txt
   else
      echo "  krizovy preklad prekladacu pro ToaruOS selhal (mkcross.sh, $T/mkcross.log)" >&2
   fi
}

cstencils_ref ()   # cstencils_ref <adresar> - referencni vystup na hostiteli (g++): <adresar>/out/list.hpp, list.cpp
{
   local d=$1 u objs=""
   mkdir -p $d/obj $d/out
   for u in utils/inf utils/str utils/conv utils/envio utils/textio elem fact field flexible stencil code main-code
   do
      b=$(echo $u | tr / _)
      g++ -std=c++17 -w -I $TOP/cstencils -I $TOP/cstencils/utils -c $TOP/cstencils/$u.cc -o $d/obj/$b.o || return 1
      objs="$objs $d/obj/$b.o"
   done
   g++ $objs -o $d/code || return 1
   (cd $d/out && $d/code --output-dir $d/out $TOP/cstencils/example/list.t > /dev/null 2>&1)
}

cmini_cstencils ()   # cmini_cstencils <adresar> - cstencils prelozeny cmini v ToaruOS (-L /lib), beh na example/list.t
{
   # vystup se porovnava podle CRC32 (nastroj crc32 v ToaruOS = zlib) a velikosti: konzole
   # v prenosu zahazuje prazdne radky, proto se soubory necitaji celé
   $CMINI_OK || return
   G=$1
   if cstencils_ref $T/cst-ref
   then
      for f in list.hpp list.cpp
      do
         python3 -c "import sys,zlib; d=open(sys.argv[1],'rb').read(); print('%08x %d' % (zlib.crc32(d) & 0xffffffff, len(d)))" \
            $T/cst-ref/out/$f > $T/exp/cst-$(echo $f | tr . -).txt
      done
   fi
   echo "RC=0" > $T/exp/cst-rc.txt
   # cmini v ToaruOS: unity.cc (jeden zdroj = jeden program), libc ToaruOS z /lib, pasm linker
   cmini_run cst-build "cd /extras/tmp; rm -rf cst; cp -r /extras/cstencils cst; cd cst" "/extras/tmp/cmini/bin/cmini.bin -L /lib -I /extras/tmp/cmini/include -I . -I utils -o /extras/tmp/code-cst unity.cc; ls -l /extras/tmp/code-cst"
   cmini_run cst-run "cd /extras/tmp" "rm -rf out; mkdir -p out; ./code-cst --output-dir /extras/tmp/out /extras/tmp/cst/example/list.t; echo RC=\$?; echo ==HPP==; crc32 out/list.hpp; wc -c out/list.hpp; echo ==CPP==; crc32 out/list.cpp; wc -c out/list.cpp; echo ==END-CST=="
   $VM log 1000000 > $T/cst-guest.log
   awk -v b="==BEG-cst-run-$RUN==" -v e="==END-cst-run-$RUN==" '$0==b{f=1;next} $0==e{f=0} f && !/^root@/' \
      $T/cst-guest.log | tr -d '\r' > $G/cst-run.txt
   grep -a '^RC=' $G/cst-run.txt | head -1 > $G/cst-rc.out
   sed -n '/^==HPP==$/,/^==CPP==$/p' $G/cst-run.txt | sed '1d;$d' | awk '{print $1}' | paste -sd' ' > $G/cst-list-hpp.out
   sed -n '/^==CPP==$/,/^==END-CST==$/p' $G/cst-run.txt | sed '1d;$d' | awk '{print $1}' | paste -sd' ' > $G/cst-list-cpp.out
   check $G/cst-rc.out cst-rc
   check $G/cst-list-hpp.out cst-list-hpp
   check $G/cst-list-cpp.out cst-list-cpp
}

cmini_run ()   # cmini_run <jmeno> <prikazy pred behem> <program> - jeden test v hostovi
{
   $VM send "$2; echo ==BEG-$1-$RUN==; $3; echo; echo ==END-$1-$RUN=="
   for i in $(seq 1 ${RUN_WAIT:-120}); do
      $VM log 5 | grep -qx "==END-$1-$RUN==" && return
      sleep 1
   done
}

cmini_guest ()
{
   $CMINI_OK || return
   G=$1
   mkdir -p $G
   $VM send "mkdir -p /extras/tmp; cd /extras/tmp; rm -rf cmini src; cp -r /extras/cmini-toaru cmini; mkdir src; cp -r /extras/cmini-src src/; echo ==PKG-$RUN=="
   for i in $(seq 1 60); do
      $VM log 5 | grep -qx "==PKG-$RUN==" && break
      sleep 1
   done
   c="cd /extras/tmp/cmini; export PASM_LIBDIR=/lib"
   cmini_run guest-cmini-T  "$c; bin/cmini.bin -T -o /tmp/g1 tests/examples/dama.cc" /tmp/g1
   cmini_run guest-cmini-L  "$c; bin/cmini.bin -L /lib -I include -o /tmp/g2 tests/examples/classes.cc" /tmp/g2
   cmini_run guest-pasm-x   "$c; cp tests/libc64.asm /tmp/g3.asm; bin/pasm.bin -x /tmp/g3.asm" /tmp/g3.bin
   cmini_run guest-cmini-L-avl "$c; bin/cmini.bin -L /lib -I include -o /tmp/g-avl tests/examples/avl.cc" /tmp/g-avl
   cmini_run guest-cmini-L-btree "$c; bin/cmini.bin -L /lib -I include -o /tmp/g-btree tests/examples/btree.cc" /tmp/g-btree
   cmini_run guest-cmini-L-jezdec "$c; bin/cmini.bin -L /lib -I include -o /tmp/g-jezdec tests/examples/jezdec.cc" /tmp/g-jezdec
   cmini_run guest-cmini-L-veze "$c; bin/cmini.bin -L /lib -I include -o /tmp/g-veze tests/examples/veze.cc" /tmp/g-veze
   cmini_run guest-cmini-L-heapsort "$c; bin/cmini.bin -L /lib -I include -o /tmp/g-heapsort tests/examples/heapsort.cc" /tmp/g-heapsort
   cmini_run guest-cmini-L-quicksort "$c; bin/cmini.bin -L /lib -I include -o /tmp/g-quicksort tests/examples/quicksort.cc" /tmp/g-quicksort
   # cross-compiled examples (mkcross.sh, host cmini -L): run them as they are
   for p in $CROSS_PROGRAMS; do
      cmini_run cross-$p "cd /extras/cmini-toaru/examples-bin" "./$p"
   done
   # tiny-ide a mini-commander cross-compiled on the host: -v (version) must answer
   cmini_run cross-tiny-ide-v "cd /extras" "/extras/tiny-ide/tiny-ide-toaru.bin -v"
   cmini_run cross-mc-v "cd /extras" "/extras/mini-commander/mc-toaru.bin -v"
   # tiny-ide a mini-commander zkompilovane cmini UVNITR ToaruOS (guest skripty build-new-*.sh)
   RUN_WAIT=1200 cmini_run build-new-tiny-ide "cd /extras" "sh /extras/build-new-tiny-ide.sh"
   cmini_run guest-tiny-ide-v "cd /extras" "/extras/new-tiny-ide/tiny-ide-toaru.bin -v"
   RUN_WAIT=1200 cmini_run build-new-mc "cd /extras" "sh /extras/build-new-mc.sh"
   cmini_run guest-mc-v "cd /extras" "/extras/new-mc/mc-toaru.bin -v"
   # /extras/build-examples.sh: cmini v ToaruOS prelozi /extras/examples a spusti je (vystup ==BEG-p== .. ==END-p==)
   RUN_WAIT=1800 cmini_run build-examples "cd /extras" "sh /extras/build-examples.sh"
   cmini_cstencils $G
   $VM log 1000000 > $T/cmini.log
   for p in $EX_PROGRAMS
   do
      awk -v b="==BEG-$p==" -v e="==END-$p==" '$0==b{f=1;next} $0==e{f=0} f' $T/cmini.log | grep -v '^RC=' > $G/ex-$p.out
      rc=$(awk -v b="==BEG-$p==" -v e="==END-$p==" '$0==b{f=1;next} $0==e{f=0} f && /^RC=/' $T/cmini.log | head -1)
      want=RC=0; [ $p = rc_check ] && want=RC=7
      if [ "$rc" = "$want" ]; then check $G/ex-$p.out ex-$p; else echo "ex-$p" >> $T/bad; echo "  ex-$p: navratovy kod '$rc' (ocekavano $want)" >&2; fi
   done
   for p in $CROSS_NAMES cross-tiny-ide-v cross-mc-v guest-tiny-ide-v guest-mc-v build-new-tiny-ide build-new-mc; do
      awk -v b="==BEG-$p-$RUN==" -v e="==END-$p-$RUN==" '$0==b{f=1;next} $0==e{f=0} f && !/^root@/' \
         $T/cmini.log | sed '$ { /^$/ d }' > $G/$p.out
   done
   # vystupy prekladu (build-new-*) nemaji vzor: uspech je guest-*-v (program se spustil a vypsal verzi)
   for p in $CROSS_NAMES cross-tiny-ide-v cross-mc-v guest-tiny-ide-v guest-mc-v; do check $G/$p.out $p; done
   for p in guest-cmini-T guest-cmini-L guest-pasm-x guest-cmini-L-avl guest-cmini-L-btree guest-cmini-L-jezdec guest-cmini-L-veze guest-cmini-L-heapsort guest-cmini-L-quicksort; do
      # echo pred koncovou znackou => posledni prazdny radek pryc
      awk -v b="==BEG-$p-$RUN==" -v e="==END-$p-$RUN==" '$0==b{f=1;next} $0==e{f=0} f && !/^root@/' \
         $T/cmini.log | sed '$ { /^$/ d }' > $G/$p.out
      check $G/$p.out $p
   done
}

cmini_stop ()
{
   :
}
