#!/bin/bash
# build-toaru-cmini.sh - tiny-ide for ToaruOS, compiled by cmini (no TinyCC):
#   cmini.bin -L /lib -c per source (pasm backend), toaruos-tools/cmini-curses/curses8.c = 8-bit curses
#   subset, glue.c, calloc that zeroes memory (toaruos-tools/selfhost), then pasm -k
#   with ToaruOS crt0/crti/libc.a/crtn.
#
#   ./build-toaru-cmini.sh        => _build/tiny-ide/cmini-toaru/tiny-ide-toaru.bin
#
# Needs: ../cmini/cmini.bin, ../asm/pasm.bin (make), _build/toaru-lib (toaruos-tools/mklibc.sh).
set -euo pipefail
HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
TOP=$(cd "$HERE/.." && pwd)
CMINI=${CMINI:-$TOP/cmini/cmini.bin}
PASM=${PASM:-$TOP/asm/pasm.bin}
L=$TOP/_build/toaru-lib
OUT=$TOP/_build/tiny-ide/cmini-toaru
O=$OUT/obj
die() { echo "error: $*" >&2; exit 1; }
[[ -x $CMINI ]] || die "missing $CMINI (make -C cmini)"
[[ -x $PASM ]] || die "missing $PASM (make -C asm)"
[[ -f $L/libc.a ]] || die "missing $L (run toaruos-tools/mklibc.sh)"
rm -rf "$OUT"; mkdir -p "$O"

SHIM=$TOP/toaruos-tools/cmini-curses          # 8-bit curses layer shared with mini-commander
INC="-I $SHIM -I $HERE/toaru-cmini -I $TOP/cmini/ownheaders"
cc() { "$CMINI" -L "$L" $INC -D TOARU_CMINI=1 -D calloc=cm_calloc -c "$1" "$2" || die "cmini -c $2"; }

OBJS=""
for f in main app editor runner ui dialogs; do cc "$O/$f.o" "$HERE/$f.c"; OBJS="$OBJS $O/$f.o"; done
cc "$O/curses8.o" "$SHIM/curses8.c"; OBJS="$OBJS $O/curses8.o"
cc "$O/glue.o" "$SHIM/glue.c"; OBJS="$OBJS $O/glue.o"
"$CMINI" -L "$L" -I "$TOP/cmini/ownheaders" -c "$O/calloc_zero.o" "$TOP/toaruos-tools/selfhost/calloc_zero.c" || die "calloc_zero"
OBJS="$OBJS $O/calloc_zero.o"

"$PASM" -k "$OUT/tiny-ide-toaru.bin" "$L/crt0.o" "$L/crti.o" $OBJS "$L/libc.a" "$L/crtn.o" || die "link (see above: undefined symbols)"
echo "built: $OUT/tiny-ide-toaru.bin ($(stat -c%s "$OUT/tiny-ide-toaru.bin") B)"
