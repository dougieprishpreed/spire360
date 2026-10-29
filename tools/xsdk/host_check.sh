#!/usr/bin/env bash
# Builds spire_view on the Linux host (Debian, on its own or in WSL) with
# the Vulkan renderer (SPIRE_VK): the console's renderer drawn by any
# Vulkan 1.0 driver (Mesa's lavapipe), to compare it with the software
# renderer before a console run (spire_view ... --vk). The shaders go
# through xost as the console build's do, so a shader it refuses stops this
# too.
#
#   XSDK=... VULKAN4XENON=... GLSLANG=<glslang binary> tools/xsdk/host_check.sh
#
# Output: $SPIRE_HOST (default /opt/spire360-xsdk/host)/spire_view.
# SPIRE_VK_VALIDATE=1 in spire_view's environment turns on Khronos's
# validation layer.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
: "${XSDK:?Set XSDK to the xenosworks/xsdk checkout}"
: "${VULKAN4XENON:?Set VULKAN4XENON to its checkout}"
: "${GLSLANG:?Set GLSLANG to a glslang binary, such as glslang-xenon/build-host/StandAlone/glslang}"
OUT="${SPIRE_HOST:-/opt/spire360-xsdk/host}"
mkdir -p "$OUT/obj" "$OUT/shaders"

bash "$ROOT/tools/xsdk/shaders.sh" "$OUT/shaders"

FLAGS="-std=c++98 -D_DEFAULT_SOURCE -D_LARGEFILE_SOURCE -DSPIRE_VK -O2 -g -Wall -Wextra -Wno-long-long"
INCLUDES="-I$OUT/shaders -isystem $VULKAN4XENON/include -isystem $VULKAN4XENON/third_party/vulkan-headers/include"
{
  echo "rule cxx"
  echo "  command = g++ $FLAGS $INCLUDES -MMD -MF \$out.d -c \$in -o \$out"
  echo "  depfile = \$out.d"
  echo "rule cc"
  echo "  command = gcc -O2 $INCLUDES -c \$in -o \$out"
  echo "rule link"
  echo "  command = g++ -o \$out \$in -lvulkan -lpthread"
  objs=""
  for f in "$ROOT"/engine/*/*.cpp "$ROOT/tools/viewer/spire_view.cpp"; do
    case "$f" in *d3d9_renderer.cpp) continue ;; esac
    o="$OUT/obj/$(basename "$f" .cpp).o"
    echo "build $o: cxx $f || $OUT/shaders/spire_shaders.h"
    objs="$objs $o"
  done
  echo "build $OUT/obj/spire_shaders.o: cc $OUT/shaders/spire_shaders.c"
  echo "build $OUT/spire_view: link$objs $OUT/obj/spire_shaders.o"
} > "$OUT/build.ninja"
ninja -C "$OUT" -j "${JOBS:-2}"
echo "Built $OUT/spire_view"
