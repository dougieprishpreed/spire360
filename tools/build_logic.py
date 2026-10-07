#!/usr/bin/env python3
"""Build the game-logic module from the player's own Slay the Spire 2 files.

Stages (each can be run alone with --stage):
  copy     copy the game's managed DLLs into logic/lib/game/ and record hashes
  stubs    regenerate logic/Spire.GodotStubs/Generated/ from the game's real
           GodotSharp.dll (only needed after a game update)
  patch    apply logic/patches.json with Spire.Patcher -> logic/lib/patched/
  jit      dotnet build of Spire.Logic (the oracle; quick to iterate on)
  smoke    run the JIT build: start a run, play to the first decision, quit
  aot      NativeAOT publish for this PC (finds AOT/reflection problems fast)
  wasm     NativeAOT-LLVM publish to wasi-wasm -> out/logic/logic.wasm
  c        w2c2 translation of logic.wasm -> out/logic/c/ + import audit

Usage:
  build_logic.py --game-dir "<...>/Slay the Spire 2" [--stage all|copy|patch|...]

Requirements: .NET 10 SDK; for "wasm" also WASI_SDK_PATH (wasi-sdk 29) and
access to the dotnet-experimental NuGet feed (see docs/LOGIC_BUILD.md).
Nothing produced here may be redistributed: it contains the player's game code.
"""
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LOGIC = os.path.join(ROOT, "logic")
LIB_GAME = os.path.join(LOGIC, "lib", "game")
LIB_PATCHED = os.path.join(LOGIC, "lib", "patched")
OUT = os.path.join(ROOT, "out", "logic")
# Where converter/pck_tool.py extracts the localization tables by default.
DEFAULT_DATA = os.path.join(ROOT, "data")

GAME_DLLS = [
    "sts2.dll", "SmartFormat.dll", "SmartFormat.ZString.dll", "Sentry.dll",
    "Steamworks.NET.dll", "MonoMod.Backports.dll", "MonoMod.ILHelpers.dll",
    "0Harmony.dll", "System.IO.Hashing.dll",
]
STAGES = ["copy", "stubs", "patch", "jit", "smoke", "aot", "wasm", "c"]
# "aot" (a NativeAOT build for this PC) is a diagnostic; on Windows it needs
# the VS 2022 C++ tools, so it is opt-in.
DEFAULT_STAGES = ["copy", "patch", "jit", "smoke", "wasm", "c"]


def run(cmd, **kw):
    print("+", " ".join(str(c) for c in cmd), flush=True)
    kw.setdefault("check", True)
    return subprocess.run(cmd, **kw)


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def find_game_data(game_dir):
    """The folder that holds sts2.dll (Steam layouts differ per OS)."""
    for dirpath, _dirs, files in os.walk(game_dir):
        if "sts2.dll" in files:
            return dirpath
    sys.exit(f"sts2.dll not found under {game_dir}")


def stage_copy(a):
    src = find_game_data(a.game_dir)
    os.makedirs(LIB_GAME, exist_ok=True)
    manifest = {"source": src, "files": {}}
    for name in GAME_DLLS:
        p = os.path.join(src, name)
        if os.path.exists(p):
            shutil.copy2(p, os.path.join(LIB_GAME, name))
            manifest["files"][name] = sha256(p)
            print(f"  copied {name}")
        else:
            print(f"  (missing {name})")
    # The real GodotSharp.dll is only read (by the stub generator); the build
    # uses Spire.GodotStubs in its place.
    real_godot = os.path.join(src, "GodotSharp.dll")
    if os.path.exists(real_godot):
        shutil.copy2(real_godot, os.path.join(LIB_GAME, "GodotSharp.real.dll"))
        manifest["files"]["GodotSharp.dll"] = sha256(real_godot)
    for cand in (os.path.join(src, "release_info.json"), os.path.join(a.game_dir, "release_info.json")):
        if os.path.exists(cand):
            with open(cand, encoding="utf-8") as f:
                manifest["release_info"] = json.load(f)
            break
    with open(os.path.join(LIB_GAME, "manifest.json"), "w") as f:
        json.dump(manifest, f, indent=2)
    print("  game version:", manifest.get("release_info", {}).get("version", "unknown"))


def stage_stubs(a):
    tools = os.path.join(OUT, "apidump")
    manual = os.path.join(OUT, "stubs-manual")
    run(["dotnet", "build", os.path.join(LOGIC, "Spire.ApiDump"), "-c", "Release", "-o", tools, "-nologo", "-v", "q"])
    run(["dotnet", "build", os.path.join(LOGIC, "Spire.GodotStubs"), "-c", "Release", "-p:SpireManualOnly=true",
         "-o", manual, "-nologo", "-v", "q"])
    run(["dotnet", os.path.join(tools, "Spire.ApiDump.dll"), "--gen-stubs", os.path.join(LIB_GAME, "sts2.dll"),
         os.path.join(manual, "GodotSharp.dll"), os.path.join(LIB_GAME, "GodotSharp.real.dll"),
         os.path.join(LOGIC, "Spire.GodotStubs", "Generated", "GodotApi.g.cs")])


