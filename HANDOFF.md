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

**XDK spike (M0): mostly answered (user's XDK + Xenia).**
- hello: all checks pass (big-endian, `_MSC_VER` 1600, `lwbrx` incl.
  unaligned, 64 MB reserve+commit, D3D device).
- bigcode: 64 MB of code (37,400 functions) links and runs: PASS. The build
  took 2.5 h on one core; `/MP` is now on.
- probe: VS2010 PPC compiler, strict C89 (docs/CODING_RULES.md).
- selftest/bench: round 2 failed on `<intrin.h>` (w2c2's MSVC atomics) and
  would next have hit MSVC 2010's missing C99 math; both fixed (PATCHES.md
  5, 6, 7). Waiting for round 3.

**Logic as wasm (M1): built on the user's PC.** `logic.wasm` 50.7 MB core
module -> 77 C files. The first build had 1,473 "will always throw" methods
from stub metadata mismatches; fixed (DECISIONS D9). Imports: 35 missing are
`wasi:http/*` + `wasi:io/streams|error` (HTTP client, never called; trap
stubs) and `fd_advise` (implemented; its symbol length was wrong).

**Converter: started.** `converter/pck_tool.py` reads Godot 4 packs; works on
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

1. M0 round 3: `xdk\build.cmd` (selftest, bench); run them in Xenia.
2. M1 again (stub fixes): `build_logic.py` all stages + `--stage native`, then
   `random_runs.py` against `out/logic/native/logic_host`.
3. Optional: upload `out/logic/logic.wasm` (zipped) so the translated logic
   can be tested here on x86 and big-endian PowerPC (qemu).

## Next (agent)

- Converter: textures (.ctex: plain, S3TC, BC7 -> DXT), text scene/resource
  parser (.tscn/.tres), pack writer.
- M2 runner: `xdk/logic_runner` from `out/logic/c` (selfplay on the console,
  timing to the result file), after the native PC run works.
- Fidelity gaps in DECISIONS D6 (rewards screen choice first).

## Known issues

- Fidelity gaps D6 (auto-collected rewards, treasure auto-pick, healing
  between acts re-implemented, rest site forced to map).
- `InitProgressData` warns once on a fresh profile (harmless; reloaded after
  ModelDb).
