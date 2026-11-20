#!/usr/bin/env python3
"""trace.py - step-by-step trace of a small pasm program (for students).

   python3 asm/trace.py [-n MAX] prog.asm

pasm (asm/pasm.bin) assembles prog.asm with its listing (-l); this script reads the
listing (addresses, bytes, source lines, label table) and runs the program on a small
x86-64 model: registers, flags (ZF SF CF OF), the stack and the instructions

    mov lea add sub inc dec xor and or cmp test imul
    jmp je jz jne jnz jl jg jle jge jb ja call ret push pop
    syscall (rax = 1 write to stdout, rax = 60 exit; Linux numbers)

Each trace line: step, address, instruction and what changed (registers in hex, flags).
Memory: [reg + reg*scale + disp] on the stack and in memory written by the program
(data labels and db/dq are not modelled).  Not supported => the trace stops with a message.
"""
import os, re, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
PASM = os.environ.get("PASM", os.path.join(HERE, "pasm.bin"))
MASK = (1 << 64) - 1
RET_SENTINEL = 0xFFFFFFFF      # return address of main: returning to it ends the trace
REGS64 = ["rax", "rbx", "rcx", "rdx", "rsi", "rdi", "rbp", "rsp",
          "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"]
ALIAS32 = {"eax": "rax", "ebx": "rbx", "ecx": "rcx", "edx": "rdx", "esi": "rsi", "edi": "rdi",
           "ebp": "rbp", "esp": "rsp"}
ALIAS32.update({"r%dd" % i: "r%d" % i for i in range(8, 16)})
ALIAS8 = {"al": "rax", "bl": "rbx", "cl": "rcx", "dl": "rdx"}


class Unsupported(Exception):
    pass


def assemble_listing(src):
    """pasm -l: returns (instructions {addr: (text, bytes)}, labels {name: addr}, listing lines)."""
    d = tempfile.mkdtemp(prefix="pasmtrace-")
    base = os.path.join(d, "prog.asm")
    with open(src, "r", encoding="utf-8", errors="replace") as f, open(base, "w", encoding="utf-8") as g:
        g.write(f.read())
    r = subprocess.run([PASM, "-l", base], cwd=d, capture_output=True, text=True)
    if r.returncode != 0:
        sys.exit("pasm: %s" % (r.stdout + r.stderr).strip())
    text = open(os.path.join(d, "prog.lst"), encoding="utf-8", errors="replace").read()
    insns, labels = {}, {}
    in_table = False
    for line in text.split("\n"):
        m = re.match(r"^([0-9A-Fa-f]{4})\s+((?:[0-9A-Fa-f]{2} ?)+)\s+(.*)$", line)
        if m:
            addr = int(m.group(1), 16)
            src_part = m.group(3)
            # label in front of the instruction: "FMAIN:  mov eax, 0"
            lm = re.match(r"^([A-Za-z_@$?][\w@$?]*):\s*(.*)$", src_part)
            if lm:
                labels[lm.group(1).lower()] = addr
                src_part = lm.group(2)
            code = re.sub(r"\s*;.*$", "", src_part).strip()
            if code and not code.lower().startswith(("code ", "data ", "end", "public", "extrn",
                                                    ".x64", "segment", "proc", "endp", "ends")):
                insns[addr] = (code, m.group(2).strip())
            continue
        if re.match(r"^\s*$", line):
            continue
        mm = re.match(r"^([A-Za-z_@$?][\w@$?]*)\.+\s+(Near|Far|Byte|Word|Dword|Qword|Proc)\s+\S*?:?([0-9A-Fa-f]{4})?", line)
        if mm and mm.group(3):
            labels[mm.group(1).lower()] = int(mm.group(3), 16)
    # the label table: "AGAIN..........  Near CODE:000A"
    for mm in re.finditer(r"^([A-Za-z_@$?][\w@$?]*)\.+\s+Near\s+\w+:([0-9A-Fa-f]{4})", text, re.M):
        labels[mm.group(1).lower()] = int(mm.group(2), 16)
    return insns, labels, text


def parse_operand(s):
    s = s.strip()
    sm = re.match(r"^(?:(byte|word|dword|qword)\s+ptr\s+)?\[(.*)\]$", s, re.I)
    if sm:
        return ("mem", sm.group(2).strip())
    sm = re.match(r"^(?:(byte|word|dword|qword)\s+ptr\s+)?([A-Za-z_]\w*)$", s, re.I)
    if sm and sm.group(2).lower() in REGS64 + list(ALIAS32) + list(ALIAS8):
        return ("reg", sm.group(2).lower())
    if sm:
        return ("label", sm.group(2).lower())
    v = s.lower()
    if v.endswith("h"):
        return ("imm", int(v[:-1], 16))
    return ("imm", int(v, 0))


def reg_name(r):
    r = r.lower()
    if r in ALIAS32:
        return ALIAS32[r], 32
    if r in ALIAS8:
        return ALIAS8[r], 8
    return r, 64


