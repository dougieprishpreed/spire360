#!/usr/bin/env python3
"""Runtime self-test: wasi-sdk C program -> wasm -> w2c2 -> C -> native and
big-endian PowerPC (under qemu), driven over the stdin/stdout pipes.

Usage: run_tests.py [--wasi-sdk DIR] [--w2c2 PATH] [--no-ppc]

Environment fallbacks: WASI_SDK_PATH, W2C2.
"""
import argparse
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
RUNTIME = os.path.join(ROOT, "runtime")
W2C2_INC = os.path.join(ROOT, "third_party", "w2c2", "w2c2")

RUNTIME_SOURCES = ["srt_core.c", "srt_wasi.c", "srt_wasi_p2.c"]
WINE = "/usr/lib/wine/wine64"
STRICT = ["-std=c89", "-pedantic", "-Wall", "-Wextra", "-Werror",
          "-Wno-long-long", "-Wno-unused-function", "-Wno-overlength-strings"]


def run(cmd, **kw):
    print("+", " ".join(cmd), flush=True)
    return subprocess.run(cmd, check=True, **kw)


def build_w2c2(build):
    exe = os.path.join(build, "w2c2-build", "w2c2")
    if os.path.exists(exe):
        return exe
    run(["cmake", "-S", W2C2_INC, "-B", os.path.join(build, "w2c2-build"),
         "-DCMAKE_BUILD_TYPE=Release"], stdout=subprocess.DEVNULL)
    run(["cmake", "--build", os.path.join(build, "w2c2-build"), "-j4", "--target", "w2c2"],
        stdout=subprocess.DEVNULL)
    return exe


def make_root(build):
    root = os.path.join(build, "root")
    shutil.rmtree(root, ignore_errors=True)
    os.makedirs(os.path.join(root, "data"))
    for name in ("one", "two", "three.bin"):
        with open(os.path.join(root, "data", name), "w") as f:
            f.write(name)
    return root


def compile_host(cc, out, gen_dir, extra, plat="srt_plat_posix.c", libs=("-lm", "-lpthread")):
    """Compile runtime (strict C89), generated module and host, then link."""
    objs = []
    strict = True
    defs = ["-DW2C2_HOST_MEMORY", "-DSRT_MODULE=m5_guest", '-DSRT_MODULE_HEADER="guest.h"']
    incs = ["-I", os.path.join(RUNTIME, "include"), "-I", os.path.join(RUNTIME, "src"),
            "-I", W2C2_INC, "-I", gen_dir]
    for src in RUNTIME_SOURCES + [plat]:
        o = os.path.join(gen_dir, cc.replace("/", "_") + "_" + src + ".o")
        flags = (STRICT if strict else []) + ["-O2", "-c"]
        if src.startswith("srt_plat_"):
            # OS headers need feature macros or extensions that -std=c89
            # -pedantic rejects; keep warnings strict but allow them.
            flags = ["-std=c99", "-Wall", "-Wextra", "-Werror", "-O2", "-c"]
        run([cc] + flags + defs + incs + extra + [os.path.join(RUNTIME, "src", src), "-o", o])
        objs.append(o)
    for src, flags in (("guest.c", ["-std=c99", "-O1", "-c", "-w"]),
                       ("stubs.c", STRICT + ["-O2", "-c"]),
                       (os.path.join(HERE, "host_main.c"), STRICT + ["-O2", "-c"])):
        o = os.path.join(gen_dir, cc.replace("/", "_") + "_" + os.path.basename(src) + ".o")
        path = src if os.path.isabs(src) else os.path.join(gen_dir, src)
        run([cc] + flags + defs + incs + extra + [path, "-o", o])
        objs.append(o)
    run([cc] + objs + extra + ["-o", out] + list(libs))


