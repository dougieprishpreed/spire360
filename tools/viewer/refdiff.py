#!/usr/bin/env python3
"""Compares spire_view renders with Godot's own renders of the same scenes.

    python3 tools/viewer/refdiff.py <pack.s3p> <refs_dir> <out_dir> [scene ...]

<refs_dir> holds <name>.png per scene, rendered by Godot 4.5.1 from the
game files (tools/godot/shot.gd under xvfb, gl_compatibility, 1920x1080; see
tools/viewer/README.md). Scenes default to the converter's bundle roots.
Renders use --no-scripts: Godot's render has no game C# either.

Writes <out_dir>/<name>.png (ours), <name>_cmp.png (ours | Godot | diff)
and prints per-scene numbers: mean absolute difference and the share of
pixels off by more than 16/255.
"""
import os
import subprocess
import sys

import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DEFAULT_SCENES = [
    "scenes/screens/main_menu.tscn", "scenes/rooms/combat_room.tscn", "scenes/cards/card.tscn",
    "scenes/combat/creature.tscn", "scenes/combat/player_hand.tscn", "scenes/ui/top_bar.tscn",
    "scenes/screens/map/map_screen.tscn", "scenes/screens/rewards_screen.tscn",
]


def main():
    if len(sys.argv) < 4:
        sys.exit(__doc__)
    pack, refs, out = sys.argv[1:4]
    scenes = sys.argv[4:] or DEFAULT_SCENES
    os.makedirs(out, exist_ok=True)
    view = os.path.join(ROOT, "out", "engine", "spire_view")
    print(f"{'scene':28} {'mean':>6} {'>16':>7}  notes")
    for sc in scenes:
        name = os.path.splitext(os.path.basename(sc))[0]
        ref_path = os.path.join(refs, name + ".png")
        ours_path = os.path.join(out, name + ".png")
        r = subprocess.run([view, pack, sc, ours_path, "--no-scripts"], capture_output=True, text=True)
        notes = [ln.strip() for ln in r.stdout.splitlines()[1:]]
        if r.returncode != 0 or not os.path.exists(ref_path):
            print(f"{name:28} {'-':>6} {'-':>7}  {(r.stderr or 'no reference').strip()[:80]}")
            continue
        a = np.asarray(Image.open(ours_path).convert("RGB")).astype(np.int16)
        b = np.asarray(Image.open(ref_path).convert("RGB")).astype(np.int16)
        if a.shape != b.shape:
            print(f"{name:28} size mismatch {a.shape} vs {b.shape}")
            continue
        d = np.abs(a - b)
        big = (d.max(axis=2) > 16)
        print(f"{name:28} {d.mean():6.2f} {big.mean() * 100:6.2f}%  {'; '.join(notes)[:150]}")
        heat = np.zeros_like(a, dtype=np.uint8)
        heat[..., 0] = np.clip(d.max(axis=2) * 4, 0, 255)
        w = a.shape[1] // 2
        h = a.shape[0] // 2
        cmp = Image.new("RGB", (w * 3, h))
        cmp.paste(Image.fromarray(a.astype(np.uint8)).resize((w, h)), (0, 0))
        cmp.paste(Image.fromarray(b.astype(np.uint8)).resize((w, h)), (w, 0))
        cmp.paste(Image.fromarray(heat).resize((w, h)), (w * 2, 0))
        cmp.save(os.path.join(out, name + "_cmp.png"))


if __name__ == "__main__":
    main()
