"""Spine 4.2 binary skeletons (.skel, Godot imports them as .spskel) and
text atlases (.atlas / .spatlas JSON with "atlas_data").

Our own reader (DECISIONS D12): read_skeleton() turns the binary export
into plain dicts; the converter stores them as JSON for the engine's runtime
(engine/spine). Every file must parse to its last byte, which is how the
layout is checked.

Byte order is big-endian. Integers marked "varint" use 7 bits per byte, low
group first, high bit = more (optionally zig-zag). Strings are a varint
byte count + 1 (0 = null, 1 = empty), then UTF-8. Colours are RGBA8888.
"""
import json
import struct

INHERIT = ["normal", "onlyTranslation", "noRotationOrReflection", "noScale", "noScaleOrReflection"]
BLEND = ["normal", "additive", "multiply", "screen"]
ATTACHMENT_TYPES = ["region", "boundingbox", "mesh", "linkedmesh", "path", "point", "clipping"]

# slot timelines
SLOT_ATTACHMENT, SLOT_RGBA, SLOT_RGB, SLOT_RGBA2, SLOT_RGB2, SLOT_ALPHA = range(6)
# bone timelines
BONE_NAMES = ["rotate", "translate", "translatex", "translatey", "scale", "scalex", "scaley", "shear", "shearx",
              "sheary", "inherit"]
# path constraint timelines
PATH_POSITION, PATH_SPACING, PATH_MIX = range(3)
# physics timelines
PHYSICS_NAMES = {0: "inertia", 1: "strength", 2: "damping", 4: "mass", 5: "wind", 6: "gravity", 7: "mix", 8: "reset"}
# attachment timelines
ATTACH_DEFORM, ATTACH_SEQUENCE = range(2)
CURVE_LINEAR, CURVE_STEPPED, CURVE_BEZIER = range(3)


class SpineError(ValueError):
    pass


class Reader:
    def __init__(self, data):
        self.d = data
        self.p = 0
        self.strings = []

    def byte(self):
        if self.p >= len(self.d):
            raise SpineError(f"read past the end at {self.p}")
        b = self.d[self.p]
        self.p += 1
        return b

    def sbyte(self):
        b = self.byte()
        return b - 256 if b > 127 else b

    def boolean(self):
        return self.byte() != 0

    def short(self):
        v = struct.unpack_from(">h", self.d, self.p)[0]
        self.p += 2
        return v

    def int(self):
        v = struct.unpack_from(">i", self.d, self.p)[0]
        self.p += 4
        return v

    def long(self):
        v = struct.unpack_from(">q", self.d, self.p)[0]
        self.p += 8
        return v

    def float(self):
        v = struct.unpack_from(">f", self.d, self.p)[0]
        self.p += 4
        return v

    def varint(self, optimize_positive=True):
        result = 0
        shift = 0
        while True:
            b = self.byte()
            result |= (b & 0x7F) << shift
            if not b & 0x80:
                break
            shift += 7
            if shift > 35:
                raise SpineError(f"bad varint at {self.p}")
        result &= 0xFFFFFFFF
        if not optimize_positive:
            result = (result >> 1) ^ -(result & 1)
        elif result & 0x80000000:
            result -= 1 << 32
        return result

    def string(self):
        n = self.varint()
        if n == 0:
            return None
        n -= 1
        s = self.d[self.p:self.p + n].decode("utf-8")
        self.p += n
        return s

    def string_ref(self):
        i = self.varint()
        return None if i == 0 else self.strings[i - 1]

    def color(self):
        v = self.int() & 0xFFFFFFFF
        return [((v >> 24) & 255) / 255.0, ((v >> 16) & 255) / 255.0, ((v >> 8) & 255) / 255.0, (v & 255) / 255.0]

    def floats(self, n):
        v = list(struct.unpack_from(f">{n}f", self.d, self.p))
        self.p += 4 * n
        return v


