# toaruos-common.sh - spolecna cast testu ToaruOS (tests/05-toaru.sh):
#   do bezici VM s ToaruOS posle krome prikladu i PREKLADACE a ZDROJE
#   (output.sh ho samostatne nespousti - jmeno nezacina cislem)
#
#   cecko_prepare           krizove prelozene prekladace (toaruos-tools/mkcross.sh):
#                           _build/push/cecko-toaru.tar.gz (cmini.bin, pasm.bin,
#                           cminirt.o, hlavicky, priklady) a zdroje cmini-src.tar.gz;
#                           mkramdisk.sh je vlozi do ramdisku pod /extras (boot bez ISO)
#   cecko_guest <adresar>   v bezicim hostovi ($VM, toaru-vm.sh): zkopiruje balik z
#                           /extras/cmini-toaru do /extras/tmp/cecko, /extras/cmini-src do /extras/tmp/src, a prekladace PRIMO
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

cecko_cstencils ()   # cecko_cstencils <adresar> - cstencils prelozeny cmini v ToaruOS (-L /lib), beh na example/list.t
{
   # vystup se porovnava podle CRC32 (nastroj crc32 v ToaruOS = zlib) a velikosti: konzole
   # v prenosu zahazuje prazdne radky, proto se soubory necitaji celé
   $CECKO_OK || return
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
   cecko_run cst-build "cd /extras/tmp; rm -rf cst; cp -r /extras/cstencils cst; cd cst" "/extras/tmp/cecko/bin/cmini.bin -L /lib -I /extras/tmp/cecko/include -I . -I utils -o /extras/tmp/code-cst unity.cc; ls -l /extras/tmp/code-cst"
   cecko_run cst-run "cd /extras/tmp" "rm -rf out; mkdir -p out; ./code-cst --output-dir /extras/tmp/out /extras/tmp/cst/example/list.t; echo RC=\$?; echo ==HPP==; crc32 out/list.hpp; wc -c out/list.hpp; echo ==CPP==; crc32 out/list.cpp; wc -c out/list.cpp; echo ==END-CST=="
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
   $VM send "mkdir -p /extras/tmp; cd /extras/tmp; rm -rf cecko src; cp -r /extras/cmini-toaru cecko; mkdir src; cp -r /extras/cmini-src src/; echo ==PKG-$RUN=="
   for i in $(seq 1 60); do
      $VM log 5 | grep -qx "==PKG-$RUN==" && break
      sleep 1
   done
   c="cd /extras/tmp/cecko; export PASM_LIBDIR=/lib"
   cecko_run guest-cmini-T  "$c; bin/cmini.bin -T -o /tmp/g1 tests/examples/dama.cc" /tmp/g1
   cecko_run guest-cmini-L  "$c; bin/cmini.bin -L /lib -I include -o /tmp/g2 tests/examples/classes.cc" /tmp/g2
   cecko_run guest-pasm-x   "$c; cp tests/libc64.asm /tmp/g3.asm; bin/pasm.bin -x /tmp/g3.asm" /tmp/g3.bin
   cecko_cstencils $G
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
