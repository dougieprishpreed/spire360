"""Spine 4.2 binary reader and the engine form, on a small skeleton written
here (no game files in the repository)."""
import os
import struct
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
from spire_convert import spine  # noqa: E402


class W:
    """Writes the binary format's primitives."""

    def __init__(self):
        self.b = bytearray()

    def byte(self, v):
        self.b.append(v & 255)
        return self

    def boolean(self, v):
        return self.byte(1 if v else 0)

    def int(self, v):
        self.b += struct.pack(">I", v & 0xFFFFFFFF)
        return self

    def long(self, v):
        self.b += struct.pack(">q", v)
        return self

    def float(self, *vs):
        for v in vs:
            self.b += struct.pack(">f", v)
        return self

    def varint(self, v):
        v &= 0xFFFFFFFF
        while True:
            if v < 0x80:
                self.b.append(v)
                return self
            self.b.append((v & 0x7F) | 0x80)
            v >>= 7

    def string(self, s):
        if s is None:
            return self.varint(0)
        e = s.encode("utf-8")
        self.varint(len(e) + 1)
        self.b += e
        return self


def skeleton_bytes():
    w = W()
    w.long(1234).string("4.2.39").float(0, 0, 100, 200).float(100).boolean(False)
    strings = ["body", "head", "head2", "cape"]
    w.varint(len(strings))
    for s in strings:
        w.string(s)
    ref = {s: i + 1 for i, s in enumerate(strings)}
    # bones: root, body (child), noRotation child
    w.varint(3)
    w.string("root").float(0, 0, 0, 1, 1, 0, 0, 0).byte(0).boolean(False)
    w.string("body").varint(0).float(15, 10, 20, 1, 1, 0, 0, 50).byte(0).boolean(False)
    w.string("hat").varint(1).float(0, 5, 30, 1, 1, 0, 0, 10).byte(2).boolean(False)
    # slots
    w.varint(2)
    w.string("body").varint(1).int(-1).int(-1).varint(ref["body"]).varint(0)
    w.string("cape").varint(2).int(0xFF8040FF).int(-1).varint(ref["cape"]).varint(1)
    w.varint(0).varint(0).varint(0).varint(0)  # ik, transform, path, physics
    # default skin: slot 0 has a region (and a second one), slot 1 a mesh
    w.varint(2)
    w.varint(0).varint(2)
    w.varint(ref["body"]).byte(0).float(1, 2, 1, 1, 30, 40)
    w.varint(ref["head"]).byte(0 | 128).float(90).float(0, 0, 1, 1, 10, 10)
    w.varint(1).varint(1)
    w.varint(ref["cape"]).byte(2)  # mesh, unweighted, no path/colour/sequence
    w.varint(4)  # hull
    w.varint(4).float(0, 0, 10, 0, 10, 10, 0, 10)  # 4 vertices
    w.float(0, 1, 1, 1, 1, 0, 0, 0)  # region uvs
    for i in (0, 1, 2, 2, 3, 0):  # (2 * 4 - 4 - 2) * 3 = 6 indices
        w.varint(i)
    w.varint(0)  # other skins
    w.varint(0)  # events
    # one animation
    w.varint(1).string("idle")
    w.varint(5)  # timeline count
    # slots: slot 0 attachment + rgba
    w.varint(1).varint(0).varint(2)
    w.byte(0).varint(2).float(0).varint(ref["head"]).float(0.5).varint(0)
    w.byte(1).varint(2).varint(0)
    w.float(0).byte(255).byte(255).byte(255).byte(255)
    w.float(1).byte(0).byte(0).byte(0).byte(0).byte(1)  # key 1, then key 0's curve: stepped
    # bones: body rotate with a bezier between two keys
    w.varint(1).varint(1).varint(1)
    w.byte(0).varint(2).varint(1)
    w.float(0, 0)
    w.float(1, 90).byte(2).float(0.25, 0, 0.75, 90)
    w.varint(0).varint(0).varint(0).varint(0)  # ik, transform, path, physics
    # deform on the cape mesh: skin 0, slot 1
    w.varint(1).varint(0).varint(1).varint(1).varint(1).varint(ref["cape"]).byte(0).varint(2).varint(0)
    w.float(0).varint(0)  # key 0: no offsets
    w.float(1).byte(0)  # key 1's time, key 0's curve (linear)
    w.varint(2).varint(2).float(5, 6)  # key 1: two offsets from vertex value 2
    # draw order: one key swapping the slots
    w.varint(1).float(0.5).varint(1).varint(1).varint(-1)
    w.varint(0)  # events
    return bytes(w.b)


