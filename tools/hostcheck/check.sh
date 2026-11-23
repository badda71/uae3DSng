#!/bin/bash
# Host-side *syntax-only* check (clang, 32-bit target) of a source file, against the real libctru
# and SDL 1.2 headers. NOT a build: no ARM code generation, no linking, newlib replaced by glibc headers.
# It catches missing declarations/typos/signature mismatches; it cannot catch ABI, asm or link problems.
#   usage: LIBCTRU_INC=/path/to/libctru/libctru/include tools/hostcheck/check.sh src/main.cpp
#   (git clone https://github.com/devkitPro/libctru for the headers; or point it at $DEVKITPRO/libctru/include)
set -e
P="$(cd "$(dirname "$0")/../.." && pwd)"
LIBCTRU_INC="${LIBCTRU_INC:-${DEVKITPRO:-/opt/devkitpro}/libctru/include}"
case "$1" in *.c) LANGOPT="-x c -std=gnu11";; *) LANGOPT="-x c++ -std=gnu++11 -fpermissive";; esac
exec clang++ --target=i386-unknown-linux-gnu -fsyntax-only $LANGOPT -w -ferror-limit=30 \
  -D_3DS -D__3DS__ -DARM11 -DGP2X -DPANDORA -DUSE_SDL -DUSE_AUTOCONFIG -DUSE_ZFILE -DUSE_FAME_CORE -DWITH_TESTMODE \
  -DUSE_UNDERSCORE -DSHM_SUPPORT_LINKS=0 -DUNALIGNED_PROFITABLE -DOPTIMIZED_FLAGS \
  -DOS_WITHOUT_MEMORY_MANAGEMENT -DUSE_UAE4ALL_VKBD \
  '-DROM_PATH_PREFIX="x/"' '-DDATA_PREFIX="x/"' '-DSAVE_PREFIX="x/"' '-DTHUMB_PREFIX="x/"' '-DTMP_PREFIX="x/"' \
  '-DVERSION_MAJOR="0"' '-DVERSION_MINOR="1"' '-DVERSION3DS="0.1"' \
  -I"$P/tools/hostcheck/stubs" -isystem /usr/include/x86_64-linux-gnu -isystem /usr/include/c++/13 -isystem /usr/include/x86_64-linux-gnu/c++/13 \
  -isystem "$LIBCTRU_INC" ${CITRO3D_INC:+-isystem "$CITRO3D_INC"} -I"$P/UAE3DS_SDL/include" -I"$P/UAE3DS_SDL/include/SDL" \
  -I"$P/src" -I"$P/src/3ds" -I"$P/src/gp2x" -I"$P/src/gp2x/menu" -I"$P/src/menu" -I"$P/src/vkbd" -I"$P/src/m68k" -I"$P/src/m68k/fame" -I"$P/src/include" "$@"
