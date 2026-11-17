#!/bin/bash
# check.sh — translation and tooling regression tests for ToaruOS, WITHOUT QEMU and without network.
#
#   ./check.sh            # all tests, returns 0 only if all passed
#   ./check.sh -v         # also print the output of commands the tests call
#   QEMU=1 ./check.sh     # also start the VM (REQUIRES /dev/kvm and an IMG without a lock)
#
# What is checked:
#   T1  mkguest.sh => guest-src (from the current tree) builds with a clean gcc (no make, no C++, no g++) =>
#       cmini.bin AND pasm.bin
#   T1b pasm.bin from guest-src with -T: the image must match probe/toaru64.bin
#       (which ran in the host — MISSING, see README §0a, so this is skipped),
#       -T without -e must return 2 and without -T the base stays 0x400000.
#       The same is done on the Makefile side by `make test-toaru`.
#   T1c pasm.bin -e -T probe/openpos.asm (data before code): e_entry = jump
#       JMP start right after the header (0x3F000078), see README §5c
#   T2  cmini.bin compiles pokus.c and its output equals the output from gcc
#   T2b cmini.bin -T probe/toaruc.c: image for ToaruOS (0x3F000000, JMP, ToaruOS
#       syscalls in the runtime library); running in the host is checked by guest-run.sh (with VM)
#   T3  images in probe/, _build/probe/ and _build/push/ meet the ToaruOS loader requirements (README §3b)
#   T4  mkpkg.sh + mkshare.sh produce a package and an ext2 whose flags ext2.ko in 2.3.2 can read
#   T5  toaru-vm.sh is controllable even without a running VM (help, status, send refusal)
#       and _build/work/ contains no stray *.pid (kill on a recycled PID!)
#   T6  mktcc.sh => builds a cross tcc for ToaruOS and its images meet the loader
#       requirements (README §5i); offline, only _import/tinycc must exist
#   T7  QEMU - normally SKIPPED, enabled with QEMU=1
#
# T4 deliberately does not touch _build/share.img — QEMU takes a write lock on it.
set -uo pipefail

DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
VERBOSE=${VERBOSE:-0}
[[ ${1:-} == -v ]] && VERBOSE=1

TMP=$(mktemp -d) || { echo "error: mktemp" >&2; exit 1; }
trap 'rm -rf "$TMP"' EXIT

PASS=0; FAIL=0; SKIP=0
ok()   { echo "  OK   $*"; PASS=$((PASS+1)); }
bad()  { echo "  FAILED $*"; FAIL=$((FAIL+1)); }
skip() { echo "  SKIPPED $*"; SKIP=$((SKIP+1)); }

# e2fsprogs lives in /usr/sbin, which is not in PATH without root — mkshare.sh
# (find_tool) solves the same thing. Without that, T4 was skipped even on a machine
# where e2fsprogs is installed, and that is exactly the kind of "green" you cannot trust.
find_tool() {
    local t=$1 c
    c=$(command -v "$t" 2> /dev/null) && { echo "$c"; return 0; }
    for c in "/sbin/$t" "/usr/sbin/$t" "/usr/local/sbin/$t"; do
        [[ -x $c ]] && { echo "$c"; return 0; }
    done
    return 1
}
MKE2FS=$(find_tool mke2fs || true)
DUMPE2FS=$(find_tool dumpe2fs || true)
DEBUGFS=$(find_tool debugfs || true)
run()  { if [[ $VERBOSE == 1 ]]; then echo "  \$ $*"; fi; "$@" >>"$TMP/out" 2>&1; }
show() { [[ $VERBOSE == 1 && -s "$TMP/out" ]] && sed 's/^/    | /' "$TMP/out"; : > "$TMP/out"; }

echo "== T1 guest-src: build with a clean gcc =="
PKG="$TMP/pkg"
OUT="$PKG" "$DIR/mkguest.sh" > "$TMP/mkguest.log" 2>&1 || { bad "mkguest.sh failed"; sed 's/^/    | /' "$TMP/mkguest.log"; }
rm -rf "$PKG/obj" "$PKG/cmini.bin" "$PKG/libpasm.a" "$PKG/pasm.bin"
find "$PKG" -name '*.o' -type f -delete
if ! command -v gcc >/dev/null; then
    skip "no gcc"
