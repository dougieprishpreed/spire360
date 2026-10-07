# Spire360

An unofficial Xbox 360 port of Slay the Spire 2 for people who own the game.
It runs the game's **own logic**, built from your installed copy, and draws
your own copy's art, text and sound with a new native engine.

Nothing from the game is in this repository, and builds made with it contain
your copy of the game: keep them to yourself. Not affiliated with Mega Crit.

Status: early. The real game logic (v0.111.0) runs headless on PC through the
single-threaded adapter and plays full runs for all five characters; the wasm
runtime passes its tests on big-endian PowerPC. No console build yet. See
[HANDOFF.md](HANDOFF.md).

| Folder | What |
|---|---|
| `logic/` | C# adapter around the game assembly, Godot stubs, IL patcher, API tools |
| `runtime/` | C89 host for the w2c2-translated logic: WASI, pipes, memory, threads |
| `third_party/w2c2` | WebAssembly-to-C translator (MIT), with documented patches |
| `converter/` | Python: reads the game's `.pck` (more to come) |
| `tools/` | build pipeline, regression runner, import audit |
| `xdk/` | Xbox 360 test projects for the XDK (M0) |
| `docs/` | [PLAN](docs/PLAN.md), [DECISIONS](docs/DECISIONS.md), [LOGIC_BUILD](docs/LOGIC_BUILD.md) |
