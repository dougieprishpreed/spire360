"""Reader for Godot 4 resource packs (.pck), format versions 2 and 3.

Also finds a pack embedded at the end of an executable. Encrypted packs are
detected and reported, not decrypted.

Layout (Godot core/io/file_access_pack.cpp):
  u32 magic "GDPC", u32 format version (2 or 3), u32 major, minor, patch,
  u32 pack flags, u64 file base,
  v3: u64 directory offset   | v2: 16 reserved u32, directory follows
  directory: u32 count, then per file:
    u32 path length (padded), path bytes, u64 offset, u64 size, md5[16], u32 flags
"""
import fnmatch
import hashlib
import os
import struct

MAGIC = 0x43504447  # "GDPC"
PACK_DIR_ENCRYPTED = 1 << 0
PACK_REL_FILEBASE = 1 << 1
PACK_FILE_ENCRYPTED = 1 << 0
PACK_FILE_REMOVAL = 1 << 1


class PckError(Exception):
    pass


class PckEntry:
    __slots__ = ("path", "offset", "size", "md5", "flags")

    def __init__(self, path, offset, size, md5, flags):
        self.path = path
        self.offset = offset
        self.size = size
        self.md5 = md5
        self.flags = flags

    @property
    def encrypted(self):
        return bool(self.flags & PACK_FILE_ENCRYPTED)


class Pck:
    """Opened pack. Use as a context manager or call close()."""

    def __init__(self, path):
        self.path = path
        self.f = open(path, "rb")
        self.start = self._find_start()
        self.entries = {}
        self._read_directory()

    def close(self):
        self.f.close()

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()

    def _u32(self):
        return struct.unpack("<I", self.f.read(4))[0]

    def _u64(self):
        return struct.unpack("<Q", self.f.read(8))[0]

    def _find_start(self):
        self.f.seek(0)
        if self._u32() == MAGIC:
            return 0
        # Embedded in an executable: ... pack ... u64 pack_size, u32 magic
        self.f.seek(0, os.SEEK_END)
        end = self.f.tell()
        if end < 12:
            raise PckError("not a Godot pack")
        self.f.seek(end - 4)
        if self._u32() != MAGIC:
            raise PckError("not a Godot pack (no GDPC header or trailer)")
        self.f.seek(end - 12)
        size = self._u64()
        start = end - 12 - size
        self.f.seek(start)
        if self._u32() != MAGIC:
            raise PckError("embedded pack trailer found but header is not GDPC")
        return start

    def _read_directory(self):
        self.f.seek(self.start + 4)
        self.format_version = self._u32()
        self.godot_version = (self._u32(), self._u32(), self._u32())
        if self.format_version not in (2, 3):
            raise PckError(f"unsupported pack format version {self.format_version} (Godot {self.godot_version})")
        self.flags = self._u32()
        file_base = self._u64()
        if self.format_version == 3 or (self.flags & PACK_REL_FILEBASE):
            file_base += self.start
        if self.format_version == 3:
            self.f.seek(self._u64() + self.start)
        else:
            self.f.read(16 * 4)
        if self.flags & PACK_DIR_ENCRYPTED:
            raise PckError("the pack directory is encrypted (a key from the game executable is needed)")
        self.file_base = file_base
        count = self._u32()
        for _ in range(count):
            n = self._u32()
            raw = self.f.read(n)
            path = raw.split(b"\0", 1)[0].decode("utf-8")
            if path.startswith("res://"):
                path = path[6:]
            offset = self._u64()
            size = self._u64()
            md5 = self.f.read(16)
            flags = self._u32()
            if flags & PACK_FILE_REMOVAL:
                self.entries.pop(path, None)
                continue
            self.entries[path] = PckEntry(path, offset + file_base, size, md5, flags)

    def read(self, path):
        e = self.entries[path]
        if e.encrypted:
            raise PckError(f"{path} is encrypted")
        self.f.seek(e.offset)
        return self.f.read(e.size)

    def verify(self, path):
        """True if the file's MD5 matches the directory (all-zero MD5 = not recorded)."""
        e = self.entries[path]
        if e.md5 == b"\0" * 16:
            return True
        return hashlib.md5(self.read(path)).digest() == e.md5

    def match(self, patterns):
        """Paths matching any shell-style pattern ("**" behaves like "*")."""
        pats = [p.replace("**", "*") for p in patterns]
        return [p for p in sorted(self.entries) if any(fnmatch.fnmatchcase(p, q) for q in pats)]
