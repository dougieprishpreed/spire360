#!/bin/sh
# A debug spire_play for hunting the engine's memory errors:
# out/engine-dbg/spire_play, unoptimised, with frame pointers and the
# checking heap (tools/viewer/debug_heap.cpp). Its crash and heap reports
# list return addresses: addr2line -f -C -e out/engine-dbg/spire_play.exe ADDR.
set -e
cd "$(dirname "$0")/../.."
mkdir -p out/engine-dbg
CXX=${CXX:-g++}
if [ -z "${PYTHON:-}" ]; then
    if command -v python >/dev/null 2>&1; then PYTHON=python; else PYTHON=python3; fi
fi
FLAGS="-std=c++98 -pedantic -D_DEFAULT_SOURCE -D_LARGEFILE_SOURCE -Wall -Wextra -Wno-long-long -O0 -g -fno-omit-frame-pointer -DSPIRE_DEBUG_HEAP"
LIBS=""
case "$(uname -s)" in MSYS*|CYGWIN*) FLAGS="$FLAGS -I/usr/include/w32api"; LIBS="-lole32" ;; esac
stamp=out/engine-dbg/compiler-flags
if [ ! -f "$stamp" ] || [ "$(cat "$stamp")" != "$CXX $FLAGS" ]; then
    printf '%s\n' "$CXX $FLAGS" > "$stamp"
fi
objs=""
for f in engine/*/*.cpp tools/viewer/spire_play.cpp tools/viewer/debug_heap.cpp; do
    case "$f" in *d3d9_renderer.cpp) continue ;; esac
    o=out/engine-dbg/$(basename "$f" .cpp).o
    objs="$objs $o"
    if $PYTHON tools/viewer/needs_rebuild.py "$o" "$f" "$stamp" "$o.d"; then
        $CXX $FLAGS -MMD -MP -MF "$o.d" -c "$f" -o "$o"
    fi
done
$CXX -o out/engine-dbg/spire_play $objs $LIBS
echo "out/engine-dbg/spire_play"