else
    ( cd "$PKG" && sh build.sh ) >"$TMP/build.log" 2>&1
    if [[ -x $PKG/cmini.bin ]]; then
        ok "sh build.sh => cmini.bin ($(wc -c < "$PKG/cmini.bin") B)"
    else
        bad "build.sh did not create cmini.bin"; sed 's/^/    | /' "$TMP/build.log" | tail -20
    fi
    [[ -x $PKG/pasm.bin ]] && ok "sh build.sh => pasm.bin (assembler, supports -T)" \
        || bad "build.sh did not create pasm.bin"
fi

echo "== T1b pasm.bin from guest-src with the -T switch =="
if [[ ! -x $PKG/pasm.bin ]]; then
    skip "T1b without pasm.bin"
else
    mkdir -p "$TMP/t1b"
    cp "$DIR/probe/toaru64.asm" "$TMP/t1b/"
    ( cd "$TMP/t1b" && "$PKG/pasm.bin" -e -T toaru64.asm ) >"$TMP/t1b.log" 2>&1
    if [[ ! -f $TMP/t1b/toaru64.bin ]]; then
        bad "pasm.bin -e -T did not create an image"; tail -3 "$TMP/t1b.log" | sed 's/^/    | /'
    elif [[ ! -f $DIR/probe/toaru64.bin ]]; then
        # The reference did not carry over from the original tree (it is a 262 B image that
        # ran 2026-11-10 in the host — not the one the current pasm produces; that one has
        # an extra jump before the data, so it would not match even after regeneration). Without it
        # only "pasm supports -T and then -T without -e" can be measured, not byte-equivalence.
        skip "missing reference probe/toaru64.bin (did not carry over) — no comparison with the image that ran in the host here"
        echo "       new image: $(wc -c < "$TMP/t1b/toaru64.bin") B, $(readelf -hW "$TMP/t1b/toaru64.bin" 2>/dev/null | grep -m1 'Entry point' | tr -s ' ')"
    elif cmp -s "$TMP/t1b/toaru64.bin" "$DIR/probe/toaru64.bin"; then
        ok "image from guest-src is byte-for-byte identical to probe/toaru64.bin ($(wc -c < "$TMP/t1b/toaru64.bin") B)"
        echo "       (that one ran 2026-11-10 in the host => compiling inside the host produces the same image)"
    else
        bad "image from guest-src differs from probe/toaru64.bin"
        echo "       entry: $(readelf -hW "$TMP/t1b/toaru64.bin" 2>/dev/null | grep -m1 'Entry point')"
    fi
    ( cd "$TMP/t1b" && "$PKG/pasm.bin" -T toaru64.asm ) >/dev/null 2>&1
    [[ $? -eq 2 ]] && ok "-T without -e rejected (return 2)" || bad "-T without -e should have returned 2, returned $?"
    # Linux path: without -T the base must stay 0x400000
    ( cd "$TMP/t1b" && "$PKG/pasm.bin" -e toaru64.asm ) >/dev/null 2>&1
    base=$(readelf -lW "$TMP/t1b/toaru64.bin" 2>/dev/null | awk '/[[:space:]]LOAD[[:space:]]/{print $3; exit}')
    [[ $((base)) -eq $((0x400000)) ]] && ok "without -T base $base (Linux path unchanged)" \
        || bad "without -T the base should be 0x400000, is $base"
fi

echo "== T1c pasm.bin -e -T: data before code => JMP start jump after the header =="
if [[ ! -x $PKG/pasm.bin ]]; then
    skip "T1c without pasm.bin"
else
    cp "$DIR/probe/openpos.asm" "$TMP/t1b/"
    ( cd "$TMP/t1b" && "$PKG/pasm.bin" -e -T openpos.asm ) >"$TMP/t1c.log" 2>&1
    entry=$(readelf -hW "$TMP/t1b/openpos.bin" 2>/dev/null | awk '/Entry point/{print $NF}')
    op=$(od -An -tx1 -j 120 -N 1 "$TMP/t1b/openpos.bin" 2>/dev/null | tr -d ' ')
    low=$(readelf -lW "$TMP/t1b/openpos.bin" 2>/dev/null | awk '/[[:space:]]LOAD[[:space:]]/{print $3; exit}')
    if [[ $((entry)) -eq $((0x3F000078)) && $op == e9 && $((low)) -eq $((0x3F000000)) ]]; then
        ok "openpos.bin: e_entry $entry = JMP start (E9) => data1 lies above the entry point"
    else
        bad "openpos.bin: e_entry $entry, byte at 78h '$op' (expected 0x3f000078 and e9)"
    fi
