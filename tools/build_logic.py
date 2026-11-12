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
  native   compile out/logic/c with the runtime into a PC program
           (out/logic/native/logic_host): the translated logic, run natively

Usage:
  build_logic.py --game-dir "<...>/Slay the Spire 2" [--stage all|copy|patch|...]

Requirements: .NET 10 SDK; for "wasm" also WASI_SDK_PATH (wasi-sdk 29) and
access to the dotnet-experimental NuGet feed (see docs/LOGIC_BUILD.md).
Nothing produced here may be redistributed: it contains the player's game code.
"""
import argparse
import filecmp
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
STAGES = ["copy", "stubs", "patch", "jit", "smoke", "aot", "wasm", "c", "native"]
# "aot" (a NativeAOT build for this PC) is a diagnostic; on Windows it needs
# the VS 2022 C++ tools, so it is opt-in.
DEFAULT_STAGES = ["copy", "patch", "jit", "smoke", "wasm", "c"]


# Parallel work the build may use (--jobs): MSBuild nodes, the IL compiler,
# the native stage's compiles. Two by default, so the machine stays usable.
JOBS = 2


def run(cmd, **kw):
    if cmd[:2] in (["dotnet", "build"], ["dotnet", "publish"]):
        cmd = cmd + ["-m:%d" % JOBS] + (["-p:SpireIlcParallelism=%d" % JOBS] if cmd[1] == "publish" else [])
    elif cmd[:2] == ["cmake", "--build"]:
        cmd = cmd + ["--parallel", str(JOBS)]
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
    write_anim_lengths(a.game_dir)


def write_anim_lengths(game_dir):
    """Each creature's Spine animations and their lengths (the game's .pck
    through the converter's readers), embedded in the logic: the UI calls
    that answer with an animation's length (NCreature.StartDeathAnim) get
    the game's value headless."""
    out = os.path.join(LIB_GAME, "anim_lengths.json")
    pck = next((os.path.join(d, f) for d, _, fs in os.walk(game_dir) for f in fs if f.lower().endswith(".pck")), None)
    if not pck:
        print("  (no .pck: animation lengths left out)")
        return
    sys.path.insert(0, os.path.join(ROOT, "converter"))
    import re
    from spire_convert import resources
    from spire_convert import spine as spinefmt
    from spire_convert.pck import Pck
    src = resources.PckSource(Pck(pck))
    res = resources.Resources(src)

    def ext(text, kind):
        return re.findall(r'\[ext_resource type="' + kind + r'"[^\]]*path="res://([^"]+)"', text)

    table = {}
    for f in sorted(src.list()):
        if not (f.startswith("scenes/creature_visuals/") and f.endswith(".tscn")):
            continue
        try:
            scene = src.read(res.resolve("res://" + f)).decode("utf-8")
            data = ext(scene, "SpineSkeletonDataResource")
            if not data:
                continue
            skel = ext(src.read(res.resolve("res://" + data[0])).decode("utf-8"), "SpineSkeletonFileResource")
            doc = spinefmt.engine_skeleton(spinefmt.read_skeleton(src.read(res.resolve("res://" + skel[0]))))
            table["res://" + f] = {an["name"]: round(an["duration"], 4) for an in doc["animations"]}
        except Exception as e:  # noqa: BLE001
            print(f"  (animation lengths: {f}: {e})")
    with open(out, "w", encoding="utf-8") as fh:
        json.dump(table, fh, indent=0, sort_keys=True)
    print(f"  animation lengths of {len(table)} creatures")


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
    run(["dotnet", "build", os.path.join(LOGIC, "Spire.ApiDump"), "-c", "Release", "-o", os.path.join(OUT, "apidump"),
         "-nologo", "-v", "q"])
    check_stubs(os.path.join(tmp, "GodotSharp.dll"))


def check_stubs(stub_dll):
    """Stub enums, class hierarchy and constraints must match the real
    GodotSharp: NativeAOT-LLVM rejects callers whose IL disagrees with them."""
    real = os.path.join(LIB_GAME, "GodotSharp.real.dll")
    if not os.path.exists(real):
        print("  (no GodotSharp.real.dll: run the copy stage to check the stubs)")
        return
    tools = os.path.join(OUT, "apidump")
    r = run(["dotnet", os.path.join(tools, "Spire.ApiDump.dll"), "--check-stubs", stub_dll, real], check=False)
    if r.returncode != 0:
        sys.exit("stub check failed: fix logic/Spire.GodotStubs (see docs/LOGIC_BUILD.md)")


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


