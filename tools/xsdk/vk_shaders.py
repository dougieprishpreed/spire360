#!/usr/bin/env python3
"""Writes the Vulkan renderer's shaders (engine/gfx/vk_renderer.cpp) as GLSL
files for vulkan4xenon's tools/build_shaders.py, into a folder of the build:

    vk_shaders.py <folder>

- engine/gfx/shaders/vk's .vert and .frag files, common.glsl put in where
  they include it, followed by the samplers each one names;
- screen.frag once per mode it holds (screen_<mode>.frag, copy.frag and
  present.frag), each with its own mode's code alone;
- each of effects.hlsl's ps_e<id> as effect_<id>.frag: its HLSL as written
  under common.glsl's names, with the helpers it calls before it (the D3D9
  and software renderers draw the same shaders);
- vk_shader_table.h: the SPIR-V of each file by its name, which
  build_shaders.py's output (<folder>/spire_shaders.h) declares.

A file is written only when its text changes, so the build compiles again
only what changed. Prints the shader files' names."""

import os
import re
import sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
VK = os.path.join(ROOT, "engine", "gfx", "shaders", "vk")
EFFECTS = os.path.join(ROOT, "engine", "gfx", "shaders", "effects.hlsl")

# Each sampler's set and binding (vk_renderer.cpp: set 0 the draw's
# texture, set 2 the others; the fire's and the effects' share theirs).
SAMPLERS = {
    "g_tex": (0, 0), "g_group": (2, 0), "g_lut": (2, 1),
    "g_fn1": (2, 2), "g_fn2": (2, 3), "g_fmask": (2, 4), "g_fd1": (2, 5), "g_fd2": (2, 6), "g_fdmask": (2, 7),
    "g_e0": (2, 2), "g_e1": (2, 3), "g_e2": (2, 4), "g_e3": (2, 5), "g_e4": (2, 6),
    "g_scrA": (2, 8), "g_screen": (2, 8), "g_scrB": (2, 9), "g_scrNoise": (2, 10), "g_scrMask": (2, 11),
}

# screen.frag's modes and the files they make
SCREEN_MODES = {"AT_UV": "screen_at_uv", "CHROMA": "screen_chroma", "DISTORT": "screen_distort",
                "DOOM": "screen_doom", "GLASS": "screen_glass", "RADIAL": "screen_radial", "LEVEL": "screen_level",
                "DARK": "screen_dark", "NOISE": "screen_noise", "COPY": "copy", "PRESENT": "present"}


def read(path):
    with open(path, "r", encoding="utf-8") as f:
        return f.read().replace("\r\n", "\n")


def write_if_changed(path, text):
    try:
        if read(path) == text:
            return
    except OSError:
        pass
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)


def strip_comments(code):
    return re.sub(r"[ \t]*//[^\n]*", "", code)


def ternary_bounds(code, q):
    """A ?: at q: where its condition starts, its : and where it ends."""
    depth, i = 0, q - 1
    while i >= 0:
        ch = code[i]
        if ch in ")]":
            depth += 1
        elif ch in "([":
            if depth == 0:
                break
            depth -= 1
        elif depth == 0 and (ch in ",;{}?:" or (ch == "=" and code[i - 1] not in "=!<>" and code[i + 1] != "=")):
            break
        i -= 1
    start = i + 1
    ret = re.match(r"\s*return\b", code[start:q])
    if ret:
        start += ret.end()
    depth, i, colon = 0, q + 1, None
    while i < len(code):
        ch = code[i]
        if ch in "([":
            depth += 1
        elif ch in ")]":
            if depth == 0:
                break
            depth -= 1
        elif depth == 0 and ch == ":" and colon is None:
            colon = i
        elif depth == 0 and ch in ",;" and colon is not None:
            break
        i += 1
    if colon is None:
        sys.exit("vk_shaders.py: a ?: without its :")
    return start, colon, i


def selects(code):
    """Every cond ? a : b as spire_sel(cond, a, b), the rightmost first, so
    those inside others go before them."""
    while "?" in code:
        q = code.rindex("?")
        start, colon, end = ternary_bounds(code, q)
        cond, a, b = code[start:q].strip(), code[q + 1:colon].strip(), code[colon + 1:end].strip()
        lead = code[start:q][:len(code[start:q]) - len(code[start:q].lstrip())]
        code = code[:start] + lead + "spire_sel(%s, %s, %s)" % (cond, a, b) + code[end:]
    return code


def with_prelude(text, common):
    """common.glsl where the shader includes it, and the samplers it names after it."""
    text = selects(strip_comments(text))
    names = sorted(set(re.findall(r"\bg_\w+\b", text)) & set(SAMPLERS), key=lambda n: SAMPLERS[n])
    decls = "".join("layout(set = %d, binding = %d) uniform sampler2D %s;\n" % (SAMPLERS[n] + (n,)) for n in names)
    include = '#include "common.glsl"\n'
    if include not in text:
        sys.exit("vk_shaders.py: a shader that doesn't include common.glsl")
    return text.replace(include, common + "\n" + decls, 1)


def screen_modes(text):
    """screen.frag split by its #if defined(MODE_*) arms: {mode: its text}."""
    head, _, rest = text.partition("#if defined(MODE_")
    body, _, tail = rest.rpartition("#endif\n")
    arms = re.split(r"#(?:el)?if defined\(MODE_", "#if defined(MODE_" + body)
    modes = {}
    for arm in arms[1:]:
        name, _, code = arm.partition(")\n")
        modes[name] = head + code + tail
    return modes


