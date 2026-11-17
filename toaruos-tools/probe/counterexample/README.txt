Contrast test — low address 0x400000 (MUST NOT go on the shared disk!)
=================================================================

probe1.elf and probe2.elf were moved here on 2026-11-11 from ../, because they had PT_LOAD at
address 0x400000 (the linker default) — and the same file lay in ../push/, i.e. on
the shared disk that is mounted into the host. On the host such a file does not run:

    execve(...) = OK, but the process crashes with SIGSEGV before the first syscall
    (verified with strace: only execve, then --- SIGSEGV ---). The same source at
    address 0x3F000000 ran fine — see ../../README.md §5, item 1.

The source (../probe1.S, ../probe2.S) is EXACTLY THE SAME as for the variants that ran;
only the link address differs:

    BASE=0x400000 sh ../build.sh     # this contrast variant
    sh ../build.sh                   # the correct variant (default BASE=0x3F000000)

The address is the ONLY difference this file proves — that is why nothing
else belongs here. ../probe1b.elf and ../probe2b.elf are exactly the bytes that
ran on the host on 2026-11-10.
