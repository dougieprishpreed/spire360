"""Godot 4 binary formats: marshalled Variants and project.binary.

project.binary (exported project settings): "ECFG", u32 count, then per
setting: u32 key length, key (UTF-8), u32 value length, marshalled Variant.
Each value carries its length, so types this decoder does not know are
returned as ("<type>", n, raw) instead of failing.
"""
import struct

# Variant::Type (Godot 4)
NIL, BOOL, INT, FLOAT, STRING, VECTOR2, VECTOR2I, RECT2, RECT2I, VECTOR3, VECTOR3I = range(11)
COLOR, STRING_NAME, NODE_PATH = 20, 21, 22
DICTIONARY, ARRAY = 27, 28
PACKED_STRING_ARRAY = 34
FLAG_64 = 1 << 16


class _R:
    def __init__(self, b, pos=0):
        self.b, self.pos = b, pos

    def u32(self):
        v = struct.unpack_from("<I", self.b, self.pos)[0]
        self.pos += 4
        return v

    def take(self, n, fmt):
        v = struct.unpack_from(fmt, self.b, self.pos)
        self.pos += n
        return v

    def string(self):
        n = self.u32()
        s = self.b[self.pos:self.pos + n].decode("utf-8", "replace").rstrip("\0")
        self.pos += n + (-n % 4)
        return s


def decode_variant(r):
    hdr = r.u32()
    t, flags = hdr & 0xFFFF, hdr >> 16
    wide = hdr & FLAG_64
    if t == NIL:
        return None
    if t == BOOL:
        return bool(r.u32())
    if t == INT:
        return r.take(8, "<q")[0] if wide else r.take(4, "<i")[0]
    if t == FLOAT:
        return r.take(8, "<d")[0] if wide else r.take(4, "<f")[0]
    if t in (STRING, STRING_NAME):
        return r.string()
    if t in (VECTOR2, VECTOR2I):
        return r.take(8, "<ff" if t == VECTOR2 else "<ii")
    if t in (RECT2, RECT2I):
        return r.take(16, "<ffff" if t == RECT2 else "<iiii")
    if t in (VECTOR3, VECTOR3I):
        return r.take(12, "<fff" if t == VECTOR3 else "<iii")
    if t == COLOR:
        return r.take(16, "<ffff")
    if t == ARRAY:
        n = r.u32() & 0x7FFFFFFF
        return [decode_variant(r) for _ in range(n)]
    if t == DICTIONARY:
        n = r.u32() & 0x7FFFFFFF
        out = {}
        for _ in range(n):
            k = decode_variant(r)
            out[k if isinstance(k, (str, int, float, bool, tuple)) else repr(k)] = decode_variant(r)
        return out
    if t == PACKED_STRING_ARRAY:
        n = r.u32()
        return [r.string() for _ in range(n)]
    raise ValueError(f"variant type {t}")


def parse_project_binary(data):
    """{key: value} of an exported project.binary."""
    if data[:4] != b"ECFG":
        raise ValueError("not a project.binary (ECFG)")
    r = _R(data, 4)
    count = r.u32()
    out = {}
    for _ in range(count):
        n = r.u32()
        key = data[r.pos:r.pos + n].decode("utf-8", "replace")
        r.pos += n
        vlen = r.u32()
        end = r.pos + vlen
        try:
            out[key] = decode_variant(_R(data[r.pos:end]))
        except (ValueError, struct.error):
            out[key] = ("<undecoded>", vlen)
        r.pos = end
    return out
