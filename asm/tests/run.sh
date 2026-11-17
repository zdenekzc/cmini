#!/bin/sh
# Testy assembleru pasm (verze v C) - bez ulozenych vzoru, reference se vytvori pri kazdem spusteni:
#   1. 16 a 32 bitu: vsechny vystupy (.obj OMF, .lst, .inl, ELF i386, hlaseni a navratovy kod,
#      beh hello32) bajtove shodne s puvodni verzi v Pascalu
#      (../../../pascal-text/asm/pasm.pas, prelozi se fpc -Mtp; nebo PASM_REF=program)
#   2. x86-64 (rozsireni verze v C): hello64 (-e), libc64 a sse64 (-c, gcc) - vystup
#      a navratovy kod programu shodne s temiz programy v syntaxi nasm (tests/nasm/*.nasm)
#   3. Windows: win64 -m (COFF) + lld-link == nasm -f win64 + lld-link, importy win64 -w (PE32+),
#      win32 -w (PE32); beh pod wine, je-li nainstalovano
#   kodovani jednotlivych instrukci proti nasm: make test-nasm, make test-nasm64
#
#   sh tests/run.sh [PASM]      PASM implicitne ../pasm.bin (relativne k tomuto adresari)
#   ASM_OUTPUT=adresar          vystupy (implicitne ../../_build/asm/tests): pasm/, ref/, nasm/
[ -n "$1" ] && { PASM=$(realpath "$1") || exit 2; }
[ -n "$PASM_REF" ] && { PASM_REF=$(realpath "$PASM_REF") || exit 2; }
[ -n "$ASM_OUTPUT" ] && { mkdir -p "$ASM_OUTPUT" && ASM_OUTPUT=$(realpath "$ASM_OUTPUT") || exit 2; }
cd "$(dirname "$0")" || exit 2
TESTS=$(pwd)
PASM=${PASM:-$(realpath ../pasm.bin)}
OUT=${ASM_OUTPUT:-../../_build/asm/tests}
rm -rf "$OUT"
mkdir -p "$OUT" || exit 2
OUT=$(realpath "$OUT")
fail=0

result ()   # result <jmeno> <adresar1> <adresar2> - shoda vsech souboru
{
   if diff -r "$2" "$3" > /dev/null; then
      echo "ok    $1"
   else
      echo "FAIL  $1"
      diff -rq "$2" "$3" | head -5
      fail=1
   fi
}

# pasm <program> <adresar> <jmeno> <prepinace...> - preklad, hlaseni a navratovy kod (.out)
pasm ()
{
   prog=$1; dir=$2; name=$3; shift 3
   mkdir -p $dir
   cp $TESTS/$name.asm $dir/
   (cd $dir && $prog "$@" $name.asm > $name.out 2>&1; echo "rc=$?" >> $name.out)
   rm $dir/$name.asm
}

# ---------------------------------------------------------------------- 1. proti verzi v Pascalu
if [ -z "$PASM_REF" ]; then
   SRC=$(realpath ../../../pascal-text/asm 2> /dev/null)
   if [ -f "$SRC/pasm.pas" ] && command -v fpc > /dev/null; then
      mkdir -p $OUT/fpc
      if fpc -Mtp -FE$OUT/fpc -FU$OUT/fpc $SRC/pasm.pas > $OUT/fpc/log.txt 2>&1; then
         PASM_REF=$OUT/fpc/pasm
      else
         echo "FAIL  preklad verze v Pascalu (viz $OUT/fpc/log.txt)"; fail=1
      fi
   fi
fi

if [ -z "$PASM_REF" ]; then
   echo "SKIP  16 a 32 bitu: chybi verze v Pascalu (../../../pascal-text/asm a fpc, nebo PASM_REF=)"
else
   for t in "basic16 -o -l" "hello16 -o -l" "sum32 -o -l" "misc16 -o -l" \
            "fpu -o -l" "inline -i" "hello32 -e -l" "error -o -l"; do
      set -- $t
      name=$1; shift
      pasm $PASM     $OUT/pasm/$name $name "$@"
      pasm $PASM_REF $OUT/ref/$name  $name "$@"
      # spustitelny ELF i386: i vystup programu
      for d in $OUT/pasm/$name $OUT/ref/$name; do
         [ -f $d/$name.bin ] && (cd $d && ./$name.bin > $name.run 2>&1; echo "rc=$?" >> $name.run)
      done
      result "$name ($*) == pasm v Pascalu" $OUT/ref/$name $OUT/pasm/$name
   done
