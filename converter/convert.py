#!/usr/bin/env python3
"""Spire360 asset converter.

  convert.py survey  <SlayTheSpire2.pck | extracted dir> [--json out.json]
      What the pack holds and what the converter can read: every texture
      header (sizes, formats, memory at the console's scale), every scene and
      resource file parsed, every reference resolved. Fast: texture data is
      not decoded. Send the report (it lists no game content beyond paths).

  convert.py texture <pck | dir> <res://path.png> <out.png> [--level N]
      Decodes one texture to a PNG (to check the decoder by eye).

  convert.py bundle <pck> <out.zip> [scene res paths or globs...] [--max-mb 45] [--have old.zip]
      A scene and everything it references (recursively: scenes, resources,
      textures with their .import stubs, fonts, Spine data), plus project
      settings and English localization, as a zip laid out like the pack.
      For developing the engine against real layouts. Defaults to the main
      menu, combat room, card, creature, hand, top bar, map and rewards.

  convert.py build <pck | dir> <out_dir> [--scale 0.6667] [--only GLOB]... [--jobs N]
                   [--godot PATH] [--languages eng,deu,...]
      Writes <out_dir>/pack.s3p (textures as DXT at the console's scale,
      scenes and resources as JSON, fonts, Spine data, audio streams) and
      <out_dir>/data/localization (the tables the logic reads), plus
      <out_dir>/pack_report.json. --only limits it to matching res paths.
      Fonts need Godot 4.5.1 (it renders the glyphs, see
      spire_convert/fonts.py): --godot, $SPIRE_GODOT, or fetch-godot. Fonts
      keep only the characters of --languages (default eng).
      Procedural noise textures also use Godot 4.5.1; their generated
      images are cached in converter/.cache/procedural, then packed as data
      textures. Keep that cache between pack builds.

  convert.py fetch-godot
      Downloads the official Godot 4.5.1 build (MIT, ~60 MB) into
      converter/.cache/godot for the font step.

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
from spire_convert import atlas, ctex, fonts as fontconv, godot_binary, resources, s3p, scenes, texture, variant  # noqa: E402
from spire_convert import spine as spinefmt  # noqa: E402
from spire_convert import procedural  # noqa: E402
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

    # Spine: every skeleton through the reader (it must consume each file
    # exactly) and the features they use, for the engine's runtime.
    say("")
    say("== spine")
    feats, errors, versions = collections.Counter(), {}, collections.Counter()
    skels = [f for f in files if f.endswith(".spskel")]
    for f in skels:
        try:
            sk = spinefmt.read_skeleton(src.read(f))
        except Exception as e:  # noqa: BLE001
            errors[f] = f"{type(e).__name__}: {e}"
            continue
        versions[sk["version"]] += 1
        for k in ("ik", "transform", "path", "physics"):
            if sk[k]:
                feats["constraint " + k] += 1
        if len(sk["skins"]) > 1:
            feats["several skins"] += 1
        kinds = set()
        for skin in sk["skins"]:
            for att in skin["attachments"]:
                kinds.add(att["type"] + (" weighted" if "bones" in att else ""))
                if "sequence" in att:
                    kinds.add("sequence")
        kinds.update("blend " + s["blend"] for s in sk["slots"] if s["blend"] != "normal")
        kinds.update("dark colour" for s in sk["slots"] if "dark" in s)
        kinds.update("inherit " + b["inherit"] for b in sk["bones"] if b["inherit"] != "normal")
        for an in sk["animations"]:
            for grp in ("slots", "bones", "path", "physics", "attachments"):
                kinds.update("timeline " + t["type"] for t in an[grp])
            for grp in ("ik", "transform", "draw_order", "events"):
                if an[grp]:
                    kinds.add("timeline " + grp)
        feats.update(kinds)
    say(f"skeletons {len(skels)}, read {len(skels) - len(errors)}; versions "
        + ", ".join(f"{k} {v}" for k, v in versions.most_common()))
    for k, v in sorted(feats.items()):
        say(f"  {k:32s} {v:5d}")
    for k, v in list(errors.items())[:20]:
        say(f"  error {k}: {v}")
    report["spine"] = {"skeletons": len(skels), "versions": dict(versions), "features": dict(feats),
                       "errors": errors}
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


# ---- bundle ----

DEFAULT_BUNDLE = [
    # The screens ported so far and the next ones (rest site, shop, treasure,
    # rewards, game over, character select), with the pictures the code
    # loads by name (relic and potion icons, run history icons).
    "scenes/screens/main_menu.tscn", "scenes/rooms/combat_room.tscn", "scenes/cards/card.tscn",
    "scenes/combat/creature.tscn", "scenes/combat/player_hand.tscn", "scenes/ui/top_bar.tscn",
    "scenes/screens/map/map_screen.tscn", "scenes/screens/rewards_screen.tscn", "scenes/rewards/*.tscn",
    "scenes/run.tscn", "scenes/rooms/event_room.tscn", "scenes/rooms/rest_site_room.tscn",
    "scenes/rest_site/*.tscn", "scenes/rest_site/characters/*.tscn", "scenes/rooms/merchant_room.tscn",
    "scenes/merchant/*.tscn", "scenes/merchant/characters/*.tscn", "scenes/rooms/treasure_room.tscn",
    "scenes/screens/game_over_screen.tscn", "scenes/screens/character_select_screen.tscn",
    "scenes/screens/char_select/*.tscn", "scenes/screens/card_selection/*.tscn", "scenes/events/*.tscn",
    "scenes/ui/ancient_name_banner.tscn", "images/atlases/relic_atlas.sprites/*.tres",
    "images/atlases/relic_outline_atlas.sprites/*.tres", "images/atlases/potion_atlas.sprites/*.tres",
    "images/ui/run_history/*.png", "images/ui/rest_site/*.png", "images/ui/game_over_screen/*.png",
    "images/events/neow.png", "images/events/this_or_that.png", "images/ui/reward_screen/*.png",
    "scenes/screens/game_over_screen/*.tscn", "scenes/orbs/*.tscn", "scenes/orbs/orb_visuals/*.tscn",
    "images/packed/character_select/*.png", "images/packed/sprite_fonts/*.png",
    "scenes/rooms/treasure_room/*.tscn", "scenes/ui/treasure_relic_holder.tscn",
    # The large set ("bundle --max-mb 1000" for a split zip): every
    # creature, combat background and event picture, card portraits and the
    # relics' big icons.
    "scenes/creature_visuals/*.tscn", "scenes/backgrounds/*.tscn", "images/events/*.png",
    "images/atlases/card_atlas.sprites/*.tres", "images/relics/*.png",
    "images/atlases/potion_outline_atlas.sprites/*.tres", "images/packed/map/*.png",
    # What the code loads by name: the acts' treasure chests, the effects
    # (attacks, block, heal, coins...), the combat banners, potion popups,
    # card overlays, encounter scenes and the rest of the UI scenes.
    "animations/backgrounds/treasure_room/*.tres", "scenes/vfx/*.tscn", "scenes/combat/*.tscn",
    "scenes/potions/*.tscn", "scenes/relics/*.tscn", "scenes/cards/*.tscn", "scenes/encounters/*.tscn",
    "scenes/ui/*.tscn",
]


def dependencies(src, res, roots):
    """Pack paths needed to load the given res paths: the files themselves,
    .import stubs and their imported data, and everything scenes/resources
    reference, recursively."""
    seen, out = set(), []
    stack = [resources.strip_res(r) for r in roots]
    while stack:
        p = stack.pop()
        if p in seen:
            continue
        seen.add(p)
        sheet = atlas.sheet_for_sprite(p)
        if sheet:
            # The game cuts atlas sprites from the sheet (atlas.py), the
            # .tres at that path may not even exist.
            stack.append(sheet)
        if src.exists(p + ".import"):
            out.append(p + ".import")
        real = res.resolve(p)
        if real is None:
            continue
        out.append(real)
        if real.endswith((".tscn", ".tres")):
            try:
                tr = variant.parse_text(src.read(real).decode("utf-8"))
            except Exception:  # noqa: BLE001
                continue
            for e in tr.ext_resources.values():
                path = e.get("path", "")
                if e.get("uid"):
                    path = res.uid_path(e["uid"]) or path
                if path and not path.endswith((".cs", ".gd")):
                    stack.append(resources.strip_res(path))
        elif real.endswith(".tpsheet"):
            try:
                stack.extend(atlas.page_images(src.read(real)))
            except Exception:  # noqa: BLE001
                pass
        elif real.endswith(".spatlas"):
            # Spine atlases load their page images by name, next to the atlas.
            try:
                stack.extend(spinefmt.page_textures(src.read(real), p))
            except Exception:  # noqa: BLE001
                pass
    return out


def res_dir_listing(src):
    """The folders the logic lists (scenes/backgrounds/<name>/layers), files
    in the .pck's order (the order Godot lists them in). A bundle carries the
    listing from its .pck (res_dirs.json): an extracted folder has no order."""
    if not isinstance(src, resources.PckSource) and src.exists("res_dirs.json"):
        return json.loads(src.read("res_dirs.json").decode("utf-8"))
    out = {}
    for f in src.ordered():
        parts = f.split("/")
        if len(parts) == 5 and parts[0] == "scenes" and parts[1] == "backgrounds" and parts[3] == "layers":
            out.setdefault("/".join(parts[:4]), []).append(parts[4])
    return out


def cmd_bundle(a):
    import zipfile
    src = open_source(a.pack)
    res = resources.Resources(src)
    roots = a.scenes or DEFAULT_BUNDLE
    if any(ch in r for r in roots for ch in "*?["):
        import fnmatch
        listing = src.list()
        expanded = []
        for r in roots:
            r = resources.strip_res(r)
            if any(ch in r for ch in "*?["):
                # A glob over a folder of images matches their .import stubs:
                # take the images (the stub alone does not bring the texture).
                # The .pck has no "x.png", only "x.png.import": match the
                # glob against the stub's name without ".import".
                expanded += sorted({f[:-len(".import")] if f.endswith(".import") else f
                                    for f in listing
                                    if fnmatch.fnmatchcase(f[:-len(".import")] if f.endswith(".import") else f, r)})
            else:
                expanded.append(r)
        # A glob over an atlas's sprites may match nothing in the .pck (the
        # game cuts them from the sheet): take the sheet.
        expanded += sorted({s for s in (atlas.sheet_for_sprite(resources.strip_res(r)) for r in roots) if s})
        roots = expanded
    files = dependencies(src, res, roots)
    extra = [f for f in ("project.binary", ".godot/uid_cache.bin") if src.exists(f)]
    extra += [f for f in src.list() if f.startswith("localization/eng/")]
    # --have: bundles already sent; only what they lack is written (merged
    # over them, the result is the whole set).
    have = set()
    for h in a.have or []:
        with zipfile.ZipFile(h) as hz:
            have.update(n for n in hz.namelist() if n not in ("bundle_info.json", "res_dirs.json"))
    budget = a.max_mb * 1024 * 1024
    total = 0
    skipped = []
    already = 0
    with zipfile.ZipFile(a.out, "w", zipfile.ZIP_DEFLATED) as z:
        for f in extra + sorted(set(files), key=lambda x: (x.startswith(".godot/imported"), src.size(x))):
            if f in have:
                already += 1
                continue
            size = src.size(f)
            if total + size > budget and f.startswith(".godot/imported"):
                skipped.append(f)
                continue
            z.writestr(f, src.read(f))
            total += size
        z.writestr("bundle_info.json", json.dumps({"roots": roots, "skipped": skipped}, indent=1))
        z.writestr("res_dirs.json", json.dumps(res_dir_listing(src), indent=0))
    print(f"{len(files)} files for {len(roots)} scene(s)"
          + (f", {already} already in --have" if have else "") + f"; {human(total)} written to {a.out}"
          + (f"; {len(skipped)} largest imported files left out (over --max-mb)" if skipped else ""))


# ---- build ----

_worker = {}


def _worker_init(pack_path, scale):
    _worker["src"] = open_source(pack_path)
    _worker["res"] = resources.Resources(_worker["src"])
    _worker["scale"] = scale


def _convert_texture(name):
    try:
        src, res = _worker["src"], _worker["res"]
        t = ctex.load(src.read(res.resolve(name)))
        out = texture.convert(t, _worker["scale"])
        return name, s3p.texture_payload(out), {"src": t.format_name, "fmt": texture.FORMAT_NAMES[out.format],
                                                "w": out.width, "h": out.height, "ow": out.orig_width,
                                                "oh": out.orig_height, "levels": len(out.levels)}, None
    except Exception as e:  # noqa: BLE001
        return name, None, None, f"{type(e).__name__}: {e}"


def _round_floats(v):
    """Spine data is float32: 7 significant digits keep it exact enough and
    halve the JSON."""
    if isinstance(v, float):
        return float(f"{v:.7g}")
    if isinstance(v, list):
        return [_round_floats(x) for x in v]
    if isinstance(v, dict):
        return {k: _round_floats(x) for k, x in v.items()}
    return v


def cmd_build(a):
    import fnmatch
    import multiprocessing
    t0 = time.time()
    src = open_source(a.pack)
    res = resources.Resources(src)
    files = src.list()
    os.makedirs(a.out, exist_ok=True)

    def wanted(name):
        return not a.only or any(fnmatch.fnmatchcase(name, g) for g in a.only)

    version = ""
    rel = [f for f in files if f.endswith("release_info.json")]
    try:
        if rel:
            version = json.loads(src.read(rel[0])).get("version", "")
        else:
            # The game keeps it next to the .pck (the install folder).
            beside = os.path.join(os.path.dirname(os.path.abspath(a.pack)), "release_info.json")
            if os.path.isfile(beside):
                with open(beside, encoding="utf-8") as f:
                    version = json.load(f).get("version", "")
    except Exception:  # noqa: BLE001
        pass

    textures, fonts, spine, audio, text = [], [], [], [], []
    for f in files:
        if f.endswith(".import"):
            p = f[:-7]
            if not wanted(p):
                continue
            imp = res.importer(p)
            if not imp:
                continue
            importer, type_ = imp
            if importer in ("texture",) and res.resolve(p):
                textures.append(p)
            elif type_ == "FontFile" and res.resolve(p):
                fonts.append(p)
            elif type_ in ("SpineSkeletonFileResource", "SpineAtlasResource") and res.resolve(p):
                spine.append(p)
            elif type_ and type_.startswith("AudioStream") and res.resolve(p):
                audio.append(p)
        elif f.endswith((".tscn", ".tres")) and wanted(f):
            text.append(f)
    print(f"build: {len(textures)} textures, {len(text)} scenes/resources, {len(fonts)} fonts, "
          f"{len(spine)} spine files, {len(audio)} audio streams -> {a.out}", flush=True)

    report = {"version": version, "scale": a.scale, "entries": {}, "errors": {}}
    sizes = collections.Counter()
    with s3p.Writer(os.path.join(a.out, "pack.s3p"), version) as w:
        def add(name, type_, payload, info=None):
            w.add(name, type_, payload)
            sizes[s3p.TYPE_NAMES[type_]] += len(payload)
            report["entries"][name] = dict(info or {}, type=s3p.TYPE_NAMES[type_], bytes=len(payload))

        # Textures, in parallel.
        # Each worker holds a decoded image (up to ~6000x5000 RGBA plus its
        # resampling buffers): cap the default so a big pack fits in RAM.
        jobs = a.jobs or max(1, min(6, (os.cpu_count() or 2) - 1))
        done = 0
        with multiprocessing.Pool(jobs, _worker_init, (a.pack, a.scale)) as pool:
            for name, payload, info, err in pool.imap_unordered(_convert_texture, textures, chunksize=4):
                done += 1
                if err:
                    report["errors"][name] = err
                else:
                    add(name, s3p.TYPE_TEXTURE, payload, info)
                if done % 200 == 0 or done == len(textures):
                    print(f"  textures {done}/{len(textures)} ({time.time() - t0:.0f} s)", flush=True)

        # Atlas sprites as the game's AtlasResourceLoader serves them: from
        # the .tpsheet, in place of any .tres file at the same path.
        sprite_text = {}
        for f in files:
            if atlas.is_sheet(f):
                try:
                    for name, t in atlas.sprites(src.read(f), f):
                        if wanted(name):
                            sprite_text[name] = t
                except Exception as e:  # noqa: BLE001
                    report["errors"][f] = f"{type(e).__name__}: {e}"
        text = [f for f in text if f not in sprite_text]
        godot = fontconv.find_godot(a.godot)
        procedural_baker = procedural.Baker(godot, res)
        for f in text + sorted(sprite_text):
            try:
                body = sprite_text[f] if f in sprite_text else src.read(f).decode("utf-8")
                tr = variant.parse_text(body)
                doc = scenes.to_json(tr, res)
                procedural_baker.prepare(tr, doc)
                add(f, s3p.TYPE_SCENE, json.dumps(doc, separators=(",", ":")).encode("utf-8"))
            except Exception as e:  # noqa: BLE001
                report["errors"][f] = f"{type(e).__name__}: {e}"
        procedural_baker.write(add)
        if fonts and not godot:
            print("  fonts: no Godot 4.5.1 (--godot, $SPIRE_GODOT or `convert.py fetch-godot`); "
                  "storing the raw font files, the console will not draw their text", flush=True)
        font_jobs = []
        for p in fonts:
            try:
                raw = src.read(res.resolve(p))
                if not godot:
                    main = godot_binary.parse_rsrc(raw)["resources"][-1]["props"]
                    add(p, s3p.TYPE_FONT, main["data"], {"font": main.get("font_name"), "style": main.get("style_name")})
                else:
                    font_jobs.append((p, raw))
            except Exception as e:  # noqa: BLE001
                report["errors"][p] = f"{type(e).__name__}: {e}"
        if godot:
            font_jobs.append((fontconv.DEFAULT_FONT_NAME, "default"))
            import concurrent.futures
            t1 = time.time()
            chars = None
            if a.languages != "all":
                chars = fontconv.charset(src, files, set(a.languages.split(",")))
                print(f"  fonts: {len(chars)} characters for {a.languages}", flush=True)
            # Fonts in a language's folder (fonts/jpn/..., fonts/rus/...) serve
            # that language only: no glyphs unless it is chosen.
            all_langs = {f.split("/")[1] for f in files if f.startswith("localization/") and f.count("/") >= 2}

            def font_chars(p):
                parts = p.split("/")
                if chars is not None and len(parts) > 2 and parts[1] in all_langs \
                        and parts[1] not in a.languages.split(","):
                    return ""
                return chars

            with concurrent.futures.ThreadPoolExecutor(max(1, min(jobs, 4))) as ex:
                futs = {ex.submit(fontconv.export, godot, raw, font_chars(p)): p for p, raw in font_jobs}
                for fut in concurrent.futures.as_completed(futs):
                    p = futs[fut]
                    try:
                        info, pages = fut.result()
                        for name, type_, payload in fontconv.payloads(p, info, pages):
                            add(name, type_, payload, {"font": info.get("name"), "style": info.get("style"),
                                                       "glyphs": len(info["glyphs"])} if type_ == s3p.TYPE_FONT else
                                {"font_page": p})
                    except Exception as e:  # noqa: BLE001
                        report["errors"][p] = f"{type(e).__name__}: {e}"
            print(f"  fonts {len(font_jobs)} ({time.time() - t1:.0f} s)", flush=True)
        for p in spine:
            try:
                raw = src.read(res.resolve(p))
                if p.endswith(".atlas"):
                    doc = spinefmt.engine_atlas(spinefmt.read_spatlas(raw), p.rsplit("/", 1)[0] if "/" in p else "")
                    info = {"pages": len(doc["pages"]), "regions": len(doc["regions"])}
                else:
                    doc = spinefmt.engine_skeleton(spinefmt.read_skeleton(raw))
                    info = {"bones": len(doc["bones"]), "slots": len(doc["slots"]),
                            "animations": len(doc["animations"])}
                    add(p, s3p.TYPE_SPINE, spinefmt.engine_binary(doc), info)
                    continue
                add(p, s3p.TYPE_SPINE, json.dumps(_round_floats(doc), separators=(",", ":")).encode("utf-8"), info)
            except Exception as e:  # noqa: BLE001
                report["errors"][p] = f"{type(e).__name__}: {e}"
        for p in audio:
            try:
                raw = src.read(res.resolve(p))
                r = godot_binary.parse_rsrc(raw)
                main = r["resources"][-1]
                add(p, s3p.TYPE_AUDIO, main["props"].get("data") or b"",
                    {"stream": main["type"], **{k: v for k, v in main["props"].items()
                                                if k in ("format", "mix_rate", "stereo", "loop", "bpm")}})
            except Exception as e:  # noqa: BLE001
                report["errors"][p] = f"{type(e).__name__}: {e}"

    # Folder listings the logic's game code reads (the combat background
    # picker lists scenes/backgrounds/<name>/layers), in .pck order.
    res_dirs = res_dir_listing(src)
    os.makedirs(os.path.join(a.out, "data"), exist_ok=True)
    with open(os.path.join(a.out, "data", "res_dirs.json"), "w", encoding="utf-8") as fh:
        json.dump(res_dirs, fh, indent=0)

    # Localization for the logic (data/localization/<lang>/*.json).
    loc = [f for f in files if f.startswith("localization/") and f.endswith(".json")]
    for f in loc:
        dest = os.path.join(a.out, "data", *f.split("/"))
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        with open(dest, "wb") as fh:
            fh.write(src.read(f))

    report["sizes"] = dict(sizes)
    with open(os.path.join(a.out, "pack_report.json"), "w", encoding="utf-8") as fh:
        json.dump(report, fh, indent=1, default=str)
    total = os.path.getsize(os.path.join(a.out, "pack.s3p"))
    print(f"pack.s3p {human(total)}: " + ", ".join(f"{k} {human(v)}" for k, v in sizes.most_common()))
    print(f"{len(loc)} localization files -> {os.path.join(a.out, 'data')}")
    print(f"{len(report['errors'])} errors" + (" (see pack_report.json)" if report["errors"] else ""))
    for k, v in list(report["errors"].items())[:10]:
        print(f"  {k}: {v}")
    print(f"build took {time.time() - t0:.0f} s")


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
    bu = sub.add_parser("bundle")
    bu.add_argument("pack")
    bu.add_argument("out")
    bu.add_argument("scenes", nargs="*")
    bu.add_argument("--max-mb", type=float, default=45)
    bu.add_argument("--have", action="append", help="a bundle zip already sent (repeatable): leave out its files")
    b = sub.add_parser("build")
    b.add_argument("pack")
    b.add_argument("out")
    b.add_argument("--scale", type=float, default=CONSOLE_SCALE)
    b.add_argument("--only", action="append", help="res path glob (repeatable), e.g. 'images/cards/*'")
    b.add_argument("--jobs", type=int, help="texture worker processes (default: cores - 1, at most 6)")
    b.add_argument("--languages", default="eng",
                   help="comma-separated localization folders whose characters the fonts keep "
                        "(default eng; 'all' keeps every glyph: gigabytes for the CJK fonts)")
    b.add_argument("--godot", help="Godot 4.5.1 binary for the font step")
    sub.add_parser("fetch-godot")
    a = ap.parse_args()
    try:
        {"survey": cmd_survey, "texture": cmd_texture, "bundle": cmd_bundle, "build": cmd_build,
         "fetch-godot": lambda _a: print(f"godot: {fontconv.fetch_godot()}")}[a.cmd](a)
    except KeyboardInterrupt:
        sys.exit(1)
    except Exception:  # noqa: BLE001
        traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()