def check(name, cmd, expected, env=None):
    print(f"== {name}", flush=True)
    r = subprocess.run(cmd, capture_output=True, text=True, timeout=600, env=env)
    got = r.stdout.replace("\r\n", "\n")
    if r.stderr:
        print(r.stderr.rstrip())
    if got != expected:
        print(f"FAIL {name}")
        import difflib
        sys.stdout.writelines(difflib.unified_diff(expected.splitlines(True), got.splitlines(True),
                                                   "expected", "got"))
        return False
    print(f"PASS {name}")
    return True


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--wasi-sdk", default=os.environ.get("WASI_SDK_PATH"))
    ap.add_argument("--w2c2", default=os.environ.get("W2C2"))
    ap.add_argument("--no-ppc", action="store_true")
    ap.add_argument("--build", default=os.path.join(HERE, "build"))
    a = ap.parse_args()
    if not a.wasi_sdk:
        sys.exit("set --wasi-sdk or WASI_SDK_PATH")
    build = a.build
    os.makedirs(build, exist_ok=True)
    w2c2 = a.w2c2 or build_w2c2(build)

    gen = os.path.join(build, "gen")
    os.makedirs(gen, exist_ok=True)
    wasm = os.path.join(gen, "guest.wasm")
    run([os.path.join(a.wasi_sdk, "bin", "clang"), "--sysroot", os.path.join(a.wasi_sdk, "share", "wasi-sysroot"),
         "-O2", "-D_WASI_EMULATED_PROCESS_CLOCKS", os.path.join(HERE, "guest_selftest.c"), "-o", wasm, "-lm"])
    run([w2c2, wasm, os.path.join(gen, "guest.c")], stderr=subprocess.DEVNULL)
    # The guest deliberately imports one function the runtime lacks.
    audit = subprocess.run([sys.executable, os.path.join(ROOT, "tools", "w2c2_imports.py"),
                            os.path.join(gen, "guest.h"), "--stubs", os.path.join(gen, "stubs.c")],
                           capture_output=True, text=True)
    print(audit.stdout)
    if audit.returncode != 1 or "MISSING missing" not in audit.stdout:
        sys.exit("import audit did not flag spire_test.missing")

    root = make_root(build)
    script = os.path.join(HERE, "script.txt")
    with open(os.path.join(HERE, "expected.txt")) as f:
        expected = f.read()

    ok = True
    native = os.path.join(build, "host_native")
    compile_host("gcc", native, gen, [])
    ok &= check("native x86-64", [native, root, script, "alpha"], expected)

    if not a.no_ppc and shutil.which("powerpc-linux-gnu-gcc") and shutil.which("qemu-ppc"):
        ppc = os.path.join(build, "host_ppc")
        compile_host("powerpc-linux-gnu-gcc", ppc, gen, ["-static"])
        ok &= check("powerpc big-endian (qemu)", ["qemu-ppc", ppc, root, script, "alpha"], expected)
    else:
        print("SKIP powerpc (needs powerpc-linux-gnu-gcc and qemu-ppc)")

    if shutil.which("x86_64-w64-mingw32-gcc") and os.path.exists(WINE):
        win = os.path.join(build, "host_win32.exe")
        compile_host("x86_64-w64-mingw32-gcc", win, gen, ["-static"], plat="srt_plat_win32.c", libs=())
        env = dict(os.environ, WINEDEBUG="-all", WINEPREFIX=os.path.join(build, "wineprefix"))
        ok &= check("win32 backend (wine)", [WINE, win, "Z:" + root, "Z:" + script, "alpha"], expected, env=env)
    else:
        print("SKIP win32 (needs x86_64-w64-mingw32-gcc and wine64)")

    # Exit code and trap paths.
    for name, line, want in (("exit code", "exit3\n", "ready\nexit 3\n"),
                             ("trap", "trap\n", "ready\nexit 1000\n"),
                             ("missing import stub", "missing\n", "ready\nexit 1000\n")):
        p = os.path.join(build, "script_" + name.replace(" ", "_") + ".txt")
        with open(p, "w") as f:
            f.write(line)
        ok &= check(name, [native, root, p], want)

    print("ALL PASS" if ok else "FAILURES")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
