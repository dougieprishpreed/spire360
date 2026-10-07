# Building the game logic

Everything here runs on the player's PC and produces files that contain the
player's game code: keep them local.

## Prerequisites

- .NET 10 SDK.
- Python 3.9+.
- For the wasm stage: [wasi-sdk 29](https://github.com/WebAssembly/wasi-sdk/releases)
  with `WASI_SDK_PATH` pointing at it, and access to the dotnet-experimental
  NuGet feed (used only by that stage, through `logic/wasm-nuget.config`).
- For the C stage: CMake and a C compiler (to build w2c2 once).

## One command

```
python tools/build_logic.py --game-dir "C:\Program Files (x86)\Steam\steamapps\common\Slay the Spire 2"
```

Stages, in order (`--stage a,b,...` runs a subset):

| Stage | Output | Check |
|---|---|---|
| copy | `logic/lib/game/` + `manifest.json` (hashes, game version) | prints the version |
| patch | `logic/lib/patched/sts2.dll` + `patch_report.json` | every non-optional patch `applied` |
| jit | `out/logic/jit/` | builds |
| smoke | - | a decision comes back |
| aot | `out/logic/aot-<rid>/` | builds (ILC warnings list members that will throw) |
| wasm | `out/logic/logic.wasm` | builds |
| c | `out/logic/c/*.c` + `logic_stubs.c` | import audit prints 0 missing, or the list to implement |

`stubs` (not in the default list) regenerates
`logic/Spire.GodotStubs/Generated/` from the game's real `GodotSharp.dll`;
run it after a game update, before `patch`.

## Regression

The logic needs localization tables in `<data>/localization/<lang>/*.json`
(the converter extracts them; `SPIRE_DATA` points at `<data>`).

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
