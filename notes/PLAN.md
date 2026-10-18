# Spire360 plan

Spire360 plays Slay the Spire 2 on an Xbox 360 by running the game's own
logic, taken from the player's install, under a new native presentation layer
that draws the player's own assets. Nothing from Mega Crit is in this
repository; the converter and the build produce everything game-derived on the
player's machine, and the resulting XEX is for personal use only.

This replaces the first plan's clean-room re-implementation of ~1,600 models.
Why: [DECISIONS.md](DECISIONS.md) D1-D3. What exists today and what to run
next: [HANDOFF.md](HANDOFF.md).

Pinned build: **v0.111.0** (commit 41cef1ea, 2026-08-13). Moving to a newer
build is an explicit task: `tools/build_logic.py --stage copy,stubs,patch,jit`
then fix what the build and the regression report.

## Architecture

```
 player's PC (once per game version)                    Xbox 360 (XEX)
 ──────────────────────────────────────                 ─────────────────────────────────
 sts2.dll ─Spire.Patcher─▶ patched IL                   ┌ logic thread (HW thread 2)
   + Spire.Logic (adapter) + Spire.GodotStubs           │  logic.c  (w2c2 output, C89)
   ─NativeAOT-LLVM (wasi-wasm, p1 core module)─▶        │  runtime/ (WASI, pipes, memory)
   logic.wasm ─w2c2─▶ logic*.c ─XDK cl.exe────────────▶ │   stdin ◀── commands (JSON lines)
                                                        │   stdout ──▶ decisions + steps
 SlayTheSpire2.pck ─converter─▶ game:\spire\            └──────────────┬───────────────────
   pack.s3p (textures DXT, Spine, audio, fonts,         ┌ main thread  ▼
   layouts) + data/ (localization, saves)               │  pres/ screens, animation, input
                                                        │  platform/x360: D3D9, XAudio2, XInput
                                                        └ loader + audio threads
```

**Logic** (`logic/`, `runtime/`, `third_party/w2c2`). The real game code,
unmodified except for documented IL patches, runs single-threaded; the
adapter's own queue drives every async continuation (no runtime event loop,
no blocking waits). It speaks sts2-cli's line-based JSON protocol, extended
with `steps`: every intermediate state change (damage, block, card moves,
powers, intents) and every *UI cue* the game logic issued (sound, VFX, speech,
music parameters, through the `ui` patch), in the order the game produced them.
The same adapter runs under JIT .NET (fast iteration, the oracle), NativeAOT
(AOT smoke test), wasmtime, the w2c2 PC host, and the console.

**Presentation** (`pres/`, `platform/`). Native C++ in the XDK dialect. It
never decides game outcomes; it shows decision points, sends the player's
choices, and animates `steps`. Screens are rebuilt from the game's own scene
files (layout data the converter exports), so positions, textures and text
come from the real game.

**Converter** (`converter/`, Python). Reads the player's `.pck`, writes one
big-endian pack the console streams directly, and the data folder the logic
reads (localization tables).

## Milestones

Each milestone ends with a check that cannot be faked. "User" marks steps
that need the XDK, Xenia, hardware or the dotnet-experimental feed.

| | Milestone | Gate |
|---|---|---|
| M0 | **Toolchain spike** (user) | `xdk/` probe and test XEXs build; Xenia + hardware run them; record dialect results in CODING_RULES.md, Xenon/PC speed ratio (coremark via w2c2), and whether >32 MB of code links |
| M1 | **Logic pipeline on PC** | JIT and NativeAOT builds play 25/25 random runs (done for JIT + AOT smoke); user builds `logic.wasm`; import audit clean; w2c2 PC host plays the same seeds with byte-identical traces |
| M2 | **Logic on the console** | `xdk/logic_runner` XEX plays a scripted run in Xenia and on hardware; startup < 30 s, typical action < 100 ms, linear memory < 200 MB |
| M3 | **Converter** | Detects the pinned build; decodes every texture; exports layouts, Spine, audio, fonts, localization; PC viewer browses the pack |
| M4 | **Platform layer** | Sprite batcher, Xenos texture tiling, SDF text, XInput, XAudio2, streaming; test scenes match the PC build in Xenia and on hardware |
| M5 | **T0 playable** | Every decision type has a screen; a full run is playable with a controller on hardware; saves and continue work |
| M6 | **Coverage** | Every screen the game's AutoSlay bot knows (list in DECISIONS D7) is supported; no adapter shortcuts left (rewards chosen by the player, treasure choice, Crystal Sphere, card-reward alternatives); 1,000 seeded random runs complete |
| M7 | **Faithful presentation** | T1 for all screens (layouts, tweens, intents, Spine idle/hit); T2 for key animations; every UI cue mapped to audio/VFX |
| M8 | **Performance and soak** | 60 fps in the heaviest combats on hardware; 24 h soak clean |
| M9 | **Done** | Setup guide works end to end for a fresh install of the pinned build |

M3 and M4 can run in parallel with M1/M2.

## Logic: how fidelity is kept

- The adapter never re-implements game rules. It calls the same entry points
  the game's UI calls (`PlayCardAction`, `PlayerCmd.EndTurn`,
  `EventOption.Chosen`, merchant entries, rest-site synchronizer, ...).
- Every IL patch is listed with its reason in `logic/patches.json`, and the
  patcher's report records what it changed. Patches only remove engine/UI
  dependencies; none changes a rule. The one rule-changing patch inherited
  from sts2-cli (Neutralize) is disabled.
- Flow that the real game drives from UI nodes is the adapter's
  responsibility, and each place is a known fidelity risk tracked in
  DECISIONS D6 (e.g. between-act healing, reward collection, treasure).
- Determinism: same seed + same commands = same trace, compared across builds
  with `tools/random_runs.py --trace-dir` (JIT vs w2c2 host vs console).

## Presentation tiers

| Tier | Adds |
|---|---|
| T0 | Static sprites from the real assets, instant moves, text feedback; whole game playable |
| T1 | Real layouts from scenes, tweened card movement, damage numbers, intents, focus, Spine idle/hit, UI-cue audio |
| T2 | Full Spine sets, VFX approximations driven by UI cues, transitions, screen shake |

Every screen reaches T0 before any screen gets T2.

## Console budget (to be replaced by measurements in M2/M8)

| | MB |
|---|---|
| XEX code (game logic as native PPC + engine) | 40-80 (measure in M0/M2) |
| Logic linear memory (GC heap, statics) | 120-200 |
| Textures resident + card-art cache | 80 |
| Spine, fonts, audio, layouts | 40 |
| Headroom | rest of ~480 |

## Risks

| Risk | Mitigation |
|---|---|
| XDK linker cannot place a very large code image (PPC branch range, XEX limits) | M0 big-code link test before anything else; fallbacks: size-optimised ILC + `wasm-opt -Oz`, trimming the game's UI namespaces from the wasm, splitting cold code |
| Xenon too slow for the logic | M0 coremark ratio + M2 timings; logic runs on its own HW thread so frames never wait; the protocol is asynchronous |
| NativeAOT-LLVM is experimental and moves | Pin the package version that works (`--ilc-version`); the patched IL also builds with stock NativeAOT, which bounds what can break |
| Game update changes internal APIs | Pinned build; regeneration tools (stubs, patch report, API dump, regression) make drift visible in minutes |
| Adapter flow differs from the UI's flow | D6 list; replace shortcuts by driving the game's own handlers, using the AutoSlay handlers as the map of screens |
| Memory over budget | Measure early (M2); streaming; per-category texture scale |
| Spine runtime licensing | The player's build only; see THIRD_PARTY.md before integrating spine-c |
