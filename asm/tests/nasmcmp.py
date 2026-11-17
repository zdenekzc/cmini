"""Porovnani kodovani instrukci: pasm (syntaxe MASM, .obj) proti nasm (-f bin).
Pri rozdilu se oba vysledky disassembluji (ndisasm); FWAIT (9B) pred FPU se ignoruje.
pouziti: python3 -I nasmcmp.py PASM PRACOVNI_ADRESAR"""
import os, re, subprocess, sys

PASM, WD = sys.argv[1], sys.argv[2]
os.makedirs(WD, exist_ok=True)

I16 = """
mov ax, bx|mov al, 5|mov cx, 1234h|mov word ptr [bx+si+4], 7|mov byte ptr [di], 80h
mov ax, [bx]|mov [bp+2], dx|mov ds, ax|mov ax, es|mov al, [1234h]
add ax, 5|add al, 5|add cx, 1234h|add word ptr [bx], 1|adc dx, cx|sub sp, 8
sbb bx, [si]|cmp al, 'A'|cmp cx, -1|and ax, 0FF0h|or bl, 1|xor ax, ax|test al, 80h
test word ptr [bx], 1|inc ax|inc byte ptr [bx]|dec cx|dec word ptr [di+10h]
neg ax|not dx|mul bl|imul cx|div word ptr [bx]|idiv bh
shl ax, 1|shr bx, cl|sar dx, 1|rol al, cl|ror byte ptr [bx], 1|rcl ax, 1|rcr ax, cl
push ax|pop bx|push ds|pop es|push word ptr [bx]|pushf|popf
xchg ax, bx|xchg cl, dl|xchg [bx], ax|lea si, [bx+di+12h]|lds si, [bx]|les di, [bp+4]
int 21h|int 3|into|iret|ret|ret 4|retf|nop|hlt|cli|sti|cld|std|clc|stc|cmc
cbw|cwd|lahf|sahf|aaa|aas|daa|das|aam|aad|xlat
movsb|movsw|cmpsb|scasw|lodsb|stosw|rep movsb|repne scasb|rep stosw|lock inc word ptr [bx]
in al, 60h|out 20h, al|in ax, dx|out dx, al
push 1234h|push 5|push -3|mov ax, [bx+127]|mov ax, [bx+128]|mov ax, [bx-128]|mov ax, [bx-129]|mov ax, [bp]|imul ax, bx, 10|enter 8, 0|leave|pusha|popa|shl ax, 4|bound ax, [bx]
"""

I32 = """
mov eax, ebx|mov eax, 12345678h|mov ecx, [ebx]|mov [eax+ecx*4+8], edx|mov ax, bx
mov dword ptr [esi], 1|mov al, [ebx+esi]|add eax, 1|add eax, 1000h|sub esp, 16
cmp dword ptr [ebp+8], 0|xor eax, eax|test eax, eax|inc ecx|dec dword ptr [edi]
push eax|pop ebp|push 12345678h|push dword ptr [esp+4]|lea eax, [eax+eax*2]
movzx eax, byte ptr [ebx]|movsx ecx, word ptr [esi]|movzx eax, al|movsx edx, cx
shl eax, 3|shr ecx, cl|shld eax, edx, 4|shrd eax, edx, cl|imul eax, ecx|imul eax, ecx, 7
bt eax, 3|bts ecx, edx|btr dword ptr [ebx], 1|bsf eax, ecx|bsr edx, [ebx]
sete al|setne bl|setg byte ptr [ebx]|cdq|cwde|pushad|popad|pushfd|popfd
mov eax, cr0|mov cr0, eax|mov ebx, dr7|lods byte ptr [esi]|movsd|stosd|rep movsd
mov fs, ax|push fs|pop gs|mov eax, [12345678h]|and eax, 0FFFFFF00h|neg dword ptr [ecx]|mov eax, [ebx+127]|mov eax, [ebx+128]|mov eax, [ebx-128]|inc dword ptr [eax]|dec byte ptr [esi+4]|mov eax, [esp]|mov eax, [ebp]|mov eax, [ecx*4]
"""

