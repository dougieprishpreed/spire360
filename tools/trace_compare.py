#!/usr/bin/env python3
"""Compares two folders of random_runs.py traces (*.jsonl), ignoring what the
presentation adds: the regression check after a logic change.

    python tools/random_runs.py --cmd "dotnet out/logic/jit/Spire.Logic.dll" \
        --runs 3 --trace-dir traces/new --data-dir data
    python tools/trace_compare.py traces/base traces/new [--histogram]

Both folders are normalised the same way before the comparison:
- decisions the self-play agent answers differently from the older traces
  (rewards, treasure, a finished rest site) are dropped with the command
  after them;
- presentation-only keys (DROP below) are ignored at every depth.

Prints the first differing line per file and "differences: N" (0 = same
game). --histogram counts which keys differ, to see what a change moved.
Traces hold game text: keep them out of git (traces/ is ignored).
"""
import argparse
import json
import os
import sys
from collections import Counter

DROP = {
    "instance_id",
    "audio",  # NRunMusicController's presentation selection and parameters
    "combat_visuals",  # visual-only encounter behind a combat event's options
    "steps", "tips", "background", "colorless", "card_removal_used", "orb_slots", "orbs", "score_lines", "score",
    "death_quote", "victory_text", "jitter", "room", "look", "max_potions", "ascension", "at_second_boss",
    "ancient", "point_type", "at_boss", "total_floor", "second_boss", "description_by_target",
    "description_multi", "text_bbcode", "relic", "will_kill", "event_id", "layout", "button_color",
    "title_text", "description_bbcode", "kind", "prompt", "cancelable", "manual_confirm", "upgrade_preview",
    "usage", "usable", "discardable", "use_label",
    "pets",  # the pets beside the player (their own state stays in the steps)
    "ancient_border", "ancient_text_bg", "enchantment_view", "overlay",  # a card's look (NCard)
    "death_fade", "death_padding", "death_anim_length",  # a monster's death (NCreature.AnimDie)
    "run_time", "icon_base", "relic_icon_base", "inspect_upgrade", "inspect", "item_views",  # NRunTimer (wall clock)
    "crystal_sphere",  # the finished NCrystalSphereScreen under its rewards
    "spine_ops", "phobia_skin",  # MonsterModel.SetupSkins / OnPhobiaModeToggled on the spine sprite
    "anim_init", "play",  # what the CreatureAnimator plays
}


def normalise(path):
    """The trace's lines, without the decisions the agent answers its own way."""
    out = []
    skip = False
    for line in open(path, encoding="utf-8").read().splitlines():
        if skip:
            skip = False
            if line.startswith(">"):
                continue
        try:
            d = json.loads(line)
        except ValueError:
            d = None
        if isinstance(d, dict):
            kind = d.get("decision")
            if kind in ("rewards", "treasure") or (kind == "rest_site" and d.get("done")):
                skip = True
                continue
            if kind == "rest_site":
                for o in d.get("options", []):
                    o.pop("title", None)
                    o.pop("description", None)
        out.append(d if d is not None else line)
    return out


def strip(o):
    if isinstance(o, dict):
        return {k: strip(v) for k, v in o.items() if k not in DROP}
    if isinstance(o, list):
        return [strip(x) for x in o]
    return o


def diff(x, y, p, out):
    if type(x) != type(y):
        out.append((p, "type"))
    elif isinstance(x, dict):
        for k in sorted(set(x) | set(y)):
            if k not in x or k not in y:
                out.append((p + "/" + k, "only " + ("new" if k in y else "old")))
            else:
                diff(x[k], y[k], p + "/" + k, out)
    elif isinstance(x, list):
        if len(x) != len(y):
            out.append((p, "len"))
        for u, v in zip(x, y):
            diff(u, v, p + "/#", out)
    elif x != y:
        out.append((p, "value"))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("base")
    ap.add_argument("new")
    ap.add_argument("--histogram", action="store_true", help="count the differing keys")
    a = ap.parse_args()
    bad = 0
    hist = Counter()
    for f in sorted(os.listdir(a.base)):
        if not f.endswith(".jsonl"):
            continue
        if not os.path.exists(os.path.join(a.new, f)):
            print(f, "missing in", a.new)
            bad += 1
            continue
        la = [strip(x) for x in normalise(os.path.join(a.base, f))]
        lb = [strip(x) for x in normalise(os.path.join(a.new, f))]
        if len(la) != len(lb):
            print(f, "length", len(la), len(lb))
            bad += 1
        else:
            for i, (x, y) in enumerate(zip(la, lb)):
                if x != y:
                    print(f, "line", i)
                    bad += 1
                    break
        if a.histogram:
            for x, y in zip(la, lb):
                o = []
                diff(x, y, "", o)
                hist.update(set(o))
    print("differences:", bad)
    for k, v in hist.most_common(30):
        print(v, k)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
