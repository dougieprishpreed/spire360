# Handoff

Rewritten at each change of session; read it first. Plan: [PLAN.md](PLAN.md).
Why things are the way they are: [DECISIONS.md](DECISIONS.md). Dialect rules:
[../docs/CODING_RULES.md](../docs/CODING_RULES.md).

Last rewrite: 2026-10-18, when the work moved from a cloud session (game
data reached it as zip "bundles") to a local desktop session on the user's
PC, with the game installed.

## Goal: the alpha release candidate

Port **everything in the game except multiplayer**, systematically, and keep
going until the build is an alpha release candidate: every screen,
effect, animation, transition and sound of a single-player run, as the game
shows them. The content is finite: work through the checklist below rather
than stopping after each feature for feedback. The user builds and tests
in Xenia only when the agent says the alpha RC is ready (or when the user
asks). **Never start an XDK build yourself unless the user asks for one.**
Use the PC tools (spire_view, spire_play, the JIT logic) to check the work.

## Rules (keep)

- **No game data in git, ever.** That covers the game's files and anything
  made from them: decompiled source, packs, extracted images, bundles, run
  traces (they hold game text), screenshots of the game. Local copies go in
  ignored folders (`work/`, `decomp/`, `traces/`, `out/`, `data/`,
  `logic/lib/`). Before every commit, `git status` and check that only
  source, tools and docs are staged.
- Engine and console code: **C++03 for Visual Studio 2010** (the XDK's
  compiler). `tools/viewer/build.sh` builds with `g++ -std=c++98
  -pedantic` and greps for C99 maths VS2010 lacks (`round`, `fminf`, ...;
  it also catches them in comments). Translated logic: C89.
- Logic changes must not change the game: run the seeded trace check below
  (0 differences) after every change under `logic/`.
- At most **one background job** at a time (usage quota).
- No model names or ids in commits, PRs, code or docs.
- Branch: `claude/peaceful-ritchie-zgh7h5`. No pull requests unless the
  user asks.
- Comments say what the game does and where (the C# class or scene being
  ported, its numbers), in the style of the surrounding code.

## The user's machine

| What | Where |
|---|---|
| Repository | `F:\downloads\temp\python\hermes\spire360` (was a zip extract; becomes a git clone at the start of the desktop session) |
| Game (v0.111.0) | `F:\Program Files (x86)\Slay the Spire 2` (`SlayTheSpire2.pck`, sts2.dll in its data folder) |
| wasi-sdk 29 | `C:\wasi-sdk-29.0-x86_64-windows` |
| XDK | Visual Studio 2010 + Xbox 360 XDK; "Xbox 360 Command Prompt"; `xdk\build.cmd` |
| Emulator | Xenia Canary; the XEX's log is `game:\spire360.txt`, Xenia's own log `xenia.log` |
| Also | Python, .NET 10 SDK, 7-Zip |

Build caches to keep (gitignored, slow to remake): `out\logic` (the
translated logic; `out\logic\w2c2_base.h.stamp`), `logic\lib`, `data`,
`xdk\logic_lib\Release` (the 47 min logic library), the converted pack.

## Local workflow

The PC tools need a POSIX shell and g++ for `tools/viewer/build.sh`: use
WSL (Ubuntu: `sudo apt install g++ python3 python3-pip`, .NET 10 SDK for
Linux, the repo at `/mnt/f/downloads/temp/python/hermes/spire360`) or
MSYS2. Pick one environment and run the logic, the converter and the tools
from it. Paths below are relative to the repository.

1. **Decompiled game source** (the reference for every port; never
   committed): `dotnet tool install -g ilspycmd`, then
   `ilspycmd -p -o decomp "<game>/<data folder>/sts2.dll"` (the folder that
   holds sts2.dll; `tools/build_logic.py` finds it the same way). Godot's
   own C++ (4.5.1, MIT) is the reference for engine behaviour.