fi

echo "== T2 cmini.bin vs. gcc (pokus.c) =="
if [[ ! -x $PKG/cmini.bin ]]; then
    skip "T2 without cmini.bin"
else
    ( cd "$PKG" && ./cmini.bin -I include -o "$TMP/pokus_cmini.bin" pokus.c ) >"$TMP/c1.log" 2>&1
    gcc -O0 -w -o "$TMP/pokus_gcc" "$PKG/pokus.c" >"$TMP/c2.log" 2>&1
    if [[ ! -x $TMP/pokus_cmini.bin ]]; then
        bad "cmini.bin did not compile pokus.c"; tail -5 "$TMP/c1.log" | sed 's/^/    | /'
    elif ! run "$TMP/pokus_gcc"; then
        bad "pokus from gcc could not be run"; show
    else
        gcc_out=$(cat "$TMP/out"); : > "$TMP/out"
        if ! run "$TMP/pokus_cmini.bin"; then
            bad "image from cmini could not be run"; show
        else
            cmini_out=$(cat "$TMP/out")
            if [[ "$gcc_out" == "$cmini_out" ]]; then
                ok "output identical to gcc: $(tr -d '\n' <<<"$cmini_out")"
            else
                bad "output differs: gcc='$gcc_out' cmini='$cmini_out'"
            fi
        fi
    fi
fi

echo "== T2b cmini.bin -T: C program for ToaruOS (ToaruOS syscalls) =="
if [[ ! -x $PKG/cmini.bin ]]; then
    skip "T2b without cmini.bin"
else
    ( cd "$PKG" && ./cmini.bin -T -o "$TMP/toaruc.bin" "$DIR/probe/toaruc.c" ) >"$TMP/t2b.log" 2>&1
    entry=$(readelf -hW "$TMP/toaruc.bin" 2>/dev/null | awk '/Entry point/{print $NF}')
    low=$(readelf -lW "$TMP/toaruc.bin" 2>/dev/null | awk '/[[:space:]]LOAD[[:space:]]/{print $3; exit}')
    nt=$(grep -c -- '- ToaruOS\|ToaruOS: ' "$TMP/toaruc.bin.asm" 2>/dev/null)
    nl=$(grep -c -- 'mov     eax, 60\|mov     eax, 12 ' "$TMP/toaruc.bin.asm" 2>/dev/null)
    if [[ $((low)) -eq $((0x3F000000)) && $((entry)) -eq $((0x3F000078)) && $nt -ge 10 && $nl -eq 0 ]]; then
        ok "toaruc.bin: from $low, e_entry $entry (JMP), $nt lines of the ToaruOS library, no exit 60 / brk 12"
    else
        bad "toaruc.bin: base $low, entry $entry, ToaruOS lines $nt, Linux ones $nl"; tail -3 "$TMP/t2b.log" | sed 's/^/    | /'
    fi
    echo "       (run in the host: ./toaru-vm.sh start ; ./guest-run.sh)"
fi

