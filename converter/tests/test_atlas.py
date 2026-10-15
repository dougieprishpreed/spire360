"""TexturePacker sheets -> the AtlasTextures the game's loader serves."""
import json
import os
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
from spire_convert import atlas, variant  # noqa: E402

SHEET = json.dumps({"textures": [{"image": "intent_atlas.png", "size": {"w": 512, "h": 256}, "sprites": [
    {"filename": "attack/intent_attack_1.png", "region": {"x": 4, "y": 8, "w": 60, "h": 50},
     "margin": {"x": 0, "y": 0, "w": 0, "h": 0}},
    {"Filename": "intent_hidden", "Region": {"X": 70, "Y": 0, "W": 30, "H": 40},
     "Margin": {"X": 2, "Y": 3, "W": 4, "H": 6}}]}]})


class Atlas(unittest.TestCase):
    def test_paths_and_pages(self):
        self.assertTrue(atlas.is_sheet("images/atlases/intent_atlas.tpsheet"))
        self.assertFalse(atlas.is_sheet("images/atlases/sub/x.tpsheet"))
        self.assertEqual(atlas.page_images(SHEET), ["images/atlases/intent_atlas.png"])

    def test_sprites(self):
        out = dict(atlas.sprites(SHEET, "images/atlases/intent_atlas.tpsheet"))
        self.assertEqual(sorted(out), ["images/atlases/intent_atlas.sprites/attack/intent_attack_1.tres",
                                       "images/atlases/intent_atlas.sprites/intent_hidden.tres"])
        a = out["images/atlases/intent_atlas.sprites/attack/intent_attack_1.tres"]
        self.assertIn("region = Rect2(4, 8, 60, 50)", a)
        self.assertNotIn("margin", a)
        b = out["images/atlases/intent_atlas.sprites/intent_hidden.tres"]
        self.assertIn("margin = Rect2(2, 3, 4, 6)", b)
        # Parses like any Godot text resource.
        tr = variant.parse_text(b)
        self.assertEqual(list(tr.ext_resources.values())[0]["path"], "res://images/atlases/intent_atlas.png")


if __name__ == "__main__":
    unittest.main()
