# cmini → ToaruOS: running QEMU, without a container

This directory is a separate part of the work that does not depend on containers. The second
(often non-existent) path is `toaruos/build-tools` + the cross toolchain
`x86_64-pc-toaru-gcc` — we **do not use** it, because on this machine rootless
podman does not start (see README.md §1.3, §1.4 of the earlier development tree).

Instead: the cmini compiler (which is itself just C) is compiled **directly inside
ToaruOS** using `msk install build-essential`, and all its programs are then produced by its own
ELF writer. This is also the reason why `asm/code.cc` is being changed — see point 3.

```
check.sh                REGRESSION without QEMU: mkguest.sh, builds guest-src, checks -T, compares with gcc, images
toaru-vm.sh             start/stop/log of QEMU, control via the serial line (no window)
mkguest.sh              _build/guest-src/ = asm/, cmini/ (20 files in C), include/ from the main
                        tree + guest/ - COMPILATION FOR ToaruOS, buildable inside the guest
mkpkg.sh                mkguest.sh and _build/guest-src/ => _build/push/cmini-src.tar.gz (deterministic tar)
mkshare.sh              builds _build/share.img (ext2) from the contents of the _build/push/ directory
guest-run.sh            programs from c-mini (cmini -T) run in the running VM and are compared with Linux
guest-cmini.sh          cmini is built DIRECTLY IN ToaruOS and compiles tests/examples there (§5e)
mklibc.sh               crt0.o crti.o crtn.o libc.a and /usr/include of ToaruOS from the ISO (§5g)
mkcross.sh              cmini.bin, pasm.bin for ToaruOS cross-built on the host (§5h)
guest/                  hand-written parts of the package (the rest is a copy of the tree):
  build.sh              plain gcc ("sh build.sh") => cmini.bin + pasm.bin (supports -T)
                        without make / g++ / C++ sources
  pokus.c               test program (check.sh T2)
  README-toaru.txt      procedure inside the guest + what the compiler still lacks
  regenerate/           original scripts for generating C from C++ (the earlier development tree)
probe/                  the smallest possible ELF64 with which we verify the ToaruOS loader
  probe1.S probe2.S     asm sources (gcc), build.sh compiles them to 0x3F000000
                        into _build/probe/ and copies them to _build/push/
  probe1b.elf probe2b.elf  EXACTLY THE BYTES that ran in the host on 2026-11-10
  toaru64.asm           example for OUR assembler: pasm -e -T toaru64
  openpos.asm           open() from data below / above the entry point (§5c)
  toaruc.c              program in C for cmini -T (§5d)
  toaru64.bin           result (262 B) that ran in the host on 2026-11-10 - reference
                        for check.sh T1b and make test-toaru
  counterexample/       images at 0x400000 = CONTRAST, which will NOT run in the host

_import/                ../_import/ in the cecko tree: DOWNLOADED files, outside _build (not part
                        of a backup, make clean does not delete them)
  toaruos-2.3.2.iso     release ISO of ToaruOS v2.3.2 (7.5 MB, downloaded from the
                        v2.3.2 release of the klange/toaruos project on GitHub;
                        md5 0043dba93a361b908814a03be2ba7ab2; tests/04 downloads it itself)

_build/                 GENERATED files (not part of a backup, see §0a)
  share.img             ext2 disk, which the host offers to the guest (64 MB, mkshare.sh)
  push/                 what should get inside:
    cmini-src.tar.gz    mkpkg.sh
    probe1.elf probe2.elf  probe/build.sh
    toaru64.bin         copy of probe/toaru64.bin
    hello64-toaru.bin   asm/tests/hello64.asm compiled by pasm -e -T (careful: the syscalls
                        have Linux numbers, so it does not run in the guest)
  probe/                probe1.elf, probe2.elf from probe/build.sh
  work/out.log          what the host wrote to the serial line (toaru-vm.sh)
  guest-src/            output of mkguest.sh (always from the current tree)
  guest-src-2026-11-11/ last manually maintained snapshot (formerly _preklad/, then guest-src/),
                        only for comparison
  guest-src-prebuilt/   in the package earlier: asm/pasm.bin, asm/libpasm.a (built
                        on Linux; the host builds them itself with build.sh)
```

## Summary: what worked in ToaruOS (state 2026-11-11)

ToaruOS 2.3.2 in QEMU (KVM); details in sections 1-5c.

1. **Environment and file transfer** (2026-11-10, §1, §2, §4)
   * VM without a container: `toaru-vm.sh` (QEMU `-M pc`, e1000, start without graphics),
     control via the serial line (`send`, `log`, `stop`).
   * Files inside: ext2 disk from `mkshare.sh` (`mount ext2 /dev/hda,rw`, options that
     ext2.ko in 2.3.2 can read), or `fetch http://10.0.2.2:PORT/...`
     from `python3 -m http.server` on the host.
   * Out only via the serial line / network - not the disk (cache of `ata.ko` is not emptied, measured).
2. **Loader and ABI measured with our own programs** (2026-11-10, §3b, §5)
   * `probe1`, `probe2` (gcc on the host) ran: `ET_EXEC` + `PT_LOAD`, without
     `PT_INTERP` / `PT_DYNAMIC`, **base 0x3F000000** (0x400000 => SIGSEGV
     before the first syscall).
   * Syscalls: exit 0, open 2, read 3, write 4, close 5; 4th parameter in `r10`.
   * argv / envp on the stack as in Linux (`.ktmp` is not used); the exit code does not
     reach `$?` in esh.
