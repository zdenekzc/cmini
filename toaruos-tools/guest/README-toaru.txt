cmini in C for ToaruOS
=====================

What this is: the cmini compiler and the pasm assembler, mostly in C. Any other C
compiler is enough — make, g++ and C++ sources are not here.

Verified on Fedora 45 (gcc, x86_64):  sh build.sh  =>  cmini.bin
and it compiled and ran a program (for, while, arithmetic, results identical
to gcc). It has not run on ToaruOS yet - this package is for that.


Steps inside ToaruOS
--------------------
  1. msk install build-essential        # gcc 10.3 + binutils (make is not in the ISO)
  2. tar xzf cmini-src.tar.gz ; cd cmini-src
     (if xzf does not work: gunzip cmini-src.tar.gz && tar xf cmini-src.tar)
  3. sh build.sh                # => cmini.bin and pasm.bin
  4. ./cmini.bin -I include -o pokus.bin pokus.c && ./pokus.bin
  5. ./pasm.bin -e -T muj.asm   # image with base 0x3F000000, i.e. runnable


How it gets inside
------------------
  host:      ./setup-toaruos.sh --push cmini-src.tar.gz ; ./setup-toaruos.sh --run
  guest:     insmod /mod/ext2.ko
             mkdir -p /mnt/share
             mount ext2 /dev/hda,rw /mnt/share      # ",rw" belongs to the device

Results (cmini.bin, logs) go OUT of the machine by other means than this disk:
ata.ko has a write-back cache that is not flushed on shutdown, and sync() in the
ToaruOS libc is /* TODO */, so the files on the disk stay empty. The most reliable
is the serial console — the output of the files arrives at the host in the log.


What will probably be solved once on ToaruOS
--------------------------------------------
  - to link programs cmini calls `gcc -no-pie ... -lm`; the switch for it is
    CPARSER_CC (e.g. CPARSER_CC=cc). -no-pie is supported by gcc since version 6, i.e. also 10.3
    from build-essential.
  - our own ELF writer (asm/code.c) writes ET_EXEC; the ToaruOS kernel
    (kernel/misc/elf64.c) accepts both ET_EXEC and ET_DYN and does not check e_machine.
  - JIT paths (asmjit) are not here - ToaruOS has no mprotect, so nothing
    mapped as EXEC after writing either.
  - process functions (fork/execvp/pipe/dup2/system) are in the ToaruOS libc, so
    a driver using fork+exec should work.


State of the sources (why there is an older set of files)
---------------------------------------------------------
The current tree of the earlier development tree (cecko) can be compiled only through g++:
  1. cmini/decl.cc used Register, although the enumerator had been renamed to Register_
     because of a clash with the type Register from asm/reg.h (a comment at
     cmini/decl.h:138 points this out). I rewrote three places to Register_.
  2. After that cmini.bin -C (g++) could be built, but it did not turn the new sources into C:
       - it does not handle the C++ attributes [[noreturn]] (asm/def.h) and [[maybe_unused]],
       - it does not handle a decimal constant with the suffix LL ("Bad number"),
         the own include/stdint.h lacks INT64_MIN / INT32_MIN,
       - for a declaration with an initializer (ExprRec a = {0};) it generates
         "struct ExprRec" - and def.h makes typedef struct { ... } ExprRec; without a
         tag (def.h:109), so "struct ExprRec" is an incomplete type and the compilation
         fails on hundreds of lines. Without an initializer it behaves correctly,
         so it fails only on the new lines with "= {0}".
     These are gaps to fill in cmini.bin -C, not in those sources themselves.
     The scripts I tried it with are in the regenerate/ directory.

So the package contains a set that is already C (asm/*.c including, instr_drv.c + sem.c)
and that compiles without g++.


Where the source is (since 2026-11-11)
--------------------------------------
This package is not a copy put aside somewhere — it is assembled again every time
from the current tree cecko (asm/, cmini/, include/) and the hand-written files from
cecko/toaruos-tools/guest/ (build.sh, pokus.c, this README, regenerate/):

    cd toaruos-tools && ./mkpkg.sh            # mkguest.sh => _build/guest-src/
                                              # => _build/push/cmini-src.tar.gz
    ./check.sh                                # regression tests without QEMU (27 checks)

The other end of the same channel: in cecko/ the regression
    make test-toaru        # part of `make test-asm`
which checks the same as T1b (match with probe/toaru64.bin, base without -T and
rejection of -T without -e), only with pasm.bin built through make and with the project's CFlags.

What is in the state of the sources (2026-11-10, added 2026-11-11)
-------------------------------------------------------------------
It is a SNAPSHOT, not a living branch. Since then the cecko/ tree has diverged — it
has different contents, e.g. cmini/{decl,out,stat,instr_drv,lex,translate}.c, because
cecko/ additionally contains a layer of .cc sources. Changing it therefore requires
checking again that it compiles without g++ (check.sh T1), not just overwriting files.

Of the files in asm/, the snapshot differed only in code.c: cecko/ additionally distinguishes the
.eh_frame section and has a longer comment at the base address. The other
files the -T switch touches — {pasmlib,pasm,def}.c and def.h — were byte-identical
before the transfer, so the package was overwritten by the same version as in the tree,
and only two smaller pieces went into code.c (the definition of ElfBase64Toaru and the choice of
base in OpenElf). The rest of code.c remains a snapshot — .eh_frame is not here.

The -T switch (ToaruOS target, base 0x3F000000) IS in this set since 2026-11-11.
Before, it was only in the second tree, cecko/asm/prod.cc; here the behavior was
transferred into files that the package already contained:

    asm/code.c      ElfBase64Toaru 0x3F000000 + choice of base in OpenElf
    asm/pasmlib.h   PASM_TOARU (flag 32)
    asm/pasmlib.c   toaru is also set from the environment PASM_TARGET=toaru
    asm/pasm.c      the -T switch (only together with -e), help
    asm/def.h/.c    bool toaru + the error "ToaruOS target requires USE64 segments"

build.sh therefore builds pasm.bin in addition to cmini.bin, and inside the host it is enough to run

    ./pasm.bin -e -T prog.asm       =>    prog.bin with base 0x3F000000

WITHOUT -T the base stays 0x400000, which does not run on the host — it crashes before the first
syscall; the reasons and measurements are in ../README.md §0 and §3b.

What is NOT YET VERIFIED: running inside ToaruOS. The byte match is verified on Linux —
the image from pasm.bin -e -T probe/toaru64.asm is identical to probe/toaru64.bin, which
ran on the host on 2026-11-10 (check.sh T1b). That gcc 10.3 builds it there from build-essential
and that it can be run there, nobody has repeated yet.
