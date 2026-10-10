#!/bin/sh
# Builds the PC viewer with the engine's strict C++03 settings (what the
# XDK's VS2010 compiler accepts): out/engine/spire_view.
set -e
cd "$(dirname "$0")/../.."
mkdir -p out/engine
CXX=${CXX:-g++}
FLAGS="-std=c++98 -pedantic -Wall -Wextra -Wno-long-long -O2 ${EXTRA_FLAGS:-}"
pids=""
for f in engine/*/*.cpp tools/viewer/spire_view.cpp; do
    o=out/engine/$(basename "$f" .cpp).o
    $CXX $FLAGS -c "$f" -o "$o" &
    pids="$pids $!"
done
for p in $pids; do wait $p; done
$CXX -o out/engine/spire_view out/engine/*.o
echo "out/engine/spire_view"
