; TOARU64.ASM - the smallest program for ToaruOS 2.3.2, made by OUR assembler
;
;   pasm -e -T toaru64          (or: PASM_TARGET=toaru pasm -e toaru64)
;
; Unlike asm/tests/hello64.asm, this is not an assembler test but the other end of
; the chain: the resulting file must also be loadable by the ToaruOS kernel. These are two
; independent things that can succeed separately:
;
;   1. LOADER - already verified earlier with an ELF64 from the host gcc: ET_EXEC + a single
;      PT_LOAD and base 3F000000h. That is why the -T switch exists.
;   2. ABI - the syscall and register numbers are DIFFERENT from Linux. -T cannot
;      solve that (the assembler does not invent numbers, it only translates them), so it
;      must be written correctly here:
;
;          rax = number, parameters rdi rsi rdx r10 r8 (max 5!)
;          return value rax, error = -errno, no exit_group
;          Sys_Exit = 0   (Linux 60)
;          Sys_Open = 2   Sys_Read = 3   Sys_Write = 4 (Linux 1)   Sys_Close = 5
;      Beware of the 4th parameter: with the SYSCALL instruction the return address is stored
;      in rcx and the kernel zeroes rcx at once (irq.S:283), so the 4th parameter
;      goes in r10. Linux has rcx — that is why this mistake is hard to find.
;
; The exit return code does not reach the esh (sys_exit does task_exit(exitcode<<8)),
; so in the guest do not be fooled by the value of `$?` — judge by the text.
;
; Segment order: the data lie after the code, i.e. above the entry point address. Syscalls
; that validate a pointer (PTR_VALIDATE, e.g. open) return -EFAULT if the
; buffer lies BELOW e_entry (base/usr/include/kernel/syscall.h, PTR_INRANGE); write
; does not. Since 2026-11-11 pasm -T solves this itself: if a segment lies below the entry point,
; it inserts JMP start after the header (see openpos.asm). Here no jump is needed,
; the image stays byte-identical with toaru64.bin, which ran on the host.

; The message length must match: pasm does not accept `equ $ - msg` across segments (it reports
; "Improper operand size" - $ outside the same segment is not a constant),
; so msglen is a number that must be updated together with the text.

        .x64
msglen  equ     63                      ; 'TOARU64: ... loaded by the kernel\n' + 'argv[0] = '

code    segment use64
start:  lea     rsi, msg                ; RIP-relative address into another segment
        mov     edx, msglen
        call    write

        mov     rsi, qword ptr [rsp+8]  ; [rsp]=argc, the string argv[0] right after it
        call    puts0                   ; (the kernel passes it ON THE STACK, not via .ktmp)

        xor     edi, edi
        mov     eax, 0                  ; Sys_Exit (Linux: 60)
        syscall
forever:jmp     forever                 ; if exit did not work, we will notice

write   proc    near                    ; rsi = text, rdx = length
        mov     eax, 4                  ; Sys_Write (Linux: 1)
        mov     edi, 1
        syscall
        ret
write   endp

puts0   proc    near                    ; rsi = string terminated by zero
        xor     rcx, rcx
dls1:   cmp     byte ptr [rsi+rcx], 0
        je      dls2
        inc     rcx
        jmp     dls1
dls2:   mov     rdx, rcx
        call    write
        ret
puts0   endp
code    ends

data    segment use64 para              ; AFTER the code, see the comment at the top
msg     db      'TOARU64: image from pasm -e -T, loaded by the kernel', 10
        db      'argv[0] = '            ; + \n = 63, match with msglen above
data    ends

        end     start
