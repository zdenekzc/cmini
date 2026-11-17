; ELF i386 pro Linux: pasm -e hello32
        .386
msglen  equ     13
code    segment use32
        assume  cs:code, ds:data
start:  mov     ecx, offset msg
        mov     edx, msglen
        call    write
        mov     esi, dword ptr [ptab]   ; adresa z DD
        mov     ecx, esi
        mov     edx, 3
        call    write
        mov     eax, 1                  ; exit (7)
        mov     ebx, 7
        int     80h

write   proc    near                    ; ecx = text, edx = delka
        mov     eax, 4
        mov     ebx, 1
        int     80h
        ret
write   endp
code    ends

data    segment use32 para
msg     db      'Hello, world', 10

ptab    dd      offset ok
buf     db      100 dup (0)
ok      db      'ok', 10
data    ends
        end     start