3. **Executable programs from our own assembler** (§5b, §5c)
   * 2026-11-10: `pasm -e -T toaru64.asm` => 262 B image ran in the host
     (text + `argv[0]`); it is the reference `probe/toaru64.bin`.
   * 2026-11-11: the kernel rejects pointers below e_entry (`open` with a path in the data
     before the code failed, `write` did not). pasm `-T` now inserts
     `JMP start` after the header; `probe/openpos.asm` in the host: open below and above entry OK / OK.
4. **Objects `.o` from pasm** (2026-11-11, §5c)
   * `pasm -c libc64.asm`, `sse64.asm` => in the host `gcc x.o -o x` with the ToaruOS libc
     => runs, and output identical to Linux (printf, puts, strlen, pointers, calls into another
     segment, SSE2 + printf %f). Nothing special for ToaruOS is needed.
5. **Compilers built directly in ToaruOS** (2026-11-11, §0b, §5c)
   * `msk install build-essential` => gcc 10.3 + binutils from the network.
   * `cmini-src.tar.gz` (from the current tree: `mkguest.sh` + `mkpkg.sh`)
     is built in the host by `sh build.sh` => `cmini.bin` and `pasm.bin`.
     Before, it did not work: gcc in ToaruOS predefines the macro `toaru` (`bool toaru;`
     => `bool 1;`), fixed by renaming to `ToaruTarget`.
   * `pasm.bin` compiled in the host produces the executable `openpos` (`-e -T`)
     and the objects `libc64`, `sse64` (`-c`, linked by the gcc there) - all runs,
     outputs identical to Linux.
6. **Programs in C from c-mini with ToaruOS syscalls** (2026-11-11, §5d)
   * `cmini.bin -T`: own runtime library with the ToaruOS syscall numbers (write 4,
     exit 0, gettimeofday 6, sbrk 10, ...), image `pasm -T`.
   * In the host `probe/toaruc.c` runs (heap via sbrk also above 64 KB, calloc,
     time) and 8 programs from `tests/examples` (dama, jezdec, avl, classes,
     strings, defaults, chars, dtors) - output identical to Linux (`guest-run.sh`).
   * Found along the way: ToaruOS does not zero pages from `sbrk` - the library zeroes them itself.
7. **cmini compiler built directly in ToaruOS** (2026-11-11, §5e)
   * cmini.bin built by the guest gcc compiles and runs programs in the host -
     libc mode (`-o`, compile and run), own library (`-T`), assert.
   * 4 differences of the ToaruOS libc were worked around: `fflush (NULL)` crashes,
     `%-7s` truncates the string, there is no `dprintf`, `.fini_array` is not called.
     `./guest-cmini.sh`: 18 programs, 14 identical, 4 only `%f`.
8. **cmini and pasm compilers for ToaruOS built on the host** (2026-11-11, §5h)
   * `mkcross.sh`: host gcc + ToaruOS headers from the ISO + the pasm linker =>
     `cmini-toaru.tar.gz`; in a clean ToaruOS (without gcc) cmini -T, cmini -L /lib,
     pasm -x, pasm -k - verify tests/15 and 16 (guest-*.out).
8b. **Static linking with the ToaruOS libc on the host** (2026-11-11, §5g)
   * linker in asm/code.c (`pasm -x`, `cmini -L`, `PasmLink`), libc.a from the ISO
     (`mklibc.sh`) => the program runs in ToaruOS without gcc and without ld.so;
     `tests/16-toaruos-libc.sh`: 20 programs, 15 identical, 5 only `%f`.
9. **Tests on the host without QEMU**
   * `make test-toaru` (5 checks): the image byte-identical with the one that ran; without `-T`
     base 0x400000; `-T` without `-e` => 2; `-T` + USE32 => error; a jump before the data
     (e_entry 0x3F000078, E9).
   * `./check.sh` (20 OK, 0 failed, QEMU skipped; ch. 84 in CHANGELOG.txt): building the package with a clean gcc, `-T` and the jump, cmini
     against gcc, cmini `-T` (T2b), image format, package and ext2, VM control.
   * with VM: `./guest-run.sh` - programs from c-mini in the host against Linux.
   * `sh tests/15-toaruos.sh` (from the root of the tree): the same for 16 programs
     from tests/examples into `_output/toaruos`, and then a **QEMU window** with the ToaruOS desktop,
     a terminal and the programs in `/tmp/prog` (control by keyboard via the QEMU monitor -
     in graphics mode the login on the serial line does not run).

**Not done / not verified**
* (solved in §5e: `cmini.bin` in the host crashed on `fflush (NULL)` in the ToaruOS libc;
  now it compiles all 18 programs from tests/examples in the host, 14 identical,
  4 differ only in the `printf %f` of the ToaruOS libc)
* (solved in §5d: `cmini.bin -T` makes programs with the ToaruOS syscalls)
* The exit code of a program in the host cannot be verified via `$?`, only by the text.

## 0c. Headers: include/ is now cmini/ownheaders/ (2026-11-14)

The main tree has the parser headers in `cmini/ownheaders/` (formerly `include/` in the root).
The packages (`mkguest.sh`, `en/mkguest.sh`, `mkcross.sh`) copy them from
`cmini/ownheaders/` and in the package they are still called `include/` — the scripts running
in ToaruOS (`-I include`) are not changed. `guest/regenerate/gen*.sh` read trees with `include/`
(the earlier development tree) and are not changed either.

## 0a. Layout of the directory (2026-11-11)

