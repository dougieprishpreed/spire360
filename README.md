# Spire360

An unofficial Xbox 360 port of **Slay the Spire 2**, built on your PC from your own copy of the game.

The game's rules, cards and monsters are your copy's own code, translated for the Xbox 360's processor; the art,
sound and text are converted from your copy's files.

> **Built with [xSDK](https://github.com/xenosworks/xsdk) and [vulkan4xenon](https://github.com/xenosworks/vulkan4xenon).**
> Building a game for the Xbox 360 used to mean Microsoft's XDK, licensed only to registered developers and not
> yours to share; Spire360's own first builds used it. xSDK is a new, open-source Xbox 360 SDK written from scratch
> from public reverse engineering, with no Microsoft tooling and no leaked SDK material. Its own GCC toolchain builds
> titles that run on the console's retail kernel and dashboard, and its GPU layer drives the console's Xenos GPU
> directly. vulkan4xenon puts the Vulkan graphics API on top of it, which Spire360's renderer is written in. Together
> they make a full modern game buildable for the Xbox 360 by anyone, from source, on their own PC: that's what makes
> the one-click Setup below possible.

## Status

**Fully playable** on an Xbox 360 that runs homebrew, with a controller: whole runs from the main menu to the last
boss, with the game's music and sound, saves, and Save and Quit. Seeded runs play out exactly as on PC.

Expect some bugs in the game's logic or its screens. Please report them in
[Issues](../../issues), with the two logs and a screenshot (see [Reporting a bug](#reporting-a-bug)). Not done yet:
multiplayer, custom and daily runs.

## What you need

- Slay the Spire 2 **v0.111.0**, installed on a Windows 10 or 11 PC (64-bit). It's the only version tested so far;
  later game updates will get matching Spire360 updates.
- An Xbox 360 that runs homebrew (such as with ABadAvatar), and a way to copy files to it (USB drive or FTP).
- About **20 GB free** on the PC's drive you build on (the Windows drive holding the Spire360Setup folder, not the
  console's). The Spire360Setup folder ends up at about **14 GB**: the folder for the console, about **1 GB**, and
  the build environment, which you can keep to build future updates (or remove with **Reset...**).
- An internet connection, and about an hour for the first build. With **Low CPU use** ticked (the default) Setup
  compiles with two CPU threads so the PC stays usable meanwhile; untick it to use every core, which is quicker. Later
  builds only redo what changed and are much quicker.

## Building it (Windows)

1. Download the **Spire360Setup** zip from [Releases](../../releases) and extract it where there's room, such as
   `D:\`. Everything the build needs goes into its `Spire360Setup` folder.
2. Run **Spire360Setup.exe** from that folder, click **Choose folder...** and pick the Slay the Spire 2 folder (in
   Steam: right-click the game, *Manage*, *Browse local files*). Then click **Build**.
3. Wait: the window shows each step as it goes. If WSL (Windows Subsystem for Linux) isn't installed yet, Setup
   offers to install it; restart the PC when it asks, then run Setup again.

When it finishes, the `spire360` folder next to Setup is ready for the console.

To carry on from your progress on PC, click **Import...** under *Your saves* once the build is done: Setup finds the
game's saves on this PC (choose the account if there's more than one) and copies them into the `spire360` folder.
Copying that folder to the console then replaces Spire360's saves there. The next build takes the imported saves out
of the folder again, so copying an update onto the console never puts them back over what you've played since.

Building on Linux or macOS instead: [docs/BUILD_LINUX_MAC.md](docs/BUILD_LINUX_MAC.md).

## Putting it on the console

Copy the whole `spire360` folder to the console, for example to `Hdd1:\Games\spire360` or a USB drive's
`Games\spire360`, and start `default.xex` from your dashboard (Aurora, FSD, ...).

## Updates

Run **Spire360Setup.exe** again. It lists what has changed since your build; **Update and rebuild** builds the
latest. Copy the new `spire360` folder over the old one: your saves (`spire\data\user`) and your `spire360.ini`
are kept.

To start again from nothing, or to free the space, click **Reset...** in Setup: it removes the build environment
and everything else Setup made in its folder, and leaves Spire360Setup.exe.

## Controls

- D-pad or left stick: move. A: confirm. B: back. Start: pause.
- Y: the screen's proceed arrow (Skip, Proceed) and End Turn.
- X: potions. LB: your deck. In a fight, LT, RT and RB: the draw, discard and exhaust piles.
- Back: the map.
- In a fight: A takes a card, A again plays it (left and right to aim), B puts it back.
- **Back + RB: a screenshot** (`spire360_shot_NN.bmp`, beside `default.xex`). Back + Start: quit.

## Reporting a bug

Each start writes `spire360.txt` and `spire360_logic.txt` beside `default.xex`. Open an [issue](../../issues) saying
what happened and what you expected, and attach both logs and a screenshot (Back + RB) if it's something you can
see. The logs let a session be replayed exactly as it was played.

Options for testing and performance reports are in `spire360.ini` beside `default.xex`.

## For developers

How the port works and how to build each part by hand: [docs/DEVELOPING.md](docs/DEVELOPING.md).

## Keep your build to yourself

The build is made from your copy of the game: `default.xex` holds the game's own code, translated, and the pack its
art, sound and text. That's why nothing from the game is in this repository and why everyone builds their own:
don't share your `spire360` folder or anything the build makes. This project is not affiliated with or endorsed by
Mega Crit.

## Acknowledgements

- **Mena**, for [xSDK](https://github.com/xenosworks/xsdk), [vulkan4xenon](https://github.com/xenosworks/vulkan4xenon)
  and [glslang-xenon](https://github.com/xenosworks/glslang-xenon), and the console research behind them: Spire360
  runs on them.
- **Mega Crit**, for Slay the Spire 2.
- The projects xSDK builds on: Xenia, libxenon and xost.
- **Hao Wu**, for sts2-cli, which the headless driver of the game's logic started from.
- **Bastian Mueller**, for w2c2, which translates the game's logic to C.
- The **Godot Engine** contributors, whose layout, text, particle and shader code the engine reproduces.
- The .NET team's NativeAOT-LLVM and the WebAssembly community's wasi-sdk, which compile the game's logic.
- **Sean Barrett**, for stb_vorbis, which plays the game's music and sound.

Everything else Spire360 uses is listed in [THIRD_PARTY.md](THIRD_PARTY.md).
