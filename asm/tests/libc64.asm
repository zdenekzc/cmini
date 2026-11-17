; objekt ELF64 pro gcc / glibc: pasm -c libc64, gcc -no-pie libc64.o
        .x64
        extrn   X1: near 'printf'
        extrn   X2: near 'puts'
        extrn   X3: near 'strlen'
        public  FMAIN 'main'
        public  counter

data    segment use64 para
fmt     db      'value %d, len %d, other %ld', 10, 0
msg     db      'hello from pasm', 0
counter dq      40
ptrs    dq      offset msg, offset helper
data    ends

code    segment use64 para
FMAIN:  push    rbp
        mov     rbp, rsp
        push    rbx
        sub     rsp, 8
        lea     rdi, msg
        call    X2                      ; puts
        mov     rdi, qword ptr [ptrs]   ; offset msg
        call    X3                      ; strlen
        mov     rbx, rax
        mov     rax, qword ptr [ptrs+8]
        call    rax                     ; helper pres ukazatel
        add     qword ptr [counter], 2
        call    other                   ; funkce v jinem segmentu
        lea     rdi, fmt
        mov     rsi, qword ptr [counter]
        mov     rdx, rbx
        mov     rcx, rax
        xor     eax, eax
        call    X1                      ; printf
        mov     eax, 7
        add     rsp, 8
        pop     rbx
        pop     rbp
        ret
helper: inc     qword ptr [counter]
        ret
code    ends

code2   segment use64 para
other:  mov     rax, 1234567890123h
        ret
code2   ends
        end
