#!/usr/bin/env python3
"""Print a shader's code from a pack: a .gdshader/.tres file name, or an
inline one as "<entry>::<sub id>" (shader_survey.py's names).

    python tools/shader_code.py work/pack-80/pack.s3p shaders/scrolling_smoke.tres
"""
import json
import os
import sys
import zlib

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "converter"))
from spire_convert import pck, s3p  # noqa: E402

GAME_PCK = os.environ.get("SPIRE_GAME_PCK", r"F:\Program Files (x86)\Slay the Spire 2\SlayTheSpire2.pck")


def load(r, name):
    data = r.read(name)
    try:
        return json.loads(data)
    except ValueError:
        try:
            return json.loads(zlib.decompress(data))
        except Exception:
            return data.decode("utf-8", "replace")


def main():
    r = s3p.Reader(sys.argv[1])
    for name in sys.argv[2:]:
        entry, _, sub = name.partition("::")
        if entry not in r.entries:
            # .gdshader files stay in the game's pck (the engine ports them by name)
            print(pck.Pck(GAME_PCK).read(entry).decode("utf-8", "replace"))
            continue
        d = load(r, entry)
        if isinstance(d, str):
            print(d)
            continue
        if sub:
            print(((d.get("sub") or {}).get(sub) or {}).get("props", {}).get("code", "(no code)"))
        elif d.get("kind") == "resource":
            res = d.get("resource") or {}
            print(res.get("code") or json.dumps(res, indent=1)[:4000])
        else:
            print(json.dumps(d, indent=1)[:4000])


if __name__ == "__main__":
    main()
