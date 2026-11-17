"""Porovnani kodovani 64bitovych instrukci: pasm (syntaxe MASM, -e, segment USE64)
proti nasm (bits 64, -f bin).  Pri rozdilu se oba vysledky disassembluji (ndisasm -b 64).
pouziti: python3 -I nasm64cmp.py PASM PRACOVNI_ADRESAR"""
import os, re, subprocess, sys

PASM, WD = sys.argv[1], sys.argv[2]
os.makedirs(WD, exist_ok=True)

I64 = """
mov rax, rbx|mov r8, r15|mov rax, r9|mov r10, rcx|mov eax, r8d|mov r9d, ecx|mov ax, r8w|mov r12w, dx
mov al, r8b|mov r15b, cl|mov sil, al|mov dil, spl|mov bpl, 5|mov r9b, 7Fh|mov r11d, 12345678h
mov rax, 12345678h|mov rcx, -1|mov r13, 5|mov eax, 1|mov ax, 1
mov rax, [rbx]|mov rax, [r12]|mov rax, [r13]|mov rax, [rbp]|mov rax, [rsp]|mov [rsp+8], rdi
mov rax, [rbx+rcx*8]|mov rax, [r8+r9*4+10h]|mov rcx, [rax+r12*2]|mov rdx, [r13+rax]|mov rsi, [rbp+r13*1-8]
mov eax, [ebx]|mov rax, [eax+ecx*2]|mov r8, [r9d]|mov qword ptr [rax], 5|mov dword ptr [r10], -2
mov byte ptr [r11+3], 1|mov word ptr [rdi], 1234h|mov rax, [1000h]|mov eax, [12345678h]
add rax, rbx|add rax, 1|add rcx, 1000h|add r9, -1|add qword ptr [rax], 7|add [r8], rcx|add r8, [rsi+rdi]
sub rsp, 8|sub rsp, 1000h|adc r10, r11|sbb rax, [rbx]|cmp r15, 0|cmp qword ptr [rbp-8], 0
and rax, 0FFh|or r8d, 1|xor rax, rax|xor r8, r8|xor r8d, r8d|test rax, rax|test r9, 1|test rax, 1
inc rax|inc r8|dec ecx|dec r12d|inc ax|inc al|inc r8b|dec qword ptr [rax]|inc dword ptr [r9]
neg rax|not r11|mul rcx|imul r8|div qword ptr [rax]|idiv r9d
imul rax, rbx|imul r8, [rax]|imul rax, rcx, 10|imul r9, r10, 1000h|imul eax, r8d, 3
shl rax, 1|shr r8, cl|sar rdx, 4|rol r10d, 3|ror byte ptr [r8], 1|shl r9b, 2
push rax|push r8|push r15|pop rbx|pop r12|push qword ptr [rax]|pop qword ptr [r9]|push 5|push 1234h|push -3
push ax|pop r8w|push fs|pop gs
xchg rax, rbx|xchg rax, r8|xchg r9, r10|xchg ecx, eax|xchg [rax], r11
lea rax, [rbx+rcx*4+8]|lea r8, [r9+r10]|lea eax, [rax+rax*2]|lea rsp, [rsp+16]
movsx rax, byte ptr [rbx]|movsx r8, cx|movzx rax, al|movzx r9, word ptr [rsi]|movsx eax, r8b|movzx r10d, r11w
movsxd rax, ecx|movsxd r8, dword ptr [rax]|movsxd rdx, r9d
cdqe|cqo|cdq|cwde|syscall|ret|ret 8|nop|leave
lodsq|stosq|movsq|scasq|cmpsq|rep movsq|rep stosq|lodsb|stosd
bsf rax, rbx|bsr r8, [rax]|bt rax, 5|bts r9, r10|btr qword ptr [rax], 3
shld rax, rbx, 4|shrd r8, r9, cl|seta al|setne r8b|sete sil
jmp rax|call r8|jmp qword ptr [rax]|call qword ptr [r9+8]
fld qword ptr [rax]|fstp dword ptr [r8]|fild word ptr [r9]|fadd qword ptr [rbx+8]
mov eax, fs:[rbx]|mov rax, gs:[r8+10h]
mov rax, 123456789ABCDEF0h|mov r15, 7FFFFFFFFFFFFFFFh|mov rbx, 8000000000000000h|mov r9, -80000001h
mov rcx, 0FFFFFFFFh|mov rdx, 80000000h|mov rax, -80000000h|mov r10, 100000000h
add rax, -80000000h|cmp rcx, 7FFFFFFFh|push -80000000h|mov qword ptr [rax], 7FFFFFFFh
movsd xmm0, xmm1|movsd xmm8, xmm15|movsd xmm0, qword ptr [rax]|movsd xmm9, [r12+8]|movsd qword ptr [rbp-8], xmm3|movsd [r13], xmm10
movss xmm2, xmm3|movss xmm1, dword ptr [rsp+4]|movss dword ptr [rax], xmm14|movapd xmm0, xmm9|movaps xmm11, xmm2
addsd xmm0, xmm1|addsd xmm8, qword ptr [rax+rcx*8]|subsd xmm1, xmm2|mulsd xmm15, xmm0|divsd xmm3, [rbx]|sqrtsd xmm0, xmm0
minsd xmm1, xmm2|maxsd xmm2, [r9]|addss xmm0, xmm1|subss xmm9, dword ptr [rax]|mulss xmm1, xmm2|divss xmm3, xmm4|sqrtss xmm5, xmm6
ucomisd xmm0, xmm1|ucomisd xmm8, qword ptr [rax]|comisd xmm1, xmm9|ucomiss xmm0, xmm1|comiss xmm2, dword ptr [rcx]
cvtsi2sd xmm0, rax|cvtsi2sd xmm1, eax|cvtsi2sd xmm9, r10|cvtsi2sd xmm0, qword ptr [rax]|cvtsi2sd xmm2, dword ptr [rbx]|cvtsi2ss xmm0, rcx
cvttsd2si rax, xmm0|cvttsd2si eax, xmm1|cvttsd2si r9, xmm12|cvtsd2si rax, qword ptr [rbx]|cvttss2si ecx, xmm3|cvtss2si r8, xmm0
cvtsd2ss xmm0, xmm1|cvtss2sd xmm2, xmm3|cvtsd2ss xmm8, qword ptr [rax]|cvtss2sd xmm1, dword ptr [r9]
xorpd xmm0, xmm0|xorpd xmm1, [rax]|andpd xmm2, xmm3|andnpd xmm4, xmm5|orpd xmm8, xmm9|xorps xmm0, xmm1|andps xmm10, xmm2|orps xmm1, xmm2|pxor xmm3, xmm4
movq xmm0, rax|movq rax, xmm0|movq xmm9, r10|movq r11, xmm12|movq xmm1, xmm2|movq xmm3, qword ptr [rax]|movq qword ptr [rbx], xmm4
movd xmm0, eax|movd ecx, xmm1|movd xmm9, r8d|movd xmm2, dword ptr [rax]|movd dword ptr [rbx], xmm3
movsd|cmpsd|rep movsd
"""

