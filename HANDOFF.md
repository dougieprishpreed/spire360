# Handoff

Kept short and rewritten each session. The full plan is being written to
`docs/PLAN.md`.

## Direction

Run the real game logic instead of re-implementing it:

    player's sts2.dll --Spire.Patcher--> patched IL
      --NativeAOT-LLVM (wasi-wasm, preview1 core module, emulated EH)--> logic.wasm
      --w2c2--> C89 --XDK cl.exe--> linked into the XEX with runtime/ + engine/

The logic runs on its own hardware thread and talks the sts2-cli JSON protocol
over in-memory stdin/stdout pipes. Presentation (engine/, not started) is
native C++ drawing the player's converted assets.

## Done and verified in this sandbox

- `runtime/`: WASI p1 + CoreLib's p2 clock/poll subset, pipes, guest thread,
  virtual timers, reserve/commit memory. `runtime/tests/run_tests.py` passes on
  x86-64, big-endian PowerPC (qemu) and Win32 (wine).
- `tools/w2c2_imports.py`: import audit + trapping stubs (tested).
- `logic/Spire.Patcher`: Cecil IL patcher, all actions tested on a mock
  assembly (`logic/tests/run_patcher_test.sh`), and the patched IL passes
  NativeAOT x64.

## Written but not yet compiled against the real game

- `logic/Spire.Logic`: single-threaded port of sts2-cli's RunSimulator, hooks,
  selector with nested decisions, animation step recorder.
- `logic/patches.json`, `tools/build_logic.py`.

## Next

1. Get the player's game DLLs into the session (not into git), compile
   Spire.Logic, run the smoke stage and sts2-cli's random-agent regression.
2. Plan docs, XDK M0 spike, asset converter, engine.
