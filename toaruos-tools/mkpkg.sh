#!/bin/bash
# mkpkg.sh — builds the COMPILER package for ToaruOS: _build/guest-src/ => _build/push/cmini-src.tar.gz
#
#   ./mkpkg.sh                 # mkguest.sh (_build/guest-src/ from the current tree)
#                              # and from it _build/push/cmini-src.tar.gz
#   OUT=/somewhere/other.tar.gz ./mkpkg.sh
#   SRC=/ready/dir ./mkpkg.sh     # without mkguest.sh, package from the given directory
#
# The package is cmini + the pasm assembler, mostly in C; inside ToaruOS it
# builds ITSELF ("sh build.sh", plain gcc from "msk install build-essential" is enough,
# make and g++ are not there). The sources are not only in a tar — they lie unpacked, so
# diff and version control work on them; the tar is just the transport format to the host.
#
# Sequence: ./mkpkg.sh ; ./mkshare.sh ; ./toaru-vm.sh start
#
# The tar is deterministic (same content => same bytes): sorted
# entries, owner 0:0, epoch as mtime, gzip without timestamp. This matters
# because the package is rebuilt between tests and its md5 is printed in the log.
set -euo pipefail

DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
die() { echo "error: $*" >&2; exit 1; }

if [[ -z ${SRC:-} ]]; then
    OUT= "$DIR/mkguest.sh" > /dev/null || die "mkguest.sh failed"   # here OUT = tar, not a directory
    SRC=${DIR}/_build/guest-src
fi
OUT=${OUT:-${DIR}/_build/push/cmini-src.tar.gz}


[[ -d $SRC ]] || die "not a directory with sources: $SRC"
[[ -f $SRC/build.sh ]] || die "$SRC is missing build.sh — this is not a compiler package"
[[ -d $SRC/cmini && -d $SRC/asm ]] || die "$SRC is missing the cmini/ or asm/ directory"

TMP=$(mktemp -d) || die "mktemp"
trap 'rm -rf "$TMP"' EXIT
PKG="$TMP/cmini-src"          # name the host sees after "tar xzf ... ; cd cmini-src"
mkdir -p "$PKG"
cp -a "$SRC"/. "$PKG"/

# build artifacts that do not belong in the package (they arise when
# build.sh is tried in the source directory by accident); the host builds the binaries itself (build.sh)
rm -rf "$PKG/obj" "$PKG/cmini.bin" "$PKG/libpasm.a" "$PKG/pasm.bin" \
       "$PKG/asm/pasm.bin" "$PKG/asm/libpasm.a"
find "$PKG" -name '*.o' -type f -delete

mkdir -p "$(dirname "$OUT")"
tar --format=ustar --sort=name --owner=0 --group=0 --numeric-owner --mtime=@0 \
    -cf - -C "$TMP" cmini-src | gzip -n > "$OUT"

echo "package: $OUT ($(wc -c < "$OUT") B, $(tar tzf "$OUT" | wc -l) entries)"
echo "md5:   $(md5sum "$OUT" | cut -d' ' -f1)"
echo "host:  tar xzf cmini-src.tar.gz ; cd cmini-src ; sh build.sh"
