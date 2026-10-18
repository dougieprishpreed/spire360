"""Procedural resource isolation and Godot-backed noise export."""
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from spire_convert import fonts, procedural, resources, s3p, scenes, variant


def noise_scene(seed=0):
    return variant.parse_text('''[gd_scene format=3]
[sub_resource type="FastNoiseLite" id="noise"]
seed = %d
frequency = 0.04
[sub_resource type="NoiseTexture2D" id="texture"]
width = 64
height = 32
noise = SubResource("noise")
[node name="Unused" type="Node"]
''' % seed)


class Procedural(unittest.TestCase):
    def test_dependency_isolation(self):
        tr = noise_scene()
        text = procedural.resource_text(tr, tr.sub_resources["texture"], None)
        out = variant.parse_text(text)
        self.assertFalse(out.is_scene)
        self.assertEqual(out.header[1]["type"], "NoiseTexture2D")
        self.assertEqual(len(out.sub_resources), 1)
        ref = out.resource["noise"]
        self.assertEqual(out.sub_resources[ref.id]["type"], "FastNoiseLite")
        self.assertEqual(out.resource["width"], 64)
        self.assertNotIn("Unused", text)

    def test_godot_export_and_cache(self):
        godot = fonts.find_godot()
        if not godot:
            self.skipTest("Godot 4.5.1 unavailable")
        with tempfile.TemporaryDirectory() as work:
            res = resources.Resources(resources.DirSource(work))
            baker = procedural.Baker(godot, res)
            baker.cache = Path(work, "cache")
            names = []
            for seed in (1, 2, 1):
                tr = noise_scene(seed)
                doc = scenes.to_json(tr, res)
                baker.prepare(tr, doc)
                names.append(doc["sub"]["texture"]["props"]["spire_baked_texture"])
            self.assertEqual(names[0], names[2])
            self.assertNotEqual(names[0], names[1])
            exported = {}
            baker.write(lambda name, kind, payload: exported.update({name: (kind, payload)}))
            self.assertEqual(len(exported), 2)
            self.assertNotEqual(exported[names[0]][1], exported[names[1]][1])
            for kind, payload in exported.values():
                tex = s3p.parse_texture_payload(payload)
                self.assertEqual(kind, s3p.TYPE_TEXTURE)
                self.assertEqual((tex["width"], tex["height"]), (64, 32))
                self.assertTrue(tex["flags"] & s3p.TEX_FLAG_DATA)
            before = {p: p.stat().st_mtime_ns for p in baker.cache.glob("*.png")}
            baker.write(lambda *args: None)
            self.assertEqual(before, {p: p.stat().st_mtime_ns for p in baker.cache.glob("*.png")})


if __name__ == "__main__":
    unittest.main()