def stage_patch(a):
    tmp = os.path.join(OUT, "patcher")
    run(["dotnet", "build", os.path.join(LOGIC, "Spire.Patcher"), "-c", "Release", "-o", tmp, "-nologo", "-v", "q"])
    run(["dotnet", "build", os.path.join(LOGIC, "Spire.HookBridge"), "-c", "Release", "-o", tmp, "-nologo", "-v", "q"])
    run(["dotnet", "build", os.path.join(LOGIC, "Spire.GodotStubs"), "-c", "Release", "-o", tmp, "-nologo", "-v", "q"])
    os.makedirs(LIB_PATCHED, exist_ok=True)
    for name in GAME_DLLS:
        p = os.path.join(LIB_GAME, name)
        if os.path.exists(p) and name != "sts2.dll":
            shutil.copy2(p, os.path.join(LIB_PATCHED, name))
    run(["dotnet", os.path.join(tmp, "Spire.Patcher.dll"),
         "--in", os.path.join(LIB_GAME, "sts2.dll"), "--out", LIB_PATCHED,
         "--config", os.path.join(LOGIC, "patches.json"),
         "--hookbridge", os.path.join(tmp, "Spire.HookBridge.dll"),
         "--search", tmp, "--search", LIB_GAME,
         "--report", os.path.join(LIB_PATCHED, "patch_report.json")])


def stage_jit(a):
    run(["dotnet", "build", os.path.join(LOGIC, "Spire.Logic"), "-c", "Release",
         "-o", os.path.join(OUT, "jit"), "-nologo"])


def stage_smoke(a):
    exe = os.path.join(OUT, "jit", "Spire.Logic.dll")
    script = "\n".join([
        json.dumps({"cmd": "start_run", "character": a.character, "seed": "spire360-smoke", "ascension": 0}),
        json.dumps({"cmd": "get_map"}),
        json.dumps({"cmd": "quit"}),
    ]) + "\n"
    data = a.data_dir or (DEFAULT_DATA if os.path.isdir(os.path.join(DEFAULT_DATA, "localization")) else None)
    env = dict(os.environ, SPIRE_DATA=data) if data else None
    r = subprocess.run(["dotnet", exe], input=script, capture_output=True, text=True,
                       encoding="utf-8", errors="replace", timeout=600, env=env)
    print(r.stderr[-4000:])
    lines = [l for l in r.stdout.splitlines() if l.strip()]
    for l in lines:
        print(l[:300])
    ok = any('"decision"' in l for l in lines)
    print("SMOKE", "PASS" if ok else "FAIL")
    if not ok:
        sys.exit(1)


def stage_aot(a):
    rid = a.rid or ("win-x64" if os.name == "nt" else "linux-x64")
    run(["dotnet", "publish", os.path.join(LOGIC, "Spire.Logic"), "-c", "Release", "-r", rid,
         "-p:PublishAot=true", "-o", os.path.join(OUT, "aot-" + rid), "-nologo"])


