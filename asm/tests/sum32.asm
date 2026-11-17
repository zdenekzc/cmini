; 32bitovy segment, extrn/public, SIB adresa, kratky jnz zpet, fixup na externi call
        .386
        extrn   ext1: near
        public  sum
code32  segment use32
        assume  cs:code32
sum     proc    near
        xor     eax, eax
        mov     ecx, [esp+4]
next:   add     eax, dword ptr [ecx*4+table]
        dec     ecx
        jnz     next
        call    ext1
        ret     4
sum     endp
table   dd      1, 2, 3, 4
code32  ends
        end
