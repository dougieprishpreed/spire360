# Building the game logic

Everything here runs on the player's PC and produces files that contain the
player's game code: keep them local.

## Prerequisites

- .NET 10 SDK.
- Python 3.9+.
- For the wasm stage: [wasi-sdk 29](https://github.com/WebAssembly/wasi-sdk/releases)
  with `WASI_SDK_PATH` pointing at it, and internet access to the
  dotnet-experimental NuGet feed (nothing to configure: the stage passes
  `logic/wasm-nuget.config` to restore).
- For the C stage: a C compiler to build w2c2 once (CMake is used if present,
  otherwise one direct `cl`/`cc` call).

## Walkthrough on Windows

1. Install the [.NET 10 SDK](https://dotnet.microsoft.com/download/dotnet/10.0)
   (`winget install Microsoft.DotNet.SDK.10`). `dotnet --version` prints `10.x`.
2. Download `wasi-sdk-29.0-x86_64-windows.tar.gz` from the wasi-sdk 29
   release and extract it (Windows 10+ has `tar`):
   `tar -xzf wasi-sdk-29.0-x86_64-windows.tar.gz -C C:\` gives
   `C:\wasi-sdk-29.0-x86_64-windows\bin\clang.exe`.
3. Check the feed is reachable: opening
   `https://pkgs.dev.azure.com/dnceng/public/_packaging/dotnet-experimental/nuget/v3/index.json`
   in a browser shows JSON.
4. Open a **Developer Command Prompt** of Visual Studio (any edition, or its
   free Build Tools with the C++ workload); its `cl` builds w2c2. CMake on
   `PATH` is used instead when there is one.
5. In the repository, with `<game>` the Slay the Spire 2 folder (the one
   holding `SlayTheSpire2.pck`):

   ```
   python converter\pck_tool.py extract "<game>\SlayTheSpire2.pck" data "localization/*"
   set WASI_SDK_PATH=C:\wasi-sdk-29.0-x86_64-windows
   python tools\build_logic.py --game-dir "<game>" > build_logic.log 2>&1
   ```

   The first run downloads the ILCompiler.LLVM packages (a few hundred MB)
   and takes several minutes. `data\` (the extracted tables) is git-ignored.
6. The last line of `build_logic.log` must be `BUILD_LOGIC OK: ...` (it
   names the stages run; for the console, `c` must be among them). Before
   it come `SMOKE PASS`, `logic.wasm: <n> KB, core module (good)`, the
   import audit and the number of C files. `BUILD_LOGIC FAILED in stage
   ...` means the stages after it did not run: `out\logic\c` is still the
   previous build. Send `build_logic.log` either way.

   `WASI_SDK_PATH` can be left unset when wasi-sdk is extracted to
   `C:\wasi-sdk-29.0-x86_64-windows` (or next to the repository): the
   script finds it.

Linux and WSL work as well, with the Linux wasi-sdk tarball and the game's
folder as Linux sees it (in WSL, `/mnt/c/...`). `tools/install/build_all.sh`
does all of this on Linux in one go.

## One command

```
python tools/build_logic.py --game-dir "C:\Program Files (x86)\Steam\steamapps\common\Slay the Spire 2"
```

Stages, in order (`--stage a,b,...` runs a subset):

| Stage | Output | Check |
|---|---|---|
| copy | `logic/lib/game/` + `manifest.json` (hashes, game version) | prints the version |
| patch | `logic/lib/patched/sts2.dll` + `patch_report.json` | every non-optional patch `applied`; `check-stubs: ... 0 problem(s)` |
| jit | `out/logic/jit/` | builds |
| smoke | - | a decision comes back |
| aot | `out/logic/aot-<rid>/` | builds (ILC warnings list members that will throw); opt-in, needs the platform's native linker (VS 2022 C++ tools on Windows) |
| wasm | `out/logic/logic.wasm` | builds |
| c | `out/logic/c/*.c` + `logic_stubs.c` | import audit prints 0 missing, or the list to implement |
| native | `out/logic/native/logic_host` | the translated C as a PC program; drive it with `random_runs.py --cmd` |

`stubs` and `aot` (not in the default list) are opt-in: `stubs`
regenerates `logic/Spire.GodotStubs/Generated/` from the game's real
`GodotSharp.dll` (run it after a game update, before `patch`).

## The translated logic on PC

`--stage native` compiles `out/logic/c` with the runtime and
`runtime/hosts/logic_host.c` (all cores; `--native-opt 0|1|2`). On Windows
run it from the **x64 Native Tools Command Prompt for VS 2022** (64-bit
`cl`/`link`; the host reserves 1 GB for the guest's memory). Then:

```
python tools/random_runs.py --cmd "out/logic/native/logic_host --root ." --runs 2
```

`--root` is mounted as the guest's `/`; the logic reads `/data`, i.e.
`./data/localization`. Same seeds should give the same traces as the JIT
build (`--trace-dir`).

## Regression

The logic needs localization tables in `<data>/localization/<lang>/*.json`
(`pck_tool.py extract <pck> data "localization/*"`; `SPIRE_DATA` points at
`<data>`, and `./data` is used when it is not set).

```
python tools/random_runs.py --cmd "dotnet out/logic/jit/Spire.Logic.dll" --runs 5 --data-dir <data>
python tools/random_runs.py --cmd "dotnet out/logic/jit/Spire.Logic.dll" --runs 3 --tank --data-dir <data>
```

`--tank` keeps the player alive so later acts are exercised. `--trace-dir`
writes one JSONL per run (commands and responses); traces from two builds with
the same seeds should be identical.

## When a game update breaks something

1. `--stage copy,stubs,patch,jit`. A missing patch target or a compile error
   names what moved.
2. `dotnet out/apidump/Spire.ApiDump.dll logic/lib/game/sts2.dll <TypeName>`
   prints a type's members; `--calls <dll> Type::Method` lists what a method
   calls (handy for "why does this NRE headless").
3. Re-run the regression; read `<trace>.log` of a failing run.
