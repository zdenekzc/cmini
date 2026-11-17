; ELF x86-64 pro Linux: pasm -e hello64
        .x64
msglen  equ     13

data    segment use64 para
msg     db      'Hello, world', 10
ptab    dq      offset ok
count   dd      0
nums    dd      5, -7, 12, 100
buf     db      16 dup (0)
ok      db      'ok', 10
big     dq      123456789ABCDEF0h       ; 64bitova konstanta
data    ends

huge    equ     100000000h * 3          ; konstanta nad 32 bitu

code    segment use64
start:  lea     rsi, msg                ; RIP-relativni adresa (jiny segment)
        mov     edx, msglen
        call    write
        mov     rsi, qword ptr [ptab]   ; adresa z DQ
        mov     edx, 3
        call    write

        ; soucet pole nums v r8 (r9 = index)
        xor     r8d, r8d
        xor     r9, r9
        lea     r10, nums
next:   movsxd  rax, dword ptr [r10+r9*4]
        add     r8, rax
        inc     r9
        cmp     r9, 4
        jl      next                    ; 5 - 7 + 12 + 100 = 110

        mov     dword ptr [count], 3    ; RIP-relativni s primou hodnotou za adresou
        add     dword ptr [count], 4
        cmp     byte ptr [later], 'L'   ; dopredny odkaz, RIP-relativni, 1 bajt za adresou
        jne     bad
        mov     eax, dword ptr [count]  ; 7
        add     r8, rax                 ; 117

        ; deleni se znamenkem: -117 / 10 = -11
        mov     rax, r8
        neg     rax
        cqo
        mov     rcx, 10
        idiv    rcx
        push    rax
        pop     r15
        neg     r15                     ; 11
        add     r8, r15                 ; 128

        ; kopie 'ok' do buf a vypis
        lea     rsi, ok
        lea     rdi, buf
        mov     ecx, 3
        rep     movsb
        lea     rsi, buf
        mov     edx, 3
        call    write

        ; 64bitove prime hodnoty
        mov     rax, 123456789ABCDEF0h  ; MOV r64, imm64
        cmp     rax, qword ptr [big]
        jne     bad
        mov     rcx, 0FFFFFFFFh         ; imm64 (nevejde se do imm32 se znamenkem)
        inc     rcx                     ; 100000000h
        shr     rcx, 32                 ; 1
        add     r8, rcx                 ; 129
        mov     rdx, huge               ; 300000000h
        shr     rdx, 32                 ; 3
        add     r8, rdx                 ; 132
        mov     rax, huge / 100h        ; 3000000h (imm32)
        shr     rax, 24                 ; 3
        add     r8, rax                 ; 135

        mov     rdi, r8                 ; exit (135 - 100 = 35)
        sub     rdi, 100
        mov     eax, 60
        syscall

bad:    mov     edi, 1
        mov     eax, 60
        syscall

write   proc    near                    ; rsi = text, rdx = delka
        mov     eax, 1
        mov     edi, 1
        syscall
        ret
write   endp

later   db      'L'
code    ends
        end     start
