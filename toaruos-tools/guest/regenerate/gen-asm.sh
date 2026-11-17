#!/bin/sh
# asm/*.cc -> asm/*.c. cmini.bin -C does not handle the C++ attribute [[noreturn]] (asm/def.h),
# so it is generated from a scrubbed copy - the source itself is not changed.
set -u
# SRC = the earlier development tree (with asm/*.cc and include/). Not part of this
# project: it must be set in the environment, e.g.  SRC=/path/to/old/cecko sh gen-asm.sh
SRC=${SRC:?set SRC to the earlier development tree (see README.md §0)}
S=/tmp/cmini-bundle/sanitized
B=/tmp/cmini-bundle/cmini-src
T=/tmp/cmini-bundle/bin/cmini.bin
E=/tmp/cecko-ctest

rm -rf "$S" || exit 1
mkdir -p "$S" "$E" "$B/asm" || exit 1
cp -a "$SRC/asm" "$S/asm" || exit 1
cp -a "$SRC/include" "$S/include" || exit 1
# cmini.bin -C does not know C++ attributes [[...]] (we do not need them in C11/C23)
find "$S" -type f -print0 | xargs -0 sed -i 's/\[\[[a-z_]*\]\]//g'
# the project's own include/stdint.h does not contain INT*_MIN/MAX and the compiler
# must see them; in the translated .C the real system stdint.h is used
cat >> "$S/include/stdint.h" <<'EOF'
/* hex form: cmini.bin -C cannot convert a decimal constant with the suffix
   LL ("Bad number"), hex without suffix works; the value is the same */
#define INT32_MIN (-0x7fffffff - 1)
#define INT32_MAX 0x7fffffff
#define INT64_MIN (-0x7fffffffffffffff - 1)
#define INT64_MAX 0x7fffffffffffffff
#define UINT32_MAX 0xffffffffU
#define UINT64_MAX 0xffffffffffffffffUL
EOF

cd "$S" || exit 1
status=0
for f in pasmlib code def expr inst key oth pasm; do
    if "$T" -C -I include -I asm -o "$B/asm/$f.c" "asm/$f.cc" 2>"$E/tr_asm_$f.err"; then
        echo "OK   asm/$f.c  $(wc -l < "$B/asm/$f.c") radku"
    else
        echo "FAIL asm/$f.cc"; head -4 "$E/tr_asm_$f.err"; status=1
    fi
done
exit $status
