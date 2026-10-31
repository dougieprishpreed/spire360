# Spire360

An unofficial Xbox 360 port of **Slay the Spire 2**, built on your PC from your own copy of the game.

The game's rules, cards and monsters are your copy's own code, translated for the Xbox 360's processor; the art,
sound and text are converted from your copy's files. Nothing from the game is in this repository, and nothing the
build makes may be shared: **build it yourself, keep it to yourself.** This project is not affiliated with or
endorsed by Mega Crit.

## Status

**Fully playable** on an RGH or JTAG console with a controller: whole runs from the main menu to the last boss, with
the game's music and sound, saves, and Save and Quit. Seeded runs play out exactly as on PC.

Expect some bugs in the game's logic or its screens. Please report them in
[Issues](../../issues), with the two logs and a screenshot (see [Reporting a bug](#reporting-a-bug)). Not done yet:
multiplayer, custom and daily runs.

## What you need

- Slay the Spire 2 installed on a Windows 10 or 11 PC (64-bit).
- An Xbox 360 that runs homebrew (RGH or JTAG), and a way to copy files to it (USB drive or FTP).
- About **30 GB free** on the drive you build on, for the first build. The finished folder for the console is
  about **1 GB**; the rest is the build environment, which you can keep to build future updates (or delete).
- An internet connection, and an hour or two for the first build (later ones are much quicker).

## Building it (Windows)

1. Download **Spire360Setup.exe** from [Releases](../../releases) into an empty folder, such as `D:\Spire360`.
   Everything the build needs goes into that folder.
2. Run it, click **Choose folder...** and pick the Slay the Spire 2 folder (in Steam: right-click the game,
   *Manage*, *Browse local files*). Then click **Build**.
3. Wait: the window shows each step as it goes. If WSL (Windows Subsystem for Linux) isn't installed yet, Setup
   offers to install it; restart the PC when it asks, then run Setup again.

When it finishes, the `spire360` folder next to Setup is ready for the console.

Building on Linux or macOS instead: [docs/BUILD_LINUX_MAC.md](docs/BUILD_LINUX_MAC.md).

## Putting it on the console

Copy the whole `spire360` folder to the console, for example to `Hdd1:\Games\spire360` or a USB drive's
`Games\spire360`, and start `default.xex` from your dashboard (Aurora, FSD, ...).

## Updates

Run **Spire360Setup.exe** again. It lists what has changed since your build; **Update and rebuild** builds the
latest. Copy the new `spire360` folder over the old one: your saves (`spire\data\user`) and your `spire360.ini`
are kept.

To remove everything, run `wsl --unregister Spire360` in a command prompt and delete the folder.

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

## License

Spire360's own code is under the MIT license ([LICENSE](LICENSE)). Third-party code keeps its own licenses:
[THIRD_PARTY.md](THIRD_PARTY.md).