def run(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, **kw)

def pasm_bytes(ins):
    src = ("        .x64\n        .387\n"
           "code    segment use64 byte\n"
           "start:  " + ins + "\n"
           "code    ends\n"
           "        end start\n")
    open(os.path.join(WD, "t.asm"), "w").write(src)
    r = run([PASM, "-e", "t.asm"], cwd=WD)
    if r.returncode != 0:
        return None, r.stdout.decode().strip()
    data = open(os.path.join(WD, "t.bin"), "rb").read()
    return data[120:], ""

def nasm_bytes(ins):
    n = re.sub(r"\s+ptr\b", "", ins, flags=re.I)
    n = re.sub(r"\b(fs|gs):\[", r"[\1:", n, flags=re.I)
    open(os.path.join(WD, "n.asm"), "w").write("bits 64\n" + n + "\n")
    r = run(["nasm", "-f", "bin", "n.asm", "-o", "n.bin"], cwd=WD)
    if r.returncode != 0:
        return None, r.stderr.decode().strip()
    return open(os.path.join(WD, "n.bin"), "rb").read(), ""

def dis(b):
    open(os.path.join(WD, "d.bin"), "wb").write(b)
    r = run(["ndisasm", "-b", "64", "d.bin"], cwd=WD)
    return " ; ".join(" ".join(l.split()[2:]) for l in r.stdout.decode().splitlines())

def norm(d):
    """normalizace disassembleru: velikosti, 'byte +', poradi scitancu v adrese, FWAIT"""
    d = d.lower().replace("wait ", "")
    for w in ("byte ", "word ", "dword ", "qword "):
        d = d.replace(w, "")
    d = d.replace(",+", ",").replace("+0x0]", "]")
    d = re.sub(r"\[([^\]]*)\]", lambda m: "[" + "+".join(sorted(m.group(1).split("+"))) + "]", d)
    return d

# zname rozdily: pasm vola jine (rovnocenne) kodovani
KNOWN = {
    "mov rax, 12345678h": "MOV r64, imm32 se znamenkovym rozsirenim (nasm: MOV r32)",
    "mov r13, 5":         "MOV r64, imm32 se znamenkovym rozsirenim (nasm: MOV r32)",
    "add rax, 1":         "akumulator: 48 05 imm32 (nasm: 83 /0 imm8)",
    "xchg r9, r10":       "XCHG je symetricka, registry v ModRM prohozene",
    "mov rcx, 0FFFFFFFFh": "MOV r64, imm64 (nasm: MOV r32, imm32 s nulovym rozsirenim)",
    "mov rdx, 80000000h":  "MOV r64, imm64 (nasm: MOV r32, imm32 s nulovym rozsirenim)",
}

ok = same = known = bad = 0
for ins in [x.strip() for x in I64.replace("\n", "|").split("|") if x.strip()]:
    p, perr = pasm_bytes(ins)
    n, nerr = nasm_bytes(ins)
    if p is None:
        print("PASM-ERR  %-36s %s" % (ins, perr)); bad += 1; continue
    if n is None:
        print("NASM-ERR  %-36s %s" % (ins, nerr)); bad += 1; continue
    if p == n:
        ok += 1
        continue
    dp, dn = dis(p), dis(n)
    if norm(dp) == norm(dn):
        same += 1      # rovnocenne kodovani (smer operandu, poradi registru v adrese, FWAIT)
    elif ins in KNOWN:
        known += 1
        print("ZNAMY %-36s %s" % (ins, KNOWN[ins]))
    else:
        bad += 1
        print("DIFF  %-36s pasm=%s  nasm=%s\n      pasm: %s\n      nasm: %s" %
              (ins, p.hex(" "), n.hex(" "), dp, dn))
print("OK", ok, "ROVNOCENNE", same, "ZNAME", known, "CHYBY", bad)
sys.exit(1 if bad else 0)
