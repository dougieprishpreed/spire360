"""Checks the node paths of engine/game/creature_vfx.cpp against the game's
scenes: for each script it ports (the VFX_TABLES rows and the classes
registered with make<...>), every node path it uses is looked up from the
script's spine sprite (its parent; the Queen's _spineNode) in the creature
scene that holds the script (instanced sub-scenes included).

    python tools/check_vfx_paths.py [scenes_dir]

scenes_dir: the game's .tscn files extracted from the pck (default
work/res_phobia: `python converter/pck_tool.py extract <pck> work/res_phobia
"*.tscn"`).
"""
import glob
import os
import posixpath
import re
import sys

SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "engine", "game", "creature_vfx.cpp")


def specs(src):
    out = {}
    # the tables: { "res://.../NName.cs", {plays}, { {when, path, op}, ... } },
    for m in re.finditer(r'\{ "res://src/Core/Nodes/Vfx/(N\w+)\.cs",', src):
        end = src.find('\n    { "res://', m.end())
        if end < 0:
            end = src.find("};", m.end())
        out[m.group(1)] = set(re.findall(r'\{ "[^"]+", "([^"]+)", \'.\' \}', src[m.end():end]))
    # the ports: register_script("res://.../NName.cs", make<Class>)
    for name, cls in re.findall(r'register_script\("res://src/Core/Nodes/Vfx/(N\w+)\.cs", make<(\w+)>\)', src):
        m = re.search(r"class " + cls + r" : public SpineScript \{(.*?)\n\};", src, re.S)
        if not m:
            continue
        body = m.group(1)
        paths = set(re.findall(r'node2?d?\("([^"]+)"\)', body))
        for arr in re.findall(r"static const char\* const \w+\[\d+\] = \{([^}]*)\}", body):
            paths |= set(re.findall(r'"([^"]+)"', arr))
        out[name] = paths
    return out


def scene_nodes(text, script_id, scenes_dir="", depth=0):
    nodes, parent = set(), None
    ext = dict((i, path) for path, i in re.findall(r'\[ext_resource type="PackedScene"[^\]]*path="res://([^"]+)" id="([^"]+)"\]', text))
    for nm in re.finditer(r'\[node name="([^"]+)"([^\]]*)\]((?:\n(?!\[).*)*)', text):
        n, attrs, body = nm.group(1), nm.group(2), nm.group(3)
        pm = re.search(r'parent="([^"]*)"', attrs)
        par = pm.group(1) if pm else None
        full = "." if par is None else (n if par == "." else par + "/" + n)
        nodes.add(full)
        # an instanced scene: its nodes under this one
        im = re.search(r'instance=ExtResource\("([^"]+)"\)', attrs)
        if im and im.group(1) in ext and depth < 3:
            sub = os.path.join(scenes_dir, ext[im.group(1)])
            if os.path.exists(sub):
                inner, _ = scene_nodes(open(sub, encoding="utf-8", errors="replace").read(), None, scenes_dir, depth + 1)
                for q in inner:
                    if q != ".":
                        nodes.add(q if full == "." else full + "/" + q)
        if script_id and f'script = ExtResource("{script_id}")' in body:
            sp = re.search(r'_spineNode = NodePath\("([^"]*)"\)', body)
            own = full
            parent = posixpath.normpath(posixpath.join(own, sp.group(1))) if sp else par
    return nodes, parent


def main():
    scenes_dir = sys.argv[1] if len(sys.argv) > 1 else "work/res_phobia"
    src = open(SRC, encoding="utf-8").read()
    scenes = glob.glob(os.path.join(scenes_dir, "scenes", "**", "*.tscn"), recursive=True)
    bad = 0
    for name, paths in sorted(specs(src).items()):
        for sc in scenes:
            t = open(sc, encoding="utf-8", errors="replace").read()
            m = re.search(r'\[ext_resource type="Script"[^\]]*path="res://src/Core/Nodes/Vfx/' + name + r'\.cs" id="([^"]+)"\]', t)
            if not m:
                continue
            nodes, sp = scene_nodes(t, m.group(1), scenes_dir)
            base = "." if sp in (".", None) else sp
            missing = []
            for p in paths:
                full = posixpath.normpath(posixpath.join(base, p))
                if full not in nodes:
                    missing.append(p)
            print(name, os.path.basename(sc), "sprite=" + base, ("MISSING " + str(sorted(missing))) if missing else "ok")
            bad += bool(missing)
            break
        else:
            print(name, "no scene")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
