# Spire360

An unofficial Xbox 360 port of **Slay the Spire 2**, for people who own the
game on PC.

Spire360 does not re-create the game. It takes **your installed copy** and
turns it into something the Xbox 360 can run:

- **The game itself** (its rules, cards, monsters and random number
  generator) is your copy's own code, translated to run natively on the
  Xbox 360's processor. A run with a given seed plays out exactly as it does
  on PC.
- **The graphics and sound** come from a new engine that reproduces the
  game's own screens and audio: the same layouts, art, fonts, animations,
  effects, music and sound effects, converted from your copy's game files.

Nothing from the game is in this repository. Everything the tools produce is
made from your copy of the game, so **keep your builds to yourself**. This
project is not affiliated with or endorsed by Mega Crit.

This branch (`main`) is the build made with the open
[xsdk](https://github.com/xenosworks/xsdk) toolchain and
[vulkan4xenon](https://github.com/xenosworks/vulkan4xenon). The earlier
build with Microsoft's XDK is kept on the `xdk` branch and is no longer
maintained.

## Status

Work in progress: **a preview, playable with a controller on an RGH or
JTAG console.**

- Whole runs play, drawn as the game draws them: the main menu, character
  select, Neow, the map, fights, rewards, events, rest sites, shops,
  treasure rooms, bosses, the game over screen. Seeded runs play out exactly
  as on PC.
- Music and sound effects play from the game's own sound banks.
- Runs save as the game saves them; Save and Quit, then Continue, picks the
  run up where it was.
- Not done yet: multiplayer, custom and daily runs. Tested with Slay the
  Spire 2 v0.111.0.
- xSDK builds don't run in the Xenia emulator: they are tested on PC first
  (below), then on a console.

## What you need

- Slay the Spire 2 on a Windows PC.
- An Xbox 360 that can run homebrew (RGH/JTAG).
- To build:
  - Python 3.9 or newer (`pip install -r converter/requirements.txt`);
  - the .NET 10 SDK and wasi-sdk 29 (to build the game logic);
  - Linux, or WSL on Windows, with xsdk's toolchain built (its
    `docs/GETTING_STARTED.md`) and checkouts of xsdk, vulkan4xenon and
    glslang-xenon.

## Building

1. **Convert the game's files** into a pack the console reads:

   ```
   python converter\convert.py fetch-godot
   python converter\convert.py build "C:\...\Slay the Spire 2\SlayTheSpire2.pck" out
   ```

   This writes `out\pack.s3p` (about 1 GB) and `out\data`. `fetch-godot`
   downloads the official Godot 4.5.1 build once; it renders the game's
   fonts for the console.

2. **Build the game logic** from your copy's code:
   [docs/LOGIC_BUILD.md](docs/LOGIC_BUILD.md).

3. **Build `default.xex`** in Linux or WSL:
   [xsdk/README.md](xsdk/README.md#build).

## Running it

Copy the converter's output folder (`pack.s3p` and `data\`) to a `spire`
folder on the console's drive, put `default.xex` beside it, and start it
from the dashboard:

```
Games\spire360\default.xex
Games\spire360\spire\pack.s3p
Games\spire360\spire\data\...
```

Options for testing go in `spire360.ini` beside the XEX (see
[xdk/spire360/spire360.ini](xdk/spire360/spire360.ini): `invincible`,
`godmode`, `trace_loads`, `profile`); without it every option is off.

### Controls

Any of the four controller slots plays.

- D-pad or left stick: move the focus. A: confirm. B: back. Start: pause.
- Y: the screen's proceed arrow (Skip, Proceed), and End Turn in a fight.
- X: the potions (A: drink, throw or discard).
- LB: your deck, on any screen of a run (a card reward and a card choice
  too). In a fight, LT, RT and RB: the draw, discard and exhaust piles.
- Back: the map (and the log written out).
- In a fight: A takes a card, A again plays it (left/right aim), B puts it
  back.
- Back + RB: a screenshot (`spire360_shot_NN.bmp`). Back + Start: quit.

### The logs

Each start writes `spire360.txt` (start-up, every decision and how long it
took, slow frames, memory, sound) and `spire360_logic.txt` (the game
logic's own output and the commands it was sent) beside the XEX, and copies
the saves it started from to `spire\data\session_start`. With those, a
session can be replayed on the PC exactly as it was played. Send them with
a report.

## Repository

| Folder | What |
|---|---|
| `converter/` | Reads the game's `.pck` and builds the console pack (textures, scenes, fonts, animations, sound banks) |
| `logic/` | Prepares the game's code for the console: Godot stand-ins, patches, a command interface |
| `runtime/` | Runs the translated game code: memory, files, clocks |
| `engine/` | The console engine: scenes, layout, text, particles, Spine animations, audio, renderers |
| `xsdk/` | The Xbox 360 program built with xsdk (its build: [xsdk/README.md](xsdk/README.md)) |
| `tools/` | Build scripts, PC tools (a scene viewer, a headless player, checks), test runners |
| `xdk/` | The earlier Visual Studio 2010 + XDK projects (no longer maintained) |
| `third_party/` | w2c2 (the WebAssembly-to-C translator) and stb_vorbis |

Third-party code and licenses: [THIRD_PARTY.md](THIRD_PARTY.md).
