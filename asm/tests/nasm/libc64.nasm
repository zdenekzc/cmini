; libc64.asm v syntaxi nasm - reference pro tests/run.sh: stejny vystup
; a navratovy kod (7) jako pasm -c libc64.asm;  nasm -f elf64, gcc -no-pie
        bits    64
        default rel
        extern  printf, puts, strlen
        global  main, counter

        section .data
fmt     db      'value %d, len %d, other %ld', 10, 0
msg     db      'hello from pasm', 0
counter dq      40
ptrs    dq      msg, helper

        section .text
main:   push    rbp
        mov     rbp, rsp
        push    rbx
        sub     rsp, 8
        lea     rdi, [msg]
        call    puts
        mov     rdi, [ptrs]             ; offset msg
        call    strlen
        mov     rbx, rax
        mov     rax, [ptrs+8]
        call    rax                     ; helper pres ukazatel
        add     qword [counter], 2
        call    other                   ; funkce v jinem segmentu
        lea     rdi, [fmt]
        mov     rsi, [counter]
        mov     rdx, rbx
        mov     rcx, rax
        xor     eax, eax
        call    printf
        mov     eax, 7
        add     rsp, 8
        pop     rbx
        pop     rbp
        ret
helper: inc     qword [counter]
        ret

        section .text2 progbits alloc exec nowrite
other:  mov     rax, 1234567890123h
        ret

        section .note.GNU-stack noalloc noexec nowrite progbits