def find_wasi_sdk():
    """WASI_SDK_PATH, else wasi-sdk 29 where docs/LOGIC_BUILD.md puts it
    (or next to the repository, or in the home folder)."""
    sdk = os.environ.get("WASI_SDK_PATH")
    if sdk:
        return sdk
    names = ["wasi-sdk-29.0-x86_64-windows", "wasi-sdk-29.0-x86_64-linux", "wasi-sdk-29.0-arm64-linux",
             "wasi-sdk-29.0-x86_64-macos", "wasi-sdk-29.0-arm64-macos", "wasi-sdk-29.0", "wasi-sdk"]
    roots = [os.path.join(os.path.splitdrive(ROOT)[0] or "C:", os.sep), "C:" + os.sep, ROOT,
             os.path.dirname(ROOT), os.path.expanduser("~"), "/opt"]
    exe = "clang.exe" if os.name == "nt" else "clang"
    for r in roots:
        for n in names:
            d = os.path.join(r, n)
            if os.path.exists(os.path.join(d, "bin", exe)):
                print(f"(wasi-sdk: {d})")
                return d
    return None


def stage_wasm(a):
    sdk = find_wasi_sdk()
    if not sdk:
        sys.exit("wasi-sdk 29 not found: extract it to C:\\wasi-sdk-29.0-x86_64-windows (docs/LOGIC_BUILD.md) "
                 "or set WASI_SDK_PATH to the folder that contains bin/ and share/")
    clang = os.path.join(sdk, "bin", "clang" + (".exe" if os.name == "nt" else ""))
    if not os.path.exists(clang):
        sys.exit(f"WASI_SDK_PATH={sdk} has no bin/{os.path.basename(clang)}; point it at the extracted wasi-sdk folder")
    out = os.path.join(OUT, "wasm")
    # A wasm-tools / wasi-experimental workload would otherwise take over the
    # wasi-wasm RID from the ILCompiler.LLVM packages.
    # The ILCompiler.LLVM targets read WASI_SDK_PATH themselves: pass on
    # the one found here.
    env = dict(os.environ, MSBuildEnableWorkloadResolver="false", WASI_SDK_PATH=sdk)
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
    cdir = a.c_dir or os.path.join(OUT, "c")
    # Translated into a fresh folder, then only the files whose contents
    # changed are copied over: the others keep their dates, so the XDK build
    # (an hour for everything) compiles only what changed.
    fresh = cdir.rstrip("/\\") + ".new"
    shutil.rmtree(fresh, ignore_errors=True)
    os.makedirs(fresh)
    # Module name "logic" -> symbols m5_logic*; 2000 functions per file keeps
    # each translation unit small enough for the XDK compiler.
    run([w2c2, "-f", str(a.funcs_per_file), os.path.join(OUT, "logic.wasm"), os.path.join(fresh, "logic.c")])
    # The table initialiser as data: one 100k-statement function is more than
    # the XDK compiler's heap can take.
    run([sys.executable, os.path.join(ROOT, "tools", "w2c2_fix_tables.py"), os.path.join(fresh, "logic.c")])
    run([sys.executable, os.path.join(ROOT, "tools", "w2c2_imports.py"), os.path.join(fresh, "logic.h"),
         "--stubs", os.path.join(fresh, "logic_stubs.c")], check=False)
    data_segments_out(os.path.join(fresh, "logic.c"))
    if a.no_hoist:
        print("the memory base is not hoisted (--no-hoist)")
    else:
        hoist_memory_base(fresh)
    if a.no_cold_os:
        print("the -Os split is off (--no-cold-os): every function -O2")
    else:
        split_cold(fresh, LINKMAP, HOT_LIST)
    os.makedirs(cdir, exist_ok=True)
    changed = 0
    for name in sorted(os.listdir(fresh)):
        new, old = os.path.join(fresh, name), os.path.join(cdir, name)
        if os.path.exists(old) and filecmp.cmp(new, old, shallow=False):
            continue
        shutil.copyfile(new, old)
        changed += 1
    for name in os.listdir(cdir):
        if not os.path.exists(os.path.join(fresh, name)):
            os.remove(os.path.join(cdir, name))
    shutil.rmtree(fresh, ignore_errors=True)
    n = len([f for f in os.listdir(cdir) if f.endswith(".c")])
    print(f"{n} C files in {cdir} ({changed} files changed)")