Downloaded files are in `_import/` (in the cecko tree; `cmini-files.py` does not back it up, and
`make clean` does not delete it). Generated files are in `_build/` (it can be deleted at any time).
Moved: `img/toaruos-2.3.2.iso` => `_import/`, `img/share.img` => `_build/`,
`push/` => `_build/push/`, `work/` => `_build/work/`, `probe/probe1.elf`,
`probe/probe2.elf` (output of probe/build.sh) => `_build/probe/`,
`_preklad/asm/pasm.bin`, `libpasm.a` => `_build/guest-src-prebuilt/asm/`.
Directory `_preklad/` renamed to `guest-src/` (sources of the compiler, which is built
inside the guest - nothing generated, therefore without `_`).  The reference
`push/toaru64.bin` (byte-identical with the original `probe/toaru64.bin`) is no longer a reference
file in the repository: `check.sh` T1b and `make test-toaru` check only the LOAD address
(0x3F000000 with -T, 0x400000 without -T), not the bytes (ch. 84 in CHANGELOG.txt).
The package `cmini-src.tar.gz` no longer carries the precompiled `asm/pasm.bin` and `libpasm.a`
(mkpkg.sh always leaves them out, the LEAN option was removed): 235 556 B, 108 entries.
Verified: `./check.sh` 25 OK, 0 failed, 1 skipped (VM start);
`make -C asm test-toaru` OK; `probe/build.sh` and `mkpkg.sh` write to `_build/`.
Directory `guest-src/nova-generovat/` renamed to `guest-src/regenerate/`.
The paths in the text below are adjusted to the new layout.

## 0b. guest-src is generated (2026-11-11)

The originally frozen snapshot (`_preklad/`, later `guest-src/`) contained only 5 own
files, the rest were copies of `asm/`, `cmini/`, `include/` - 76 identical to the tree
and 19 outdated (without the Windows PE/COFF, fixes of the static initialization of cmini, ...;
`-T`, `ToaruTarget` and the jump before the data had to be added to it by hand).
Therefore: the own files are in `guest/` (the source, it is backed up), and the package is assembled
by `mkguest.sh` into `_build/guest-src/` every time from the current tree
(`_build` and not `_import`: this is not foreign code taken over unchanged, but a derivative
of our tree).  `mkpkg.sh` calls `mkguest.sh` itself (`SRC=directory` => without it),
`check.sh` T1 generates guest-src into a temporary directory.

Verified: `check.sh` 27 OK, 0 failed, 1 skipped; in the guest (QEMU) the package
from the current tree builds (`sh build.sh` => `cmini.bin` 404 072 B, `pasm.bin`
163 560 B), `pasm.bin -e -T openpos.asm` => `open` below and above entry OK,
`pasm.bin -c libc64.asm` + gcc => output identical to Linux.

In the sections below (history) `guest-src/` today means `guest/` + `_build/guest-src/`.

UNRESOLVED (older, not a consequence of the move): `cmini.bin` built in the guest
crashes (Segmentation fault) when compiling `pokus.c` (`-o`); it also crashes from the
older snapshot guest-src-2026-11-11.  On Linux the same sources work (T2).


## 0. Transfer into this tree (2026-11-11) and how to check that it matches

This directory was moved from the earlier development tree `cecko/toaruos-tools`. What changed
during the transfer and why:

* **`guest-src/`** — previously the ToaruOS build was only a tar
  (`img/cmini-src.tar.gz`, `push/cmini-src.tar.gz`, 3x the same). Unpacked into
  `guest-src/`, so that a diff can be made on it and so that it is visible that it is a SNAPSHOT, not part
  of this tree (cf. `_library/asmjit` — that is a foreign library, `guest-src/` is
  our own compiler, frozen because it is built inside the guest).
  The tar is now GENERATED: `./mkpkg.sh`. Verified by unpacking and `diff -r` against the
  original tar: the day after the transfer ONE file differed (README-toaru.txt,
  with the section "Where the source is" added), the rest was byte-for-byte the same — the tar is
  responsible only for the wrapper. FROM 2026-11-11 (the `-T` transfer) it also contains:
  `asm/{code,pasmlib,pasm,def}.c`, `asm/def.h`, `build.sh` (step 3 => pasm.bin)
  and README-toaru.txt. Current package (2026-11-11): 399 063 B, 110 entries,
  md5 `efb60ce13996ee8bbb2276e5455182bf` — but the number changes with every change in
  `guest-src/`, so take it as a state, not as a checksum; the current output of
  `./mkpkg.sh` (two runs in a row give the same tar).
* **`check.sh`** — regression tests, which need neither QEMU nor the network (25 checks, see
  the output inside). It runs by itself: `./check.sh`. Since 2026-11-11 it also contains T1b:
  pasm.bin built from `guest-src/` must produce, with `-T`, the same image as
  `probe/toaru64.bin` and without `-T` remain at 0x400000.
* **`work/*.pid` and `work/in` were NOT TRANSFERRED** (today `_build/work/`) — a PID from another boot may
  be reassigned to another process and `./toaru-vm.sh stop` would kill it. `toaru-vm.sh`
  now remembers the `cmdline` (`_build/work/*.cmd`) for each PID and kills only
  its own processes; a foreign process with the same PID is left alone.
* **`push/probe1.elf`, `push/probe2.elf` (today `_build/push/`) were at 0x400000** — i.e. at an address
  that causes SIGSEGV in the host before the first syscall (§5, item 1). The original
  files are kept in `probe/counterexample/` (they are exactly the contrast evidence) and in
  `_build/push/` the images from 2026-11-11 were rebuilt by `probe/build.sh` at 0x3F000000.
  **They are NOT the same bytes that ran** — gcc may differ; the verified originals remain
  `probe/*b.elf`. Before a new deployment, rely on `check.sh` (the format), not on the fact that "it already ran".

### `-T` is in this tree since 2026-11-11 as well

Originally the ToaruOS target was not in this tree — §2b ("`pasm -e -T`") and §5b are
described in the earlier development tree `cecko`, where the image production is split
into `asm/prod.cc`. This tree has the assembler mostly in C and has no `prod.{c,cc}`,
so the BEHAVIOR was transferred, not the file split: -T got by with the files
that were already in the Makefile (the list of sources in `cmini-makefiles.py` did not change).
Where -T touches:

