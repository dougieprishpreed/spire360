"""Godot 4 text resources: .tscn, .tres, .import, .remap, project files.

parse_text(text) -> TextResource with
    header          (tag, {attrs})   e.g. ("gd_scene", {"format": 3, ...})
    ext_resources   {id: {"type", "path", "uid", ...}}
    sub_resources   {id: {"type": ..., "props": {...}}}
    nodes           [{"name", "type", "parent", "instance", ..., "props": {...}}]
    connections     [{"signal", "from", "to", "method", ...}]
    resource        {props} of the [resource] section (.tres)
    sections        every section in order: (tag, {attrs}, {props})

Values follow Godot's VariantParser: int, float (inf, -inf, nan), bool,
null -> None, "strings" -> str, &"names" -> StringName, NodePath("a/b"),
[arrays], {dicts}, Array[T]([...]), Dictionary[K, V]({...}),
ExtResource("id") / SubResource("id") -> Ref, other constructors such as
Vector2(1, 2), Rect2(...), Color(...), PackedFloat32Array(...) -> Ctor.
"""
import math


class StringName(str):
    """&"name" in Godot text; behaves as a str."""


class NodePath(str):
    """NodePath("a/b")."""


class Ref:
    """ExtResource("id") or SubResource("id")."""
    __slots__ = ("kind", "id")

    def __init__(self, kind, id_):
        self.kind, self.id = kind, id_

    def __eq__(self, other):
        return isinstance(other, Ref) and (self.kind, self.id) == (other.kind, other.id)

    def __hash__(self):
        return hash((self.kind, self.id))

    def __repr__(self):
        return f'{self.kind}("{self.id}")'


class Ctor:
    """Any other constructor: Vector2(1, 2) -> Ctor("Vector2", [1, 2])."""
    __slots__ = ("name", "args")

    def __init__(self, name, args):
        self.name, self.args = name, args

    def __eq__(self, other):
        return isinstance(other, Ctor) and (self.name, self.args) == (other.name, other.args)

    def __repr__(self):
        return f"{self.name}({', '.join(map(repr, self.args))})"


class ObjectValue:
    """Object(Type, "prop": value, ...)."""
    __slots__ = ("type", "props")

    def __init__(self, type_, props):
        self.type, self.props = type_, props

    def __repr__(self):
        return f"Object({self.type}, {self.props!r})"


class ParseError(ValueError):
    pass


_IDENT_START = set("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_")
_IDENT = _IDENT_START | set("0123456789")
_NUM = set("0123456789+-.eE")
_KEYWORDS = {"true": True, "false": False, "null": None, "nil": None,
             "inf": math.inf, "inf_neg": -math.inf, "nan": math.nan}


