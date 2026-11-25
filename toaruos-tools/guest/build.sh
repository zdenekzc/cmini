#!/bin/sh
# build.sh - builds cmini.bin and pasm.bin from C sources. No make, no C++, no g++.
#
# Two programs are produced: cmini.bin (C compiler) and pasm.bin (assembler).
# pasm.bin also supports -T = ToaruOS target, i.e. ELF64 with base 0x3F000000; without -T
# it writes base 0x400000, which does not run on the host (SIGSEGV before the first
# syscall). Pasm is built here precisely because of -T - inside the host it is enough to run
#   ./pasm.bin -e -T prog.asm     =>   prog.bin runnable on the host
#
#   Linux:     sh build.sh
#   ToaruOS:   msk install build-essential    # gcc 10.3 + binutils (make is not there)
#              sh build.sh
#
# Every command is written out literally and without variables / $(...) - deliberately, so that it
# can be read even in esh, which is not a full POSIX shell.

mkdir obj

# 1) pasm assembler as a library
gcc -O1 -std=gnu11 -w -Icmini -Iasm -c asm/pasmlib.c -o obj/a_pasmlib.o
gcc -O1 -std=gnu11 -w -Icmini -Iasm -c asm/code.c -o obj/a_code.o
gcc -O1 -std=gnu11 -w -Icmini -Iasm -c asm/def.c -o obj/a_def.o
gcc -O1 -std=gnu11 -w -Icmini -Iasm -c asm/expr.c -o obj/a_expr.o
gcc -O1 -std=gnu11 -w -Icmini -Iasm -c asm/inst.c -o obj/a_inst.o
gcc -O1 -std=gnu11 -w -Icmini -Iasm -c asm/key.c -o obj/a_key.o
gcc -O1 -std=gnu11 -w -Icmini -Iasm -c asm/oth.c -o obj/a_oth.o
gcc -O1 -std=gnu11 -w -Icmini -Iasm -c asm/asmlist.c -o obj/a_asmlist.o
ar rcs libpasm.a obj/a_pasmlib.o obj/a_code.o obj/a_def.o obj/a_expr.o obj/a_inst.o obj/a_key.o obj/a_oth.o obj/a_asmlist.o

# 2) cmini compiler: driver, front-end, instruction generator, own runtime
gcc -O1 -std=gnu11 -w -DUSE_LEXER -Icmini -Iasm -c cmini/instr_drv.c -o obj/c_instr_drv.o
gcc -O1 -std=gnu11 -w -DUSE_LEXER -Icmini -Iasm -c cmini/translate.c -o obj/c_translate.o
gcc -O1 -std=gnu11 -w -DUSE_LEXER -Icmini -Iasm -c cmini/decl.c -o obj/c_decl.o
gcc -O1 -std=gnu11 -w -DUSE_LEXER -Icmini -Iasm -c cmini/expr.c -o obj/c_expr.o
gcc -O1 -std=gnu11 -w -DUSE_LEXER -Icmini -Iasm -c cmini/stat.c -o obj/c_stat.o
gcc -O1 -std=gnu11 -w -DUSE_LEXER -Icmini -Iasm -c cmini/sem.c -o obj/c_sem.o
gcc -O1 -std=gnu11 -w -DUSE_LEXER -Icmini -Iasm -c cmini/util.c -o obj/c_util.o
gcc -O1 -std=gnu11 -w -DUSE_LEXER -Icmini -Iasm -c cmini/lexer.c -o obj/c_lexer.o
gcc -O1 -std=gnu11 -w -DUSE_LEXER -Icmini -Iasm -c cmini/instr.c -o obj/c_instr.o
gcc -O1 -std=gnu11 -w -DUSE_LEXER -Icmini -Iasm -c cminirt/cminirt.c -o obj/c_cminirt.o

gcc obj/c_instr_drv.o obj/c_translate.o obj/c_decl.o obj/c_expr.o obj/c_stat.o obj/c_sem.o obj/c_util.o obj/c_lexer.o obj/c_instr.o obj/c_cminirt.o libpasm.a -o cmini.bin -lm
ls -l cmini.bin

# 3) pasm assembler as a standalone program (switches -e, -l, -c and -T)
gcc -O1 -std=gnu11 -w -Icmini -Iasm -c asm/pasm.c -o obj/a_pasm.o
gcc obj/a_pasm.o libpasm.a -o pasm.bin
ls -l pasm.bin
