"""The console pack (pack.s3p): one file the console reads directly.

All integers big-endian (the console's byte order). See docs/PACK_FORMAT.md.

    header (64 bytes)
      0  "S3P1"
      4  u32 version (1)
      8  u32 entry count
     12  u32 flags            bit 0: texture blocks in PC (little-endian) order
     16  u64 directory offset
     24  u64 names offset
     32  u32 names size
     36  u32 reserved
     40  char[24] game version, NUL padded
    directory: entry count x 32 bytes, sorted by hash
      u64 hash   FNV-1a 64 of the UTF-8 res path (no "res://")
      u64 offset
      u32 size
      u16 type   1 texture, 2 blob, 3 scene (JSON), 4 font, 5 audio
      u16 flags
      u32 name offset (into names: NUL-terminated UTF-8)
      u32 crc32 of the payload
    names, payloads (textures aligned to 4096, others to 16)

Texture payload:
      u8 format (1 DXT1, 2 DXT5, 3 ARGB8), u8 levels, u16 flags
                                     (bit 0 TEX_FLAG_DATA: not colour (MSDF
                                     glyphs): sample as is, never premultiply)
      u16 width, u16 height          stored size
      u16 orig_width, u16 orig_height size in layout units (the game's)
      u32 level offset [levels]      from the payload start, 16-aligned
      level data
"""
import struct
import zlib

TYPE_TEXTURE, TYPE_BLOB, TYPE_SCENE, TYPE_FONT, TYPE_AUDIO = 1, 2, 3, 4, 5
TYPE_NAMES = {1: "texture", 2: "blob", 3: "scene", 4: "font", 5: "audio"}
FLAG_TEX_LITTLE_ENDIAN = 1
TEX_FLAG_DATA = 1
HEADER = struct.Struct(">4sIIIQQII24s")
ENTRY = struct.Struct(">QQIHHII")
TEX_HEADER = struct.Struct(">BBHHHHH")


def fnv1a64(name):
    h = 0xCBF29CE484222325
    for b in name.encode("utf-8"):
        h ^= b
        h = (h * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return h


def texture_payload(t, flags=0):
    """ConsoleTexture -> bytes."""
    n = len(t.levels)
    head = TEX_HEADER.pack(t.format, n, flags, t.width, t.height, t.orig_width, t.orig_height)
    pos = len(head) + 4 * n
    pos += -pos % 16
    offsets = []
    body = bytearray()
    for lv in t.levels:
        offsets.append(pos + len(body))
        body += lv
        body += b"\0" * (-len(body) % 16)
    out = head + struct.pack(f">{n}I", *offsets)
    out += b"\0" * (-len(out) % 16)
    return out + bytes(body)


def parse_texture_payload(data):
    fmt, n, flags, w, h, ow, oh = TEX_HEADER.unpack_from(data, 0)
    offsets = struct.unpack_from(f">{n}I", data, TEX_HEADER.size)
    ends = list(offsets[1:]) + [len(data)]
    return {"format": fmt, "flags": flags, "width": w, "height": h, "orig_width": ow, "orig_height": oh,
            "levels": [data[o:e] for o, e in zip(offsets, ends)]}


class Writer:
    """Streams payloads to disk, writes the directory at the end."""

    def __init__(self, path, game_version="", flags=FLAG_TEX_LITTLE_ENDIAN):
        self.f = open(path, "wb")
        self.f.write(b"\0" * HEADER.size)
        self.entries = []
        self.names = {}
        self.game_version = game_version
        self.flags = flags

    def add(self, name, type_, payload, flags=0):
        h = fnv1a64(name)
        if h in self.names:
            other = self.names[h]
            if other != name:
                raise ValueError(f"hash collision: {name} / {other}")
            raise ValueError(f"duplicate entry {name}")
        self.names[h] = name
        align = 4096 if type_ == TYPE_TEXTURE else 16
        pos = self.f.tell()
        pad = -pos % align
        if pad:
            self.f.write(b"\0" * pad)
            pos += pad
        self.f.write(payload)
        self.entries.append((h, pos, len(payload), type_, flags, name, zlib.crc32(payload)))

    def close(self):
        names = bytearray()
        name_off = {}
        for e in self.entries:
            name_off[e[0]] = len(names)
            names += e[5].encode("utf-8") + b"\0"
        pos = self.f.tell()
        pos += -pos % 16
        self.f.seek(pos)
        dir_off = pos
        for h, off, size, t, fl, name, crc in sorted(self.entries):
            self.f.write(ENTRY.pack(h, off, size, t, fl, name_off[h], crc))
        names_off = self.f.tell()
        self.f.write(bytes(names))
        self.f.seek(0)
        self.f.write(HEADER.pack(b"S3P1", 1, len(self.entries), self.flags, dir_off, names_off, len(names), 0,
                                 self.game_version.encode("utf-8")[:24].ljust(24, b"\0")))
        self.f.close()

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        if exc[0] is None:
            self.close()
        else:
            self.f.close()


class Reader:
    def __init__(self, path):
        self.f = open(path, "rb")
        magic, ver, count, self.flags, dir_off, names_off, names_size, _, gv = HEADER.unpack(self.f.read(HEADER.size))
        if magic != b"S3P1" or ver != 1:
            raise ValueError("not a Spire360 pack")
        self.game_version = gv.rstrip(b"\0").decode()
        self.f.seek(names_off)
        names = self.f.read(names_size)
        self.f.seek(dir_off)
        self.entries = {}
        self.order = []
        for _ in range(count):
            h, off, size, t, fl, no, crc = ENTRY.unpack(self.f.read(ENTRY.size))
            name = names[no:names.index(b"\0", no)].decode("utf-8")
            self.entries[name] = (h, off, size, t, fl, crc)
            self.order.append(h)

    def read(self, name, verify=True):
        h, off, size, t, fl, crc = self.entries[name]
        self.f.seek(off)
        data = self.f.read(size)
        if verify and zlib.crc32(data) != crc:
            raise ValueError(f"{name}: CRC mismatch")
        return data

    def type(self, name):
        return self.entries[name][3]

    def close(self):
        self.f.close()
