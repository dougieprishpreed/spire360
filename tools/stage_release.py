#!/usr/bin/env python3
"""Stages the console release folder from the built XEX and the pack:
default.xex, spire360.ini (the debug options off), spire/pack.s3p and
spire/data (the pack's localization), checked for FATX's 42-character
names.

    python tools/stage_release.py [--pack work/pack-80] [--out out/release/spire360]

The XEX is xdk/spire360/Release/spire360.xex (xdk\\build.cmd spire360 or
build_light.cmd); nothing else of the build goes in."""
import argparse
import hashlib
import os
import shutil
import sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))


def fatx_check(top):
    bad = []
    longest = ("", 0)
    for d, dirs, files in os.walk(top):
        for name in dirs + files:
            if len(name) > 42:
                bad.append(os.path.join(d, name))
            rel = "game:\\" + os.path.relpath(os.path.join(d, name), top)
            if len(rel) > longest[1]:
                longest = (rel, len(rel))
    return bad, longest


def md5(path):
    h = hashlib.md5()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--pack", default=os.path.join(ROOT, "work", "pack-80"))
    ap.add_argument("--out", default=os.path.join(ROOT, "out", "release", "spire360"))
    a = ap.parse_args()
    xex = os.path.join(ROOT, "xdk", "spire360", "Release", "spire360.xex")
    ini = os.path.join(ROOT, "xdk", "spire360", "spire360.ini")
    if not os.path.exists(xex):
        sys.exit("no XEX: " + xex)
    if os.path.exists(a.out):
        shutil.rmtree(a.out)
    os.makedirs(os.path.join(a.out, "spire"))
    shutil.copy2(xex, os.path.join(a.out, "default.xex"))
    shutil.copy2(ini, os.path.join(a.out, "spire360.ini"))
    shutil.copy2(os.path.join(a.pack, "pack.s3p"), os.path.join(a.out, "spire", "pack.s3p"))
    shutil.copytree(os.path.join(a.pack, "data"), os.path.join(a.out, "spire", "data"))
    for src, dst in ((xex, "default.xex"), (os.path.join(a.pack, "pack.s3p"), os.path.join("spire", "pack.s3p"))):
        if md5(src) != md5(os.path.join(a.out, dst)):
            sys.exit("copy differs: " + dst)
    bad, longest = fatx_check(a.out)
    if bad:
        sys.exit("names over 42 characters: %s" % bad)
    files = 0
    total = 0
    for d, _, fs in os.walk(a.out):
        for f in fs:
            files += 1
            total += os.path.getsize(os.path.join(d, f))
    print("staged %s: %d files, %.1f MB; longest path %d (%s)" % (a.out, files, total / 1048576.0, longest[1], longest[0]))


main()
