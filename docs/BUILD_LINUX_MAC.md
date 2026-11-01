# Building Spire360 on Linux or macOS

This guide is written to be followed step by step, by a person or by an AI agent working in a terminal on their
behalf. Each step says what to run, what success looks like, and what to do when it fails. Windows users want the
installer in the [README](../README.md) instead.

**Rules for whoever follows this:**

- The output is made from the user's own copy of Slay the Spire 2. Never upload, share or commit it (the `spire360`
  folder, `pack.s3p`, `default.xex`, anything under the build home), and don't push anything to the repository.
- Ask the user before installing system packages if you don't have their permission to use `sudo`.
- Don't change files in the repository checkout; everything the build makes goes under the build home and the output
  folder.

## What it needs

- **Linux, x86-64**: Debian 12 or 13, or Ubuntu 22.04 or later. Other distributions work with the same packages under
  their own names (see `stage_packages` in `tools/install/build_all.sh`).
- **macOS**: not built on directly. Use a Debian or Ubuntu x86-64 Linux environment: a VM (UTM, Parallels, VMware
  Fusion), or Lima or OrbStack. On Apple Silicon, give it x86-64 with Rosetta (OrbStack's and Lima's `--arch x86_64`
  with Rosetta enabled); emulation without Rosetta works but is many times slower. Then follow the Linux steps inside
  it. macOS has not been tested: report what happens in an issue.
- **Windows without Setup**: the Linux steps work inside a WSL 2 instance of Debian or Ubuntu (`wsl --install -d
  Debian`, or one already there). The game's folder is under `/mnt/c/...` (for Steam's default library,
  `/mnt/c/Program Files (x86)/Steam/steamapps/common/Slay the Spire 2`); give `--out` a Windows folder such as
  `/mnt/c/spire360-out` to find the output in Explorer. Keep the build home in Linux's own disk (the default), which
  is many times quicker than `/mnt/c`.
- About **20 GB free** for the first build (it ends at about 13 GB, the output folder about 1 GB of it), 8 GB of RAM
  or more, and an internet connection.
- The game: the Slay the Spire 2 folder holding `SlayTheSpire2.pck`, wherever the user keeps it. Ask the user for it;
  don't search the disk. Inside a VM, the user copies or shares that folder in. Version **v0.111.0** is the only one
  tested (its `release_info.json` says so); with another, tell the user it's untested before going on.

## 1. Get the source

```
git clone https://github.com/dougieprishpreed/spire360.git ~/spire360
cd ~/spire360
git log -1 --format='%h %s'
```

Success: the last line prints a commit. Failure "Repository not found" or an authentication prompt: the repository is
private; ask the user for access (a GitHub token, used as `https://<token>@github.com/...`).

## 2. Find the game folder

```
GAME="<the Slay the Spire 2 folder>"
ls -la "$GAME/SlayTheSpire2.pck"
```

Success: the file is listed. Failure: ask the user again; the folder is the one holding `SlayTheSpire2.pck`, next to
the game's executable.

## 3. Run the build

```
tools/install/build_all.sh --game "$GAME" --out ~/spire360-out 2>&1 | tee ~/spire360-build.log
```

It prints each stage as it starts:

```
##spire stage 1 11 packages Installing the system packages
##spire stage 2 11 sources Fetching xsdk, vulkan4xenon and glslang
##spire stage 3 11 toolchain Building the Xbox 360 compiler (the longest step the first time)
...
##spire stage 11 11 stage Putting the console folder together
##spire done /home/<user>/spire360-out
```

The first run takes about an hour, mostly the compiler (stage 3) and the logic (stages 9 and 10). The system
packages stage runs `sudo apt-get`: pass `--no-packages` if they're installed already (the list is in
`stage_packages`). Other options: `--home DIR` puts the toolchain and the work elsewhere (default
`~/spire360-build`), `--jobs N` sets the compilers and workers run at once (default 2, so the machine stays usable;
more is quicker if it has the cores: it keeps to 1 GB of memory for each).

**Success** is the `##spire done` line and, in the output folder:

```
ls -la ~/spire360-out/default.xex ~/spire360-out/spire/pack.s3p ~/spire360-out/spire360.ini
cat ~/spire360-out/build.json
```

`default.xex` (about 90 MB), `spire/pack.s3p` (about 1 GB), `spire360.ini`, `spire/data/`, and `build.json` naming
the source commit and the game version.

**On failure** the script stops with `##spire fail <stage> <log>` and the log's last 40 lines. The full log is
`~/spire360-build/logs/<stage>.log`. Fix what it names and run the same command again: finished stages are quick to
pass through (the compiler, .NET and wasi-sdk stay installed). Known causes:

| Stage | Cause | Fix |
|---|---|---|
| packages | no `sudo` rights | ask the user to install the packages, then `--no-packages` |
| toolchain | out of disk or memory | free space (7 GB while it builds); `--jobs 1` |
| dotnet, wasi, sources | no network, or a proxy | check `curl -I https://github.com` |
| pack | the `.pck` isn't the game's | check `$GAME`; the game must be a supported version |
| logic | "BUILD_LOGIC FAILED in stage ..." | the log names the stage; a game update can need a new Spire360 version (`git pull`) |
| xex | GCC killed (out of memory) | `--jobs 1` |

## 4. Put it on the console

Copy the whole output folder to the console as `Games\spire360` (a USB drive formatted FAT32 or the console's hard
disk via FTP), and start `default.xex` from the dashboard. Tell the user where the folder is; don't copy it anywhere
else.

## Updates

```
cd ~/spire360 && git pull
tools/install/build_all.sh --game "$GAME" --out ~/spire360-out --no-packages
```

Only what changed is built again. The saves (`spire/data/user`) and `spire360.ini` in the output folder are kept.

## Removing it

Delete `~/spire360-build` (the toolchain and the work, most of the space), the output folder once it's on the
console, and the checkout.
