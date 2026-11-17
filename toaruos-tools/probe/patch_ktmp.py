#!/usr/bin/python3
"""patch_ktmp.py — rewrites the type of the .ktmp section to SHT_KTMP (0x8000000c).

GNU as on x86-64 does not allow a numeric section type (.section .ktmp,"aw",0x8000000c
=> "junk at end of line"), so the section is assembled as progbits and its type is
rewritten in the finished ELF. Exactly this is what the assembler's own function will do
(asm/prod.cc in the earlier tree) — here we only verify it by hand, so that we know how
the loader behaves.

usage: patch_ktmp.py FILE.elf [.sectionname]
"""
import struct
import sys

SHT_KTMP = 0x8000000C


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    path = argv[1]
    want = argv[2] if len(argv) > 2 else ".ktmp"
    with open(path, "r+b") as f:
        data = f.read()
        if data[:4] != b"\x7fELF" or data[4] != 2:
            print("not an ELF64:", path)
            return 1
        (e_shoff, ) = struct.unpack_from("<Q", data, 0x28)
        (e_shentsize, e_shnum, e_shstrndx) = struct.unpack_from("<HHH", data, 0x3A)
        # shstrtab: its offset and size
        sh = e_shoff + e_shstrndx * e_shentsize
        shstr_off, shstr_size = struct.unpack_from("<QQ", data, sh + 24)
        shstr = data[shstr_off:shstr_off + shstr_size]
        hit = 0
        for i in range(e_shnum):
            base = e_shoff + i * e_shentsize
            (sh_name, sh_type) = struct.unpack_from("<II", data, base)
            end = shstr.find(b"\0", sh_name)
            name = shstr[sh_name:end].decode("ascii", "replace")
            if name == want:
                f.seek(base + 4)
                f.write(struct.pack("<I", SHT_KTMP))
                print("section %s (#%d): type %08x -> %08x" % (name, i, sh_type, SHT_KTMP))
                hit += 1
        if not hit:
            print("section %s not in %s" % (want, path))
            return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