```
# (line numbers as of 2026-11-11; find with grep ElfBase64Toaru / PASM_TOARU / toaru)
asm/code.c:739     #define ElfBase64Toaru  0x3F000000  (+ measurement of the loader in the comment)
asm/code.c:823-824 if (toaru && ! Elf64) Abort (ToaruUse64);
                                     ElfBase = (toaru && Elf64) ? ElfBase64Toaru : ...
asm/def.h:272,347  extern bool toaru;   vyct { ..., EobjDup, ToaruUse64 }
asm/def.c:74,664   bool toaru;          "ToaruOS target requires USE64 segments"
asm/pasmlib.h:43   #define PASM_TOARU  32
asm/pasmlib.c:39   toaru is also set by the environment PASM_TARGET=toaru (call from the library)
asm/pasm.c:91      case 'T': ...        and -T only together with -e, otherwise Usage => exit 2
```

Verified in two ways: `make test-toaru` (part of `make test-asm`) for
pasm.bin from the Makefile, and `check.sh` T1b for pasm.bin from `guest-src/`.

* `pasm -e -T probe/toaru64.asm` => image **byte-identical**
  with `probe/toaru64.bin`, which ran in the host on 2026-11-10 (§5b);
* `pasm -e` without `-T` => base still `0x400000`, i.e. the Linux path
  did not change (before and after the change the outputs of `asm/tests` have identical md5);
* `-T` without `-e` => `Usage:` and exit 2; `-T` with `USE32` segments => error
  `ToaruOS target requires USE64 segments`;
* `make -C asm test` => "all tests passed"; `make test-nasm` (nasmcmp.py)
  => `OK 230 BAD 3 (of which known 6)` with exit code 0, i.e. no UNEXPECTED
  difference from nasm; `make test-nasm64` => `OK 209 EQUIVALENT 25 KNOWN 4
  ERRORS 0`.

Consequences to keep in mind:

1. `probe/toaru64.asm` **can be built here now** — `pasm -e -T` (our path,
   `make test-toaru`) and `probe/build.sh` (gcc). The FINISHED `probe/toaru64.bin`
   (262 B, 1× PT_LOAD at 0x3F000000) remains a check that it matches.
2. `guest-src/asm/` also has -T, and `build.sh` in addition to `cmini.bin` builds
   `pasm.bin`, so the compiler built inside the guest can already create the ToaruOS image.
   (Originally not verified; verified in the guest on 2026-11-11, and the collision with
   the macro `toaru` was fixed in the process - see §5c.) This includes
   §6 (gaps in `cmini.bin -C`: `[[noreturn]]`, `[[maybe_unused]]`,
   `typedef enum X X;`).
3. What works HERE, verified: `sh build.sh` => `cmini.bin`, which compiles and runs a program
   whose result equals gcc (`check.sh` T1/T2).

## 1. File transfer inside (verified)
```
cp MY_FILE _build/push/ ; ./mkshare.sh  # fills _build/share.img
./toaru-vm.sh start
./toaru-vm.sh start
# on the host (getty on ttyS1 logs in as root automatically):
insmod /mod/ext2.ko
mkdir -p /mnt/share
mount ext2 /dev/hda,rw /mnt/share        # ,rw belongs IN THE CONTROL
```
QEMU holds a write lock on the image — while the VM runs, `./mkshare.sh` cannot overwrite it
and `debugfs -n` is the only way to look into the running machine.

## 2. Back out (do NOT use the disk for this!)
`ata.ko` has a write-back cache of 4096×4 KB and it is not emptied on shutdown:
`ata_finalize()` is empty, `ATA_CMD_CACHE_FLUSH` is `#if 0`, `sync()` in the libc is
`/* TODO */` and `ioctl(fd, 0x4F03)` (IOCTLSYNC) is not called by anyone from user space in the live
ISO. Measured: after the cache was flushed only the directory entry with an empty i-node remained on the disk. So host → host **does not work with this disk**.
Send outputs via `./toaru-vm.sh log` (serial line) or `fetch`/network.

## 2b. What is new in the assembler: `pasm -e -T` (ToaruOS target) and `asm/prod.cc`

> **Which tree:** the split `code.cc` / `prod.cc` in this chapter comes from the tree
> the earlier development tree `cecko`. This one (the tree here) has no `prod.cc` —
> the assembler is mostly in C and the same BEHAVIOR was added to `asm/code.c` from 2026-11-11.
> Where exactly, and how it is compiled here: §0 ("`-T` is in this tree since 2026-11-11") and `make test-toaru`.

Done and verified on Linux (in the guest see §5b):

```
asm/code.cc   2036 -> 1298 lines   second pass, writing to segments, .obj/.lst/.inl
asm/prod.cc   786 lines            production of the IMAGE: segment addresses, ELF/ELF64 headers,
                                   program headers, sections+symbols ET_REL
asm/prod.h    OpenElf/WriteElf/CloseElf/FreeElf + ElfRipFix/CloseElfFile
asm/code.h    RepRec, RepLevel, RepStack, ThisInx, LongBytes (shared by code.cc and prod.cc)
asm/def.h     LAdd/LSub/LMul (static inline) - formerly local in code.cc
```
The split follows the question each file answers: `prod.cc` = *what kind of file this is* (addresses, headers,
writing), `code.cc` = *what is written into it* and *what is printed*. Windows PE/COFF was not moved — it is
not in the project at all (grep `COFF|IMAGE_NT|0x5A4D` is empty; "object" is Intel OMF).

The switch: `-T` only changes the **base** of the image, the format stays ELF64 `ET_EXEC`.
```
pasm -e -T file.asm            # or: PASM_TARGET=toaru pasm -e file.asm
```
* without `-e` => `Usage:` (exit 2), with `USE32` segments => error
  `ToaruOS target requires USE64 segments`
