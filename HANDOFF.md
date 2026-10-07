# Handoff

Rewritten each session; kept short. Plan: [docs/PLAN.md](docs/PLAN.md).
Why things are the way they are: [docs/DECISIONS.md](docs/DECISIONS.md).

## State (pinned game build v0.111.0)

**Logic, on PC: working.** The player's sts2.dll runs headless through
`logic/Spire.Logic` on one thread:

- `tools/random_runs.py`: 25/25 random runs complete (all five characters);
  with `--tank` the agent reaches Act 3 and wins full runs.
- NativeAOT x64 build of adapter + game runs (all 1,660 models register
  through reflection without JIT): strong evidence for NativeAOT-LLVM.
- `selfplay` command (agent inside the logic): PC JIT baseline: start_run
  2.1 s, action avg 1.8 ms / p95 6 ms / max 119 ms, GC heap 15 MB.
- Responses carry `steps`: state diffs (hp, block, powers, card moves,
  intents, orbs) and UI cues (sounds, VFX, speech, music, `wait` pacing) in
  game order.

**Runtime: verified.** `runtime/tests/run_tests.py`: x86-64, big-endian
PowerPC (qemu), Win32 backend (wine).

- Runs on the player's real localization tables (from their pck): 25/25 and
  10/10 `--tank`; UTF-8 protocol verified with Japanese tables.

**XDK spike (M0): done (user's XDK, run in Xenia).**
- hello: all checks pass (big-endian, `_MSC_VER` 1600, `lwbrx` incl.
  unaligned, 64 MB reserve+commit, D3D device).
- bigcode: 64 MB of code (37,400 functions) links and runs: PASS. The build
  took 2.5 h on one core; `/MP` is now on.
- probe: VS2010 PPC compiler, strict C89 (docs/CODING_RULES.md).
- selftest: PASS (after the stat-by-handle fix).
- bench: all checks identical to PC; 2.40 s vs 0.63 s on the PC (3.8x), in
  Xenia (not hardware).

**Logic as wasm (M1): done and verified.** The whole pipeline runs in
this environment now (network open; wasi-sdk 29 in the scratchpad, set
WASI_SDK_PATH): `build_logic.py --stage patch,wasm,c,native`.
- 25 seeded runs on x86-64 native and 25 on big-endian PowerPC (qemu-ppc,
  `--native-cc powerpc-linux-gnu-gcc --native-tag ppc`) give traces
  byte-identical to the .NET JIT build. The user's PC: native build plays a
  full run too.
- Code size: 33.5 -> 27.8 MB (wasm code), logic.wasm 46.8 MB, C 279 MB,
  x86 binary 75 MB; PPC text was 119 MB before the cut (64 MB proven to
  link on the 360). Cuts: `ui.strip` (Godot UI bodies -> throw) and
  `mods.no_harmony.*`. Slimmer build re-verified: 20/20 native traces
  identical when paused (PPC rebuild of it was in progress).
- Next size ideas: `IlcOptimizationPreference=Size` (measure speed too);
  shared throw helper for stripped bodies.

**M2 (console logic run): ready to build.** `xdk\build.cmd logic_runner`
after `build_logic.py` (stages up to `c`), with `data\localization` copied
to `<xex folder>\spire360_logic\data\localization`. Writes the selfplay
timing report to `game:\spire360_logic\result.txt`. Not yet run; expect a
long XDK compile (279 MB of C; /MP on).

**Converter (M3): reads everything, builds the pack; needs the user's pack
to run at full scale.** `converter/convert.py survey|texture|build`
(requirements.txt). Decoders tested against Godot 4.5.1 output (12 tests).
Not yet: fonts -> glyph atlases, FMOD banks, Spine runtime on the console.

**Converter (older notes):** `converter/pck_tool.py` reads Godot 4 packs; works on
the player's SlayTheSpire2.pck (v3, Godot 4.5.1, 15,890 files). What the pack
holds, for the converter:
- textures: 3,555 `.ctex` under `.godot/imported`: 2,311 plain (PNG/WebP
  inside, 294 MB), 540 S3TC (207 MB, usable as DXT after a byte swap), 704
  BPTC/BC7 (671 MB: the 360 has no BC7, decode and re-encode to DXT).
- scenes are text `.tscn` (1,037) and `.tres` (2,296), not binary RSRC.
- localization: `localization/<lang>/*.json`, 3-letter folders (eng, deu,
  jpn, ...): the layout the logic reads.
- Spine (164 `.spskel` / 170 `.spatlas`), 12 FMOD banks (301 MB), 20
  `.fontdata` (371 MB, mostly CJK), 100 shaders.

## Waiting on the user

1. Nothing blocking. Later: `xdk\build.cmd logic_runner` in Xenia (M2), and
   hardware runs when the results are needed.

## Next (agent)

- User: `convert.py survey <pck> --out survey.txt` and send it (texture
  budget, scene parse check on all 1,037 scenes), then `build`.
- Engine skeleton (task 6): presentation core + PC viewer of pack.s3p.
- M2 runner: `xdk/logic_runner` from `out/logic/c` (selfplay on the console,
  timing to the result file), after the native PC run works.
- Fidelity gaps in DECISIONS D6 (rewards screen choice first).

## Known issues

- Fidelity gaps D6 (auto-collected rewards, treasure auto-pick, healing
  between acts re-implemented, rest site forced to map).
- `InitProgressData` warns once on a fresh profile (harmless; reloaded after
  ModelDb).