def data_segments_out(path):
    """The module's large data segments (its initial memory, 17 MB) to logic.dat beside the C, as LZ4 blocks,
    read back while the module is instantiated (srt_data_load, runtime/src/srt_data.c): the console keeps the
    whole XEX image in memory, where they were held for the session and read once. logic.dat goes beside
    default.xex (tools/xsdk/build.sh, tools/install/stage.py) and beside logic_host. The C gets
    spire_logic_data_table (count, then each segment's stored size, size and FNV-1a of the stored bytes), which
    the host holds the file to before it instantiates anything. The file: 'SPLD' u32 version (1) u32 count
    u32 0; count entries of u32 offset, stored, size, fnv; the blocks. A block as large as its segment is the
    segment as it is (no lz4 module: pip install lz4)."""
    import re
    import struct
    try:
        import lz4.block
    except ImportError:
        lz4 = None
        print("WARNING: no lz4 module (pip install lz4): logic.dat holds the segments uncompressed")
    with open(path, "rb") as f:
        text = f.read().decode("utf-8")
    loads = re.findall(r"LOAD_DATA\(\(\*i->m0\), (\d+)U, (m5_logicData\d+), (\d+)\);", text)
    out, at, blobs, calls, before = [], 0, [], {}, 0
    for offset, name, size in loads:
        offset, size = int(offset), int(size)
        if size < 65536:
            continue
        head = "const U8 %s[]={" % name
        start = text.index(head, at)
        end = text.index("};", start)
        data = bytes(int(v, 0) for v in text[start + len(head):end].split(",") if v.strip())   # w2c2 writes 7 or 0x45
        if len(data) != size:
            sys.exit("data segment %s: %d bytes, LOAD_DATA says %d" % (name, len(data), size))
        stored = data
        if lz4 is not None:
            packed = lz4.block.compress(data, mode="high_compression", compression=12, store_size=False)
            if len(packed) < size:
                stored = packed
        fnv = 0x811c9dc5
        for b in stored:
            fnv = ((fnv ^ b) * 0x01000193) & 0xffffffff
        out.append(text[at:start])
        out.append("/* %s: in logic.dat */\n" % name)
        at = end + 2
        calls[name] = (len(blobs), offset, size, len(stored), fnv)
        blobs.append((stored, size, fnv))
        before += size
    if not blobs:
        sys.exit("no data segments of 64 KB or more in %s: has w2c2's output changed?" % path)
    out.append(text[at:])
    text = "".join(out)
    for name, (index, offset, size, stored, fnv) in calls.items():
        call = "LOAD_DATA((*i->m0), %dU, %s, %d);" % (offset, name, size)
        if text.count(call) != 1:
            sys.exit("data segment %s: its LOAD_DATA call not found once" % name)
        text = text.replace(call, "wasmMemoryCheckRange(i->m0, %dU, %d);\nif (srt_data_load(%d, %d, 0x%08xu, i->m0->data + %dU, %d) != 0) abort();"
                            % (offset, size, index, stored, fnv, offset, size))
    table = ",".join("%d,%d,0x%08xu" % (len(s), size, fnv) for s, size, fnv in blobs)
    decl = ("int srt_data_load(unsigned index, unsigned stored, unsigned fnv, unsigned char* dst, unsigned size);\n"
            "const unsigned spire_logic_data_table[] = {%d,%s};\n" % (len(blobs), table))
    if text.count("static void m5_logicInitDataSegments(") != 1:
        sys.exit("m5_logicInitDataSegments not found once in " + path)
    text = text.replace("static void m5_logicInitDataSegments(", decl + "static void m5_logicInitDataSegments(", 1)
    with open(path, "wb") as f:
        f.write(text.encode("utf-8"))
    entries, pos = b"", 16 + 16 * len(blobs)
    for stored, size, fnv in blobs:
        entries += struct.pack("<IIII", pos, len(stored), size, fnv)
        pos += len(stored)
    with open(os.path.join(os.path.dirname(path), "logic.dat"), "wb") as f:
        f.write(struct.pack("<4sIII", b"SPLD", 1, len(blobs), 0) + entries + b"".join(s for s, _, _ in blobs))
    print("data segments: %d KB as %d KB in logic.dat" % (before // 1024, (pos - 16 - 16 * len(blobs)) // 1024))


# The wasm link map of the logic's build (the IL compiler's wasm-ld), which names each translated function, and
# the list of those that run often (tools/hot_functions.py), by the hash of that name.
LINKMAP = os.path.join(LOGIC, "Spire.Logic", "obj", "Release", "net10.0", "wasi-wasm", "native", "Spire.Logic.linkmap")
HOT_LIST = os.path.join(LOGIC, "hot_functions.txt")


def function_names(linkmap, header):
    """fN -> its method's name (wasm-ld's map: the CODE section's functions in index order, after the module's
    imports, which the C numbers first). A name met again gets #2, #3..."""
    import re
    sec = re.compile(r"^\s+-\s+[0-9a-f]+\s+[0-9a-f]+ ([A-Z_]+)$")
    fn = re.compile(r"^\s+-\s+[0-9a-f]+\s+[0-9a-f]+\s+\S.*?:\((.*)\)$")
    names, inside = [], False
    with open(linkmap, encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.rstrip()
            m = sec.match(line)
            if m:
                if inside:
                    break
                inside = m.group(1) == "CODE"
                continue
            if inside:
                m = fn.match(line)
                if m:
                    names.append(m.group(1))
    with open(header, encoding="utf-8", errors="replace") as f:
        imports = len(set(re.findall(r"\b(i\d+_\w+)\(", f.read())))
    seen, out = {}, {}
    for k, n in enumerate(names):
        seen[n] = seen.get(n, 0) + 1
        out["f%d" % (imports + k)] = n if seen[n] == 1 else "%s#%d" % (n, seen[n])
    return out


def name_hash(name):
    """FNV-1a 64 of a function's name, as the hot list keeps it."""
    h = 0xcbf29ce484222325
    for b in name.encode("utf-8"):
        h = ((h ^ b) * 0x100000001b3) & 0xffffffffffffffff
    return "%016x" % h


def split_cold(cdir, linkmap, hot_list):
    """Each logic file's functions that ran rarely when sampled go to a twin, <file>_cold.c, which the console
    compiles for size (-Os, xsdk/CMakeLists.txt): smaller than -O2 and slower, and the console keeps all code
    resident. The list (tools/hot_functions.py) names the functions that ran often, by the hash of their name
    in the wasm link map; the others are rare, functions new since the sampling among them. Without the list
    or the map every function stays -O2."""
    import re
    if not os.path.exists(hot_list) or not os.path.exists(linkmap):
        print("no hot list or link map (%s, %s): every function -O2" % (hot_list, linkmap))
        return
    with open(hot_list, encoding="utf-8") as f:
        hot = set(line.strip() for line in f if line.strip() and not line.startswith("#"))
    names = function_names(linkmap, os.path.join(cdir, "logic.h"))
    sig = re.compile(r"^\w+ (f\d+)\(wasmModuleInstance\* instance")
    n_hot = n_cold = n_unnamed = 0
    for name in sorted(os.listdir(cdir)):
        if not re.match(r"m5_logic\.s\d+\.c$", name):
            continue
        path = os.path.join(cdir, name)
        with open(path, "rb") as f:
            text = f.read().decode("utf-8")
        eol = "\r\n" if "\r\n" in text else "\n"
        head, keep, rare = [], [], []
        cur, depth, fname, opened = None, 0, None, False
        for ln in text.split(eol):
            if cur is None:
                m = sig.match(ln)
                if not m:
                    (keep if keep or rare else head).append(ln)
                    continue
                cur, fname, depth, opened = [], m.group(1), 0, False
            cur.append(ln)
            depth += ln.count("{") - ln.count("}")
            opened = opened or "{" in ln
            if depth == 0 and opened:
                method = names.get(fname)
                if method is None:
                    n_unnamed += 1
                if method is not None and name_hash(method) not in hot:
                    rare.extend(cur)
                    n_cold += 1
                else:
                    keep.extend(cur)
                    n_hot += 1
                cur = None
        if cur is not None:
            sys.exit("%s: a function without its end" % name)
        with open(path, "wb") as f:
            f.write(eol.join(head + keep).encode("utf-8"))
        with open(path[:-2] + "_cold.c", "wb") as f:
            f.write(eol.join(head + rare).encode("utf-8"))
    if n_unnamed:
        print("WARNING: %d functions the link map does not name (kept -O2): is the map this build's?" % n_unnamed)
    print("hot list: %d functions -O2, %d to the -Os files" % (n_hot, n_cold))


ACCESS = r"\b((?:i32|i64|f32|f64)_(?:load|store)(?:8|16|32)?(?:_[su])?)\(i->m0,"
INSTANCE = r"(m5_logicInstance\* i = \(m5_logicInstance\*\)instance;(\r?\n))"


def hoist_memory_base(cdir):
    """Each function that reads or writes the linear memory loads its base once (mb) and goes through the
    accessors' base-pointer twins (name_b, third_party/w2c2/w2c2/w2c2_base.h). Through i->m0 every access
    loaded the memory struct and its data pointer again, since a store might have changed them (the C is
    compiled without strict aliasing): two dependent loads per access. The hosts never move the block
    (W2C2_HOST_MEMORY: reserved whole, committed in place as it grows), so the base holds for the memory's
    life; the PC's logic_host compiles this same C."""
    import re
    access, instance = re.compile(ACCESS), re.compile(INSTANCE)
    funcs = accesses = 0
    for name in sorted(os.listdir(cdir)):
        if not name.endswith(".c"):
            continue
        path = os.path.join(cdir, name)
        with open(path, "rb") as f:
            text = f.read().decode("utf-8")
        parts = instance.split(text)   # [before, line, eol, body, line, eol, body, ...]
        if len(parts) == 1:
            continue
        out = [parts[0]]
        for k in range(1, len(parts), 3):
            line, eol, body = parts[k], parts[k + 1], parts[k + 2]
            body, n = access.subn(r"\1_b(mb,", body)
            out.append(line + ("U8* const mb = i->m0->data;" + eol if n else "") + body)
            if n:
                funcs += 1
                accesses += n
        with open(path, "wb") as f:
            f.write("".join(out).encode("utf-8"))
    print("memory base hoisted: %d functions, %d accesses" % (funcs, accesses))


def stage_native(a):
    """The translated logic as a PC program, built from the same C the console
    compiles: tools/random_runs.py --cmd out/logic/native/logic_host."""
    import concurrent.futures
    cdir = a.c_dir or os.path.join(OUT, "c")
    if not os.path.isdir(cdir):
        sys.exit("run the c stage first")
    wasm = os.path.join(OUT, "logic.wasm")
    if os.path.exists(wasm) and os.path.getmtime(wasm) > os.path.getmtime(os.path.join(cdir, "logic.h")):
        print("WARNING: logic.wasm is newer than out/logic/c: run the c stage first")
    # Stubs for whatever the runtime still lacks, against the runtime as it is now.
    run([sys.executable, os.path.join(ROOT, "tools", "w2c2_imports.py"), os.path.join(cdir, "logic.h"),
         "--stubs", os.path.join(cdir, "logic_stubs.c")], check=False, stdout=subprocess.DEVNULL)
    build = os.path.join(OUT, "native" + ("-" + a.native_tag if a.native_tag else ""))
    objdir = os.path.join(build, "obj")
    os.makedirs(objdir, exist_ok=True)
    rt = os.path.join(ROOT, "runtime")
    plat = "srt_plat_win32.c" if os.name == "nt" else "srt_plat_posix.c"
    sources = sorted(os.path.join(cdir, f) for f in os.listdir(cdir) if f.endswith(".c"))
    sources += [os.path.join(rt, "src", f) for f in ("srt_core.c", "srt_wasi.c", "srt_wasi_p2.c", "srt_data.c", plat)]
    sources.append(os.path.join(rt, "hosts", "logic_host.c"))
    incs = [os.path.join(rt, "include"), os.path.join(rt, "src"), os.path.join(ROOT, "third_party", "w2c2", "w2c2"), cdir]
    cl = shutil.which("cl") if os.name == "nt" and not a.native_cc else None
    cc = a.native_cc or (None if cl else next((c for c in ("cc", "gcc", "clang") if shutil.which(c)), None))
    if not cl and not cc:
        sys.exit("no C compiler: run from a Visual Studio x64 command prompt, or install gcc/clang")
    opt = a.native_opt
    extra = a.native_flags.split() if a.native_flags else []
    exe = os.path.join(build, "logic_host" + (".exe" if os.name == "nt" else ""))

    def compile_one(src):
        obj = os.path.join(objdir, os.path.splitext(os.path.basename(src))[0] + (".obj" if cl else ".o"))
        if os.path.exists(obj) and os.path.getmtime(obj) > os.path.getmtime(src):
            return obj
        if cl:
            cmd = [cl, "/nologo", "/c", "/O" + opt, "/w", "/bigobj", "/DW2C2_HOST_MEMORY", "/D_CRT_SECURE_NO_WARNINGS"] + \
                  extra + ["/I" + i for i in incs] + ["/Fo" + obj, src]
        else:
            cmd = [cc, "-c", "-O" + opt, "-w", "-DW2C2_HOST_MEMORY", "-fno-strict-aliasing"] + \
                  extra + ["-I" + i for i in incs] + ["-o", obj, src]
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode != 0:
            raise RuntimeError(f"{os.path.basename(src)}:\n{r.stdout}{r.stderr}")
        return obj

    jobs = JOBS
    print(f"compiling {len(sources)} files with {cl or cc} -O{opt}, {jobs} jobs", flush=True)
    t0 = __import__("time").time()
    objs = []
    with concurrent.futures.ThreadPoolExecutor(jobs) as pool:
        for n, obj in enumerate(pool.map(compile_one, sources), 1):
            objs.append(obj)
            if n % 10 == 0 or n == len(sources):
                print(f"  {n}/{len(sources)} ({__import__('time').time() - t0:.0f} s)", flush=True)
    if cl:
        link = shutil.which("link") or "link"
        rsp = os.path.join(build, "link.rsp")
        with open(rsp, "w") as f:
            f.write("\n".join('"%s"' % o for o in objs))
        run([link, "/nologo", "/STACK:16777216", "/OUT:" + exe, "@" + rsp] +
            (["/MAP:" + os.path.join(build, "logic_host.map")] if a.native_map else []))
    else:
        run([cc, "-o", exe] + objs + ["-lm", "-lpthread"] + (["-static"] if a.native_cc else []))
    # the module's data segments, which logic_host reads from beside itself
    shutil.copyfile(os.path.join(cdir, "logic.dat"), os.path.join(build, "logic.dat"))
    print(f"{exe} ({os.path.getsize(exe) // 1024} KB), {__import__('time').time() - t0:.0f} s")


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
    ap.add_argument("--c-dir", help="c and native stages: the C's folder (default out/logic/c)")
    ap.add_argument("--no-cold-os", action="store_true",
                    help="c stage: no -Os files (logic/hot_functions.txt unused): every function -O2")
    ap.add_argument("--no-hoist", action="store_true",
                    help="c stage: every memory access through i->m0, as w2c2 writes it (a console test)")
    ap.add_argument("--native-opt", default="1", help="optimisation level for the native stage (0, 1, 2)")
    ap.add_argument("--native-cc", help="native stage: another C compiler, e.g. powerpc-linux-gnu-gcc "
                    "(big-endian check under qemu-ppc; links statically)")
    ap.add_argument("--native-tag", help="native stage: output to out/logic/native-<tag>")
    ap.add_argument("--native-flags", help="native stage: more compiler options (the sampling build: /Ob0, no inlining)")
    ap.add_argument("--native-map", action="store_true", help="native stage: the linker's map (logic_host.map) beside it")
    ap.add_argument("--jobs", type=int, default=2,
                    help="parallel work: MSBuild nodes, the IL compiler, the native stage's compiles (default: 2)")
    a = ap.parse_args()
    global JOBS
    JOBS = max(1, a.jobs)
    # Visual Studio developer prompts set Platform=x64, which MSBuild reads as
    # a property (wasi-wasm publish fails with NETSDK1032).
    for k in [k for k in os.environ if k.lower() == "platform"]:
        print(f"(ignoring environment variable {k}={os.environ[k]})")
        del os.environ[k]
    stages = DEFAULT_STAGES if a.stage == "all" else a.stage.split(",")
    if "copy" in stages and not a.game_dir:
        sys.exit("--game-dir is required for the copy stage")
    for s in stages:
        print(f"== {s}", flush=True)
        try:
            globals()["stage_" + s](a)
        except SystemExit as e:
            if e.code in (0, None):
                raise
            print(f"\n{e.code}" if isinstance(e.code, str) else "", flush=True)
            print(f"BUILD_LOGIC FAILED in stage {s}", flush=True)
            sys.exit(1)
        except subprocess.CalledProcessError as e:
            # The failing tool printed its own errors above.
            print(f"\n{os.path.basename(str(e.cmd[0]))} exited with {e.returncode}", flush=True)
            print(f"BUILD_LOGIC FAILED in stage {s}", flush=True)
            sys.exit(1)
    done = "BUILD_LOGIC OK: " + ", ".join(stages)
    if "c" in stages:
        done += " (out/logic/c is ready for the XDK build)"
    print(done, flush=True)


if __name__ == "__main__":
    main()
