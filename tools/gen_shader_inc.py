#!/usr/bin/env python3
"""HLSL source -> C string table (engine/gfx/shaders/<name>_hlsl.inc), for
the PC D3D9 renderer, which compiles its shaders at startup. The Xbox 360
build compiles the .hlsl with the XDK's fxc instead.

    python3 tools/gen_shader_inc.py engine/gfx/shaders/canvas.hlsl
"""
import os
import sys

for src in sys.argv[1:]:
    out = os.path.splitext(src)[0] + "_hlsl.inc"
    with open(src, encoding="utf-8") as f:
        lines = f.read().splitlines()
    with open(out, "w", newline="\n") as f:
        f.write(f"/* Generated from {os.path.basename(src)} by tools/gen_shader_inc.py. Do not edit. */\n")
        for ln in lines:
            esc = ln.replace("\\", "\\\\").replace('"', '\\"')
            f.write(f'"{esc}\\n",\n')
    print(out)
