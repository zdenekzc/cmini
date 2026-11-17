; test program
count   equ     10
code    segment
        assume  cs:code, ds:code
        org     100h
start:  mov     cx, count
        mov     si, offset msg
again:  lodsb
        cmp     al, '$'
        je      done
        call    putc
        loop    again
done:   mov     ax, 4C00h
        int     21h

putc    proc    near
        push    dx
        mov     dl, al
        mov     ah, 2
        int     21h
        pop     dx
        ret
putc    endp

msg     db      'Hello, world', 13, 10, '$'
tab     dw      1, 2, 3, offset start
buf     db      5 dup (0)
code    ends
        end     start
