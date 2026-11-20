        .x64
        public  FMAIN 'main'
code    segment use64 para
FMAIN:  push    rbp
        mov     rbp, rsp
        mov     edi, 6
        call    square
        pop     rbp
        ret
square: mov     eax, edi
        imul    eax, edi
        ret
code    ends
        end
