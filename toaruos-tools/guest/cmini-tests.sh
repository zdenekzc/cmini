#!/bin/sh
# cmini-tests.sh - tests of cmini and cstencils inside ToaruOS (the same steps as tests/05-toaru.sh)
#
#   in the terminal of ToaruOS:   /extras/cmini-tests.sh   (or: sh /extras/cmini-tests.sh)
#
# Everything is built in /extras/tmp (visible in the file browser). Expected values are
# printed next to the results; no test runs by itself, read the output.

echo "== 1. copy the tools (cmini, pasm, cminirt.o, include) to /extras/tmp/cmini"
mkdir -p /extras/tmp
cd /extras/tmp
rm -rf cmini
cp -r /extras/cmini-toaru cmini
cd /extras/tmp/cmini
export PASM_LIBDIR=/lib

echo "== 2. cmini -T (own library, ToaruOS syscalls): dama.cc => run it (expected: tests/examples/expected/dama.txt)"
bin/cmini.bin -T -o /tmp/g1 tests/examples/dama.cc
/tmp/g1
echo "RC=$?"

echo "== 3. cmini -L /lib (ToaruOS libc, pasm linker): classes.cc => run it (expected: tests/examples/expected/classes.txt)"
bin/cmini.bin -L /lib -I include -o /tmp/g2 tests/examples/classes.cc
/tmp/g2
echo "RC=$?"

echo "== 3.1 cmini -L /lib: avl.cc => run it (expected: tests/examples/expected/avl.txt)"
bin/cmini.bin -L /lib -I include -o /tmp/g-avl tests/examples/avl.cc
/tmp/g-avl
echo "RC=$?"

echo "== 3.2 cmini -L /lib: btree.cc => run it (expected: tests/examples/expected/btree.txt)"
bin/cmini.bin -L /lib -I include -o /tmp/g-btree tests/examples/btree.cc
/tmp/g-btree
echo "RC=$?"

echo "== 3.3 cmini -L /lib: jezdec.cc => run it (expected: tests/examples/expected/jezdec.txt)"
bin/cmini.bin -L /lib -I include -o /tmp/g-jezdec tests/examples/jezdec.cc
/tmp/g-jezdec
echo "RC=$?"

echo "== 3.4 cmini -L /lib: veze.cc => run it (expected: tests/examples/expected/veze.txt)"
bin/cmini.bin -L /lib -I include -o /tmp/g-veze tests/examples/veze.cc
/tmp/g-veze
echo "RC=$?"

echo "== 3.5 cmini -L /lib: heapsort.cc => run it (expected: the last line O.K.)"
bin/cmini.bin -L /lib -I include -o /tmp/g-heapsort tests/examples/heapsort.cc
/tmp/g-heapsort
echo "RC=$?"

echo "== 3.6 cmini -L /lib: quicksort.cc => run it (expected: the last line O.K.)"
bin/cmini.bin -L /lib -I include -o /tmp/g-quicksort tests/examples/quicksort.cc
/tmp/g-quicksort
echo "RC=$?"

echo "== 4. pasm -x (assembler + linker): libc64.asm => run it"
cp tests/libc64.asm /tmp/g3.asm
bin/pasm.bin -x /tmp/g3.asm
/tmp/g3.bin
echo "RC=$?"

echo "== 5. cstencils: copy /extras/cstencils to /extras/tmp/cst and build it with cmini -L /lib (unity.cc)"
cd /extras/tmp
rm -rf cst
cp -r /extras/cstencils cst
cd /extras/tmp/cst
/extras/tmp/cmini/bin/cmini.bin -L /lib -I /extras/tmp/cmini/include -I . -I utils -o /extras/tmp/code-cst unity.cc
ls -l /extras/tmp/code-cst

echo "== 6. run code-cst on example/list.t => /extras/tmp/out/list.hpp, list.cpp"
cd /extras/tmp
rm -rf out
mkdir -p out
./code-cst --output-dir /extras/tmp/out /extras/tmp/cst/example/list.t
echo "RC=$?"

echo "== 7. checksums of the results (crc32 and size); expected: list.hpp 32d55fd9 4086, list.cpp 06cb6bfd 14847"
crc32 out/list.hpp
wc -c out/list.hpp
crc32 out/list.cpp
wc -c out/list.cpp
echo "== done"
