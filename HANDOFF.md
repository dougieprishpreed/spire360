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

**XDK spike: ready to build (user).** `xdk/README.md` has the commands.

**Converter: started.** `converter/pck_tool.py` reads Godot 4 packs (tested
on a Godot 4.5 export).

## Waiting on the user

1. M0: build and run `xdk/` (hello, selftest, bench, bigcode, probe); send the
   result files and any build errors.
2. M1: on the PC with the dotnet-experimental feed and wasi-sdk 29:
   `python tools/build_logic.py --game-dir <install> --stage copy,patch,wasm,c`
   and send the output (wasm size, import audit).
3. `python converter/pck_tool.py info <SlayTheSpire2.pck>` output, plus
   `pck_tool.py list` (as a file) and a `pck_tool.py sample` zip if small
   enough to attach.

## Next (agent)

- Converter: localization extraction (unblocks running the logic from the
  player's own files), textures (.ctex), scenes (binary RSRC), pack writer.
- M2 runner: `xdk/logic_runner` generated from `out/logic/c` (selfplay on the
  console, timing to the result file).
- Fidelity gaps in DECISIONS D6 (rewards screen choice first).

## Known issues

- Fidelity gaps D6 (auto-collected rewards, treasure auto-pick, healing
  between acts re-implemented, rest site forced to map).
- `InitProgressData` warns once on a fresh profile (harmless; reloaded after
  ModelDb).
