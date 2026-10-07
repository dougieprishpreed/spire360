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


# ---- compressed files (FileAccessCompressed, "RSCC" / "GCPF") ----

COMPRESSION_FASTLZ, COMPRESSION_DEFLATE, COMPRESSION_ZSTD, COMPRESSION_GZIP = 0, 1, 2, 3


def decompress_rscc(data):
    """'RSCC' + u32 mode + u32 block size + u32 total, u32 compressed size
    per block, then the blocks. Returns the uncompressed bytes."""
    magic, mode, block, total = struct.unpack_from("<4sIII", data, 0)
    if magic not in (b"RSCC", b"GCPF"):
        raise ValueError("not a compressed Godot file")
    n = (total + block - 1) // block
    sizes = struct.unpack_from(f"<{n}I", data, 16)
    pos = 16 + 4 * n
    out = bytearray()
    for i, s in enumerate(sizes):
        chunk = data[pos:pos + s]
        pos += s
        want = min(block, total - i * block)
        if mode == COMPRESSION_ZSTD:
            import zstandard
            out += zstandard.ZstdDecompressor().decompress(chunk, max_output_size=want)
        elif mode in (COMPRESSION_DEFLATE, COMPRESSION_GZIP):
            import zlib
            out += zlib.decompress(chunk, 15 + (16 if mode == COMPRESSION_GZIP else 0))
        else:
            raise ValueError(f"compression mode {mode} not supported")
    return bytes(out)


# ---- binary resources ("RSRC", .res / .scn / imported data) ----

class ExtRef:
    __slots__ = ("type", "path", "index")

    def __init__(self, type_, path, index=None):
        self.type, self.path, self.index = type_, path, index

    def __repr__(self):
        return f"ExtRef({self.type}, {self.path!r})"


class SubRef:
    __slots__ = ("index",)

    def __init__(self, index):
        self.index = index

    def __repr__(self):
        return f"SubRef({self.index})"


