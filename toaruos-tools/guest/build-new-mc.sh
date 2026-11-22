#!/bin/sh
# build-new-mc.sh - cmini compiles mini-commander inside ToaruOS, no gcc
#
#   in the terminal of ToaruOS:   /extras/build-new-mc.sh   (or: sh /extras/build-new-mc.sh)
#
# The same steps as mini-commander/build-toaru-cmini.sh on the host: the installed
# compiler /extras/cmini-toaru/bin/cmini.bin compiles the sources of
# /extras/mini-commander/src with -L /lib -c (pasm backend, ToaruOS libc) together with
# the 8-bit curses layer (src/curses: curses8.c, glue.c) and the zeroing calloc,
# pasm -k links them with the ToaruOS crt0/crti/libc.a/crtn from /lib.
#
# Result: /extras/new-mc/mc-toaru.bin (run it: /extras/new-mc/mc-toaru.bin), objects in obj/
#
# Every command is written out literally, without variables, $(...) and loops, so that
# esh (not a full POSIX shell) can run it.

echo "== 1. /extras/new-mc"
mkdir -p /extras/new-mc
rm -rf /extras/new-mc
mkdir -p /extras/new-mc/obj
cd /extras/mini-commander/src

echo "== 2. cmini: mini-commander sources (cmini -L /lib -c)"
/extras/cmini-toaru/bin/cmini.bin -L /lib -I cmini-toaru -I curses -D TOARU_CMINI=1 -D calloc=cm_calloc -c /extras/new-mc/obj/mc.o mc.c
/extras/cmini-toaru/bin/cmini.bin -L /lib -I cmini-toaru -I curses -D TOARU_CMINI=1 -D calloc=cm_calloc -c /extras/new-mc/obj/cmd.o cmd.c
/extras/cmini-toaru/bin/cmini.bin -L /lib -I cmini-toaru -I curses -D TOARU_CMINI=1 -D calloc=cm_calloc -c /extras/new-mc/obj/operations.o operations.c
/extras/cmini-toaru/bin/cmini.bin -L /lib -I cmini-toaru -I curses -D TOARU_CMINI=1 -D calloc=cm_calloc -c /extras/new-mc/obj/dialog.o dialog.c
/extras/cmini-toaru/bin/cmini.bin -L /lib -I cmini-toaru -I curses -D TOARU_CMINI=1 -D calloc=cm_calloc -c /extras/new-mc/obj/filelist.o filelist.c
/extras/cmini-toaru/bin/cmini.bin -L /lib -I cmini-toaru -I curses -D TOARU_CMINI=1 -D calloc=cm_calloc -c /extras/new-mc/obj/init.o init.c
/extras/cmini-toaru/bin/cmini.bin -L /lib -I cmini-toaru -I curses -D TOARU_CMINI=1 -D calloc=cm_calloc -c /extras/new-mc/obj/panel.o panel.c
/extras/cmini-toaru/bin/cmini.bin -L /lib -I cmini-toaru -I curses -D TOARU_CMINI=1 -D calloc=cm_calloc -c /extras/new-mc/obj/ui.o ui.c
/extras/cmini-toaru/bin/cmini.bin -L /lib -I cmini-toaru -I curses -D TOARU_CMINI=1 -D calloc=cm_calloc -c /extras/new-mc/obj/view_edit.o view_edit.c
/extras/cmini-toaru/bin/cmini.bin -L /lib -I cmini-toaru -I curses -D TOARU_CMINI=1 -D calloc=cm_calloc -c /extras/new-mc/obj/progress.o progress.c

echo "== 3. curses layer, glue, zeroing calloc"
/extras/cmini-toaru/bin/cmini.bin -L /lib -I curses -I /extras/cmini-toaru/include -c /extras/new-mc/obj/curses8.o curses/curses8.c
/extras/cmini-toaru/bin/cmini.bin -L /lib -c /extras/new-mc/obj/glue.o curses/glue.c
/extras/cmini-toaru/bin/cmini.bin -L /lib -I /extras/cmini-toaru/include -c /extras/new-mc/obj/calloc_zero.o calloc_zero.c

echo "== 4. link (pasm -k, ToaruOS crt0/crti/libc.a/crtn)"
cd /extras/new-mc/obj
/extras/cmini-toaru/bin/pasm.bin -k /extras/new-mc/mc-toaru.bin /lib/crt0.o /lib/crti.o mc.o cmd.o operations.o dialog.o filelist.o init.o panel.o ui.o view_edit.o progress.o curses8.o glue.o calloc_zero.o /lib/libc.a /lib/crtn.o
ls -l /extras/new-mc/mc-toaru.bin
echo "== done: /extras/new-mc/mc-toaru.bin"