* result: `Type: EXEC`, `Entry: 0x3f0000d0`, 1× `LOAD` at `0x3f000000`,
  no `PT_INTERP`/`PT_DYNAMIC` — exactly the requirements from §3b
* the Linux variant (`-e` without `-T`) is **byte-for-byte identical** with the previous
  state (`make test-asmlist` compares with `cmp`, result "asmlist: OK … exit 35")
* ABI (syscall numbers, `r10` instead of `rcx`) cannot be handled by -T — that belongs in
  the source program, not in the assembler. The correct example is written here:
  `probe/toaru64.asm`.

## 3. Output formats in the assembler (release)
The original inventory, according to which the split was made (code.cc had 2036 lines):

| format | where | what it is |
|---|---|---|
| Object | `WriteObj`/`OpenObj`/`CloseObj` (code.cc:603,672,683) | **Intel OMF** (16bit, markers `THEADR 0x80`…), **not** PE/COFF |
| Linux ELF executable | `OpenElf`/`WriteElf`/`CloseElf` (:781,:880,:1004) | own writer, `ET_EXEC`, **1× PT_LOAD**, no PT_INTERP, base `ElfBase64 = 0x400000` (:715) |
| Linux ELF relocatable object | `CloseElfObj` (:1233) | `ET_REL`, sections + symbol tables + `R_X86_64_*`, linked by gcc |

**A Windows PE/COFF writer is not in the project at all** (grep `COFF|IMAGE_NT|0x5A4D` =
empty). So no "Win PE" can be transferred — what exists is moved:
production of the **executable image** (ELF exec) into the new `asm/prod.cc`,
and a second variant for ToaruOS will be added to it. Objects (OMF and `ET_REL`) stay in
`code.cc`, because their helpers (`PutByte`, `SendRec`, …) are, on the contrary, tied
to the listing. Conditions for compilation via `cmini.bin -C` (our C++→C tool):
no `[[noreturn]]`, no initializer for an anonymous `typedef struct`,
`(unsigned int)` instead of `(unsigned)`, `typedef enum X X;` additionally.

## 3b. What the ToaruOS loader requires (verified during running, not just read)
```
e_type      = ET_EXEC (2)          ET_DYN/PIE = -EINVAL  (elf64.c:276)
e_machine   does not matter, it is not checked
pt_load     >= 1, mapped in 4 KiB pages, p_flags and p_align are NOT checked (everything RW)
no PT_DYNAMIC and PT_INTERP        otherwise /lib/ld.so is exec'd again
BASE ADDRESS: 0x3F000000           0x400000 => SIGSEGV even before the first syscall
p_filesz <= p_memsz                bss is zeroed by you, the kernel only adds zeros
entry        = e_entry, the kernel does not require _start nor symbols
```
ABI: `syscall`; `rax`=number, parameters `rdi rsi rdx r10 r8` (max 5; the 4th is NOT in
`rcx` — with the `syscall` instruction rcx catches the return address, the kernel zeroes `rcx`
right away, `irq.S:283`); return value `rax`, errors as `-errno`, no
`exit_group`. The numbers differ from Linux (`base/usr/include/syscall_nums.h`):
`EXT(exit)=0 OPEN=2 READ=3 WRITE=4 CLOSE=5 EXECVE=7 GETPID=9 SBRK=10 SEEK=14`,
mmap does not exist as a syscall (only `sysfunc` 43). argv/envp come **on the stack**
(`[rsp]=argc`, right after it `argv[0]`, NULL, `envp[]`, NULL, auxv); the section `.ktmp`
with `kargv`/`kenvp` is the mechanism of the old toaru3, in 2.3.2 it does not occur anywhere in
the whole tree. Pitfall: syscalls with `PTR_VALIDATE` (e.g. `open` on a path) return
`-EFAULT` if the buffer lies below the address `e_entry` (`PTR_INRANGE`) — so the data
must lie higher than the entry.

## 4. Verification procedure (without a container)
```
./toaru-vm.sh start
./toaru-vm.sh send 'insmod /mod/ext2.ko'
./toaru-vm.sh send 'mkdir -p /mnt/share; mount ext2 /dev/hda,rw /mnt/share'
./toaru-vm.sh send 'msk install build-essential'
./toaru-vm.sh send 'cd /tmp; tar xzf /mnt/share/cmini-src.tar.gz; cd cmini-src; sh build.sh'
./toaru-vm.sh send './cmini.bin -I include -o /tmp/pokus.bin /tmp/cmini-src/pokus.c'
./toaru-vm.sh send '/tmp/pokus.bin; echo navrat=$?'
./toaru-vm.sh log 60
```
The order is not random: `build-essential` must come before `build.sh` calls gcc.

Quick repetition without restarting the VM (QEMU user-net maps 10.0.2.2 to the host
loopback, so it is enough to bind to 127.0.0.1):
```
python3 -m http.server 8010 --bind 127.0.0.1 -d probe &
./toaru-vm.sh send 'fetch http://10.0.2.2:8010/probe2b.elf -o /tmp/p2b; chmod 755 /tmp/p2b; /tmp/p2b first second'
```

## 5. Results of the probes (2026-11-10, QEMU + ISO 2.3.2, our ELF64 from the host's gcc)

