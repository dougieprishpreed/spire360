#!/usr/bin/env python3
"""Every TestMode.IsOn / IsOff read in the game's logic (its Nodes left out):
headless the logic runs with TestMode on, so each read skips or takes a
branch the game plays differently (Lord's Parasol's shop opening and its
waits, a monster's position cue). Each read needs an answer:

- a patch on its method (logic/patches.json: a redirect, a hook before or
  after it, or a testmode.* entry naming it in in_types), or
- a testmode / godot verdict in notes/presentation_verdicts.tsv
  (Class.Method, fnmatch).

    python tools/testmode_audit.py [--decomp decomp] [--all]

Prints the reads without an answer (--all: every read and its answer) and
exits 1 when there are any: after a game update, the new branches to look at.
"""
import argparse
import fnmatch
import glob
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SIG = re.compile(r"^\t(?:public|private|protected|internal)[^=;(]*?\b([A-Za-z_][A-Za-z0-9_]*)\s*(?:<[^>]*>)?\(")
PROP = re.compile(r"^\t(?:public|private|protected|internal)[^=;(]*?\b([A-Za-z_][A-Za-z0-9_]*)\s*(?:=>|$)")


def patch_answers():
    """Type::Method (or Type) -> the patch ids that answer its reads."""
    data = json.load(open(os.path.join(ROOT, "logic", "patches.json"), encoding="utf-8"))
    items = data if isinstance(data, list) else data["patches"]
    out = {}
    for e in items:
        if not e.get("type", "").endswith("TestMode") and e.get("method"):
            out.setdefault(f"{e['type']}::{e['method']}", []).append(e["id"])
        for t in e.get("in_types", []):
            out.setdefault(t, []).append(e["id"])
    return out


def verdicts():
    rows = []
    for line in open(os.path.join(ROOT, "notes", "presentation_verdicts.tsv"), encoding="utf-8"):
        parts = line.rstrip("\n").split("\t")
        if len(parts) >= 3 and parts[0] in ("testmode", "godot"):
            rows.append((parts[1], parts[2]))
    return rows


def reads(decomp):
    """(namespace.Class::Method, IsOn|IsOff, the line) for each read."""
    for path in sorted(glob.glob(os.path.join(decomp, "*", "*.cs"))):
        ns = os.path.basename(os.path.dirname(path))
        if ".Nodes" in ns or "TestSupport" in ns:
            continue
        cls = os.path.basename(path)[:-3]
        method = None
        for line in open(path, encoding="utf-8", errors="replace"):
            m = SIG.match(line) or PROP.match(line)
            if m:
                method = m.group(1)
            if "TestMode.IsOn" in line or "TestMode.IsOff" in line:
                yield f"{ns}.{cls}::{method}", "IsOff" if "IsOff" in line else "IsOn", line.strip()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--decomp", default=os.path.join(ROOT, "decomp"))
    ap.add_argument("--all", action="store_true")
    a = ap.parse_args()
    patches, rows = patch_answers(), verdicts()
    seen, missing = set(), 0
    for key, which, text in reads(a.decomp):
        if (key, which) in seen:
            continue
        seen.add((key, which))
        cls_key = key.split("::")[0]
        answer = ",".join(patches.get(key) or patches.get(cls_key) or [])
        if not answer:
            short = key.split(".")[-1].replace("::", ".")
            answer = next(("verdict:" + s for p, s in rows if fnmatch.fnmatchcase(short, p)), "")
        if not answer:
            missing += 1
        if a.all or not answer:
            print(f"{answer or '-':40.40s} {which:5s} {key}  | {text[:100]}")
    print(f"{len(seen)} reads, {missing} without an answer")
    return 1 if missing else 0


if __name__ == "__main__":
    sys.exit(main())
