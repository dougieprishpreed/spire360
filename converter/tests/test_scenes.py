"""Scene JSON: inline copies of a shader file become references to it."""
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from spire_convert import resources, scenes, variant

SWAY = 'shader_type canvas_item;\nuniform float speed = 1.0;\nvoid vertex() {\n    VERTEX.x += speed;\n}\n'


class InlineShaders(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.TemporaryDirectory()
        root = Path(self.dir.name)
        (root / "shaders").mkdir()
        (root / "shaders" / "sway.gdshader").write_text(SWAY, encoding="utf-8")
        self.res = resources.Resources(resources.DirSource(str(root)))

    def tearDown(self):
        self.dir.cleanup()

    def scene(self, code):
        quoted = code.replace('"', '\\"')
        return variant.parse_text('''[gd_scene format=3]
[sub_resource type="Shader" id="Shader_a"]
code = "%s"
[sub_resource type="ShaderMaterial" id="Mat_a"]
shader = SubResource("Shader_a")
shader_parameter/speed = 2.0
[node name="Root" type="Node2D"]
[node name="Leaf" type="Sprite2D" parent="."]
material = SubResource("Mat_a")
''' % quoted)

    def test_copy_of_a_file_refers_to_it(self):
        doc = scenes.to_json(self.scene(SWAY), self.res)
        self.assertEqual(doc["sub"]["Mat_a"]["props"]["shader"], {"ext": "shaders/sway.gdshader", "type": "Shader"})
        self.assertEqual(doc["nodes"][1]["props"]["material"], {"sub": "Mat_a"})

    def test_other_code_stays_inline(self):
        doc = scenes.to_json(self.scene(SWAY.replace("speed;", "speed * 2.0;")), self.res)
        self.assertEqual(doc["sub"]["Mat_a"]["props"]["shader"], {"sub": "Shader_a"})


if __name__ == "__main__":
    unittest.main()
