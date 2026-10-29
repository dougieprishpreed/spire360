#!/usr/bin/env bash
# Builds default.xex with the open xSDK toolchain (Linux, or WSL on Windows):
#
#   XSDK=... VULKAN4XENON=... GLSLANG=... XSDK_TOOLCHAIN=... tools/xsdk/build.sh
#
# XSDK and VULKAN4XENON: checkouts of xenosworks/xsdk and vulkan4xenon,
# GLSLANG a glslang binary (glslang-xenon's host build), XSDK_TOOLCHAIN
# where xsdk's tools/toolchain/build.sh installed GCC. Spire360 follows the
# latest of both: it builds on what the checkouts hold, and says when
# their origin has moved on (the log's first lines name the commits).
#
# The translated logic comes from out/logic/c (tools/build_logic.py), or
# SPIRE_LOGIC_C. Output: $SPIRE_XSDK_BUILD (default
# /opt/spire360-xsdk/output)/default.xex, with the licences it ships under
# in licenses/. JOBS compilers at once (default 2; GCC takes ~1 GB on each
# of the logic's large files).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
: "${XSDK:?Set XSDK to the xenosworks/xsdk checkout}"
: "${VULKAN4XENON:?Set VULKAN4XENON to its checkout}"
: "${GLSLANG:?Set GLSLANG to a glslang binary, such as glslang-xenon/build-host/StandAlone/glslang}"
: "${XSDK_TOOLCHAIN:?Set XSDK_TOOLCHAIN to the compiler installation prefix}"
OUT="${SPIRE_XSDK_BUILD:-/opt/spire360-xsdk/output}"
LOGIC="${SPIRE_LOGIC_C:-$ROOT/out/logic/c}"
JOBS="${JOBS:-2}"
mkdir -p "$OUT"
if [ ! -f "$LOGIC/logic.h" ]; then
  echo "No translated logic in $LOGIC: run tools/build_logic.py first (or set SPIRE_LOGIC_C)" >&2
  exit 1
fi

for dep in XSDK VULKAN4XENON; do
  dir="${!dep}"
  head="$(git -C "$dir" rev-parse --short HEAD)"
  origin="$(git -C "$dir" ls-remote origin HEAD 2>/dev/null | cut -c1-7 || true)"
  if [ -n "$origin" ] && [ "$origin" != "$head" ]; then
    echo "$dep: $head, but origin is at $origin: update the checkout to build on the latest"
  else
    echo "$dep: $head"
  fi
done

TC="$XSDK_TOOLCHAIN/bin/powerpc-xsdk-elf-"
make -C "$XSDK" -j"$JOBS" TOOLCHAIN="$TC" build/libxsdk.a build/host/libxsc.so
make -C "$VULKAN4XENON" -j"$JOBS" XSDK="$XSDK" TOOLCHAIN="$TC" build/libvulkan4xenon.a
bash "$ROOT/tools/xsdk/shaders.sh" "$OUT/shaders"

# The log's first line names the commit ("+": local changes; a Windows
# checkout's CRLF copies of LF files are no change).
COMMIT="$(git -C "$ROOT" rev-parse --short HEAD 2>/dev/null || true)"
if [ -n "$COMMIT" ] &&
  [ -n "$(git -C "$ROOT" -c core.autocrlf=input status --porcelain --untracked-files=no 2>/dev/null)" ]; then
  COMMIT="$COMMIT+"
fi
DATE="$(git -C "$ROOT" log -1 --format=%cI 2>/dev/null || true)"

cmake -S "$ROOT/xsdk" -B "$OUT/cmake" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$XSDK/cmake/xsdk.cmake" -DXSDK_TOOLCHAIN="$XSDK_TOOLCHAIN" -DCMAKE_BUILD_TYPE=Release \
  -DVULKAN4XENON="$VULKAN4XENON" -DSPIRE_SHADERS="$OUT/shaders" -DSPIRE_LOGIC_C="$LOGIC" \
  -DSPIRE_GIT_COMMIT="$COMMIT" -DSPIRE_GIT_DATE="$DATE"
cmake --build "$OUT/cmake" -j"$JOBS"

cp "$OUT/cmake/spire360.xex" "$OUT/default.xex"
mkdir -p "$OUT/licenses"
cp "$XSDK/LICENSE" "$OUT/licenses/xsdk.txt"
cp "$XSDK/THIRD_PARTY_NOTICES.md" "$OUT/licenses/xsdk-third-party.md"
cp "$VULKAN4XENON/LICENSE" "$OUT/licenses/vulkan4xenon.txt"
cp "$XSDK_TOOLCHAIN"/share/*-licenses/* "$OUT/licenses/" 2>/dev/null || true
echo "Built $OUT/default.xex ($(stat -c %s "$OUT/default.xex") bytes, source $COMMIT)"
