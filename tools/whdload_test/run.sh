#!/usr/bin/env bash
# Host test of the WHDLoad installer (src/3ds/whdload3ds.cpp, src/3ds/ofs_image.cpp + vendored Lhasa), with AddressSanitizer/UBSan.
#   tools/whdload_test/run.sh /path/to/lhasa/test/archives
# The archives are the ones of the Lhasa test-suite (https://github.com/fragglet/lhasa, test/archives): real files made
# by LHA on the Amiga (1.22, 2.12), MorphOS, Unix, OS/2, Windows, ...
set -e
here=$(cd "$(dirname "$0")" && pwd); top=$(cd "$here/../.." && pwd)
arch=${1:?usage: run.sh <archives dir>}
out=${TMPDIR:-/tmp}/whdload_host_test; rm -rf "$out"; mkdir -p "$out/obj" "$out/work"
EXCL=" lh_new_decoder.c bit_stream_reader.c tree_decode.c pma_common.c "
objs=()
for f in "$top"/src/3ds/lhasa/*.c; do
  b=$(basename "$f"); case "$EXCL" in *" $b "*) continue;; esac
  gcc -std=gnu99 -O1 -g -fsanitize=address,undefined -I"$top/src/3ds/lhasa" -c "$f" -o "$out/obj/${b%.c}.o"; objs+=("$out/obj/${b%.c}.o")
done
WARN="-Wall -Wextra -Wno-unused-parameter -Wno-format-truncation"
# the OFS image writer on its own (block layout, bitmap, large volumes) ...
g++ -std=gnu++14 -O1 -g -fsanitize=address,undefined $WARN -I"$top/src/3ds" -I"$here" \
    "$here/ofs_host_test.cpp" "$top/src/3ds/ofs_image.cpp" -o "$out/ofs_host_test"
"$out/ofs_host_test"
# ... and the installer: hand made archives + every archive of the Lhasa suite, as folder and as image
g++ -std=gnu++14 -O1 -g -fsanitize=address,undefined $WARN \
    -I"$top/src/3ds" -I"$top/src/3ds/lhasa" -I"$here" "$here/whdload_host_test.cpp" "$top/src/3ds/whdload3ds.cpp" \
    "$top/src/3ds/ofs_image.cpp" "${objs[@]}" -o "$out/whdload_host_test"
"$out/whdload_host_test" "$arch" "$out/work"