(The binaries of 2026-11-10 printed the original Czech messages; the output below is shown in English, matching the translated sources.)
```
$ /tmp/p1b first
PROBE1: loaded, write works
argc=001
/tmp/p1b
PROBE1 done

$ /tmp/p2b first second
PROBE2: argc=003
  argv=/tmp/p2b
  argv=first
  argv=second
  env=START=--headless
  env=USER=root
  ... (8 variables)
  kargv[0]=NULL (expected: .ktmp is not used in 2.3.2)
```
Three things that are worth knowing:
1. **The kernel is enough with ET_EXEC + PT_LOAD at 0x3F000000.** A low address (0x400000,
   the default of the linker) = SIGSEGV before the first syscall. Verified also by the contrast
   with `strace`, which showed only `execve(...)` and `--- SIGSEGV ---`. Their own static binary
   `/lib/ld.so` at that lower address does not exist — it is at 0x3F000000.
2. **Parameters from the shell work without any section** — our `.ktmp` stayed
   empty and argv/envp arrived anyway. It is because the kernel passes them on the stack.
3. **The exit code does not reach `$?`** (the probe returned 7 and 9, esh reported 0).
   `sys_exit` does `task_exit(exitcode << 8)` and esh does not handle it this way — when
   testing, `$?` cannot be relied on; for example, read it from a `write` to fd 1.

The pitfall into which I fell myself and which is commented in probe2: `argv` is not a pointer to an array,
`[rsp+8]` is already `argv[0]`. The original version assumed the opposite and
failed on `strlen` over the first eight bytes taken as a string.

## 5b. Our own output: `pasm -e -T` in the guest (2026-11-10, 16:58)

(The message was printed in Czech by the 2026-11-10 binary; shown here in English.)
It is no longer a binary from the host's gcc, but an **image of our assembler**:

```
$ cd asm && make && ./pasm.bin -e -T ../toaruos-tools/probe/toaru64.asm
$ readelf -h toaru64.bin | egrep 'Type|Entry'   # EXEC, 0x3f0000d0; 1x LOAD 0x3f000000
$ # host: fetch http://10.0.2.2:8010/toaru64.bin -o /tmp/t64; chmod 755 /tmp/t64; /tmp/t64 second
TOARU64: image from pasm -e -T, loaded by the kernel
argv[0] = /tmp/t64
```
All of this has a single `PT_LOAD`, no sections, no `PT_INTERP`, 262 bytes.
The output ends after `argv[0]` without a new line - the length of the message is fixed in the `.asm`
(`msglen equ 63`), so the text and the number must be changed together.

Three things that Linux did not demand from us:
1. `msglen equ 54` as a number - `equ $ - msg` across another segment the assembler
   does not handle ("Improper operand size", `$` outside the same segment is not a constant).
2. `mov rsi, qword ptr [rsp+8]` - without `qword ptr` the same error report.
3. **data after the code** — see the comment in `probe/toaru64.asm` (PTR_INRANGE).

## 5c. pasm for ToaruOS: `.o` and executable programs (2026-11-11, verified in the host)

QEMU + ISO 2.3.2, gcc 10.3 from `msk install build-essential`:

| what | how | result in the host |
|---|---|---|
| object `.o` from the host | `pasm -c libc64.asm`, `pasm -c sse64.asm` (asm/tests), in the host `gcc x.o -o x` | runs, output identical to Linux (printf, puts, strlen, DQ OFFSET, call into another segment, SSE2 + printf %f) |
| executable, data before code | `pasm -e -T` (original state) | `write` from data below `e_entry` works, **`open` on a path below `e_entry` returns an error** (PTR_INRANGE) - `probe/openpos.asm`: `ERR` / `OK` |
| the same after the fix | `pasm -e -T` inserts `JMP start` right after the header | `open` below and above: `OK` / `OK` |
| pasm BUILT IN THE HOST | `guest-src`: `sh build.sh` => `cmini.bin`, `pasm.bin`; with them `-e -T openpos.asm` and `-c libc64.asm`, `-c sse64.asm` + gcc | everything runs, outputs identical to Linux |

Conclusions:

* **`.o` (`pasm -c`) needs nothing extra for ToaruOS** — it is an ordinary ELF64
  `ET_REL` (System V ABI like Linux), the gcc / ld there links it with their libc.
  So `-c -T` makes no sense (the linker determines the base) and stays rejected.
* **Executable (`pasm -e -T`): a jump before the data.** If some segment lies below the entry point
  (typically `data` before `code`), pasm inserts `E9 rel32` = `JMP start` after the header
  (offset 78h) and `e_entry = 0x3F000078`; all segments are then above the entry point
  and `open` / `read` into a buffer in the data work. A program that starts with code at the entry point
  (`probe/toaru64.asm`) does not get the jump — its image is byte-identical with `probe/toaru64.bin`.
  The limitation of the original comment in `toaru64.asm` ("data AFTER the code") thus fell away.
  Checks: `make test-toaru` and `check.sh` T1c (`probe/openpos.asm`).
* **Fix for compilation inside the guest:** gcc in ToaruOS predefines the macros
  `toaru`, `__toaru`, `__toaru__` (= 1) - the variable `bool toaru;` from the -T transfer
  (§0) expanded to `bool 1;` and `build.sh` in the host built neither
  `pasm.bin` nor `cmini.bin` (cmini.bin links libpasm.a).  Renamed to `ToaruTarget` in `asm/` and `guest-src/asm/`.
  With that, point 2 of §0 is also verified (gcc 10.3 from the `build-essential` package builds it).
* The esh in the host does not know `2>&1` (it creates a file `&1`) nor `tail`; read the
  output of the command via `./toaru-vm.sh log`.
* `toaru-vm.sh stop` did not kill the FIFO holder (`sleep 7200`): its cmdline was saved
  right after `&`, when opening the FIFO was still waiting for QEMU and the process was a
  subprocess of bash. Now it is saved only after QEMU starts.

