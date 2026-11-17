; 16bitovy kod: zakladni instrukce, kratky skok zpet
code segment
        assume cs:code, ds:code
start:  mov ax, bx
        mov al, 5
        add cx, 1234h
        jmp short start
        int 21h
        mov word ptr [bx+si+4], 7
        ret
code ends
        end start
