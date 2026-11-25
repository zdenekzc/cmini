#!/bin/bash
# mkexamples.sh - /extras/examples and /extras/build-examples.sh for ToaruOS (x86_64 and aarch64).
#
#   ./mkexamples.sh <extras-dir> x86|a64
#
# The same ten programs on both architectures (PROGRAMS), plus rc_check (main returns 7):
#   <extras>/examples/<p>.cc       the cmini sources (for reading and for cmini inside ToaruOS)
#   <extras>/examples/<p>.c        a64 only: cmini -C (translate) of <p>.cc, compiled by the guest tcc
#   <extras>/examples/cminirt.h    a64 only: runtime header of the translated programs
#   <extras>/examples/cminirt.o    a64 only: runtime for aarch64 (cross TinyCC)
#   <extras>/build-examples.sh     compiles and runs them inside ToaruOS, output framed by
#                                  ==BEG-<name>== / RC=<status> / ==END-<name>==
#     x86:  /extras/cmini-toaru/bin/cmini.bin -L /lib (cmini and pasm inside ToaruOS)
#     a64:  /extras/tcc/bin/tcc (TinyCC inside ToaruOS) on the translated C
#
# a64 needs: ./cmini/cmini.bin (make), TCC=<cross aarch64 tcc> (toaruos-tools/mktcc-aarch64.sh).
set -euo pipefail

DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
TOP=$(cd "$DIR/.." && pwd)
EXTRAS=${1:?usage: mkexamples.sh <extras-dir> x86|a64}
FLAVOUR=${2:?usage: mkexamples.sh <extras-dir> x86|a64}
CMINI=$TOP/cmini/cmini.bin
TCC=${TCC:-$TOP/_build/tcc-aarch64/bin/aarch64-toaru-tcc}
TCCLIB=$(dirname "$(dirname "$TCC")")/lib
PROGRAMS="dama classes avl btree jezdec veze heapsort quicksort strings dyncast"
E=$EXTRAS/examples
die() { echo "error: $*" >&2; exit 1; }

rm -rf "$E"
mkdir -p "$E"
for p in $PROGRAMS
do
   cp "$TOP/tests/examples/$p.cc" "$E/"
done
printf '/* rc_check.cc - main returns 7: the exit status must come back to the shell as RC=7 */\nint main ()\n{\n   return 7;\n}\n' > "$E/rc_check.cc"

if [ "$FLAVOUR" = a64 ]
then
   [ -x "$CMINI" ] || die "missing $CMINI (make -C cmini)"
   [ -x "$TCC" ] || die "missing $TCC (toaruos-tools/mktcc-aarch64.sh)"
   for p in $PROGRAMS rc_check
   do
      "$CMINI" -C -I "$TOP/cmini/ownheaders" -o "$E/$p.c" "$E/$p.cc" || die "cmini -C $p"
   done
   cp "$TOP/cmini/cminirt.h" "$E/"
   "$TCC" -B"$TCCLIB" -I "$TOP/cmini" -c "$TOP/cmini/cminirt.c" -o "$E/cminirt.o" 2> /dev/null \
      || die "cross tcc cminirt.c"
fi

{
   echo '#!/bin/sh'
   echo '# build-examples.sh - the examples of /extras/examples compiled and run inside ToaruOS'
   echo '# (output of each program is framed by ==BEG-<name>== ... RC=<status> ... ==END-<name>==)'
   if [ "$FLAVOUR" = x86 ]
   then
      echo 'export PASM_LIBDIR=/lib'
   fi
   for p in $PROGRAMS rc_check
   do
      echo "echo ==BEG-$p=="
      if [ "$FLAVOUR" = x86 ]
      then
         echo "/extras/cmini-toaru/bin/cmini.bin -L /lib -I /extras/cmini-toaru/include -o /tmp/$p /extras/examples/$p.cc"
      else
         echo "/extras/tcc/bin/tcc -w -I /extras/examples -o /tmp/$p /extras/examples/$p.c /extras/examples/cminirt.o"
      fi
      echo "if stat -q \"/tmp/$p\" then /tmp/$p"
      echo 'echo RC=$?'
      echo "echo ==END-$p=="
   done
} > "$EXTRAS/build-examples.sh"
chmod 755 "$EXTRAS/build-examples.sh"
echo "examples: $E (x86 or a64: $FLAVOUR), $EXTRAS/build-examples.sh"
