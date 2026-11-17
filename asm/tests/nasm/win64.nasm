; win64.asm v syntaxi nasm - reference pro tests/run.sh: stejny vystup
; a navratovy kod (35) jako pasm -w win64 a pasm -m win64;  nasm -f win64, lld-link
        bits    64
        default rel
        extern  GetStdHandle, WriteFile, printf, exit, __imp_strlen
        global  start, counter

        section .data
msg     db      'Hello from pasm (Windows x64)', 13, 10
written dd      0
fmt     db      'value %d, len %d, other %I64d', 10, 0
text    db      'strlen via IAT', 0
counter dq      40
ptrs    dq      text, helper, printf

        section .text
start:  sub     rsp, 40
        mov     ecx, -11
        call    GetStdHandle
        mov     rcx, rax
        lea     rdx, [msg]
        mov     r8d, 31
        lea     r9, [written]
        mov     qword [rsp+32], 0
        call    WriteFile
        mov     rcx, [ptrs]
        call    [__imp_strlen]
        mov     rbx, rax
        mov     rax, [ptrs+8]
        call    rax
        add     qword [counter], 2
        call    other
        mov     r9, rax
        lea     rcx, [fmt]
        mov     rdx, [counter]
        mov     r8, rbx
        mov     rax, [ptrs+16]
        call    rax
        mov     ecx, [counter]
        sub     ecx, 8
        call    exit
helper: inc     qword [counter]
        ret

        section .text$code2 code
other:  mov     rax, 1234567890123h
        ret
