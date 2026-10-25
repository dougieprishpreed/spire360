#!/usr/bin/env python3
"""The game logic's reads of the UI singletons (NCombatRoom.Instance,
NRun.Instance, NMapScreen.Instance, ...), found in the decompiled source by
the method that makes them, against the methods coverage's ledgers saw make
them (each ledger records a UI call with its caller). A read no run reached
is presentation (or rules behind a UI check) the audit has never looked at:
content the coverage harness does not drive into that path.

    python tools/ui_reach_audit.py --runs work/cov73 [--runs work/scen74 ...]
                                   [--decomp decomp] [--all]

Prints the unreached sites by UI type (--all: the reached ones too).
Verdicts for sites no run can reach (multiplayer, the bestiary's own
screen, debug paths) are reach rows in notes/presentation_verdicts.tsv
("reach", "NType@Class.Method" fnmatch, n/a, note).
"""
import argparse
import collections
import fnmatch
import glob
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SIG = re.compile(r"^\t(?:public|private|protected|internal)[^=;(]*?\b([A-Za-z_][A-Za-z0-9_]*)\s*(?:<[^>]*>)?\(")
PROP = re.compile(r"^\t(?:public|private|protected|internal)[^=;(]*?\b([A-Za-z_][A-Za-z0-9_]*)\s*(?:=>|$)")
INST = re.compile(r"\b(N[A-Z][A-Za-z0-9]*)\.Instance\b")


def static_sites(decomp):
    """(UI type, Class.Method) -> (namespace, a line)"""
    sites = {}
    for path in glob.glob(os.path.join(decomp, "*", "*.cs")):
        ns = os.path.basename(os.path.dirname(path))
        if ".Nodes" in ns:
            continue
        cls = os.path.basename(path)[:-3]
        method = None
        for line in open(path, encoding="utf-8", errors="replace"):
            m = SIG.match(line) or PROP.match(line)
            if m:
                method = m.group(1)
            for t in INST.findall(line):
                sites.setdefault((t, f"{cls}.{method}"), (ns, line.strip()[:110]))
    return sites


def reached(runs):
    """The Class.Method callers of any UI call the ledgers saw: a method that
    ran (its singleton read can sit behind a stand-in's null answer, as
    NGoopyImpactVfx.Create's before Slimed adds it to the container)."""
    out = set()
    for run in runs:
        for path in glob.glob(os.path.join(run, "**", "*.ledger.jsonl"), recursive=True):
            for line in open(path, encoding="utf-8", errors="replace"):
                if '"caller"' not in line:
                    continue
                try:
                    e = json.loads(line)
                except ValueError:
                    continue
                caller = e.get("caller") or ""
                if "::" not in caller:
                    continue
                c, m = caller.split("::", 1)
                # an async method's state machine: <OnPlay>d__3::MoveNext
                m = re.sub(r"^<([^>]*).*$", r"\1", m)
                # a property's accessor: the sites name the property
                m = re.sub(r"^[gs]et_", "", m)
                out.add(f"{c.split('.')[-1].split('+')[0]}.{m}")
    return out


def verdicts():
    rows = []
    for line in open(os.path.join(ROOT, "notes", "presentation_verdicts.tsv"), encoding="utf-8"):
        p = line.rstrip("\n").split("\t")
        if len(p) >= 3 and p[0] == "reach":
            rows.append((p[1], p[2], p[3] if len(p) > 3 else ""))
    return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--runs", action="append", required=True)
    ap.add_argument("--decomp", default=os.path.join(ROOT, "decomp"))
    ap.add_argument("--all", action="store_true")
    a = ap.parse_args()
    sites, seen, rows = static_sites(a.decomp), reached(a.runs), verdicts()
    by_type = collections.defaultdict(list)
    open_n = 0
    for (t, where), (ns, text) in sorted(sites.items()):
        hit = where in seen
        v = next((s for p, s, _ in rows if fnmatch.fnmatchcase(f"{t}@{where}", p)), None)
        if not hit and not v:
            open_n += 1
        if a.all or (not hit and not v):
            by_type[t].append(f"  {'reached' if hit else (v or '-'):8s} {where}  | {text}")
    for t in sorted(by_type):
        print(f"{t} ({len(by_type[t])})")
        print("\n".join(by_type[t]))
    print(f"{len(sites)} sites, {sum(1 for (_, w) in sites if w in seen)} reached, {open_n} neither reached nor given a verdict")
    return 1 if open_n else 0


if __name__ == "__main__":
    sys.exit(main())
