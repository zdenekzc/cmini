/* probe2.S — argv/envp in ToaruOS 2.3.2: they come ON THE STACK, not via .ktmp
 *
 * The kernel (kernel/misc/elf64.c:373-436) builds the user stack by pushing the strings, then the
 * envp[] array, the argv[] array and finally argc. So at the moment of entry
 * [rsp]=argc, below it argv[] terminated by NULL, then envp[] NULL, then
 * auxv (0, 12(AT_EUID), uid, 11(AT_UID), uid, 0). arch_enter_user
 * (kernel/arch/x86_64/user.c:41-50) also passes rdi=argc, rsi=argv, rdx=envp.
 *
 * The .ktmp section is deliberately NOT filled here, as a contrast test: kargv/kenvp is
 * the mechanism of the old toaru3, it does not occur anywhere in the whole tree in 2.3.2 (grep
 * kargv|ktmp|SHT_KTMP = empty result). It should therefore stay unfilled, and loading
 * the binary with an unknown section type should not cause anything at all —
 * the loader does not look at the section headers for exec.
 *
 * In addition we demonstrate a pitfall that is easy to hit: syscalls with
 * PTR_VALIDATE (e.g. SYS_OPEN on a path) return -EFAULT if the buffer lies
 * BELOW the entry point (base/usr/include/kernel/syscall.h:16-17,
 * PTR_INRANGE). So the data lie high above the entry (0x402000 vs entry 0x401000).
 */

    .section .ktmp,"aw",@progbits      /* patch_ktmp.py rewrites the type to 0x8000000c */
    .align 8
    .globl kargv
kargv:
    .space 64
    .globl kenvp
kenvp:
    .space 64

    .text
    .globl _start
_start:
    andq $-16, %rsp                   /* the kernel does not promise alignment (elf64.c:394-399) */
    movq (%rsp), %rbx                 /* argc */
    /* WATCH OUT, PITFALL: [rsp]=argc and right below it lies the POINTER ARRAY argv[0],
       it is not a pointer to an array as in main(argc, argv). We do the 16 alignment
       ourselves here, because the kernel admits it in its comment as unfinished.
       (The original version had "movq 8(%rsp),%rbp" here = it took the string as an array
       and failed with SIGSEGV in strlen over its first 8 bytes — see README.) */
    leaq 8(%rsp), %rbp                /* &argv[0] */

    movl $4, %eax                     /* SYS_WRITE */
    movl $1, %edi
    leaq m1(%rip), %rsi
    movl $m1len, %edx
    syscall

    /* argc as three digits */
    movq %rbx, %rax
    movl $10, %r9d
    leaq num(%rip), %r8
    xorl %edx, %edx
    divq %r9
    addl $48, %edx
    movb %dl, 2(%r8)
    xorl %edx, %edx
    divq %r9
    addl $48, %edx
    movb %dl, 1(%r8)
    xorl %edx, %edx
    divq %r9
    addl $48, %edx
    movb %dl, 0(%r8)
    movl $4, %eax
    movl $1, %edi
    leaq num(%rip), %rsi
    movl $3, %edx
    syscall
    movl $4, %eax
    movl $1, %edi
    leaq nl(%rip), %rsi
    movl $1, %edx
    syscall

    /* --- argv[] --- */
    xorl %r12d, %r12d                 /* i */
av: movq (%rbp,%r12,8), %r13
    testq %r13, %r13
    je   ad
    movl $4, %eax
    movl $1, %edi
    leaq m2(%rip), %rsi
    movl $m2len, %edx
    syscall
    xorq %rcx, %rcx
1:  cmpb $0, (%r13,%rcx)
    je   2f
    incq %rcx
    jmp  1b
2:  movl $4, %eax
    movl $1, %edi
    movq %r13, %rsi
    movq %rcx, %rdx
    syscall
    movl $4, %eax
    movl $1, %edi
    leaq nl(%rip), %rsi
    movl $1, %edx
    syscall
    incq %r12
    jmp  av
ad:
    /* envp follows the first NULL after argv[] */
    leaq 8(%rbp,%r12,8), %rbp
    xorl %r12d, %r12d
ev: movq (%rbp,%r12,8), %r13
    testq %r13, %r13
    je   ed
    movl $4, %eax
    movl $1, %edi
    leaq m3(%rip), %rsi
    movl $m3len, %edx
    syscall
    xorq %rcx, %rcx
3:  cmpb $0, (%r13,%rcx)
    je   4f
    incq %rcx
    jmp  3b
4:  movl $4, %eax
    movl $1, %edi
    movq %r13, %rsi
    movq %rcx, %rdx
    syscall
    movl $4, %eax
    movl $1, %edi
    leaq nl(%rip), %rsi
    movl $1, %edx
    syscall
    incq %r12
    jmp  ev

    /* --- .ktmp: should be empty --- */
ed: leaq kargv(%rip), %r13
    movq (%r13), %r13
    testq %r13, %r13
    je   nz
    movl $4, %eax
    movl $1, %edi
    leaq m4(%rip), %rsi
    movl $m4len, %edx
    syscall
    movl $0, %eax                     /* SYS_EXT = exit, return value in rdi */
    movl $5, %edi
    syscall
nz: movl $4, %eax
    movl $1, %edi
    leaq m5(%rip), %rsi
    movl $m5len, %edx
    syscall
    movl $0, %eax
    movl $9, %edi
    syscall
5:  jmp 5b                            /* if exit did not work, we will notice */

    .section .rodata
m1:  .ascii "PROBE2: argc="
     .set m1len, . - m1
m2:  .ascii "  argv="
     .set m2len, . - m2
m3:  .ascii "  env="
     .set m3len, . - m3
m4:  .ascii "  kargv[0] filled (unexpected!)\n"
     .set m4len, . - m4
m5:  .ascii "  kargv[0]=NULL (expected: .ktmp is not used in 2.3.2)\n"
     .set m5len, . - m5
nl:  .ascii "\n"
num: .space 3
