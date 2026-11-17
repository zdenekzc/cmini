; hello64.asm v syntaxi nasm - reference pro tests/run.sh: stejny vystup
; a navratovy kod (35) jako pasm -e hello64.asm;  nasm -f elf64, ld
        bits    64
        default rel
msglen  equ     13
huge    equ     100000000h * 3          ; konstanta nad 32 bitu

        section .data
msg     db      'Hello, world', 10
ptab    dq      ok
count   dd      0
nums    dd      5, -7, 12, 100
buf     times 16 db 0
ok      db      'ok', 10
big     dq      123456789ABCDEF0h

        section .text
        global  _start
_start: lea     rsi, [msg]
        mov     edx, msglen
        call    write
        mov     rsi, [ptab]
        mov     edx, 3
        call    write

        xor     r8d, r8d
        xor     r9, r9
        lea     r10, [nums]
next:   movsxd  rax, dword [r10+r9*4]
        add     r8, rax
        inc     r9
        cmp     r9, 4
        jl      next                    ; 110

        mov     dword [count], 3
        add     dword [count], 4
        cmp     byte [later], 'L'
        jne     bad
        mov     eax, [count]            ; 7
        add     r8, rax                 ; 117

        mov     rax, r8
        neg     rax
        cqo
        mov     rcx, 10
        idiv    rcx
        push    rax
        pop     r15
        neg     r15                     ; 11
        add     r8, r15                 ; 128

        lea     rsi, [ok]
        lea     rdi, [buf]
        mov     ecx, 3
        rep     movsb
        lea     rsi, [buf]
        mov     edx, 3
        call    write

        mov     rax, 123456789ABCDEF0h
        cmp     rax, [big]
        jne     bad
        mov     rcx, 0FFFFFFFFh
        inc     rcx
        shr     rcx, 32                 ; 1
        add     r8, rcx                 ; 129
        mov     rdx, huge
        shr     rdx, 32                 ; 3
        add     r8, rdx                 ; 132
        mov     rax, huge / 100h
        shr     rax, 24                 ; 3
        add     r8, rax                 ; 135

        mov     rdi, r8                 ; exit (135 - 100 = 35)
        sub     rdi, 100
        mov     eax, 60
        syscall

bad:    mov     edi, 1
        mov     eax, 60
        syscall

write:  mov     eax, 1                  ; rsi = text, rdx = delka
        mov     edi, 1
        syscall
        ret

later   db      'L'

        section .note.GNU-stack noalloc noexec nowrite progbits
