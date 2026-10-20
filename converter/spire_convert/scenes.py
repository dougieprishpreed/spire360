"""Text scenes/resources -> JSON for the console (pack entry type "scene").

The presentation layer rebuilds screens from these: the node tree with every
property as the game set it. References become pack names, so the console
never deals with res://, uid:// or .import remaps.

Value encoding (JSON):
    numbers, strings, bools, null, lists, objects as usual (inf/nan as
    {"f": "inf"} etc.)
    constructors  {"t": "Vector2", "v": [1, 2]}
    NodePath      {"np": "a/b"}
    StringName    plain string
    ExtResource   {"ext": "<pack name>", "type": "Texture2D"}
                  ({"ext": null, "type", "path": "res://..."} if not in the pack, e.g. scripts)
    SubResource   {"sub": "<id>"}

Scene JSON: {"kind": "scene"|"resource", "type", "ext": {id: {...}},
"sub": {id: {"type", "props"}}, "nodes": [{"name", "type", "parent",
"instance", "props", ...}], "connections": [...], "resource": {...}}
"""
import math

from . import resources, variant


def _value(v, ext_names):
    if isinstance(v, bool) or v is None or isinstance(v, int):
        return v
    if isinstance(v, float):
        if math.isnan(v):
            return {"f": "nan"}
        if math.isinf(v):
            return {"f": "inf" if v > 0 else "-inf"}
        return v
    if isinstance(v, variant.NodePath):
        return {"np": str(v)}
    if isinstance(v, str):
        return str(v)
    if isinstance(v, variant.Ref):
        if v.kind == "SubResource":
            return {"sub": v.id}
        name, type_, path = ext_names.get(v.id, (None, None, None))
        if name is None:
            return {"ext": None, "type": type_, "path": path}  # not in the pack (scripts): keep the res:// path
        if type_ == "Script":  # the engine attaches script ports by res:// path
            return {"ext": name, "type": type_, "path": path}
        return {"ext": name, "type": type_}
    if isinstance(v, variant.Ctor):
        return {"t": v.name, "v": [_value(a, ext_names) for a in v.args]}
    if isinstance(v, variant.ObjectValue):
        return {"t": "Object", "class": v.type, "props": {str(k): _value(x, ext_names) for k, x in v.props.items()}}
    if isinstance(v, list):
        return [_value(x, ext_names) for x in v]
    if isinstance(v, dict):
        return {str(k): _value(x, ext_names) for k, x in v.items()}
    return repr(v)


def pack_name_for(res, path, uid=None):
    """The pack entry name of a res:// reference (the res path without the
    prefix, for imported assets the source path, e.g. images/x.png)."""
    if uid and uid.startswith("uid://"):
        p = res.uid_path(uid)
        if p:
            path = p
    p = resources.strip_res(path)
    return p if res.resolve(p) is not None else None


_SHADER_FILES = {}


def known_shader(res, code):
    """The game's .gdshader file whose code an inline Shader repeats (the
    Overgrowth rest site embeds copies of shaders/vfx/wind_sway.gdshader),
    or None. The engine ports shaders by file, so such a copy is that
    file."""
    files = _SHADER_FILES.get(id(res))
    if files is None:
        files = {}
        for f in res.src.list():
            if f.endswith(".gdshader"):
                files.setdefault(_normal(res.src.read(f).decode("utf-8", "replace")), f)
        _SHADER_FILES[id(res)] = files
    return files.get(_normal(code)) if isinstance(code, str) else None


def _normal(code):
    return code.replace("\r\n", "\n").strip()


def _same_shader(v, copies):
    """{"sub": id} of an inline copy -> the file's {"ext": path}."""
    if isinstance(v, dict):
        if set(v) == {"sub"} and v["sub"] in copies:
            return {"ext": copies[v["sub"]], "type": "Shader"}
        return {k: _same_shader(x, copies) for k, x in v.items()}
    if isinstance(v, list):
        return [_same_shader(x, copies) for x in v]
    return v


def to_json(tr, res):
    ext_names = {}
    ext = {}
    for id_, e in tr.ext_resources.items():
        name = pack_name_for(res, e.get("path", ""), e.get("uid"))
        ext_names[id_] = (name, e.get("type"), e.get("path"))
        ext[id_] = {"type": e.get("type"), "name": name, "path": e.get("path")}
    out = {
        "kind": "scene" if tr.is_scene else "resource",
        "type": tr.header[1].get("type"),
        "ext": ext,
        "sub": {id_: {"type": s["type"], "props": {k: _value(v, ext_names) for k, v in s["props"].items()}}
                for id_, s in tr.sub_resources.items()},
    }
    if tr.is_scene:
        nodes = []
        for n in tr.nodes:
            node = {k: _value(v, ext_names) for k, v in n.items() if k != "props"}
            node["props"] = {k: _value(v, ext_names) for k, v in n["props"].items()}
            nodes.append(node)
        out["nodes"] = nodes
        out["connections"] = [{k: _value(v, ext_names) for k, v in c.items()} for c in tr.connections]
    else:
        out["resource"] = {k: _value(v, ext_names) for k, v in tr.resource.items()}
    copies = {}
    for id_, s in tr.sub_resources.items():
        if s["type"] == "Shader":
            path = known_shader(res, s["props"].get("code"))
            if path:
                copies[id_] = path
    if copies:
        for k in ("sub", "nodes", "resource"):
            if k in out:
                out[k] = _same_shader(out[k], copies)
    return out
