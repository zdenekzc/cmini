; SSE / SSE2: skalarni operace double a float, prevody, porovnani, XMM8 .. XMM15
; objekt ELF64 pro gcc / glibc: pasm -c sse64, gcc -no-pie sse64.o
        .x64
        extrn   X1: near 'printf'
        public  FMAIN 'main'

data    segment use64 para
absmsk  dq      7FFFFFFFFFFFFFFFh, 7FFFFFFFFFFFFFFFh   ; abs (andpd - 16 bajtu, zarovnano)
fmt1    db      'sum %f, product %f, sqrt %f', 10, 0
fmt2    db      'trunc %ld, round %ld, int->double %f, float %f', 10, 0
fmt3    db      'less %d, abs %f, min %f, max %f', 10, 0
a       dq      400C000000000000h       ; 3.5
b       dq      3FF4000000000000h       ; 1.25
c       dq      4030000000000000h       ; 16.0
d       dq      0C006000000000000h      ; -2.75
f       dd      3FC00000h               ; 1.5 (float)
res     dq      0
data    ends

code    segment use64 para
FMAIN:  push    rbp
        mov     rbp, rsp
        sub     rsp, 16

        ; sum, product, sqrt
        movsd   xmm0, qword ptr [a]
        addsd   xmm0, qword ptr [b]     ; 4.75
        movsd   xmm1, [a]
        movsd   xmm9, [b]
        mulsd   xmm1, xmm9              ; 4.375
        sqrtsd  xmm2, qword ptr [c]     ; 4
        lea     rdi, fmt1
        mov     eax, 3                  ; pocet registru XMM pro printf
        call    X1

        ; prevody
        movsd   xmm8, [d]
        cvttsd2si rsi, xmm8             ; -2
        cvtsd2si rdx, xmm8              ; -3 (zaokrouhleni k sudemu: -2.75 => -3)
        mov     rcx, 7
        cvtsi2sd xmm0, rcx              ; 7.0
        movss   xmm3, dword ptr [f]
        cvtss2sd xmm1, xmm3             ; 1.5
        movsd   qword ptr [res], xmm1
        movq    rax, xmm1
        movq    xmm1, rax
        lea     rdi, fmt2
        mov     eax, 2
        call    X1

        ; porovnani, abs, min, max
        movsd   xmm4, [d]
        movsd   xmm5, [a]
        xor     esi, esi
        ucomisd xmm4, xmm5
        setb    sil                     ; -2.75 < 3.5 => 1
        movapd  xmm0, xmm4
        andpd   xmm0, [absmsk]            ; 2.75
        movsd   xmm1, xmm4
        minsd   xmm1, xmm5              ; -2.75
        movsd   xmm2, xmm4
        maxsd   xmm2, xmm5              ; 3.5
        lea     rdi, fmt3
        mov     eax, 3
        call    X1

        xorpd   xmm0, xmm0
        xor     eax, eax
        mov     rsp, rbp
        pop     rbp
        ret
code    ends
        end