fi

# ---------------------------------------------------------------------- 2. x86-64 proti nasm
if ! command -v nasm > /dev/null; then
   echo "SKIP  x86-64: chybi nasm"
else
   for t in "hello64 -e -l" "libc64 -c -l" "sse64 -c -l"; do
      set -- $t
      name=$1; shift
      P=$OUT/pasm/$name; N=$OUT/nasm/$name
      pasm $PASM $P $name "$@"
      mkdir -p $N
      if nasm -f elf64 nasm/$name.nasm -o $N/$name.o > $N/nasm.log 2>&1; then
         if [ "$1" = -e ]; then
            ld $N/$name.o -o $N/$name.bin > $N/nasm.log 2>&1
         else
            gcc -no-pie $N/$name.o -o $N/$name.bin > $N/nasm.log 2>&1
            gcc -no-pie $P/$name.o -o $P/$name.bin > $P/gcc.log 2>&1
         fi
      fi
      for d in $P $N; do
         (cd $d && { [ -x ./$name.bin ] && ./$name.bin; echo "rc=$?"; } > run.txt 2>&1)
      done
      if [ "$(tail -1 $P/$name.out)" != "rc=0" ]; then
         echo "FAIL  $name ($*): $(head -1 $P/$name.out)"; fail=1
      elif [ ! -x $N/$name.bin ]; then
         echo "FAIL  $name: nasm / linker (viz $N/nasm.log)"; fail=1
      elif cmp -s $P/run.txt $N/run.txt; then
         echo "ok    $name ($*) - beh == nasm ($(tail -1 $N/run.txt))"
      else
         echo "FAIL  $name ($*) - beh se lisi od nasm"
         diff $N/run.txt $P/run.txt | head -5
         fail=1
      fi
   done
fi

# ---------------------------------------------------------------------- 3. Windows (PE -w, COFF -m)
#   win64 -m: objekt slinkuje lld-link (importni knihovny z llvm-dlltool) - instrukce, data
#             a importy shodne s nasm -f win64 + lld-link (objdump bez bajtu instrukci:
#             pasm a nasm volí u MOV reg, reg opacny smer operandu)
#   win64 -w: importy obrazce == nasm + lld-link
#   beh (jen je-li wine): win64 -w, -m a nasm - stejny vystup a navratovy kod (35);
#   win32 -w (PE32) - vystup a navratovy kod 7
if ! command -v nasm > /dev/null || ! command -v lld-link > /dev/null ||
   ! command -v llvm-dlltool > /dev/null || ! command -v llvm-readobj > /dev/null; then
   echo "SKIP  Windows: chybi nasm, lld-link, llvm-dlltool nebo llvm-readobj"
