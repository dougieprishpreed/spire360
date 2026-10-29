#!/usr/bin/env python3
"""The Vulkan renderer's shaders on xost's simulator (xsdk's xenos-toolchain/xdis/simulate.py), run as the console
runs them, to find what lavapipe on the PC can't show:

- every shader built by xost with its optimiser and without, for random pixels (in 2x2 quads, for fwidth) and a
  random parameter block: the two must agree (the optimiser has dropped a second length() before);
- canvas.vert's position against the screen-to-clip mapping, and which export carries each varying;
- plain.frag's colour against the texel times the vertex colour, premultiplied, which settles that the pixel
  shader reads each varying where the vertex shader writes it.

    XSDK=... GLSLANG=... tools/xsdk/check_shaders.py <shaders folder> [--count N]

<shaders folder> is tools/xsdk/shaders.sh's. Exits 1 on a problem."""

import argparse
import math
import os
import random
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

W, H = 1280.0, 720.0
PARAM_FLOATS = 46 * 4


def texture(slot, coords):
    """A smooth pattern of the coordinates, a little different in each slot."""
    u, v = coords[0], coords[1]
    k = 1.0 + 0.1 * slot
    return [0.5 + 0.5 * math.sin(7 * k * u), 0.5 + 0.5 * math.cos(5 * k * v), (3 * u + 2 * v * k) % 1.0, 0.75]


def differ(a, b):
    return max(abs(p - q) if math.isfinite(p) and math.isfinite(q) else float(repr(p) != repr(q)) for p, q in zip(a, b))


class Builder:
    def __init__(self, xsdk, glslang, work):
        sys.path.insert(0, str(Path(xsdk) / "xenos-toolchain"))
        from xasm.program import XSDK_REGISTERS
        from xsc import native
        from xdis import simulate
        self.registers, self.native, self.simulate = XSDK_REGISTERS, native, simulate
        self.glslang, self.work = glslang, work

    def spirv(self, source):
        out = self.work / (source.name + ".spv")
        r = subprocess.run([self.glslang, "-V", str(source), "-o", str(out)], capture_output=True, text=True)
        if r.returncode:
            raise RuntimeError("glslang: " + r.stdout + r.stderr)
        return out.read_bytes()

    def build(self, spv, optimize, block, push):
        """The container, its constants (literals, the parameter block, the push constants) and its loops."""
        container, manifest, resources = self.native.compile_spirv(spv, self.registers.pixel, self.registers.vertex,
                                                                   optimize)
        consts = {m[0]: list(m[1]) for m in manifest if isinstance(m, tuple) and isinstance(m[0], int)}
        loops = {}
        for m in manifest:
            if isinstance(m, tuple) and m and m[0] == "block_loop":
                n = struct.unpack_from("<i", block, m[5])[0]
                loops[m[1]] = self.native.loop_passes(m[6], m[7], n) if m[7] else max(0, n)
            elif isinstance(m, tuple) and m and m[0] == "fixed_loop":
                loops[m[1]] = m[2]
        for kind, _set, _binding, reg, regs in resources:
            data = block if kind == "uniform" else push if kind == "push" else None
            if data is None:
                continue
            for k in range(regs):
                consts[reg + k] = list(struct.unpack_from("<4f", data, 16 * k)) if 16 * k + 16 <= len(data) else [0.0] * 4
        consts[self.native.FRAG_COORD_CONSTANT] = [0.5, 0.5, 0.0, 0.0]
        return container, consts, loops


def random_block(rng, count):
    block = bytearray(PARAM_FLOATS * 4 + 16)
    for i in range(PARAM_FLOATS):
        struct.pack_into("<f", block, 4 * i, rng.uniform(0.0, 1.0))
    struct.pack_into("<i", block, PARAM_FLOATS * 4, count)
    return block


def quad(rng):
    """A 2x2 quad's interpolators: v_color (r0), v_uv (r1), v_screen (r2), a pixel apart."""
    x, y = rng.uniform(0, W - 2), rng.uniform(0, H - 2)
    color = [rng.uniform(0.3, 1) for _ in range(4)]
    du, dv = rng.uniform(0.0005, 0.004), rng.uniform(0.0005, 0.004)
    u0, v0 = rng.uniform(0, 1), rng.uniform(0, 1)
    lanes = []
    for oy in (0, 1):
        for ox in (0, 1):
            lanes.append({0: list(color), 1: [u0 + ox * du, v0 + oy * dv, 0.0, 0.0],
                          2: [(x + ox) / W, (y + oy) / H, 0.0, 0.0]})
    return lanes


