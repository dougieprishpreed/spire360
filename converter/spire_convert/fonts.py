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


def export(godot, fontdata_bytes, timeout=1800):
    """Runs font_export.gd on a .fontdata (bytes) or "default".
    Returns (info dict, [RGBA arrays of the pages])."""
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


def payloads(name, info, pages):
    """[(pack name, type, payload)] for one exported font."""
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
