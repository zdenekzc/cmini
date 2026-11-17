PASM - i386 assembler from EASM, translated from Pascal to C
============================================================

Source:    Free Pascal, -Mtp
           the machine-code conversion was only a guide
           (it cannot be compiled)

Build:     make              pasm.bin and the library libpasm.a (pasmlib.h)
Run:       ./pasm.bin [-e] [-l] [-c] [-T] [-x] [-w] [-m] [-o] [-i] file.asm
                             (no switch: -e)
Tests:     make test         tests/run.sh - without stored patterns (outputs
                             in ../_build/asm):
                             16 and 32 bit: all outputs (.obj, .lst, .inl, ELF,
                             messages, return code, run of hello32) == the
                             original Pascal version
                             (built with fpc -Mtp;
                             PASM_REF=program);
                             x86-64: run of hello64, libc64, sse64 == the same
                             programs in nasm syntax (tests/nasm/*.nasm)
           make test-nasm    encoding of 227 instructions 8086 .. 386 against NASM
           make test-nasm64  encoding of x86-64 instructions against NASM (bits 64)

Files
-----
key.h, key.c     keywords (generated from key.pas)
def.h, def.c     shared types, variables, name trees, error messages
expr.h, expr.c   reading the source, lexical analysis, expressions
oth.h, oth.c     output records (ResultRec), directives
inst.h, inst.c   instructions 8086 .. 386, 8087 .. 387
code.h, code.c   translation, outputs .obj (OMF), .bin (ELF), .lst, .inl
pasm.c           main program (command line) - calls PasmAssemble
pasmlib.h, pasmlib.c  assembler as a library (cparser: ../cmini/instr.cc)
reg.h             enumeration of Register and RegTable (C and C++)
asmlist.h, asmlist.c   program as a linked list of instructions (plain C,
                  phases 1 and 2 - see asmlist.txt; make test-asmlist); phase 2 =
                  PasmAssembleList (pasmlib.h) - instructions and labels are read
                  by pasm directly from the nodes (expr.c NodeOperand, inst.c
                  CompileNode); used by ../cmini/instr.cc (interface with
                  pointers AsmP..., AsmInsP - also for cparser)

Translation
-----------
C style (translated with g++): functions, structures, char [] strings, stdio,
setjmp/longjmp.
- types: small = uint8_t, Pascal int (0..$FFFF) = card (uint16_t),
  long = longint (int32_t, arithmetic with wrap-around via uint32_t),
  shr = logical shift
- renamed (C++ keywords): short -> short_, near -> near_, far -> far_,
  default -> default_, terminate -> terminate_, class -> cls, register -> Register,
  procedure Float -> Float_
- variant records (ExprRec, ResultRec) - all variants as separate items
- strings: column and start remain numbered from 1 (error messages, listing),
  LineBuf [column] in Pascal = LineBuf [column-1] in C
- Assign replaced by the file names ModName, ObjName, ElfName, LstName, PasName
- eof (source) as in FPC: SourceEof (expr.c)
- the original behaviour is kept deliberately, including its quirks (long
  identifiers, empty case branches, ...); the outputs must be byte-identical

Verification
------------
- make test: all 22 outputs identical to the patterns
- encoding of 227 instructions against NASM:
  same result as the Pascal version (6 known differences)
- all tests with all combinations of switches and 1500 randomly corrupted
  sources: outputs, messages and return code identical to the original pasm.bin


X86-64 (extension to the Pascal version)
========================================

Usage
-----
        .x64                        ; processor x86-64 (includes .386)
code    segment use64               ; 64-bit segment
        ...
        pasm -e -l program          ; ELF64 for Linux x86-64 (see tests/hello64.asm)

- segment USE64 only with output -e (ELF64) or -l; .obj (OMF), .inl and .COM
  cannot have 64-bit segments; USE32 and USE64 cannot be mixed in ELF
- ELF64: header 64 + 56 bytes, loaded from 400000h (R W X), kernel services
  via SYSCALL (rax = number: 1 write, 60 exit; rdi, rsi, rdx = parameters)

Registers
---------
RAX .. RDI, R8 .. R15 (64 bit), R8D .. R15D, R8W .. R15W, R8B .. R15B,
SPL, BPL, SIL, DIL - only in a USE64 segment.
AH, BH, CH, DH cannot be used in an instruction with a REX prefix (error).

Encoding
--------
- REX prefix (40h..4Fh) right before the opcode: W - 64-bit operand, R -
  ModRM.reg, X - SIB.index, B - ModRM.rm / SIB.base / register in the opcode
- implicit operand size 32 bit (default_ = dword), 64 bit => REX.W, 16 bit =>
  66h; implicit address size 64 bit, [EAX..], [R8D..] => 67h
- address:
     [label]              RIP-relative (mod 00, rm 101, disp32 = target - end
                          of instruction; for a direct value after the address
                          its length is subtracted)
     [number]             absolute (SIB without base and index)
     [rbx + r12*8 + 10h]  registers RAX .. R15 (RSP cannot be an index, RBP and
                          R13 as base always with a displacement, RSP and R12
                          always with SIB)
  a RIP-relative reference to another segment is computed only in ELF output
  (linear addresses of segments), otherwise it stays in the listing as a
  reference (R)
- segment prefixes: only FS and GS (ES, CS, SS, DS are not used in 64-bit
  mode, ASSUME is not needed)
- constants: expressions are computed in parallel in 32 bits (val, original
  behaviour) and in 64 bits (val64); in a USE64 segment the 64-bit value applies
  (e.g. 100000000h / 10h = 10000000h), division by zero and comparison use 64 bits
- MOV r64, imm: a value within the signed 32-bit range => REX.W C7 /0 imm32,
  otherwise REX.W B8+r imm64 (e.g. MOV RAX, 123456789ABCDEF0h, MOV RCX, 0FFFFFFFFh)
- other instructions with a 64-bit operand (ADD, CMP, PUSH, MOV [..], ...):
  a direct 32-bit value is sign-extended, a larger one => "Value is out of range"
- a displacement in an address is at most 32 bits signed
- DQ in a USE64 segment: 64-bit value (DQ 123456789ABCDEF0h), 16 hex digits in
  the listing; in USE16 / USE32 segments unchanged
- PUSH / POP: 64-bit operand (without REX.W) or 16 bit, PUSH number => 64 bit;
  a 32-bit operand is an error
- INC / DEC register: FE / FF (40h..4Fh are REX prefixes in x86-64)
- JMP / CALL register64 or QWORD PTR [..]; a direct far jump does not exist
- LOOP, JRCXZ - RCX; JECXZ - ECX (67h); JCXZ does not exist
- MOV accumulator, [address] is not encoded as A0..A3 (in x86-64 the address
  should be 8 bytes)

New instructions: CDQE, CQO, MOVSXD, SYSCALL, JRCXZ, LODSQ, STOSQ, MOVSQ,
SCASQ, CMPSQ (and LODS, MOVS ... with a QWORD operand, MOVSX / MOVZX r64).

Not available in a USE64 segment: AAA, AAD, AAM, AAS, DAA, DAS, INTO, BOUND, LDS,
LES, ARPL, JCXZ, PUSHA(D), POPA(D), PUSH / POP ES, CS, SS, DS, far direct jump.

Verification
------------
- make test: the original outputs (16 and 32 bit) are unchanged, randomly
  corrupted sources (1500) still give the same outputs as the original pasm.bin
- make test-nasm64: 172 x86-64 instructions against NASM - 139 identical
  bytes, 25 equivalent encodings (operand direction, register order in an
  address, FWAIT), 5 known differences (MOV r64, imm32 and imm64 instead of
  MOV r32; XCHG)
- tests/hello64.asm: ELF64 runs (RIP-relative addresses into another segment,
  forward references, a direct value after a RIP-relative address, SYSCALL,
  CQO, IDIV, MOVSXD, REP MOVSB, R8 .. R15, MOV r64, imm64, DQ and EQU above
  32 bits)

Fix (when translating programs from ../instr.cc): a short jump (EB, 7x) is
chosen according to the distance from the end of the 2-byte instruction
(RelTiny: ip + 2, originally ip + 1 - a backward jump of 129 bytes was chosen
as short and reported "Jump too long"). Tests, comparison with NASM and 1500
random inputs give the same results as before.


RELOCATABLE ELF64 OBJECT (switch -c)
====================================

        pasm -c -l program.asm      => program.o  (x86-64, USE64 segments)
        gcc -no-pie program.o -o program        (glibc, e.g. printf)

- each segment is a PROGBITS section: a name containing CODE or TEXT => .text
  (AX), otherwise .data (WA); further segments of the same kind .text.<name>,
  .data.<name>; segment EH_FRAME => .eh_frame (A, alignment 8, CFI for unwind);
  alignment according to ALIGN; .note.GNU-stack
- relocations (.rela<section>, Elf64_Rela):
     R_X86_64_PC32   [label] relative to RIP into another segment / external,
                     near jump into another segment
     R_X86_64_PLT32  CALL / JMP to an external symbol
     R_X86_64_64     DQ OFFSET x
     R_X86_64_32     DD OFFSET x, OFFSET x in an instruction (only -no-pie)
- symbols: sections (local), EXTRN (undefined), PUBLIC (global, also constants
  EQU - SHN_ABS)
- name for the linker (upper / lower case, and words that are keywords for the
  assembler):   EXTRN X1: NEAR 'printf'     PUBLIC FMAIN 'main'
  without a string the identifier is used as written
- relocations inside DUP are not allowed; neither are GROUP, SEG and far jumps
- test: tests/libc64.asm (puts, strlen, printf, DQ pointers, call into another
  segment) - it is assembled, linked by gcc and run (make test)


SSE, SSE2 - XMM REGISTERS AND SCALAR FLOATING POINT
===================================================

XMM0 .. XMM15 registers (a new kind of operand xmm, ExprRec.ceil = number);
XMM8 .. XMM15 require REX - only in a USE64 segment. An XMM register must not
be used in an address ([xmm0] => "Bad base register").

Encoding: the mandatory prefix (F2 - double, F3 - float, 66 - pd / integer
operations) is written before REX, then 0F xx and ModRM (reg = XMM or general
register, rm = XMM, register or memory; addresses as for the other
instructions, also RIP-relative).

Instructions:
  MOVSD, MOVSS          xmm, xmm/m      m, xmm
                        (MOVSD without operands remains the string instruction)
  MOVAPD, MOVAPS        xmm, xmm/m128   m128, xmm   (memory aligned to 16)
  MOVQ                  xmm, r64/m64    r64, xmm    xmm, xmm    m64, xmm
  MOVD                  xmm, r32/m32    r32/m32, xmm
  ADDSD SUBSD MULSD DIVSD SQRTSD MINSD MAXSD          xmm, xmm/m64
  ADDSS SUBSS MULSS DIVSS SQRTSS MINSS MAXSS          xmm, xmm/m32
  UCOMISD COMISD (m64), UCOMISS COMISS (m32)          xmm, xmm/m
  CVTSI2SD CVTSI2SS     xmm, r/m32 or r/m64 (REX.W)
  CVTTSD2SI CVTSD2SI    r32 or r64, xmm/m64
  CVTTSS2SI CVTSS2SI    r32 or r64, xmm/m32
  CVTSD2SS (m64), CVTSS2SD (m32)                      xmm, xmm/m
  ANDPD ANDNPD ORPD XORPD ANDPS ANDNPS ORPS XORPS PXOR  xmm, xmm/m128
The memory operand size is checked (QWORD PTR for ...SD, DWORD PTR for ...SS;
without PTR is also correct); for 128-bit operations it is not checked
(XMMWORD does not exist). Floating-point constants in data: DQ 400C000000000000h
(3.5), DD 3FC00000h (1.5) - or the original notation ...R.

Verification: make test-nasm64 - 69 SSE / SSE2 instructions (all forms, XMM8 ..
XMM15, addresses with SIB and REX) byte-identical with NASM, in total OK 208,
equivalent 25, known differences 5, errors 0; tests/sse64.asm - ELF64 object
with glibc (printf %f): sum, product, square root, conversions, comparison, abs
(ANDPD), MINSD, MAXSD - make test.

.init_array and .fini_array sections (-c): a segment named INIT_ARRAY or
FINI_ARRAY is written as the section .init_array / .fini_array (type
SHT_INIT_ARRAY 14 / SHT_FINI_ARRAY 15, WA, alignment 8):
        init_array segment use64 para
                dq      offset constructors     ; glibc calls before main
        init_array ends
(cparser - constructors and destructors of global objects of each object).



KNIHOVNA (pasmlib.h)
====================

        #include "asm/pasmlib.h"
        char msg [512];
        if (PasmAssemble ("prog.asm", "prog", PASM_ELFOBJ | PASM_LST, msg, sizeof (msg)) != 0)
           ... error, msg = "prog.asm(radka,sloupec) Error: text"

- text from memory (without a .asm file):
        const char * parts [] = { "...", "..." };     /* text parts in sequence */
        PasmAssembleText ("prog.asm", parts, 2, "prog", PASM_ELFOBJ, msg, sizeof (msg));
  expr.c reads the source via SrcGetc / SrcUngetc: the file ModName, or parts
  of the text (SetSourceText); "prog.asm" is only a name for messages
- flags = PASM_OBJ (-o), PASM_EXE (-e), PASM_LST (-l), PASM_INL (-i),
  PASM_ELFOBJ (-c); outputs base.obj, .bin, .o, .lst, .inl
- Compile () is called as from the pasm program: errors via setjmp / longjmp
  (AbortJump), without exit; Init and InitRes set the state at the start of each
  translation, they can be called repeatedly (memory is not freed)
- C linkage (extern "C") - it is also called from C (cmini/instr.c from
  translate.c); the header is also read by cparser (../cmini/instr.cc)
- renamed because of collisions with cparser:  sy => CurSy,
  Search => SearchTree, Error => AsmError
- make libpasm.a (pasmlib.c, code.c, def.c, ...; without pasm.c),
  cparser: build-test.sh, build-translate.sh, cparser.pro

Verification: pasm.bin before and after a change - tests/*.asm and texts from
instr.cc (tests/examples/*.cc in cparser) with the switches -o -l, -e -l, -c -l,
-l -i, -c and without switches: 156 translations, outputs, messages and return
code identical; repeated calls of PasmAssemble in one process (also after an
error) - identical outputs.  PasmAssembleText against PasmAssemble (file):
tests/*.asm and texts from instr.cc, text in one part, split by lines and into
7-character parts (also empty parts, CR LF split between parts), 5 combinations
of switches - 405 translations, outputs and messages identical.

Fix 2026-11-05: dup beyond the 64 kB segment boundary
-----------------------------------------------------
oth.c: the start position of DUP (DupStart) was card (uint16_t) from the Pascal
conversion - an array starting beyond the 64 kB data segment (c-mini: the Fmt
buffer in translate.c, 256 kB) gave a wrong segment length and "Compiler error"
(ElfPut). Now longint. The repeat count stays card (OMF; c-mini splits dup
into parts of 30000).

Tests 2026-11-05: tests/run.sh without stored patterns (tests/_expected removed)
--------------------------------------------------------------------------------
16 and 32 bit: all 8 tests byte-identical with the Pascal version
(built with fpc -Mtp at each run); x86-64:
hello64 (-e, ld), libc64 and sse64 (-c, gcc) - run identical with the same
programs in nasm syntax (tests/nasm/*.nasm); make test-nasm (230 OK, 3 known
differences), make test-nasm64 (209 OK, 25 equivalent, 4 known, 0 errors).


WINDOWS - EXECUTABLE PE FILE (-w) AND COFF x64 OBJECT (-m)
==========================================================

        pasm -w -l program.asm      => program.exe  (PE32 for USE32, PE32+ for USE64)
        pasm -m program.asm         => program.obj  (COFF x64, USE64 segments)
        (link.exe, or x86_64-w64-mingw32-gcc program.obj)

-w and -e, -m and -o (both .obj), and also -m and -c cannot be combined. -w uses
the checks of an executable ELF (-e), -m the checks of an ELF64 object (-c);
messages are named after the format ("Group not allowed in PE executable",
"COFF object requires USE64 segments"). The writing itself: code.c, section
WINDOWS PE A COFF.

PE (-w) - image without a linker
--------------------------------
- fixed ImageBase 400000h, no relocations (RELOCS_STRIPPED, no ASLR); addresses
  OFFSET, DD / DQ OFFSET, [label] are filled in directly as for ELF
- sections: headers on the 1st page, each segment starts on a new page (4 KiB),
  a name containing CODE or TEXT => .text (R X), otherwise .data (R W,
  NX_COMPAT); further segments .<name> (8 characters), empty ones are omitted;
  .itext - thunks, .idata - import table (descriptors, ILT, IAT, names)
- MS-DOS header only "MZ" and e_lfanew (no DOS program), TimeDateStamp 0 (the
  output is repeatable), console subsystem, OS / subsystem 6.0
- entry point = END label (required)
- a near jump / call into another segment is allowed in PE (not in ELF -e)
- imports: EXTRN = function from a DLL
        extrn   GetStdHandle: near                      ; kernel32.dll
        extrn   PRINTF: near 'msvcrt.dll!printf'        ; 'dll!function'
        extrn   IMPSTRLEN: near 'msvcrt.dll!__imp_strlen'
     CALL X, JMP X, OFFSET X, DQ OFFSET X  => thunk  JMP [IAT]  (FF 25)
     name __imp_function                   => IAT entry  (CALL QWORD PTR [X])
  i.e. as with a linker - the same source can be assembled with -w and -m.
  The same DLL + function (also via different EXTRN) has one thunk and one IAT
  entry. EXTRN X: ABS is not possible in PE ("Extrn ABS not allowed in PE
  executable").
- i386 (USE32): thunk JMP DWORD PTR [abs], stdcall WinAPI functions clean up
  the parameters themselves; for [X] with EXTRN an EXTRN inside the data segment
  and ASSUME are needed (tests/win32.asm)
- not supported: .pdata / .xdata (SEH unwind - running programs is not affected,
  but exceptions could not be unwound), resources (.rsrc), GUI subsystem, DLL

COFF (-m) - object for a linker
-------------------------------
- relocations from the second pass as for -c: R_X86_64_64 => IMAGE_REL_AMD64_ADDR64,
  R_X86_64_32 => ADDR32 (the linker wants /LARGEADDRESSAWARE:NO), PC32 / PLT32 =>
  REL32; the added value is in the section contents (for REL32 + 4)
- sections .text, .data, further .text$<name>, .data$<name> (the linker joins
  them), names over 8 characters in the string table ("/4"); alignment according
  to ALIGN; INIT_ARRAY => .CRT$XCU (CRT constructors, read-only), EH_FRAME =>
  .eh_frame (data), FINI_ARRAY is not possible ("FINI_ARRAY segment not allowed
  in COFF object")
- symbols: sections (STATIC + auxiliary record), EXTRN (undefined), PUBLIC
  (EXTERNAL, in a code segment the function type; EQU => ABSOLUTE); 'dll!function'
  is written as a function - for the linker the import library applies
- x86-64 only (no COFF i386 with decorated names _f@n)

Verification (make test, section 3 in tests/run.sh)
---------------------------------------------------
- tests/win64.asm -m + linker (import libraries kernel32, msvcrt) ==
  tests/nasm/win64.nasm (nasm -f win64) + linker: identical
  instructions (objdump without bytes - pasm and nasm differ only in the
  direction of MOV reg, reg), .data and .rdata (imports)
- tests/win64.asm -w: imports == nasm + linker;
  tests/win32.asm -w: PE32
- if wine is available (Fedora: dnf install wine): run of win64 (-w), win64
  (-m) and nasm - the same output and return code 35 (thunks, IAT,
  RIP-relative addresses into another segment, DQ OFFSET of imports, call into
  another segment, printf and exit from msvcrt); win32 (-w) - output and return
  code 7
- messages for -m with USE32 and for -w with GROUP
- original tests (16 / 32 bit against the Pascal version, x86-64 against nasm),
  make test-nasm, test-nasm64, test-asmlist, test-toaru unchanged


ToaruOS (-T) - JUMP BEFORE DATA, .o OBJECTS (2026-11-11)
========================================================

- pasm -e -T: if some segment lies below the entry point (e.g. data before
  code), a jump E9 rel32 = JMP start is written right after the ELF64 header
  (offset 78h) and e_entry = 3F000078h. The ToaruOS 2.3.2 kernel rejects
  pointers below e_entry (open on a path => error; write does not) - verified on
  the host with ../toaruos-tools/probe/openpos.asm. Without a segment below
  the entry point nothing changes (probe/toaru64.asm => byte-identical image).
- pasm -c: the ELF64 object is the same for ToaruOS as for Linux (System V ABI);
  libc64.o and sse64.o were linked by gcc 10.3 on the host and run with the
  same output.
- variable toaru => ToaruTarget: gcc in ToaruOS defines the macro toaru (= 1),
  pasm was not rebuilt on the host.
- tests: make test-toaru (5 checks), ../toaruos-tools/check.sh T1c;
  details in ../toaruos-tools/README.md §5c


LINKER FOR ToaruOS (pasm -x, PasmLink) - STATIC WITH THE ToaruOS LIBC (2026-11-11)
==================================================================================

        PASM_LIBDIR=../toaruos-tools/_build/toaru-lib pasm -x prog.asm
                => prog.o (as -c) and prog.bin: ELF64 linked statically
                   crt0.o crti.o prog.o libc.a crtn.o  (ToaruOS libc)
        cmini -L DIR -o prog.bin prog.c     the same from c-mini (cmini/instr.cc linkToaru)
        PasmLink (out, inputs, count, msg, msg_size)   pasmlib.h

ToaruOS libc on the host: ../toaruos-tools/mklibc.sh extracts from the
ISO (ISO => fat.img => ramdisk.igz => tar) /lib/crt0.o, crti.o, crtn.o and
libc.a. The resulting program runs in the base ToaruOS system - without gcc
(build-essential) and without ld.so.

Linker (code.c, section LINKER; LinkToaru):
- inputs: ELF64 REL x86-64 objects (pasm -c, gcc -c) and .a archives; an archive
  is read by itself (ToaruOS libc.a has two members math.o) and members are added
  as long as one of them defines an undefined symbol; GLOBAL / WEAK (undefined
  weak = 0), strong twice => error; COMMON is not supported
- relocations R_X86_64_64, PC32, PLT32, 32, 32S, PC64, GOTPCREL, GOTPCRELX,
  REX_GOTPCRELX (own GOT, 8 bytes per symbol); any other => error
- linker symbols: _GLOBAL_OFFSET_TABLE_, __init_array_start, __init_array_end,
  __bss_start, _edata, _end, end
- output as pasm -e -T: a single PT_LOAD R W X from 0x3F000000: header (128),
  .text (crt0.o first => _start lowest), .init, .fini, .rodata*, .data*, the
  constructor array (.init_array*, .ctors* - first the archive members = libc,
  then the program), GOT, .bss (p_memsz); .eh_frame, .note*, .comment, .fini_array
  are omitted (ToaruOS libc does not call .fini_array - destructors via atexit)
- NOTHING BELOW e_entry: the ToaruOS kernel rejects pointers below the entry
  point (open, write - PTR_INRANGE) and write in ToaruOS libc retries on the
  error => a program from GNU ld (.rodata in the first segment below .text)
  hung in ToaruOS

Verification: ../tests/16-toaruos-libc.sh - 18 programs from tests/examples
(cmini -L) and asm/tests libc64, sse64 (pasm -x) run in ToaruOS in QEMU without
gcc: 15 identical to the pattern, 5 differ only in printf %f of ToaruOS libc.

Link-only mode (2026-11-11):  pasm -k out.bin input.o ... library.a ...
- objects and archives in linker order (crt0.o crti.o ... libc.a crtn.o are NOT
  added automatically) - e.g. the cross build of the translators for ToaruOS
  (../toaruos-tools/mkcross.sh) and linking objects in ToaruOS:
        pasm.bin -k p /lib/crt0.o /lib/crti.o p.o lib/cminirt.o /lib/libc.a /lib/crtn.o

AN_CFI nodes in asmlist (2026-11-16, ../README.txt chapter 65):
- AsmCfi (p, ASM_CFI_OFFSET, RBX, -24) - information for .eh_frame as
  pseudo-instructions of GNU as (.cfi_startproc, .cfi_endproc, .cfi_def_cfa_offset,
  .cfi_def_cfa_register, .cfi_def_cfa, .cfi_offset, .cfi_restore,
  .cfi_remember_state, .cfi_restore_state); AsmCfiName, AsmCfiText
  (".cfi_offset rbx, -24"), AsmRegName (RAX => "rax")
- pasm prints and reads the node as a comment "; .cfi_..." (the library is not
  changed).
