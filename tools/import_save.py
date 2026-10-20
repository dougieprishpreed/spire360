"""Copy Slay the Spire 2 profiles between the PC game and Spire360.

The PC game keeps an account's saves in
%APPDATA%/SlayTheSpire2/steam/<account id>/ (profile.save, then
profileN/saves/{progress,prefs,current_run}.save and history/*.run).
Spire360's logic reads and writes the same files, in the same format, under
<data>/user/, so importing is a copy:

    python tools/import_save.py --to out/data
    python tools/import_save.py --from "%APPDATA%/SlayTheSpire2/steam/7656..." --to work/pack/data

and taking a profile back to the PC is the reverse (the PC folder is backed
up first, and --force is required because it overwrites the game's saves):

    python tools/import_save.py --from out/data --to "%APPDATA%/SlayTheSpire2/steam/7656..." --force

A <data> folder (one holding user/) or a save folder itself are both
accepted on either side. The source is only read. An existing destination
is moved aside to <dest>.bak-<time> before the copy. Without --from the
tool picks the PC account folder when there is exactly one with saves.
"""
import argparse
import json
import os
import shutil
import sys
import time

FILES = ("progress.save", "prefs.save", "current_run.save", "current_run_mp.save")


def save_root(path):
    """The folder that holds profile.save / profileN: path or path/user."""
    path = os.path.abspath(os.path.expandvars(os.path.expanduser(path)))
    if os.path.isdir(os.path.join(path, "user")) and not os.path.exists(os.path.join(path, "profile.save")):
        return os.path.join(path, "user")
    return path


def pc_accounts():
    base = os.path.join(os.environ.get("APPDATA", ""), "SlayTheSpire2", "steam")
    if not os.path.isdir(base):
        return []
    return [os.path.join(base, d) for d in sorted(os.listdir(base))
            if os.path.isfile(os.path.join(base, d, "profile.save"))
            or any(os.path.isfile(os.path.join(base, d, f"profile{i}", "saves", "progress.save")) for i in (1, 2, 3))]


def tree_files(root):
    """The save files under root, as relative paths (only what the game owns)."""
    out = []
    for name in ("profile.save", "profile.save.backup"):
        if os.path.isfile(os.path.join(root, name)):
            out.append(name)
    for i in (1, 2, 3):
        saves = os.path.join(f"profile{i}", "saves")
        for f in FILES:
            for suffix in ("", ".backup"):
                rel = os.path.join(saves, f + suffix)
                if os.path.isfile(os.path.join(root, rel)):
                    out.append(rel)
        hist = os.path.join(root, saves, "history")
        if os.path.isdir(hist):
            for f in sorted(os.listdir(hist)):
                if f.endswith(".run"):
                    out.append(os.path.join(saves, "history", f))
    return out


