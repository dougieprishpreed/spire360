# Handoff

Rewritten at each change of session; read it first. Plan: [PLAN.md](PLAN.md).
Why things are the way they are: [DECISIONS.md](DECISIONS.md). Dialect rules:
[../docs/CODING_RULES.md](../docs/CODING_RULES.md).

Last rewrite: 2026-10-18, when the work moved from a cloud session (game
data reached it as zip "bundles") to a local desktop session on the user's
PC, with the game installed. Updated 2026-10-19 (profiles checkpoint below)
and 2026-10-20 (coverage and screen-texture checkpoint below; alpha push
parts 2 and 3 above it).

## Alpha push, part 3 (2026-10-20; commits 40ced41 and the next)

Paused here at the user's request. Validation at the pause: traces
`differences: 0` (base5 vs new15), `spire_check` on new15 0 failures
(1,421 / 995 / 366 / 181 / 26), D3D9 check rc=0, strict C++98 PC build
clean, content coverage (all suites, `--act-runs 3`) all ok.

- GodotStubs: the generator emits `return default` for every member the
  manual stubs lack. `Vector2I.One` was zero (the Crystal Sphere's small
  gold had no cells; `==` on Vector2I was always false). Real versions of
  the value-type / math members are in `Manual/ValueMath.cs` (Vector2,
  Vector2I, Vector3, Quaternion, Rect2, Transform2D, Colors, Mathf);
  Transform2D.Identity is the identity now. A full content-coverage run
  with every remaining generated stub instrumented hit none of them.