class Machine:
    def __init__(self, labels):
        self.r = {n: 0 for n in REGS64}
        self.r["rsp"] = 0x7FF00000
        self.r["rbp"] = 0x7FF00000
        self.flags = {"ZF": 0, "SF": 0, "CF": 0, "OF": 0}
        self.mem = {}
        self.labels = labels
        self.out = []

    def value(self, kind, text, size=64):
        if kind == "reg":
            name, bits = reg_name(text)
            v = self.r[name]
            return v & ((1 << bits) - 1)
        if kind == "imm":
            return text & MASK
        if kind == "label":
            if text in self.labels:
                return self.labels[text]
            raise Unsupported("label %s is not a code label (data is not modelled)" % text)
        if kind == "mem":
            return self.read(self.address(text), size // 8 if size else 8)
        raise Unsupported(kind)

    def address(self, expr):
        total = 0
        for term in re.findall(r"[+-]?[^+-]+", expr.replace(" ", "")):
            sign = -1 if term.startswith("-") else 1
            t = term.lstrip("+-")
            if "*" in t:
                a, b = t.split("*")
                ra, rb = (a, b) if a.lower() in REGS64 + list(ALIAS32) else (b, a)
                total += sign * self.r[reg_name(ra)[0]] * int(rb, 0)
            elif t.lower() in REGS64 or t.lower() in ALIAS32:
                total += sign * self.r[reg_name(t)[0]]
            elif re.match(r"^[0-9a-fA-F]+h$", t) or re.match(r"^\d+$", t):
                total += sign * (int(t[:-1], 16) if t.lower().endswith("h") else int(t))
            else:
                raise Unsupported("address term %s" % t)
        return total & MASK

    def read(self, addr, nbytes):
        return sum(self.mem.get(addr + i, 0) << (8 * i) for i in range(nbytes))

    def write(self, addr, nbytes, value):
        for i in range(nbytes):
            self.mem[addr + i] = (value >> (8 * i)) & 0xFF

    def set_flags(self, res, bits, cf=0, of=0):
        mask = (1 << bits) - 1
        res &= mask
        self.flags["ZF"] = int(res == 0)
        self.flags["SF"] = int((res >> (bits - 1)) & 1)
        self.flags["CF"] = cf
        self.flags["OF"] = of


def signed(v, bits):
    v &= (1 << bits) - 1
    return v - (1 << bits) if v >> (bits - 1) else v


LINE_LIMIT = 400


def quiet_limit(steps):
    return steps >= LINE_LIMIT


def run(src, max_steps=2000, quiet=False):
    insns, labels, listing = assemble_listing(src)
    m = Machine(labels)
    if not insns:
        sys.exit("no instructions in %s" % src)
    addrs = sorted(insns)
    pc = addrs[0]
    if "fmain" in labels:
        pc = labels["fmain"]
    elif "main" in labels:
        pc = labels["main"]
    steps = 0
    lines = []
    m.r["rsp"] = (m.r["rsp"] - 8) & MASK          # main returns to the sentinel address
    m.write(m.r["rsp"], 8, RET_SENTINEL)
    def emit(s):
        lines.append(s)
        if not quiet:
            print(s)
    emit("trace of %s (entry 0x%04X, %d instructions)" % (os.path.basename(src), pc, len(insns)))
    while steps < max_steps:
        if pc not in insns:
            emit("stop: address 0x%04X is not an instruction" % pc)
            break
        text, bts = insns[pc]
        before = dict(m.r)
        fbefore = dict(m.flags)
        steps += 1
        mn, _, rest = text.partition(" ")
        mn = mn.lower()
        ops = [o.strip() for o in re.split(r",(?![^\[]*\])", rest)] if rest.strip() else []
        next_pc = pc + len(bytes.fromhex(bts.replace(" ", ""))) if bts else None
        try:
            if mn in ("mov", "lea"):
                dst, src_op = ops
                dk, dt = parse_operand(dst)
                sk, st = parse_operand(src_op)
                if mn == "lea":
                    val = m.address(st[1] if sk == "mem" else st)
                else:
                    val = m.value(sk, st, 64 if dk == "reg" else 64)
                if dk == "reg":
                    name, bits = reg_name(dt)
                    if bits == 32:
                        m.r[name] = val & 0xFFFFFFFF
                    elif bits == 8:
                        m.r[name] = (m.r[name] & ~0xFF) | (val & 0xFF)
                    else:
                        m.r[name] = val & MASK
                else:
                    m.write(m.address(dt[1] if False else dt), 8, val)
            elif mn in ("add", "sub", "and", "or", "xor", "cmp", "test"):
                dst, src_op = ops
                dk, dt = parse_operand(dst)
                sk, st = parse_operand(src_op)
                a = m.value(dk, dt)
                b = m.value(sk, st)
                bits = 64
                if mn == "add":
                    res = a + b; cf = int(res > MASK); of = int(signed(a, 64) + signed(b, 64) != signed(res, 64))
                elif mn in ("sub", "cmp"):
                    res = a - b; cf = int(a < b); of = int(signed(a, 64) - signed(b, 64) != signed(res, 64))
                elif mn == "and" or mn == "test":
                    res = a & b; cf = 0; of = 0
                elif mn == "or":
                    res = a | b; cf = 0; of = 0
                else:
                    res = a ^ b; cf = 0; of = 0
                m.set_flags(res, bits, cf, of)
                if mn not in ("cmp", "test") and dk == "reg":
                    name, _ = reg_name(dt)
                    m.r[name] = res & MASK
            elif mn in ("inc", "dec"):
                dk, dt = parse_operand(ops[0])
                a = m.value(dk, dt)
                res = a + 1 if mn == "inc" else a - 1
                m.set_flags(res, 64, of=int(signed(res, 64) != signed(a, 64) + (1 if mn == "inc" else -1)))
                m.r[reg_name(dt)[0]] = res & MASK
            elif mn == "imul":
                dk, dt = parse_operand(ops[0])
                sk, st = parse_operand(ops[1])
                res = signed(m.value(dk, dt), 64) * signed(m.value(sk, st), 64)
                m.r[reg_name(dt)[0]] = res & MASK
                m.set_flags(res, 64)
            elif mn in ("jmp", "je", "jz", "jne", "jnz", "jl", "jg", "jle", "jge", "jb", "ja", "jbe", "jae"):
                _, tgt = parse_operand(ops[0])
                if tgt not in labels:
                    raise Unsupported("jump target %s" % tgt)
                F = m.flags
                take = {"jmp": True, "je": F["ZF"], "jz": F["ZF"], "jne": not F["ZF"], "jnz": not F["ZF"],
                        "jl": F["SF"] != F["OF"], "jg": (not F["ZF"]) and F["SF"] == F["OF"],
                        "jle": F["ZF"] or F["SF"] != F["OF"], "jge": F["SF"] == F["OF"],
                        "jb": F["CF"], "ja": (not F["CF"]) and (not F["ZF"]),
                        "jbe": F["CF"] or F["ZF"], "jae": not F["CF"]}[mn]
                if take:
                    next_pc = labels[tgt]
            elif mn == "call":
                _, tgt = parse_operand(ops[0])
                m.r["rsp"] = (m.r["rsp"] - 8) & MASK
                m.write(m.r["rsp"], 8, next_pc)
                next_pc = labels[tgt]
            elif mn == "ret":
                next_pc = m.read(m.r["rsp"], 8)
                m.r["rsp"] = (m.r["rsp"] + 8) & MASK
                if next_pc == RET_SENTINEL:
                    emit("%5d  %04X  %-28s return from main, rax = %d (0x%X)" % (steps, pc, text, signed(m.r["rax"], 64), m.r["rax"] & MASK))
                    return m, lines
            elif mn == "push":
                k, t = parse_operand(ops[0])
                m.r["rsp"] = (m.r["rsp"] - 8) & MASK
                m.write(m.r["rsp"], 8, m.value(k, t))
            elif mn == "pop":
                k, t = parse_operand(ops[0])
                m.r[reg_name(t)[0]] = m.read(m.r["rsp"], 8)
                m.r["rsp"] = (m.r["rsp"] + 8) & MASK
            elif mn == "syscall":
                n = m.r["rax"]
                if n == 1:                          # write(rdi, rsi, rdx)
                    data = bytes(m.read(m.r["rsi"] + i, 1) for i in range(m.r["rdx"]))
                    m.out.append(data.decode("utf-8", "replace"))
                    m.r["rax"] = m.r["rdx"]
                    if not quiet:
                        print("       output: %r" % data.decode("utf-8", "replace"))
                elif n == 60:                       # exit(rdi)
                    emit("%5d  %04X  %-28s exit %d" % (steps, pc, text, m.r["rdi"] & 0xFF))
                    return m, lines
                else:
                    raise Unsupported("syscall %d" % n)
            else:
                raise Unsupported("instruction %s" % mn)
        except Unsupported as e:
            emit("stop: not supported: %s (at 0x%04X: %s)" % (e, pc, text))
            break
        changes = []
        for n in REGS64:
            if before[n] != m.r[n]:
                changes.append("%s=%X" % (n, m.r[n]))
        for f in ("ZF", "SF", "CF", "OF"):
            if fbefore[f] != m.flags[f]:
                changes.append("%s=%d" % (f, m.flags[f]))
        emit("%5d  %04X  %-28s %s" % (steps, pc, text, " ".join(changes)))
        if quiet_limit(steps):
            emit("...")
            break
        if next_pc is None:
            break
        pc = next_pc
    else:
        emit("stop: %d steps" % max_steps)
    return m, lines


def main(argv):
    max_steps = 2000
    if len(argv) >= 3 and argv[1] == "-n":
        max_steps = int(argv[2]); argv = argv[:1] + argv[3:]
    if len(argv) != 2:
        print(__doc__)
        return 2
    m, _ = run(argv[1], max_steps)
    print("final registers: " + " ".join("%s=%X" % (n, m.r[n]) for n in ("rax", "rcx", "rdx", "rdi")))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