class SpineReader(unittest.TestCase):
    def test_reads_to_the_end(self):
        sk = spine.read_skeleton(skeleton_bytes())
        self.assertEqual([b["name"] for b in sk["bones"]], ["root", "body", "hat"])
        self.assertEqual(sk["bones"][2]["inherit"], "noRotationOrReflection")
        self.assertEqual(sk["slots"][1]["blend"], "additive")
        self.assertAlmostEqual(sk["slots"][1]["color"][1], 0x80 / 255)
        atts = sk["skins"][0]["attachments"]
        self.assertEqual([(a["slot"], a["key"], a["type"]) for a in atts],
                         [(0, "body", "region"), (0, "head", "region"), (1, "cape", "mesh")])
        self.assertEqual(atts[1]["rotation"], 90)
        self.assertEqual(atts[2]["triangles"], [0, 1, 2, 2, 3, 0])
        an = sk["animations"][0]
        self.assertEqual(an["slots"][1]["frames"][0]["curve"], "stepped")
        self.assertEqual(an["bones"][0]["frames"][0]["curve"], [0.25, 0, 0.75, 90])
        self.assertEqual(an["attachments"][0]["frames"][1]["offsets"], [5, 6])

    def test_rejects_trailing_bytes(self):
        with self.assertRaises(spine.SpineError):
            spine.read_skeleton(skeleton_bytes() + b"\0")

    def test_engine_form(self):
        e = spine.engine_skeleton(spine.read_skeleton(skeleton_bytes()))
        an = e["animations"][0]
        self.assertEqual(an["duration"], 1.0)
        by = {t["type"]: t for t in an["timelines"]}
        self.assertEqual(by["rotate"]["frames"], [0, 0, 1, 90])
        self.assertEqual(by["rotate"]["curves"], [[0.25, 0, 0.75, 90]])
        self.assertEqual(by["rgba"]["curves"], [1])
        self.assertEqual(by["attachment"]["names"], ["head", None])
        # Unweighted deform keys are whole vertex arrays (setup + offsets).
        self.assertEqual(by["deform"]["vertices"], [[0, 0, 10, 0, 10, 10, 0, 10], [0, 0, 15, 6, 10, 10, 0, 10]])
        self.assertEqual(by["draw_order"]["orders"], [[1, 0]])

    def test_binary_form(self):
        e = spine.engine_skeleton(spine.read_skeleton(skeleton_bytes()))
        b = spine.engine_binary(e)
        self.assertEqual(b[:4], b"SPB1")
        self.assertEqual(b[-4:], b"END1")
        # header: version, x, y, width, height, reference scale, bone count
        self.assertEqual(struct.unpack_from(">I5fI", b, 4), (1, 0.0, 0.0, 100.0, 200.0, 100.0, 3))

    def test_atlas(self):
        text = ("page.png\nsize:64,32\nfilter:Linear,Linear\nscale:0.5\n"
                "a\nbounds:0,0,10,20\noffsets:1,2,12,24\nrotate:90\nb\nbounds:20,0,5,5\n")
        a = spine.engine_atlas(spine.read_atlas(text), "anim/x")
        self.assertEqual(a["pages"][0]["texture"], "anim/x/page.png")
        self.assertEqual(a["pages"][0]["size"], [64, 32])
        ra, rb = a["regions"]
        self.assertEqual((ra["rotate"], ra["ox"], ra["oy"], ra["ow"], ra["oh"]), (90, 1, 2, 12, 24))
        self.assertEqual((rb["ow"], rb["oh"]), (5, 5))


if __name__ == "__main__":
    unittest.main()