def check_fragment(b, path, rng, count, problems):
    spv = b.spirv(path)
    block = random_block(rng, 5)
    builds = [b.build(spv, opt, block, b"") for opt in (True, False)]
    mismatches, worst, plain_bad = 0, 0.0, 0
    for _ in range(max(1, count // 4)):
        lanes = quad(rng)
        runs = [b.simulate.run_group(c, lanes, constants=k, loops=lp, texture_fetch=texture, quads=True)
                for c, k, lp in builds]
        for i in range(4):
            outs = [r[i].exports.get(0, [0.0] * 4) + [float(r[i].killed)] for r in runs]
            d = differ(*outs)
            worst = max(worst, d)
            mismatches += d > 1.0 / 255
            if path.name == "plain.frag":
                premul = struct.unpack_from("<f", block, 28 * 16)[0] > 0.5
                t = texture(0, lanes[i][1])
                if premul:
                    t = [t[0] / t[3], t[1] / t[3], t[2] / t[3], t[3]]
                c = [t[j] * lanes[i][0][j] for j in range(4)]
                want = [c[0] * c[3], c[1] * c[3], c[2] * c[3], c[3]]
                plain_bad += differ(outs[0][:4], want) > 1e-3
    n = 4 * max(1, count // 4)
    if mismatches > n // 100:
        problems.append("%s: optimised and plain builds differ for %d of %d pixels (by up to %.3g)"
                        % (path.name, mismatches, n, worst))
    if plain_bad:
        problems.append("%s: %d of %d pixels aren't the texel times the vertex colour" % (path.name, plain_bad, n))


def check_vertex(b, path, rng, count, problems):
    spv = b.spirv(path)
    push = struct.pack("<8f", 2 / W, 2 / H, -1.0, -1.0, 1 / W, 1 / H, 0.0, 0.0)
    builds = [b.build(spv, opt, b"", push) for opt in (True, False)]
    bad, found = 0, {}
    for _ in range(count):
        x, y = rng.uniform(0, W), rng.uniform(0, H)
        uv = [rng.uniform(0, 1), rng.uniform(0, 1), 0.0, 1.0]
        color = [rng.uniform(0, 1) for _ in range(4)]
        # xost fetches by location, the highest first: the texture's coordinates (2), the colour (1), the position (0)
        inputs = {0: uv, 1: color, 2: [x, y, 0.0, 1.0]}
        runs = [b.simulate.run(c, registers={0: [0.0] * 4}, constants=k, loops=lp,
                               vertex_fetch=lambda n, fetch, index: inputs[n]) for c, k, lp in builds]
        want = [2 * x / W - 1, 2 * y / H - 1, 0.0, 1.0]
        for r in runs:
            if differ(r.exports.get(62, [0.0] * 4), want) > 1e-4 * max(1.0, x, y):
                bad += 1
        for reg, value in runs[0].exports.items():
            for name, expect in (("v_color", color), ("v_uv", uv[:2]), ("v_screen", [x / W, y / H])):
                if differ(value[:len(expect)], expect) < 1e-5:
                    found.setdefault(name, set()).add(reg)
    if bad:
        problems.append("%s: the position is wrong for %d of %d vertices" % (path.name, bad, 2 * count))
    for name in ("v_color", "v_uv", "v_screen"):
        if len(found.get(name, ())) != 1:
            problems.append("%s: %s in exports %s" % (path.name, name, sorted(found.get(name, ()))))
    return {name: min(regs) for name, regs in found.items() if regs}


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("folder", type=Path)
    parser.add_argument("--count", type=int, default=64, help="pixels or vertices a shader (default 64)")
    parser.add_argument("--only", help="the one shader file to check")
    args = parser.parse_args()
    xsdk, glslang = os.environ.get("XSDK"), os.environ.get("GLSLANG")
    if not xsdk or not glslang:
        sys.exit("set XSDK and GLSLANG")
    with tempfile.TemporaryDirectory() as work:
        b = Builder(xsdk, glslang, Path(work))
        rng = random.Random(1)
        problems = []
        files = sorted(p for p in args.folder.iterdir() if p.suffix in (".vert", ".frag"))
        if args.only:
            files = [p for p in files if p.name == args.only]
        for path in files:
            before = len(problems)
            try:
                if path.suffix == ".vert":
                    exports = check_vertex(b, path, rng, args.count, problems)
                    print("%s: varyings in exports %s" % (path.name, exports))
                else:
                    check_fragment(b, path, rng, args.count, problems)
            except Exception as e:  # a shader the simulator can't run is a problem to look at
                problems.append("%s: %s: %s" % (path.name, type(e).__name__, e))
            if len(problems) == before:
                print("%s: ok" % path.name)
        for p in problems:
            print("PROBLEM " + p)
        print("%d shaders, %d problems" % (len(files), len(problems)))
        return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
