#!/usr/bin/env python3
"""Model coverage: the models ModelDb lists that no suite drives by
construction, against what the coverage traces show of them, so a kind of
content no run reaches is named instead of met in a play test.

    python tools/model_coverage.py --runs work/covNN [--runs work/scenNN]

The content list is the first run's content/content.jsonl (the logic's
`content` command). Seen:
  powers        a power cue ("power", "power_vfx") or on a creature's powers
  orbs          a channelled orb (the state's orbs)
  monsters      an enemy or a pet on a fight's state
  enchantments  the enchantments suite's case finished ok, or a card carrying
                it (its enchantment_view icon)
  afflictions   the afflictions suite's case finished ok
  relic flashes a relic whose code calls Flash() (its trigger, read from the
                decompiled game in decomp/ when it is there) flashing in a
                run (a relic_flash cue): its trigger ran
(cards, relics, potions, events and encounters each have a suite case of
their own; content_coverage's report says how each went.)

Verdicts: notes/presentation_verdicts.tsv rows of category "model", the key
"<kind>:<ID>" (fnmatch), for what no solo run can show (multiplayer-only
cards' powers, powers nothing applies).

Prints what was never seen per kind; exits 1 while something unanswered was
not.
"""
import argparse
import fnmatch
import glob
import json
import os
import re
import sys

ICON = re.compile(r"enchantments/(?:beta/)?([a-z0-9_]+)\.png")
FLASH = re.compile(r'"t":\s*"relic_flash",\s*"relic":\s*"([A-Z0-9_]+)"')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def snake(name):
    """RelicModel class name to its id (ChemicalX -> CHEMICAL_X)."""
    return re.sub(r"(?<=[a-z0-9])(?=[A-Z])|(?<=[A-Z])(?=[A-Z][a-z])", "_", name).upper()


def flashing_relics():
    """Relics whose code calls Flash(), from the decompiled game (empty
    without decomp/)."""
    out = set()
    for path in glob.glob(os.path.join(ROOT, "decomp", "MegaCrit.Sts2.Core.Models.Relics", "*.cs")):
        with open(path, encoding="utf-8", errors="replace") as fh:
            if "Flash()" in fh.read():
                out.add(snake(os.path.basename(path)[:-3]))
    return out


def verdicts():
    rows = []
    path = os.path.join(ROOT, "notes", "presentation_verdicts.tsv")
    if not os.path.exists(path):  # a local file, none in a fresh checkout
        return rows
    with open(path, encoding="utf-8") as fh:
        for line in fh:
            parts = line.rstrip("\r\n").split("\t")
            if len(parts) >= 3 and parts[0] == "model":
                rows.append((parts[1], parts[2], parts[3] if len(parts) > 3 else ""))
    return rows


def content_of(run):
    path = os.path.join(run, "content", "content.jsonl")
    with open(path, encoding="utf-8") as fh:
        for line in fh:
            if line.startswith("{") and '"type":"content"' in line.replace(" ", ""):
                return json.loads(line)
    raise SystemExit(f"no content response in {path}")


def creatures(d):
    for key in ("enemies", "pets", "allies"):
        for c in d.get(key) or []:
            if isinstance(c, dict):
                yield c
    for key in ("player", "osty"):
        c = d.get(key)
        if isinstance(c, dict):
            yield c


def scan(path, seen):
    with open(path, encoding="utf-8", errors="replace") as fh:
        for line in fh:
            if not line.startswith("{"):
                continue
            try:
                d = json.loads(line)
            except ValueError:
                continue
            if not isinstance(d, dict) or d.get("type") == "content":
                continue
            for p in d.get("player_powers") or []:
                if isinstance(p, dict) and isinstance(p.get("id"), str):
                    seen["powers"].add(p["id"])
            for c in creatures(d):
                if c is not d.get("player") and c is not d.get("osty") and isinstance(c.get("id"), str):
                    seen["monsters"].add(c["id"])
                for p in c.get("powers") or []:
                    if isinstance(p, dict) and isinstance(p.get("id"), str):
                        seen["powers"].add(p["id"])
            for o in d.get("orbs") or []:
                if isinstance(o, dict) and isinstance(o.get("id"), str):
                    seen["orbs"].add(o["id"])
            for step in d.get("steps") or []:
                for cue in step if isinstance(step, list) else []:
                    if isinstance(cue, dict) and cue.get("t") in ("power", "power_vfx") and isinstance(cue.get("power"), str):
                        seen["powers"].add(cue["power"])
            if "relic_flash" in line:
                for m in FLASH.finditer(line):
                    seen["relic flashes"].add(m.group(1))
            if "enchantments/" in line:
                for m in ICON.finditer(line):
                    seen["enchantments"].add(m.group(1).upper())


def suite_ok(run, suite):
    path = os.path.join(run, "report.json")
    if not os.path.exists(path):
        return set()
    with open(path, encoding="utf-8") as fh:
        report = json.load(fh)
    return {c["name"] for c in report.get("cases") or [] if c.get("suite") == suite and c.get("status") == "ok"}


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--runs", action="append", required=True, help="a content_coverage (or scenarios) work dir")
    a = ap.parse_args()
    content = content_of(a.runs[0])
    kinds = {
        "powers": set(content.get("power_ids") or []),
        "orbs": set(content.get("orbs") or []),
        "monsters": set(content.get("monsters") or []),
        "enchantments": set(content.get("enchantments") or []),
        "afflictions": set(content.get("afflictions") or []),
        "relic flashes": flashing_relics() & {r["id"] for r in content.get("relics") or []},
    }
    answered = verdicts()
    seen = {k: set() for k in kinds}
    for run in a.runs:
        for path in sorted(glob.glob(os.path.join(run, "**", "*.jsonl"), recursive=True)):
            if path.endswith(".ledger.jsonl"):
                continue
            scan(path, seen)
        seen["enchantments"] |= suite_ok(run, "enchantments")
        seen["afflictions"] |= suite_ok(run, "afflictions")
    missing = 0
    for kind, ids in kinds.items():
        if not ids:
            print(f"{kind}: none listed (the content command predates them?)")
            missing += 1
            continue
        never = sorted(ids - seen[kind])
        known = [i for i in never if any(fnmatch.fnmatch(f"{kind}:{i}", k) for k, _, _ in answered)]
        open_ = [i for i in never if i not in known]
        print(f"{kind}: {len(ids) - len(never)}/{len(ids)} seen" + (f", {len(known)} answered by verdicts" if known else "")
              + (f"; never: {', '.join(open_)}" if open_ else ""))
        missing += len(open_)
    return 1 if missing else 0


if __name__ == "__main__":
    sys.exit(main())
