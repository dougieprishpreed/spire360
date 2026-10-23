#!/usr/bin/env python3
"""The presentation audit: where the game's logic meets its Godot UI layer
(what the headless logic leaves out) against what the client plays.

    dotnet out/logic/apidump/Spire.ApiDump.dll --ui-surface logic/lib/game/sts2.dll work/ui_surface.json
    python tools/ui_audit.py work/ui_surface.json [--all]

For each UI method game code calls it prints the callers and whether the
client's sources name it ("Class.Method" or the class in a handler table).
Queries (getters, lookups), the developer console, multiplayer and the
debug helpers are left out unless --all. The verdicts that need reading
the code are kept in notes/UI_AUDIT.md.
"""
import argparse
import json
import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

QUERY = re.compile(r"^(get_|set_|Is|Has|Find|Get|Try|Can|Should|Format|ParsePath|op_|add_|remove_)")
IGNORED_CALLERS = re.compile(r"DevConsole|AutoSlay|Multiplayer|Platform\.|Debug\.|TestSupport|Modding|MemoryProfiler|Achievements")


def engine_text():
    parts = []
    for d in ("engine/game", "engine/audio", "engine/scene", "tools/viewer", "logic/Spire.Logic"):
        for dp, _, fs in os.walk(os.path.join(ROOT, d)):
            for f in fs:
                if f.endswith((".cpp", ".h", ".cs")):
                    parts.append(open(os.path.join(dp, f), encoding="utf-8", errors="replace").read())
    return "\n".join(parts)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("surface")
    ap.add_argument("--all", action="store_true")
    a = ap.parse_args()
    d = json.load(open(a.surface, encoding="utf-8"))
    text = engine_text()
    rows = []
    for callee, callers in d["ui_calls"].items():
        cls, meth = callee.rsplit("::", 1)
        short = cls.split(".")[-1].split("/")[-1]
        callers = [c for c in callers if a.all or not IGNORED_CALLERS.search(c)]
        if not callers:
            continue
        if not a.all and (QUERY.match(meth) or meth in ("Create",) and short.endswith("Screen")):
            pass
        query = bool(QUERY.match(meth))
        named = (short + "." + meth) in text or ('"' + short + '"') in text
        rows.append((query, named, short, meth, callers))
    rows.sort(key=lambda r: (r[0], r[1], r[2], r[3]))
    for query, named, short, meth, callers in rows:
        if query and not a.all:
            continue
        mark = "ok " if named else "-- "
        owners = ", ".join(sorted(set(c.split("::")[0].split(".")[-1] + "." + c.split("::")[1] for c in callers)))
        print(f"{mark}{short}.{meth}  <- {owners[:300]}")


if __name__ == "__main__":
    main()
