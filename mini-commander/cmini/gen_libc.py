#!/usr/bin/env python3
"""Generate cmini/cmini_libc.h: the glibc/ncursesw declarations that the
mini-commander sources use, with everything they depend on.

Run once (needs gcc only to preprocess the system headers; cmini does not use gcc):
    python3 cmini/gen_libc.py  ->  cmini/cmini_libc.h

The declarations are copied from the real headers (same ABI); cparser does not
read the system headers itself (they use GNU extensions and tags it cannot
handle), so the used subset is written out here.
"""
import os, re, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.dirname(HERE)
HEADERS = ["ctype.h", "dirent.h", "errno.h", "fcntl.h", "getopt.h", "grp.h", "limits.h",
           "ncurses.h", "pwd.h", "signal.h", "stdint.h", "stdio.h", "stdlib.h", "string.h",
           "sys/mman.h", "sys/stat.h", "sys/time.h", "sys/types.h", "sys/utsname.h",
           "sys/wait.h", "sys/select.h", "time.h", "unistd.h"]
KEEP_PREFIXES = ("/usr/include/",)
DROP_PREFIXES = ("/usr/include/linux", "/usr/include/asm", "/usr/lib/gcc", "/usr/include/c++")
KEEP_ANYWAY = ("errno",)   # errno constants come through asm-generic/errno*.h

def preprocess():
    src = "\n".join("#include <%s>" % h for h in HEADERS) + "\n#define _GNU_SOURCE 1\n"
    # _GNU_SOURCE must come first: write it at the top
    src = "#define _GNU_SOURCE 1\n" + src.replace("#define _GNU_SOURCE 1\n", "")
    tmp = os.path.join(HERE, "_gen_all.c")
    with open(tmp, "w") as f:
        f.write(src)
    try:
        out = subprocess.run(["gcc", "-E", "-dD", tmp], capture_output=True, text=True, check=True).stdout
    finally:
        os.remove(tmp)
    return out

def items_from(text):
    """Top-level items (defines and declarations) from gcc -E -dD output."""
    cur = None
    chunks = []          # (file, text)
    buf = []
    depth = 0
    for ln in text.split("\n"):
        m = re.match(r'^# \d+ "([^"]+)"', ln)
        if m:
            cur = m.group(1)
            continue
        if cur is None or not cur.startswith(KEEP_PREFIXES):
            continue
        if cur.startswith(DROP_PREFIXES) and not any(k in cur for k in KEEP_ANYWAY):
            continue
        if re.match(r"^#\s*define\s+\w+", ln):
            # macros are not scoped by braces: keep them even inside a struct body;
            # _Generic wrappers (strchr, strrchr, ...) are left out: the functions remain
            # ctype macros go through __ctype_b_loc; the functions (isalnum, ...) remain
            if not re.search(r"_Generic|__glibc_const_generic|__isctype|__ctype_|\b_IS[a-z]", ln):
                chunks.append(("define", ln.strip()))
            continue
        if ln.startswith("#"):
            continue
        for ch in ln + "\n":
            buf.append(ch)
            if ch == "{": depth += 1
            elif ch == "}":
                depth -= 1
                if depth == 0 and "".join(buf).rstrip().endswith("}"):
                    pass
            elif ch == ";" and depth == 0:
                chunks.append(("decl", "".join(buf).strip()))
                buf = []
        if depth == 0 and buf and not "".join(buf).strip():
            buf = []
        # a function definition ends with "}" and no ";": close it here
        if depth == 0 and buf:
            cur_text = "".join(buf).rstrip()
            head = cur_text.split("{", 1)[0]
            if cur_text.endswith("}") and ")" in head:
                chunks.append(("decl", cur_text.strip()))
                buf = []
    return chunks

GNU_JUNK = [
    (r"__extension__\s*", ""),
    (r"__asm__\s*\(\s*\"[^\"]*\"(\s*\"[^\"]*\")*\s*\)", ""),
    (r"\b(__restrict|restrict|__inline|inline|_Noreturn|__wur)\b", ""),
    (r"__attribute__\s*\(\(([^()]|\([^()]*\))*\)\)", ""),
    (r"\b__signed__\b", "signed"),
]

def clean(s):
    for pat, rep in GNU_JUNK:
        s = re.sub(pat, rep, s)
    # cparser rejects a void typedef; _IO_lock_t is only used through pointers
    s = s.replace("typedef void _IO_lock_t;", "typedef char _IO_lock_t;")
    return s

def tags_in(text):
    return sorted(set(re.findall(r"\bstruct\s+(\w+)", text)))

def tag_rewrite(text, header):
    """Rewrite struct tags to typedef names (see main)."""
    tags = [t for t in tags_in(text) if t not in ("stat",)]
    defs = set(re.findall(r"\bstruct\s+(\w+)\s*\{", text))
    text = re.sub(r"\bstruct\s+stat\s*\{", "struct stat_s {", text)
    text = re.sub(r"\bstruct\s+stat\b(?!_)", "stat_t", text)
    for t in tags:
        if t == "stat" or t.endswith("_s"):
            continue
        # definitions keep "struct X {" ; every other "struct X" becomes X_t
        text = re.sub(r"\bstruct\s+%s\b(?!\s*\{)" % re.escape(t), t + "_t", text)
    typedefs = ["typedef struct %s %s_t;" % (t, t) for t in tags if t != "stat"]
    typedefs.insert(0, "typedef struct stat_s stat_t;")
    if header:
        text = text.replace("#define CMINI_LIBC_H\n", "#define CMINI_LIBC_H\n\n" + "\n".join(typedefs) + "\n", 1)
    return text

