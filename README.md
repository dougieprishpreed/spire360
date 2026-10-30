# Spire360

> **This branch (`xdk`) is no longer maintained.** It holds the build made
> with Microsoft's Xbox 360 XDK (Visual Studio 2010), as it stood when the
> work moved to the open [xsdk](https://github.com/xenosworks/xsdk)
> toolchain. Development continues on the
> [`main`](https://github.com/dougieprishpreed/spire360/tree/main) branch,
> which has the current build, its instructions and its fixes.

An unofficial Xbox 360 port of **Slay the Spire 2**, for people who own the
game on PC.

Spire360 does not re-create the game. It takes **your installed copy** and
turns it into something the Xbox 360 can run:

- **The game itself** (its rules, cards, monsters and random number
  generator) is your copy's own code, translated to run natively on the
  Xbox 360's processor. A run with a given seed plays out exactly as it does
  on PC.
- **The graphics** are drawn by a new engine that reproduces the game's own
  screens: the same layouts, art, fonts, animations and effects, converted
  from your copy's game files.

Nothing from the game is in this repository. Everything the tools produce is
made from your copy of the game, so **keep your builds to yourself**. This
project is not affiliated with or endorsed by Mega Crit.

## Status

Work in progress: **a first preview, playable with a controller.**

- A whole run plays: main menu, character select, Neow, the map, fights,
  rewards, events, rest sites, shops, treasure rooms, bosses and the game
  over screen, drawn as the game draws them. Seeded runs play out exactly as
  on PC.
- Combat plays out as in the game: attack and hurt animations, the
  effects, damage numbers, deaths, the turn banners; cards show their
  keyword tips.
- Not done yet: sound, saving, the Crystal Sphere minigame's own screen,
  some background shaders, multiplayer. Tested with Slay the Spire 2
  v0.111.0.
- Controls: [xdk/README.md](xdk/README.md#spire360-the-game).

## What you need

- Slay the Spire 2 on a Windows PC.
- An Xbox 360 that can run homebrew (RGH/JTAG), or the Xenia emulator.
- To build:
  - Python 3.9 or newer (`pip install -r converter/requirements.txt`);
  - the .NET 10 SDK and wasi-sdk 29 (to build the game logic);
  - Visual Studio 2010 with the Xbox 360 XDK. The XDK is not provided here:
    Microsoft licensed it to registered developers only.

## Building

1. **Convert the game's files** into a pack the console reads:

   ```
   python converter\convert.py fetch-godot
   python converter\convert.py build "C:\...\Slay the Spire 2\SlayTheSpire2.pck" out
   ```

   This writes `out\pack.s3p` (about 700 MB) and `out\data\localization`.
   `fetch-godot` downloads the official Godot 4.5.1 build once; it renders
   the game's fonts for the console.

2. **Build the game logic** from your copy's code:
   [docs/LOGIC_BUILD.md](docs/LOGIC_BUILD.md).

3. **Build the Xbox 360 programs** from the "Xbox 360 Command Prompt":
   [xdk/README.md](xdk/README.md). Today these are test programs: one draws
   the game's screens, the other runs the game's logic.

## Repository

| Folder | What |
|---|---|
| `converter/` | Reads the game's `.pck` and builds the console pack (textures, scenes, fonts, animations) |
| `logic/` | Prepares the game's code for the console: Godot stand-ins, patches, a command interface |
| `runtime/` | Runs the translated game code: memory, files, clocks |
| `engine/` | The console engine: scenes, layout, text, particles, Spine animations, renderers |
| `xdk/` | Xbox 360 projects (Visual Studio 2010 + XDK) |
| `tools/` | Build scripts, a PC scene viewer, test runners |
| `third_party/` | w2c2, the WebAssembly-to-C translator (MIT) |

Third-party code and licenses: [THIRD_PARTY.md](THIRD_PARTY.md).