def read_skeleton(data):
    r = Reader(data)
    sk = {"hash": r.long(), "version": r.string()}
    if not (sk["version"] or "").startswith("4.2"):
        raise SpineError(f"Spine {sk['version']} (want 4.2)")
    sk["x"], sk["y"], sk["width"], sk["height"] = r.floats(4)
    sk["reference_scale"] = r.float()
    nonessential = r.boolean()
    if nonessential:
        sk["fps"] = r.float()
        sk["images"] = r.string()
        sk["audio"] = r.string()
    r.strings = [r.string() for _ in range(r.varint())]

    bones = []
    for i in range(r.varint()):
        b = {"name": r.string()}
        b["parent"] = None if i == 0 else r.varint()
        b["rotation"], b["x"], b["y"], b["scale_x"], b["scale_y"], b["shear_x"], b["shear_y"], b["length"] = r.floats(8)
        b["inherit"] = INHERIT[r.byte()]
        b["skin_required"] = r.boolean()
        if nonessential:
            r.int()  # colour
            r.string()  # icon
            r.boolean()  # visible
        bones.append(b)
    sk["bones"] = bones

    slots = []
    for _ in range(r.varint()):
        s = {"name": r.string(), "bone": r.varint(), "color": r.color()}
        dark = r.int()
        if dark != -1:
            v = dark & 0xFFFFFFFF
            s["dark"] = [((v >> 16) & 255) / 255.0, ((v >> 8) & 255) / 255.0, (v & 255) / 255.0]
        s["attachment"] = r.string_ref()
        s["blend"] = BLEND[r.varint()]
        if nonessential:
            r.boolean()  # visible
        slots.append(s)
    sk["slots"] = slots

    iks = []
    for _ in range(r.varint()):
        c = {"name": r.string(), "order": r.varint()}
        c["bones"] = [r.varint() for _ in range(r.varint())]
        c["target"] = r.varint()
        f = r.byte()
        c["skin_required"] = bool(f & 1)
        c["bend"] = 1 if f & 2 else -1
        c["compress"] = bool(f & 4)
        c["stretch"] = bool(f & 8)
        c["uniform"] = bool(f & 16)
        c["mix"] = 1.0
        if f & 32:
            c["mix"] = r.float() if f & 64 else 1.0
        else:
            c["mix"] = 0.0 if not (f & 64) else 1.0
        c["softness"] = r.float() if f & 128 else 0.0
        iks.append(c)
    sk["ik"] = iks

    transforms = []
    for _ in range(r.varint()):
        c = {"name": r.string(), "order": r.varint()}
        c["bones"] = [r.varint() for _ in range(r.varint())]
        c["target"] = r.varint()
        f = r.byte()
        c["skin_required"] = bool(f & 1)
        c["local"] = bool(f & 2)
        c["relative"] = bool(f & 4)
        c["offset_rotation"] = r.float() if f & 8 else 0.0
        c["offset_x"] = r.float() if f & 16 else 0.0
        c["offset_y"] = r.float() if f & 32 else 0.0
        c["offset_scale_x"] = r.float() if f & 64 else 0.0
        c["offset_scale_y"] = r.float() if f & 128 else 0.0
        f = r.byte()
        c["offset_shear_y"] = r.float() if f & 1 else 0.0
        c["mix_rotate"] = r.float() if f & 2 else 0.0
        c["mix_x"] = r.float() if f & 4 else 0.0
        c["mix_y"] = r.float() if f & 8 else 0.0
        c["mix_scale_x"] = r.float() if f & 16 else 0.0
        c["mix_scale_y"] = r.float() if f & 32 else 0.0
        c["mix_shear_y"] = r.float() if f & 64 else 0.0
        transforms.append(c)
    sk["transform"] = transforms

    paths = []
    for _ in range(r.varint()):
        c = {"name": r.string(), "order": r.varint(), "skin_required": r.boolean()}
        c["bones"] = [r.varint() for _ in range(r.varint())]
        c["target"] = r.varint()
        f = r.byte()
        c["position_mode"] = f & 1
        c["spacing_mode"] = (f >> 1) & 3
        c["rotate_mode"] = (f >> 3) & 3
        c["offset_rotation"] = r.float() if f & 128 else 0.0
        c["position"], c["spacing"], c["mix_rotate"], c["mix_x"], c["mix_y"] = r.floats(5)
        paths.append(c)
    sk["path"] = paths

    physics = []
    for _ in range(r.varint()):
        c = {"name": r.string(), "order": r.varint(), "bone": r.varint()}
        f = r.byte()
        c["skin_required"] = bool(f & 1)
        c["x"] = r.float() if f & 2 else 0.0
        c["y"] = r.float() if f & 4 else 0.0
        c["rotate"] = r.float() if f & 8 else 0.0
        c["scale_x"] = r.float() if f & 16 else 0.0
        c["shear_x"] = r.float() if f & 32 else 0.0
        c["limit"] = r.float() if f & 64 else 5000.0
        c["step"] = 1.0 / r.byte()
        c["inertia"], c["strength"], c["damping"] = r.floats(3)
        c["mass_inverse"] = r.float() if f & 128 else 1.0
        c["wind"], c["gravity"] = r.floats(2)
        f = r.byte()
        for k, bit in (("inertia_global", 1), ("strength_global", 2), ("damping_global", 4), ("mass_global", 8),
                       ("wind_global", 16), ("gravity_global", 32), ("mix_global", 64)):
            c[k] = bool(f & bit)
        c["mix"] = r.float() if f & 128 else 1.0
        physics.append(c)
    sk["physics"] = physics

    skins = []
    default = read_skin(r, True, nonessential)
    if default:
        skins.append(default)
    for _ in range(r.varint()):
        skins.append(read_skin(r, False, nonessential))
    sk["skins"] = skins

    events = []
    for _ in range(r.varint()):
        e = {"name": r.string(), "int": r.varint(False), "float": r.float(), "string": r.string()}
        e["audio"] = r.string()
        if e["audio"] is not None:
            e["volume"], e["balance"] = r.floats(2)
        events.append(e)
    sk["events"] = events

    animations = []
    for _ in range(r.varint()):
        animations.append(read_animation(r, r.string(), sk))
    sk["animations"] = animations
    if r.p != len(data):
        raise SpineError(f"{len(data) - r.p} bytes left after the skeleton")
    return sk


