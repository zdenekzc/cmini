# toaru-top.sh — Where does the main `cecko` tree belong? The scripts learn it like this:
#
#   . "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/toaru-top.sh"
#   TOP=$(find_top "$DIR") || die "no main cecko tree — set TOP=/path"
#
# Originally it was assumed that the directory with these scripts lives in
# <cecko>/toaruos-tools, so TOP was always "../..". That stopped being true
# with the tree being moved here (where "../.." is no longer the cecko tree) — and the scripts
# suffered for it in a way that is not visible: mkguest.sh failed, check.sh gave
# one "FAILED" for that and five further checks were not run at all ("SKIPPED without
# pasm.bin"), so the output could only be trusted where it said OK.
#
# Therefore the directory is searched by CONTENT (asm/code.c + cmini/instr.c) and "../.." is
# just one of the candidates, so the older layout keeps working. It can always be overridden
# with the TOP=/path variable.
#
# Note: cmini/instr.c is a generated file (make -C cmini from instr.cc), so
# a tree that was only copied without building will not be found — that is intentional.
find_top() {
    local here=$1 c d=${TOP:-}
    if [[ -n $d ]]; then
        [[ -f $d/asm/code.c && -f $d/cmini/instr.c ]] && { (cd "$d" && pwd); return 0; }
        return 1
    fi
    for c in "$here"/../* "$here" "$here"/../.. "$here"/.. "$here"/*; do
        [[ -f $c/asm/code.c && -f $c/cmini/instr.c ]] || continue
        (cd "$c" && pwd)
        return 0
    done
    return 1
}
