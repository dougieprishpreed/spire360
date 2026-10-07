"""Godot 4 CompressedTexture2D (.ctex, "GST2") reader.

Layout (Godot 4.x, format version 1, little-endian):

    "GST2" u32 version, u32 width, u32 height, u32 flags, i32 mipmap_limit,
    3 x u32 reserved                                           (36 bytes)
    u32 data_format   0 = raw Image data, 1 = PNG, 2 = WebP, 3 = Basis Universal
    u16 width, u16 height  (padded to the 4x4 block size for block formats)
    u32 mipmaps       number of levels after the first
    u32 image_format  Godot's Image::Format
    PNG/WebP: per level, u32 size + that many bytes of a PNG/WebP file
    raw:      every level's data back to back

flags bit 23 (HAS_MIPMAPS) mirrors mipmaps > 0. StS2's pack uses WebP
(lossless/lossy imports), DXT1/DXT5 (".s3tc.ctex") and BC7 (".bptc.ctex").
"""
import io
import struct

import numpy as np

MAGIC = b"GST2"
DATA_IMAGE, DATA_PNG, DATA_WEBP, DATA_BASIS = 0, 1, 2, 3
FLAG_HAS_MIPMAPS = 1 << 23

# Image::Format values used by imported 2D textures.
FMT_L8, FMT_LA8, FMT_R8, FMT_RG8, FMT_RGB8, FMT_RGBA8, FMT_RGBA4444, FMT_RGB565 = range(8)
FMT_DXT1, FMT_DXT3, FMT_DXT5, FMT_RGTC_R, FMT_RGTC_RG, FMT_BPTC_RGBA = 17, 18, 19, 20, 21, 22

FORMAT_NAMES = {
    FMT_L8: "L8", FMT_LA8: "LA8", FMT_R8: "R8", FMT_RG8: "RG8", FMT_RGB8: "RGB8", FMT_RGBA8: "RGBA8",
    FMT_RGBA4444: "RGBA4444", FMT_RGB565: "RGB565", FMT_DXT1: "DXT1", FMT_DXT3: "DXT3", FMT_DXT5: "DXT5",
    FMT_RGTC_R: "BC4", FMT_RGTC_RG: "BC5", FMT_BPTC_RGBA: "BC7",
}
# Bytes per pixel (plain formats) or per 4x4 block (block formats).
_PIXEL_BYTES = {FMT_L8: 1, FMT_LA8: 2, FMT_R8: 1, FMT_RG8: 2, FMT_RGB8: 3, FMT_RGBA8: 4,
                FMT_RGBA4444: 2, FMT_RGB565: 2}
_BLOCK_BYTES = {FMT_DXT1: 8, FMT_DXT3: 16, FMT_DXT5: 16, FMT_RGTC_R: 8, FMT_RGTC_RG: 16, FMT_BPTC_RGBA: 16}


class CtexError(ValueError):
    pass


def is_block_format(fmt):
    return fmt in _BLOCK_BYTES


