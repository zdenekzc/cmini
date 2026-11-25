#!/bin/sh
# build.sh — builds probe*.elf ON THE HOST into ../_build/probe/ and copies them into
# ../_build/push/ (shared disk).  The verified originals probe*b.elf stay here.
#
# The resulting file must be an ELF64 that the ToaruOS kernel can load:
#   * ET_EXEC — the kernel rejects ET_DYN (PIE) ("Not an executable", elf64.c:276)
#   * no PT_DYNAMIC and no PT_INTERP — with PT_DYNAMIC /lib/ld.so would be started
#   * THE BASE ADDRESS MUST NOT BE LOW. Verified on 2026-11-10: with the default
#     linker address (0x400000) the process crashes with SIGSEGV before the first
#     syscall (strace shows only execve). At 0x3F000000, i.e. the address of their
#     linker/link.ld, which is also used for /lib/ld.so, it works fine. Hence
#     -Wl,-Ttext-segment. Another requirement is -z max-page-size=0x1000, the loader
#     maps 4 KiB pages and p_align is not checked anywhere.
#
# Verified on QEMU + ISO 2.3.2: probe1 and probe2 both PASSED, including write(1) and
# the parameters from the shell.
set -u

D=$(cd "$(dirname "$0")" && pwd)
TOP=$(cd "$D/../.." && pwd)
OUT=${OUT:-${TOP}/_build/probe}
PUSH=${PUSH:-${TOP}/_build/push}
CC=${CC:-gcc}
BASE=${BASE:-0x3F000000}
LDFLAGS="-nostdlib -no-pie -static -Wl,-Ttext-segment=${BASE} -Wl,-z,max-page-size=0x1000"

mkdir -p "$OUT" "$PUSH" || exit 1

for f in probe1 probe2; do
    # shellcheck disable=SC2086
    "$CC" ${LDFLAGS} -o "$OUT/$f.elf" "$D/$f.S" || exit 1
    if [ "$f" = probe2 ]; then
        python3 -I "$D/patch_ktmp.py" "$OUT/$f.elf" || exit 1
    fi
    chmod 755 "$OUT/$f.elf"
    cp -a "$OUT/$f.elf" "$PUSH/" || exit 1
    echo "--- $f.elf"
    readelf -h "$OUT/$f.elf" | grep -E 'Class|Type|Machine|Entry'
    readelf -l "$OUT/$f.elf" | grep -E 'LOAD|INTERP|Type' | head -5
done

echo "--- probe2 sections (.ktmp should have type 8000000c, but this is a contrast test):"
readelf -S "$OUT/probe2.elf" | grep -A1 -E 'Name|ktmp' | head -8
echo "--- in _build/push/:"; ls -l "$PUSH"
