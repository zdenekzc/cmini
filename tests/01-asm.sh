#!/bin/sh
# 01-asm.sh - _output/asm/: priklady pro assembler pasm (asm/tests/*.asm):
#   vypis (.lst), ELF (.bin) a jeho vystup s navratovym kodem (.out),
#   objekt OMF (.obj), Turbo Pascal inline (.inl); Windows win64 / win32: PE (.exe,
#   vystup pod wine v .out), win64 i objekt COFF (.obj)

. "$(dirname "$0")/common.sh"
# preklad pasm (make je inkrementalni: nic nedela, kdyz je pasm.bin aktualni)
make -s -C "$TOP/asm" || exit 1
begin asm

D=$V/asm/examples; mkdir -p $D
for f in asm/tests/*.asm
do
   n=$(basename $f .asm)
   [ $n = error ] && continue
   cp $f $D/
   case $n in
      hello32|hello64) opt="-e -l" ;;
      libc64|sse64)    opt="-c -l" ;;
      win64|win32)     opt="-w -l" ;;
      inline)          opt="-i" ;;
      *)               opt="-o -l" ;;
   esac
   (cd $D && $TOP/asm/pasm.bin $opt $n.asm > /dev/null 2>&1)
   [ -f $D/$n.o ] && gcc -no-pie $D/$n.o -o $D/$n.bin 2> /dev/null && rm -f $D/$n.o
   [ -x $D/$n.bin ] && (cd $D && timeout 10 ./$n.bin > $n.out 2>&1; echo "rc=$?" >> $n.out)
   # Windows: PE (.exe) a pro x64 i objekt COFF (.obj); beh pod wine
   [ $n = win64 ] && (cd $D && $TOP/asm/pasm.bin -m $n.asm > /dev/null 2>&1)
   [ -f $D/$n.exe ] && command -v wine > /dev/null &&
      (cd $D && WINEDEBUG=-all timeout 60 wine ./$n.exe > $n.out 2> /dev/null; echo "rc=$?" >> $n.out)
done
report asm "pasm: priklady .asm, vypis (.lst), ELF (.bin), objekt OMF (.obj), inline (.inl), PE (.exe), COFF (.obj)"
finish
