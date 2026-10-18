"""Bake Godot procedural textures with the same Godot build used for fonts.

Only the texture and its resource dependencies enter the temporary project;
game scripts are never loaded. Outputs are cached under converter/.cache.
"""
import hashlib
import json
import math
from pathlib import Path
import subprocess
import tempfile

import numpy as np
from PIL import Image

from . import fonts, s3p, texture, variant


def resource_text(tr, root, res):
    sections, ids = [], {}
    contexts = [tr] # keep external dependency contexts alive while ids use them

    def sub_resource(key, type_, props, context):
        if key not in ids:
            id_ = f"r{len(ids)}"
            ids[key] = id_
            body = properties(props, context)
            sections.append(f'[sub_resource type={json.dumps(type_)} id="{id_}"]\n{body}')
        return f'SubResource("{ids[key]}")'

    def value(v, context):
        if isinstance(v, variant.Ref):
            if v.kind == "SubResource":
                r = context.sub_resources[v.id]
                return sub_resource((id(context), v.id), r["type"], r["props"], context)
            path = context.ext_resources[v.id]["path"]
            resolved = res.resolve(path)
            if not resolved:
                raise ValueError(f"unresolved procedural dependency: {path}")
            child = variant.parse_text(res.src.read(resolved).decode("utf-8"))
            contexts.append(child)
            return sub_resource(("external", resolved), child.header[1]["type"], child.resource, child)
        if isinstance(v, variant.NodePath):
            return f"NodePath({json.dumps(str(v))})"
        if isinstance(v, variant.StringName):
            return "&" + json.dumps(str(v))
        if isinstance(v, variant.Ctor):
            return v.name + "(" + ", ".join(value(a, context) for a in v.args) + ")"
        if isinstance(v, list):
            return "[" + ", ".join(value(a, context) for a in v) + "]"
        if isinstance(v, dict):
            return "{" + ", ".join(value(k, context) + ": " + value(a, context) for k, a in v.items()) + "}"
        if isinstance(v, float) and not math.isfinite(v):
            return "inf" if v > 0 else "-inf" if v < 0 else "nan"
        return json.dumps(v, ensure_ascii=False)

    def properties(props, context):
        return "\n".join(k + " = " + value(v, context) for k, v in props.items()) + "\n"

    body = properties(root["props"], tr)
    return (f'[gd_resource type={json.dumps(root["type"])} load_steps={len(sections) + 1} format=3]\n\n'
            + "\n".join(sections) + "\n[resource]\n" + body)


class Baker:
    def __init__(self, godot, res):
        self.godot, self.res = godot, res
        self.cache = Path(fonts.CACHE_DIR).parent / "procedural"
        self.jobs = {}

    def prepare(self, tr, doc):
        roots = [(r, doc["sub"][id_]["props"]) for id_, r in tr.sub_resources.items()
                 if r["type"] == "NoiseTexture2D"]
        if not tr.is_scene and tr.header[1].get("type") == "NoiseTexture2D":
            roots.append((dict(type="NoiseTexture2D", props=tr.resource), doc["resource"]))
        for root, props in roots:
            if not self.godot:
                raise RuntimeError("procedural textures require Godot 4.5.1 (--godot or fetch-godot)")
            body = resource_text(tr, root, self.res)
            key = hashlib.sha256(("godot-4.5.1-noise-v1\n" + body).encode()).hexdigest()
            name = "generated/procedural/" + key + ".tex"
            props["spire_baked_texture"] = name
            self.jobs[name] = (key, body)

    def write(self, add):
        if not self.jobs:
            return
        self.cache.mkdir(parents=True, exist_ok=True)
        pending = []
        for key, body in self.jobs.values():
            src, out = self.cache / (key + ".tres"), self.cache / (key + ".png")
            if not out.is_file():
                src.write_text(body, encoding="utf-8")
                pending.append(dict(source=str(src.resolve()), output=str(out.resolve())))
        if pending:
            with tempfile.TemporaryDirectory(prefix="spire_procedural_") as work:
                Path(work, "project.godot").write_text("config_version=5\n")
                jobs = Path(work, "jobs.json")
                jobs.write_text(json.dumps(pending), encoding="utf-8")
                script = Path(__file__).resolve().parents[2] / "tools/godot/procedural_export.gd"
                result = subprocess.run([self.godot, "--headless", "--path", work, "--script", str(script),
                                         "--", str(jobs)], capture_output=True, text=True, timeout=600)
                if result.returncode or any(not Path(j["output"]).is_file() for j in pending):
                    raise RuntimeError("Godot procedural texture export failed: " + (result.stderr or result.stdout)[-2000:])
        for name, (key, _) in self.jobs.items():
            with Image.open(self.cache / (key + ".png")) as image:
                rgba = np.asarray(image.convert("RGBA"))
            h, w = rgba.shape[:2]
            fmt = texture.choose_format(rgba, w, h)
            tex = texture.ConsoleTexture(fmt, w, h, w, h, [texture.encode(rgba, fmt)])
            add(name, s3p.TYPE_TEXTURE, s3p.texture_payload(tex, flags=s3p.TEX_FLAG_DATA))
        print(f"  procedural textures {len(self.jobs)} ({len(pending)} generated)", flush=True)
