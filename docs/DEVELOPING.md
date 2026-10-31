# Developing Spire360

How the port is put together, and how to build each part by hand. Players want the
[README](../README.md)'s installer instead.

## How it works

- **The game's logic** (rules, cards, monsters, the random number generator) is the player's own `sts2.dll`,
  patched to run without Godot, compiled to WebAssembly with NativeAOT-LLVM, then translated to C with w2c2 and
  compiled for the console's PowerPC. A run with a given seed plays out exactly as it does on PC.
- **The presentation** is a new engine (`engine/`) that reproduces the game's screens: its scenes, layouts, text,
  Spine animations, particles, shaders and FMOD sound banks, read from a pack converted from the player's `.pck`.
- **The console program** is built with the open [xsdk](https://github.com/xenosworks/xsdk) toolchain and
  [vulkan4xenon](https://github.com/xenosworks/vulkan4xenon). The earlier build with Microsoft's XDK is kept on the
  `xdk` branch and is no longer maintained.

## The whole build in one command

`tools/install/build_all.sh` runs every step on Linux (Debian or Ubuntu), from the game's folder to the console's:

```
tools/install/build_all.sh --game "<the Slay the Spire 2 folder>" --out <folder>
```

It installs the system packages (with `sudo`), fetches and builds the toolchain, .NET, wasi-sdk and the Python
packages under `--home` (default `~/spire360-build`), then converts the game's files, builds the logic and
`default.xex`, and stages the folder (`tools/install/stage.py`). The Windows installer (`installer/`) runs the same
script in a WSL instance of its own. Each step's log is in `<home>/logs`.

## Each step by hand

1. **Convert the game's files** into the pack the console reads:

   ```
   python converter/convert.py fetch-godot
   python converter/convert.py build "<game>/SlayTheSpire2.pck" <out>
   ```

   This writes `<out>/pack.s3p` (about 1 GB) and `<out>/data`. `fetch-godot` downloads the official Godot 4.5.1
   build once; it renders the game's fonts for the console. Python 3.9 or newer, with
   `pip install -r converter/requirements.txt`.

2. **Build the game logic** from your copy's code: [LOGIC_BUILD.md](LOGIC_BUILD.md).

3. **Build `default.xex`** (Linux or WSL): [xsdk/README.md](../xsdk/README.md#build).

4. **Stage the folder**: `python tools/install/stage.py --pack <out> --out <folder> --xex <default.xex>`.

The console folder:

```
spire360\default.xex
spire360\spire360.ini
spire360\spire\pack.s3p
spire360\spire\data\...
```

## Options and logs

`spire360.ini` beside the XEX (the staged one is `tools/install/spire360.ini`): `trace_loads` and `profile` for
performance work, `workers`, `fps`, and `invincible` and `godmode` for testing. Without the file every option is
off.

Each start writes `spire360.txt` (start-up, every decision and how long it took, slow frames, memory, sound) and
`spire360_logic.txt` (the logic's own output and the commands it was sent), and copies the saves it started from
to `spire\data\session_start`. With those, a session can be replayed on the PC exactly as it was played
(`spire_play --commands`).

## Testing on the PC

xSDK titles don't run in Xenia: changes are checked on the PC first. `tools/viewer/build.sh` builds the PC tools
(`spire_view` draws one scene or decision to a PNG, `spire_play` runs the game headless, `spire_check` replays
traces through the presentation), and `tools/` holds the regression runners (`random_runs.py`,
`presentation_scenarios.py`, `content_coverage.py`). [LOGIC_BUILD.md](LOGIC_BUILD.md) covers the logic's own
regression.

## Repository

| Folder | What |
|---|---|
| `converter/` | Reads the game's `.pck` and builds the console pack (textures, scenes, fonts, animations, sound banks) |
| `logic/` | Prepares the game's code for the console: Godot stand-ins, patches, a command interface |
| `runtime/` | Runs the translated game code: memory, files, clocks |
| `engine/` | The console engine: scenes, layout, text, particles, Spine animations, audio, renderers |
| `xsdk/` | The Xbox 360 program built with xsdk ([xsdk/README.md](../xsdk/README.md)) |
| `installer/` | The Windows installer (WPF, .NET Framework 4.8): `dotnet build -c Release installer` |
| `tools/` | Build scripts, PC tools (a scene viewer, a headless player, checks), test runners |
| `xdk/` | The earlier Visual Studio 2010 + XDK projects (no longer maintained) |
| `third_party/` | w2c2 (the WebAssembly-to-C translator) and stb_vorbis |