Procedure (abbreviated; files via `python3 -m http.server 8010 --bind 127.0.0.1 -d DIRECTORY`):
```
./toaru-vm.sh start
./toaru-vm.sh send 'msk install build-essential'        # confirm: ./toaru-vm.sh send y
./toaru-vm.sh send 'cd /tmp; fetch http://10.0.2.2:8010/cmini-src.tar.gz -o c.tgz; tar xzf c.tgz; cd cmini-src; sh build.sh'
./toaru-vm.sh send 'fetch http://10.0.2.2:8010/openpos.asm -o o.asm; ./pasm.bin -e -T o.asm; ./o.bin'
./toaru-vm.sh send 'fetch http://10.0.2.2:8010/libc64.asm -o l.asm; ./pasm.bin -c l.asm; gcc l.o -o l; ./l'
./toaru-vm.sh log 20 ; ./toaru-vm.sh stop
```

## 5d. Program in C from c-mini with ToaruOS syscalls (2026-11-11, verified in the host)

    cmini.bin -T -o prog.bin prog.c        # or PASM_TARGET=toaru cmini.bin -o ...

`-T` (cmini/instr_drv.c, `InstrToaru` in cmini/instr.h) or the environment
`PASM_TARGET=toaru`: a standalone program with **its own runtime library** of cmini
(without glibc, like `CPARSER_RTL=own`), with the ToaruOS syscalls in it, and `pasm -T`
(image from 0x3F000000; data before code => jump `JMP start`, §5c).
Compile without `-I include` (the headers are for glibc; printf, malloc, ...
the own library knows). With `-c` (object) nothing changes — that is linked by the gcc there.

Changes to the runtime library (`RTL_TOARU` in cmini/instr.c - replaces the lines
`RTL_CODE`, it is checked that all 11 groups match):

| Linux | ToaruOS (`/usr/include/syscall_nums.h` in the host) |
|---|---|
| write 1 | 4 |
| exit 60 | 0 (`SYS_EXT`) |
| time 201 | gettimeofday 6 (`tv_sec`, also into `*t`) |
| brk 12 (returns the new end) | sbrk 10: increment in 4 KiB steps, returns the OLD end |
| getpid 39, kill 62 | 9, 37 |

**Pitfall: ToaruOS does not zero pages from `sbrk`** (`kernel/sys/syscall.c`
`sys_sbrk` only does `mmu_frame_allocate`; measured: in `calloc (300000, 1)` there were
126 108 non-zero bytes).  Linux `brk` zeroes and the cmini library relies on it
(calloc, new) - the ToaruOS version therefore zeroes the new pages (`rep stosb`).
The start of the heap is the end of the highest `PT_LOAD` rounded up to a page (`elf64.c`:
`image.heap = (heapBase + 0xFFF) & ~0xFFF`; measured with the probe
sbrk: 0x3F001000 for an image of 0x124 B).

Result in the host (`./guest-run.sh`, VM running, gcc in the host not needed):

```
ok    toaruc - output in ToaruOS == Linux (5 lines)     probe/toaruc.c: list of 20000 items, calloc 300000, time
ok    dama - output in ToaruOS == Linux (93 lines)
ok    jezdec, avl, classes, strings, defaults, chars, dtors   (tests/examples)
guest-run: OK
```

The programs are compiled on the host (cmini.bin built in the host so far crashes,
see §0b).  Check without VM: `check.sh` T2b (image from 0x3F000000, `JMP`,
11 lines of the library for ToaruOS, no Linux exit / brk).

## 5e. cmini compiler directly in ToaruOS (2026-11-11, verified in the host)

`cmini.bin` built in the host (`guest/build.sh`, gcc 10.3) compiled
`-T` (own library) even earlier; it crashed in the mode with libc and at "compile and run" (§0b).
The cause and other differences of the ToaruOS libc from glibc — all measured with small
programs from the guest gcc:

| ToaruOS 2.3.2 libc | consequence in cmini | fix |
|---|---|---|
| `fflush (NULL)` => Segmentation fault | crash before every `fork` (gcc, program start) | `fflush (stdout); fflush (stderr);` (cmini/instr.c) |
| `printf ("%-7s", "init_array")` => `init_ar` (the width truncates the string, C: only the minimum) | text for pasm `init_ar segment` => section `.data.init_ar`, constructors of global objects were not called | `TextPad` instead of `%-8s` / `%-7s` (asm/asmlist.c) |
| `dprintf` does not exist | linking of every program failed | RT_ASSERT, RT_PURE: `fflush (stdout)` + `write (2, ...)` |
| `.fini_array` is not called on exit (not even for programs from gcc), `atexit` yes | destructors of global objects were missing | CINIT registers `atexit (CFINI)`, `.fini_array` is not generated |
| `printf ("%f")` without the default precision (2.5 instead of 2.500000) | floats, pointers, sequence, dictionary differ from the pattern | not fixed - libc |
| `sscanf ("%hd")` does not fill the short (returns one less) | a test program with `%hd` (not kept in the tree), verified with a program from the ToaruOS gcc | not fixed - libc |

Result (`./guest-cmini.sh`): cmini.bin in the host compiles all 18 programs
from tests/examples (with `-I include`, ToaruOS libc, gcc in the host), 14 have output
identical to the pattern (dama, jezdec, avl, btree 764 lines, veze, classes, strings,
defaults, chars, dtors, objcopy, dyncast, quicksort, heapsort), 4 differ only
in `%f`.  All modes work: `-o` (libc), without `-o` (compile and run, `main
returned N`), `-T` (own library), `assert` (message + abort).

## 5g. Static linking with the ToaruOS libc already on the host (2026-11-11, verified in the host)

Programs that do not need gcc or ld.so in ToaruOS:

    ./mklibc.sh                                   _build/toaru-lib/ from the ISO
    PASM_LIBDIR=_build/toaru-lib pasm -x p.asm    => p.bin
    cmini -L _build/toaru-lib -I include -o p.bin p.c
    sh tests/16-toaruos-libc.sh                    (from the root of the tree) _output/toaruos-libc

