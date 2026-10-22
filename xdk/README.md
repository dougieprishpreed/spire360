# Xbox 360 projects

Built with the Xbox 360 XDK and Visual Studio 2010:

- **spire360**: the game: the engine and the game's logic in one program
  (see below). Work in progress.
- **spire_view**: the engine drawing the game's scenes from your converted
  pack (see below).
- **logic_runner**: the game's own logic, translated from your copy, playing
  a seeded run and reporting its timings.
- **hello, selftest, bench, bigcode, probe**: small checks of the compiler,
  the runtime the logic runs on, its speed and very large code images.

Everything here builds with the XDK and Visual Studio 2010 (platform
"Xbox 360"). The translated C in `selftest/gen` and `bench/gen` is committed
(regenerate with `tools/gen_xdk_tests.py`); it passed on x86-64 and on
big-endian PowerPC under qemu before being committed.

## Build

Open the **Xbox 360 Command Prompt** and run:

```
cd /d F:\downloads\temp\python\hermes\spire360
xdk\build.cmd
```

That prompt sets up the XDK compiler but does not put `msbuild` on PATH;
`build.cmd` adds the .NET 4.0 MSBuild that Visual Studio 2010 uses
(`%WINDIR%\Microsoft.NET\Framework\v4.0.30319\MSBuild.exe`), builds hello,
selftest, bench and bigcode (generating the 64 MB of bigcode sources first;
needs `python` on PATH), runs the probe, and prints one line per project.
`xdk\build.cmd hello` builds one project. Build logs land in
`xdk\build_logs\<project>.log`.

By hand, the same thing is:

```
set PATH=%PATH%;%WINDIR%\Microsoft.NET\Framework\v4.0.30319
msbuild xdk\hello\hello.vcxproj /p:Configuration=Release /p:Platform="Xbox 360"
```