def level_size(fmt, w, h):
    """Bytes of one level of a raw image (block formats round up to 4x4)."""
    if fmt in _PIXEL_BYTES:
        return w * h * _PIXEL_BYTES[fmt]
    if fmt in _BLOCK_BYTES:
        return ((w + 3) // 4) * ((h + 3) // 4) * _BLOCK_BYTES[fmt]
    raise CtexError(f"unsupported image format {fmt}")


class Level:
    __slots__ = ("width", "height", "data")

    def __init__(self, width, height, data):
        self.width, self.height, self.data = width, height, data


class Ctex:
    """A parsed .ctex. width/height are the texture's size; levels[0] may be
    larger for block formats (padded to whole 4x4 blocks)."""

    def __init__(self, raw):
        if len(raw) < 52 or raw[:4] != MAGIC:
            raise CtexError("not a Godot 4 .ctex (GST2) file")
        _, self.version, self.width, self.height, self.flags, self.mipmap_limit = struct.unpack_from("<4sIIIIi", raw, 0)
        if self.version != 1:
            raise CtexError(f"unsupported .ctex version {self.version}")
        self.data_format, iw, ih, mips, self.format = struct.unpack_from("<IHHII", raw, 36)
        self.levels = []
        pos = 52
        if self.data_format in (DATA_PNG, DATA_WEBP):
            for i in range(mips + 1):
                (n,) = struct.unpack_from("<I", raw, pos)
                pos += 4
                self.levels.append(Level(max(1, iw >> i), max(1, ih >> i), raw[pos:pos + n]))
                pos += n
        elif self.data_format == DATA_IMAGE:
            w, h = iw, ih
            for i in range(mips + 1):
                n = level_size(self.format, w, h)
                if pos + n > len(raw):
                    raise CtexError(f"level {i} truncated ({pos + n} > {len(raw)})")
                self.levels.append(Level(w, h, raw[pos:pos + n]))
                pos += n
                w, h = max(1, w >> 1), max(1, h >> 1)
        elif self.data_format == DATA_BASIS:
            raise CtexError("Basis Universal textures are not supported")
        else:
            raise CtexError(f"unknown data format {self.data_format}")

    @property
    def format_name(self):
        kind = {DATA_PNG: "PNG", DATA_WEBP: "WebP"}.get(self.data_format)
        return kind or FORMAT_NAMES.get(self.format, f"fmt{self.format}")

    @property
    def has_mipmaps(self):
        return len(self.levels) > 1

    def decode(self, level=0):
        """RGBA8 pixels of one level as a (h, w, 4) uint8 array, cropped to
        the texture's real size (level 0) or that size halved per level."""
        lv = self.levels[level]
        if self.data_format in (DATA_PNG, DATA_WEBP):
            from PIL import Image
            img = np.asarray(Image.open(io.BytesIO(lv.data)).convert("RGBA"))
        else:
            img = _decode_raw(self.format, lv.width, lv.height, lv.data)
        w = max(1, self.width >> level)
        h = max(1, self.height >> level)
        return np.ascontiguousarray(img[:h, :w])


def _decode_raw(fmt, w, h, data):
    if fmt in _BLOCK_BYTES:
        import texture2ddecoder as t2d
        fn = {FMT_DXT1: t2d.decode_bc1, FMT_DXT3: _decode_dxt3, FMT_DXT5: t2d.decode_bc3,
              FMT_RGTC_R: t2d.decode_bc4, FMT_RGTC_RG: t2d.decode_bc5, FMT_BPTC_RGBA: t2d.decode_bc7}[fmt]
        # Decoders work on whole blocks and return BGRA.
        bw, bh = (w + 3) // 4 * 4, (h + 3) // 4 * 4
        bgra = np.frombuffer(fn(bytes(data), bw, bh), np.uint8).reshape(bh, bw, 4)
        rgba = bgra[..., [2, 1, 0, 3]]
        if fmt == FMT_RGTC_R:
            rgba = np.dstack([rgba[..., 0], rgba[..., 0] * 0, rgba[..., 0] * 0, np.full_like(rgba[..., 0], 255)])
        return rgba
    a = np.frombuffer(data, np.uint8)
    if fmt == FMT_RGBA8:
        return a.reshape(h, w, 4)
    if fmt == FMT_RGB8:
        rgb = a.reshape(h, w, 3)
        return np.dstack([rgb, np.full((h, w), 255, np.uint8)])
    if fmt in (FMT_L8, FMT_R8):
        l = a.reshape(h, w)
        g = l if fmt == FMT_L8 else np.zeros_like(l)
        return np.dstack([l, g, g, np.full((h, w), 255, np.uint8)])
    if fmt == FMT_LA8:
        la = a.reshape(h, w, 2)
        return np.dstack([la[..., 0], la[..., 0], la[..., 0], la[..., 1]])
    if fmt == FMT_RG8:
        rg = a.reshape(h, w, 2)
        return np.dstack([rg, np.zeros((h, w), np.uint8), np.full((h, w), 255, np.uint8)])
    v = np.frombuffer(data, "<u2").reshape(h, w).astype(np.uint32)
    if fmt == FMT_RGBA4444:
        ch = [(v >> s) & 0xF for s in (12, 8, 4, 0)]
        return np.dstack([c * 17 for c in ch]).astype(np.uint8)
    if fmt == FMT_RGB565:
        r, g, b = (v >> 11) & 31, (v >> 5) & 63, v & 31
        return np.dstack([(r * 255 + 15) // 31, (g * 255 + 31) // 63, (b * 255 + 15) // 31,
                          np.full((h, w), 255)]).astype(np.uint8)
    raise CtexError(f"unsupported image format {fmt}")


def _decode_dxt3(data, w, h):
    """DXT3 (explicit 4-bit alpha + BC1-style colour); texture2ddecoder lacks
    it. Returns BGRA like the library's decoders. (Colour uses BC1 decoding,
    which differs from DXT3 only for blocks with color0 <= color1.)"""
    import texture2ddecoder as t2d
    blocks = np.frombuffer(data, np.uint8).reshape(-1, 16)
    bgra = np.frombuffer(t2d.decode_bc1(blocks[:, 8:].tobytes(), w, h), np.uint8).reshape(h, w, 4).copy()
    alpha = blocks[:, :8]
    a = np.empty((blocks.shape[0], 16), np.uint8)
    a[:, 0::2] = (alpha & 0x0F) * 17
    a[:, 1::2] = (alpha >> 4) * 17
    a = a.reshape(h // 4, w // 4, 4, 4).transpose(0, 2, 1, 3).reshape(h, w)
    bgra[..., 3] = a
    return bgra.tobytes()


def header(raw):
    """Header fields from the first 56 bytes of a .ctex: dict with width,
    height, flags, data_format, format, format_name, mipmaps, padded_width,
    padded_height."""
    if len(raw) < 52 or raw[:4] != MAGIC:
        raise CtexError("not a Godot 4 .ctex (GST2) file")
    _, version, w, h, flags, _limit = struct.unpack_from("<4sIIIIi", raw, 0)
    df, iw, ih, mips, fmt = struct.unpack_from("<IHHII", raw, 36)
    name = {DATA_PNG: "PNG", DATA_WEBP: "WebP", DATA_BASIS: "Basis"}.get(df) or FORMAT_NAMES.get(fmt, f"fmt{fmt}")
    return {"width": w, "height": h, "flags": flags, "data_format": df, "format": fmt, "format_name": name,
            "mipmaps": mips, "padded_width": iw, "padded_height": ih}


def load(path_or_bytes):
    if isinstance(path_or_bytes, (bytes, bytearray, memoryview)):
        return Ctex(bytes(path_or_bytes))
    with open(path_or_bytes, "rb") as f:
        return Ctex(f.read())