def stage_wasm(a):
    sdk = os.environ.get("WASI_SDK_PATH")
    if not sdk:
        sys.exit("set WASI_SDK_PATH to a wasi-sdk 29 install (the folder that contains bin/ and share/)")
    clang = os.path.join(sdk, "bin", "clang" + (".exe" if os.name == "nt" else ""))
    if not os.path.exists(clang):
        sys.exit(f"WASI_SDK_PATH={sdk} has no bin/{os.path.basename(clang)}; point it at the extracted wasi-sdk folder")
    out = os.path.join(OUT, "wasm")
    # A wasm-tools / wasi-experimental workload would otherwise take over the
    # wasi-wasm RID from the ILCompiler.LLVM packages.
    env = dict(os.environ, MSBuildEnableWorkloadResolver="false")
    # Global properties win over the SDK's own defaults: a core module that
    # imports WASI preview1 (+ CoreLib's few preview2 clock/poll functions),
    # not a preview2 component.
    run(["dotnet", "publish", os.path.join(LOGIC, "Spire.Logic"), "-c", "Release", "-r", "wasi-wasm",
         "-p:SpireWasm=true", "-p:IlcLlvmTarget=wasm32-unknown-wasip1", "-p:LinkerFlavor=lld",
         "-p:DebugType=none", "-o", out, "-nologo",
         "-p:RestoreConfigFile=" + os.path.join(LOGIC, "wasm-nuget.config")]
        + (["-p:SpireIlcLlvmVersion=" + a.ilc_version] if a.ilc_version else []), env=env)
    found = [os.path.join(d, f) for d, _, fs in os.walk(out) for f in fs if f.endswith(".wasm")]
    if not found:
        sys.exit(f"publish succeeded but no .wasm under {out}")
    wasm = max(found, key=os.path.getsize)
    shutil.copy2(wasm, os.path.join(OUT, "logic.wasm"))
    with open(wasm, "rb") as f:
        head = f.read(8)
    kind = {b"\x01\x00\x00\x00": "core module (good)", b"\x0d\x00\x01\x00": "COMPONENT (wrong: w2c2 needs a core module)"}
    print("logic.wasm:", os.path.getsize(wasm) // 1024, "KB,", kind.get(head[4:8], "version " + head[4:8].hex()))


def build_w2c2():
    """Builds third_party/w2c2 once: CMake if present, else one compiler call."""
    exe = "w2c2" + (".exe" if os.name == "nt" else "")
    build = os.path.join(OUT, "w2c2-build")
    for cand in (os.path.join(build, exe), os.path.join(build, "Release", exe)):
        if os.path.exists(cand):
            return cand
    src = os.path.join(ROOT, "third_party", "w2c2", "w2c2")
    os.makedirs(build, exist_ok=True)
    if shutil.which("cmake"):
        run(["cmake", "-S", src, "-B", build, "-DCMAKE_BUILD_TYPE=Release"])
        run(["cmake", "--build", build, "--config", "Release", "--target", "w2c2"])
        for cand in (os.path.join(build, exe), os.path.join(build, "Release", exe)):
            if os.path.exists(cand):
                return cand
        sys.exit("cmake built w2c2 but the executable was not found in " + build)
    # Plain C89 with no configuration needed (no pthreads: single-threaded
    # translation, the same output).
    sources = sorted(os.path.join(src, f) for f in os.listdir(src)
                     if f.endswith(".c") and not f.endswith("_test.c") and f != "test.c")
    out_exe = os.path.join(build, exe)
    cl = shutil.which("cl")
    xedk = os.environ.get("XEDK")
    if cl and xedk and os.path.normcase(os.path.abspath(cl)).startswith(os.path.normcase(os.path.abspath(xedk))):
        sys.exit("the cl on PATH is the XDK's PowerPC compiler: run this from the Visual Studio command "
                 "prompt instead of the Xbox 360 one")
    if cl:
        run([cl, "/nologo", "/O2", "/D_CRT_SECURE_NO_DEPRECATE", "/Fe" + out_exe, "/Fo" + build + os.sep] + sources)
    else:
        cc = next((c for c in ("cc", "gcc", "clang") if shutil.which(c)), None)
        if not cc:
            sys.exit("no C compiler for w2c2: run from a Visual Studio command prompt (cl), install CMake, "
                     "or pass --w2c2 <path to a built w2c2>")
        posix = [] if os.name == "nt" else ["-DHAS_UNISTD=1", "-DHAS_GETOPT=1", "-DHAS_LIBGEN=1", "-DHAS_GLOB=1"]
        run([cc, "-O2"] + posix + ["-o", out_exe] + sources + ([] if os.name == "nt" else ["-lm"]))
    return out_exe


def stage_c(a):
    w2c2 = a.w2c2 or shutil.which("w2c2") or build_w2c2()
    cdir = os.path.join(OUT, "c")
    shutil.rmtree(cdir, ignore_errors=True)
    os.makedirs(cdir)
    # Module name "logic" -> symbols m5_logic*; 2000 functions per file keeps
    # each translation unit small enough for the XDK compiler.
    run([w2c2, "-f", str(a.funcs_per_file), os.path.join(OUT, "logic.wasm"), os.path.join(cdir, "logic.c")])
    run([sys.executable, os.path.join(ROOT, "tools", "w2c2_imports.py"), os.path.join(cdir, "logic.h"),
         "--stubs", os.path.join(cdir, "logic_stubs.c")], check=False)
    n = len([f for f in os.listdir(cdir) if f.endswith(".c")])
    print(f"{n} C files in {cdir}")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--game-dir", help="Slay the Spire 2 install folder (needed for 'copy')")
    ap.add_argument("--stage", default="all", help="all, or a comma list of: " + ",".join(STAGES))
    ap.add_argument("--character", default="Ironclad")
    ap.add_argument("--data-dir", help="SPIRE_DATA for the smoke run (localization tables; default ./data if present)")
    ap.add_argument("--rid", help="RID for the aot stage")
    ap.add_argument("--ilc-version", help="pin Microsoft.DotNet.ILCompiler.LLVM version")
    ap.add_argument("--w2c2", help="path to a built w2c2")
    ap.add_argument("--funcs-per-file", type=int, default=2000)
    a = ap.parse_args()
    stages = DEFAULT_STAGES if a.stage == "all" else a.stage.split(",")
    if "copy" in stages and not a.game_dir:
        sys.exit("--game-dir is required for the copy stage")
    for s in stages:
        print(f"== {s}", flush=True)
        globals()["stage_" + s](a)


if __name__ == "__main__":
    main()
