#!/usr/bin/env python3
"""Inspect a Godot 4 .pck (or an executable with an embedded pack).

  pck_tool.py info    <game.pck>
  pck_tool.py list    <game.pck> [pattern...]        > listing.txt
  pck_tool.py extract <game.pck> <out_dir> pattern...
  pck_tool.py sample  <game.pck> <out_dir> [--max-mb 40]

"sample" copies a small representative subset (all localization tables and
project settings, plus a few files of every type) so a developer can work on
the converter without the whole pack. Output stays on your machine.
"""
import argparse
import collections
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from spire_convert.pck import Pck, PckError  # noqa: E402


def human(n):
    for unit in ("B", "KB", "MB", "GB"):
        if n < 1024 or unit == "GB":
            return f"{n:.0f} {unit}" if unit == "B" else f"{n:.1f} {unit}"
        n /= 1024


def cmd_info(pck, a):
    print(f"pack:       {pck.path}")
    print(f"format:     v{pck.format_version}, Godot {'.'.join(map(str, pck.godot_version))}, flags 0x{pck.flags:x}")
    print(f"start:      {pck.start} (non-zero = embedded in an executable)")
    print(f"files:      {len(pck.entries)}, {human(sum(e.size for e in pck.entries.values()))}")
    enc = sum(e.encrypted for e in pck.entries.values())
    if enc:
        print(f"encrypted:  {enc} files")
    by_ext = collections.Counter()
    size_ext = collections.Counter()
    for p, e in pck.entries.items():
        ext = os.path.splitext(p)[1].lower() or "(none)"
        by_ext[ext] += 1
        size_ext[ext] += e.size
    print("\nby extension:")
    for ext, n in by_ext.most_common():
        print(f"  {ext:12} {n:7d}  {human(size_ext[ext]):>10}")
    by_dir = collections.Counter()
    for p, e in pck.entries.items():
        top = "/".join(p.split("/")[:2]) if p.startswith(".godot/") else p.split("/")[0]
        by_dir[top] += e.size
    print("\nlargest top-level folders:")
    for d, n in by_dir.most_common(25):
        print(f"  {human(n):>10}  {d}")


def cmd_list(pck, a):
    paths = pck.match(a.patterns) if a.patterns else sorted(pck.entries)
    for p in paths:
        e = pck.entries[p]
        print(f"{e.size}\t{'E' if e.encrypted else '-'}\t{p}")


def write_out(pck, out_dir, path):
    dst = os.path.join(out_dir, *path.split("/"))
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    with open(dst, "wb") as f:
        f.write(pck.read(path))
    return pck.entries[path].size


def cmd_extract(pck, a):
    total = 0
    paths = pck.match(a.patterns)
    for p in paths:
        total += write_out(pck, a.out_dir, p)
    print(f"extracted {len(paths)} files, {human(total)} -> {a.out_dir}")


def cmd_sample(pck, a):
    budget = a.max_mb * 1024 * 1024
    chosen = []
    # Always: text data the converter and the logic need.
    always = ["project.godot", "project.binary", "*.json", "localization/*", "*.csv", "*.translation",
              "*.remap", "*.import", "*.gdshader", "*.tres"]
    for p in pck.match(always):
        if pck.entries[p].size <= 4 * 1024 * 1024:
            chosen.append(p)
    # Plus a few of every other type, smallest first, spread over folders.
    per_ext = collections.defaultdict(list)
    for p, e in pck.entries.items():
        per_ext[os.path.splitext(p)[1].lower()].append(e)
    for ext, items in per_ext.items():
        items.sort(key=lambda e: e.size)
        seen_dirs = set()
        picked = 0
        for e in items:
            d = os.path.dirname(e.path)
            if d in seen_dirs:
                continue
            seen_dirs.add(d)
            chosen.append(e.path)
            picked += 1
            if picked >= 12:
                break
    total = 0
    written = 0
    for p in dict.fromkeys(chosen):
        e = pck.entries[p]
        if e.encrypted or total + e.size > budget:
            continue
        total += write_out(pck, a.out_dir, p)
        written += 1
    print(f"sampled {written} files, {human(total)} -> {a.out_dir}")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    s = sub.add_parser("info"); s.add_argument("pck")
    s = sub.add_parser("list"); s.add_argument("pck"); s.add_argument("patterns", nargs="*")
    s = sub.add_parser("extract"); s.add_argument("pck"); s.add_argument("out_dir"); s.add_argument("patterns", nargs="+")
    s = sub.add_parser("sample"); s.add_argument("pck"); s.add_argument("out_dir"); s.add_argument("--max-mb", type=int, default=40)
    a = ap.parse_args()
    try:
        with Pck(a.pck) as pck:
            {"info": cmd_info, "list": cmd_list, "extract": cmd_extract, "sample": cmd_sample}[a.cmd](pck, a)
    except PckError as ex:
        sys.exit(f"error: {ex}")


if __name__ == "__main__":
    main()
