"""Resolves res:// paths in the player's pack the way Godot does.

- An imported asset (res://images/x.png) is a stub "<path>.import" whose
  [remap] section names the real file (.godot/imported/x.png-<hash>.ctex);
  VRAM textures name one per format (path.s3tc, path.bptc).
- Exported scripts and some scenes have "<path>.remap" files.
- Scenes refer to resources by uid:// too; .godot/uid_cache.bin maps them.

Works on a .pck (Pck) or on a folder extracted from one (DirSource).
"""
import os
import struct

from . import variant


class DirSource:
    """A folder laid out like the pack (pck_tool.py extract)."""

    def __init__(self, root):
        self.root = root
        self.files = set()
        for d, _, fs in os.walk(root):
            for f in fs:
                self.files.add(os.path.relpath(os.path.join(d, f), root).replace(os.sep, "/"))

    def read(self, path):
        with open(os.path.join(self.root, path), "rb") as f:
            return f.read()

    def read_head(self, path, n):
        with open(os.path.join(self.root, path), "rb") as f:
            return f.read(n)

    def size(self, path):
        return os.path.getsize(os.path.join(self.root, path))

    def exists(self, path):
        return path in self.files

    def list(self):
        return sorted(self.files)


class PckSource:
    def __init__(self, pck):
        self.pck = pck

    def read(self, path):
        return self.pck.read(path)

    def read_head(self, path, n):
        return self.pck.read_head(path, n)

    def size(self, path):
        return self.pck.entries[path].size

    def exists(self, path):
        return path in self.pck.entries

    def list(self):
        return sorted(self.pck.entries)


def strip_res(path):
    return path[6:] if path.startswith("res://") else path


def parse_uid_cache(data):
    """.godot/uid_cache.bin: u32 count, then (u64 id, u32 len, path) entries.
    Returns {"uid://...": "res://..."}."""
    (count,) = struct.unpack_from("<I", data, 0)
    pos = 4
    out = {}
    for _ in range(count):
        uid, n = struct.unpack_from("<QI", data, pos)
        pos += 12
        path = data[pos:pos + n].rstrip(b"\0").decode("utf-8")
        pos += n
        out[uid_to_text(uid)] = path
    return out


_UID_CHARS = "abcdefghijklmnopqrstuvwxy012345678"


def uid_to_text(uid):
    """Godot's ResourceUID::id_to_text (base 34: a-y then 0-8, most significant first)."""
    if uid < 0:
        return "uid://<invalid>"
    chars = []
    v = uid
    while True:
        chars.append(_UID_CHARS[v % len(_UID_CHARS)])
        v //= len(_UID_CHARS)
        if v == 0:
            break
    return "uid://" + "".join(reversed(chars))


class Resources:
    """res:// lookups over a source. texture_formats orders the VRAM variants
    to prefer when an import has several (path.s3tc, path.bptc, ...)."""

    def __init__(self, source, texture_formats=("s3tc", "bptc")):
        self.src = source
        self.texture_formats = texture_formats
        self._imports = {}
        self._uids = None

    def _import(self, path):
        p = strip_res(path)
        if p not in self._imports:
            stub = p + ".import"
            self._imports[p] = (variant.parse_text(self.src.read(stub).decode("utf-8", "replace"))
                                if self.src.exists(stub) else None)
        return self._imports[p]

    def import_info(self, path):
        """The [remap] section of <path>.import, or None."""
        imp = self._import(path)
        if imp is None:
            return None
        for tag, attrs, props in imp.sections:
            if tag == "remap":
                return props
        return None

    def resolve(self, path):
        """The pack path holding a res:// path's data (following .import and
        .remap), or None."""
        if path.startswith("uid://"):
            path = self.uid_path(path) or ""
        p = strip_res(path)
        remap = self.import_info(p)
        if remap is not None:
            for key in ["path." + fmt for fmt in self.texture_formats] + ["path"]:
                if key in remap and self.src.exists(strip_res(remap[key])):
                    return strip_res(remap[key])
            return None
        rm = p + ".remap"
        if self.src.exists(rm):
            r = variant.parse_text(self.src.read(rm).decode("utf-8", "replace"))
            for tag, attrs, props in r.sections:
                if tag == "remap" and "path" in props:
                    return strip_res(props["path"])
        return p if self.src.exists(p) else None

    def read(self, path):
        real = self.resolve(path)
        if real is None:
            raise FileNotFoundError(path)
        return self.src.read(real)

    def importer(self, path):
        """(importer, type) of an imported asset, e.g. ("texture", "CompressedTexture2D")."""
        imp = self._import(path)
        if imp is None:
            return None
        for tag, attrs, props in imp.sections:
            if tag == "remap":
                return props.get("importer"), props.get("type")
        return None

    def uid_path(self, uid):
        if self._uids is None:
            cache = ".godot/uid_cache.bin"
            self._uids = parse_uid_cache(self.src.read(cache)) if self.src.exists(cache) else {}
        return self._uids.get(uid)

    def text(self, path):
        """A parsed .tscn/.tres."""
        return variant.parse_text(self.read(path).decode("utf-8"))