def functions(text):
    """effects.hlsl's top-level functions in order: (name, text)."""
    out = []
    for m in re.finditer(r"^(float[234]?|void)\s+(\w+)\s*\(([^)]*)\)\s*(?::\s*COLOR)?\s*\{", text, re.M):
        depth, i = 0, m.end() - 1
        while True:
            if text[i] == "{":
                depth += 1
            elif text[i] == "}":
                depth -= 1
                if depth == 0:
                    break
            i += 1
        # with the comment just above it
        start = m.start()
        lines = text[:start].split("\n")[:-1]
        k = len(lines)
        while k > 0 and lines[k - 1].startswith("//"):
            k -= 1
        comment = "\n".join(lines[k:]) + "\n" if k < len(lines) else ""
        out.append((m.group(2), comment + text[start:i + 1] + "\n"))
    return out


def hlsl_to_glsl(code):
    code = re.sub(r"\bi\.(uv|color|screen)\b", r"v_\1", code)
    # glslang makes != of floats an unordered compare, and of ints INotEqual,
    # neither of which xost takes
    code = re.sub(r"(\b[\w.]+(?:\[[^\]]*\])?(?:\.\w+)?)\s*!=\s*([0-9]*\.?[0-9]+)\b", r"!(\1 == \2)", code)
    code = re.sub(r"\bg_x\[", "P.c[29 + ", code)
    code = re.sub(r"\[(?:unroll|loop|branch|flatten)\]\s*", "", code)
    # array initialisers, and vectors set from a scalar
    code = re.sub(r"\bfloat([234]?)\s+(\w+)\[(\d+)\]\s*=\s*\{([^}]*)\}",
                  lambda m: "float%s %s[%s] = float%s[%s](%s)" % (m.group(1), m.group(2), m.group(3), m.group(1),
                                                                  m.group(3), m.group(4).strip()), code)
    code = re.sub(r"\b(float[234])\s+(\w+)\s*=\s*([0-9.]+)\s*;", r"\1 \2 = \1(\3);", code)
    return code


def effect_shaders(common):
    funcs = functions(read(EFFECTS))
    helpers = [(n, t) for n, t in funcs if not re.match(r"ps_e\d+$", n)]
    out = {}
    for name, text in funcs:
        m = re.match(r"ps_e(\d+)$", name)
        if not m:
            continue
        # the helpers it calls, and theirs, in their order in the file
        wanted, todo = set(), [text]
        while todo:
            t = todo.pop()
            for hn, ht in helpers:
                if hn not in wanted and re.search(r"\b%s\s*\(" % hn, t):
                    wanted.add(hn)
                    todo.append(ht)
        body = text
        if len(re.findall(r"\breturn\b", body)) != 1:
            sys.exit("vk_shaders.py: %s returns more than once" % name)
        body = re.sub(r"^float4\s+ps_e\d+\s*\(\s*VsOut\s+i\s*\)\s*:\s*COLOR\s*\{", "void main() {", body, flags=re.M)
        body = re.sub(r"\breturn\s+([^;]*);", r"o_color = \1;", body)
        src = "".join(ht for hn, ht in helpers if hn in wanted) + body
        src = hlsl_to_glsl(src)
        text = ("#version 450\n// Generated by tools/xsdk/vk_shaders.py from effects.hlsl's %s; edit it there.\n"
                '#include "common.glsl"\n\n%s' % (name, src))
        out["effect_%s.frag" % m.group(1)] = with_prelude(text, common)
    return out


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    folder = sys.argv[1]
    os.makedirs(folder, exist_ok=True)
    common = read(os.path.join(VK, "common.glsl"))
    files = {}
    for name in sorted(os.listdir(VK)):
        text = read(os.path.join(VK, name))
        if name == "screen.frag":
            for mode, code in sorted(screen_modes(text).items()):
                files[SCREEN_MODES[mode] + ".frag"] = with_prelude(code, common)
        elif name.endswith(".frag"):
            files[name] = with_prelude(text, common)
        elif name.endswith(".vert"):
            files[name] = text
    files.update(effect_shaders(common))
    for name, text in files.items():
        write_if_changed(os.path.join(folder, name), text)
    entries = []
    for name in sorted(files):
        c = name.replace(".", "_")
        entries.append('    { "%s", %s_spirv, %s_SPIRV_BYTES },' % (name, c, c.upper()))
    table = ("/* Generated by tools/xsdk/vk_shaders.py: each shader's SPIR-V by its file's name. */\n"
             "#ifndef SPIRE_VK_SHADER_TABLE_H\n#define SPIRE_VK_SHADER_TABLE_H\n\n"
             "#ifdef __cplusplus\nextern \"C\" {\n#endif\n#include \"spire_shaders.h\"\n#ifdef __cplusplus\n}\n#endif\n\n"
             "struct SpireVkShader {\n    const char* name;\n    const uint32_t* code;\n    unsigned bytes;\n};\n\n"
             "static const SpireVkShader SPIRE_VK_SHADERS[] = {\n%s\n};\n\n#endif\n" % "\n".join(entries))
    write_if_changed(os.path.join(folder, "vk_shader_table.h"), table)
    print(" ".join(sorted(files)))


if __name__ == "__main__":
    main()
