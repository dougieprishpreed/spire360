#!/usr/bin/env python3
"""Spire360 asset converter.

  convert.py survey  <SlayTheSpire2.pck | extracted dir> [--json out.json]
      What the pack holds and what the converter can read: every texture
      header (sizes, formats, memory at the console's scale), every scene and
      resource file parsed, every reference resolved. Fast: texture data is
      not decoded. Send the report (it lists no game content beyond paths).

  convert.py texture <pck | dir> <res://path.png> <out.png> [--level N]
      Decodes one texture to a PNG (to check the decoder by eye).

Needs Python 3.9+, numpy, Pillow, texture2ddecoder (converter/requirements.txt).
"""
import argparse
import collections
import json
import os
import sys
import time
import traceback

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from spire_convert import ctex, godot_binary, resources, variant  # noqa: E402
from spire_convert.pck import Pck  # noqa: E402

# 1920x1080 design resolution -> 1280x720 on the console.
CONSOLE_SCALE = 2 / 3


def open_source(path):
    if os.path.isdir(path):
        return resources.DirSource(path)
    return resources.PckSource(Pck(path))


def human(n):
    for unit in ("B", "KB", "MB", "GB"):
        if abs(n) < 1024 or unit == "GB":
            return f"{n:.0f} {unit}" if unit == "B" else f"{n:.1f} {unit}"
        n /= 1024


def folder(path, depth=2):
    parts = path.split("/")
    return "/".join(parts[:depth]) if len(parts) > depth else "/".join(parts[:-1]) or "."


