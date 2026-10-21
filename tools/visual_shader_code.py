#!/usr/bin/env python3
"""The shader code of one of the game's VisualShader resources, as Godot
4.5.1 generates it (for porting the node-graph shaders by hand).

    python tools/visual_shader_code.py <pck> <res path> [--godot PATH]

e.g. shaders/vfx/vfx_doom_flame_shader.tres. The resource is copied out of
the pck into a temporary project (with the resources it refers to) and
loaded by tools/godot/shader_code.gd. The printed code is the game's: keep
it out of git.
"""
import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "converter"))
from spire_convert import fonts as fontconv, resources  # noqa: E402
from spire_convert.pck import Pck  # noqa: E402


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("pck")
    ap.add_argument("res")
    ap.add_argument("--godot")
    a = ap.parse_args()
    godot = fontconv.find_godot(a.godot)
    if not godot:
        raise SystemExit("no Godot 4.5.1 (--godot, $SPIRE_GODOT or `convert.py fetch-godot`)")
    src = resources.PckSource(Pck(a.pck))
    work = tempfile.mkdtemp(prefix="spire_vs_")
    try:
        with open(os.path.join(work, "project.godot"), "w") as f:
            f.write("config_version=5\n")
        todo, done = [a.res], set()
        while todo:
            p = todo.pop()
            if p in done or not src.exists(p):
                continue
            done.add(p)
            data = src.read(p)
            out = os.path.join(work, p)
            os.makedirs(os.path.dirname(out), exist_ok=True)
            with open(out, "wb") as f:
                f.write(data)
            if p.endswith((".tres", ".gdshader", ".gdshaderinc")):
                text = data.decode("utf-8", "replace")
                todo += [m for m in re.findall(r'path="res://([^"]+\.(?:tres|gdshader|gdshaderinc))"', text)]
                todo += [m for m in re.findall(r'#include "res://([^"]+)"', text)]
        script = os.path.join(HERE, "godot", "shader_code.gd")
        r = subprocess.run([godot, "--headless", "--path", work, "--script", script, "--", "res://" + a.res],
                           capture_output=True, text=True, timeout=300)
        out = r.stdout.split("----8<----", 1)
        if r.returncode != 0 or len(out) < 2:
            raise SystemExit(f"godot failed ({r.returncode}): {(r.stderr or r.stdout)[-800:]}")
        print(out[1].strip())
    finally:
        shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    main()
