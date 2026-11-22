#!/bin/sh
# build-new-tiny-ide.sh - cmini compiles tiny-ide inside ToaruOS, no gcc
#
#   in the terminal of ToaruOS:   /extras/build-new-tiny-ide.sh   (or: sh /extras/build-new-tiny-ide.sh)
#
# The same steps as tiny-ide/build-toaru-cmini.sh on the host: the installed compiler
# /extras/cmini-toaru/bin/cmini.bin compiles the sources of /extras/tiny-ide/src with
# -L /lib -c (pasm backend, ToaruOS libc) together with the 8-bit curses layer of
# tiny-ide (src/toaru-cmini: curses8.c, glue.c) and the zeroing calloc, pasm -k links
# them with the ToaruOS crt0/crti/libc.a/crtn from /lib.
#
# Result: /extras/new-tiny-ide/tiny-ide-toaru.bin, objects in obj/
#
# Every command is written out literally, without variables, $(...) and loops, so that
# esh (not a full POSIX shell) can run it.

echo "== 1. /extras/new-tiny-ide"
mkdir -p /extras/new-tiny-ide
rm -rf /extras/new-tiny-ide
mkdir -p /extras/new-tiny-ide/obj
cd /extras/tiny-ide/src

echo "== 2. cmini: tiny-ide sources and its curses layer (cmini -L /lib -c)"
/extras/cmini-toaru/bin/cmini.bin -L /lib -I toaru-cmini -I /extras/cmini-toaru/include -D TOARU_CMINI=1 -D calloc=cm_calloc -c /extras/new-tiny-ide/obj/main.o main.c
/extras/cmini-toaru/bin/cmini.bin -L /lib -I toaru-cmini -I /extras/cmini-toaru/include -D TOARU_CMINI=1 -D calloc=cm_calloc -c /extras/new-tiny-ide/obj/app.o app.c
/extras/cmini-toaru/bin/cmini.bin -L /lib -I toaru-cmini -I /extras/cmini-toaru/include -D TOARU_CMINI=1 -D calloc=cm_calloc -c /extras/new-tiny-ide/obj/editor.o editor.c
/extras/cmini-toaru/bin/cmini.bin -L /lib -I toaru-cmini -I /extras/cmini-toaru/include -D TOARU_CMINI=1 -D calloc=cm_calloc -c /extras/new-tiny-ide/obj/runner.o runner.c
/extras/cmini-toaru/bin/cmini.bin -L /lib -I toaru-cmini -I /extras/cmini-toaru/include -D TOARU_CMINI=1 -D calloc=cm_calloc -c /extras/new-tiny-ide/obj/ui.o ui.c
/extras/cmini-toaru/bin/cmini.bin -L /lib -I toaru-cmini -I /extras/cmini-toaru/include -D TOARU_CMINI=1 -D calloc=cm_calloc -c /extras/new-tiny-ide/obj/dialogs.o dialogs.c
/extras/cmini-toaru/bin/cmini.bin -L /lib -I toaru-cmini -I /extras/cmini-toaru/include -D TOARU_CMINI=1 -D calloc=cm_calloc -c /extras/new-tiny-ide/obj/curses8.o toaru-cmini/curses8.c
/extras/cmini-toaru/bin/cmini.bin -L /lib -I toaru-cmini -I /extras/cmini-toaru/include -D TOARU_CMINI=1 -D calloc=cm_calloc -c /extras/new-tiny-ide/obj/glue.o toaru-cmini/glue.c
/extras/cmini-toaru/bin/cmini.bin -L /lib -I /extras/cmini-toaru/include -c /extras/new-tiny-ide/obj/calloc_zero.o calloc_zero.c

echo "== 3. link (pasm -k, ToaruOS crt0/crti/libc.a/crtn)"
cd /extras/new-tiny-ide/obj
/extras/cmini-toaru/bin/pasm.bin -k /extras/new-tiny-ide/tiny-ide-toaru.bin /lib/crt0.o /lib/crti.o main.o app.o editor.o runner.o ui.o dialogs.o curses8.o glue.o calloc_zero.o /lib/libc.a /lib/crtn.o
ls -l /extras/new-tiny-ide/tiny-ide-toaru.bin
echo "== done: /extras/new-tiny-ide/tiny-ide-toaru.bin"
