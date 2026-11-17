; OPENPOS.ASM - open() with a path BELOW the first code segment (data1) and AFTER it (data2)
;
;   pasm -e -T openpos          => openpos.bin for ToaruOS 2.3.2
;
; The ToaruOS kernel rejects pointers below e_entry (PTR_INRANGE): open on a path below the
; entry point returns an error, write does not. Segment data1 lies BEFORE the code, therefore
; pasm -T inserts a JMP start right after the header and e_entry = 3F000078h — everything
; is then above the entry point. On the host (2026-11-11):
;     without the jump:  open(path below entry): ERR   open(path above entry): OK
;     with the jump:     open(path below entry): OK    open(path above entry): OK
; ToaruOS syscall numbers: Exit 0, Open 2, Write 4 (see toaru64.asm).
        .x64
data1   segment use64 para
path1   db      '/tmp/openpos.bin', 0
m1      db      'open(path below entry): '
data1   ends

code    segment use64
start:  lea     rsi, m1
        mov     edx, 24
        call    write
        lea     rdi, path1
        call    tryopen
        lea     rsi, m2
        mov     edx, 24
        call    write
        lea     rdi, path2
        call    tryopen
        xor     edi, edi
        mov     eax, 0                  ; Sys_Exit
        syscall
forever:jmp     forever

tryopen proc    near                    ; rdi = path; prints OK / ERR
        xor     esi, esi                ; O_RDONLY
        xor     edx, edx
        mov     eax, 2                  ; Sys_Open
        syscall
        lea     rsi, ok
        test    rax, rax
        jns     tro1
        lea     rsi, err
tro1:   mov     edx, 4
        call    write
        ret
tryopen endp

write   proc    near                    ; rsi = text, rdx = length
        mov     eax, 4                  ; Sys_Write
        mov     edi, 1
        syscall
        ret
write   endp
code    ends

data2   segment use64 para
path2   db      '/tmp/openpos.bin', 0
m2      db      'open(path above entry): '
ok      db      'OK', 13, 10
err     db      'ERR', 10
data2   ends
        end     start
