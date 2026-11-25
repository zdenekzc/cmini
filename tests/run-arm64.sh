#!/bin/bash
# run-arm64.sh - ToaruOS aarch64 v okne (QEMU, gtk). Aarch64 protejsek tests/run-toaru.sh (x86_64).
#
#   tests/run-arm64.sh          priprava (zip, cross TinyCC, payload) a VM v okne; VM zustava bezet
#   VISIBLE=0 tests/run-arm64.sh   bez okna (seriova linka), skript skonci, VM bezi na pozadi
#
# V guestovi: /extras/build-examples.sh (prelozi a spusti priklady z cmini -C),
# /extras/tcc/bin/tcc (TinyCC v ToaruOS), /bin/tcc.
# Zastaveni:  pkill -f qemu-system-aarch64
# Test:       tests/06-toaru-arm64.sh
set -eu

TOP=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
. "$TOP/scripts/need.sh"
need qemu-system-aarch64
need unzip
need fakeroot
need curl

A64_TOP=$TOP
export A64_TOP
. "$TOP/toaruos-tools/arm64-common.sh"

a64_prepare || { echo "error: priprava ToaruOS aarch64 selhala" >&2; exit 1; }
mkdir -p "$A64_WORK"
a64_start "${VISIBLE:-1}" "$A64_WORK/qemu.log"
echo "ToaruOS aarch64 bezi (QEMU pid $A64_QEMU_PID). Po startu: sh /extras/build-examples.sh"
echo "Zastaveni: pkill -f qemu-system-aarch64"
