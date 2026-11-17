; sse64.asm v syntaxi nasm - reference pro tests/run.sh: stejny vystup
; a navratovy kod jako pasm -c sse64.asm;  nasm -f elf64, gcc -no-pie
        bits    64
        default rel
        extern  printf
        global  main

        section .data align=16
absmsk  dq      7FFFFFFFFFFFFFFFh, 7FFFFFFFFFFFFFFFh
fmt1    db      'sum %f, product %f, sqrt %f', 10, 0
fmt2    db      'trunc %ld, round %ld, int->double %f, float %f', 10, 0
fmt3    db      'less %d, abs %f, min %f, max %f', 10, 0
a       dq      400C000000000000h       ; 3.5
b       dq      3FF4000000000000h       ; 1.25
c       dq      4030000000000000h       ; 16.0
d       dq      0C006000000000000h      ; -2.75
f       dd      3FC00000h               ; 1.5 (float)
res     dq      0

        section .text
main:   push    rbp
        mov     rbp, rsp
        sub     rsp, 16

        movsd   xmm0, [a]
        addsd   xmm0, [b]
        movsd   xmm1, [a]
        movsd   xmm9, [b]
        mulsd   xmm1, xmm9
        sqrtsd  xmm2, [c]
        lea     rdi, [fmt1]
        mov     eax, 3
        call    printf

        movsd   xmm8, [d]
        cvttsd2si rsi, xmm8
        cvtsd2si rdx, xmm8
        mov     rcx, 7
        cvtsi2sd xmm0, rcx
        movss   xmm3, [f]
        cvtss2sd xmm1, xmm3
        movsd   [res], xmm1
        movq    rax, xmm1
        movq    xmm1, rax
        lea     rdi, [fmt2]
        mov     eax, 2
        call    printf

        movsd   xmm4, [d]
        movsd   xmm5, [a]
        xor     esi, esi
        ucomisd xmm4, xmm5
        setb    sil
        movapd  xmm0, xmm4
        andpd   xmm0, [absmsk]
        movsd   xmm1, xmm4
        minsd   xmm1, xmm5
        movsd   xmm2, xmm4
        maxsd   xmm2, xmm5
        lea     rdi, [fmt3]
        mov     eax, 3
        call    printf

        xorpd   xmm0, xmm0
        xor     eax, eax
        mov     rsp, rbp
        pop     rbp
        ret

        section .note.GNU-stack noalloc noexec nowrite progbits
