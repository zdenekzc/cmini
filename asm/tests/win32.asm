; Windows i386: pasm -w win32 (PE32 win32.exe, importy z kernel32.dll, stdcall)
        .386
        extrn   GetStdHandle: near
        extrn   WriteFile: near

msglen  equ     30

data    segment use32 para
        extrn   EXITPROC: dword '__imp_ExitProcess'     ; polozka IAT (v segmentu dat)
msg     db      'Hello from pasm (Windows x86)', 10
written dd      0
ptab    dd      offset ok, offset WriteFile
ok      db      'ok', 10
data    ends

code    segment use32 para
        assume  cs:code, ds:data
start:  push    -11                     ; STD_OUTPUT_HANDLE
        call    GetStdHandle            ; thunk JMP [IAT]
        mov     ebx, eax
        push    0
        push    offset written
        push    msglen
        push    offset msg
        push    ebx
        call    WriteFile               ; stdcall - parametry uklidi volany
        push    0
        push    offset written
        push    3
        push    dword ptr [ptab]        ; offset ok
        push    ebx
        call    dword ptr [ptab+4]      ; WriteFile pres ukazatel na thunk
        mov     eax, dword ptr [written]
        add     eax, 4                  ; 3 + 4 = 7
        push    eax
        call    dword ptr [EXITPROC]    ; ExitProcess (7) pres IAT
code    ends
        end     start
