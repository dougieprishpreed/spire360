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
if [ -z "${PYTHON:-}" ]; then
    if command -v python >/dev/null 2>&1; then PYTHON=python; else PYTHON=python3; fi
fi
# The POSIX host uses fseeko/usleep; MSYS2 hides them in strict ANSI mode.
FLAGS="-std=c++98 -pedantic -D_DEFAULT_SOURCE -D_LARGEFILE_SOURCE -Wall -Wextra -Wno-long-long -O2 ${EXTRA_FLAGS:-}"
LIBS=""
case "$(uname -s)" in MSYS*|CYGWIN*) FLAGS="$FLAGS -I/usr/include/w32api"; LIBS="-lole32" ;; esac
stamp=out/engine/compiler-flags
if [ ! -f "$stamp" ] || [ "$(cat "$stamp")" != "$CXX $FLAGS" ]; then
    printf '%s\n' "$CXX $FLAGS" > "$stamp"
fi
objs=""
for f in engine/*/*.cpp tools/viewer/spire_view.cpp tools/viewer/spire_play.cpp tools/viewer/spire_check.cpp tools/viewer/spire_audio_check.cpp; do
    case "$f" in *d3d9_renderer.cpp) continue ;; esac  # Windows / XDK only
    o=out/engine/$(basename "$f" .cpp).o
    objs="$objs $o"
    # Keep the verifier to one compiler process at a time.
    if $PYTHON tools/viewer/needs_rebuild.py "$o" "$f" "$stamp" "$o.d"; then
        $CXX $FLAGS -MMD -MP -MF "$o.d" -c "$f" -o "$o"
    fi
done
engine_objs=$(echo $objs | tr ' ' '\n' | grep -v -e spire_view.o -e spire_play.o -e spire_check.o -e spire_audio_check.o | tr '\n' ' ')
$CXX -o out/engine/spire_view $engine_objs out/engine/spire_view.o $LIBS
$CXX -o out/engine/spire_play $engine_objs out/engine/spire_play.o $LIBS
$CXX -o out/engine/spire_check $engine_objs out/engine/spire_check.o $LIBS
$CXX -o out/engine/spire_audio_check $engine_objs out/engine/spire_audio_check.o $LIBS
echo "out/engine/spire_view out/engine/spire_play out/engine/spire_check out/engine/spire_audio_check"
