"""Binary resources (RSRC, and RSCC-compressed) saved by Godot 4.5.1 from
fixtures/rsrc/make_fixtures.gd: every value type the game's resources use."""
import os
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
from spire_convert import godot_binary as gb  # noqa: E402

FIX = os.path.join(HERE, "fixtures", "rsrc")


class BinaryResource(unittest.TestCase):
    def check(self, name):
        with open(os.path.join(FIX, name), "rb") as f:
            r = gb.parse_rsrc(f.read())
        self.assertEqual(r["type"], "GradientTexture1D")
        self.assertEqual([x["type"] for x in r["resources"]], ["Gradient", "GradientTexture1D"])
        self.assertEqual(r["ext_resources"][0][:2], ("Texture2D", "res://grad_opaque_256.png"))
        grad = r["resources"][0]["props"]
        self.assertEqual(grad["colors"], [(1.0, 0.0, 0.0, 1.0), (0.0, 0.0, 1.0, 0.5)])
        m = r["resources"][-1]["props"]
        self.assertIsInstance(m["gradient"], gb.SubRef)
        self.assertEqual(m["width"], 64)
        expect = {
            "int": 42, "big": 1 << 40, "float": 1.5, "str": "héllo", "sname": "name", "path": "../a/b:prop",
            "v2": (1.25, -2.0), "v2i": (3, 4), "rect": (1.0, 2.0, 3.0, 4.0), "color": (0.25, 0.5, 0.75, 1.0),
            "arr": [1, "two", 3.0, None, True], "dict": {"k": (1.0, 2.0, 3.0), 7: ["x"]},
            "bytes": b"\x01\x02\x03\x04\x05", "ints": [-1, 2, 3], "floats": [0.5, 2.5], "strings": ["a", "bc"],
            "v2s": [(1.0, 2.0), (3.0, 4.0)], "xform": (1.0, 0.0, -0.0, 1.0, 5.0, 6.0),
        }
        for k, v in expect.items():
            self.assertEqual(m["metadata/" + k], v, k)
        ext = m["metadata/ext"]
        self.assertEqual((ext.type, ext.path), ("Texture2D", "res://grad_opaque_256.png"))

    def test_plain(self):
        self.check("test_plain.res")

    def test_compressed(self):
        self.check("test_compressed.res")


if __name__ == "__main__":
    unittest.main()