else
   L=$OUT/win/lib; N=$OUT/nasm/win64; W=$OUT/pasm/win64w; M=$OUT/pasm/win64m; X=$OUT/pasm/win32w
   mkdir -p $L $N
   printf 'LIBRARY kernel32.dll\nEXPORTS\nGetStdHandle\nWriteFile\nExitProcess\n' > $L/kernel32.def
   printf 'LIBRARY msvcrt.dll\nEXPORTS\nprintf\nexit\nstrlen\n' > $L/msvcrt.def
   for d in kernel32 msvcrt; do
      llvm-dlltool -m i386:x86-64 -d $L/$d.def -l $L/$d.lib || fail=1
   done
   link64 ()   # link64 <adresar> - win64.obj => win64.exe
   {
      (cd $1 && lld-link /nologo /entry:start /subsystem:console /nodefaultlib /base:0x400000 \
          /fixed /out:win64.exe win64.obj $L/kernel32.lib $L/msvcrt.lib > link.log 2>&1)
   }
   pasm $PASM $W win64 -w -l
   pasm $PASM $M win64 -m
   pasm $PASM $X win32 -w -l
   nasm -f win64 nasm/win64.nasm -o $N/win64.obj > $N/nasm.log 2>&1 && link64 $N
   [ -f $M/win64.obj ] && link64 $M
   for d in $W $M $X; do
      [ "$(tail -1 $d/*.out)" = "rc=0" ] || { echo "FAIL  $d: $(head -1 $d/*.out)"; fail=1; }
   done
   if [ ! -f $N/win64.exe ]; then
      echo "FAIL  win64: nasm / lld-link (viz $N/nasm.log, $N/link.log)"; fail=1
   elif [ ! -f $M/win64.exe ]; then
      echo "FAIL  win64 (-m): lld-link"; head -5 $M/link.log; fail=1
   else
      for d in $M $N; do
         objdump -d --no-show-raw-insn -M intel $d/win64.exe | tail -n +3 > $d/text.dis
         objdump -s -j .data -j .rdata $d/win64.exe | tail -n +3 > $d/data.hex
      done
      if cmp -s $M/text.dis $N/text.dis && cmp -s $M/data.hex $N/data.hex; then
         echo "ok    win64 (-m) + lld-link == nasm + lld-link (instrukce, data, importy)"
      else
         echo "FAIL  win64 (-m) + lld-link se lisi od nasm + lld-link"
         diff $N/text.dis $M/text.dis | head -5; diff $N/data.hex $M/data.hex | head -5
         fail=1
      fi
   fi
   imports ()  # seznam DLL: funkce
   {
      llvm-readobj --coff-imports $1 | awk '/Name:/ { dll = $2 } /Symbol:/ { print dll ": " $2 }' | sort
   }
   if [ -f $W/win64.exe ] && [ -f $N/win64.exe ] && [ "$(imports $W/win64.exe)" = "$(imports $N/win64.exe)" ] &&
      [ -n "$(imports $X/win32.exe)" ] && llvm-readobj --file-headers $X/win32.exe | grep -q 'Magic: 0x10B'; then
      echo "ok    win64 (-w) - importy == nasm + lld-link; win32 (-w) - PE32"
   else
      echo "FAIL  importy win64 (-w) / hlavicka win32 (-w)"; fail=1
   fi
   if ! command -v wine > /dev/null; then
      echo "SKIP  beh programu pro Windows: chybi wine"
   else
      export WINEDEBUG=-all
      for d in $W $M $N $X; do
         (cd $d && { wine ./win*.exe 2> wine.log; echo "rc=$?"; } > run.txt)
      done
      if cmp -s $W/run.txt $N/run.txt && cmp -s $M/run.txt $N/run.txt; then
         echo "ok    win64 (-w), win64 (-m) - beh pod wine == nasm ($(tail -1 $N/run.txt))"
      else
         echo "FAIL  win64 - beh pod wine se lisi od nasm"
         diff $N/run.txt $W/run.txt | head -5; diff $N/run.txt $M/run.txt | head -5
         fail=1
      fi
      if [ "$(cat $X/run.txt)" = "$(printf 'Hello from pasm (Windows x86)\nok\nrc=7')" ]; then
         echo "ok    win32 (-w) - beh pod wine (rc=7)"
      else
         echo "FAIL  win32 (-w) - beh pod wine"; cat $X/run.txt | head -5; fail=1
      fi
   fi
   # hlaseni: kontroly z -e / -c pojmenovane podle formatu
   E=$OUT/pasm/winerr; mkdir -p $E
   printf 'code segment use32\nstart: ret\ncode ends\n end start\n' > $E/e32.asm
   printf 'g group code\ncode segment use32\nstart: ret\ncode ends\n end start\n' > $E/egrp.asm
   r1=$(cd $E && $PASM -m e32.asm); r2=$(cd $E && $PASM -w egrp.asm)
   if [ "$r1" = "e32.asm(3,10) Error: COFF object requires USE64 segments" ] &&
      [ "$r2" = "egrp.asm(1,9) Error: Group not allowed in PE executable" ]; then
      echo "ok    hlaseni -m / -w"
   else
      echo "FAIL  hlaseni -m / -w: $r1 / $r2"; fail=1
   fi
fi

[ $fail = 0 ] && echo "vsechny testy prosly" || echo "NEKTERE TESTY SELHALY"
exit $fail
