#!/usr/bin/env python3
"""Which of the game's shaders the engine draws, and where the others are used.

    python tools/shader_survey.py work/pack-sound-effects/pack.s3p [--json out.json]

Reads every scene and resource in a pack (the converter's JSON), finds each
ShaderMaterial's shader (a .gdshader / .tres file, or an inline one named
"<entry>::<sub id>" as the engine names it) and the scenes whose nodes use
it (directly or through a material resource), and compares the list with
the shaders the engine ports: the names engine/scene/*.cpp tests for.
"""
import argparse
import json
import os
import re
import sys
import zlib

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "converter"))
from spire_convert import s3p  # noqa: E402


def load(reader, name):
    data = reader.read(name)
    try:
        return json.loads(data)
    except ValueError:
        return json.loads(zlib.decompress(data))


def ported_names(root):
    names = set()
    for d in ("engine/scene", "engine/game"):
        for f in os.listdir(os.path.join(root, d)):
            if f.endswith(".cpp"):
                text = open(os.path.join(root, d, f), encoding="utf-8").read()
                names.update(re.findall(r'"((?:shaders|scenes|images|materials)/[^"]+\.(?:gdshader|tres)[^"]*)"', text))
    return names


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("pack")
    ap.add_argument("--json")
    a = ap.parse_args()
    root = os.path.join(os.path.dirname(__file__), "..")
    r = s3p.Reader(a.pack)
    scenes = [n for n in r.entries if r.type(n) == s3p.TYPE_SCENE]
    material_shader = {}  # material resource -> shader
    docs = {}
    for n in scenes:
        try:
            docs[n] = load(r, n)
        except Exception:
            continue
    for n, d in docs.items():
        if d.get("kind") == "resource" and d.get("type") == "ShaderMaterial":
            sh = (d.get("resource") or {}).get("shader")
            if isinstance(sh, dict) and sh.get("ext"):
                material_shader[n] = sh["ext"]
            elif isinstance(sh, dict) and sh.get("sub"):
                material_shader[n] = n + "::" + sh["sub"]
    uses = {}  # shader -> set(scene)

    def note(shader, where):
        uses.setdefault(shader, set()).add(where)

    for n, d in docs.items():
        subs = d.get("sub") or {}
        mats = {}
        for sid, s in subs.items():
            if s.get("type") != "ShaderMaterial":
                continue
            sh = (s.get("props") or {}).get("shader")
            if isinstance(sh, dict) and sh.get("ext"):
                mats[sid] = sh["ext"]
            elif isinstance(sh, dict) and sh.get("sub"):
                mats[sid] = n + "::" + sh["sub"]
        text = json.dumps(d)
        for sid, sh in mats.items():
            if '{"sub": "%s"}' % sid in text:
                note(sh, n)
        for m, sh in material_shader.items():
            if '"ext": "%s"' % m in text:
                note(sh, n)
    ported = ported_names(root)
    rows = []
    for sh, where in uses.items():
        rows.append({"shader": sh, "ported": sh in ported, "scenes": len(where), "examples": sorted(where)[:5]})
    rows.sort(key=lambda x: (x["ported"], -x["scenes"], x["shader"]))
    for x in rows:
        print("%s %4d  %s  (%s)" % ("ok " if x["ported"] else "-- ", x["scenes"], x["shader"], ", ".join(x["examples"][:2])))
    missing = [x for x in rows if not x["ported"]]
    print("%d shaders used, %d ported, %d not" % (len(rows), len(rows) - len(missing), len(missing)))
    if a.json:
        json.dump(rows, open(a.json, "w"), indent=1)


if __name__ == "__main__":
    main()
