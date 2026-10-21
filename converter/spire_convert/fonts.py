"""Fonts for the console: MSDF glyph atlases, metrics, kerning, ligatures.

The game imports its fonts as MSDF (multichannel signed distance field)
FontFiles. Godot renders glyphs on demand; the console cannot (no FreeType,
no HarfBuzz there), so the converter has Godot 4.5.1 itself render every
glyph of each font and measure its shaping, with tools/godot/font_export.gd.
Godot reproduces the game's own pre-rendered glyphs bit for bit, so the
console draws the same pixels the game would.

Pack entries per font (name = the font's res path, e.g. fonts/kreon_bold.ttf):
    <name>          type font: JSON (see font_export.gd), "pages" lists
                    the page textures
    <name>#page<N>  type texture: ARGB8, flag TEX_FLAG_DATA (MSDF data:
                    filtered as is, never premultiplied)
Godot's default theme font goes in as godot/OpenSans_SemiBold.ttf.

Only the characters the chosen languages need are exported (charset()):
the game's CJK fonts hold tens of thousands of glyphs, gigabytes as MSDF.
"""
import json
import os
import shutil
import subprocess
import sys
import tempfile
import urllib.request
import zipfile

import numpy as np
from PIL import Image

from . import s3p, texture

DEFAULT_FONT_NAME = "godot/OpenSans_SemiBold.ttf"
GODOT_VERSION = "4.5.1"
GODOT_URLS = {
    "win32": f"https://github.com/godotengine/godot/releases/download/{GODOT_VERSION}-stable/"
             f"Godot_v{GODOT_VERSION}-stable_win64.exe.zip",
    "linux": f"https://github.com/godotengine/godot/releases/download/{GODOT_VERSION}-stable/"
             f"Godot_v{GODOT_VERSION}-stable_linux.x86_64.zip",
}
SCRIPT = os.path.join(os.path.dirname(__file__), "..", "..", "tools", "godot", "font_export.gd")
CACHE_DIR = os.path.join(os.path.dirname(__file__), "..", ".cache", "godot")


def find_godot(explicit=None):
    """The Godot 4.5.1 binary: --godot, $SPIRE_GODOT, or one fetched by
    `convert.py fetch-godot`. None if there is none."""
    for p in (explicit, os.environ.get("SPIRE_GODOT")):
        if p and os.path.isfile(p):
            return p
    if os.path.isdir(CACHE_DIR):
        names = sorted(os.listdir(CACHE_DIR))
        # On Windows the _console build prints to the terminal.
        for n in names:
            if "console" in n and n.endswith(".exe"):
                return os.path.join(CACHE_DIR, n)
        for n in names:
            if n.startswith("Godot_v") and not n.endswith(".zip"):
                return os.path.join(CACHE_DIR, n)
    return None


def fetch_godot():
    """Downloads the official Godot 4.5.1 build (MIT) into converter/.cache."""
    url = GODOT_URLS.get("win32" if sys.platform == "win32" else "linux")
    os.makedirs(CACHE_DIR, exist_ok=True)
    zpath = os.path.join(CACHE_DIR, os.path.basename(url))
    print(f"downloading {url}", flush=True)
    urllib.request.urlretrieve(url, zpath)
    with zipfile.ZipFile(zpath) as z:
        z.extractall(CACHE_DIR)
    os.remove(zpath)
    for n in os.listdir(CACHE_DIR):
        p = os.path.join(CACHE_DIR, n)
        if n.startswith("Godot_v") and not n.endswith(".exe"):
            os.chmod(p, 0o755)
    return find_godot()


# Always kept, whatever the languages' text holds: printable ASCII (seeds,
# numbers, typed text) and the replacement character.
BASE_RANGES = [(0x20, 0x7E), (0xFFFD, 0xFFFD)]

# The language picker's native names (NLanguageDropdown._languageCodeToName):
# drawn through the languages' fonts even when the language is not chosen
# (the game leans on Godot's system font fallback for them).
LANGUAGE_NAMES = ("English 中文 繁體中文 Deutsch Español (Latinoamérica) Français Bahasa Indonesia Italiano "
                  "日本語 한국어 Polski Português Brasileiro Русский Español (Castellano) ไทย Türkçe")


def font_chars(font_path, chars, chosen, all_langs, by_lang=None):
    """The characters to export for one font. A language folder's font
    (fonts/jpn/...) serves its language: when the language is chosen it
    gets that language's characters (by_lang[lang], else all of chars),
    else just the native names. Every other font gets the chosen
    characters (None: all) and the native names."""
    parts = font_path.split("/")
    # FontManager: Polish draws with the Russian folder's fonts
    serves = {"rus": {"rus", "pol"}}.get(parts[1], {parts[1]}) if len(parts) > 2 else set()
    names = set(LANGUAGE_NAMES + " ")
    if chars is not None and len(parts) > 2 and parts[1] in all_langs:
        mine = serves & set(chosen)
        if not mine:
            return "".join(sorted(names))
        if by_lang is not None:
            own = set()
            for lang in mine:
                own |= set(by_lang.get(lang, ""))
            return "".join(sorted(own | names))
    if chars is not None:
        return "".join(sorted(set(chars) | names))
    return chars


def languages_with_glyphs(chosen, all_langs):
    """The pack's "languages" entry: the chosen languages (the picker shows
    only these; the others' fonts hold just the native names)."""
    return sorted(lang for lang in chosen if lang in all_langs)


def _strings(v, out):
    if isinstance(v, str):
        out.append(v)
    elif isinstance(v, dict):
        for x in v.values():
            _strings(x, out)
    elif isinstance(v, list):
        for x in v:
            _strings(x, out)