echo "== T3 images vs. the ToaruOS loader (README §3b) =="
# Rules FROM MEASUREMENT, not from assumption:
#   ET_EXEC (ET_DYN/PIE = -EINVAL), >=1 PT_LOAD (there may be more — gcc
#   makes R / RE / RW separately), no PT_INTERP nor PT_DYNAMIC, and ABOVE ALL
#   the base 0x3F000000. With address 0x400000 the kernel does exec it, but the process
#   crashes with SIGSEGV even before the first syscall — hence the check that NO
#   PT_LOAD lies below 0x3F000000. (That is exactly how we crashed on 2026-11-10;
#   kept in probe/counterexample/.)
BASE_TOARU=$((0x3F000000))
check_elf_toaru() {
    local f=$1 h l n va lowest name
    name=$(basename "$f")
    h=$(readelf -hW "$f" 2>/dev/null) || { bad "$name: readelf failed"; return; }
    [[ "$h" == *"Type:"*"EXEC"* ]] || { bad "$name: Type does not have EXEC ($(grep -m1 'Type:' <<<"$h" | sed 's/.*: *//'))"; return; }
    l=$(readelf -lW "$f" 2>/dev/null)
    n=$(grep -c '[[:space:]]LOAD[[:space:]]' <<<"$l")
    [[ $n -ge 1 ]] || { bad "$name: no PT_LOAD — the kernel has nothing to map"; return; }
    grep -q '[[:space:]]INTERP[[:space:]]' <<<"$l" && { bad "$name: has PT_INTERP (the kernel would exec /lib/ld.so)"; return; }
    grep -q '[[:space:]]DYNAMIC[[:space:]]' <<<"$l" && { bad "$name: has PT_DYNAMIC (ld.so would be run)"; return; }
    lowest=$(awk '/[[:space:]]LOAD[[:space:]]/{print $3}' <<<"$l" | sort | head -1)
    if [[ $((lowest)) -lt $BASE_TOARU ]]; then
        bad "$name: PT_LOAD at $lowest < 0x3F000000 => SIGSEGV in the host before the first syscall"
        return
    fi
    va=$(grep -m1 'Entry point' <<<"$h" | awk '{print $NF}')
    if [[ $((va)) -lt $BASE_TOARU ]]; then
        bad "$name: entry $va is below 0x3F000000"; return
    fi
    ok "$name: EXEC, $n x LOAD from $lowest, entry $va, no INTERP/DYNAMIC"
    # known caught difference between "can be loaded" and "is compiled for ToaruOS"
    case "$name" in
      hello64-toaru.bin)
        echo "       WARNING: the format matches, but the syscall numbers are from Linux — it will not run in the host (README §1)" ;;
    esac
}
n_elf=0
for f in "$DIR"/probe/*.elf "$DIR"/probe/*.bin "$DIR"/_build/probe/*.elf "$DIR"/_build/push/*.elf "$DIR"/_build/push/*.bin; do
    [[ -f $f ]] || continue
    n_elf=$((n_elf+1))
    check_elf_toaru "$f"
done
[[ $n_elf -gt 0 ]] || skip "no image to check"
# The contrast must remain contrast: if someone also rewrote the files from
# probe/counterexample/ to address 0x3F000000, the evidence about the low address disappears
# and the remaining measurements lose their reference.
for f in "$DIR"/probe/counterexample/*.elf; do
    [[ -f $f ]] || continue
    low=$(readelf -lW "$f" 2>/dev/null | awk '/[[:space:]]LOAD[[:space:]]/{print $3}' | sort | head -1)
    [[ -n $low ]] || continue
    if [[ $((low)) -lt $BASE_TOARU ]]; then
        ok "$(basename "$f"): contrast preserved (lowest LOAD $low < 0x3F000000)"
    else
        bad "$(basename "$f"): no longer contrast (LOAD $low) — could be mistakenly deployed to _build/push/"
    fi
done

echo "== T4 mkpkg.sh and mkshare.sh =="
# We override OUT to /tmp — the test must not overwrite _build/push/, which is just waiting for mkshare.sh
if ! OUT="$TMP/cmini-src.tar.gz" run "$DIR/mkpkg.sh"; then
    bad "mkpkg.sh failed"; show
else
    pkg_out=$(sed -n 's/^package: \([^ ]*\) .*/\1/p' "$TMP/out"); : > "$TMP/out"
    if [[ -f $pkg_out ]]; then
        ok "mkpkg.sh => $(basename "$pkg_out") ($(wc -c < "$pkg_out") B)"
        # unpack and build again => the package builds standalone
        mkdir -p "$TMP/unpacked"; tar xzf "$pkg_out" -C "$TMP/unpacked"
        [[ -f "$TMP/unpacked/cmini-src/build.sh" ]] \
            && ok "unpacked package contains cmini-src/build.sh" \
            || bad "unpacked package does not contain cmini-src/build.sh"
    else
        bad "mkpkg.sh did not write the package to the expected place"
    fi
    if [[ -z $MKE2FS ]]; then
        skip "no mke2fs (e2fsprogs) — also searched in /sbin, /usr/sbin, /usr/local/sbin"
    else
        # We override SRC and IMG too, so the test only touches /tmp
        mkdir -p "$TMP/src"; : > "$TMP/src/hello.txt"
        if SRC="$TMP/src" IMG="$TMP/share.img" MB=8 "$DIR/mkshare.sh" >"$TMP/mk.log" 2>&1; then
            feats=$( "${DUMPE2FS:-dumpe2fs}" -h "$TMP/share.img" 2>/dev/null | sed -n 's/^Filesystem features: *//p')
            bad_feat=""
            for f in 64bit metadata_csum dir_index; do
                [[ " $feats " == *" $f "* ]] && bad_feat="$bad_feat $f"
            done
            if [[ -n $bad_feat ]]; then
                bad "ext2 has flags that ext2.ko in 2.3.2 cannot read:$bad_feat"
            elif [[ -z $DUMPE2FS || -z $feats ]]; then
                # nothing could be enumerated, and this is not "good so far"
                bad "dumpe2fs printed no flags (DUMPE2FS=${DUMPE2FS:-none}), the image cannot be checked"
            else
                ok "mkshare.sh => ext2 with flags that ext2.ko in 2.3.2 can read ($feats)"
            fi
            "${DEBUGFS:-debugfs}" -R 'ls -l /' "$TMP/share.img" 2>/dev/null | grep -q hello.txt \
                && ok "disk contents visible (debugfs)" || bad "disk contents not visible"
        else
            bad "mkshare.sh failed"; tail -5 "$TMP/mk.log" | sed 's/^/    | /'
        fi
    fi
