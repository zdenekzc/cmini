/* probe1.S — the smallest program that verifies the ELF64 loader in ToaruOS.
 *
 * Built ON THE HOST (gcc -nostdlib -no-pie -static), the finished ELF is transferred to the host.
 * The loader (kernel/misc/elf64.c) loads the program itself and starts it at
 * e_entry; _start is only a linker convention, the kernel does not require it (by name).
 *
 * We verify these three things:
 *   1) the kernel loads and starts a static ET_EXEC ELF64 without PT_INTERP at all
 *   2) syscalls with their own numbers work. The numbers differ from Linux and the error
 *      reports are not (we number from base/usr/include/syscall_nums.h):
 *        SYS_EXT(exit)=0  SYS_OPEN=2  SYS_READ=3  SYS_WRITE=4  SYS_CLOSE=5
 *        SYS_EXECVE=7     SYS_GETPID=9  SYS_SBRK=10  SYS_SEEK=14
 *     exit_group does not exist; on a direct call the 4th parameter is passed in r10
 *      (not rcx — with the syscall instruction that is impossible, rcx holds the return
 *      address; the kernel zeroes rcx in the frame anyway, irq.S:283).
 *   3) at start rsp points to argc, followed by argv[]/envp[] — i.e. Linux-like
 *      (kernel/misc/elf64.c:373-436); the kernel also passes rdi=argc, rsi=argv,
 *      rdx=envp (kernel/arch/x86_64/user.c:41-50).
 *
 * For comparison: toaru3 passed parameters via the .ktmp section (kargv/kenvp),
 * 2.3.2 does not have that mechanism — see probe2.S, which lets us check it.
 */

    .text
    .globl _start
_start:
    /* the stack has not been touched yet: (rsp)=argc, 8(rsp)=argv[0] */
    movq (%rsp), %rbx             /* argc */
    movq 8(%rsp), %rbp            /* argv[0] */

    /* write(1, m1, m1len) */
    movl $4, %eax
    movl $1, %edi
    leaq m1(%rip), %rsi
    movl $m1len, %edx
    syscall

    /* write(1, "argc=", 5) */
    movl $4, %eax
    movl $1, %edi
    leaq m2(%rip), %rsi
    movl $m2len, %edx
    syscall

    /* three decimal digits of argc (leading zeros do no harm) */
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

    /* argv[0] — if present, print it (length = strlen) */
    testq %rbp, %rbp
    je   out
    xorl %ecx, %ecx
1:  cmpb $0, (%rbp,%rcx)
    je   2f
    incq %rcx
    jmp  1b
2:  movl $4, %eax
    movl $1, %edi
    movq %rbp, %rsi
    movq %rcx, %rdx
    syscall
    movl $4, %eax
    movl $1, %edi
    leaq nl(%rip), %rsi
    movl $1, %edx
    syscall

out:
    /* write(1, "PROBE1 done\n", ...) and exit(0) */
    movl $4, %eax
    movl $1, %edi
    leaq m3(%rip), %rsi
    movl $m3len, %edx
    syscall

    movl $0, %eax                 /* SYS_EXT = exit (there is no exit_group) */
    movl $7, %edi                 /* return value, so that it is visible in the shell's $? */
    syscall
    /* if exit did not work, we loop here — it will show anyway */
3:  jmp 3b

    .section .rodata
m1:  .ascii "PROBE1: loaded, write works\n"
     .set m1len, . - m1
m2:  .ascii "argc="
     .set m2len, . - m2
m3:  .ascii "PROBE1 done\n"
     .set m3len, . - m3
nl:  .ascii "\n"
num: .space 3
