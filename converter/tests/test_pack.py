"""Console texture conversion and the pack format."""
import glob
import os
import sys
import tempfile
import unittest

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
from spire_convert import ctex, s3p, texture  # noqa: E402

FIX = os.path.join(HERE, "fixtures", "ctex")


def psnr(a, b):
    mse = np.mean((a.astype(np.float64) - b.astype(np.float64)) ** 2)
    return 99.0 if mse == 0 else 10 * np.log10(255.0 ** 2 / mse)


class TextureConversion(unittest.TestCase):
    def test_scaled_formats_and_quality(self):
        for f in sorted(glob.glob(os.path.join(FIX, "grad_*__*.ctex"))):
            with self.subTest(f=os.path.basename(f)):
                src = ctex.load(f)
                t = texture.convert(src, 2 / 3)
                self.assertEqual((t.orig_width, t.orig_height), (src.width, src.height))
                self.assertEqual((t.width, t.height), (round(src.width * 2 / 3), round(src.height * 2 / 3)))
                rgba = src.decode(0)
                opaque = (rgba[..., 3] == 255).all()
                self.assertEqual(t.format, texture.FMT_DXT1 if opaque else texture.FMT_DXT5)
                self.assertEqual(len(t.levels) > 1, src.has_mipmaps)
                got = texture.decode_console(t)
                ref = np.asarray(Image.fromarray(rgba).resize((t.width, t.height), Image.LANCZOS))
                vis = ref[..., 3] >= 20
                self.assertGreater(psnr(got[vis][:, :3], ref[vis][:, :3]), 24.0)

    def test_dxt_passthrough_at_scale_1(self):
        f = os.path.join(FIX, "grad_opaque_256__s3tc.ctex")
        src = ctex.load(f)
        t = texture.convert(src, 1.0)
        self.assertEqual(t.format, texture.FMT_DXT1)
        self.assertEqual(t.levels[0], bytes(src.levels[0].data))

    def test_small_textures_stay_uncompressed(self):
        rgba = np.zeros((20, 30, 4), np.uint8)
        self.assertEqual(texture.choose_format(rgba, 30, 20), texture.FMT_ARGB8)


class Pack(unittest.TestCase):
    def test_roundtrip(self):
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, "pack.s3p")
            tex = texture.convert(ctex.load(os.path.join(FIX, "grad_alpha_101x89__lossless_mip.ctex")), 2 / 3)
            with s3p.Writer(path, "v0.111.0") as w:
                w.add("images/a.png", s3p.TYPE_TEXTURE, s3p.texture_payload(tex))
                w.add("scenes/x.json", s3p.TYPE_SCENE, b'{"nodes": []}')
                w.add("misc/blob.bin", s3p.TYPE_BLOB, bytes(range(256)) * 3)
            r = s3p.Reader(path)
            self.assertEqual(r.game_version, "v0.111.0")
            self.assertEqual(r.order, sorted(r.order))
            self.assertEqual(r.read("scenes/x.json"), b'{"nodes": []}')
            self.assertEqual(r.read("misc/blob.bin"), bytes(range(256)) * 3)
            self.assertEqual(r.entries["images/a.png"][1] % 4096, 0)
            p = s3p.parse_texture_payload(r.read("images/a.png"))
            self.assertEqual((p["width"], p["height"], p["orig_width"], p["orig_height"]),
                             (tex.width, tex.height, 101, 89))
            self.assertEqual([bytes(x[:len(y)]) for x, y in zip(p["levels"], tex.levels)], tex.levels)
            r.close()

    def test_fnv(self):
        self.assertEqual(s3p.fnv1a64(""), 0xCBF29CE484222325)
        self.assertEqual(s3p.fnv1a64("a"), 0xAF63DC4C8601EC8C)


if __name__ == "__main__":
    unittest.main()
