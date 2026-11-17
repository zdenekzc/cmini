; dalsi opravy: INC/DEC do pameti, rozsah 8bit posunuti, PUSH konstanty,
; POPA (driv bran jako prefix), dopredny odkaz na PROC a data
        .386
code    segment
        assume  cs:code, ds:code
        inc     byte ptr [bx]
        dec     word ptr [di+10h]
        lock inc word ptr [bx]
        mov     ax, [bx+127]
        mov     ax, [bx+128]
        mov     ax, [bx-128]
        mov     ax, [bx-129]
        push    1234h
        push    -3
        pusha
        popa
        enter   8, 0
        leave
        call    later
        mov     si, offset data1
        jmp     back
back:   jmp     back
later   proc    near
        ret
later   endp
data1   db      'abc', 0
code    ends
        end
