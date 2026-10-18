#!/bin/sh
# Builds the PC tools with the engine's strict C++03 settings (what the
# XDK's VS2010 compiler accepts): out/engine/spire_view (one scene or
# decision to a PNG) and out/engine/spire_play (the game, headless).
set -e
cd "$(dirname "$0")/../.."
mkdir -p out/engine
# C99 maths VS2010 does not have; g++ -std=c++98 still declares them.
if grep -nE '\b(fminf?|fmaxf?|roundf?|truncf?|l?lroundf?|cbrtf?|log2f?|exp2f?|copysignf?|nearbyintf?|rintf?|remainderf?|fdimf?|expm1f?|log1pf?|isnan|isinf|isfinite|signbit|fpclassify|strtof|strtoll|strtoull)[[:space:]]*\(' \
        engine/*/*.cpp engine/*/*.h xdk/spire360/*.cpp; then
    echo "build.sh: C99 functions above are missing from VS2010" >&2
    exit 1
fi
CXX=${CXX:-g++}
# The POSIX host uses fseeko/usleep; MSYS2 hides them in strict ANSI mode.
FLAGS="-std=c++98 -pedantic -D_DEFAULT_SOURCE -D_LARGEFILE_SOURCE -Wall -Wextra -Wno-long-long -O2 ${EXTRA_FLAGS:-}"
objs=""
for f in engine/*/*.cpp tools/viewer/spire_view.cpp tools/viewer/spire_play.cpp tools/viewer/spire_check.cpp; do
    case "$f" in *d3d9_renderer.cpp) continue ;; esac  # Windows / XDK only
    o=out/engine/$(basename "$f" .cpp).o
    objs="$objs $o"
    # Keep the verifier to one compiler process at a time.
    $CXX $FLAGS -c "$f" -o "$o"
done
engine_objs=$(echo $objs | tr ' ' '\n' | grep -v -e spire_view.o -e spire_play.o -e spire_check.o | tr '\n' ' ')
$CXX -o out/engine/spire_view $engine_objs out/engine/spire_view.o
$CXX -o out/engine/spire_play $engine_objs out/engine/spire_play.o
$CXX -o out/engine/spire_check $engine_objs out/engine/spire_check.o
echo "out/engine/spire_view out/engine/spire_play out/engine/spire_check"