def key_and_refs(kind, text):
    """Return (key, set-of-referenced-names) for one item."""
    if kind == "define":
        m = re.match(r"#\s*define\s+(\w+)", text)
        name = m.group(1)
        body = text[m.end():]
        refs = set(re.findall(r"\b[A-Za-z_]\w*\b", body)) - {name}
        return name, refs
    text = clean(text)
    if "{" in text:
        m = re.match(r"\s*(typedef\s+)?(struct|union|enum)\s+(\w+)\s*\{", text)
        if m and not m.group(1):
            key = "%s %s" % (m.group(2), m.group(3))
        else:
            # typedef struct {...} NAME;   or   typedef struct X {...} NAME;
            m2 = re.search(r"\}\s*\*?\s*(\w+)\s*;\s*$", text)
            if not m2:
                return None, set()
            key = m2.group(1)
    else:
        if re.match(r"\s*typedef", text):
            fp = re.search(r"\(\s*\*\s*(\w+)\s*\)", text)
            found = re.findall(r"(\w+)\s*(\[[^\]]*\])?\s*;\s*$", text)
            key = fp.group(1) if fp else (found[0][0] if found else None)
            if key is None:
                return None, set()
        else:
            m = re.search(r"\b(\w+)\s*\(", text)
            if m:
                key = m.group(1)
            else:
                m3 = re.search(r"(\w+)\s*(\[[^\]]*\])?\s*;\s*$", text)
                if not m3:
                    return None, set()
                key = m3.group(1)
    # references: plain identifiers and "struct X" tags, excluding the declared key
    refs = set(re.findall(r"\b(struct|union|enum)\s+(\w+)", text))
    refs = {"%s %s" % r for r in refs}
    refs |= set(re.findall(r"\b[A-Za-z_]\w*\b", text))
    refs.discard(key)
    return key, refs

def mc_seeds():
    seeds = set()
    for fn in os.listdir(SRC):
        if not (fn.endswith(".c") or fn.endswith(".h")):
            continue
        s = open(os.path.join(SRC, fn), encoding="utf-8", errors="replace").read()
        s = re.sub(r"//[^\n]*", "", s)
        s = re.sub(r"/\*.*?\*/", "", s, flags=re.S)
        seeds |= set(re.findall(r"\b[A-Za-z_]\w*\b", s))
        seeds |= {"%s %s" % m for m in re.findall(r"\b(struct|union|enum)\s+(\w+)", s)}
        # struct X used as X_t in the sources (see tag_rewrite)
        seeds |= {"struct " + t[:-2] for t in re.findall(r"\b\w+_t\b", s)}
    return seeds

def builtin_macros():
    out = subprocess.run(["gcc", "-dM", "-E", "-x", "c", "/dev/null"], capture_output=True, text=True, check=True).stdout
    d = {}
    for ln in out.split("\n"):
        m = re.match(r"#define\s+(\w+)\s+(.*)", ln)
        if m and m.group(1).startswith("__"):
            d[m.group(1)] = "#define %s %s" % (m.group(1), m.group(2))
    return d

def main():
    chunks = items_from(preprocess())
    builtins = builtin_macros()
    index = {}       # key -> list of item indices
    parsed = []
    for i, (kind, text) in enumerate(chunks):
        key, refs = key_and_refs(kind, text)
        parsed.append((kind, text, key, refs))
        if key:
            index.setdefault(key, []).append(i)
    wanted = set(mc_seeds())
    # keep only keys that exist in the system headers
    queue = [k for k in wanted if k in index]
    chosen = set()
    seen = set(queue)
    extra = []
    while queue:
        k = queue.pop()
        for i in index.get(k, []):
            if i in chosen:
                continue
            chosen.add(i)
            for r in parsed[i][3]:
                if r in index and r not in seen:
                    seen.add(r)
                    queue.append(r)
                elif r in builtins and r not in extra:
                    extra.append(r)
    out = ["/* cmini_libc.h - generated by cmini/gen_libc.py from the glibc and ncursesw",
           "   headers (declarations used by mini-commander and what they depend on).",
           "   Do not edit by hand. */",
           "#ifndef CMINI_LIBC_H", "#define CMINI_LIBC_H", "",
           "/* compiler-provided types (gcc stddef.h is not used) */",
           "typedef unsigned long size_t;",
           "typedef long ptrdiff_t;",
           "#define NULL 0",
           "/* limits from gcc's limits.h (not used from the gcc include directory) */",
           "#define CHAR_BIT 8", "#define INT_MAX 2147483647", "#define INT_MIN (-INT_MAX-1)",
           "#define LONG_MAX 9223372036854775807L", "#define SIZE_MAX 18446744073709551615UL",
           "#define PATH_MAX 4096", "#define NAME_MAX 255", ""]
    out += [builtins[r] for r in extra]
    for i in sorted(chosen):
        kind, text, key, _ = parsed[i]
        if kind == "define":
            out.append(text)
        else:
            out.append(clean(text))
    out += ["", "#endif", ""]
    # cparser keeps struct tags and functions in one namespace and does not accept
    # bare "struct X" in function bodies: every tag X becomes the type X_t
    # (typedef at the top); the definition itself keeps the tag as struct X.
    text = "\n".join(out)
    text = tag_rewrite(text, header=True)
    out = [text]
    dest = os.path.join(HERE, "cmini_libc.h")
    with open(dest, "w") as f:
        f.write("".join(out) if len(out) == 1 and out[0].endswith("\n") else "\n".join(out))
    print("items: %d chosen of %d; keys: %d; wrote %s" % (len(chosen), len(chunks), len(seen), dest))

if __name__ == "__main__":
    main()
