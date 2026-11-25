#!/bin/sh
# build-new-cmini.sh - cmini compiles cmini (and pasm) inside ToaruOS, no gcc
#
#   in the terminal of ToaruOS:   /extras/build-new-cmini.sh   (or: sh /extras/build-new-cmini.sh)
#
# The same steps as toaruos-tools/selfhost.sh on the host: the installed compiler
# /extras/cmini-toaru/bin/cmini.bin compiles every source of /extras/cmini-src with
# -L /lib -c (pasm backend, ToaruOS libc), pasm -k links them statically with the
# ToaruOS crt0/crti/libc.a/crtn from /lib.
#
# Result in /extras/new-cmini (the same layout as /extras/cmini-toaru):
#   bin/cmini.bin  bin/pasm.bin  lib/cminirt.o  include/  obj/ (objects)
#
# Every command is written out literally, without variables and $(...), so that esh
# (not a full POSIX shell) can run it too.

echo "== 1. /extras/new-cmini"
mkdir -p /extras/new-cmini
rm -rf /extras/new-cmini
mkdir -p /extras/new-cmini/obj
mkdir -p /extras/new-cmini/bin
mkdir -p /extras/new-cmini/lib
cd /extras/cmini-src
export PASM_LIBDIR=/lib

echo "== 2. cmini: compiler sources (cmini -L /lib -c)"
/extras/cmini-toaru/bin/cmini.bin -L /lib -I include -I cmini -I asm -D calloc=cm_calloc -c /extras/new-cmini/obj/c_instr_drv.o cmini/instr_drv.c
/extras/cmini-toaru/bin/cmini.bin -L /lib -I include -I cmini -I asm -D calloc=cm_calloc -c /extras/new-cmini/obj/c_translate.o cmini/translate.c
/extras/cmini-toaru/bin/cmini.bin -L /lib -I include -I cmini -I asm -D calloc=cm_calloc -c /extras/new-cmini/obj/c_decl.o cmini/decl.c
/extras/cmini-toaru/bin/cmini.bin -L /lib -I include -I cmini -I asm -D calloc=cm_calloc -c /extras/new-cmini/obj/c_expr.o cmini/expr.c
/extras/cmini-toaru/bin/cmini.bin -L /lib -I include -I cmini -I asm -D calloc=cm_calloc -c /extras/new-cmini/obj/c_stat.o cmini/stat.c
/extras/cmini-toaru/bin/cmini.bin -L /lib -I include -I cmini -I asm -D calloc=cm_calloc -c /extras/new-cmini/obj/c_sem.o cmini/sem.c
/extras/cmini-toaru/bin/cmini.bin -L /lib -I include -I cmini -I asm -D calloc=cm_calloc -c /extras/new-cmini/obj/c_util.o cmini/util.c
/extras/cmini-toaru/bin/cmini.bin -L /lib -I include -I cmini -I asm -D calloc=cm_calloc -c /extras/new-cmini/obj/c_lexer.o cmini/lexer.c
/extras/cmini-toaru/bin/cmini.bin -L /lib -I include -I cmini -I asm -D calloc=cm_calloc -c /extras/new-cmini/obj/c_instr.o cmini/instr.c
/extras/cmini-toaru/bin/cmini.bin -L /lib -I include -I cmini -I asm -D calloc=cm_calloc -c /extras/new-cmini/obj/c_cminirt.o cminirt/cminirt.c

echo "== 3. cmini: pasm sources (cmini -L /lib -c)"
/extras/cmini-toaru/bin/cmini.bin -L /lib -I include -I cmini -I asm -D calloc=cm_calloc -c /extras/new-cmini/obj/a_pasmlib.o asm/pasmlib.c
/extras/cmini-toaru/bin/cmini.bin -L /lib -I include -I cmini -I asm -D calloc=cm_calloc -c /extras/new-cmini/obj/a_code.o asm/code.c
/extras/cmini-toaru/bin/cmini.bin -L /lib -I include -I cmini -I asm -D calloc=cm_calloc -c /extras/new-cmini/obj/a_def.o asm/def.c
/extras/cmini-toaru/bin/cmini.bin -L /lib -I include -I cmini -I asm -D calloc=cm_calloc -c /extras/new-cmini/obj/a_expr.o asm/expr.c
/extras/cmini-toaru/bin/cmini.bin -L /lib -I include -I cmini -I asm -D calloc=cm_calloc -c /extras/new-cmini/obj/a_inst.o asm/inst.c
/extras/cmini-toaru/bin/cmini.bin -L /lib -I include -I cmini -I asm -D calloc=cm_calloc -c /extras/new-cmini/obj/a_key.o asm/key.c
/extras/cmini-toaru/bin/cmini.bin -L /lib -I include -I cmini -I asm -D calloc=cm_calloc -c /extras/new-cmini/obj/a_oth.o asm/oth.c
/extras/cmini-toaru/bin/cmini.bin -L /lib -I include -I cmini -I asm -D calloc=cm_calloc -c /extras/new-cmini/obj/a_asmlist.o asm/asmlist.c
/extras/cmini-toaru/bin/cmini.bin -L /lib -I include -I cmini -I asm -D calloc=cm_calloc -c /extras/new-cmini/obj/a_pasm.o asm/pasm.c

echo "== 4. helpers: calloc that zeroes memory, _setjmp alias (ToaruOS libc has setjmp only)"
/extras/cmini-toaru/bin/cmini.bin -L /lib -I include -c /extras/new-cmini/obj/calloc_zero.o selfhost/calloc_zero.c
cp selfhost/setjmp_alias.asm /extras/new-cmini/obj/setjmp_alias.asm
cd /extras/new-cmini/obj
/extras/cmini-toaru/bin/pasm.bin -c setjmp_alias.asm

echo "== 5. link (pasm -k, ToaruOS crt0/crti/libc.a/crtn)"
/extras/cmini-toaru/bin/pasm.bin -k /extras/new-cmini/bin/cmini.bin /lib/crt0.o /lib/crti.o c_instr_drv.o c_translate.o c_decl.o c_expr.o c_stat.o c_sem.o c_util.o c_lexer.o c_instr.o a_pasmlib.o a_code.o a_def.o a_expr.o a_inst.o a_key.o a_oth.o a_asmlist.o setjmp_alias.o calloc_zero.o /lib/libc.a /lib/crtn.o
/extras/cmini-toaru/bin/pasm.bin -k /extras/new-cmini/bin/pasm.bin /lib/crt0.o /lib/crti.o a_pasm.o a_pasmlib.o a_code.o a_def.o a_expr.o a_inst.o a_key.o a_oth.o a_asmlist.o setjmp_alias.o calloc_zero.o /lib/libc.a /lib/crtn.o
cp c_cminirt.o /extras/new-cmini/lib/cminirt.o
cp -r /extras/cmini-src/include /extras/new-cmini/include

echo "== 6. check: the new cmini compiles dama.cc (expected: /extras/cmini-toaru/tests/examples/expected/dama.txt)"
cd /extras/new-cmini
bin/cmini.bin -L /lib -I include -o /tmp/new-dama /extras/cmini-toaru/tests/examples/dama.cc
/tmp/new-dama
echo "RC=$?"
ls -l /extras/new-cmini/bin /extras/new-cmini/lib
echo "== done: /extras/new-cmini"