(Do not run `vsvars32.bat` in the same prompt: it puts the x86 `cl` ahead of
the XDK's PowerPC `cl`, which breaks the probe.)

`xdk\build_light.cmd` takes the same arguments but runs at most two
compiler processes at a time (`set SPIRE_JOBS=3` first for another cap)
instead of one per core, so the PC stays usable during the build.
logic_lib's longest file is one compiler process either way. Switching
between the two scripts changes the compiler command line, so logic_lib
compiles in full once.

Each XEX lands in `xdk\<project>\Release\`. If a build fails, send the log.
Library names in `xdk/common/spire360.props` (`xinput2.lib`, `xaudio2.lib`,
...) and the project keyword are best guesses from the XDK's conventions and
may need adjusting for your XDK version.

## Run and collect

Run each XEX in Xenia Canary (and on hardware when convenient). Each writes
its report next to itself (`game:\` is the XEX's folder in Xenia) and to the
debug output:

| XEX | Report | Pass when |
|---|---|---|
| hello | `game:\spire360_hello.txt` | all lines present; values match the "expect" lines; the screen cycles colour; START exits |
| selftest | `game:\spire360_selftest\result.txt` | last line ends in `PASS` |
| bench | `game:\spire360_bench\result.txt` | `check` values equal the PC ones below; send the `us` numbers |
| bigcode | `game:\spire360_bigcode.txt` | `PASS` (also note link time, XEX size, linker messages) |
| probe | `xdk\probe\probe_results.txt` | (informational: which language features compile) |

Reference bench output from this sandbox (x86-64 PC, same translated code):

```
strings     42173 us  check cb385040
sort       505092 us  check 7f645ccb
objects     23271 us  check 075862f1
math        54511 us  check 992414c544b3a77a
total      625047 us
```

The `check` values must match exactly on the 360; the time ratio gives the
Xenon/PC factor for translated code.

## logic_runner: the game logic on the console

Builds the translated game logic (`out/logic/c`, from
`tools/build_logic.py`, stages up to `c`) with the same runtime as selftest.
`out/logic/c` from before October 15 2026 needs its table initialiser
rewritten first (the stage does it now; otherwise the XDK compiler stops
with C1002 "out of heap space" on logic.c):

    python tools\w2c2_fix_tables.py out\logic\c\logic.c

Then:

```
xdk\build.cmd logic_runner
```

The runtime and the translated logic are compiled once into
`logic_lib\Release\logic_lib.lib`, which logic_runner and spire360 both
link. `build.cmd` builds it first and incrementally, so the hour of
compiling is not repeated for the other program or after a failed link;
`set SPIRE_REBUILD=1` first to start from scratch. The programs link
without debug information or identical-COMDAT folding (the 32-bit XDK
linker runs out of memory otherwise: LNK1000).

Copy the localization tables next to the XEX:
`<xex folder>\spire360_logic\data\localization\eng\*.json` (from `data\localization`).
It plays a seeded self-play session and writes the timing report (start-up,
per-action average/p95/max, GC heap) to `game:\spire360_logic\result.txt`.
A `spire360_logic\script.txt` (one JSON command per line) replaces the
default commands.

## spire360: the game

```
xdk\build.cmd spire360
```

Needs `out\logic\c` (as logic_runner) and a converted pack. Copy the
converter's whole output folder (`pack.s3p` and `data\`) to a `spire`
folder next to the XEX: `game:\spire\pack.s3p`, `game:\spire\data\...`.

The main menu: up/down between Singleplayer and Quit. Singleplayer opens
the character select: left/right choose the character, A embarks (a new
seed each time; the screen stays dimmed while the logic starts the run,
~10 s), B goes back. After a game over, A on Continue shows the run
summary, A on Main Menu returns to the menu.

Controls in a run: D-pad or left stick move the focus, A confirms, B goes
back, Y presses the proceed arrow (Skip / Proceed) where there is one.

- Combat: A takes a card (A again plays it, left/right aim, B puts it
  back), Y ends the turn, X opens the potions (A: drink, throw or discard).
- Rewards after a fight: up/down choose, A takes a reward (a card reward
  opens its own screen: A picks, B skips), Y skips the rest / proceeds.
- Rest site: left/right choose an option, A uses it; then Y goes to the map.
- Shop: A on the merchant opens the inventory (A buys, B closes it), Y
  leaves.
- Treasure: A opens the chest, A takes the relic (Y skips it), Y proceeds.
- Card selection screens: A picks, Y confirms, B cancels.
- Back: write the log now; Back + Start: quit; Back + Y: gamma ramp on/off.

The log is `game:\spire360.txt` (startup, each decision and how long the
logic took, frame times, any texture missing from the pack). Screens not
ported yet show the decision's choices as a list.

## spire_view: the engine drawing the game's scenes on the console

`xdk\build.cmd spire_view` builds `spire_view.xex`: the engine (layout, MSDF
text, particles, ported shaders) with the D3D9 renderer at 1280x720. Shaders
are compiled by the XDK's fxc (`%XEDK%\bin\win32\fxc.exe`) during the build.

It needs a pack converted at the console's scale, with fonts:

    python converter\convert.py fetch-godot          (once: Godot 4.5.1 renders the fonts)
    python converter\convert.py build <SlayTheSpire2.pck or bundle dir> out

Put `out\pack.s3p` in a `spire` folder next to the XEX (`game:\spire\pack.s3p`).
In Xenia, `game:` is the XEX's folder.

Pad: D-pad left/right = previous/next scene, A = game scripts on/off,
Back = write the log now, Start = quit. The log, `game:\spire360_view.txt`,
holds load and per-frame CPU times per scene. Send it with a screenshot.

## What each one tells us

- **hello**: compiler version and type sizes; whether `lwbrx` byte-reversed
  loads work on unaligned addresses (decides `W2C2_XBOX_BYTEREVERSE`);
  reserve-then-commit memory (what the runtime does for the .NET heap).
- **selftest**: the whole runtime (WASI files/dirs/clocks, pipes, guest
  thread on HW thread 2, memory growth, preview2 timers) compiled by the XDK
  compiler and run big-endian on the real CPU.
- **bench**: speed of w2c2 output on Xenon vs PC.
- **bigcode**: whether the XDK linker handles code beyond the +-32 MB branch
  range (the game logic will be tens of MB). Try `--mb 32`, `64`, `96`.
- **probe**: C and C++ features of the XDK compiler (results in docs/CODING_RULES.md).