* The ISO contains in the ramdisk of the base system (ISO => `fat.img` => `ramdisk.igz`
  => tar) `/lib/crt0.o`, `crti.o`, `crtn.o` and a **static `libc.a`** (166 objects,
  including `sqrt`, `qsort`) — no package from `msk`.
* **Linker in asm/code.c** (section LINKER, `PasmLink` in pasmlib.h): ELF64 objects
  + archives, relocations 64 / PC32 / PLT32 / 32 / 32S / PC64 / GOTPCREL(X) (libc.a
  uses REX_GOTPCRELX => its own GOT), the only PT_LOAD from 0x3F000000.
* **Pitfall: nothing may lie below `e_entry`.**  A reference program linked by GNU ld
  (`-static -Ttext-segment=0x3F000000`) had `.rodata` in the first segment below
  `.text` — in the host it HUNG (no output; the kernel rejects pointers below the entry
  point - PTR_INRANGE, §5c - and write in libc repeats the error).  `ld -N` (text
  first) and our linker (`_start` = 0x3F000080, everything above it) run.  The static
  `/lib/ld.so` from ToaruOS has `_start` also exactly at the beginning.
* Start without ld.so: crt0 `_start` => `pre_main` (libc.a): `__get_argv ()` is NULL
  => it stores argv, calls `__init_array_start .. __init_array_end` (the linker puts libc first
  there — its `_libc_init` from `.ctors` prepares stdio —, then the constructors of the program),
  `_init`, `main`, `exit`.
* Result of `tests/16-toaruos-libc.sh` (clean host, `ls /usr/bin/gcc` - not present):
  20 programs (18 from tests/examples including floats and pointers + asm/tests libc64,
  sse64), 15 identical to the pattern, 5 only `%f` of the ToaruOS libc.

## 5h. Compilers for ToaruOS built on the host (cross build, 2026-11-11)

    ./mkcross.sh       _build/cross/cmini and _build/push/cmini-toaru.tar.gz
                       (+ source package _build/push/cmini-src.tar.gz)

`cmini.bin`, `pasm.bin` and `cminirt.o` for ToaruOS are compiled ON THE
HOST with the host gcc and the ToaruOS headers (`mklibc.sh` extracts `/usr/include` from the ISO
=> `_build/toaru-include`) - `-nostdinc`, without `__linux__`,
with `__toaru__` / `toaru` as the ToaruOS gcc, `-fno-pie -fno-stack-protector
-fcf-protection=none`; implicit declarations only warn (gcc 14+ takes them as an error,
the ToaruOS headers e.g. do not declare `chmod`) - and they are linked by the pasm linker
(`pasm -k`, a new mode, linking only) with the ToaruOS `libc.a`.  In a clean ToaruOS
(without gcc, without ld.so) the whole chain then works:

    cd /tmp/cmini
    bin/cmini.bin -T -o /tmp/d tests/examples/dama.cc          own library
    bin/cmini.bin -L /lib -o /tmp/c tests/examples/classes.cc  ToaruOS libc from /lib
    export PASM_LIBDIR=/lib; bin/pasm.bin -x tests/libc64.asm

(`/lib/crt0.o`, `crti.o`, `crtn.o`, `libc.a` are in the base system of every
ToaruOS.)  `tests/15-toaruos.sh` and `16-toaruos-libc.sh` (shared part
`tests/toaruos-common.sh`) send to the running VM, in addition to the examples,
`cmini-toaru.tar.gz` (=> `/tmp/cmini`) and the sources (=> `/tmp/src`, gcc compiles them
from `msk install build-essential`, §5e) and verify the 3 steps above directly
in the host (`_output/toaruos*/cmini/guest-*.out`: dama, classes, libc64 -
all identical to the pattern).  In the window (test 15) `run.sh` fetches them and prints the instructions;
verified also by hand in the terminal (cmini -T jezdec.c => O.K.).

## 6. Is C++→C converter `cmini.bin -C` sold through us?
`prod.cc` **yes** (21 kB of C, `cmini.bin -C` converted it without a report),
but `asm/def.h` throws it away — and that was already long before this change:

| obstacle | where | verification |
|---|---|---|
| `[[noreturn]]` | `def.h` (Message/AsmError/Abort/StopCompile) | the old `code.cc` from the backup failed with the same headers too |
| `[[maybe_unused]]` | `code.cc` (function `Comment`) | old and new state, the old one at line 348 |
| C++ style `enum symbol` without `typedef enum symbol symbol;` | `def.h`, `expr.h`, `oth.h` | the converted `prod.c` failed on `gcc -std=c99` only with 8 reports `unknown type name`: AlignTyp, ClassTyp, CombiTyp, ErrTyp, ExprSort, FixTyp, Intel, symbol |

`prod.cc` itself brought no new obstacle (no template, no
reference, no `new`, no initializer for an anonymous `typedef struct`).
If those three attributes were wrapped in a macro and `typedef enum X X;` added, the translation of the
assembler cecko => C would be one step closer.

So far `asm/*` is compiled by `g++` according to a hand-written `makefile`;
the generated `asm/Makefile` does not fit the script `python3 cmini-makefiles.py` —
it reports it itself `SKIP asm/Makefile (the directory has a hand-written makefile)`,
because GNU make prefers the smaller `makefile`. The sources `prod.c` /
`prod.cc` are therefore written where they are really used: `asm/makefile`
(`LIBSRC`), `cmini-makefiles.py` (in case someone deleted the hand-written one),
`tests/build-translate.sh` and `tests/02-cmini-pasm.sh` (the path "assembler
translated by c-mini").
