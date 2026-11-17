#!/bin/bash
# guest-cmini.sh — the cmini compiler DIRECTLY IN ToaruOS: installs gcc in the host
# (msk install build-essential, needs the network), builds the package (mkpkg.sh =>
# sh build.sh => cmini.bin, pasm.bin), cmini.bin compiles the programs from
# tests/examples in the host (libc mode: pasm -c + gcc + ToaruOS libc), runs them
# and compares them with the templates in tests/examples/expected.
#
#   ./guest-cmini.sh                   starts the VM itself (and stops it at the end)
#   PROGS="dama avl" ./guest-cmini.sh
#
# Known differences of the ToaruOS 2.3.2 libc (not cmini): printf %f without the default precision 6
# (2.5 instead of 2.500000) - floats, pointers, sequence, dictionary; a difference in which
# all differing lines contain a decimal number is reported as "just %f".
set -uo pipefail

DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# the main cecko tree is not always two levels up — see toaru-top.sh
. "$DIR/toaru-top.sh"
TOP=$(find_top "$DIR") \
    || { echo "error: no main cecko tree (asm/code.c + cmini/instr.c) — set TOP=/path" >&2; exit 1; }
OUT=${OUT:-${DIR}/_build/guest-cmini}
PORT=${PORT:-8011}
PROGS=${PROGS:-"dama jezdec avl btree veze classes strings defaults chars dtors floats objcopy pointers sequence dictionary dyncast quicksort heapsort"}
VM=$DIR/toaru-vm.sh

die() { echo "error: $*" >&2; exit 2; }

rm -rf "$OUT"; mkdir -p "$OUT"
OUT= SRC= "$DIR/mkpkg.sh" > /dev/null || die "mkpkg.sh"
cp "$DIR/_build/push/cmini-src.tar.gz" "$OUT/"
for p in $PROGS; do cp "$TOP/tests/examples/$p.cc" "$OUT/" || die "missing tests/examples/$p.cc"; done

started=false
if [[ $("$VM" status) != *"QEMU is running"* ]]; then
    "$VM" start > /dev/null || die "toaru-vm.sh start"
    started=true
fi
python3 -m http.server "$PORT" --bind 127.0.0.1 -d "$OUT" > /dev/null 2>&1 &
HTTP=$!
trap 'kill $HTTP 2>/dev/null; $started && "$VM" stop > /dev/null' EXIT

wait_for() {   # wait_for <line> <seconds>
    local i
    for i in $(seq 1 "$2"); do
        [[ $'\n'$("$VM" log 6)$'\n' == *$'\n'"$1"$'\n'* ]] && return 0
        sleep 1
    done
    return 1
}
RUN=$(date +%s)
for i in $(seq 1 180); do                 # shell prompt on the serial line (startup ~20 s)
    [[ $("$VM" log 3) == *root@livecd* ]] && break
    sleep 1
done
"$VM" send "echo ==UP-$RUN=="
wait_for "==UP-$RUN==" 60 || die "host is not responding (./toaru-vm.sh log)"

echo "gcc in the host (msk install build-essential) ..."
"$VM" send "msk install build-essential; echo ==MSK-$RUN=="
sleep 8
"$VM" send "y"                      # installation confirmation (otherwise just "y: Command not found")
wait_for "==MSK-$RUN==" 300 || die "msk install did not finish"

echo "building cmini.bin and pasm.bin in the host (sh build.sh) ..."
"$VM" send "cd /tmp; rm -rf cmini-src; fetch http://10.0.2.2:$PORT/cmini-src.tar.gz -o c.tgz; tar xzf c.tgz; cd cmini-src; sh build.sh; mkdir -p tests/examples; echo ==BLD-$RUN=="
wait_for "==BLD-$RUN==" 900 || die "build.sh did not finish"

echo "compiling and running the programs with cmini.bin in the host ..."
for p in $PROGS; do
    # tests/examples next to include/ - the examples include ../../include/containers.h
    "$VM" send "fetch http://10.0.2.2:$PORT/$p.cc -o tests/examples/$p.cc; ./cmini.bin -I include -o /tmp/$p tests/examples/$p.cc; echo ==BEG-$p-$RUN==; /tmp/$p; echo ==END-$p-$RUN=="
    wait_for "==END-$p-$RUN==" 120 || echo "warning: $p did not finish"
done
"$VM" log 1000000 > "$OUT/guest.log"

# difference only in lines with a decimal number? (diff separately - with pipefail its
# exit code would override grep's result and every difference would be "just %f")
onlyf() {
    local d
    d=$(diff "$1" "$2" | grep '^[<>]')
    [[ -s $2 && -n $d ]] && ! grep -qv '[0-9]\.[0-9]' <<< "$d"
}

fail=0
for p in $PROGS; do
    awk -v b="==BEG-$p-$RUN==" -v e="==END-$p-$RUN==" '$0==b{f=1;next} $0==e{f=0} f && !/^root@/' \
        "$OUT/guest.log" > "$OUT/$p.out"
    exp=$TOP/tests/examples/expected/$p.txt
    if [[ ! -f $exp ]]; then
        [[ $(tail -n 1 "$OUT/$p.out") == "O.K." ]] && echo "ok    $p (O.K.)" || { echo "FAIL  $p"; fail=1; }
    elif cmp -s "$exp" "$OUT/$p.out"; then
        echo "ok    $p ($(wc -l < "$OUT/$p.out") lines)"
    elif onlyf "$exp" "$OUT/$p.out"; then
        echo "f     $p - only lines with a decimal number differ (printf %f in ToaruOS libc)"
    else
        echo "FAIL  $p"; diff "$exp" "$OUT/$p.out" | head -5; fail=1
    fi
done
[[ $fail == 0 ]] && echo "guest-cmini: OK" || echo "guest-cmini: SOME FAILED"
exit $fail
