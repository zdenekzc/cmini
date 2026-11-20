; _setjmp for ToaruOS: the ToaruOS libc has only setjmp; cmini (ownheaders/setjmp.h)
; calls _setjmp. A tail jump keeps the return address of the caller.
        .x64
        extrn   X1: near 'setjmp'
        public  X2 '_setjmp'

code    segment use64 para
X2:     jmp     X1
code    ends
        end