class _RsrcReader:
    def __init__(self, data):
        self.b = data
        self.pos = 0
        self.double = False
        self.strings = []
        self.ext = []

    def u32(self):
        v = struct.unpack_from("<I", self.b, self.pos)[0]
        self.pos += 4
        return v

    def u64(self):
        v = struct.unpack_from("<Q", self.b, self.pos)[0]
        self.pos += 8
        return v

    def f(self, n):
        fmt = f"<{n}{'d' if self.double else 'f'}"
        v = struct.unpack_from(fmt, self.b, self.pos)
        self.pos += struct.calcsize(fmt)
        return v

    def ustr(self):
        n = self.u32()
        s = self.b[self.pos:self.pos + n].rstrip(b"\0").decode("utf-8", "replace")
        self.pos += n
        return s

    def sname(self):
        idx = self.u32()
        if idx & 0x80000000:
            n = idx & 0x7FFFFFFF
            s = self.b[self.pos:self.pos + n].rstrip(b"\0").decode("utf-8", "replace")
            self.pos += n
            return s
        return self.strings[idx]

    def packed(self, fmt, size):
        n = self.u32()
        v = list(struct.unpack_from(f"<{n * len(fmt)}{fmt[0]}", self.b, self.pos)) if fmt else []
        self.pos += n * size
        return v

    def variant(self):
        t = self.u32()
        if t == 1:
            return None
        if t == 2:
            return bool(self.u32())
        if t == 3:
            return struct.unpack("<i", struct.pack("<I", self.u32()))[0]
        if t == 40:
            return struct.unpack("<q", struct.pack("<Q", self.u64()))[0]
        if t == 4:
            return self.f(1)[0]
        if t == 41:
            v = struct.unpack_from("<d", self.b, self.pos)[0]
            self.pos += 8
            return v
        if t in (5, 44):
            return self.ustr()
        if t in (10, 11, 12, 13, 14, 15, 16, 17, 18, 50, 52):
            return self.f({10: 2, 11: 4, 12: 3, 13: 4, 14: 4, 15: 6, 16: 9, 17: 12, 18: 6, 50: 4, 52: 16}[t])
        if t in (45, 46, 47, 51):
            n = {45: 2, 46: 4, 47: 3, 51: 4}[t]
            v = struct.unpack_from(f"<{n}i", self.b, self.pos)
            self.pos += 4 * n
            return v
        if t == 20:
            v = struct.unpack_from("<4f", self.b, self.pos)
            self.pos += 16
            return v
        if t == 22:
            names, subs = struct.unpack_from("<HH", self.b, self.pos)
            self.pos += 4
            absolute = bool(subs & 0x8000)
            subs &= 0x7FFF
            parts = [self.sname() for _ in range(names)]
            sub = [self.sname() for _ in range(subs)]
            return ("/" if absolute else "") + "/".join(parts) + "".join(":" + s for s in sub)
        if t == 23:
            return ("RID", self.u32())
        if t == 24:
            kind = self.u32()
            if kind == 0:
                return None
            if kind == 1:
                return ExtRef(self.ustr(), self.ustr())
            if kind == 2:
                return SubRef(self.u32())
            if kind == 3:
                i = self.u32()
                e = self.ext[i] if i < len(self.ext) else ("?", "?")
                return ExtRef(e[0], e[1], i)
            raise ValueError(f"object kind {kind}")
        if t in (42, 43):
            return None
        if t == 26:
            n = self.u32() & 0x7FFFFFFF
            out = {}
            for _ in range(n):
                k = self.variant()
                v = self.variant()
                out[k if isinstance(k, (str, int, float, bool, tuple)) else repr(k)] = v
            return out
        if t == 30:
            n = self.u32() & 0x7FFFFFFF
            return [self.variant() for _ in range(n)]
        if t == 31:
            n = self.u32()
            v = self.b[self.pos:self.pos + n]
            self.pos += n + (-n % 4)
            return bytes(v)
        if t == 32:
            return self.packed("i", 4)
        if t == 33:
            return self.packed("f", 4)
        if t == 48:
            return self.packed("q", 8)
        if t == 49:
            return self.packed("d", 8)
        if t == 34:
            n = self.u32()
            return [self.ustr() for _ in range(n)]
        if t in (35, 36, 37, 53):
            per = {35: 3, 36: 4, 37: 2, 53: 4}[t]
            n = self.u32()
            fmt = "f" if t == 36 or not self.double else "d"
            vals = struct.unpack_from(f"<{n * per}{fmt}", self.b, self.pos)
            self.pos += n * per * struct.calcsize(fmt)
            return [vals[i:i + per] for i in range(0, len(vals), per)]
        raise ValueError(f"binary resource variant type {t} at {self.pos - 4}")


def parse_rsrc(data):
    """A binary resource (RSRC, or RSCC-compressed). Returns
    {"type", "uid", "ext_resources": [(type, path, uid)],
     "resources": [{"path", "type", "props"}]}; the last resource is the main one."""
    if data[:4] == b"RSCC":
        # The compressed stream starts where "RSRC" would end, and resource
        # offsets count from the stream's start.
        r = _RsrcReader(decompress_rscc(data))
    elif data[:4] == b"RSRC":
        r = _RsrcReader(data)
        r.pos = 4
    else:
        raise ValueError("not a binary resource")
    if r.u32():
        raise ValueError("big-endian resources are not supported")
    r.double = bool(r.u32())
    major, minor, fmt = r.u32(), r.u32(), r.u32()
    main_type = r.ustr()
    r.u64()  # import metadata offset
    flags = r.u32()
    uid = r.u64()
    if flags & 8:
        r.ustr()  # script class
    if flags & 4:
        r.double = True
    r.pos += 4 * 11  # reserved
    r.strings = [r.ustr() for _ in range(r.u32())]
    using_uids = bool(flags & 2)
    for _ in range(r.u32()):
        t, p = r.ustr(), r.ustr()
        u = r.u64() if using_uids else None
        r.ext.append((t, p, u))
    entries = [(r.ustr(), r.u64()) for _ in range(r.u32())]
    out = []
    for path, off in entries:
        r.pos = off
        t = r.ustr()
        props = {}
        for _ in range(r.u32()):
            name = r.sname()
            props[name] = r.variant()
        out.append({"path": path, "type": t, "props": props})
    return {"type": main_type, "uid": uid, "version": (major, minor, fmt), "ext_resources": r.ext, "resources": out}