def read_skin(r, default, nonessential):
    if default:
        slot_count = r.varint()
        if slot_count == 0:
            return None
        skin = {"name": "default"}
    else:
        skin = {"name": r.string()}
        if nonessential:
            r.int()  # colour
        skin["bones"] = [r.varint() for _ in range(r.varint())]
        skin["ik"] = [r.varint() for _ in range(r.varint())]
        skin["transform"] = [r.varint() for _ in range(r.varint())]
        skin["path"] = [r.varint() for _ in range(r.varint())]
        skin["physics"] = [r.varint() for _ in range(r.varint())]
        slot_count = r.varint()
    attachments = []
    for _ in range(slot_count):
        slot = r.varint()
        for _ in range(r.varint()):
            name = r.string_ref()
            att = read_attachment(r, name, nonessential)
            att["slot"] = slot
            att["key"] = name
            attachments.append(att)
    skin["attachments"] = attachments
    return skin


def read_sequence(r):
    return {"count": r.varint(), "start": r.varint(), "digits": r.varint(), "setup": r.varint()}


def read_vertices(r, weighted):
    n = r.varint()
    if not weighted:
        return n, {"vertices": r.floats(n * 2)}
    out = []
    for _ in range(n):
        k = r.varint()
        out.append(k)
        for _ in range(k):
            out.append(r.varint())
            out.extend(r.floats(3))  # x, y, weight
    return n, {"bones": out}


