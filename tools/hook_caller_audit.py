#!/usr/bin/env python3
"""The game's hooks (MegaCrit.Sts2.Core.Hooks.Hook's static methods: what
relics, powers and cards react to) whose only callers are the UI's nodes.
Headless the UI does not run, so such a hook fires only where the logic's
driver calls it itself (Lasting Candy counts its combat rewards in
BeforeCombatRewardOffered, which NCombatUi's OfferRoomEndRewards path
reaches; the driver generated the rewards without it).

    python tools/hook_caller_audit.py [--decomp decomp] [--all]

For each hook: its callers by type (UI or game logic) and whether
logic/Spire.Logic calls it. A game-logic caller that is itself called only
from the UI (by name, outside its declaration: CombatRoom.
OfferRoomEndRewards from NCombatUi) counts as the UI's. Prints the hooks
only the UI reaches that the logic does not call, directly or through that
caller (--all: every hook); exits 1 when there are any.
"""
import argparse
import collections
import glob
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DECL = re.compile(r"^\tpublic static (?:async )?[\w<>\[\], ?]+ (\w+)\(")
SIG = re.compile(r"^\t(?:public|private|protected|internal)[^=;(]*?\b([A-Za-z_][A-Za-z0-9_]*)\s*(?:<[^>]*>)?\(")
PROP = re.compile(r"^\t(?:public|private|protected|internal)[^=;(]*?\b([A-Za-z_][A-Za-z0-9_]*)\s*(?:=>|$)")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--decomp", default=os.path.join(ROOT, "decomp"))
    ap.add_argument("--all", action="store_true")
    a = ap.parse_args()
    hook_file = os.path.join(a.decomp, "MegaCrit.Sts2.Core.Hooks", "Hook.cs")
    hooks = []
    for line in open(hook_file, encoding="utf-8", errors="replace"):
        m = DECL.match(line)
        if m and m.group(1) not in hooks:
            hooks.append(m.group(1))
    callers = collections.defaultdict(set)  # hook -> {"ui:Type.Method" | "logic:Type.Method"}
    call = re.compile(r"\bHook\.(\w+)\(")
    logic_lines, ui_lines = [], []  # the code's lines but declarations
    props = set()  # the callers that are properties (read without a call)
    for path in glob.glob(os.path.join(a.decomp, "*", "*.cs")):
        if os.path.samefile(path, hook_file):
            continue
        ns = os.path.basename(os.path.dirname(path))
        cls = os.path.basename(path)[:-3]
        method = None
        for line in open(path, encoding="utf-8", errors="replace"):
            m = SIG.match(line)
            p = None if m else PROP.match(line)
            if m or p:
                method = (m or p).group(1)
                if p:
                    props.add(method)
            elif "MethodName." not in line:
                (ui_lines if ".Nodes" in ns else logic_lines).append(line)
            for h in call.findall(line):
                kind = "ui" if ".Nodes" in ns else "logic"
                callers[h].add(f"{kind}:{cls}.{method}")
    ours = ""
    for path in glob.glob(os.path.join(ROOT, "logic", "Spire.Logic", "*.cs")):
        ours += open(path, encoding="utf-8", errors="replace").read()

    def reached_outside_ui(name):
        """A method name called (a property read) from game logic or from
        logic/Spire.Logic."""
        pat = re.compile(r"[.\s(!]" + re.escape(name) + (r"\b" if name in props else r"\("))
        return any(pat.search(l) for l in logic_lines) or pat.search(ours) is not None

    via = {}  # a logic caller -> reached only from the UI
    for h, cs in callers.items():
        for c in cs:
            if c.startswith("logic:") and c not in via:
                via[c] = not reached_outside_ui(c.split(".")[-1])
    # hook verdicts (notes/presentation_verdicts.tsv: hook, its name, n/a;
    # a local file, none in a fresh checkout)
    reviewed = {}
    path = os.path.join(ROOT, "notes", "presentation_verdicts.tsv")
    for line in open(path, encoding="utf-8") if os.path.exists(path) else []:
        p = line.rstrip("\n").split("\t")
        if len(p) >= 3 and p[0] == "hook":
            reviewed[p[1]] = p[2]
    missing = 0
    for h in hooks:
        cs = sorted(callers.get(h, ()))
        ui_only = cs and all(c.startswith("ui:") or via.get(c) for c in cs)
        cs = [c + (" (reached from the UI only)" if via.get(c) else "") for c in cs]
        driver = re.search(r"\bHook\." + h + r"\(", ours) is not None
        gap = ui_only and not driver and h not in reviewed
        missing += gap
        if a.all or gap:
            mark = "GAP" if gap else ("driver" if driver else (reviewed.get(h) or ("none" if not cs else "game")))
            print(f"{mark:6s} {h}: {', '.join(cs) or '(no callers)'}")
    print(f"{len(hooks)} hooks, {missing} called only by the UI and not by the logic")
    return 1 if missing else 0


if __name__ == "__main__":
    sys.exit(main())
