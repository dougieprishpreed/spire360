"""The game's sprite atlases (images/atlases/<atlas>.tpsheet, TexturePacker
JSON, plus page images next to it).

The game installs its own resource loader (AtlasResourceLoader, in front of
Godot's) for every path res://images/atlases/<atlas>.sprites/<sprite>.tres:
it answers with an AtlasTexture cut from the atlas's .tpsheet, whether or
not a .tres file exists at that path (some atlases have stale or no .tres
files in the .pck). The converter does the same: each sprite of each sheet
becomes that .tres, written as Godot text so it converts like any other
resource."""
import json

ATLAS_DIR = "images/atlases/"


def is_sheet(path):
    return path.startswith(ATLAS_DIR) and path.endswith(".tpsheet") and path.count("/") == 2


def sheet_for_sprite(path):
    """The sheet behind a sprite path (images/atlases/<atlas>.sprites/...),
    else None."""
    if not path.startswith(ATLAS_DIR):
        return None
    head = path[len(ATLAS_DIR):].split("/", 1)[0]
    if not head.endswith(".sprites"):
        return None
    return ATLAS_DIR + head[:-len(".sprites")] + ".tpsheet"


def atlas_name(sheet_path):
    return sheet_path[len(ATLAS_DIR):-len(".tpsheet")]


def _get(d, key, default=None):
    """The game reads the sheet case-insensitively (PropertyNameCaseInsensitive)."""
    for k, v in d.items():
        if k.lower() == key:
            return v
    return default


def _rect(d):
    d = d or {}
    return [int(_get(d, k, 0) or 0) for k in ("x", "y", "w", "h")]


def page_images(raw):
    """The page images a sheet uses (pack paths)."""
    sheet = json.loads(raw)
    return [ATLAS_DIR + _get(t, "image", "") for t in _get(sheet, "textures", []) or []]


def sprites(raw, sheet_path):
    """[(pack path of the sprite's .tres, Godot text of an AtlasTexture)] as
    AtlasManager.CreateAtlasTexture makes them: region, and the margin when
    any of its values is set."""
    sheet = json.loads(raw)
    base = ATLAS_DIR + atlas_name(sheet_path) + ".sprites/"
    out = []
    for tex in _get(sheet, "textures", []) or []:
        image = ATLAS_DIR + _get(tex, "image", "")
        for sp in _get(tex, "sprites", []) or []:
            name = _get(sp, "filename", "")
            if name.lower().endswith(".png"):  # NormalizeSpriteKey
                name = name[:-4]
            region = _rect(_get(sp, "region"))
            margin = _rect(_get(sp, "margin"))
            text = ('[gd_resource type="AtlasTexture" load_steps=2 format=3]\n\n'
                    f'[ext_resource type="Texture2D" path="res://{image}" id="1"]\n\n'
                    '[resource]\n'
                    'atlas = ExtResource("1")\n'
                    'region = Rect2({}, {}, {}, {})\n'.format(*region))
            if any(margin):
                text += 'margin = Rect2({}, {}, {}, {})\n'.format(*margin)
            out.append((base + name + ".tres", text))
    return out