def read_attachment(r, key, nonessential):
    f = r.byte()
    name = r.string_ref() if f & 8 else key
    kind = ATTACHMENT_TYPES[f & 7]
    a = {"type": kind, "name": name}
    if kind == "region":
        a["path"] = r.string_ref() if f & 16 else name
        a["color"] = r.color() if f & 32 else [1, 1, 1, 1]
        if f & 64:
            a["sequence"] = read_sequence(r)
        a["rotation"] = r.float() if f & 128 else 0.0
        a["x"], a["y"], a["scale_x"], a["scale_y"], a["width"], a["height"] = r.floats(6)
    elif kind == "boundingbox":
        a["vertex_count"], v = read_vertices(r, bool(f & 16))
        a.update(v)
        if nonessential:
            r.int()
    elif kind == "mesh":
        a["path"] = r.string_ref() if f & 16 else name
        a["color"] = r.color() if f & 32 else [1, 1, 1, 1]
        if f & 64:
            a["sequence"] = read_sequence(r)
        a["hull"] = r.varint()
        a["vertex_count"], v = read_vertices(r, bool(f & 128))
        a.update(v)
        n = a["vertex_count"]
        a["uvs"] = r.floats(n * 2)
        a["triangles"] = [r.varint() for _ in range((2 * n - a["hull"] - 2) * 3)]
        if nonessential:
            edges = r.varint()
            for _ in range(edges):
                r.varint()
            a["width"], a["height"] = r.floats(2)
    elif kind == "linkedmesh":
        a["path"] = r.string_ref() if f & 16 else name
        a["color"] = r.color() if f & 32 else [1, 1, 1, 1]
        if f & 64:
            a["sequence"] = read_sequence(r)
        a["inherit_timelines"] = bool(f & 128)
        a["skin"] = r.varint()
        a["parent"] = r.string_ref()
        if nonessential:
            a["width"], a["height"] = r.floats(2)
    elif kind == "path":
        a["closed"] = bool(f & 16)
        a["constant_speed"] = bool(f & 32)
        a["vertex_count"], v = read_vertices(r, bool(f & 64))
        a.update(v)
        a["lengths"] = r.floats(a["vertex_count"] // 3)
        if nonessential:
            r.int()
    elif kind == "point":
        a["rotation"], a["x"], a["y"] = r.floats(3)
        if nonessential:
            r.int()
    elif kind == "clipping":
        a["end_slot"] = r.varint()
        a["vertex_count"], v = read_vertices(r, bool(f & 16))
        a.update(v)
        if nonessential:
            r.int()
    return a


def read_curve(r, fr, values):
    """The curve from frame fr to the next: 0 linear, 1 stepped, 2 bezier
    (4 floats per value: cx1, cy1, cx2, cy2)."""
    c = r.byte()
    if c == CURVE_STEPPED:
        fr["curve"] = "stepped"
    elif c == CURVE_BEZIER:
        fr["curve"] = r.floats(4 * values)
    elif c != CURVE_LINEAR:
        raise SpineError(f"curve type {c} at {r.p}")


def read_curves(r, frames, values, read_values=None):
    """Frames of (time, values...); the curve of frame i is stored after the
    time and values of frame i + 1."""
    read_values = read_values or (lambda: r.floats(values))
    out = []
    for i in range(frames):
        fr = {"time": r.float(), "v": read_values()}
        if i > 0:
            read_curve(r, out[-1], values)
        out.append(fr)
    return out


def read_animation(r, name, sk):
    anim = {"name": name, "slots": [], "bones": [], "ik": [], "transform": [], "path": [], "physics": [],
            "attachments": [], "draw_order": [], "events": []}
    r.varint()  # timeline count (a capacity hint)
    # slots
    for _ in range(r.varint()):
        slot = r.varint()
        for _ in range(r.varint()):
            t = r.byte()
            frames = r.varint()
            tl = {"slot": slot}
            if t == SLOT_ATTACHMENT:
                tl["type"] = "attachment"
                tl["frames"] = [{"time": r.float(), "name": r.string_ref()} for _ in range(frames)]
            else:
                r.varint()  # bezier count
                if t == SLOT_RGBA:
                    tl["type"], ch = "rgba", 4
                elif t == SLOT_RGB:
                    tl["type"], ch = "rgb", 3
                elif t == SLOT_RGBA2:
                    tl["type"], ch = "rgba2", 7
                elif t == SLOT_RGB2:
                    tl["type"], ch = "rgb2", 6
                elif t == SLOT_ALPHA:
                    tl["type"], ch = "alpha", 1
                else:
                    raise SpineError(f"slot timeline type {t} at {r.p}")
                tl["frames"] = read_curves(r, frames, ch, lambda: [r.byte() / 255.0 for _ in range(ch)])
            anim["slots"].append(tl)
    # bones
    for _ in range(r.varint()):
        bone = r.varint()
        for _ in range(r.varint()):
            t = r.byte()
            frames = r.varint()
            if t >= len(BONE_NAMES):
                raise SpineError(f"bone timeline type {t} at {r.p}")
            tl = {"bone": bone, "type": BONE_NAMES[t]}
            if BONE_NAMES[t] == "inherit":
                tl["frames"] = [{"time": r.float(), "inherit": INHERIT[r.byte()]} for _ in range(frames)]
            else:
                r.varint()  # bezier count
                values = 2 if BONE_NAMES[t] in ("translate", "scale", "shear") else 1
                tl["frames"] = read_curves(r, frames, values)
            anim["bones"].append(tl)
    # ik
    for _ in range(r.varint()):
        index = r.varint()
        frames = r.varint()
        r.varint()  # bezier count
        out = []
        for i in range(frames):
            f = r.byte()
            fr = {"time": r.float()}
            fr["mix"] = (r.float() if f & 2 else 1.0) if f & 1 else 0.0
            fr["softness"] = r.float() if f & 4 else 0.0
            fr["bend"] = 1 if f & 8 else -1
            fr["compress"] = bool(f & 16)
            fr["stretch"] = bool(f & 32)
            if i > 0:  # the previous frame's curve rides on this frame's flags
                if f & 64:
                    out[-1]["curve"] = "stepped"
                elif f & 128:
                    out[-1]["curve"] = r.floats(8)
            out.append(fr)
        anim["ik"].append({"constraint": index, "frames": out})
    # transform
    for _ in range(r.varint()):
        index = r.varint()
        frames = r.varint()
        r.varint()
        anim["transform"].append({"constraint": index, "frames": read_curves(r, frames, 6)})
    # path
    for _ in range(r.varint()):
        index = r.varint()
        for _ in range(r.varint()):
            t = r.byte()
            frames = r.varint()
            r.varint()
            values = 3 if t == PATH_MIX else 1
            anim["path"].append({"constraint": index, "type": ["position", "spacing", "mix"][t],
                                 "frames": read_curves(r, frames, values)})
    # physics
    for _ in range(r.varint()):
        index = r.varint() - 1  # -1: all constraints
        for _ in range(r.varint()):
            t = r.byte()
            frames = r.varint()
            if t not in PHYSICS_NAMES:
                raise SpineError(f"physics timeline type {t} at {r.p}")
            tl = {"constraint": index, "type": PHYSICS_NAMES[t]}
            if PHYSICS_NAMES[t] == "reset":
                tl["frames"] = [{"time": r.float()} for _ in range(frames)]
            else:
                r.varint()
                tl["frames"] = read_curves(r, frames, 1)
            anim["physics"].append(tl)
    # attachments (deform, sequence)
    for _ in range(r.varint()):
        skin = r.varint()
        for _ in range(r.varint()):
            slot = r.varint()
            for _ in range(r.varint()):
                att = r.string_ref()
                t = r.byte()
                frames = r.varint()
                tl = {"skin": skin, "slot": slot, "attachment": att}
                if t == ATTACH_DEFORM:
                    tl["type"] = "deform"
                    r.varint()  # bezier count
                    out = []
                    time = r.float()
                    for i in range(frames):
                        fr = {"time": time, "start": 0, "offsets": []}
                        end = r.varint()
                        if end:
                            fr["start"] = r.varint()
                            fr["offsets"] = r.floats(end)
                        out.append(fr)
                        if i < frames - 1:
                            time = r.float()
                            read_curve(r, fr, 1)
                    tl["frames"] = out
                elif t == ATTACH_SEQUENCE:
                    tl["type"] = "sequence"
                    out = []
                    for _ in range(frames):
                        time = r.float()
                        mode_index = r.int()
                        out.append({"time": time, "mode": mode_index & 0xF, "index": mode_index >> 4,
                                    "delay": r.float()})
                    tl["frames"] = out
                else:
                    raise SpineError(f"attachment timeline type {t} at {r.p}")
                anim["attachments"].append(tl)
    # draw order
    for _ in range(r.varint()):
        fr = {"time": r.float(), "offsets": []}
        for _ in range(r.varint()):
            fr["offsets"].append([r.varint(), r.varint()])  # slot, offset
        anim["draw_order"].append(fr)
    # events
    for _ in range(r.varint()):
        fr = {"time": r.float(), "event": r.varint()}
        ev = sk["events"][fr["event"]]
        fr["int"] = r.varint(False)
        fr["float"] = r.float()
        fr["string"] = r.string() if r.boolean() else ev["string"]
        if ev["audio"] is not None:
            fr["volume"], fr["balance"] = r.floats(2)
        anim["events"].append(fr)
    return anim


def read_atlas(text):
    """libGDX / Spine text atlas -> {"pages": [...], "regions": [...]}."""
    pages, regions = [], []
    page = None
    lines = [ln.rstrip("\r") for ln in text.split("\n")]
    i = 0
    while i < len(lines):
        ln = lines[i].strip()
        i += 1
        if not ln:
            page = None
            continue
        if page is None:
            page = {"name": ln, "size": [0, 0], "filter": "Linear", "pma": False, "scale": 1.0}
            pages.append(page)
            while i < len(lines) and ":" in lines[i]:
                k, v = (x.strip() for x in lines[i].split(":", 1))
                i += 1
                if k == "size":
                    page["size"] = [int(x) for x in v.split(",")]
                elif k == "filter":
                    page["filter"] = v
                elif k == "pma":
                    page["pma"] = v == "true"
                elif k == "scale":
                    page["scale"] = float(v)
            continue
        reg = {"name": ln, "page": len(pages) - 1, "rotate": 0, "index": -1}
        while i < len(lines) and ":" in lines[i]:
            k, v = (x.strip() for x in lines[i].split(":", 1))
            i += 1
            nums = [x.strip() for x in v.split(",")]
            if k == "bounds":
                reg["x"], reg["y"], reg["w"], reg["h"] = (int(x) for x in nums)
            elif k == "offsets":
                reg["ox"], reg["oy"], reg["ow"], reg["oh"] = (int(x) for x in nums)
            elif k == "rotate":
                reg["rotate"] = 90 if v == "true" else (0 if v == "false" else int(v))
            elif k == "index":
                reg["index"] = int(v)
        if "ow" not in reg:
            reg["ox"], reg["oy"], reg["ow"], reg["oh"] = 0, 0, reg.get("w", 0), reg.get("h", 0)
        regions.append(reg)
    return {"pages": pages, "regions": regions}


def read_spatlas(data):
    """Godot's imported .spatlas: JSON with the atlas text."""
    j = json.loads(data.decode("utf-8"))
    return read_atlas(j["atlas_data"])


# ---- engine form ----
#
# What the engine's runtime (engine/spine) reads: the skeleton as read above,
# with linked meshes resolved, every timeline in one flat shape and the
# sparse data expanded (deform frames as full vertex arrays, draw-order
# frames as whole orders). See docs/PACK_FORMAT.md, "Spine".

def _find_attachment(sk, skin_index, slot, key):
    skins = sk["skins"]
    if not 0 <= skin_index < len(skins):
        return None
    for a in skins[skin_index]["attachments"]:
        if a["slot"] == slot and a["key"] == key:
            return a
    return None


def _resolve_linked_meshes(sk):
    for skin in sk["skins"]:
        for a in skin["attachments"]:
            if a["type"] != "linkedmesh":
                continue
            parent = _find_attachment(sk, a["skin"], a["slot"], a["parent"])
            if parent is None or parent["type"] != "mesh":
                raise SpineError(f"linked mesh {a['name']}: no parent mesh {a['parent']}")
            for k in ("hull", "vertex_count", "vertices", "bones", "uvs", "triangles"):
                if k in parent:
                    a[k] = parent[k]
            a["type"] = "mesh"
            # Deform timelines keyed on the parent apply to this mesh too.
            a["timeline_key"] = parent["key"] if a.pop("inherit_timelines") else a["key"]
            del a["skin"], a["parent"]


def _deform_length(att):
    if "bones" in att:
        n, i, b = 0, 0, att["bones"]
        while i < len(b):
            k = b[i]
            n += k
            i += 1 + 4 * k
        return 2 * n
    return 2 * att["vertex_count"]


def _flat(frames, value_key="v"):
    out, curves = [], []
    for fr in frames:
        out.append(fr["time"])
        out.extend(fr[value_key])
    for fr in frames[:-1]:
        c = fr.get("curve")
        curves.append(1 if c == "stepped" else (c if c else 0))
    return out, curves


def _draw_order(n, offsets):
    if offsets is None:
        return None
    order = [-1] * n
    unchanged = []
    original = 0
    for slot, offset in offsets:
        while original != slot:
            unchanged.append(original)
            original += 1
        order[original + offset] = original
        original += 1
    while original < n:
        unchanged.append(original)
        original += 1
    for i in range(n - 1, -1, -1):
        if order[i] == -1:
            order[i] = unchanged.pop()
    return order


def engine_skeleton(sk):
    _resolve_linked_meshes(sk)
    out = {k: sk[k] for k in ("version", "x", "y", "width", "height", "reference_scale", "ik", "transform",
                              "path", "physics", "events")}
    out["kind"] = "spine_skeleton"
    out["bones"] = [dict(b, inherit=INHERIT.index(b["inherit"])) for b in sk["bones"]]
    out["slots"] = [dict(s, blend=BLEND.index(s["blend"])) for s in sk["slots"]]
    skins = []
    for s in sk["skins"]:
        atts = []
        for a in s["attachments"]:
            a = dict(a)
            a.setdefault("timeline_key", a["key"])
            atts.append(a)
        skins.append(dict(s, attachments=atts))
    out["skins"] = skins
    nslots = len(sk["slots"])
    anims = []
    for an in sk["animations"]:
        tls = []
        duration = 0.0

        def last(times):
            nonlocal duration
            if times:
                duration = max(duration, times[-1])

        for t in an["slots"]:
            if t["type"] == "attachment":
                times = [f["time"] for f in t["frames"]]
                tls.append({"type": "attachment", "slot": t["slot"], "times": times,
                            "names": [f["name"] for f in t["frames"]]})
            else:
                flat, curves = _flat(t["frames"])
                times = [f["time"] for f in t["frames"]]
                tls.append({"type": t["type"], "slot": t["slot"], "frames": flat, "curves": curves})
            last(times)
        for t in an["bones"]:
            times = [f["time"] for f in t["frames"]]
            if t["type"] == "inherit":
                tls.append({"type": "inherit", "bone": t["bone"], "times": times,
                            "modes": [INHERIT.index(f["inherit"]) for f in t["frames"]]})
            else:
                flat, curves = _flat(t["frames"])
                tls.append({"type": t["type"], "bone": t["bone"], "frames": flat, "curves": curves})
            last(times)
        for t in an["attachments"]:
            att = _find_attachment(sk, t["skin"], t["slot"], t["attachment"])
            times = [f["time"] for f in t["frames"]]
            last(times)
            if att is None:
                continue
            if t["type"] == "deform":
                n = _deform_length(att)
                setup = att.get("vertices") if "bones" not in att else None
                verts = []
                for f in t["frames"]:
                    v = list(setup) if setup else [0.0] * n
                    for i, d in enumerate(f["offsets"]):
                        v[f["start"] + i] += d
                    verts.append(v)
                curves = [1 if f.get("curve") == "stepped" else (f.get("curve") or 0) for f in t["frames"][:-1]]
                tls.append({"type": "deform", "skin": t["skin"], "slot": t["slot"], "attachment": t["attachment"],
                            "times": times, "vertices": verts, "curves": curves})
            else:
                flat = []
                for f in t["frames"]:
                    flat += [f["time"], f["mode"], f["index"], f["delay"]]
                tls.append({"type": "sequence", "skin": t["skin"], "slot": t["slot"],
                            "attachment": t["attachment"], "frames": flat})
        if an["draw_order"]:
            times = [f["time"] for f in an["draw_order"]]
            last(times)
            tls.append({"type": "draw_order", "times": times,
                        "orders": [_draw_order(nslots, f["offsets"] if f["offsets"] else None)
                                   for f in an["draw_order"]]})
        if an["events"]:
            times = [f["time"] for f in an["events"]]
            last(times)
            tls.append({"type": "event", "times": times, "events": an["events"]})
        for t in an["ik"]:
            last([f["time"] for f in t["frames"]])
            tls.append(dict(t, type="ik"))
        for t in an["transform"]:
            last([f["time"] for f in t["frames"]])
            flat, curves = _flat(t["frames"])
            tls.append({"type": "transform", "constraint": t["constraint"], "frames": flat, "curves": curves})
        for t in an["path"]:
            last([f["time"] for f in t["frames"]])
            flat, curves = _flat(t["frames"])
            tls.append({"type": "path_" + t["type"], "constraint": t["constraint"], "frames": flat,
                        "curves": curves})
        for t in an["physics"]:
            last([f["time"] for f in t["frames"]])
            if t["type"] == "reset":
                tls.append({"type": "physics_reset", "constraint": t["constraint"],
                            "times": [f["time"] for f in t["frames"]]})
            else:
                flat, curves = _flat(t["frames"])
                tls.append({"type": "physics_" + t["type"], "constraint": t["constraint"], "frames": flat,
                            "curves": curves})
        anims.append({"name": an["name"], "duration": duration, "timelines": tls})
    out["animations"] = anims
    return out


def engine_atlas(atlas, base_dir):
    """Pages name their texture by pack path (next to the atlas, as the
    Spine importer resolves them)."""
    pages = []
    for p in atlas["pages"]:
        pages.append(dict(p, texture=(base_dir + "/" if base_dir else "") + p["name"]))
    return {"kind": "spine_atlas", "pages": pages, "regions": atlas["regions"]}


def page_textures(spatlas_data, atlas_path):
    """Pack paths of an imported atlas's page images (for the bundle)."""
    base = atlas_path.rsplit("/", 1)[0] if "/" in atlas_path else ""
    return [p["texture"] for p in engine_atlas(read_spatlas(spatlas_data), base)["pages"]]
