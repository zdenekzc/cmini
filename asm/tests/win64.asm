; Windows x64: pasm -w win64 (PE32+ win64.exe, importy primo v obrazci)
;              pasm -m win64 (objekt COFF win64.obj; lld-link / link + kernel32, msvcrt)
        .x64
        extrn   GetStdHandle: near                      ; bez 'dll!' => kernel32.dll
        extrn   WriteFile: near 'kernel32.dll!WriteFile'
        extrn   PRINTF: near 'msvcrt.dll!printf'
        extrn   EXIT: near 'msvcrt.dll!exit'
        extrn   IMPSTRLEN: near 'msvcrt.dll!__imp_strlen' ; polozka IAT
        public  start
        public  counter

msglen  equ     31

data    segment use64 para
msg     db      'Hello from pasm (Windows x64)', 13, 10
written dd      0
fmt     db      'value %d, len %d, other %I64d', 10, 0
text    db      'strlen via IAT', 0
counter dq      40
ptrs    dq      offset text, offset helper, offset PRINTF
data    ends

code    segment use64 para
start:  sub     rsp, 40                 ; 32 bajtu pro volaneho + zarovnani na 16
        mov     ecx, -11                ; STD_OUTPUT_HANDLE
        call    GetStdHandle            ; thunk JMP [IAT]
        mov     rcx, rax
        lea     rdx, msg                ; RIP-relativni, jiny segment
        mov     r8d, msglen
        lea     r9, written
        mov     qword ptr [rsp+32], 0
        call    WriteFile
        mov     rcx, qword ptr [ptrs]   ; offset text
        call    qword ptr [IMPSTRLEN]   ; strlen primo pres IAT
        mov     rbx, rax                ; 14
        mov     rax, qword ptr [ptrs+8]
        call    rax                     ; helper pres ukazatel
        add     qword ptr [counter], 2
        call    other                   ; funkce v jinem segmentu
        mov     r9, rax
        lea     rcx, fmt
        mov     rdx, qword ptr [counter]
        mov     r8, rbx
        mov     rax, qword ptr [ptrs+16]
        call    rax                     ; printf pres ukazatel (OFFSET importu = thunk)
        mov     ecx, dword ptr [counter]
        sub     ecx, 8                  ; 43 - 8 = 35
        call    EXIT                    ; exit z msvcrt vyprazdni stdout
helper: inc     qword ptr [counter]
        ret
code    ends

code2   segment use64 para
other:  mov     rax, 1234567890123h
        ret
code2   ends
        end     start
