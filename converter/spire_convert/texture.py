"""Godot textures -> console textures.

The game is laid out for 1920x1080; the console renders 1280x720, so
textures are stored at 2/3 of their size by default (the renderer draws them
at their original size in layout units). Formats:

    DXT1    opaque, 0.5 byte/pixel
    DXT5    with alpha, 1 byte/pixel
    ARGB8   small textures, where block artefacts would be large relative to
            the image (<= 1024 pixels after scaling), 4 bytes/pixel

Resampling is done on premultiplied alpha, so transparent pixels' colours
(which Godot's importer fills with neighbours) do not bleed into edges. A
source already in DXT1/DXT5 at scale 1 is passed through without
re-encoding. Mip chains are kept where the import had them.

Data is written in PC (little-endian) block order; the console loader swaps
16-bit words and tiles for Xenos when it creates the texture.
"""
import numpy as np
from PIL import Image

from . import ctex

FMT_DXT1, FMT_DXT5, FMT_ARGB8 = 1, 2, 3
FORMAT_NAMES = {FMT_DXT1: "DXT1", FMT_DXT5: "DXT5", FMT_ARGB8: "ARGB8"}
SMALL_PIXELS = 1024


class ConsoleTexture:
    __slots__ = ("format", "width", "height", "orig_width", "orig_height", "levels")

    def __init__(self, fmt, width, height, orig_width, orig_height, levels):
        self.format, self.width, self.height = fmt, width, height
        self.orig_width, self.orig_height = orig_width, orig_height
        self.levels = levels  # list of bytes, level 0 first

    @property
    def nbytes(self):
        return sum(len(lv) for lv in self.levels)


def _premultiplied_resize(rgba, w, h):
    img = Image.fromarray(rgba, "RGBA").convert("RGBa")
    return np.asarray(img.resize((w, h), Image.LANCZOS if w < rgba.shape[1] else Image.BICUBIC).convert("RGBA"))


def _box_half(rgba):
    h, w = rgba.shape[:2]
    nw, nh = max(1, w // 2), max(1, h // 2)
    return np.asarray(Image.fromarray(rgba, "RGBA").convert("RGBa").resize((nw, nh), Image.BOX).convert("RGBA"))


def _pad4(rgba):
    h, w = rgba.shape[:2]
    ph, pw = (-h) % 4, (-w) % 4
    if ph or pw:
        rgba = np.pad(rgba, ((0, ph), (0, pw), (0, 0)), mode="edge")
    return np.ascontiguousarray(rgba)


def encode(rgba, fmt):
    """One level in a console format (block formats padded to 4x4)."""
    import etcpak
    if fmt == FMT_ARGB8:
        # Stored as B, G, R, A bytes (D3DFMT_A8R8G8B8 in little-endian memory).
        return np.ascontiguousarray(rgba[..., [2, 1, 0, 3]]).tobytes()
    p = _pad4(rgba)
    h, w = p.shape[:2]
    data = p.tobytes()
    return etcpak.compress_bc1(data, w, h) if fmt == FMT_DXT1 else etcpak.compress_bc3(data, w, h)


def choose_format(rgba, w, h):
    if w * h <= SMALL_PIXELS:
        return FMT_ARGB8
    return FMT_DXT1 if (rgba[..., 3] == 255).all() else FMT_DXT5


def convert(tex, scale=2 / 3, force_format=None):
    """A ctex.Ctex -> ConsoleTexture."""
    ow, oh = tex.width, tex.height
    w, h = max(1, round(ow * scale)), max(1, round(oh * scale))
    src_fmt = {ctex.FMT_DXT1: FMT_DXT1, ctex.FMT_DXT5: FMT_DXT5}.get(tex.format) \
        if tex.data_format == ctex.DATA_IMAGE else None
    if (w, h) == (ow, oh) and src_fmt and force_format in (None, src_fmt) and w * h > SMALL_PIXELS:
        # Same size, already DXT: keep the game's own blocks.
        return ConsoleTexture(src_fmt, w, h, ow, oh, [bytes(lv.data) for lv in tex.levels])
    rgba = tex.decode(0)
    if (w, h) != (ow, oh):
        rgba = _premultiplied_resize(rgba, w, h)
    fmt = force_format or choose_format(rgba, w, h)
    levels = [encode(rgba, fmt)]
    if tex.has_mipmaps:
        cur = rgba
        while cur.shape[0] > 1 or cur.shape[1] > 1:
            cur = _box_half(cur)
            levels.append(encode(cur, fmt))
    return ConsoleTexture(fmt, w, h, ow, oh, levels)


def decode_console(t, level=0):
    """ConsoleTexture level -> RGBA (for tests and the PC viewer)."""
    import texture2ddecoder as t2d
    w, h = max(1, t.width >> level), max(1, t.height >> level)
    data = t.levels[level]
    if t.format == FMT_ARGB8:
        return np.frombuffer(data, np.uint8).reshape(h, w, 4)[..., [2, 1, 0, 3]]
    pw, ph = (w + 3) // 4 * 4, (h + 3) // 4 * 4
    fn = t2d.decode_bc1 if t.format == FMT_DXT1 else t2d.decode_bc3
    bgra = np.frombuffer(fn(bytes(data), pw, ph), np.uint8).reshape(ph, pw, 4)
    return np.ascontiguousarray(bgra[:h, :w, [2, 1, 0, 3]])
