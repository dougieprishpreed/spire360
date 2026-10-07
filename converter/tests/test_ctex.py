"""Decodes .ctex fixtures made by Godot 4.5.1's importer from known PNGs.

Fixtures (tests/fixtures/ctex): <image>__<mode>.ctex, imported with
compress/mode lossless (WebP), lossy (WebP), raw (VRAM uncompressed),
s3tc (VRAM compressed: DXT1/DXT5) and bptc (VRAM high quality: BC7), with
and without mipmaps. The source images are synthetic.

    python -m unittest discover converter/tests
"""
import glob
import os
import sys
import unittest

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
from spire_convert import ctex  # noqa: E402

FIX = os.path.join(HERE, "fixtures", "ctex")

# Minimum PSNR (dB) of the decoded level 0 against the source per mode;
# None = must be exact.
EXPECT = {"lossless": None, "raw": None, "lossy": 24.0, "s3tc": 24.0, "bptc": 30.0}


def psnr(a, b):
    mse = np.mean((a.astype(np.float64) - b.astype(np.float64)) ** 2)
    return 99.0 if mse == 0 else 10 * np.log10(255.0 ** 2 / mse)


def source(name):
    return np.asarray(Image.open(os.path.join(FIX, name + ".png")).convert("RGBA"))


class CtexFixtures(unittest.TestCase):
    def fixtures(self):
        files = sorted(glob.glob(os.path.join(FIX, "*__*.ctex")))
        self.assertGreater(len(files), 20, "fixtures missing")
        for f in files:
            name, mode = os.path.basename(f)[:-5].split("__")
            yield f, name, mode.replace("_mip", ""), mode.endswith("_mip")

    def test_matches_godot(self):
        """Every level Godot itself decodes (fixtures/ctex/godot, written by
        Image.decompress() in Godot 4.5.1) must come out the same here. Block
        decoders may differ by rounding."""
        for f, name, mode, mip in self.fixtures():
            base = os.path.basename(f)[:-5]
            for level in (0, 1) if mip else (0,):
                ref_path = os.path.join(FIX, "godot", f"{base}__L{level}.png")
                with self.subTest(f=base, level=level):
                    ref = np.asarray(Image.open(ref_path).convert("RGBA"))
                    img = ctex.load(f).decode(level)
                    ref = ref[:img.shape[0], :img.shape[1]]  # Godot keeps the 4x4 padding
                    self.assertEqual(img.shape, ref.shape)
                    diff = np.abs(img.astype(int) - ref.astype(int))
                    tol = 0 if mode in ("lossless", "raw", "lossy") else 3
                    self.assertLessEqual(int(diff.max()), tol, f"max diff {diff.max()} mean {diff.mean():.3f}")

    def test_level0_matches_source(self):
        """Against the original PNG: exact where the format is lossless,
        a quality floor otherwise (smooth images only; noise does not survive
        lossy or block compression)."""
        for f, name, mode, _ in self.fixtures():
            with self.subTest(f=os.path.basename(f)):
                src = source(name)
                img = ctex.load(f).decode(0)
                self.assertEqual(img.shape, src.shape)
                # The importer's fix_alpha_border (on by default) recolours
                # nearly transparent pixels: compare alpha everywhere, colour
                # where visible.
                visible = src[..., 3] >= 20
                limit = EXPECT[mode]
                if limit is None:
                    np.testing.assert_array_equal(img[..., 3], src[..., 3])
                    np.testing.assert_array_equal(img[visible], src[visible])
                elif not name.startswith("noise"):
                    self.assertGreater(psnr(img[visible], src[visible]), limit)

    def test_formats(self):
        for f, name, mode, mip in self.fixtures():
            with self.subTest(f=os.path.basename(f)):
                t = ctex.load(f)
                self.assertEqual(t.has_mipmaps, mip)
                self.assertEqual(bool(t.flags & ctex.FLAG_HAS_MIPMAPS), mip)
                if mode in ("lossless", "lossy"):
                    self.assertEqual(t.data_format, ctex.DATA_WEBP)
                elif mode == "raw":
                    self.assertEqual(t.format, ctex.FMT_RGBA8)
                elif mode == "s3tc":
                    opaque = (source(name)[..., 3] == 255).all()
                    self.assertEqual(t.format, ctex.FMT_DXT1 if opaque else ctex.FMT_DXT5)
                elif mode == "bptc":
                    self.assertEqual(t.format, ctex.FMT_BPTC_RGBA)

    def test_mip_levels(self):
        for f, name, mode, mip in self.fixtures():
            if not mip:
                continue
            with self.subTest(f=os.path.basename(f)):
                t = ctex.load(f)
                w, h = t.width, t.height
                expect_levels = 1
                while w > 1 or h > 1:
                    w, h = max(1, w >> 1), max(1, h >> 1)
                    expect_levels += 1
                self.assertEqual(len(t.levels), expect_levels)
                lv1 = t.decode(1)
                src = source(name)
                self.assertEqual(lv1.shape[:2], (src.shape[0] >> 1, src.shape[1] >> 1))
                half = np.asarray(Image.fromarray(src).resize((lv1.shape[1], lv1.shape[0]), Image.BOX))
                if not name.startswith("noise"):
                    self.assertGreater(psnr(lv1, half), 20.0)
                last = t.decode(len(t.levels) - 1)
                self.assertEqual(last.shape[:2], (1, 1))

    def test_bad_magic(self):
        with self.assertRaises(ctex.CtexError):
            ctex.load(b"RSRC" + b"\0" * 60)


if __name__ == "__main__":
    unittest.main()
