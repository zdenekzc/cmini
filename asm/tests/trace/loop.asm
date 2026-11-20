        .x64
        public  FMAIN 'main'
code    segment use64 para
FMAIN:  mov     eax, 0
        mov     ecx, 5
again:  add     eax, ecx
        dec     ecx
        jnz     again
        ret
code    ends
        end