- Crystal Sphere end flow: the logic adds the finished minigame's state
  (`crystal_sphere`, presentation only) to that floor's rewards and card
  reward decisions; the client keeps the sphere (its last divination
  revealed) under them and opens the rewards 0.75 s later
  (`RewardsScreen::set_delay`, CompleteMinigame's Cmd.Wait).
- Monster spine setup: the logic reads what each monster's SetupSkins does
  to its spine (the bindings are UI calls in the logic; `Steps.Intercept`
  diverts them to `SpineRecorder`) and sends `spine_ops` (composite skins,
  track animations) and `phobia_skin`; the client applies them after the
  idle starts (`SpineSprite::set_skins` / `add_animation`). Calcified
  Cultist's coral, Damp Cultist's slug, Two-Tailed Rat's parts, Queen /
  Vantom tracks, Bowlbug Egg's cocoon were all missing before.
- Phobia mode works: PhobiaModeVisuals bodies, normal / phobia skins,
  NCombatBackgroundLayer (PhobiaModeVisual), NPhobiaAnimationToggler
  (AnimationPlayer inactive), the event phobia portrait; toggling it
  updates the fight in place. Not done: Decimillipede's shriveled phobia
  texture on a segment's death (ChangePhobiaModeTexture).
- PadState has right-stick bits (RS_UP / DOWN / LEFT / RIGHT; main.cpp maps
  sThumbR) for the Bestiary's move hotkeys 6-9.
- Bestiary, logic side done (presentation only): `bestiary` (sections per
  discovered act + events, entries with discovered / room type /
  encounter / monster, stats per filter with STATS.layout text, filters
  with quotes and dialogue colours, stats_mode), `bestiary_entry
  {monster, encounter}` (monsters with spine_ops, moves from
  GenerateBestiaryMoveList(null), hurt / death sfx), `bestiary_move
  {monster, index}` (runs the move on a fresh copy in NullCombatState; the
  response's steps carry its cues: sfx, `anim` on `who`, vfx).

Resume with: the client `bestiary_screen.{h,cpp}` (scenes/screens/bestiary:
sidebar of label dividers + entries in the Sidebar ScrollArea, selection
arrow at entry pos + (-42, 119), default layout with the creature in
%MonsterVisualsContainer at (0, hitbox.h / 2) fading in from black, name /
epithet / dialogue tweens, move buttons with glyphs LB RB RT LT X then the
right stick, Y toggles moves / stats, LB / RB pick unlocked filters in
stats mode; anim moves play on the spine (+ idle_loop unless die), state
moves ask `bestiary_move` and play its cues; the Kaiser Crab and
Decimillipede layouts); show the Bestiary button from the compendium
answer's `bestiary`. Then: Spine dark colour, LightOccluder2D /
SpineMesh2D, long-press confirmation, intro logo, language setting, soak
runs, a small `spire_gaps` sweep.

## Alpha push, part 2 (2026-10-20; commits 9c95c67..11a1afd)

Validation at 11a1afd: traces `differences: 0` (base5 vs new13),
`spire_check` on new13 0 failures (1,421 / 995 / 366 / 181 / 26), D3D9
syntax check rc=0, all 38 ps_e<id> and the canvas shader compile.

- AnimationPlayer (`engine/scene/animation.*`): value (continuous /
  discrete), bezier and method (queue_free) tracks on paths from the
  player's parent, autoplay, loops, `finished()`. Block broken
  (NBlockBrokenVfx on the health bar), lightning hit frames, the shop's
  used card removal, the infection overlay and dive bomb run on it.
- Shaders: `SCREEN_DISTORT` (outward / scream polar distortions); effects
  22-38 (goopy bits, Bound / Entangled / Tainted / Hexed / Ringing / Smog /
  Galvanized afflictions and the leaf, scream rings, queen fire, knowledge
  demon distortion, 2D sphere, missile core, noisy dissolve, Vantom oil);
  effects take 5 textures; particles pass CUSTOM.x (= rotation);
  CurveXYZTexture samplers; amalgam glare additive; four inline copies
  mapped to their originals. The game's `shaders/vfx/_util/*.gdshaderinc`
  are in the pck (read with converter/spire_convert/pck.py): polar
  coordinates are `mod(r*2*zoom, angle/2pi)`, `rotate` uses the
  sin/-cos matrix (PI/2 is the identity).
- Compendium: Relic Collection + Potion Lab (`collection_screen`),
  Statistics (`stats_screen`), Run History (`run_history_screen`),
  inspect relic / card (`inspect_screens`, A on entries), ScrollArea
  (NScrollableContainer + NScrollbar). Logic queries (presentation only):
  `relic_collection`, `potion_lab`, `statistics`, `run_history {index}`,
  `compendium` (which buttons show). Run files need `select_profile`.
- Relics carry `icon_base` (RelicModel.IconBaseName: LoomingFruit,
  YummyCookie differ from the id) everywhere the client draws one;
  trace_compare DROPs `icon_base` / `relic_icon_base`.
- The logic prints command exceptions with their stack to stderr.

Open: Bestiary (NBestiary: monster layouts with their animations, move
buttons, stats mode; shown once a fight was won), badges on the game
over screen (BadgeView in RunHistoryView.cs is ready), chromatic
aberration and sweeping beam flipbook shaders, inspect from the pile /
card-select screens, the Crystal Sphere, Spine dark colour,
LightOccluder2D / SpineMesh2D, phobia mode, long-press confirmation,
intro logo, language setting, soak runs. A fresh `spire_gaps` sweep was
interrupted (the app quit); rerun on a sample (two act runs +
encounters, `--dt 0.2`) when the binaries are free.

## Alpha push checkpoint (2026-10-20, later; commits ddc2c14..633b2cb)

Validation at 633b2cb: seeded traces `differences: 0` (traces/base5 vs
traces/new9, `work/play-data`); `spire_check` on traces/new9: 1,421
responses, 995 rooms, 366 impacts, 181 orbs, 26 deaths, **0 failures**;
the D3D9 path syntax-checks and every effect/canvas HLSL entry compiles
for ps_3_0 (canvas shader ~425/512 slots). Full content coverage on the
logic of this session (work/coverage4): all suites ok. No XDK build ran.
Use the complete pack `work/pack-sound-effects/pack.s3p` with
`SPIRE_DATA=work/pack-sound-effects/data` (baked NoiseTexture2D, audio);
`work/pack` is older.

Logic (presentation keys, DROPped by trace_compare where they reach
decisions): `view_pile` (deck/draw/discard/exhaust card views, the draw
pile by rarity, deck upgrade previews), `view_map` (the map screen's view
without choices), `settings` / `set_setting` / `reset_gameplay_settings`
(PrefsSave + SettingsSave through SaveManager after the FileStore is
installed), creature `death_fade` / `death_padding` /
`death_anim_length`, context `run_time` (RunManager.RunTime). The UI
stand-ins (previous session): `NCombatRoom.Instance` and per-creature
NCreature stand-ins answer GetCreatureNode, positions are encoded Vector2
(x = 1e6 + slot·1e4, y = −1e6 − anchor·1e4; Steps prints `e1@bottom+dx,dy`).

Renderer: effect shaders (`engine/gfx/effects.h`, one ps_e<id> each in
`shaders/effects.hlsl`, the C++ twin in `SwRenderer::effect_pixel`), 20
so far; per-particle parameters through `shader_effect_particle` (two
curves at the life, CUSTOM.z offsets). Godot 4's fragment COLOR is vertex
colour × texel (game shaders keep a `vertex_color` varying for the
vertex colour alone). Render to texture: `begin_capture/end_capture`
(the group target copied out; premultiplied textures, the canvas shader
unpremultiplies with c28) behind `SubViewport` (update modes, size 2D
override), `ViewportTexture` (bound per node instance:
`bind_texture`), `SubViewportContainer`. Inline shaders are named
`inline:<fnv64 of their code without comments/whitespace>` so copies share
a port. Effect samplers take GradientTexture1D/2D and CurveTexture.
MeshInstance2D draws ArrayMesh surfaces; SpineSlotNode materials apply
per slot (ShaderMaterials included).

Shown now: monster death (NMonsterDeathVfx: body captured once into the
effect's viewport, dissolve.gdshader with its flakes and sound, after the
death animation + 0.5 s; non-fading monsters vanish), card exhaust
(NCardExhaustQuickVfx from the hand, NCardExhaustVfx for a played card in
its viewport), the low-HP border (PlayerHurtVignetteHelper.Play), deck
view (LB: sorters, View Upgrades on Y) and pile views (LT/RT/RB), the map
overlay (Back) and map scrolling (up/down), the act banner with the
start-of-act map rise, the settings screen (pause and title: General +
Sound; volumes applied to the buses squared as NAudioManager, shake
multiplier, hand indices, text effects, fast mode's act timings, run
timer) and the credits.

Open (next): Crystal Sphere screen (scry_reveal needs 121 per-cell fades:
plan a CPU-baked ball texture + per-cell vertex alpha + border quads;
crystal_sphere_item shine), compendium / run history / statistics,
phobia mode bodies, long-press confirmation, intro logo, ~55 remaining
effect shaders (`python tools/shader_survey.py <pack>`; afflictions,
forms, scream, boss/background inline ones), NCardPlay flight details,
inspect-card screen, soak runs. `spire_gaps` over traces is slow; sample
(work/gaps5.* covers 2 act runs + encounters + events + scenarios).

## Coverage and screen-texture checkpoint (2026-10-20)

Content coverage (`tools/content_coverage.py`, everything unlocked): all
suites pass with no logic exceptions: cards (each pool with its own
character, 25 a batch, plain and upgraded; 60 cases), 66 potions, 299
relics (obtain, then monster/rest/shop/treasure), 198 events (every option
probed), 85 encounters, scenarios (Byrdonis egg hatching into Byrdpip at a
rest site, Byrdonis nest), 10 game-flow runs (Overgrowth/Underdocks, Hive,
Glory reached; 4 victories). The harness gives event-made content what its
event sets (`Coverage.cs` EventSetup: Darv's Dusty Tome card, Orobas's Sea
Glass character, Tinker Time's Mad Science type and rider) and skips
relics the game never allows in a solo run (Massive Scroll). Remaining
"unplayable" notes are curses/statuses/quest cards and upgraded Regent
cards after the batch spent its stars (harness, not logic).

Screen-reading shaders: `Renderer::set_screen(ScreenParams)` copies the
screen once per canvas (tree render) with Godot's back-buffer mipmaps
(halve, 7-tap Gaussian; `image.cpp` blur_mip_level). Software renderer and
D3D9 (PC: StretchRect + render-target level textures; Xbox 360: Resolve of
the back buffer, level passes in an EDRAM surface after the group surface,
resolved; the HLSL compiles for vs_3_0/ps_3_0 with d3dcompiler_47 and the
PC C++ syntax-checks; the Xbox path is unbuilt: no XDK build was run).
Ported: dark_blur (main menu BlurBackstop tweened as NMainMenu
EnableBackstop/DisableBackstop), vfx_water_reflection_post (Neow),
the_insatiable_sand_fall_2. The NotObtained epoch slot shows its
SubViewport's blurred portrait through a canvas-made texture
(`DerivedImage`: source stretched to 162x100 over the clear colour, hsv,
dark_blur lod 2), radial_blur (`NCombatRoom.RadialBlur` cue: the room's
RadialBlur node as NRadialBlurVfx.Activate). Still to port: doom_overlay,
chromatic aberration (glass orb), scream distortion.

Logic: the `ui` patch has `stand_in_singletons`: a UI singleton getter
heading a `?.` chain that is a statement of its own returns a stand-in, so
`NCombatRoom.Instance?.PlaySplashVfx(...)`, `?.RadialBlur(...)` and
`?.CombatVfxContainer.AddChildSafely(NThinSliceVfx.Create(...))` now reach
the steps as ui cues (220 sites) instead of being skipped; a stack-taint
check keeps the null wherever a chain would dereference or keep a UI
result outside the UI (`IsStatementUiChain`). Godot colours in cue args
are `#rrggbbaa`. Seeded traces: differences 0. The console logic (wasm)
needs the patch stage rebuilt to pick this up.

Scripted combat VFX (`engine/game/combat_vfx.*`): `NXxxVfx.Create` ui
cues spawn the game's effect scenes as their Create/_Ready do. A spec
table (anchor, tint mode, flip, restart lists on a timetable, free time)
covers Gaseous/Wormy/Poison/Goopy/Spore/SweepingBeam/GrandFinale/
Hyperbeam impacts, FireBurst, FireBurning, Scratch, (Fire)SmokePuff,
LineBurst, DaggerSpray impact/flurry, ShivThrow; bespoke scripts:
ThinSlice, Stab, GroundFire, Stunned, PowerUp, Bolas; BigSlash(Impact)
and HitSpark placed for the existing scripts; PlaySplashVfx -> NSplashVfx.
Screen shake (`engine/game/screen_shake.*`, NScreenShake: punch, rumble,
trauma with FastNoiseLite-like FBM noise) moves the combat room's scene
container; `NGame.ScreenShake/ScreenRumble/ScreenShakeTrauma` cues and the
effects' own shakes drive it (other rooms do not set a target yet).
`spire_play --settle FRAMES` takes decision shots earlier/later to see
them (cues run in the tree's process). Not yet: NHyperbeamVfx,
NSweepingBeamVfx, NGrandFinaleVfx, NHellraiser(Sword)Vfx,
NSpikeSplashVfx (Fg/Bg spikes), NHorizontalLinesVfx, form auras
(NDemon/Echo/Reaper/Serpent/VoidFormVfx via Visuals.AddFormVfx),
NLiquidOverlayVfx (creature overlay), NMonsterDeathVfx, hit stop
(NGame.DoHitStop), and
effects whose Create takes positions from creature nodes (magic missiles,
NItemThrowVfx potion throws, NSleepingVfx): headless those positions are
zero or the code is skipped behind `if (nCreature != null)`; a creature
node stand-in that carries its creature and anchor is the class-level fix.
The full `spire_gaps` sweep over every coverage trace is too slow (minutes
per long trace): sample traces instead.

## Profiles, saves and progression checkpoint (2026-10-19)

The user asked for: real PC save import, save and quit / continue, the
game's own progression for new players (or an unlock-everything option
with a chosen ascension), and a rigorous final test of all content with
everything unlocked. Design: DECISIONS D14.

Logic (`logic/Spire.Logic/Profiles.cs`, `Timeline.cs`, `Coverage.cs`):
- Saves stay in the PC game's format and layout under `<data>/user`
  (= `%APPDATA%/SlayTheSpire2/steam/<account>`), via the game's
  SaveManager. `tools/import_save.py --from <PC account> --to out/data`
  copies a PC account in (and back with `--force`, backing up first).
  The user's account is `...\steam\76561198730798228` (profile 1: 544
  runs, A10 on all five characters, a saved Ironclad A10 run at the Act 1
  boss). There is a second account folder `76561199685111768`.
- Commands: `profiles`, `select_profile` (0: the last), `main_menu`,
  `set_unlock_all`, `start_game_run`, `continue_run`, `abandon_run`,
  `give_up`, `return_to_menu` (save and quit), `timeline`,
  `timeline_first_time_done`, `reveal_epoch`, `clear_pending_unlock`;
  coverage aids `content`, `obtain`, `add_to_hand`.
- Game-flow runs: unlocks from progress (or all), acts from
  GetRandomList (Underdocks now appears; old runs never reached it),
  ascension clamp/preferred, autosave, The Architect after the last boss
  (old flow declared victory at the boss), OnEnded progress/history, the
  game over screen's badges and score-bar epoch grant, TestMode-gated
  rules switched on for these runs only (`testmode.game_flow.*`: alt-act
  discovery, potion price variance, discovery order, first-run tutorial
  rooms, per-floor history, reload count). No metrics/leaderboard upload.
- `start_run` (old flow) unchanged: 15/15 seeded runs, **differences: 0**
  (`traces/profiles` vs `traces/base`).
- `tools/profile_check.py` cases (menu, continue, new_runs, fresh,
  unlock_all, progression) all pass on a copy of the imported saves: the
  user's run continues at Vantom with HP 69/80; new runs at A10 for all
  five; a new profile has only the Ironclad, earns Neow, reveals Neow ->
  Silent -> Regent -> Necrobinder over three runs.

Presentation (`engine/game/menu_flow.*` shared by the console and
`spire_play --menu`): main menu with Continue (run info), Abandon Run
(confirm popup), Singleplayer, Timeline (notification dot, gating), Quit,
profile button (Start) -> profile screen (switch profile; Y toggles
Unlock Everything); character select with locked characters and their
unlock tips, ascension panel on LB/RB; pause menu on Start (Resume, Give
Up with confirmation, Save and Quit, capstone backstop); game over score
bar with the epoch unlock, then the Timeline when epochs were discovered;
Timeline screen (eras, slot states, tips, tutorial, reveal -> inspect ->
unlock screens for cards/relics/potions/character/text -> expansion).
Checked with spire_play menu drives on the imported profile and a new
one (screens under ignored `work/menu-test/`).

Open in this area: the epoch chains' shard VFX,
stars particles and some timeline tweens; badges/discoveries on the game
over screen; profile deletion; Settings/Compendium/run history screens;
NCharacterSelectScreen's character unlock animation (pending_unlock is
exported). Coverage: see the 2026-10-20 checkpoint above.

PC tool runs from this shell: PowerShell 5.1 mangles embedded quotes, so
MSYS2 commands go through `work/msys_run.sh <script>` (scripts under
`work/`), e.g. `work/pcbuild.sh` for the PC build.

## Resumed Sound checkpoint (2026-10-18)

Repository setup, decompilation and baselines remain intact. No caches were
reset or removed. Use the GitHub integration for remote operations; the
Windows Git CLI has no saved login. No XDK build has been run.

Sound is now connected in source. `AudioEvents` executes compact, lazy-loaded
per-bank programs compiled from `audio/events.json`: nested events, layers,
weighted/sequential playlists, random modulation, bus gains/automation,
parameter conditions, timed/trimmed instruments, loop regions and quantized
transitions. Transition segments retain their own instruments and delay the
destination timeline; inactive act banks are released. The raw Studio graph
is retained in the pack for offline inspection.

`GameAudio` connects the combat cue timeline, non-combat steps, room/UI
sounds, creature-owned loops and NRunMusicController selection/parameters.
The console main loop now pumps the existing XAudio2 backend (source only;
no XDK compile). `spire_play` mixes headlessly by default; `--audio` enables
the PC device and `--audio-wav work/<name>.wav` records its actual mix.

The offline converter normalizes all 52 imported MP3 streams to PCM16 WAV
using the local ffmpeg on PATH; bank music stays compressed and streaming.
Godot's temporary stream randomizers are resolved by the presentation code.
Do not load the installed fmod.dll in a helper process.

The UI bridge previously returned null for audio singletons, so ?. calls
never reached sound/music exports. Audio receivers now retain harmless
constructor-only placeholders. ResolveMusic and its MusicSelection value
type are preserved as pure helpers. The context exports the game's seeded
music/bank selection, encounter ambience/custom music and room progress.

Validation so far: strict C++03 PC build; 27 converter tests; synthetic
sample-level scheduling/layer/playlist/nesting/condition/loop/stream checks;
591 named events (588 start samples within 2 s), 52 imported streams, zero
decoder failures. JIT seeded regression: 15/15 runs, **differences: 0**
(`traces/audio-runtime`, compared with `traces/base`). The new traces include
1,742 temporary-audio and 448 music-controller calls as well as SfxCmd cues.
The focused audio-enabled replay passes 99 responses / 74 persistent rooms /
33 impacts / 57 orb checks / 2 deaths, zero presentation and decoder failures,
640 voices started. Two absent-event calls are the Vine Shambler mismatch
below. A 14-screen Defect PC render records a non-silent 20.95 s stereo mix
(`work/audio-play.wav`), 110 voices, zero missing/decoder failures. All 19
music events pass Progress 0..9 and boss-parameter checks; transition-bridge
fixtures and elite-phase persistence checks pass. The full 15-trace audio
replay now passes against the latest normal converter pack, as recorded below.

Follow-up fidelity checks now cover asymmetric parameter seek speeds,
authored per-bus transition fades and scatterer spawn deadlines. A scatterer
count of 33 in the current bank format means unlimited spawning; the fixture
checks 42 exactly timed spawns. Linear transition fades are checked against
individual output samples. Nonlinear curve shapes and authored DSP still
need verification.

Fidelity fixes are published in `23fc0e9`. The compact compiler now retains
ordered pre/post-fader effects, sends, typed parameters and effect automation
with its unit mappings. FModBankParser 1.2.0 uses the newer DSP enum names;
the installed v142 banks use the [2.02 enum](https://github.com/fmod/fmod-for-unity/blob/2.02/Assets/Plugins/FMOD/src/fmod_dsp.cs).
Correct inventory: 444 PAN, 105 sends, 46 multiband EQ, 31 echo, 19 flange,
10 SFX reverb, 5 chorus, 4 three-band EQ, 2 limiters, 2 sidechains, 1
distortion and 1 fader. Earlier labels for loudness meters, delay and
convolution reverb were parser enum mismatches. Integer parameter union bits
are recovered before writing programs. These effects are **not executed by
the runtime yet**. Converter tests: 29 pass; all installed bus effect
references resolve. The complete `work/pack-sound-effects/pack.s3p` build
passes: 954.8 MB, 737 localization files, zero conversion errors. Named-event,
all 52 temporary-stream and all 19 music checks pass against it. The full
15-trace audio presentation replay plus the Lantern Key branches passes:
1,428 responses / 995 persistent rooms / 366 impacts / 181 orb checks /
26 deaths, zero failures; 8,097 voices, four absent-event calls and zero
decoder failures. Logs: `work/audio-effects-*`.

The game's Vine Shambler code requests `vine_shambler_cast`, which is absent
from the installed bank's string table (the bank has `grasping_vines`).
Slithering Strangler similarly requests `slithering_strangler_tail`, absent
from the bank (which has headbutt, cast and die). Both were verified against
the installed code and offline string table. The full replay's four absent
calls use these two paths. Keep these content mismatches visible; do not
silently invent replacement cues.

Packs preserved: `work/pack/pack.s3p`, `work/pack-next/pack.s3p`,
`work/audio-pack/pack.s3p`. New audio test pack: `work/audio-runtime/pack.s3p`.
`work/pack-audio/pack.s3p` combines pack-next visuals and the new audio for PC
checks. The earlier complete converter build is `work/pack-sound/pack.s3p`:
954.6 MB, 737 localization files, zero conversion errors. Full decode passes
2,365 samples / 922,744,173 frames, with zero
failures; named-event, imported-stream, all-music and focused 99-response
presentation checks pass against it. Older intermediate packs are preserved.
Keep `converter/.cache/audio` and every existing ignored build cache.

Combat events now use `combat_event_layout.tscn` and the synchronizer's
existing encounter in VisualOnly mode. The room, Spine tracks and creatures
persist across event pages and into combat, when health displays and combat
UI activate. The Lantern Key's gold and fight branches pass with the real
logic; the rendered layout and controller options are checked. Focused
replay: 106 responses / 74 persistent rooms / 33 impacts / 57 orb checks /
2 deaths, zero failures, 667 voices and zero decoder failures. Seeded logic:
15/15 runs in `traces/combat-event`, **differences: 0**. No gameplay RNG is
consumed by the visual export. Punch-Off and The Architect also pass real
logic export, controller/layout and render checks; Punch-Off's transition
into combat preserves its room. Their focused replay passes 105 responses,
zero failures. Punch-Off's ambient sparring loop and The Architect's speech
bubbles/head animation are still open. Renders: ignored `work/lantern-gold.png`
and `work/event-{punch_off,the_architect}.png`. The current complete pack is
`work/pack-sound-effects/pack.s3p`; every earlier pack/cache is preserved.

Aeonglass's RaySlot warning is a stale unused anchor in the installed scene:
`slot_name=blast_placeholder` is absent from the original skeleton; RaySlot
has no children and NAeonGlassVfx does not use it. Do not invent a substitute
slot or hide other missing anchors. It is not a converter bone omission.

Next: finish the audio fidelity audit (authored DSP and nonlinear transition
curve shapes still need coverage), cover the other combat events and continue
the alpha checklist. Saving/continue, Crystal Sphere, pause/deck/piles,
act/victory transitions and remaining shaders are still open. This is **not
an alpha candidate**. Event playback is connected, but Sound is not ticked
complete while its remaining fidelity gaps are unverified.

After logic changes use the same baseline data directory (its exact string
is in the ready header), and require `differences: 0`:

```
python tools/build_logic.py --game-dir "F:\Program Files (x86)\Slay the Spire 2" --stage patch,jit
python tools/random_runs.py --cmd "dotnet out/logic/jit/Spire.Logic.dll" --runs 3 --trace-dir traces/audio-runtime --data-dir work/pack/data
python tools/trace_compare.py traces/base traces/audio-runtime
```

The muted Windows XAudio2 check submits/recycles 16 buffers and confirms
positive frame consumption. No background job should remain after validation/publishing.

PC tools use the preserved MSYS2 toolchain with `MSYS2_PATH_TYPE=inherit`;
explicitly cd to the repo inside bash -lc. Checks/outputs remain under
ignored `work/audio-runtime-*`. Keep the one-background-job limit. The next
user-requested console build needs a new pack and wasm/C + logic_lib because
logic presentation exports changed; never run an XDK build without a request.

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
- Offline bank converter (`eec6c86`): `tools/fmod_metadata` uses
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
  music and temporary mp3 playback were still open at that commit; do not
  tick Sound yet. See the current decoder/mixer work below.
- Portable streaming decode (current batch): `engine/audio/decoder.*` uses
  stb_vorbis 1.22 (MIT/public domain, pinned under `third_party/stb`) and a
  PCM16 WAV reader. Pack audio uses independent file handles and bounded
  decoder buffers, so music need not be loaded as whole PCM tracks.
  `spire_audio_check work/audio-pack/pack.s3p` fully decoded **2,313
  samples / 918,182,894 frames**, checking EOF and sample-accurate seeking
  against the first decode, **0 failures**. Strict C++03 build passes.
  The mixer now passes source comparisons for layering, finite loops and
  their tails, resampling, pause and volume fades (including sub-sample
  fades). The Windows XAudio2 backend submitted/recycled 16 buffers and
  consumed 4,096 frames with its master muted. Xbox output uses the same
  mixer; its API was checked against the installed SDK headers, with no
  XDK build. A fresh sound-hook presentation replay passes 99 responses /
  74 persistent rooms / 33 impacts / 57 orb checks / 2 deaths, 0 failures.
  Event interpretation and game playback integration remain open.

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
      ending screens. Act banner + start-of-act map rise done (`fde9a44`).
- [ ] Crystal Sphere minigame screen (a plain list now).
- [ ] Pause / settings menu (Start), deck viewer, draw/discard/exhaust
      pile viewers, inspect card (zoomed card with upgrade preview).
      Done: pause, settings + credits (`c9fa02f`, `633b2cb`), deck and
      pile viewers (`dcfe373`), map overlay (`3d2eb47`); open: inspect card.
- [ ] Any decision type still shown as the plain list: find them with
      spire_play runs on every character and the AutoSlay screen list
      (DECISIONS D7).
- [ ] Saving and continue (the game's save files; on the console under
      the XEX's folder or a save container), Continue on the main menu.
- [ ] Main menu extras that work in single-player (compendium, run
      history, statistics, timeline) as far as they are single-player.

**Sound**
- [x] Converter: the 12 FMOD Studio banks (301 MB, 591 named events) and the
      mp3 temp sfx: extract the samples at conversion time (FSB5 inside
      the banks), plus the event -> sample(s) mapping and the parameters
      the game sets (`eec6c86`, `7877c59`, `5bb51a6`; complete local pack and
      all sample/event/stream checks pass).
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
