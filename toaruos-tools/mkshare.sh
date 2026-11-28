#!/bin/bash
# mkshare.sh — builds _build/share/share.img (ext2) from the contents of the _build/push/ directory.
#
#   ./mkshare.sh                 # image from _build/push/
#   SRC=/elsewhere ./mkshare.sh
#
# The mke2fs options are not random — ToaruOS modules/ext2.c handles only 32-bit ext2
# (no 64bit / metadata_csum) and has no htree, hence no dir_index either. Inode size
# 256 does no harm, the driver reads it from the superblock.
set -u

DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
TOP=$(cd "$DIR/.." && pwd)
SRC=${SRC:-${TOP}/_build/push}
IMG=${IMG:-${TOP}/_build/share/share.img}
MB=${MB:-64}

die() { echo "error: $*" >&2; exit 1; }

[[ -d "$SRC" ]] || die "not a directory with files: $SRC"
command -v mke2fs >/dev/null || die "missing mke2fs (dnf install e2fsprogs)"
[[ $(find "$SRC" -type f | wc -l) -gt 0 ]] || die "$SRC is empty — nothing to transfer"

mkdir -p "$(dirname "$IMG")"
rm -f "$IMG"
truncate -s "${MB}M" "$IMG"
mke2fs -q -F -L cmini -t ext2 -O ^64bit,^metadata_csum,^dir_index \
    -b 2048 -m 0 -d "$SRC" "$IMG"

echo "image: $IMG ($(wc -c < "$IMG") B)"
debugfs -R 'ls -l /' "$IMG" 2>/dev/null | tail -n +2