2. **Game files**: read them straight from the .pck.
   `python converter/pck_tool.py extract <pck> work/res "<glob>"` pulls out
   scenes, scripts, shaders, images to read. No more bundles.
3. **Pack for the PC tools**: `python converter/convert.py build <pck>
   work/pack` (the full pack, ~700 MB, at the console's 2/3 scale; `--only
   'glob'` for a quick partial one, `--godot` / `fetch-godot` for fonts).
   It also writes `work/pack/data` (localization, `res_dirs.json`).
4. **Logic (JIT)**: `python tools/build_logic.py --game-dir <game> --stage
   copy,patch,jit` (after a change to `logic/patches.json`, `patch` must
   run before `jit`). Run: `dotnet out/logic/jit/Spire.Logic.dll` with
   `SPIRE_DATA=work/pack/data` (or `data`).
5. **Engine tools**: `sh tools/viewer/build.sh` gives
   `out/engine/spire_view` (a scene or a decision JSON to PNG; plays
   combat steps; viewer-only keys `previous_stage`, `previous_player`)
   and `out/engine/spire_play` (the whole game headless with the real
   logic: `spire_play work/pack/pack.s3p work/shots --logic "dotnet
   out/logic/jit/Spire.Logic.dll" --character Ironclad --seed S --steps N
   [--auto SEED]`, a PNG per screen). Compare the PNGs with the real game
   (run it, screenshot the same screen) and with the decompiled code.
6. **Logic regression check** (after every logic change):

   ```
   python tools/random_runs.py --cmd "dotnet out/logic/jit/Spire.Logic.dll" --runs 3 --trace-dir traces/base --data-dir work/pack/data   # once, before the change
   python tools/random_runs.py --cmd "dotnet out/logic/jit/Spire.Logic.dll" --runs 3 --trace-dir traces/new  --data-dir work/pack/data
   python tools/trace_compare.py traces/base traces/new        # must print "differences: 0"
   ```

   `trace_compare.py` ignores presentation-only keys (`steps`, `tips`, ...)
   and the decisions the self-play agent answers its own way; add a key to
   its DROP set only when it is presentation data.
7. **Console build** (only when the user asks): the user runs, in the Xbox
   360 Command Prompt, `python tools\build_logic.py --game-dir <game>`
   (when logic changed; stage `c` needed), `python converter\convert.py
   build <pck> out` (when the converter changed), `xdk\build.cmd spire360`,
   and copies `out\pack.s3p` and `out\data` to `<xex folder>\spire\`.
   See `xdk/README.md`. Tell the user exactly which of these steps a build
   needs (logic_lib recompiles for ~47 min when the logic changed).

The stale-file list at the top of `xdk/build.cmd` was for zip updates; with
a git clone, deleted files go away by themselves (keep the list as is).

### Desktop setup (2026-10-18)

- Local Git now tracks the exact signed branch head `bf024082` as a
  shallow checkout. The GitHub integration supplied its commit and tree;
  all 451 local source files, the tree and the signed commit were verified
  against GitHub's object hashes. Use the integration for remote reads and
  commits; Windows command-line Git has no saved GitHub credentials.
- Deleted remote `bundle6` and the six root `bundle*.zip*` archives.
  Removed superseded root `HANDOFF.md` and `docs/PLAN.md` /
  `docs/DECISIONS.md`. Other old reports, the screenshot and `pck_sample/`
  were moved to ignored `work/legacy/`. Build caches stayed in place.
- MSYS2 + its MSYS `gcc` package live in ignored
  `work/toolchain/msys64/`; set `MSYS2_PATH_TYPE=inherit` before launching
  `usr/bin/bash.exe -lc` so Windows Python/.NET remain on PATH. Use that
  shell for the converter, JIT driver and PC tools. The PC build compiles
  serially and enables MSYS2's POSIX declarations.
- ILSpy is local at `out/tools/ilspycmd.exe`; the complete installed
  v0.111.0 DLL is decompiled into `decomp/`.
- Full pack: `work/pack/pack.s3p`, 687.8 MB, 0 conversion errors; its
  data lives at `work/pack/data`. Existing console pack/caches preserved.
- JIT `copy,patch,jit` passes. Strict C++03 PC build passes. Baseline:
  `traces/base`, three seeds per character, **15/15 completed**, no logic
  errors. The baseline comparator check prints `differences: 0`.
- Do not tick alpha checklist items for this setup. Gameplay fixes and
  their validation are still required; no XDK build has been run.

### Desktop implementation progress

- Setup/tooling published in `0c3486a`.
- Combat continuity (`b98df1f`): the room, creature Spine tracks and
  surviving hand holders stay alive across decisions and behind combat
  selections/rewards. Stable card instance ids connect hand holders to
  steps. HP, block, powers, energy and pile counts update at their impact
  cues; the orange health bar follows NHealthBar's delayed tween. Cards
  move into/out of the hand; disappeared enemies play their deaths.
  Auto-play waits for playback and submits complete multi-card choices.
- Validation: strict C++03 build; 15/15 seeded JIT runs with
  `differences: 0`; `spire_check` replays 1,421 decisions, checking 995
  persistent rooms and 366 delayed HP impacts, with **0 failures**.
  It also checks controller selection of two cards in simple/combat/deck/
  upgrade grids. An 80-decision PC render run completed.
- This is partial checklist progress: map/room transitions, all intermediate
  card animations, orb cues, VFX cleanup and other alpha items remain.
  The local full pack also exposes a missing Silent `sidestep` atlas path
  in a trace; investigate before the no-missing-texture soak gate.
- Logic presentation exports changed: the eventual console build needs
  wasm/C + logic_lib rebuilt. The converter/pack is unchanged so far.
- VFX cleanup (`cdc59ff`): one-shot particles keep simulating after emission stops;
  this fixes frozen Osty heal particles. Effects now leave the scene tree
  through deferred deletion. Slash sprites, generic particle/Spine VFX,
  damage numbers and turn banners release their nodes. Big slash and hit
  spark scripts follow their particle restart and lifetime sequences.
  Delayed tweens retain the remainder of a late frame.
- Validation: strict C++03 build and all 1,421 presentation responses pass
  with 0 failures, including new particle restart/expiry, Osty/slash node
  cleanup and delayed-tween timing checks. No logic or pack change in this
  batch. Other custom VFX scripts still need systematic coverage.
- Orb playback (`b62fc2e`): stable orb instance ids and explicit OrbModel passive /
  evoke cues; channel/capacity snapshots follow the action timeline.
  The engine preserves surviving orb sprites, moves slots over 0.45 s,
  fades removed orbs over 0.25 s, restarts activation particles and plays
  the scene's evoke effect, frost shields and glass evoke impacts.
  Follow-up projectile/shaker progress is below. Remaining specialized
  effects and trail rendering keep the full orb checklist item open.
- Orb validation: 15/15 seeded JIT runs, `differences: 0`; strict C++03
  PC build; 1,421 presentation responses with 181 orb persistence/removal
  checks and **0 failures**. No converter change.
- Reproduced Room Full of Cheese's Gorge selection with the real logic:
  simple_grid, eight cards, exactly two required, no manual confirmation.
  Selecting indices 0,1 resumes the map and grows the deck from 10 to 12.
  Controller end-to-end coverage is recorded below.
- Texture cache (`2adfce5`): Canvas owns uploads by pack path, including generated
  gradient samplers. An 80 MiB LRU evicts after the frame, with backend
  allocation estimates; textures unused for 120 rendered frames are also
  released. D3D9 flushes batches and clears cached/bound sampler references
  before destruction. Resource aliases share uploads and do not retain GPU
  pointers after the renderer dies. PC builds reuse unchanged objects.
- Card selection (`2adfce5`): manual confirmation works at a fixed count as well as
  a range, selection cannot exceed the maximum, and upgrade templates get
  a Confirm button when needed to open their preview. The real Gorge
  decision now passes through GameClient controller input, sending both
  indices. All eight automatic/manual grid cases pass.
- Validation: strict C++03 PC build; 1,425 presentation responses, including
  the actual event and texture cache lifecycle cases, **0 failures**.
  Timed JIT runs: 15/15 completed, `differences: 0`. Timings are under
  ignored `work/timings`; the repeated rewards delay is not reproduced on
  JIT, so the Xenia performance item remains open.
- Console log (`2adfce5`) writes and flushes each line instead of retaining and
  rewriting the whole history. This console-only change was reviewed;
  no XDK compile or Xenia validation was run. Pack remains unchanged.
- Doom (`85e4ffe`): the converter now bakes NoiseTexture2D resources with Godot 4.5.1
  (including FastNoiseLite defaults and the resource's settings). Outputs
  are cached under ignored `converter/.cache/procedural/`. Both renderers
  implement the doom bar's screen-space noise and gradient fragment shader.
  The PC render check verifies purple pixels and animation over time.
- New full pack: `work/pack-next/pack.s3p`, 690.0 MB, 23 unique procedural
  textures, **0 conversion errors**. Earlier packs remain in place.
  Use `work/pack-next/data` with this pack. This converter change means
  the console pack must be rebuilt for the next Xenia test.
- AtlasResourceLoader's unpacked/beta fallbacks (`85e4ffe`) resolve absent atlas
  sprites (cards, relics, powers and potions). Sidestep resolves to its
  actual beta portrait; its previous missing-texture warning is fixed.
- Validation: 21 converter tests pass; strict C++03 PC build; full replay
  of 1,425 responses with 0 failures for doom/procedural changes, followed
  by targeted Silent/Necrobinder/event runs for atlas fallback and enemy
  death cleanup, all 0 failures. Doom images checked in ignored
  `work/doom-shots`. PC D3DCompiler compiles both shader-model-3 stages
  (`python tools/check_canvas_shader.py`). No XDK build was run.
- Map/run flow (`4add1c6`): map opening fades and the legend slides in; selected path
  ticks change sequentially over 0.8 s and grow from 1.7 to 1.2 over 0.4 s.
  The marker hides, changes point, and bounces into place. Room travel
  retains the old scene while the logic resolves, sweeps the game's black
  gradient after 0.5 s, swaps only once black (1.1 s), then fades in over
  0.8 s. Fast responses cannot cut playback short; delayed responses leave
  the old room behind the black overlay. Input and auto-play wait.
  The gradient loader now honors colors with omitted/default offsets and
  supports cubic interpolation as Godot does.
- Validation: strict C++03 build; full 1,425-response replay with 995
  persistent-room checks, 366 HP impacts, 181 orb checks and 26 death
  checks, **0 failures**; controller map travel tested with fast and slow
  replies. Map travel render inspected under `work/transition-shots`.
  A real Doom-only fight (Scourge test deck) reached rewards in five
  combat responses with a death cue; replay passes. Its trace stays in
  ignored `traces/doom.jsonl`. Doom-specific dissolve/overlay VFX, map
  start-of-act animation and map close/view controls remain open.
  No logic or converter change in this flow batch.
- Orb VFX (`3097f9f`): NShaker interpolates chaotic offsets and returns the target
  to rest with NOrbVfx's sine OutIn strength decay. Passive activations use
  each orb's duration and strength. NVfxProjectileHandler follows the
  scene's movement/height curves, stops emission at impact, spawns impact
  VFX and frees its trailing particles after 2 s. Dark evokes and Plasma
  passive/evoke projectiles use it; Plasma's second projectile is delayed
  by the scene's interval. Focus particles require a positive Focus amount.
- Validation: strict C++03 build; shaker motion/reset, gradient defaults/
  cubic interpolation, Dark/Plasma arrival timing and cleanup checks pass.
  Real logic test decks recorded passive and evoke cues for all five orb
  types (`traces/orb-cases`, ignored). Replay: 133 responses, 98 persistent
  rooms, 42 HP impacts, 129 orb checks, **0 failures**. No logic or pack
  changes. Line2D/Bezier trail drawing and specialized shader coverage
  remain before the orb presentation is complete.
- Trail geometry: Line2D uses Godot 4.5.1's MIT-licensed line builder,
  adapted to C++03 (sharp/bevel/round joins, caps, closed paths, width
  curves, gradients and texture modes). NBezierTrail, NBasicTrail and
  NTrail2D now generate their movement points; the projectile flipbook
  shader's frame UVs and gradient map are rendered. Inventory: 19 scene
  files contain native Line2D nodes (some multiplayer/debug only).
  Edge antialiasing and visibility-reset details remain open.
- Validation: strict C++03 build; line width/gradient, all join modes,
  round caps and closing segment checks pass; 133 orb replay responses,
  **0 failures**. Dark/Plasma renders inspected in `work/trail-shots`.
  PC builds now use GCC dependency files to reuse unaffected objects;
  included-header invalidation and escaped paths verified.
- Console timing now logs `logic response` as soon as a complete protocol
  line arrives. The existing decision timer is labeled `response +
  presentation` because it includes scene building/playback. This will
  distinguish the reported rewards delay in the next Xenia report. Console
  code reviewed; no XDK build was run. No logic or pack change in this batch.

- Later-act coverage: `traces/soak` has one high-HP test run per character;
  all five completed and Silent reached victory. `spire_check` replayed
  **5,130 responses, 3,786 persistent rooms, 1,416 HP impacts, 575 orb checks
  and 134 death checks, 0 failures**. Two remaining warnings: Aeonglass's
  `RaySlot` refers to `blast_placeholder` (investigate Spine slot setup),
  and The Lantern Key incorrectly requests a portrait: its actual layout
  is Combat, which must embed the encounter rather than a default portrait.
- Offline bank converter (this batch): `tools/fmod_metadata` uses
  pinned NuGet FModBankParser 1.2.0 + Fmod5Sharp 3.0.1; the parser's own
  audio path expects an unpublished stream overload, so its in-memory
  metadata input masks SND chunks. The separate FSB path calls the public
  byte-array API to rebuild Ogg without re-encoding. No game DLLs load.
- **Do not load the installed fmod.dll in helper processes.** ctypes probes
  stalled in LoadLibrary and coincided with the user's "Self-protection
  failed. Error code 4" popups. Those helpers were terminated. Use the
  offline parser/decoder; don't repeat the DLL probes.
- `converter/spire_convert/audio.py` preserves the complete Studio graph
  and per-bank waveform mapping (waveform GUIDs may occur in multiple
  banks with different sample indices). FSB loop ends are exclusive;
  final Ogg granules are corrected to FSB's frame count with a new CRC,
  retaining the original audio packets. Original PCM would be 4.493 GiB;
  the rebuilt audio-only pack is **258,547,949 bytes**, 2,313 unique samples.
  The installed v0.111.0 has **591 named events** (the old 336 count was
  stale), 650 event GUIDs including nested events, and 32 parameter names.
- Audio check: all 2,509 source samples' rebuilt rate/channel/frame counts
  pass independent vgmstream r2117 checks. One source/rebuilt decode was
  compared sample-by-sample, **identical PCM**. Full converter tests: 25
  pass; strict C++03 PC build passes. No logic changes in this batch.
  `work/audio-pack/pack.s3p` is audio-only; the current visual pack remains
  `work/pack-next/pack.s3p`. Full builds now include bank audio; optional
  `--vgmstream work/toolchain/vgmstream/vgmstream-cli.exe` checks it;
  `--skip-bank-audio` is only for visual test packs. Keep the ignored
  `converter/.cache/audio` cache. Engine decoder/mixer/XAudio2, adaptive
  music and temporary mp3 playback are still open; do not tick Sound yet.

## State (v0.111.0, 2026-10-18, commit 5fb9bdf and later)

**Logic**: the game's own sts2.dll, IL-patched (`logic/patches.json`),
driven by `logic/Spire.Logic` (JSON lines: decisions, commands, `steps`).
Runs as .NET JIT (the oracle), NativeAOT-LLVM wasm -> w2c2 C (PC native
and the console; seeded traces byte-identical, x86-64 and big-endian PPC).
Decisions with screens: main menu, character select, Neow and events,
map, combat, card rewards and rewards, card selections (grid, upgrade,
choose a card, bundles), rest site, shop, treasure (player picks the
relic: `open_chest`, `pick_relic`, `proceed`), game over. `steps` carry
state diffs and UI cues: `vfx` (on a creature, its side, the screen),
`anim` (creature animation triggers), damage numbers, `death` (also for
enemies alive when the combat ends), `wait`. Cards, powers and relics
export their hover `tips`.

**Engine** (`engine/`): Godot 4.5.1's scene tree, Control layout,
containers, themes, text (MSDF fonts exported by Godot, TextServer
shaping, RichTextLabel with the game's effects), CPU particles (flipbooks,
per-particle erosion), our Spine 4.2 runtime, tweens, a software renderer
and a D3D9 renderer (PC + 360, one `canvas.hlsl`; regenerate
`canvas_hlsl.inc` with `python tools/gen_shader_inc.py
engine/gfx/shaders/canvas.hlsl`). Shader ports in `engine/scene/shaders.cpp`
(hsv, button pulse, card glow, colour matrix, ripple, gradient map,
grayscale, flipbook, stepped fire, the erosion particle family: ring polar,
poof, common particle). Game screens in `engine/game/` (one file per
screen; `game_client.cpp` turns decisions into screens and choices into
commands; `combat_screen.cpp` plays `steps`).

**Console** (`xdk/spire360`): the engine + the translated logic in one
XEX, logic on its own thread. A whole run plays in Xenia.

## The user's last Xenia test (2026-10-18) and the log

Works: attack effects show; rest site fire, treasure room, Osty fine.

Reported, in the user's order of importance:

1. **No continuous flow (primary).** Combat, map travel and room changes
   feel like a slideshow of static scenes; the game flows continuously.
   Cause: each decision rebuilds its screen. Needed: a persistent combat
   scene animating state changes (HP bar tweens, block, cards flying to
   the piles and from the draw pile, intents changing, powers appearing),
   the game's room transitions (fades, NMapScreen open/close, the map's
   travel animation along the path, act changes), the hand drawn and
   discarded as in NPlayerHand. Sound will help, but this is not only
   sound.
2. **Effect timing.** Effects start late and play too slowly (the "slash"
   text inside the Strike slash effect is visible: the effect's timeline
   or its texture/region is wrong); effects linger until the next turn
   (Osty's rebirth): VFX nodes need the game's lifetimes (QueueFree after
   their particles/animations end); animations play at the wrong moment
   (cue order vs `wait` pacing in `play_steps`).
3. **Defect orbs**: evoke damage shows on the next turn and without its
   effects; channel/evoke animations missing.
4. **Doom**: not drawn on the health bars (needs FastNoiseLite + the
   doom_bar shader); an enemy killed by doom plays no death animation and
   stays until the next turn.
5. **Freeze** on a card select screen, probably the event that adds two
   common cards. Xenia log: last line "decision 295: card_select (logic
   138 ms)" after a combat_play, nothing after: the screen built (or hung
   building) and input never produced a command. Reproduce with
   spire_play on that event's decision (find the event in the decompiled
   source; drive it with a seed or a scripted decision in spire_view).
6. **Slowdowns**: Xenia's log has ~187k "Loaded tiled" texture uploads in
   ~45k frames: `Canvas::gpu_texture` (D3D9 renderer) never frees GPU
   textures, memory grows, Xenia re-uploads. Needs a texture budget with
   LRU eviction (and freeing textures of screens that are gone). Frame
   cost: ~4 ms process + layout, ~12.5 ms draw, 300-700 draw calls
   (batch by texture/state).
7. **Slow logic**: the second `rewards` decision of every fight takes
   ~2 s in Xenia: profile it on PC (likely card reward generation or a
   save/serialisation path; time per command in random_runs).
8. **Log**: `spire360.txt` stopped at decision 221 while Xenia's log went
   to 295: `flush_log` (xdk/spire360/main.cpp) rewrites the file only now
   and then. Append each line (or flush after each decision) so a freeze
   leaves the full log.
9. Tips (cards, powers, relics) not tested yet.

## Port checklist (everything except multiplayer)

Order: the flow and the bugs above first, then the rest. Tick items off
here as they land, with the commit.

**Flow and combat**
- [ ] Persistent combat scene; state changes animated from `steps` (HP and
      block bars, powers, intents, energy, piles, card moves: draw,
      discard, exhaust, shuffle, generated cards), the game's timings.
- [ ] NPlayerHand behaviour: draw/discard animations, card play flying to
      its target, retained cards, hand selection mode (choose cards from
      the hand: now a grid).
- [ ] VFX lifetimes and cleanup; effect timing and speed; damage blocked
      effects; hit flashes and shakes (NCreature, ScreenShake).
- [ ] Orb channel / evoke / passive effects and their timing.
- [ ] Doom bar; death by doom (and every non-attack death) animated.
- [ ] Creature focus tips (powers, intents) in combat.
- [ ] Remaining combat VFX shaders (~60 effect shaders in the pack: the
      erosion family is done; screen distortion, heavy_blunt_erosion,
      water reflection, dark_blur and the screen-texture blurs, the rest
      as `survey` lists them).
- [ ] Turn banners, combat start/end, victory and boss transitions.

**Run flow and screens**
- [ ] Room transitions (fade, the room's own enter animations) and map
      travel (the path drawn, the marker moving, the map opening/closing).
- [ ] Act transitions, the act start banners, the final victory and the
      ending screens.
- [ ] Crystal Sphere minigame screen (a plain list now).
- [ ] Pause / settings menu (Start), deck viewer, draw/discard/exhaust
      pile viewers, inspect card (zoomed card with upgrade preview).
- [ ] Any decision type still shown as the plain list: find them with
      spire_play runs on every character and the AutoSlay screen list
      (DECISIONS D7).
- [ ] Saving and continue (the game's save files; on the console under
      the XEX's folder or a save container), Continue on the main menu.
- [ ] Main menu extras that work in single-player (compendium, run
      history, statistics, timeline) as far as they are single-player.

**Sound**
- [ ] Converter: the 12 FMOD Studio banks (301 MB, 591 named events) and the
      mp3 temp sfx: extract the samples at conversion time (FSB5 inside
      the banks), plus the event -> sample(s) mapping and the parameters
      the game sets.
- [ ] Engine: XAudio2 playback on the console (a PC backend for the
      tools), streaming music.
- [ ] The game's sound cues from `steps` (`sfx` / `music` cues are in the
      protocol), the music logic of NRunMusicController, UI sounds.

**Performance and robustness**
- [x] GPU texture budget + LRU; idle screen textures released (`2adfce5`,
      validated by PC cache tests; console verification pending).
- [ ] Draw call batching.
- [ ] The 2 s rewards decision.
- [x] Log appended and flushed per line (`2adfce5`; console
      verification pending).
- [ ] Soak: long spire_play `--auto` runs on every character with no
      plain-list screens, no missing-texture warnings, no hangs.

**Not ported (by decision)**: multiplayer and everything that only exists
for it (votes, other players' hands and holders).

## Known gaps in the logic adapter

DECISIONS D6: healing between acts re-implemented in the adapter;
`random_runs.py`'s agent answers rewards/treasure differently from the
oldest traces (hence the normalisation in `trace_compare.py`).
`InitProgressData` warns once on a fresh profile (harmless).
