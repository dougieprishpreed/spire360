#!/usr/bin/env python3
"""Puts the console folder together: default.xex (and its licenses), spire360.ini (the release
options), spire/pack.s3p and spire/data (the pack's localization), checked
for FATX's 42-character names, and build.json (what it was made from).

    python tools/install/stage.py --pack <converter output> --out <folder> (--xex <default.xex> | --keep-xex)

--keep-xex keeps the default.xex already in --out (the installer's assets
only run: the same source, the game's files converted again). An existing
spire360.ini is kept: the player's options stay theirs. The saves and logs
the console writes beside default.xex (spire/data/user, spire360*.txt,
screenshots) are left alone."""
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import time

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))


def md5(path):
    h = hashlib.md5()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def fatx_check(top):
    bad = []
    for d, dirs, files in os.walk(top):
        for name in dirs + files:
            if len(name) > 42:
                bad.append(os.path.join(d, name))
    return bad


def copy_checked(src, dst):
    tmp = dst + ".part"
    shutil.copyfile(src, tmp)
    if md5(src) != md5(tmp):
        sys.exit("copy differs: " + dst)
    os.replace(tmp, dst)


def git_commit():
    try:
        return subprocess.check_output(["git", "-C", ROOT, "rev-parse", "--short", "HEAD"], text=True).strip()
    except (OSError, subprocess.CalledProcessError):
        return ""


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--pack", required=True, help="the converter's output (pack.s3p, data/)")
    ap.add_argument("--out", required=True)
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--xex")
    g.add_argument("--keep-xex", action="store_true")
    a = ap.parse_args()

    os.makedirs(os.path.join(a.out, "spire"), exist_ok=True)
    xex = os.path.join(a.out, "default.xex")
    if a.keep_xex:
        if not os.path.exists(xex):
            sys.exit("no default.xex in " + a.out)
    else:
        copy_checked(a.xex, xex)
        # the licenses the XEX ships under (GCC's runtime, newlib, xsdk,
        # vulkan4xenon), from beside it
        lic = os.path.join(os.path.dirname(os.path.abspath(a.xex)), "licenses")
        if os.path.isdir(lic):
            shutil.rmtree(os.path.join(a.out, "licenses"), ignore_errors=True)
            shutil.copytree(lic, os.path.join(a.out, "licenses"))
    ini = os.path.join(a.out, "spire360.ini")
    if not os.path.exists(ini):
        shutil.copyfile(os.path.join(ROOT, "tools", "install", "spire360.ini"), ini)
    copy_checked(os.path.join(a.pack, "pack.s3p"), os.path.join(a.out, "spire", "pack.s3p"))
    # the pack's own data (localization, the scenes' directory listing). The
    # player's saves (user) and the console's copies of them (session_start)
    # are never touched, nor taken from the pack's folder (a PC run's saves
    # can sit there)
    src_data = os.path.join(a.pack, "data")
    for name in os.listdir(src_data):
        if name in ("user", "session_start"):
            continue
        s, d = os.path.join(src_data, name), os.path.join(a.out, "spire", "data", name)
        if os.path.isdir(s):
            shutil.rmtree(d, ignore_errors=True)
            shutil.copytree(s, d)
        else:
            os.makedirs(os.path.dirname(d), exist_ok=True)
            shutil.copyfile(s, d)
    bad = fatx_check(a.out)
    if bad:
        sys.exit("names over 42 characters: %s" % bad)

    info_path = os.path.join(a.out, "build.json")
    info = {}
    if a.keep_xex and os.path.exists(info_path):
        with open(info_path, encoding="utf-8") as f:
            info = json.load(f)
    if not a.keep_xex:
        info["source"] = git_commit()
        info["xex_md5"] = md5(xex)
        # the XEX holds the game's code: a game update needs a new one
        manifest = os.path.join(ROOT, "logic", "lib", "game", "manifest.json")
        if os.path.exists(manifest):
            with open(manifest, encoding="utf-8") as f:
                info["game_version"] = json.load(f).get("release_info", {}).get("version")
    info["staged"] = time.strftime("%Y-%m-%d %H:%M:%S")
    with open(info_path, "w", encoding="utf-8") as f:
        json.dump(info, f, indent=2)

    files = total = 0
    for d, _, fs in os.walk(a.out):
        for f in fs:
            files += 1
            total += os.path.getsize(os.path.join(d, f))
    print("staged %s: %d files, %.1f MB, source %s" % (a.out, files, total / 1048576.0, info.get("source", "?")))


main()
