#!/bin/sh
# Builds the mock game assembly, patches it, and runs the checks against it.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
OUT=${1:-$HERE/build/patcher}
mkdir -p "$OUT"
dotnet build "$HERE/PatchTest" -c Release -o "$OUT/app" -v q -nologo >/dev/null
dotnet build "$HERE/../Spire.Patcher" -c Release -o "$OUT/patcher" -v q -nologo >/dev/null
dotnet "$OUT/patcher/Spire.Patcher.dll" --in "$OUT/app/sts2.dll" --out "$OUT/patched" \
    --config "$HERE/mock_patches.json" --hookbridge "$OUT/app/Spire.HookBridge.dll" \
    --report "$OUT/report.json"
cp "$OUT/patched/sts2.dll" "$OUT/app/sts2.dll"
dotnet "$OUT/app/PatchTest.dll"