def describe(root):
    lines = []
    try:
        with open(os.path.join(root, "profile.save"), encoding="utf-8") as fh:
            lines.append(f"  last profile: {json.load(fh).get('last_profile_id')}")
    except (OSError, ValueError):
        pass
    for i in (1, 2, 3):
        saves = os.path.join(root, f"profile{i}", "saves")
        prog = os.path.join(saves, "progress.save")
        if not os.path.isfile(prog):
            continue
        try:
            with open(prog, encoding="utf-8") as fh:
                p = json.load(fh)
        except (OSError, ValueError) as ex:
            lines.append(f"  profile{i}: progress.save unreadable ({ex})")
            continue
        stats = p.get("character_stats", [])
        wins = sum(c.get("total_wins", 0) for c in stats)
        losses = sum(c.get("total_losses", 0) for c in stats)
        revealed = sum(1 for e in p.get("epochs", []) if e.get("state") == "revealed")
        asc = ", ".join(f"{c.get('id', '?').split('.')[-1].title()} A{c.get('max_ascension', 0)}" for c in stats)
        hist = os.path.join(saves, "history")
        runs = len([f for f in os.listdir(hist) if f.endswith(".run")]) if os.path.isdir(hist) else 0
        lines.append(f"  profile{i}: schema v{p.get('schema_version')}, {wins} wins / {losses} losses, "
                     f"{revealed} epochs revealed, {runs} run histories")
        if asc:
            lines.append(f"    max ascension: {asc}")
        cur = os.path.join(saves, "current_run.save")
        if os.path.isfile(cur):
            try:
                with open(cur, encoding="utf-8") as fh:
                    r = json.load(fh)
                pl = (r.get("players") or [{}])[0]
                lines.append(f"    saved run: {pl.get('character_id', '?').split('.')[-1].title()}, "
                             f"ascension {r.get('ascension', 0)}, act {r.get('current_act_index', 0) + 1}, "
                             f"HP {pl.get('current_hp')}/{pl.get('max_hp')}, schema v{r.get('schema_version')}")
            except (OSError, ValueError) as ex:
                lines.append(f"    saved run unreadable ({ex})")
    return lines


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--from", dest="src", help="PC account folder, or a Spire360 data folder (default: the PC account)")
    ap.add_argument("--to", dest="dst", required=True, help="Spire360 data folder (e.g. out/data), or a PC account folder")
    ap.add_argument("--force", action="store_true", help="allow writing into the PC game's own save folder")
    ap.add_argument("--dry-run", action="store_true", help="show what would be copied")
    a = ap.parse_args()

    if a.src:
        src = save_root(a.src)
    else:
        accounts = pc_accounts()
        if len(accounts) != 1:
            print("Pick the PC account folder with --from:" if accounts else "No PC saves found under %APPDATA%.")
            for acc in accounts:
                print("  " + acc)
            return 2
        src = accounts[0]
    if not os.path.isdir(src):
        print(f"No such folder: {src}")
        return 2
    files = tree_files(src)
    if not files:
        print(f"No Slay the Spire 2 saves in {src}")
        return 2
    for rel in files:
        if rel.endswith((".save", ".run")):
            try:
                with open(os.path.join(src, rel), encoding="utf-8") as fh:
                    json.load(fh)
            except (OSError, ValueError) as ex:
                print(f"Not a readable save: {rel} ({ex})")
                return 1

    dst = a.dst if a.dst.rstrip("/\\").endswith(("user",)) else a.dst
    dst = os.path.abspath(os.path.expandvars(os.path.expanduser(dst)))
    pc_base = os.path.abspath(os.path.join(os.environ.get("APPDATA", "-"), "SlayTheSpire2"))
    into_pc = os.path.normcase(dst).startswith(os.path.normcase(pc_base))
    if not into_pc and not os.path.basename(dst.rstrip("/\\")) == "user":
        dst = os.path.join(dst, "user")
    if os.path.normcase(dst) == os.path.normcase(src):
        print("Source and destination are the same folder.")
        return 2
    if into_pc and not a.force:
        print(f"{dst} is the PC game's save folder: add --force to overwrite it (it is backed up first).")
        return 2

    print(f"From {src}")
    for line in describe(src):
        print(line)
    print(f"To   {dst}  ({len(files)} files)")
    if a.dry_run:
        for rel in files:
            print("  " + rel)
        return 0

    if os.path.isdir(dst) and os.listdir(dst):
        backup = f"{dst.rstrip(os.sep)}.bak-{time.strftime('%Y%m%d-%H%M%S')}"
        if into_pc:
            shutil.copytree(dst, backup)  # the game's folder keeps everything else (settings, replays)
            for i in (1, 2, 3):
                saves = os.path.join(dst, f"profile{i}", "saves")
                for f in FILES:
                    for suffix in ("", ".backup"):
                        p = os.path.join(saves, f + suffix)
                        if os.path.isfile(p):
                            os.remove(p)
        else:
            shutil.move(dst, backup)
        print(f"Previous saves kept in {backup}")
    for rel in files:
        target = os.path.join(dst, rel)
        os.makedirs(os.path.dirname(target), exist_ok=True)
        shutil.copy2(os.path.join(src, rel), target)
    print("Done.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