F87 = """
fld st(1)|fld dword ptr [bx]|fld qword ptr [bx]|fld tbyte ptr [bx]|fst dword ptr [bx]
fstp st(2)|fild word ptr [bx]|fistp dword ptr [bx]|fadd st, st(1)|fadd st(2), st|faddp st(1), st
fsub dword ptr [bx]|fmul qword ptr [bx]|fdiv st, st(3)|fdivr st(1), st|fcom st(1)|fcompp
fxch st(1)|fchs|fabs|fsqrt|fldz|fld1|fldpi|fsin|fcos|fninit|fstsw ax|fnstsw word ptr [bx]
fst st(3)|fstp dword ptr [bx]|fstp qword ptr [bx]|fstp tbyte ptr [bx]|fist word ptr [bx]|fist dword ptr [bx]
fistp qword ptr [bx]|fild qword ptr [bx]|fprem1|fprem|fucom st(2)|fucomp st(3)|fucompp|ffree st(4)|fxch
fsubr dword ptr [bx]|fsubp st(1), st|fdivp st(2), st|fmulp st(1), st|fcomp dword ptr [bx]|ftst|fxam
fldcw word ptr [bx]|fnstcw word ptr [bx]|fiadd word ptr [bx]|fimul dword ptr [bx]|fbld tbyte ptr [bx]|fsincos|fscale
"""

# zname rozdily: jine platne kodovani, nebo nepodporovany zapis
KNOWN = {'add ax, 5', 'xchg cl, dl', 'int 3', 'mov al, [ebx+esi]',
         'add eax, 1', 'lods byte ptr [esi]'}
UNEXPECTED = []

def masm_to_nasm(s):
    s = s.replace(' ptr', '')
    s = re.sub(r'\bst\((\d)\)', r'st\1', s)
    s = re.sub(r'\bst\b(?!\d)', 'st0', s)
    s = s.replace('tbyte', 'tword')
    s = re.sub(r'^lods byte \[esi\]', 'lodsb', s)
    if re.match(r'^(pushfd|popfd|pushad|popad)$', s): pass
    return s

def omf_bytes(path):
    d = open(path, 'rb').read(); i = 0; out = b''
    while i < len(d):
        t = d[i]; l = d[i+1] | d[i+2] << 8; rec = d[i+3:i+3+l-1]
        if t in (0xA0, 0xA1):
            j = 1  # segment index (1 byte for small indices)
            j += 2 if t == 0xA0 else 4
            out += rec[j:]
        i += 3 + l
    return out

def run(lines, bits, pre):
    bad = ok = 0
    for ins in [x.strip() for x in lines.replace('\n', '|').split('|') if x.strip()]:
        a = os.path.join(WD, 't.asm')
        use = 'use32' if bits == 32 else ''
        open(a, 'w').write(f"{pre}\ncode segment {use}\n assume cs:code\n {ins}\ncode ends\n end\n")
        r = subprocess.run([PASM, '-o', a], capture_output=True, text=True)
        if r.returncode != 0 or not os.path.exists(os.path.join(WD, 't.obj')):
            print(f'{bits} PASM-ERR  {ins:35s} {r.stdout.strip()}'); bad += 1
            if ins not in KNOWN: UNEXPECTED.append(ins)
            continue
        pb = omf_bytes(os.path.join(WD, 't.obj')); os.remove(os.path.join(WD, 't.obj'))
        n = os.path.join(WD, 'n.asm'); nb = os.path.join(WD, 'n.bin')
        open(n, 'w').write(f"bits {bits}\ncpu 386\n {masm_to_nasm(ins)}\n")
        r2 = subprocess.run(['nasm', '-f', 'bin', '-o', nb, n], capture_output=True, text=True)
        if r2.returncode != 0:
            print(f'{bits} NASM-ERR  {ins:35s} {r2.stderr.strip()[:60]}'); continue
        nbytes = open(nb, 'rb').read()
        def dis(b):
            open(os.path.join(WD,'d.bin'),'wb').write(b)
            r=subprocess.run(['ndisasm','-b',str(bits),os.path.join(WD,'d.bin')],capture_output=True,text=True).stdout
            return [l.split(None,2)[2] for l in r.splitlines() if len(l.split(None,2))>2]
        pcmp = pb[1:] if pb[:1]==bytes([0x9b]) and nbytes[:1]!=bytes([0x9b]) else pb
        if pb == nbytes or dis(pcmp) == dis(nbytes): ok += 1
        else:
            bad += 1
            if ins not in KNOWN: UNEXPECTED.append(ins)
            print(f'{bits} DIFF      {ins:35s} pasm={pb.hex(" ")}  nasm={nbytes.hex(" ")}')
    return ok, bad

tot = [0, 0]
for lines, bits, pre in ((I16, 16, '.386'), (I32, 32, '.386'), (F87, 16, '.386\n.387')):
    o, b = run(lines, bits, pre); tot[0] += o; tot[1] += b
print('OK', tot[0], 'BAD', tot[1], '(z toho znamych', len(KNOWN), ')')
sys.exit(0 if tot[1] <= len(KNOWN) and not UNEXPECTED else 1)
