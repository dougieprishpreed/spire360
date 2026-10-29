#!/usr/bin/env bash
# The Vulkan renderer's shaders for the console build and the host check:
# GLSL from engine/gfx/shaders (tools/xsdk/vk_shaders.py), then SPIR-V and
# Xenos microcode through vulkan4xenon's tools/build_shaders.py (glslang,
# then xost at xsdk's register split, optimised): <folder>/spire_shaders.c
# and .h, and vk_shader_table.h. Compiled again only when a shader or the
# tools change; a shader xost refuses stops it.
#
#   XSDK=... VULKAN4XENON=... GLSLANG=... tools/xsdk/shaders.sh <folder>
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
: "${XSDK:?Set XSDK to the xenosworks/xsdk checkout}"
: "${VULKAN4XENON:?Set VULKAN4XENON to its checkout}"
: "${GLSLANG:?Set GLSLANG to a glslang binary, such as glslang-xenon/build-host/StandAlone/glslang}"
OUT="${1:?Name the folder for the shaders}"
mkdir -p "$OUT"
python3 "$ROOT/tools/xsdk/vk_shaders.py" "$OUT" > "$OUT/list.txt"
files="$(cat "$OUT/list.txt")"
cd "$OUT"
stamp="$(cat $files "$VULKAN4XENON/tools/build_shaders.py" "$XSDK/build/host/libxsc.so" | sha256sum)"
if [ ! -f spire_shaders.c ] || [ "$(cat stamp 2>/dev/null)" != "$stamp" ]; then
  python3 "$VULKAN4XENON/tools/build_shaders.py" --xost "$XSDK/xenos-toolchain" --glslang "$GLSLANG" \
    --output spire_shaders --optimize $files
  echo "$stamp" > stamp
fi
