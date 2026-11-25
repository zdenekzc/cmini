#!/bin/bash
# guest-run.sh — programs in C compiled by c-mini FOR ToaruOS (cmini -T: own runtime library,
# ToaruOS syscalls, image from 0x3F000000) run them on the host and compare the output
# with Linux (the same programs, CPARSER_RTL=own).
#
#   ./toaru-vm.sh start ; ./guest-run.sh ; ./toaru-vm.sh stop
#   PROGS="dama avl" ./guest-run.sh
#
# Programs: tests/examples/*.cc, which run with their own library (tests/build-test.sh),
# and probe/toaruc.c (heap via sbrk above 64 KB, calloc, time). Compilation happens on the
# host (cmini/cmini.bin from the tree); the host gets the images via
# python3 -m http.server and fetch (gcc on the host is not needed).
set -uo pipefail

DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
TOP=$(cd "$DIR/.." && pwd)
OUT=${OUT:-${TOP}/_build/guest-run}
PORT=${PORT:-8010}
PROGS=${PROGS:-"toaruc dama jezdec avl classes strings defaults chars dtors"}
CMINI=${CMINI:-${TOP}/cmini/cmini.bin}

die() { echo "error: $*" >&2; exit 2; }
[[ -x $CMINI ]] || die "missing $CMINI (make all)"
[[ $("$DIR/toaru-vm.sh" status) == *"QEMU is running"* ]] || die "VM is not running — ./toaru-vm.sh start"

rm -rf "$OUT"; mkdir -p "$OUT"
for p in $PROGS; do
    src=$TOP/tests/examples/$p.cc
    [[ $p == toaruc ]] && src=$DIR/probe/toaruc.c
    CPARSER_RTL=own "$CMINI" -o "$OUT/$p.lin" "$src" > "$OUT/$p.log" 2>&1 &&
        (cd "$OUT" && "./$p.lin" > "$p.lin.out" 2>&1)
    "$CMINI" -T -o "$OUT/$p.bin" "$src" >> "$OUT/$p.log" 2>&1 || echo "FAIL  $p: -T compilation (see $OUT/$p.log)"
done

python3 -m http.server "$PORT" --bind 127.0.0.1 -d "$OUT" > /dev/null 2>&1 &
HTTP=$!
trap 'kill $HTTP 2>/dev/null' EXIT
sleep 1

wait_for() {   # wait_for <line> - until it appears in the log (max 60 s)
    local i
    for i in $(seq 1 60); do
        [[ $'\n'$("$DIR/toaru-vm.sh" log 5)$'\n' == *$'\n'"$1"$'\n'* ]] && return 0
        sleep 1
    done
    return 1
}
RUN=$(date +%s)                     # markers are unique even with several runs in one log
for p in $PROGS; do
    [[ -f $OUT/$p.bin ]] || continue
    "$DIR/toaru-vm.sh" send "fetch http://10.0.2.2:$PORT/$p.bin -o /tmp/$p.bin; chmod 755 /tmp/$p.bin; echo ==BEG-$p-$RUN==; /tmp/$p.bin; echo ==END-$p-$RUN=="
    wait_for "==END-$p-$RUN==" || echo "warning: $p did not finish within 60 s"
done
"$DIR/toaru-vm.sh" log 100000 > "$OUT/guest.log"

fail=0
for p in $PROGS; do
    awk -v b="==BEG-$p-$RUN==" -v e="==END-$p-$RUN==" '$0==b{f=1;next} $0==e{f=0} f && !/^root@/' \
        "$OUT/guest.log" > "$OUT/$p.toaru.out"
    if [[ -s $OUT/$p.lin.out ]] && cmp -s "$OUT/$p.lin.out" "$OUT/$p.toaru.out"; then
        echo "ok    $p - output in ToaruOS == Linux ($(wc -l < "$OUT/$p.toaru.out") lines)"
    else
        echo "FAIL  $p"; diff "$OUT/$p.lin.out" "$OUT/$p.toaru.out" | head -5; fail=1
    fi
done
[[ $fail == 0 ]] && echo "guest-run: OK" || echo "guest-run: SOME FAILED"
exit $fail
