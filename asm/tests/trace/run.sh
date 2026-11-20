#!/bin/sh
# run.sh - checks of asm/trace.py on the small examples: the value returned from main
#   loop.asm  sum 5+4+3+2+1 = 15 (0xF)      call.asm  square(6) = 36 (0x24)
cd "$(dirname "$0")" || exit 1
fail=0
check() {   # check <file> <expected hex value of rax at the return from main>
   out=$(python3 ../../trace.py "$1" 2>&1)
   got=$(printf '%s\n' "$out" | sed -n 's/.*return from main, rax = [0-9-]* (0x\([0-9A-F]*\)).*/\1/p')
   if [ "$got" = "$2" ]; then echo "ok    $1 rax=$got"; else echo "FAIL  $1 got '$got' want '$2'"; fail=1; fi
}
check loop.asm F
check call.asm 24
exit $fail