class _Parser:
    def __init__(self, text):
        self.s = text
        self.i = 0
        self.n = len(text)

    def error(self, msg):
        line = self.s.count("\n", 0, self.i) + 1
        raise ParseError(f"line {line}: {msg}")

    def ws(self, newlines=True):
        s, n = self.s, self.n
        while self.i < n:
            c = s[self.i]
            if c == ";" or (c == "#" and newlines):
                while self.i < n and s[self.i] != "\n":
                    self.i += 1
            elif c in " \t\r\0\ufeff" or (newlines and c == "\n"):
                self.i += 1
            else:
                break

    def peek(self):
        self.ws()
        return self.s[self.i] if self.i < self.n else ""

    def expect(self, ch):
        if self.peek() != ch:
            self.error(f"expected {ch!r}, got {self.s[self.i:self.i + 20]!r}")
        self.i += 1

    def ident(self):
        self.ws()
        start = self.i
        if self.i < self.n and self.s[self.i] in _IDENT_START:
            self.i += 1
            while self.i < self.n and self.s[self.i] in _IDENT:
                self.i += 1
        if start == self.i:
            self.error(f"expected an identifier at {self.s[self.i:self.i + 20]!r}")
        return self.s[start:self.i]

    def string(self):
        # At the opening quote.
        s = self.s
        self.i += 1
        out = []
        while True:
            if self.i >= self.n:
                self.error("unterminated string")
            c = s[self.i]
            if c == '"':
                self.i += 1
                return "".join(out)
            if c == "\\":
                self.i += 1
                e = s[self.i]
                if e == "u" or e == "U":
                    width = 4 if e == "u" else 6
                    out.append(chr(int(s[self.i + 1:self.i + 1 + width], 16)))
                    self.i += 1 + width
                    continue
                out.append({"n": "\n", "t": "\t", "r": "\r", "b": "\b", "f": "\f", "a": "\a", "v": "\v",
                            "0": "\0"}.get(e, e))
                self.i += 1
                continue
            out.append(c)
            self.i += 1

    def number(self):
        start = self.i
        while self.i < self.n and self.s[self.i] in _NUM:
            self.i += 1
        txt = self.s[start:self.i]
        try:
            if any(ch in txt for ch in ".eE") and not txt.lstrip("+-").isdigit():
                return float(txt)
            return int(txt)
        except ValueError:
            self.error(f"bad number {txt!r}")

    def value(self):
        c = self.peek()
        if c == '"':
            return self.string()
        if c == "&" and self.s.startswith('&"', self.i):
            self.i += 1
            return StringName(self.string())
        if c == "^" and self.s.startswith('^"', self.i):
            self.i += 1
            return NodePath(self.string())
        if c == "[":
            return self.array()
        if c == "{":
            return self.dict()
        if c in "0123456789+-.":
            if self.s.startswith("-inf", self.i):
                self.i += 4
                return -math.inf
            return self.number()
        if c in _IDENT_START:
            name = self.ident()
            if name in _KEYWORDS and self.peek() != "(":
                return _KEYWORDS[name]
            return self.construct(name)
        self.error(f"unexpected {self.s[self.i:self.i + 20]!r}")

    def array(self):
        self.expect("[")
        out = []
        while self.peek() != "]":
            out.append(self.value())
            if self.peek() == ",":
                self.i += 1
        self.i += 1
        return out

    def dict(self):
        self.expect("{")
        out = {}
        while self.peek() != "}":
            k = self.value()
            self.expect(":")
            v = self.value()
            out[k if _hashable(k) else repr(k)] = v
            if self.peek() == ",":
                self.i += 1
        self.i += 1
        return out

    def construct(self, name):
        if self.peek() == "[":
            # Typed container: Array[Type]([...]) / Dictionary[K, V]({...})
            self.i += 1
            depth = 1
            while depth:
                ch = self.s[self.i]
                depth += ch == "["
                depth -= ch == "]"
                self.i += 1
            self.expect("(")
            v = self.value()
            self.expect(")")
            return v
        self.expect("(")
        if name == "Object":
            type_ = self.ident()
            props = {}
            while self.peek() == ",":
                self.i += 1
                if self.peek() == ")":
                    break
                k = self.value()
                self.expect(":")
                props[k] = self.value()
            self.expect(")")
            return ObjectValue(type_, props)
        args = []
        while self.peek() != ")":
            args.append(self.value())
            if self.peek() == ",":
                self.i += 1
        self.i += 1
        if name in ("ExtResource", "SubResource"):
            return Ref(name, args[0] if args else "")
        if name == "NodePath":
            return NodePath(args[0] if args else "")
        if name == "StringName":
            return StringName(args[0] if args else "")
        return Ctor(name, args)

    # ---- file structure ----

    def section_header(self):
        self.expect("[")
        tag = self.ident()
        attrs = {}
        while self.peek() != "]":
            key = self.ident()
            self.expect("=")
            attrs[key] = self.value()
        self.i += 1
        return tag, attrs

    def prop_key(self):
        # Keys run to the '=' and may contain '/', ':' and digits
        # ("theme_override_colors/font_color", "0:0/0", quoted keys).
        self.ws()
        if self.i < self.n and self.s[self.i] == '"':
            return self.string()
        start = self.i
        while self.i < self.n and self.s[self.i] not in "=\n":
            self.i += 1
        return self.s[start:self.i].strip()


def _hashable(v):
    try:
        hash(v)
        return True
    except TypeError:
        return False


class TextResource:
    def __init__(self):
        self.header = (None, {})
        self.ext_resources = {}
        self.sub_resources = {}
        self.nodes = []
        self.connections = []
        self.resource = {}
        self.sections = []

    @property
    def is_scene(self):
        return self.header[0] == "gd_scene"


def parse_text(text):
    """Parses a whole .tscn/.tres/.import/.remap file."""
    p = _Parser(text)
    res = TextResource()
    props = None
    while True:
        c = p.peek()
        if not c:
            break
        if c == "[":
            tag, attrs = p.section_header()
            props = {}
            res.sections.append((tag, attrs, props))
            if len(res.sections) == 1 and tag in ("gd_scene", "gd_resource"):
                res.header = (tag, attrs)
            elif tag == "ext_resource":
                res.ext_resources[attrs.get("id")] = dict(attrs)
            elif tag == "sub_resource":
                res.sub_resources[attrs.get("id")] = {"type": attrs.get("type"), "props": props}
            elif tag == "node":
                res.nodes.append(dict(attrs, props=props))
            elif tag == "connection":
                res.connections.append(dict(attrs))
            elif tag == "resource":
                res.resource = props
            continue
        key = p.prop_key()
        p.expect("=")
        value = p.value()
        if props is None:
            props = {}
            res.sections.append((None, {}, props))
        props[key] = value
    return res


def parse_value(text):
    """One value, e.g. 'Vector2(1, 2)'."""
    return _Parser(text).value()