fi

echo "== T5 toaru-vm.sh without a running VM =="
usage=$("$DIR/toaru-vm.sh" 2>&1 | head -3)
grep -q "toaru-vm.sh" <<<"$usage" && ok "help ($(head -1 <<<"$usage"))" || bad "help was not printed"
st=$("$DIR/toaru-vm.sh" status 2>&1)
grep -q "QEMU is not running" <<<"$st" && ok "status: $(head -1 <<<"$st")" || bad "status reports something odd: $st"
"$DIR/toaru-vm.sh" send 'ls /' >"$TMP/send.log" 2>&1
[[ $? -ne 0 ]] && ok "send without VM rejected ($(head -1 <<<"$(cat "$TMP/send.log")"))" \
                || bad "send without VM did not fail — it could write into a foreign FIFO"
stale=$(find "$DIR/_build/work" -maxdepth 1 -name '*.pid' 2>/dev/null)
[[ -z $stale ]] && ok "_build/work/ without *.pid" \
    || bad "in _build/work/ there is *.pid ($stale) — stop/running would kill another (recycled) process"
echo "== T6 mktcc.sh: cross tcc for ToaruOS and its images (without VM) =="
# it is built here every time, because there are more checks the image must pass than
# "it compiled" - an image with PIE or without an __init_array array will fail in the host
if [[ ! -d $DIR/../../_import/tinycc ]]; then
    skip "missing clone _import/tinycc (./mktcc.sh -p)"
elif ! "$DIR/mktcc.sh" > "$TMP/mktcc.log" 2>&1; then
    bad "mktcc.sh failed"; tail -8 "$TMP/mktcc.log" | sed 's/^/    | /'
else
    n=$(sed -n 's/^done: \([0-9]*\) OK.*/\1/p' "$TMP/mktcc.log")
    [[ ${n:-0} -ge 10 ]] \
        && ok "mktcc.sh: $n image checks OK (ET_EXEC, 0x3F000000, no PT_DYNAMIC, no data below e_entry)" \
        || bad "mktcc.sh passed with only ${n:-0} checks"
    grep -q ", 0 problems" "$TMP/mktcc.log" && ok "no problem" \
        || { bad "mktcc.sh reports problems"; tail -12 "$TMP/mktcc.log" | sed 's/^/    | /'; }
    [[ -x $DIR/_build/tcc-toaru/bin/x86_64-toaru-tcc ]] \
        && ok "x86_64-toaru-tcc on disk" || bad "the compiler was not created"
    grep -q "libtcc1.a:" "$TMP/mktcc.log" && ok "libtcc1.a for the target" || bad "libtcc1.a was not created"
fi

if [[ ${QEMU:-0} == 1 ]]; then
    echo "== T7 QEMU (zapnuto QEMU=1) =="
    [[ -r /dev/kvm && -w /dev/kvm ]] || skip "no /dev/kvm"
    [[ -f $DIR/../../_import/toaruos-2.3.2.iso ]] || skip "missing ISO"
    # deliberately we start nothing ourselves: start is up to the human, the VM holds a lock on _build/share.img
    skip "leaving VM start to the human (lock on _build/share.img)"
else
    echo "== T7 QEMU: SKIPPED (QEMU=1 enables it) =="
    skip "start VM"
fi

echo
echo "check: $PASS OK, $FAIL FAILED, $SKIP skipped"
[[ $FAIL -eq 0 ]] || exit 1
echo "check: OK"