def dxt_bytes(w, h, alpha, scale=1.0, mips=False):
    """DXT1 (opaque) / DXT5 bytes at a scale, whole 4x4 blocks."""
    w, h = max(1, round(w * scale)), max(1, round(h * scale))
    total = 0
    while True:
        total += ((w + 3) // 4) * ((h + 3) // 4) * (16 if alpha else 8)
        if not mips or (w == 1 and h == 1):
            return total
        w, h = max(1, w >> 1), max(1, h >> 1)


def cmd_survey(a):
    t0 = time.time()
    src = open_source(a.pack)
    res = resources.Resources(src)
    files = src.list()
    report = {"pack": os.path.basename(a.pack), "files": len(files)}
    out = []

    def say(line=""):
        out.append(line)
        print(line, flush=True)

    say(f"Spire360 converter survey: {a.pack}")
    say(f"{len(files)} files")

    # Project settings.
    if src.exists("project.binary"):
        ps = godot_binary.parse_project_binary(src.read("project.binary"))
        keep = {k: v for k, v in ps.items() if k.startswith(("display/window", "application/config/name",
                                                              "application/run", "rendering/"))
                and not (isinstance(v, tuple) and v and v[0] == "<undecoded>")}
        report["project"] = {k: (list(v) if isinstance(v, tuple) else v) for k, v in keep.items()}
        say(f"project: {ps.get('application/config/name')}, viewport "
            f"{ps.get('display/window/size/viewport_width')}x{ps.get('display/window/size/viewport_height')}, "
            f"stretch {ps.get('display/window/stretch/mode')}/{ps.get('display/window/stretch/aspect')}")

    # Textures: every texture import, header only.
    say("")
    say("== textures")
    tex = []
    errors = collections.Counter()
    for f in files:
        if not f.endswith(".import"):
            continue
        p = f[:-7]
        imp = res.importer(p)
        if not imp or imp[0] not in ("texture", "texture_atlas"):
            continue
        real = res.resolve(p)
        if real is None or not src.exists(real):
            errors["texture import without data"] += 1
            continue
        try:
            h = ctex.header(src.read_head(real, 64))
        except Exception as e:  # noqa: BLE001 - report and continue
            errors[f"header: {e}"] += 1
            continue
        alpha = h["format"] not in (ctex.FMT_DXT1, ctex.FMT_RGB8, ctex.FMT_L8, ctex.FMT_R8, ctex.FMT_RGB565)
        tex.append({"path": p, "ctex": real, "w": h["width"], "h": h["height"], "fmt": h["format_name"],
                    "mips": h["mipmaps"], "alpha": alpha, "bytes": src.size(real)})
    by_fmt = collections.Counter(t["fmt"] for t in tex)
    say(f"{len(tex)} textures; formats: " + ", ".join(f"{k} {v}" for k, v in by_fmt.most_common()))
    mpx = sum(t["w"] * t["h"] for t in tex) / 1e6
    full = sum(dxt_bytes(t["w"], t["h"], t["alpha"]) for t in tex)
    scaled = sum(dxt_bytes(t["w"], t["h"], t["alpha"], CONSOLE_SCALE, t["mips"] > 0) for t in tex)
    half = sum(dxt_bytes(t["w"], t["h"], t["alpha"], 0.5, t["mips"] > 0) for t in tex)
    say(f"{mpx:.1f} megapixels at full size; as DXT: {human(full)} at 1x, {human(scaled)} at 2/3 (console), "
        f"{human(half)} at 1/2")
    sizes = collections.Counter()
    for t in tex:
        m = max(t["w"], t["h"])
        sizes["<=64" if m <= 64 else "<=256" if m <= 256 else "<=512" if m <= 512 else "<=1024" if m <= 1024
              else "<=2048" if m <= 2048 else "<=4096" if m <= 4096 else ">4096"] += 1
    say("largest side: " + ", ".join(f"{k}: {sizes[k]}" for k in
                                       ("<=64", "<=256", "<=512", "<=1024", "<=2048", "<=4096", ">4096") if sizes[k]))
    groups = collections.defaultdict(lambda: [0, 0, 0])
    for t in tex:
        g = groups[folder(t["path"], 3)]
        g[0] += 1
        g[1] += t["w"] * t["h"]
        g[2] += dxt_bytes(t["w"], t["h"], t["alpha"], CONSOLE_SCALE, t["mips"] > 0)
    say("by folder (count, megapixels, DXT at 2/3):")
    for k, (n, px, b) in sorted(groups.items(), key=lambda kv: -kv[1][2])[:40]:
        say(f"  {human(b):>9}  {px / 1e6:7.1f} Mpx  {n:5d}  {k}")
    say("largest 15:")
    for t in sorted(tex, key=lambda t: -t["w"] * t["h"])[:15]:
        say(f"  {t['w']}x{t['h']} {t['fmt']}{' +mips' if t['mips'] else ''}  {t['path']}")
    for k, v in errors.items():
        say(f"  error: {k}: {v}")
    report["textures"] = {"count": len(tex), "formats": dict(by_fmt), "megapixels": round(mpx, 1),
                          "dxt_full": full, "dxt_console": scaled, "dxt_half": half, "errors": dict(errors),
                          "sizes": dict(sizes)}
    if a.json:
        report["texture_list"] = tex

    # Scenes and resources.
    say("")
    say("== scenes and resources")
    scenes = [f for f in files if f.endswith(".tscn")]
    tres = [f for f in files if f.endswith(".tres")]
    parse_errors = []
    node_types = collections.Counter()
    ext_types = collections.Counter()
    sub_types = collections.Counter()
    unresolved = collections.Counter()
    nodes = 0
    for f in scenes + tres:
        try:
            r = variant.parse_text(src.read(f).decode("utf-8"))
        except Exception as e:  # noqa: BLE001
            parse_errors.append(f"{f}: {e}")
            continue
        for n in r.nodes:
            nodes += 1
            node_types[n.get("type") or ("(instance)" if "instance" in n else "(inherited)")] += 1
        for e in r.ext_resources.values():
            ext_types[e.get("type")] += 1
            path = e.get("path", "")
            if e.get("type") in ("Script",) or path.endswith((".cs", ".gd")):
                continue
            if res.resolve(path) is None and not (e.get("uid") and res.resolve(e["uid"])):
                unresolved[folder(resources.strip_res(path), 2)] += 1
        for s in r.sub_resources.values():
            sub_types[s.get("type")] += 1
    say(f"{len(scenes)} scenes, {len(tres)} resources parsed; {len(parse_errors)} failed; {nodes} nodes")
    for e in parse_errors[:20]:
        say(f"  PARSE ERROR {e}")
    say("node types: " + ", ".join(f"{k} {v}" for k, v in node_types.most_common(40)))
    say("ext_resource types: " + ", ".join(f"{k} {v}" for k, v in ext_types.most_common(25)))
    say("sub_resource types: " + ", ".join(f"{k} {v}" for k, v in sub_types.most_common(25)))
    say(f"unresolved references (non-script): {sum(unresolved.values())}"
        + ("" if not unresolved else " - " + ", ".join(f"{k} {v}" for k, v in unresolved.most_common(10))))
    report["scenes"] = {"scenes": len(scenes), "resources": len(tres), "parse_errors": parse_errors,
                        "nodes": nodes, "node_types": dict(node_types), "ext_types": dict(ext_types),
                        "sub_types": dict(sub_types), "unresolved": dict(unresolved)}

    # Everything else, by kind.
    say("")
    say("== other assets")
    kinds = {"spine skeletons": (".spskel",), "spine atlases": (".spatlas",), "fonts": (".fontdata",),
             "FMOD banks": (".bank",), "audio streams": (".mp3str", ".oggvorbisstr", ".sample"),
             "shaders": (".gdshader", ".gdshaderinc"), "localization tables": ("localization/",)}
    other = {}
    for name, keys in kinds.items():
        sel = [f for f in files if f.endswith(keys) or (keys[0].endswith("/") and f.startswith(keys[0]))]
        total = sum(src.size(f) for f in sel)
        other[name] = {"count": len(sel), "bytes": total}
        say(f"{name:20s} {len(sel):5d}  {human(total)}")
    report["other"] = other
    say("")
    say(f"survey took {time.time() - t0:.0f} s")
    if a.json:
        with open(a.json, "w", encoding="utf-8") as f:
            json.dump(report, f, indent=1, default=str)
        say(f"wrote {a.json}")
    if a.out:
        with open(a.out, "w", encoding="utf-8") as f:
            f.write("\n".join(out) + "\n")


def cmd_texture(a):
    src = open_source(a.pack)
    res = resources.Resources(src)
    real = res.resolve(a.res_path)
    if real is None:
        sys.exit(f"not found: {a.res_path}")
    t = ctex.load(src.read(real))
    from PIL import Image
    Image.fromarray(t.decode(a.level)).save(a.out)
    print(f"{a.res_path} -> {real}: {t.width}x{t.height} {t.format_name}, {len(t.levels)} level(s) -> {a.out}")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    s = sub.add_parser("survey")
    s.add_argument("pack")
    s.add_argument("--json", help="also write the details as JSON")
    s.add_argument("--out", help="also write the report as text")
    t = sub.add_parser("texture")
    t.add_argument("pack")
    t.add_argument("res_path")
    t.add_argument("out")
    t.add_argument("--level", type=int, default=0)
    a = ap.parse_args()
    try:
        {"survey": cmd_survey, "texture": cmd_texture}[a.cmd](a)
    except KeyboardInterrupt:
        sys.exit(1)
    except Exception:  # noqa: BLE001
        traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()