def charset(src, files, languages):
    """The characters to export: the base ranges, every string of the
    languages' localization tables, and the text of scenes and resources
    (labels set in the editor, the language picker's native names)."""
    chars = set()
    for a, b in BASE_RANGES:
        chars.update(chr(c) for c in range(a, b + 1))
    texts = []
    for f in files:
        try:
            if f.startswith("localization/") and f.endswith(".json") and f.split("/")[1] in languages:
                _strings(json.loads(src.read(f).decode("utf-8")), texts)
            elif f.endswith((".tscn", ".tres")):
                texts.append(src.read(f).decode("utf-8", "replace"))
        except Exception:  # noqa: BLE001
            continue
    for t in texts:
        chars.update(t)
    return "".join(sorted(c for c in chars if c >= " "))


def export(godot, fontdata_bytes, chars=None, timeout=1800):
    """Runs font_export.gd on a .fontdata (bytes) or "default"; chars: the
    characters to keep (None: all). Returns (info dict, [RGBA arrays of the
    pages])."""
    work = tempfile.mkdtemp(prefix="spire_font_")
    try:
        with open(os.path.join(work, "project.godot"), "w") as f:
            f.write("config_version=5\n")
        out = os.path.join(work, "out")
        os.makedirs(out)
        if fontdata_bytes == "default":
            src = "default"
        else:
            src = os.path.join(work, "font.fontdata")
            with open(src, "wb") as f:
                f.write(fontdata_bytes)
        cmd = [godot, "--headless", "--path", work, "--script", os.path.abspath(SCRIPT), "--", src, out]
        if chars is not None:
            with open(os.path.join(work, "chars.txt"), "w", encoding="utf-8") as f:
                f.write(chars)
            cmd.append(os.path.join(work, "chars.txt"))
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
        info_path = os.path.join(out, "font.json")
        if r.returncode != 0 or not os.path.isfile(info_path):
            raise RuntimeError(f"godot font export failed ({r.returncode}): {(r.stderr or r.stdout)[-800:]}")
        with open(info_path, encoding="utf-8") as f:
            info = json.load(f)
        pages = []
        for i in range(info["pages"]):
            with Image.open(os.path.join(out, f"page_{i}.png")) as im:
                pages.append(np.asarray(im.convert("RGBA")))
        return info, pages
    finally:
        shutil.rmtree(work, ignore_errors=True)


PAGE_SIZE = 512


def repack(info, pages, page_size=PAGE_SIZE, border=1):
    """Godot's glyph cache leaves most of each page empty (a dozen big MSDF
    glyphs on a 512 px page): the glyphs packed again in rows, each with a
    border of its own page's texels around its rect (bilinear filtering
    reads them), the uv rects moved with their sub-pixel offsets kept.
    Returns (info, pages)."""
    import math
    boxes = []
    for i, g in enumerate(info["glyphs"]):
        page, ux, uy, uw, uh = g[1], g[2], g[3], g[4], g[5]
        if page < 0 or page >= len(pages) or uw <= 0 or uh <= 0:
            continue
        x0, y0 = int(math.floor(ux)) - border, int(math.floor(uy)) - border
        x1, y1 = int(math.ceil(ux + uw)) + border, int(math.ceil(uy + uh)) + border
        boxes.append((y1 - y0, x1 - x0, i, x0, y0))
    boxes.sort(key=lambda b: (-b[0], -b[1]))
    out_pages = []
    glyphs = [list(g) for g in info["glyphs"]]
    cur = None
    x = y = row_h = 0
    for h, w, i, x0, y0 in boxes:
        if w > page_size or h > page_size:
            raise ValueError(f"glyph {glyphs[i][0]} ({w}x{h}) larger than a {page_size} px page")
        if cur is None or x + w > page_size:
            x, y, row_h = 0, y + row_h, 0
        if cur is None or y + h > page_size:
            cur = np.zeros((page_size, page_size, 4), dtype=np.uint8)
            out_pages.append(cur)
            x = y = row_h = 0
        src = pages[glyphs[i][1]]
        sh, sw = src.shape[:2]
        # the border may reach past the source page's edge: clamp, zeros there
        cx0, cy0, cx1, cy1 = max(x0, 0), max(y0, 0), min(x0 + w, sw), min(y0 + h, sh)
        cur[y + cy0 - y0:y + cy1 - y0, x + cx0 - x0:x + cx1 - x0] = src[cy0:cy1, cx0:cx1]
        glyphs[i][1] = len(out_pages) - 1
        glyphs[i][2] = glyphs[i][2] - x0 + x
        glyphs[i][3] = glyphs[i][3] - y0 + y
        x += w
        row_h = max(row_h, h)
    # the last page cut to the rows it holds (a power of two)
    if out_pages and y + row_h < page_size:
        used = y + row_h
        hh = 1
        while hh < used:
            hh *= 2
        out_pages[-1] = out_pages[-1][:hh]
    info = dict(info, glyphs=glyphs, pages=len(out_pages))
    return info, out_pages


def payloads(name, info, pages):
    """[(pack name, type, payload)] for one exported font, its glyphs packed
    again (repack)."""
    info, pages = repack(info, pages)
    out = []
    page_names = []
    for i, rgba in enumerate(pages):
        h, w = rgba.shape[:2]
        t = texture.ConsoleTexture(texture.FMT_ARGB8, w, h, w, h, [texture.encode(rgba, texture.FMT_ARGB8)])
        pn = f"{name}#page{i}"
        page_names.append(pn)
        out.append((pn, s3p.TYPE_TEXTURE, s3p.texture_payload(t, flags=s3p.TEX_FLAG_DATA)))
    meta = dict(info, pages=page_names)
    out.append((name, s3p.TYPE_FONT, json.dumps(meta, separators=(",", ":")).encode("utf-8")))
    return out
