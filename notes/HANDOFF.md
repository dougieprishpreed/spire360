# Handoff

Rewritten at each change of session; read it first. Plan: [PLAN.md](PLAN.md).
Why things are the way they are: [DECISIONS.md](DECISIONS.md). Dialect rules:
[../docs/CODING_RULES.md](../docs/CODING_RULES.md).

Last rewrite: 2026-10-18, when the work moved from a cloud session (game
data reached it as zip "bundles") to a local desktop session on the user's
PC, with the game installed. Updated 2026-10-19 (profiles checkpoint below)
and 2026-10-20 (coverage and screen-texture checkpoint below; alpha push
parts 2 and 3 above it); since then each session adds its section at the
top (the newest: the second hardware run, 2026-10-26).

## Fourth hardware run (2026-10-27): the noise's real stall, the display, and the user's findings

The user ran 2806b33 (profile=1; spire360.txt, spire360_logic.txt and six
Back + RB screenshots on E:\Games\spire360; copies in work/rp96). Effects
in fights a little faster; map and many event screens at 60; black screens
still at fight and act loads.

- The screenshots: Resolve writes the tiled layout even into a LIN_
  texture, so the BMPs looked like noise. work/untile_bmp.py untiles them
  (XGAddress2DTiledOffset as Xenia has it): work/rp96/u1..u6.png.
  write_bmp now untiles on the console.
- Dark fight backgrounds (Underdocks, Act 2): the screenshots (the frame as
  drawn, before the gamma ramp) look as the PC's do; on the user's display
  they are very dark while the creatures look normal. So it is the output,
  which is ours alone (the user's other games are fine on the same
  display): the gamma ramp (out = in^0.88 since the Xenia sessions) or the
  front buffer (pp.FrontBufferFormat left 0). Back + Y now cycles the
  ramp's presets (GAMMA_PRESETS: 0 in^0.88, 1 none, 2 in^0.75, 3 in^0.62,
  4 in^0.88 squeezed into 16-235), each named in the log, and
  spire360.ini's gamma=N starts with one: the user picks the one that
  matches the PC, then it becomes the default.
- Frame costs (profile=1): label draws fell from ~54 ms to ~4 ms a frame
  (the theme cache); a fight frame's draw calls take 16-40 ms of CPU; the
  GPU keeps up (gpu_probe: a frame 13-40 ms, its draw calls made in
  12-37). But process went up to ~90 ms in fights: GPUParticles2D 88 ms a
  frame for 5 systems, the hive's bugs1 (150 particles, turbulence) most of
  it, ~195 us per particle step against ~3 us on PC. The magic floor was
  not the stall: the noise passed its corners through small arrays, int to
  float conversions ((float)(k & 1)) and pointers, each a store read back
  at once: a load-hit-store on Xenon. noise4 / curl_3d / noise_direction
  are now scalar arguments and locals only, and the curl evaluates the 12
  noises it reads (of 18). work/noise_test.cpp: 0 of 200,000 curls differ
  from the earlier code. profile=1 logs "particles: turbulence X us per
  particle step (floor with/without conversions)" at start.
- The Smith's preview had no B or Y button: they were drawn ~4,000 px
  below the screen. CardSelectScreen::button worked out a button's corner
  from its offsets each time the preview opened, and the first opening had
  moved them (the PC run opened it once). Now once per button (buttons_).
  The same in the fights' hand selection (sel_confirm_placed_): its Y
  button left the screen from the second selection of a fight.
- An enchanted card's upgrade preview lost its enchantment:
  UpgradePreviewView cloned the canonical card and upgraded it to the
  card's level, which loses the enchantment, affliction and anything that
  changed the card (Ritual Dagger's damage...). NUpgradePreview clones the
  card itself (CardScope.CloneCard): now ClonePreservingMutability (not
  added to the scope) then UpgradeInternal; canonical cards (the
  compendium's) as before. A logic change.
- X did nothing with no potions, and down did not reach the relics:
  OnPotionShortcutPressed focuses the first holder, potion or not; the
  holders' bottom neighbour is the first relic; the relics go left / right
  around the row (up to the first potion), each OnFocus: the icon to 1.25
  and its tips (SetAlignmentForRelic: place_tips_for_relic,
  TipFollower::show_for_relic). PotionMenu's RELICS mode.
- The Decimillipede's "black rock marks" as it attacks:
  vfx_decimillipede_rocks.tscn's rocks carry an inline GDScript (no path)
  that plays idle_loop in _ready; the port ran none, so each rock showed its
  setup pose where it lands until its turn to fall. DecimillipedeRocks
  starts them. The only other inline GDScript is decimillipede.tscn's (a
  keyboard test rig, not in fights). Its dead segments: the user's video
  frames (retail v0.104) show them as our console shot does, so not wrong.
  Still open: the yellow-green dots where a live segment meets a dead one
  (console shot u3).
- Brightness against the retail frame the user sent (a YouTube capture of
  v0.104, the hive): the raw hive layers composited average 19/255, our PC
  render (work/rp95/hive1) and the console's frame are near that, retail is
  about twice as bright with the creatures and cards alike. The particles
  add +1.5/255 on PC (spire_play --skip-draws 2: hive2). No hdr_2d in
  project.binary, no brightness setting in the game; the art may differ
  between v0.104 and v0.111. The user's display shows our frame darker than
  the frame itself: the gamma presets decide that part first.
- Not yet: the sandpit boss's sand (the user is not sure it is wrong);
  black screens at fight and act loads; the native logic's act change.

Checks: PC build 0 warnings; seeded traces against base6 differences 0
(traces/new96, run with --data-dir work/play-data); scenarios 30 ok
(work/scen96); content coverage 829 ok (work/cov96); spire_check over
new96, scen96 and split96: 2475 responses, 0 failures, audio 22870 voices,
7 absent, 0 decoder failures; the console's logic rebuilt (out/logic/c):
strict_compare 14 of 15 identical to the JIT (the unseeded skin),
scenarios 30 ok, no trap; the Smith's preview opened three times keeps
its buttons (work/rp95/rest7). Two random pad sessions were still running
at the pause (work/monkey90/g96*).

Next build: logic and engine changed: xdkuild.cmd spire360 (logic_lib
compiles again). Copy default.xex; in a dark fight cycle Back + Y and note
which preset matches the PC.


## Third hardware run (2026-10-26): particles, theme lookups, the GPU probe and a console screenshot

The user ran 138c2e1 (profile=1, trace_loads=0; spire360.txt and
spire360_logic.txt on E:\Games\spire360, the session's saves in
spire\data\session_start; work/rp95 holds the copies). Menus and most events
hold their frame rate; fights crawl whenever effects play; black screens
before Act 2's map (8 s of logic), Orobas (19.5 s) and floor 19 (13 s).

Their findings and the causes:
- The black screens before Orobas and floor 19: the watchdog named
  GPUParticles2D (Orobas' stars: 100 particles, 30 s of preprocess; the
  hive's bugs1: 150 for 10 s), all with turbulence. The turbulence's curl
  takes 18 noise4 (288 corner gradients) per particle and step, each fract a
  float-to-int conversion: on Xenon a load-hit-store stall each (~2,400 per
  particle step, ~100 us). fast_floor now adds and takes away 1.5 * 2^23
  (no conversion; a start-up self-check falls back to the old floor if the
  compiler kept the sums wider), and the curl's six samples share a cell's
  corner gradients when they fall in one (eps 0.001: strength 1). The same
  turbulence runs every frame in common effects (vfx_power_up, ...): the
  fights' 0.3-1.8 s process frames while effects played.
- Preprocess up to 600 s (glory's and the test subject's stars: 18,000
  CPU steps): a looping system's particles each restart once a cycle, so
  only the last two cycles (and a trail) are simulated; the rest only moves
  the clocks (Particles2D::preprocess_skip, skip_time). One-shot and
  sub-emitter systems run in full.
- Text draws 50-70 ms a frame on the console against 0.15 ms of CPU work on
  PC (--profile-cpu): not the labels' CPU work. Two findings: every theme
  lookup walked the ancestors with string allocations (~16 lookups per label
  per draw); Control now keeps its answers (forget_theme on its overrides,
  forget_theme_below as it moves or its theme changes; theme_generation for
  the default theme): PC allocations per fight frame 7,700 -> 6,200. And the
  draw time includes waits in DrawPrimitiveUP when the 2 MB command buffer is
  full (the GPU behind): Present is ~0 in fights and 20 ms on the main menu.
  Suspects on the GPU side: the one canvas pixel shader, whose largest
  branch sets the registers every draw holds (Xenos then keeps fewer pixels
  in flight to cover texture reads), and single-level textures (no mipmaps
  on the console) from big atlases drawn small. The first is changed:
  canvas.hlsl's ps_plain and ps_text (400 and 708 bytes against ps_main's
  8,780 with D3DCompile) draw everything with no mode on (apply_state; the
  XDK projects compile them with fxc like ps_main). Mipmaps wait for the
  probe's numbers.
- profile=1 now also logs "draw calls: X ms setting their state, Y ms in
  DrawPrimitiveUP" and, every 10 s, gpu_probe: the frame drawn again from an
  idle GPU to an idle GPU with nothing left out, then without text,
  particles, spine, images, shader materials and the fight's background
  (SceneTree::set_draw_skip; not shown).
- Survivor's discard choice under the card being played: NCombatUi.
  OnHandSelectModeEntered moves the hand in front of the PlayContainer; the
  port's played cards are the hand container's (z 2), so while selecting
  they keep z 2 by themselves (z_as_relative off) and the backstop, the
  holders and the selection parts go to z 3 (CombatScreen::sel_layers).
- Cloak and Dagger's Shiv tip right of its text tip: NHoverTipSet places
  its text and card containers apart (SetAlignmentForCardHolder: the text
  right of the hitbox, the cards left of it; swapped past 3/4 of the screen;
  then CorrectVerticalOverflow / CorrectHorizontalOverflow per container).
  make_tip_set(..., split) and place_tips_beside; the hand, creature,
  TipFollower card and merchant card tips use them; the other modes keep
  the row.
- Orobas' map icon bigger and off centre on focus: every point's icon went
  to 1.45 about a pivot from its offsets (the ancient's Icon is anchored:
  pivot 0, 0). Now each kind's HoverScale (normal 1.45, ancient 1.1, boss
  1.05) about the scene's pivot (100, 100) or the icon's middle; the ancient
  unhovers Cubic and selects back to 1 (OnSelected).
- No Y on the Smith's upgrade: the port honoured RequireManualConfirmation
  (A highlighted, a Y button opened the preview); the upgrade, enchant and
  deck screens preview at the count whatever it says (their OnCardClicked):
  A opens the preview (B back, Y confirm). PC pad run: work/rp95/rest6.
- Dark Underdocks and Act 2 backgrounds on the console: the draw dump has
  every layer at full colour; PC renders look right to the user. Console-only.
  Back + RB now also saves game:\spire360_shot_NN.bmp (the frame before the
  gamma ramp, Resolve to a linear texture): bright like the PC's means the
  output (ramp, the TV's black level: this art averages ~27/255), dark
  means the console's drawing. Not fixed yet.
- Act 2's map: 8 s of logic for the act change. work/logic_times95.py
  times a session's commands on a host (the user's data, their [CMD]
  lines): the native host (out/logic/native, the console's translation)
  takes 2,195 ms for that select_map_node (the act change, 57 KB answer)
  and 2,776 ms for select_profile; the JIT 356 and 560 ms; the rest of the
  session is alike. Not the run history (101 files, 4.1 MB: without it
  1,867 / 2,853 ms). Suspect the progress save (236 KB, full unlocks)
  parsed and written by the translated code; to profile on PC before any
  logic change (a logic change costs the user an hour-long logic_lib
  compile).

spire_play: --profile counts heap allocations per frame; --profile-cpu
draws without the software rasterizer (the engine's own draw cost).
work/rest95.sh drives a scripted scene by pad (CMDS: a [CMD] file; the
Smith: enter_room rest; Cloak and Dagger's tips: add_to_hand, --settle
420). The console session's own replay (work/prof95.sh) stops where the
user returned to the menu and continued (the replay does not follow it).

Checks (the logic is unchanged): PC build 0 warnings; spire_check over
traces/new94, scen94 and split92: 2475 responses, 0 failures once its
two-card case expects the deck and upgrade screens' preview at the count
(it had expected manual confirmation there), audio 22861 voices, 7 absent
(the known enemy casts), 0 decoder failures; two random pad sessions (native
and JIT logic) 0 reports, no logic errors; D3D9 renderer syntax for
Windows, xdk_api_check, the canvas shaders with D3DCompile; pad runs: the
Smith's preview on one press (work/rp95/rest6), Cloak and Dagger's Block
tip right of the card and its Shiv left (work/rp95/cd4).

To look at next: the console's numbers with these (the GPU probe,
draw-call times); the screenshot of a dark fight; mipmaps if images
dominate the probe; the native logic's act change and profile load.

## Second hardware run (2026-10-26): its log, a shop rule, and the console's frame costs

The user ran a2d6fb1 on the console with their PC profile imported
(tools/import_save.py) and trace_loads=1, profile=1 (spire360.txt 7000
lines on E:\Games\spire360). The embark no longer waits; fights are slow,
the shop smooth until its inventory opens.

Their findings and the causes:
- A second card removal in the same shop, at the same price: the shop's
  UI marks the service used (NMerchantCardRemoval.OnCardRemovalUsed,
  reached from MerchantCardRemovalEntry.OnTryPurchase through
  NRun.Instance, null headless; FillSlot does it when a hook forbids
  removals: the Hoarder modifier). RunDriver.ShopState now applies both
  (Hook.ShouldAllowMerchantCardRemoval as the shop opens, SetUsed on the
  entry's PurchaseCompleted) and DoRemoveCard refuses a used service. The
  price was right: it rises at the next shop (CalcCost with
  CardShopRemovalsUsed).
- The same cause, swept: the UI methods the logic calls
  (presentation_verdicts.tsv "call" rows) followed through their own and
  their fields' node classes for changes to game state; and the Nodes code
  as a whole. Found: NCard.Model's setter marks a run's card as seen
  (SaveManager.MarkCardAsSeen: Progress.DiscoveredCards and the run's
  discoveries the game over screen counts) and nothing headless did for
  cards in hands, rewards and shops; AddCardView now does. Relics and
  potions are marked by game code the logic runs (obtaining, rewards, the
  shop's entries). Not mirrored: NHoverTipSet's marks as a tip shows (the
  logic builds every item's tips up front, so it would mark what was never
  focused). The audit's verdict for OnCardRemovalUsed had looked at the
  presentation only.
- A potion's tip flashed beside the next: TipFollower showed a tip the
  frame it was made, before the layout gave it a size; it now waits.
- The continued run's fight showed the main menu's background: the title
  stays until the run's first screen is shown, and the two seconds' wait
  for the warm-up (behind the embark wipe) held it with no wipe on
  Continue; a continued run no longer waits.
- Underdocks fights look dark: on PC too (ushots), so the port's, not the
  console's. Its lights: CPUParticles2D haze of light.png (alpha 0.1,
  little_light_script.gd, ported), water glints through
  vfx_stepped_shader_fire_add (22 sprites), water reflection; no 2D lights,
  nothing skipped. Waiting on a screenshot of the PC game's Underdocks
  fight to compare.

The console's frame costs (profile=1, per frame in fights): Label draw 10-19
ms (27-42 labels), RichTextLabel 10-12 ms, GPUParticles2D 4-13 ms, Spine
2-5 ms, TextureRect 1-6 ms, layout 5-8 ms; process spikes to seconds at
first uses ("script": the screens' FrameDrivers instancing effects).
Scene files parsed at 0.15 MB/s (PC: 19 MB/s): sovereign_blade.tscn, 77 KB,
1.6 s; the warm-up's 837 files 30.8 s.
- Json's parser copied every parsed value into its parent (a subtree copy
  at each level: C++03 has no moves) and every array element again as the
  vector grew: values are now parsed into their place, arrays grow by swap,
  the parsed document is swapped into place (Json::swap). PC: the 841
  files of the warm-up 141 -> 68 ms; the console's allocator and cache
  pay far more per copy.
- Label::shape and RichTextLabel::layout ran every frame (draw, the
  minimum size and the line count each called them): each now keeps its
  lines until an input changes (text, font, size, width, wrap, visible
  characters...).

The profile now names a behaviour from code by its parent ("code in
CombatRoom"), so the screens' FrameDrivers are told apart.

Checks: seeded traces against base6 differences 0 (traces/new94);
scenarios 30 ok (work/scen94); content coverage 829 ok (work/cov94);
spire_check 2475 responses 0 failures; two random pad sessions 0 reports;
Continue from the user's profile (session_start) shows its fight with no
title behind it; the console's logic rebuilt (out/logic/c), native runs 14
of 15 identical to the JIT's (the other an unseeded skin), no error or
trap; PC build 0 warnings.

To look at next: the console's numbers with these; GPUParticles2D's cost;
the warm-up's memory (the parsed scene files stay); Underdocks with the
reference.

## First Xbox 360 hardware run (2026-10-26): what it showed, and the changes

The user ran the build of 9e7a6a1 on a real console (USB, E:\Games\spire360;
spire360.txt and spire360_logic.txt there). It boots and plays. Their
report and the log:
- A long black screen before the logo, a shorter one before the main menu.
  The log: the logic's first answer took 11 s (its ModelDb, localization
  and formatter start-up on the console's in-order core) and the title's
  first frames 3.4 s and 2 s (its scene files and textures read as first
  drawn). The logo ended after about 9 s and the menu waited on black.
  Before the first frame (the 105 MB XEX loading from USB, the pack's
  index, Loc::init) nothing was timed.
- A very long black screen when starting a run: "decision 1: map_select
  (response + presentation 30209 ms)", the embark waiting for the warm-up
  (PreloadManager's sets: about 840 scene files and 370 textures, 0.4 s on
  PC); free memory went from 208 MB to 127 MB with it (the parsed scene
  files stay cached).
- Black backgrounds in battle; low frame rate (combat: 11-19 ms process +
  layout, 27-32 ms draw at 450-580 draw calls, so about 20 fps); draws of
  300-2000 ms on the map's first frames.
- No Neow and the set first fights: the game's own first run ever
  (Overgrowth.GetUnlockedAncients drops Neow until the Neow epoch is
  revealed; ApplyActDiscoveryOrderModifications fixes the rooms when
  NumberOfRuns is 0). Five cards a turn is the game's draw. The user has
  since imported their PC profile to the USB (tools/import_save.py).

The black backgrounds: not found yet. Nothing in the battle background is
special (plain TextureRects of DXT textures at sizes the atlases share; no
SubViewport; the hidden RadialBlur BackBufferCopy only), the EDRAM layout
keeps the back buffer, group and screen targets apart, and on PC the
battle draws as it should. One theory was tested and does not hold:
counted at the console's sizes (spire_play --console-textures, through the
menus into a run), a run peaks at 69-76 MB of the 80 MB budget and no frame
goes over, so the cache never evicted what a frame drew. Two changes stay
as safety on the console all the same:
- Canvas::end_frame never evicts a texture the frame drew (a frame needing
  more than the budget keeps it all; counted as "by their own").
- D3D9Renderer releases a destroyed texture only once the GPU has passed a
  fence inserted then (InsertFence / IsFencePending; BlockUntilIdle at
  shutdown): D3D on the console frees the memory at once while the GPU may
  still have the frame's draws queued.
To find it: Back + RB on the console logs the next frame's draws one by one
(the image, its texture's size and format or "(no texture)", the screen
rect, the colour; text glyphs counted), spire_play --dump-draws-at N the
same on PC; and CreateTexture failures are logged.

The waits:
- The logo holds its teal screen (or, cancelled, its background) until the
  menu can show, and the menus' scene files and textures are read while
  it plays (main_menu, character select, its buttons, the Ironclad's
  background).
- The warm-up reads for 4 ms of each frame on the console
  (SceneLibrary::set_warm_clock; the PC tools keep the fixed counts) and
  logs its totals as it ends ("warm: N files, M textures in X ms over F
  frames"); a Spine skeleton data file it reaches has its atlas and
  skeleton parsed and its pages queued (warming a scene missed them: the
  main menu's 4.6 MB background still loaded at its first draw). The embark waits for it two
  seconds at most; the rest is read in the background.

New ini option profile=1: every 10 s the node types (and behaviours by
script) costing the most process and draw time per frame
(SceneTree::set_profile; spire_play --profile the same on PC), and the
frame line's "process + layout" is split into process and layout.

New in spire360.txt: "pack: ... (N ms after start)", "start-up: data,
fonts and the logo in N ms; the first frame N ms after start", and every
10 s after the frame line "textures: N held, X MB (peak Y MB of 80), C
created, E evicted, F frames over the budget (S by their own)". CreateTexture
failures are logged (the first 20, with the free memory). New ini option
trace_loads=1: each scene file, texture and bank read with its ms, and each
frame over 8 ms split into update, process + layout and draw.

To look at next (with the next hardware log): the frame rate (process +
layout and draw per frame; the draw calls), what the start-up spends
before the first frame, the logic's 11 s start, the memory the parsed
scene files hold.

Checks: PC build 0 warnings, C89 checks 0, xdk_api_check clean; spire_check
over traces/new92 and the scenarios 2475 responses, 0 failures; two random
pad sessions from the logo (5 minutes each, menus, embark, runs) 0 reports.
The logic is unchanged (out/logic/c as for 9e7a6a1). The console-only code
(fences, the start-up timings, the ini options, Back + RB) compiles only in
the XDK build.

For the next hardware run: profile=1 and trace_loads=1 in spire360.ini for
one session (the log grows large); Back + RB once in a battle with the
black background; whether the same build shows black backgrounds in Xenia.

## Xenia session 1 (2026-10-26): the user's release-build findings, and the same causes elsewhere

The user played the staged release build in Xenia (fast mode) and listed
what was off; then asked for every other place the same causes could show
before they found it. Each report was replayed from their own session
first (the logic log's commands, see "Replaying a console session" below),
then fixed at its cause, then the cause looked for across the game.

Their reports, the cause, the fix:
- The shopkeeper's hand under the cards: ShopScreen raised the focused
  slot's z_index (the game reorders nothing). Removed.
- The shop's music: two causes. FMOD's labeled parameters are discrete
  (label k is [k, k+1)); our condition ranges were inclusive at the top, so
  Progress 2 (Merchant) also matched the [1, 2] Enemy region
  (events.cpp `matches`). And the map opened over the shop keeps
  MerchantEnd (9) while it is up (ToggleMerchantTrack, GameAudio::
  merchant_map); the logic's RunContext no longer resets the music when
  the map opens after a room (Merchant -> 9, a fight -> 7, an act's first
  map -> 0).
- No tips on the shop's relics, potions and the removal service: the logic
  sends each entry's HoverTips (and the removal's text); the client shows
  them as the game places them (TipFollower::show_for_merchant_slot /
  show_for_merchant_card).
- Card trails linger: the trail was freed after the card's fade; the game
  frees it with the card (NCardFlyVfx.OnCardExitedTree).
- Hitches on the first card plays: first-use loads. The logic sends the
  game's preload sets (AssetSets.CommonAssets, the run's characters', the
  act's, the deck's portraits) with the first decision of a run and of
  each act (Preload.cs); SceneLibrary::warm reads them a few per frame
  (24 files, 4 textures) and keeps those textures (Canvas::prefetch);
  MenuFlow waits for it behind the embark wipe. 803 files, 391 textures,
  30.8 MB of the 80 MB texture budget. `spire_play --load-trace` lists
  what is still read on first use.
- Enemy attacks right after End Turn: the waits match the game's fast
  mode (banner, 0.5 s, PerformIntent 0.25 s); the early hit was the
  first-use stall above.
- The bird elite hurt for no reason, and the last turn's draw of 2:
  Hellraiser's auto-plays. A card drawn and played within one command's
  steps is not in the decision's hand, where the client builds the cards
  coming in, so its draw, play and damage were invisible. Steps.cs now
  gives such a card's coming-in cue its view (AttachPassingViews).
- A selected card left on top after X and a Power Potion: the potion menu
  takes the focus in the game (the hand card goes down); now it does here.
- Character select: back to the Ironclad from a locked character
  restarted its background. Only the unlocked branch was ported: the
  locked one (NCharacterSelectScreen.SelectCharacter) keeps the
  background, shows the locked info and tip; the select sound and the
  weak shake play as the game's.

The same causes elsewhere (the user's request):
- Overlays: every overlay's background now sits in a CanvasLayer below
  the screen (overlay_behind; the engine sorts by layer, then z), as the
  CombatUi's absolute z had let hand cards draw over them.
- Sounds the game's node code plays from data (not from the logic):
  NAncientDialogueLine's line sound (the logic sends each line's sfx),
  Neow's ambience loop (AncientEventModel.AmbientBgm), the act's chest
  sound as the chest opens, the battle start banner's two sounds at random.
- Navigation sounds: a table of every NButton subclass's hover and press
  sounds (overrides, base calls, hotkeys: a hotkey presses the button).
  The run's catch-all (any move a hover, A a click, B a back) was wrong
  on the map (points have no hover sound), card grids (no focus sound, A
  and Y click), the proceed buttons (silent), the skip buttons (silent),
  inspect screens (silent), the shop's slots (silent), the potion holders.
  RunScreen::own_ui_sounds: the map, rewards, card reward, rest site,
  event, shop, treasure, card select, bundle select and crystal sphere play
  their controls' own; the game over screen keeps the defaults (they
  match). The main menu's screens had none at all: the compendium and its
  screens, settings, character select (back, embark, the hover before the
  select), the timeline's back click, credits. The pause menu: back on
  B / Start, a click on Resume. The top bar's hotkeys (deck, map, pause)
  click as their buttons do.
- Tips: the rewards screen's buttons show Reward.HoverTips left of the
  button (the logic sends them); the card library's filters show their
  tip at (310, the filter's y). Settings rows' tips are mouse-only in the
  game (none here, as there).
- The card library: View Stats (Victories: the card's TimesWon) and
  Multiplayer Cards (unticked: no multiplayer-only cards) were hidden
  here; the game shows both.
- Transient entities, the Hellraiser cause in general: a power applied and
  gone within one command's steps now carries its view (the client looked
  its look up in the final decision); so does a creature summoned and
  killed within them (the decision's enemies are the living), which the
  client now builds hidden from that view so its spawn, hits and death
  play. None occur in the seeded runs; the mechanism is the cards'.
- Restarts, the character select's cause in general: the client builds a
  room's screen anew for each of its decisions (a purchase, an event's
  next page, a rest site's choice, the card removal's selection over the
  shop and back) where the game keeps its nodes, so the merchant's idle,
  Neow's background, the campfire and their particles started over each
  time. GameClient::collect_loops / restore_loops carry what loops (Spine
  tracks with the same looping animation, AnimationPlayers, emitters:
  warmed up as long as they ran, a lifetime at most) to the new screen by
  node path, the path keyed from the room scene's root (a script ending
  in Room.cs or Layout.cs) so the room drawn behind an overlay matches.
  The user's session: every shop rebuild 25-27 of its loops carried, the
  rest site 12 of 12, the shop back from the removal 27.
- Checked and fine: the other z_index raises (focused grid cards: the
  game's MoveToFront), the locked and unseen branches of the collections,
  bestiary, timeline and library, the potion slosh moments, the run
  history entries' sounds (dead code in the game: nothing calls AnimateIn).

Replaying a console session: the console logic logs every command as
`[CMD] {json}` in spire360_logic.txt (SPIRE_LOG_COMMANDS, set by
logic_glue.c; passed through logic_host) and copies the profile's saves to
`spire\data\session_start` as it starts. Copy pack-80/data, put
session_start's files in as user/, then
`SPIRE_DATA=<copy> out/engine/spire_play work/pack-80/pack.s3p <out> --logic "dotnet out/logic/jit/Spire.Logic.dll" --commands spire360_logic.txt`
(the commands before the first action start the session). `--frames-after
N:EVERY --settle 300` captures one screen's animation; `--pad-after N
--pad ...` with a cut command file drives a moment by pad; `--load-trace`
lists first-use loads and slow frames; `--audio-trace` logs every sound
started.

Checks (JIT logic, final engine):
- seeded traces against traces/base6: differences 0 (traces/new92);
  scenarios 30 ok (work/scen92); content coverage 829 cases ok
  (work/cov92); spire_check over new92, the scenarios (continued ones
  split: work/split92) 2475 responses, 0 failures, audio 22889 voices, 0
  decoder failures (the 7 absent events are enemy cast sounds the game's
  banks lack too); PC build 0 warnings, the C89 checks 0.
- The console's logic (native host, out/logic/c as built): the 15 seeded
  runs 14 byte-identical to the JIT's, the other an unseeded monster skin;
  no [ERROR] or trap. With SPIRE_LOG_COMMANDS it logs [CMD] lines and
  copies the saves to session_start (File.Copy failed there: .NET copies
  the file's mode, which the WASI runtime has no call for; the bytes are
  copied instead). logic_host by random_runs needs an absolute path
  (Windows' CreateProcess does not find a relative one with slashes).
- By pad and replay: the card library walked to its toggles (the sounds
  traced, the filter tips, View Stats); a fresh run traced (Neow's
  ambience and first line, both battle start sounds); the user's session
  through its shop with the loops counted (above).

Next console build: the logic changed (`out\logic\c` rebuilt here), so
`xdk\build.cmd spire360` (or build_light) recompiles logic_lib; the engine
changed; the pack is unchanged. Restage the release folder with the new
default.xex: `python tools/stage_release.py` (out/release/spire360: the
XEX as default.xex, spire360.ini with both options off, spire/pack.s3p and
spire/data). With the
next report, send spire360_logic.txt and `spire\data\session_start`.

Ahead (the user, 2026-10-26): once the XDK builds are in a good state, a
branch ports the console build to xSDK and vulkan4xenon (both mature now);
when it works it becomes main and builds move to GitHub Actions (as
ctr360's). The first alpha RC will likely come from it. Expected in a few
days; not started. The RAM, threading and texture budgets are the
hardware's and carry over; keep XDK-only APIs out where a portable path
exists and the D3D9 rendering contained, so a Vulkan backend can replace it.

## Before the console build (2026-10-25): every angle, the debug options, and what they found

The user asked for every angle before spending an XDK build and a Xenia
(and first real-console) test on it. The earlier audits all compared the
logic with the game or replayed recorded runs; what nothing had done:
run the console's own logic build, drive the client with a pad the way a
player does (menus included), or look at what only real hardware
enforces. That is this section.

Debug options (the user's request): `game:\spire360.ini` next to the XEX
(sample: `xdk/spire360/spire360.ini`; no file, all off), read by
main.cpp into the logic's environment:
- `invincible=1` (SPIRE_INVINCIBLE): the player keeps their HP; each hit
  plays as dealt (number, spark, hurt animation, shake, the lose-HP hooks)
  and the HP is put back (patch debug.invincible: Creature.LoseHpInternal
  in CreatureCmd.Damage; the game kills only a target that is also dead).
  Max HP losses still apply. A run with it: 15,776 player hurt animations,
  the HP changed only by Max HP and heals.
- `godmode=1` (SPIRE_GODMODE): the game's own dev console godmode
  (GodModeConsoleCmd: Strength, Buffer, Regen 999999999 at each combat's
  set-up); a random Silent run won in 422 steps.
- Off, the logic is the same game: seeded traces 0 differences.
On PC the same environment variables work (dotnet and logic_host).

The console's logic (AOT -> wasm -> C) on PC: the logic had changed in
five commits since the last Xenia build and only the .NET JIT had run it.
build_logic.py stages wasm, c, native (MSVC from vcvars64, 2 jobs: 9 min)
and the native host played the 25 seeded runs: 23 byte-identical to the
JIT's, steps included (tools/strict_compare.py: every key but the wall
clock and host paths); the other 2 differ only in the game's unseeded
visual randomness (monster skins, one effect value), which differs between
two JIT runs of the same seed too. trace_compare's "differences" there are
line 0 only: the ready line's data_dir. The logic's linear memory: 69 MB
through a whole run.

logic_host: `--data DIR` (and SPIRE_DATA from the environment) mounts a
data folder as "/", so the coverage tools' per-case copies work; the
debug options pass through; it exits when the logic does (it waited on
stdin before: each run cost the driver's 30 s kill) and prints the linear
memory at exit.

**The boot blocker** (found by the first menu-mode run): since cb355bd
the logo waits for the logic's first main_menu answer (menu_ready_), but
MenuFlow::update returned during the logo before game_->update, the only
reader of the logic's pipe: the "ready" line was never read, nothing was
sent, and the game would have stayed on the logo's black forever (Xenia
and hardware; the last Xenia build predates cb355bd). The skip-intro wait
(title_waiting_) had the same hole. Both now read the logic while they
wait. No earlier check could see it: every capture since used
--replay-run (no menus).

Random pad input (spire_play `--menu --monkey SEED`): from the logo on,
a weighted random press every 3-20 frames (held now and then), also
while effects play; each run screen takes 0-59 presses, then B once and
auto play answers it so runs go on; Quit starts the menus again. Reports:
the logic's errors, long frames, a client busy for a minute, no progress
for 5 / 10 minutes (PNG each), memory each minute, the last 30 presses
with any report or crash. `--sync` takes each command's answer as it is
sent, so a session with `--seed` repeats press for press (the logic's
speed no longer moves the frames). `work/monkey90.sh` runs sessions.

The crash it found (seeds 7 async, 11 sync), and its class: the
Timeline's first-time tutorial fades out (0.5 s tween on its
modulate.a) when A is pressed, and the logic's answer deleted the node at
once; the tween kept writing into freed memory for the rest of the half
second. In release that memory was reused (JSON maps: a crash in
Rb_tree::find); on the console a heap corruption after the first game
over that unlocks an epoch. Found with the debug build:
tools/viewer/build_debug.sh (out/engine-dbg: -O0 -g, -DSPIRE_DEBUG_HEAP,
tools/viewer/debug_heap.cpp: freed blocks poisoned and held back, writes
into them reported with the deleting frames and, through a TweenSet hook,
the frames that started the last tween on that address;
RtlCaptureStackBackTrace, since Cygwin's x64 code keeps no rbp chain).
Fixed for the whole class: TweenSet keeps a list of the live sets and
Node's class operator delete (which gets the deleted node's own size)
makes every set drop its tweens inside the node; CanvasItem's destructor
does the same for its shader parameters (the map's own blocks). The
Timeline also holds the answer until the tutorial has faded, as
NTimelineTutorial.CloseTutorial spawns the timeline after its tween.

Real hardware (what Xenia does not enforce), reviewed one by one:
- Controller slot: the XEX read pad 0 only; a console can sign the pad in
  to any of the four. It now ORs XInputGetState of all four
  (XUSER_MAX_COUNT).
- Memory: every allocation on a console is backed by its 512 MB (about
  480 for a title); Xenia gives virtual allocations host memory. The XEX
  image alone is 104 MB; the logic's linear memory 69 MB (reserved 384,
  committed as it grows) plus its 16 MB thread stack; textures up to the
  canvas's 80 MB budget plus the render targets; the engine's heap (the
  SceneLibrary caches every scene file it parsed and never evicts; see
  the memory measurements below). The log now has "memory at start / with
  the logic: N MB free of M MB, logic K MB" and, every 10 s with the frame
  times, the free memory and the logic's size; "memory low" under 24 MB
  free; the least free at exit.
- The logic's stderr (warnings, errors, exceptions; 10-25 KB a run) went
  to the debug output only: lost on a console without a debugger. It now
  goes to game:\spire360_logic.txt (new each start). A logic thread that
  ends without an error answer is logged (it froze the screen silently),
  and the runtime logs a memory.grow it cannot commit (out of memory).
- Command ring buffer: D3D9Renderer sent a whole batch per
  DrawPrimitiveUP with no cap; the XDK's ring buffer is 2 MB in 32
  segments of 64 KB (D3DRING_BUFFER_PARAMETERS defaults, from the XDK
  help). A draw now holds at most 64 KB of whole triangles (2046
  vertices); only very large batches split.
- Checked, no change needed: wasm loads and stores (w2c2 copies through
  integers and byte-swaps: no unaligned float access), EDRAM layout (back
  buffer, group surface, screen copies inside 10 MB, no depth buffer),
  texture tiling (XGTileTextureLevel), no devkit-only APIs or xbdm, the
  logic pipe (mutex + condition: safe under the PowerPC's weaker memory
  ordering), FATX names (the longest the game writes is 31 characters of
  42), the new XDK names (XUSER_MAX_COUNT, GlobalMemoryStatus,
  MEMORYSTATUS: in the XDK headers).

Random-input results after the fixes: 18 sessions of 5-6 minutes (12
--sync on the release build, 3 on the debug build with the checking heap,
3 with the logic's real latency as on the console; invincible and godmode
among them), each from the logo through the menus (profiles, settings,
compendium, timeline) into runs, Save and Quit, Give Up and Quit: about
900 decisions, no crash, no write after delete, no stall. Two tool
artifacts were fixed on the way (the monkey waited for "busy" to end
while the shuffle FTUE paused the fight for a press; godmode's queued
plays kept the client busy across decisions).

Save and Quit / Continue across whole runs (random_runs.py `--game
--save-quit P`: once per floor at a random decision, P = 0.3; 10 runs,
every character, 15-40 continues each, plus Ascension 10): it found the
Crystal Sphere's: the driver kept the minigame it was showing
(Minigames.cs `_crystalSphere`) through Save and Quit, so Continue showed
the old model instead of the event again, and finishing it threw in
OneOffSynchronizer.DoLocalCrystalSphereRewards (the old run's player is not
the local one): no rewards. EndRun now drops the minigame fields (and
`_eventEntering`, `_restSync`); scenario `crystal_sphere_continue` failed
before and passes after. With the fix the 10 runs win, 0 errors.

Invincibility and kills that are not damage: two invincible runs died at
full HP (The Insatiable's Sandpit eats the player; Doom). Patch
debug.invincible_kill (CreatureCmd.Kill(Creature, bool) in DoomPower,
SandpitPower, TheGambitPower, TabletOfTruth) spares the player; Give Up
and a won run still kill (RunManager, not patched). The Sandpit hid the
body first: a debug cue NCreature.ShowVisuals brings it back (the client
handles it).

Memory (spire_play, 64-bit, no textures): a whole run's heap grows only
with the scene cache, about 40 KB per scene file (75 MB at 534 files, 80
MB at 664, Act 2); menus and the compendium add their scenes. The 150 MB
some random sessions showed were the tool's own FTUE screenshots (the
software renderer's decoded textures: debug_heap's "big block" report).
Console estimate: XEX 104 + logic 69 + its stack 16 + engine heap ~60-90 +
textures <= 80 + targets ~12 + D3D ring 2 = about 345-375 MB of ~480: it
fits, with the log to confirm it on hardware.

The console's logic, game flow (found only by running it there; the JIT
passes all of these):
- Unlock all (the profile screen's toggle) threw NotSupportedException:
  SetUnlockAll wrote spire360.json with System.Text.Json's reflection
  serializer, which has no code for Dictionary<string, bool> in the AOT
  builds. It writes with the protocol's own writer now (Json.Serialize).
  The coverage act runs turn unlock-all on first, so all 11 failed on the
  native host until then.
- A run save from the PC game whose map has drawings could not be read
  ("Unrecoverable: the underlying compression routine could not be
  loaded"): SerializableMapDrawingsJsonConverter gzips the drawings and the
  wasm link left out System.IO.Compression's native shim and zlib.
  Spire.Logic.csproj now links libSystem.IO.Compression.Native.a and
  libz.a (target SpireLinkCompression, before LinkNativeLlvm) with a
  DirectPInvoke; profile_check on an imported PC profile passes natively.
  (Declaring the DirectPInvoke without the libraries made the call an
  "unreachable" trap: the logic thread died. Both or neither.)
- The Fake Merchant's fight (a Foul Potion thrown in its shop) broke the
  next fight: RunDriver enqueued card plays and potions straight onto the
  ActionQueueSet, outside Sched.Call, so a fight a potion started ran its
  turn loop with no synchronization context; at the fight's teardown
  (CombatTurnState.Cancel, under the context) .NET could not inline that
  continuation and queued it to the thread pool, which the console's
  runtime never runs: the turn loop never ended and the next room showed
  rewards instead of a fight. Both enqueues run under Sched.Call now. The
  logic warns "thread-pool work ran during <cmd>" on the JIT whenever a
  command runs pool work (none in the seeded runs or the 829 coverage
  cases after the fix); Sched.Settle names the pending items on WASI.
  Scenario `fake` now plays a fight after the Fake Merchant's.

spire_check on save-and-continue and pad traces: it replays decisions only,
so tools/split_traces.py cuts traces into segments at each continue_run /
new run (a continued fight is a fresh build in the client); three check assumptions
were fixed on the way (a played card in flight counted as a hand holder,
the room taken as the root's first child while an unanswered confirm popup
sat before it, a card tip's art taken for the portrait layout).

Known, not changed: the console's logic formats "Last Saved" in UTC (WASI
has no time zone; the PC shows local time). Language switching is not
reachable on the console pack (its languages.json offers English only).

Next console build: the logic changed and `out\logic\c` is already built
from it here (stages patch, wasm, c, native), so `xdk\build.cmd spire360`
is enough (logic_lib recompiles: the whole translated logic changed); the
engine changed; the pack and data are unchanged (work/pack-80). Copy
`xdk\spire360\spire360.ini` next to the XEX for the debug options.

Checks, final logic and engine:
- JIT: seeded traces against traces/base6, differences 0 (traces/new90c;
  strict against the earlier JIT runs: identical but the unseeded skins);
  content coverage 829 cases ok (work/cov90j), no command running
  thread-pool work; scenarios 30 ok (double_boss_continue,
  crystal_sphere_continue, fake with the next fight among them); save and
  continue runs 10 of 10 won (invincible), 0 errors.
- The console's logic (native host, out/logic/c as built): seeded runs 22
  of 25 identical to the JIT's (3: the unseeded skins); coverage 818
  cases ok before the act runs, then the acts suite 11 of 11; scenarios 30
  ok; profile_check 6 of 6 (an imported PC profile with map drawings);
  save and continue runs 4 won and 1 at the step limit (the Regent's run
  takes the corrected Crystal Sphere path); no [ERROR], trap or pending
  thread-pool work in any of its logs; linear memory 69 MB.
- Pad: 22 random sessions in all (the last 4 on the final build, 2 of them
  driving the console's logic), no crash, stall or write after delete.
- spire_check on the seeded runs, scenarios and the segmented
  save-and-continue and pad traces: 20,322 responses, 4,954 orb checks,
  16,310 hand checks, 0 failures; audio 0 decoder failures, 7 absent
  events, all the game's own default paths missing from its banks
  (Vine Shambler, Slithering Strangler, Brute Ruby Raider, Crusher,
  Rocket, Bowlbug Silk, Living Shield).
- Builds: the PC tools with -std=c++98 -pedantic -Wall, 0 warnings;
  srt_core.c and logic_host.c strict C89 -Werror; the D3D9 renderer check.
  No XDK build.

The XDK build (2026-10-26, the user's xdk\build_light.cmd): logic_lib
built; spire360 stopped at one error, cue_ledger.cpp's getenv (C3861). A
Release build defines NDEBUG and with it _XBSTRICT, under which the XDK's
CRT headers drop getenv and the rest of the environment, working
directory, process and console functions; g++ -std=c++98 declares them,
so the PC build passed. Fixed (the cue trace is off on the console) and
for the class: tools/viewer/xdk_api_check.py, which build.sh runs in place
of its C99 grep, lists calls in the XEX's sources and the runtime to the
C99 functions VS2010 lacks and to the CRT functions _XBSTRICT drops,
skipping lines the console compiles out (_XBOX, _MSC_VER). A one-off scan
of every C library function the XEX calls against the XDK's headers (with
_XBSTRICT) and of every <header> it includes found nothing else; the
symbols main.cpp adds (GlobalMemoryStatus: xapilib.lib) are in libraries
the project links. The C files (logic_glue.c) had not compiled yet: they
come after the C++ batch.

The rebuilt XEX (0 errors) is staged for the console in
out/release/spire360 (ignored): default.xex, spire360.ini (both options
0), spire/pack.s3p and spire/data from work/pack-80 (copies verified, no
name over FATX's 42 characters, no data/user saves). The spire folder next
to xdk/spire360/Release's XEX still holds the 10-20 pack and the Xenia
test's saves: pack-80 replaces it.

Cleanup (2026-10-26, the user's request; 38.4 GB freed): the superseded
packs (pack, pack-next, pack-sound, pack-sound-effects, pack-audio,
pack-lang, doom-pack, the audio test packs, out/pack.s3p and out/data, the
two old spire folders under xdk/spire360/Release), every old run folder in
work/ (cov*, cap*, scen*, sq90*, monkey90, pace*, ftue*, formp-*, ...),
every trace set but base6, new90c and native90c, and the build output of
the old XDK test projects (spire_view, logic_runner, bigcode). Kept in
work/: toolchain, pack-80, play-data, import-test, cov90j and scen90 (the
latest coverage and scenario runs, the audits' inputs), the extracted
resources and references (res*, *-reference, *-src, fmod-parser, legacy,
baseline-shots, setup), xenia-saves (the Xenia tests' data/user folders,
10-16 and 10-22) and all loose scripts. Kept elsewhere: out/logic,
out/engine*, out/release, xdk/logic_lib (its incremental build),
converter/.cache, logic's obj. A language pack comes back from pack-80
with `convert.py refont` (fonts only). Run folders named in the sections
below are gone unless listed here.

## The four review lists (2026-10-25): every item has a verdict, and what they found

The presentation audit's long lists (call 171, script 163, asset 124) and
ui_reach_audit's 193 sites are all answered now (notes/
presentation_verdicts.tsv; "reach" rows for the reach audit). Order: the
logic-to-UI boundary first (call, then reach), then the client side
(script, then asset), since the last two shrink as the first two are
ported. Only custom and daily runs stay open (deferred past the alpha RC).

How each list was read:
- call: a UI method game code calls. Most are handled where the logic
  records them another way: a redirected or hooked caller (SfxCmd, the
  pile tween, CardCmd's previews, ...), the decision (rooms, screens,
  music from context.audio), or state diffs (orbs, hand). "runs: 0" there
  is a naming artifact (SpineAnimationAccess's calls are recorded as
  MegaAnimationState's) or a caller patched away. Method: map each callee
  to its callers (work/ui_surface.json) and each caller to its patch
  action in patches.json, then read what is left.
- reach: a UI singleton read no run reached. Fixed the audit first: it
  named property sites by the property ("Crusher.Background") while the
  ledgers record the getter (get_Background), so no property site could
  count as reached. The 58 Timeline sites: no suite revealed epochs; the
  new coverage suite `epochs` does.
- script: a scene script with no registered port. 119 are driven by the
  screens that cite them; the other 44 were read one by one.
- asset: pack files the sweep never loaded, by directory. 959 files; 814
  are roots (nothing in the pack references them), checked for a game code
  user (the game's text scenes: pck_tool extract "*.tscn" "*.tres"); the
  sweep replays runs only (no menus, no client input such as map travel),
  so menu assets are answered by their ports. Shader resources are
  compiled into the pack's effects and never "loaded".

Ported (each a gap the lists named):
- Creature animation: CreatureAnimator.OffsetLoopingAnimation (each
  looping state at a random 0.9-1.1 speed, its start within 0.1 s of the
  loop point: identical monsters no longer animate in lockstep) and the
  constructor's random idle start. The animator ops carry "offset" (every
  looping state set or queued) / "random_start" (an initial idle_loop),
  read from the calls: the stand-ins answer GetCurrent with null, so the
  track entry's own calls never arrive. The client rolls its own
  (Rng.Chaotic). SpAnimationState gained time_last / set_track_speed.
- The animator never moved past a state that does not loop: the game's
  CreatureAnimator.OnAnimationCompleted (a sprite signal, never fired
  headless) moves it to the state after, so the logic's animator stayed on
  e.g. the Owl Magistrate's take_off and its Attack (the dive, from
  fly_loop) found no branch: nothing played. Animators.Settle moves it on
  after each trigger, as completion would (a trigger mid-animation branches
  from the next state: the same branches in practice).
- A state's BoundsContainer (CreatureAnimator.BoundsUpdated ->
  NCreature.UpdateBounds): the intents move to that container's IntentPos
  (Thieving Hopper and Owl Magistrate flying, Cubex Construct burrowed,
  Test Subject respawning). %Bounds itself always resolves to the root's
  (unique names are the scene owner's), so the hitbox stays. Ops "bounds",
  view "anim_bounds".
- A Doom kill's pose: DoomPower.StartDoomAnim's Hit trigger with the hurt
  animation frozen at HurtAnimationTrackOffsetForDoom (Inklet 0.04), on the
  NDoomVfx cue ("pose", "hurt_offset"); the body was taken into the effect
  still playing its idle.
- Sand Castle: NGridCardPreviewContainer.ForceMaxColumnsUntilEmpty(3)
  (its upgrades 3+3, not 5+1), until the grid empties.
- The act 1 map marker on Neow's point (RunManager.EnterAct's InitMarker;
  the driver records when its EnterAct ran it: map "marker_at_start").
- Trial's Double Down opens the abandon-run confirmation over the event
  (Confirm gives up, Cancel returns); nothing happened before.
- The Doll Room's ambience never stopped (the testmode hook played it
  itself, so the event kept no handle): TestMode reads off there now and
  the game plays it through the ui hook, which gives it a handle; StopAudio
  stops it when a doll is taken.
- **The act's second boss (Ascension 10, DoubleBoss) was skipped**: the
  driver went to the next act straight after the first boss's rewards.
  Both proceed paths now follow NRewardsScreen.OnProceedButtonPressed: the
  first of two bosses proceeds to the map (the second waits there),
  otherwise the act change goes through the game's vote
  (ActChangeSynchronizer.SetLocalPlayerReady: MoveToNextAct, ActFloor + 1,
  EnterNextAct), not EnterNextAct directly. The acts suite ran Ascension
  0-9 only; it runs 11 runs now (0-10) and an Ascension 10 win without the
  second boss fails the case. That run died in act 1, so scenario
  `double_boss` (an Ascension 10 game-flow run moved to the last act with
  the test command `enter_act`, tanked) checks it: Test Subject at floor
  15, the Queen at 16, The Architect at 17 (MoveToNextAct's ActFloor + 1:
  the act runs' victories now end on floor 16, not 15).
- An enemy-aimed potion with no enemy to aim at (between a boss's
  respawns): the driver started UsePotionAction anyway, which cancelled,
  and then discarded the potion as "not consumed". It refuses now (the
  game's targeting has nothing to aim at; the potion stays).
- Hit effects with unported scripts (VfxCmd plays them by path): Heavy
  Blunt (Bludgeon, Break, All For One: its strong shake at 0.2 s), Starry
  Impact (Astral Pulse), Scream / Spooky Scream (continuous particles for
  _duration, then off; they ran forever), Hellraiser's attack sword (it
  stayed on screen). Each was also never freed.
- Decimillipede's falling rocks, the Symbiote event's moving eye, the rest
  site's flame (NRestSiteFireVfx: Flicker and Sway), the Dark orb's
  background at 1.2 when supercharged.
- NAncientBgContainer: the Ancient's background (Neow at every run start)
  at 0.89 about its centre, 40 px down (its 16:9 values); it was full size.
- The sleeping Zs in Japanese (LocalizedTexture).
- The Embark wipe: the character's transition sound (wipe_<id>; the Defect
  and the Necrobinder use the Ironclad's) and NTransition.FadeOut with the
  character's transition material (0.8 s), then the first room fades in;
  the run started with neither.

Answered without a port (examples): the play queue's cancellations (the
console sends one play at a time), Mend and every DoRemotePostSelectVfx
(multiplayer), RelicSelectCmd (no caller), LinkedRewardSet (nothing makes
one), the Glass orb's passive impact (only the debug tester),
NAncientMapPoint.TargetMaterial (no user), the rest site light upscaler
(the texture is its original size in the pack), PlayerCombatState.
RecalculateCardValues (only Adroit uses it, already set at enchant; no
enchantment stacks).

Tools: content_coverage suite `epochs` (every epoch obtained and revealed
in the Timeline's order; `obtain_epoch` test command; content lists
`epochs`), `--act-runs` 11; scenarios `doom`, `double_down`, `doll_room`,
`sand_castle` (each checks its cue), `double_boss`; test command
`enter_act`; ui_reach_audit reads property accessors.

Checks: seeded traces against traces/base6, differences 0 (traces/new89d,
after every logic change here); content coverage cov89: 829 cases ok (the
11 act runs, epochs: 57 revealed, 69 unlock screens), the encounters again
after the animator settle (cov89e: 104 ok; empty Hit results 74 -> 8, the
rest burrowed states with no Hit branch, as in the game; the empty Attack
results are Knowledge Demon's and the Decimillipede segments', which have
no Attack branch); the sweep over cov89 (pack-80): 52,581 decisions, 184
gaps, 56 cue kinds never played, every one with a verdict, no crash (48
min); scenarios scen89: 28 ok; spire_check on new89d + scen89 + mecha62:
3,532 responses, 2,649 hand checks, 0 failures (the audio's 3 absent
events are the game's own, as before); strict C++98 builds. Audits:
presentation only custom and daily runs open; ui_reach 486 sites, 357
reached, 0 open; model coverage complete (relic flashes 157/157);
testmode 0, hook callers 0. Captures (work/cap89): the Doom kill's hurt
pose, the Owl Magistrate flying (its intents at FlyingBounds' IntentPos),
Trial's abandon popup, Sand Castle's 3+3 grid, Neow's background at 0.89.

Next console build: logic (all stages: patches.json changed), engine; the
pack is unchanged (work/pack-80). The Embark wipe is menu-only (no replay
reaches it): look for it at the first Embark in Xenia.

## Relic triggers (2026-10-25): every relic's trigger fires, and what that found

The 28 relics whose trigger never fired (part 3's list), and the five more
the reworked suite then missed, all fire now: the relic suite checks it
itself. A relic whose code calls Flash() and that never flashed in its own
case is "untriggered" (a "model" verdict `relic flashes:<ID>` answers one no
solo run can fire; none needed so far).

Why they never fired, and the suite's answers (content_coverage.py):
- The trigger kit never ran out: Whirlwind with 20 energy killed the enemy
  halfway, so the power, poison, discard and exhaust cards after it were
  never played (Game Piece, Lost Wisp, Snecko Skull, Tough Bandages, ...),
  and True Grit's random exhaust and Survivor's discard ate kit cards. The
  enemies are healed before each kit card, Whirlwind comes last with 6
  energy, a prompt takes what the kit no longer needs (a Defend held for
  it), Dominate is the exhausting skill.
- Counters and situations no generic play reaches: RELIC_RECIPES (seven
  orbs, ten attacks or skills, ten stars spent, an Osty attack, the seventh
  turn, six fights, an act 1 boss, over 20 HP lost in a turn, five
  exhausts, an enemy dead to Doom, travelling the map to marked or unknown
  rooms). Every case also gets a power and a curse in the deck (Jeweled
  Mask, Darkstone Periapt), enemies with block in the second fight (Hand
  Drill, `set_player enemy_block`), a Retain card held (Bookmark), a draw
  on its own turn (Fiddle), self-damage (Demon Tongue), a potion drunk
  (Reptile Trinket), Heal at the rest site when the relic has no option of
  its own (Dream Catcher), a purchase (Maw Bank).

Bugs found on the way (all fixed):
- **The card reward screen had no alternatives**: Pael's Wing's Sacrifice
  and Driftwood's Reroll could not be chosen on the console. A fight's card
  reward was taken by a driver shortcut (the card added to the deck by
  hand), not CardReward.OnSelect: no alternatives, no run history card
  choices. Both fights' and events' card rewards now go through OnSelect
  (the selector's nested decision); the decision lists `alternatives`
  (index, id, label; Skip stays `can_skip`), `choose_card_reward_alternative`
  takes one, and after a Reroll the screen's list is generated again without
  it (CanReroll false), as Populate does. The client builds the buttons in
  the screen's own UI/RewardAlternatives row (Skip on B, the other on RB).
  Captured: work/cap86 (card_sacrifice, card_reroll).
- **Relics that changed a reward card did not flash** (Lava Lamp, Glitter,
  Silken Tress, Lasting Candy, Fresnel Lens): NCardRewardSelectionScreen.
  RefreshOptions flashes them and puts their icon on the card
  (NCard.FlashRelicOnCard). The selector flashes them as the cards are
  shown; each card carries `modifying_relics` and the client runs the card
  flash (flash_relic_on_card, shared with the deck previews). Captured.
- **Maw Bank never flashed in the top bar**: the relic_flash cue came from
  Player.OnRelicFlashed, which a relic with ShouldFlashOnPlayer false (or a
  melted one) is not subscribed to, while the top bar holder flashes for
  every relic. The cue now comes from RelicModel.Flash(targets) (patch
  `relic.flash`), its targets empty when the Player handler would not run.

Scenarios: `card_sacrifice`, `card_reroll`.

Checks: seeded traces against traces/base6, differences 0 (traces/new88;
the card reward path changed none of the game's choices); content
coverage cov84: 827 cases, all ok, all 299 relics triggered (25 min); the
sweep: 51,875 decisions, 189 gaps, 58 cue kinds never played, each with a
verdict (47 min); scenarios scen88: 23, all ok; spire_check on new88 +
scen88 + mecha62: 2,303 responses, 1,771 hand checks, 0 failures; model
coverage (cov84 + scen84): powers 242/265 plus 23 verdicts, orbs 4/4,
monsters 107/107, enchantments 22/22, afflictions 7/7, relic flashes
157/157; testmode_audit and hook_caller_audit 0 open; strict C++98 builds
(reward_screen.cpp too: it was missing from the check list).

Left as before: custom and daily runs (deferred), and the long review
lists (call 171, script 163, asset 124, ui_reach 193).


## Completeness audit, part 3 (2026-10-24): model coverage, and what it found

The game folder is back: the pack was rebuilt (`work/pack-80/pack.s3p`,
`SPIRE_DATA=work/pack-80/data`; 0 errors). It has the Image imports now
(the map drawing cursors, cursor_quill.png ...: captured, work/cap81/draw).
spire_check against it: the same 2,433 responses, 0 failures.

**Model coverage** (`tools/model_coverage.py --runs work/covNN [--runs ...]`):
every ModelDb model that no suite drives by construction must show up in
some trace: powers (a power cue or a creature's powers), orbs, monsters,
enchantments, afflictions, and every relic whose code calls Flash() must
flash (its trigger ran; read from decomp/). "model" verdict rows answer
what a solo run cannot show (multiplayer-only cards' powers, four powers
nothing applies). Exits 1 while something unanswered was never seen.

Its first run named (cov80): no enchantment or affliction suite existed
(Slither's cost roll never ran); 31 powers never applied; 38 relics whose
trigger never fired. The causes, each fixed in the harness:

- Suites `enchantments` (each on the deck's cards it can enchant, `obtain`
  `enchant`, Neow first; Inky only on targeted cards as the game gives it
  to Shivs) and `afflictions` (`add_to_hand` `afflict`). 22 and 7, all ok.
- The coverage agent's random picks: the Knowledge Demon offered three
  curses and Disintegration was taken each time. `act_for` now takes a
  card or rest option this run has not taken yet; scenario
  `knowledge_demon_0/1` takes both sides explicitly.
- Kill branches (The Hunt, Feed, Hand of Greed, Sunder, Knockout Blow,
  Echoing Slash): the cards suite's upgraded pass kills with every attack
  (enemy_hp 1); the plain pass heals the enemies first (`set_player
  enemy_heal`) so an attack's after-effects (Rebound's power) run.
- A card that leaves a new power on the player gets two turns (Shadow
  Step's Double Damage), not only Power cards.
- Mad Science's nine variants (scenario `mad_science`: Curious, Improvement).
- The relic suite: the relic's own character; a kit of trigger cards in
  each fight (Shivs, an X cost, an exhaust, a discard, an orb, poison,
  Strength, stars spent: RELIC_KIT), two more turns, the first fight then
  run to its end, a fight at 40% HP (Red Skull), a third fight, a shop with
  999 gold, and a last fight at 1 HP (Lizard Tail).

Bugs it found:

- **The Knowledge Demon's curse could be skipped** (and any choose-a-card
  screen the game makes mandatory): CardSelectCmd.FromChooseACardScreen asks
  the selector for 0 to 1 cards either way and only the screen enforces
  canSkip, so the decision said min_select 0 and the client showed Skip.
  The selector now requires one pick without canSkip; skip_select refuses a
  mandatory selection.
- `enter_room` (the test command) did not reset the per-room state map
  travel resets: a second fight entered that way skipped its rewards (hid
  The Hunt's extra reward and every reward relic in the suites).
- `enter_room` on a dead player's run hung (the fight waited for the player
  to be ready); it now answers "The run is over". The harness has a
  watchdog: a command over 300 s kills its logic, the case is "hung".
- The top bar's context threw on a new profile's first act (no ancient
  unlocked: RoomSet.Ancient throws) and lost the boss icon state; Act.Ancient
  is read only for an Ancient starting point, as NMapScreen does.
- InitProgressData ran once before ModelDb (a known failure on every fresh
  profile); now only after it, as NGame does.
- **A freeze (and, in the sweep, an out-of-memory crash) after a fight
  that starts with a selection** (Gambling Chip's discard-and-redraw, which
  the relic suite reached by buying relics): the client built the room
  behind that first selection from the previous fight's last state, so the
  new fight's draws piled onto the old hand (11, 12 holders); the hand
  layout read past its 10-row tables (undefined behaviour: a holder at 43
  million pixels), a played card flew from there and its NCardTrail
  interpolated hundreds of thousands of points (a quadratic erase: the
  hang). Fixed at each level: the client drops the last fight's state on
  any decision outside a fight; a selection made during a fight carries the
  fight (`fight`, CombatPlayState) and the client builds the room from it;
  the layout clamps past 10 holders; a trail does not interpolate a jump
  over 4096 px. spire_check now checks every combat response's hand: at
  most 10 holders, each on the screen ("hand checks").
- Found by isolating the stall: cut the trace response by response, then
  a temporary per-phase and per-node trace (SPIRE_PHASES, removed) named
  the node. No debugger is in the toolchain; that method works.

Tools: content_coverage `--only ID,...` runs single items; `set_player`
`enemy_heal`; the harness watchdog (300 s per command).

Checks at the pause: seeded traces against traces/base6, differences 0
(traces/new84); content coverage cov83: 828 cases, all ok (22 min); the
sweep over it (pack-80): 42,021 decisions, 185 gaps, 58 cue kinds never
played, every one with a verdict (40 min, no crash); scenarios scen84 all
ok; spire_check on new84 + scen84 + mecha62: 2,284 responses, 1,767 hand
checks, 0 failures; strict C++98 builds. Model coverage (cov83 + scen84):
powers 241/265 seen plus 23 verdicts, Self-Forming Clay's power left (it
needs unblocked damage, which a run may or may not take); orbs 4/4,
monsters 107/107, enchantments 22/22, afflictions 7/7; relic flashes
129/157. testmode_audit 0 open, hook_caller_audit 0 open.

The audit's open and unclassified rows are now only these:
- custom and daily runs (5 scene rows), deferred past the alpha RC;
- the long review lists: call (171: UI methods game code calls, mostly
  MegaAnimationState / MegaSkeleton / audio managers), script (163: scene
  scripts the replays met without a port), asset (124 directories of
  scenes never loaded), and ui_reach_audit's 193 sites (NTimelineScreen
  58: the epochs' unlock queues, NOverlayStack 19, NGame 19, NCombatRoom
  17, NRun 17, NMapScreen 15, ...; no "reach" verdict rows exist yet);
- 28 relics whose trigger never fired: Bone Flute, Bookmark, Burning
  Sticks, Darkstone Periapt, Demon Tongue, Dream Catcher, Emotion Chip,
  Fake Orichalcum, Fiddle, Galactic Dust, Game Piece, Hand Drill, History
  Course, Jeweled Mask, Lava Rock, Lost Wisp, Maw Bank, Metronome,
  Nunchaku, Pael's Flesh, Pael's Wing, Screaming Flagon, Snecko Skull,
  Stone Calendar, The Abacus, Tough Bandages, Tuning Fork, Wongo's Mystery
  Ticket (the set shifts a little between runs: random play). Each needs
  its own recipe in the relic suite (a rest at a campfire, a purchase, a
  boss reward, an orb channelled, the seventh turn, ...).

Next console build: logic (all stages: patches.json changed in part 2),
engine, and the pack (work/pack-80: the cursor images, the fonts' pages).

RNG note (the user's pointer, Andy Tockman's "Correlated randomness"):
v0.111.0's Rng wraps MegaRandom (Xoshiro256**, seeds through SplitMix64),
not System.Random, so the older build's correlations do not apply; the
port runs that code, so whatever the build does is reproduced. Coverage
must never count on a lucky roll: force content, take every side of a
choice explicitly.

## Completeness audit, part 2 (2026-10-24): static audits, and the bugs they named

Coverage only shows what it drives. Three static audits now list
everything the game does that the port has to answer, so what no run
reached is named instead of found in a play test (each exits 1 while
something is unanswered; run them after a game update too):

- `tools/testmode_audit.py`: every TestMode.IsOn / IsOff read in the game's
  logic (130), each answered by a patch on its method or a testmode /
  godot verdict. 0 without an answer. It named Lord's Parasol's shop
  sequence (below); the rest were multiplayer, mods, leaderboards and test
  helpers.
- `tools/hook_caller_audit.py`: every Hook.* the game's rules run through
  (148) that only the UI reaches (directly, or through a game method only
  the UI calls). Headless the UI never runs, so the driver must. It named
  **Winged Boots** (MapTravel.GetTravelablePointsFrom: the map offered only
  connected rooms, the relic never flew). The same class, found by the
  continue scenario: **Lasting Candy** never counted its combat rewards
  (Hook.BeforeCombatRewardOffered, which only NCombatUi's
  OfferRoomEndRewards path runs; the driver generated the rewards without
  it). Both fixed; the remaining three are verdicts (`hook` rows: the
  Hoarder modifier, hooks nothing implements).
- `tools/ui_reach_audit.py --runs work/covNN [--runs work/scenNN]`: every
  read of a UI singleton (486) by the method that makes it, against the
  methods the ledgers saw run. The unreached ones were pre-empted by
  patches, UI-driven or multiplayer, or real coverage gaps, now driven:
  powers' turn triggers (the card suite follows each power with two
  turns; an unplayable card stays in hand through an end of turn), The
  Insatiable's death (its Sandpit kills outright: a lost encounter runs
  again with the HP cap after 10 steps), the player's death (scenario
  `death`), Save and Quit / Continue (scenario `continue`).

Bugs found and fixed in this pass:

- **Continue at a won fight's rewards offered them twice** (the game's
  StartPreFinishedCombat offered the room's set; when it was taken the
  driver generated its own). A fight's own set is now the driver's
  terminal rewards screen, and the rewards decision carries `room_view`
  (the room in FinishedCombat mode behind them: background, the player,
  no enemies; it was a blank screen). (Quitting at the map after taking
  rewards returns to the rewards with the state before them: the game
  saves only at a victory and on entering a room, so that matches it.)
- **The rewards' card icon** was always the plain one for a fight's
  reward (CardReward.ToSerializable refuses a reward with creation flags;
  the logic threw and fell back): Reward.IconPath is read by reflection.
- Event options' relic probes use ModelDb.GetByIdOrNull (GRAB_SWORD and
  other ids that are not models threw on every event page).
- **Relic icons were missing from the top bar since 835b0be**: the row
  moved to relic_inventory_holder.tscn (its counters) and the icon lookup
  (`%Icon`, unique in the holder's %Relic instance, not the holder) found
  nothing. `holder_relic_icon`; spire_check now checks every holder's icon
  after each response (3,828 checks).
- **Screen shake pinned a new room at zero size** when its first update
  came before the room's first layout (set_position keeps the current
  size): the shake now adds its displacement to the target's offsets. Seen
  as the shop drawn small in a corner during Lord's Parasol.
- **Affliction overlays**: AfflictionModel.HasOverlay is a
  ResourceLoader.Exists check, false headless; the logic sends the card's
  own overlay beside the affliction's and the client takes the one the
  pack has (NCard.ReloadOverlay). The other Exists checks are the UI's.
- The act suite always began in Underdocks (a new profile's first act is
  the undiscovered alternate): odd runs now use `discover_acts`.
- The save indicator ("Game saved") showed only when the top bar had a
  state before to animate from, and its cue was never marked played:
  NSaveIndicator.SavedGame's sequence now runs on the indicator itself.

Ported in this pass:

- Lord's Parasol at a shop (`testmode.presentation.is_off` for
  PurchaseEverything; `parasol.purchase_everything` records the shop as
  entered, `merchant.purchase_completed` each purchase, auto when no buy
  command made it): the client holds the decision, shows the shop, opens
  its inventory after 0.75 s, plays each purchase on its delay (cards to
  the deck, relics and potions to a top bar rebuilt as each lands, the
  thank-yous and a line), then the card removal over the bought-out shop.
  Captured (work/cap78/parasol).
- The peek button (left stick, MegaInput.peek; PadState::LS on the console
  and `LS` in the tools): in a fight's card selection the selection and
  backstop hide to show the fight; in the hand's selection mode every hand
  card shows, the creatures can be focused, a card pressed wiggles it
  (deny.mp3, Medium pitch variance). Captured (work/cap78/peek).
- Map drawing (RB: the tools; draw / erase with a cursor the D-pad moves,
  A held draws into map_drawing.tscn's viewport, B stops; clear with
  map_erase; the lines stay for the act). The software renderer's
  blend_sub takes alpha too (D3D9 and Godot do): the eraser. Captured
  (work/cap78/draw). The cursors (cursor_quill.png, ...) are Image
  imports the converter did not pack: it does now (GDIM .image files);
  until the pack is rebuilt the tools' own quill and eraser stand in.
- The Thieving Hopper's stolen card (ThieveryMove: NCard.Create under its
  %StolenCardPos, UpdateVisuals(Deck, Normal)): the logic's `held_card`
  cue when it steals and `held_card` on the hopper's state (rebuilt
  rooms); the card hangs from its card bone behind its body and goes with
  it. Captured (work/cap80/hopper).
- The timeline's chapter paginators, the unlock info as the game shows it,
  the titles' fades; the obtained slots' NEpochHighlightVfx and
  NEpochOffscreenVfx. Creature hover tips refresh at each sync.

Tools: `spire_play --pad` takes combos and holds (`A+RIGHT*40`) and WAIT
is a second in a run too; scenarios `parasol`, `queen`, `death`,
`continue`, `continue2` (quit mid-fight: the fight again from its start;
in an event; at a rest site), `candy`, `boots`, `powers`, `hopper`.
spire_check skips a death whose body stays (`keep`: the Eye With Teeth
stunned until it heals was reported as a dead enemy left visible).

Checks: seeded traces against the new baseline traces/base6 (the Winged
Boots fix changes the Defect run that has them: its map choices; checked
to be the only difference), differences 0 (traces/new80); spire_check on
new80 and scen80: 2,433 responses, 84 death checks, 4,539 relic icon
checks, 0 failures; strict C++98 builds. The 5 absent event calls are the
game's own (vine_shambler_cast, slithering_strangler_tail and
brute_ruby_raider_cast: MonsterModel.CastSfx's default path, not in the
banks). Content coverage cov79 (before engine / logic 80): all ok, 105
encounters, the sweep 41,664 decisions.

Audit (cov79 + scen80, verdicts added for what this pass answered): open
is only the custom run screens. Unclassified: two exceptions in cov79's
ledgers that logic 80 fixes (GRAB_SWORD, CardReward.ToSerializable:
confirm with the next coverage run), two to look at (RoomSet.Ancient
read before GenerateRooms, ModelDb.Get CHARACTER.IRONCLAD: 12 cases),
content Slither (NCard.PlayRandomizeCostAnim), Byrdpip and Pael's
Legion (their skins), and the call (172), script (163) and asset (123)
review lists, still to be gone through.

Still open: custom and daily runs (the singleplayer submenu, the modifier
screens, a seed keyboard); the game's .pck is no longer at
`F:\Program Files (x86)\Slay the Spire 2`, so the pack cannot be rebuilt
with the cursor images until it is back.

Next console build: logic (all stages: patches.json changed), engine; a
pack rebuild for the map drawing cursors when the game files are back.

## Completeness audit (2026-10-24): why gaps kept surfacing, and the loop that finds them

The user asked for the Rolling Boulder, Minion Dive Bomb, the
Decimillipede's last fade, Pandora's Box's card screen and Nightmare's
hands, and for a systematic audit instead of build-by-build discovery.
Why earlier passes missed things:

- Verification checked determinism (seeded traces) and crashes, never
  whether the client showed what the game shows.
- The engine's spawn functions answered "true" on failure: the Rolling
  Boulder and Nightmare's hands sat behind `if (!have) return true` (a
  call without a single creature) and were silently dropped; Bouncing
  Flask's NGaseousImpactVfx (a Vector2 overload) likewise. Failure paths
  now return false, so the cue ledger records a drop.
- The coverage harness itself skipped content: boss fights were lost even
  at 9999 HP (their deaths never ran), Regent cards were never playable
  (no stars), cards with their own conditions (Grand Finale, Clash) were
  skipped, encounters that draw monsters at random (Ruby Raiders, 3 of 5)
  ran one seed, events branched only on their first page, rest-site relics
  took a random option, potions were never used in a shop.
- The adapter auto-skipped events without options: the Fake Merchant (a
  shared event, act 2+) went straight to the map.
- Unported-script counts at instancing flagged effects whose spawners
  attach their behaviour; real gaps hid in the noise.

The loop now (each step writes what the next reads):

1. `tools/content_coverage.py --work work/covNN` (ledgers on): cards with
   energy, stars and (BlockedByCardLogic) an empty hand and draw pile;
   encounters random for 150 steps, then the enemies' HP capped
   (`set_player enemy_hp`) until they die, re-run with new seeds until
   every possible monster appeared (`content.encounter_monsters`); events
   explored breadth first (same seed, every unlocked option, 4 deep, 16
   paths, 500 gold); potions also used in a shop; relics' own rest
   options chosen.
2. `out/engine/spire_gaps --fast --dt 0.1 pack OUT.json traces...`: the
   CueLedger (seen / used / dropped per kind), scripts that reached their
   first frame without a behaviour, scenes loaded vs the pack's.
3. `tools/presentation_audit.py --runs work/covNN --gaps OUT.json`:
   categories sound, scene, call, answer (now one item per guard site,
   `Value@Site`), event, testmode, godot, delegate, headless, exception,
   skip (new: what the adapter passed over, Ledger.Skip), cue, script,
   asset, content; verdicts in `notes/presentation_verdicts.tsv`.
4. A "used" cue is not proof: check new effects in a capture
   (`presentation_scenarios.py`, `spire_play --replay-run ... --frames-after N:k`).
   `SPIRE_CUE_TRACE=1` prints each cue played / dropped / taken back.

Ported in this pass (engine 71+, logic 71+):

- Rolling Boulder (path, bounce, shadow, hits timed to its pass), Minion
  Dive Bomb, Nightmare's hands and vignette, the Decimillipede's group
  fade, Pandora's Box's simple cards view: captured and checked.
- Intents: NCreature.RefreshIntents / UpdateIntent carry the intents the
  game's call showed; the row changes when the game's does (the turn's new
  moves at its start, a power changing the numbers at once), not when the
  steps end.
- Potion liquid overlay (EFFECT_LIQUID_OVERLAY, effects 61: sw renderer
  and HLSL; the Xbox fxc entry is in effects_build.cmd): thrown potions
  and Bouncing Flask tint the body for 1 s; NSplashVfx at a position.
- Gold changes in combat (a thief) update the top bar when they happen.
- ToggleIsInteractable / SetCreatureIsInteractable: the state display
  hides (fleeing Fat Gremlin, Thieving Hopper; segments waiting to
  reattach).
- NDebugAudioManager.Stop by handle (Dense Vegetation's sleep.tres).
- little_light_script.gd: background lights start 0..5 s pre-processed.
- The Fake Merchant: a shop decision of its six relics, its own room
  (fake_merchant.tscn, the player in relaxed_loop, its laugh and welcome),
  buying through its inventory, Foul Potion thrown at it starts its fight.
- The shop pass: the merchant's dialogue on opening, thanks on a purchase,
  disappointment and the slot wiggle on a failure (gold, a full belt);
  cards fly to the deck, relics and potions fly from their slot to the
  top bar; Foul Potion at the merchant (slime impact, line, rumble).
- Sovereign Blade's forge preview, the Bowlbug Egg's cocoon, the Queen's
  freed-Repy background, the Ceremonial Beast's background phases and
  music parameters, the Decimillipede segments' phobia textures, the
  second boss icon, the potion flash.

Also fixed in the logic: a TargetedNoCreature potion (Foul Potion outside
combat) was given the player as its target, which UsePotionAction cancels
(the potion went for nothing); it has none, as PotionModel.EnqueueManualUse
passes. The Fake Merchant's whole flow plays (scenario `fake`: its room,
a purchase, the potion thrown, its fight, the rug and relic rewards).

Checks: seeded traces differences 0 (traces/new68, new70, new71, new72);
spire_check on new72 and scen72: 1,927 responses, 0 failures, 0 decoder
failures; the boulder, dive, nightmare, segments, pandora, flask, intents
and Fake Merchant captured (work/cap69, cap71, cap72); strict C++98 on the
changed files. Still open (presentation_verdicts.tsv): the menu screens'
sounds and animations (timeline, run history, settings tickboxes and
slider, character unlock, custom run), map drawing, hover tips of a
focused creature, Lord's Parasol's purchases one by one, the card
overlays and blur materials (checks), the sleeping effects' position
(check), the Queen's own death (cov72 kills bosses now).

Next console build: logic (all stages: patches.json changed), engine
(effects.hlsl gained ps_e61: effects_build.cmd runs fxc for it); the pack
is unchanged.

## Alpha RC Xenia test 3 (2026-10-22): the systematic presentation port

The user's third Xenia test found game-behaviour gaps (a card exhausted
from the draw pile shows nothing, Havoc and Mayhem play from nowhere) and
asked for every such case to be ported before the next build. The audit
is `notes/UI_AUDIT.md` (inventory: `Spire.ApiDump --ui-surface`,
`tools/ui_audit.py`); this is what changed, by mechanism:

- Card moves the client could not see (no node on the table): the
  card.pile_tween hook (GetTweenForCardsChangingPiles) now also records
  `card_create` (a card made at its old pile, with its view, to the play
  pile; exhausted ones burn there after max(lerp, 0.25) + 0.5 s, Fast
  0.2) and `card_fly` (NCardFlyShuffleVfx silhouettes between piles); a
  card coming into the hand from the discard or exhaust pile starts at
  that pile. Pile counts rise as each card lands, with NCombatCardPile's
  bump; the shuffle's discard count falls as each silhouette leaves.
- `Creature.GetVfxContainer` / `GetBackVfxContainer` were null in TestMode
  and the game writes `container?.AddChildSafely(X.Create(...))`, so the
  effect was never even made: block sparks and "Blocked", heal numbers,
  every attack's custom hit effect (Shiv, Claw, Perfected Strike, Sovereign
  Blade, the Wrigglers' impacts, ...), Fan of Knives, the Ceremonial
  Beast's lines, Test Subject's burn. They now answer the room's stand-in
  containers. A UI method bound as a delegate (`WithHitVfxNode(NBigSlashVfx
  .Create)`) ran its stripped body once reached: AttackCommand's
  WithHitVfxNode / WithAttackerFx calls are replaced and the delegate
  wrapped (attack.hit_vfx_node: 31 sites).
- The adapter queued card plays itself and skipped
  CardModel.EnqueueManualPlay's OnEnqueuePlayVfx (Inflame's ground fire and
  power-up: the "quiet" Inflame was missing sounds, Fan of Knives,
  Hellraiser, Pyre). Rest site options other than Heal and Smith now run
  their DoLocalPostSelectVfx (Dig, Lift, Clone, Hatch, Kindle; Kindle's
  shake and Pumpkin Candle flash ported in the rest screen).
- TestMode-off branches: presentation-only methods added to
  testmode.presentation.is_off (Tunneler's attack from below, Vantom's
  charge-ups with their track-1 animations and music, Waterfall Giant's
  build-up, Bygone Effigy's and Infested's second-phase music, Bouncing
  Flask, Hand of Greed); for branches that reach into NGame, hooks that
  read TestMode as on and record the cues (Nightmare, Doll Room, Sandpit).
- Godot nodes the game drives itself, ported as cues
  (PresentationHooks.cs): the monsters' aim nodes before an attack
  (SpecialNode.AimAt / ReachTo / SetPosition), Torch Head's lights,
  the Fabricator's bots' fall position, Vantom.ScaleTo (ShrinkPower;
  driven bones now take their node's scale), Surrounded's facing, Osty's
  fade-in, CreatureCmd.Escape (no death animation), Sandpit's pull,
  Doom / Reattach's AnimDisableUi and AnimHideIntent.
- Spine: NCreature.SpineAnimation / SpineBody stand-ins bound to the
  creature; the client plays MegaAnimationState calls on them.
  SpineAnimationAccess is a struct in the UI's namespace: the redirect
  leaves its methods alone and they call MegaSprite / MegaAnimationState
  inside that namespace, so on the stand-in sprite they ran the real
  bodies and threw ("GetAnimationState() was called before the
  SpineSprite's skeleton finished initializing"), killing the fight's
  turn loop (Vantom, Lagavulin Matriarch, Queen, Soul Nexus, Waterfall
  Giant). Its calls are replaced (spine.access.*) and go through the 'ui'
  hook; a scripted Vantom fight runs clean with its track-1 charge-ups
  recorded. Its receiver is unwrapped with a DynamicDependency on _sprite
  for the AOT builds.
- Death animations: anim_lengths.json (build_logic.py's copy stage, from
  the .pck's skeletons) gives StartDeathAnim's length, so InfestedPower
  reveals the Wrigglers after the elite's death animation (the bug the
  user saw) and StockPower likewise; presentation tasks the game runs
  alongside the turn are Steps branches (their waits delay only their own
  cues).
- Hand cards: NHandCardHolder.Flash on cost / keyword / replay /
  affliction / upgrade changes (card_flash), the randomize-cost roll
  (Confused, Slither, Snecko Oil), relics flashing on a previewed card
  (CardCmd.FlashRelics), the picked reward card flying into the deck
  (NGlobalUi.ReparentCard), Neow's HP counting up (NTopBarHp.LerpAtNeow),
  the block icon animating in from 0.
- Map: NBossMapPoint (the boss's spine node or placeholder picture and
  outline, from the logic's BossNodePath); the normal points' shader is
  now the game's (EFFECT_MAP_POINT: tones within 0.5 of the icon's tan
  take the map's colour) instead of a colour matrix that kept reds red.

The user's other reports:

- Profile after the menu: the title now waits (on the logo's black) for
  the logic's first main_menu answer (MenuFlow::menu_ready_).
- Energy red at 2/3: the label is red only at 0 (refresh_energy).
- Killing blow rushed: the rewards wait for the death animation + 1 s as
  ShowRewards does (rewards_ready); the body no longer vanished early
  (the HealthBar fix of the previous batch).
- Hand fill delayed then there at once: the runtime log showed a 1.9 s
  frame at the first combat decision (building the room); not changed yet.
- Music gaps: a music bank parsed under the audio lock stopped the mixer
  (act banks are 80-190 KB of JSON). The act's banks are parsed before
  the lock (AudioEvents::preload / preload_for) and stay for the act.
- Card streaks slow: the flights match NCardFlyVfx; the counts now rise
  as the cards land, as in the game.
- Vantom freeze: the watchdog (main.cpp) logs the phase, step, node and
  audio thread state every 10 s after a 3 s stall; read
  `xdk/spire360/Release/spire360.txt` after the next test.

- The wrapped attack effects catch any exception (presentation only):
  the Mecha Knight's took VfxCmd.GetSideCenter(...).Value, null in
  TestMode, once the effects ran; GetSideCenter there now answers the
  side's first creature's centre (vfx.side_center) and the big slash
  plays at it.
- A creature hidden by SetNodeVisible(false) stays hidden through the
  response's sync (CombatScreen::node_hidden_): the capture showed the
  Wrigglers during the elite's death animation; now they come 2.5 s
  later, with the second-phase music.

Checks: seeded traces differences 0 after each logic change (new48 ...
new63); spire_check on new54 / new58: 1,421 responses, 0 failures; the
tank trace (all three acts and bosses, with audio): 1,195 responses, 0
failures, 0 decoder failures (the Vantom freeze does not reproduce on
PC); content_coverage on the final logic: encounters 85/85, cards 60/60,
potions 66/66; scripted fights (`tools/presentation_scenarios.py`:
Havoc, Headbutt, Inflame; the Phrog Parasite's Wrigglers; Vantom; the
Mecha Knight) captured with spire_play --replay-run and checked frame by
frame; strict C++98 check passes. build_logic.py runs 2
jobs at a time (--jobs).

Still open (UI_AUDIT.md): Reattach's last fade over all segments,
Rolling Boulder, Minion Dive Bomb, Pandora's Box's view, Nightmare's
hands, the first combat's 1.9 s frame in Xenia.

## Settings review (2026-10-22): Fast mode, the game's own pacing, defaults

The user tests with a new profile (PrefsSave defaults: Fast mode off, i.e.
FastModeType.Normal) and asked for both speeds and every setting to work
as in the game. The game's FastModeType: Normal (default), Fast (the
settings' tickbox), Instant (dev console only; NGame turns it into Fast at
start-up, as SelectProfile now does). Fast mode only changes presentation
times: random_runs.py --fast traces (set_setting before the run) compare
equal to Normal ones once its settings lines are dropped.

- The logic now records the pacing the game takes from its UI layer,
  which TestMode leaves out (each a wait cue; trace_compare drops steps):
  - CardPileCmd.Add awaits the cards' tween (GetTweenForCardsChangingPiles
    returns none in TestMode): patch card.pile_tween records its time (a
    draw 0.25 s, Fast 0.1; into the play pile 0.25 s; exhausted from a pile
    lerp + 0.5 s, Fast 0.2; to discard / draw: callbacks, 0).
  - Shuffle's per-card waits (min(0.045, 0.8 / n), card.shuffle +
    card.shuffle_tween) before its CustomScaledWait(0.2, 0.5).
  - OnPlayWrapper's minimum play time: CustomScaledWait(0.15 - elapsed,
    0.3 - elapsed) measured with Time.GetTicksMsec; headless elapsed was 0,
    so every play waited the whole 0.3 s on top of its effects (a Defend
    took 0.55 s instead of 0.3). time.play_clock gives it Steps.ClockMs
    (the waits recorded so far).
  - NCreature.PerformIntent's CustomScaledWait(0.25, 0.4) before each
    enemy acts (the 'ui' prefix hook).
  - Replays (CurrentPlayIndex > 0, card.play_index): a multi_play cue and
    AnimMultiCardPlay's CustomScaledWait(0.4, 0.5).
  - Powers: AddDuringManualCardPlay's 0.25 s to the play pile
    (card.manual_play_tween), then PlayPowerCardFlyVfx (card.power_fly
    redirect: headless FindOnTable found no card, so the original waited
    0.8 s for a new one): a power_fly cue and its wait (the swoosh path's
    duration, which the client measures: "scale" 1 / Fast 0.2); a power's
    replay shows a copy (power_card cue) after CustomScaledWait(0.1, 0.8).
  - RemoveFromCombat on a card in the hand or the play pile: card_burn cue.
  Per play, Normal / Fast: Strike 350 / 175 ms, Defend 300 / 150, Bash 600
  / 275, Inflame 1050 / 460; an end turn to the next hand about 5.7 / 3 s
  (banners, intents, the enemy's moves, the shuffle, five draws).
- The play pile (NCombatUi.PlayContainer): a played card now goes to the
  screen's middle 100 px up in the lerp time (0.25 s, Fast 0.1), upright,
  to 0.8 in 0.25 s, and stays there while its effects play; then
  NCardFlyVfx to its pile, NCardExhaustVfx where it is, or the power's
  swoop (power_fly). It used to fly at its target and fade in 0.4 s.
  AnimMultiCardPlay on a replay. Cards still there when the enemies' turn
  starts burn. NCardFlyShuffleVfx: each shuffled card's silhouette flies
  from the discard pile to the draw pile with the character's trail. Not
  shown yet: a card exhausted from the draw / discard pile (it flies to
  the play pile first) and one autoplayed from the draw pile (Havoc,
  Mayhem): the client has no view of draw pile cards.
- A hand card's tips stayed on screen after the card left the hand (the
  end of turn's discard): tips_for_ is a place in the hand, now rebuilt
  whenever a card leaves it.
- Deaths (the user's floor 9 report, test 2): monster_death looked for a
  "StateDisplay" node to fade (AnimDisableUi); the creature scene's state
  display is %HealthBar, so the fallback faded the whole creature 0.5 s
  after its death: the body vanished during its death animation and came
  back for NMonsterDeathVfx's dissolve. Now only %HealthBar fades; a revive
  or a spawn brings it back (AnimEnableUi, CombatScreen::enable_ui). The
  creature node goes at AnimDie's end (QueueFreeSafely): when the effect
  takes the body, at the delayed hide for one that does not fade, or half
  a second after the effect's moment if it found nothing to take
  (fade_gone_at_). spire_check's "dead enemy remained visible" now allows
  that time (25 s) instead of the response's end.
- Rewards (NCombatUi.ShowRewards): after the dying monsters' remaining
  animation + 1 s (Fast: 0.5 s; a boss, half its animation)
  (CombatScreen::rewards_ready). A monster's death no longer holds the pad
  1.5 s (the game goes on; the player's death still does).
- Fast mode in the UI (fast_mode() from the logic's settings answer): the
  room transition (no gradient; black after 0.3 s in 0.3 s; back in 0.3 s),
  the map's travel (0.3 s) and, once the map's FTUE was seen, no map rise
  at an act's start (only NActBanner), the event layout's and options'
  AnimateIn, the epoch unlock (now timed as the game's: the chains give at
  2 s, Fast 1 s; the button only then).
- Other settings checked against the game: screen shake's multipliers (the
  paginator now shakes Medium on a change, the main menu is the shake's
  target there), card indices number the active holders only, run timer,
  long press (the end turn button only), text effects, phobia mode, skip
  intro, volumes (squared, as NAudioManager and NDebugAudioManager). The
  settings' hover tips (FASTMODE_DESCRIPTION, ...) show on mouse hover
  only in the game, so not on the pad.
- Tools: random_runs.py --fast; spire_play --fast (live logic only: in
  --replay-run the recorded answers stand in for the logic, and a replayed
  --fast trace's settings answer turns the client's Fast mode on).
- Checks: seeded traces new41, differences 0; fast41 (Ironclad, Silent)
  equal to new41 without its settings lines; whole runs wait about half as
  long in Fast mode (50-52%). spire_check on new41: 1,421 responses, 0
  failures, 0 decoder failures (the same 4 absent events as before).
  Frames checked at both speeds (work/pace2, pace4): Inflame's play and
  swoop, a shuffle, a replay (Setup Strike), a kill to its rewards (3.2 s
  Normal, 1.1 s Fast). Strict C++98 on the changed files. Next console
  build: logic (all stages: patches.json changed), engine; the pack is
  unchanged.

## Alpha RC Xenia test 2 (2026-10-22): the user's findings and the fixes

The second Xenia build (`logs.zip`: `spire360.txt`, `xenia.log`); the
user's report, item by item:

- Combat start: the logic took the fight's first snapshot only after the
  whole start ran (one continuation), so the start's cues (the 1 s
  CustomScaledWait, the turn's 0.8 s, energy, the draws, start-of-combat
  relics such as Gorget's plating) went to the response after the first
  action (the potion in the report). Steps.FlushChanges now takes the
  fight's baseline at its first cue (CombatManager's music update, before
  StartTurn). The first combat decision carries those steps; the client
  builds the room from CombatScreen::before_steps (the hand without the
  drawn cards, the counters, HP and block at their first values) and plays
  them; the hand deals in from the draw pile.
- Mashing A / navigating while effects play (NCardPlayQueue): the hand
  takes input while the steps run once the player's turn has started in
  them (NPlayerHand.AreCardActionsAllowed: no pending round / player turn
  banner cue; GameClient::play_queue_open). A card played meanwhile waits
  in NCardPlayQueue's place (left of the play pile, 0.8 / (i+2)) and
  flies when its play's cues come; a response that arrives while the last
  one plays is appended to the same timeline (play_steps keeps the
  remaining cues; each SYNC carries its own decision). The hand's cards
  are matched to the decision's by Steps id while the two differ. The
  start banner no longer holds the pad for 2.6 s. `spire_play --mash`
  tests it (presses every --mash-every frames, whatever plays).
- Music before the logo: main.cpp no longer starts it (MenuFlow::show_title
  does).
- Compendium "does nothing": the buttons never showed focus. Ported
  NShortSubmenuButton / NCompendiumBottomButton.OnFocus (scale 1.02 / 1.05,
  BgPanel's hsv v + 0.2 in 0.05 s, back in 0.3 s). Also fixed a crash on
  leaving it with a sub-screen open (the screens freed after their nodes).
- "Press I to inspect": the card reward screen took %InspectPrompt out of
  its scene before setting the glyph, so %HotkeyIcon was not found and the
  scene's keyboard key ("|") showed. Set first; the moved nodes are
  disowned from the freed scene.
- Status cards an enemy gives (Slimed): CardCmd.PreviewCardPileAdd in a
  fight was not recorded (combat piles were skipped). Now a deck_card cue
  with "pile"; the combat timeline plays it (DeckPreviews::play_cue): the
  card in the middle row for its time, then NCardFlyVfx to that pile.
- Power gains showed nothing (Tremble's Vulnerable): NCreature.
  OnPowerIncreased's NPowerAppliedVfx (big icon, the title rising green or
  falling red) and the buff / debuff burst are ported; the logic records
  power_vfx at Creature.InvokePowerModified (patch power.modified; not
  silent, ShouldPlayVfx).
- Shrink Beetle: the base scale was derived after ScaleTo had set the new
  factor (shrinking did nothing, the restore doubled the player). Fixed
  order (CombatScreen::scale_base).
- Shop: the card removal's slot drew over the removal grid (z 1 behind an
  overlay; ShopScreen::set_behind). NMerchantHand ported: the hand follows
  the focused slot with its Perlin wobble, the wrist bone turning
  (SpineSprite::set_bone_rotation), back to its start 2 s after.
- Enchant selection (Sown): NDeckEnchantSelectScreen's description panel
  (icon, title, text) was hidden; the logic now sends it
  ("enchantment_screen", dropped by trace_compare). Event options now
  carry their HoverTips ("tips") and show them left of the focused option
  (right in a combat layout), as NEventOptionButton.OnFocus.
- Potions: the focused slot shows its tips under it (POTION_SLOT's when
  empty) with a slosh; the logic sends each potion's tips. The top bar in
  a fight takes the new potions at once (RemoveBeforeUse), HP and gold
  still from before the steps.
- Floor 9's death cut short: the rewards' aftermath hid every enemy,
  including ones still in AnimDie. Dying ones are left to their own fade.
- Treasure: the chest replayed its opening on the next stage (each stage
  rebuilds the room); an open chest starts at its animations' ends.
- Rest site: option.DoLocalPostSelectVfx is UI-side, so the logic records
  a rest_vfx cue at RestSiteSynchronizer.AfterPlayerOptionChosen (the
  option, the smith's cards, the CustomScaledWait). Heal: the sleep jingle
  (sleep.tres picks one of three) and blanket sounds, NRestSmokeVfx, the
  room darkening to 0.1 and back (NDesaturateTransitionVfx's brightness;
  its saturation and contrast are not ported); the proceed arrow after the
  wait. Smith: NCardSmithVfx (three hammer blows, card_smith.mp3, sparks,
  shakes), then the card flies to the deck. The smith path is untested
  (no seeded run smiths).
- Not changed: every playable card glows cyan (NHandCardHolder.UpdateCard,
  NCardHighlight.playableColor); gold only for the 15 cards with a
  ShouldGlowGoldInternal (Defend has none). Pets draw in front of the
  player, as PositionPlayersAndPets / AddCreature order them.
- Performance (Xenia: ~7 ms process + 13 ms draw in fights, over the 16.7
  ms frame): D3D9Renderer caches the device state apply_state sets
  (textures, samplers, blend and scissor states, shader, constants by
  register runs), so a batch sends only what changed (forget_state after
  any direct device call). The tree calls process() only on classes with
  one (NF_PROCESS) and sweeps freed nodes only when some were queued.
  Control::theme_constant memoizes per control until theme_generation
  moves (tree shape, themes, overrides). The console log now prints the
  update time, the worst frame and how many frames went over 16.7 ms.
- Checks: seeded traces new39, differences 0; spire_check on new38: 1,421
  responses, 0 failures, 0 decoder failures; strict C++98 on the changed
  files; d3d9check rc 0. Next console build: logic (all stages, logic_lib
  recompiles), engine; the pack is unchanged.

## Alpha RC Xenia test 1 (2026-10-22): the user's findings and the fixes

The first RC build (logic 10-22, pack `work/pack-lang`) ran in Xenia; the
user's report and `spire360.txt` / `xenia.log`, item by item:

- Menu music looping a few seconds, then gone once the profile loaded:
  event:/music/menu_update is a 4.5 s timeline whose unlocked (async) box
  plays one of the four menu themes while the playhead loops 4.0 -> 2.0.
  A zero-length jump deleted every box child and re-armed every box, so
  the theme restarted (a new SmartRandom pick) every 2 s. Now an async
  box's instrument whose region holds the destination keeps playing (FMOD's
  rule); `spire_audio_check --fixtures` has the case (asyncjump: 1 voice;
  10 before). The saved volumes now apply before the first sound
  (MenuFlow reads settings.save: GameClient::apply_volumes, squared as
  NAudioManager); before, the buses played at full volume until the
  logic's settings answer (~8 s in Xenia), then dropped to the default
  0.5^2 x 0.5^2 (the game's own default level).
- Settings sliders at 50% drawn at 0%: the handle was placed from the
  bar's size before layout (0). NSlider._Process: each frame, SmoothDamp
  (0.05 s, the helper now in ui_util) to the value, at Size.X * value /
  max.
- The compendium "missing everything": its buttons' icons were set to
  images/packed/main_menu/submenu_icon_*.png, which the game does not
  have (NShortSubmenuButton.GetImagePath only uses an existing one); the
  scene's icons now stay.
- The focused hand card over the pause menu: the run's overlays (pause,
  its popup, settings, compendium, the pile and map views) go in a
  capstone layer (z 100, GameClient::add_capstone), as NCapstoneContainer.
- End of turn jumbled (the hand not discarded first, banners out of
  order): the logic took its state snapshot only after each continuation,
  and headless the whole end of turn (discard, enemy turn, next turn,
  draw) is one continuation, so every card move came after all the cues,
  draws before discards. Steps.FlushChanges now records the changes since
  the last snapshot before each UI cue, cue and wait (a cheap fingerprint
  of round, energy, piles, HP, block, powers decides whether to take
  one). Cmd.CustomScaledWait (0.8 s after each turn banner, the end of
  every CreatureCmd.TriggerAnim, 79 call sites) skipped itself headless
  (NonInteractiveMode under TestMode); the new cmd.custom_scaled_wait
  patch records it as the game's FastMode would wait, and anim.trigger
  is now a call_before cue with no wait of its own (the scaled wait
  follows it). The client puts the turn banners where the logic's
  NEnemyTurnBanner / NPlayerTurnBanner cues are (older traces: the
  synthesized ones). Seeded traces: differences 0 (traces/new35);
  spire_check on new31 and new35: 0 failures. Checked on a replay
  (`work/endturn.sh`): the leftover hand flies to the discard pile under
  "Enemy Turn", the enemy acts, "Player Turn", the new hand is drawn.
- The Defend card "highlighted for the enemy's intent" and in front of its
  neighbour: as the game, every playable card glows (NHandCardHolder.
  UpdateCard: cyan, gold or red) and the pad's focused card is raised in
  front (NPlayerHand.AnimEnable focuses the last index at the turn's
  start). Not changed.
- Slowdown at attacks: per frame on PC (spire_play now prints it, as the
  console's log) audio was 1.4 ms of ~2 ms, worst 14 ms; 83% of it the
  event players re-reading the banks' JSON every 64 frames. The players
  now cache what they read each tick (kind, box / transition times and
  keys, the bus chain, Random modulator draws, conditions without a map):
  the output is byte-identical (`work/wavrun.sh`, a recorded replay) and
  audio is 0.70 ms. The console then mixes on its own thread (hardware
  thread 4: GameAudio::start_thread; engine/core/thread.{h,cpp}; locks in
  GameAudio, AudioEvents and around Pack's shared file; 8 buffers queued),
  so the frame loop no longer pays for it; `spire_play --audio-thread`
  runs it muted on PC. Master, sfx and ambience banks are parsed at start
  (sfx.bank alone is 63 ms on PC, a hitch at the first attack before).
- Startup wait for the profile: the menus already ask for it at once
  (MenuFlow's constructor); the time is the logic's own start-up under
  emulation (first answer 7.9 s, main menu 17.9 s). Not changed yet.
- Volume "reset at the start of the fight": no code path found that sets
  the buses again in a run; combat music has more layers (Progress).
  Waiting for the next test.
- Next console build: the logic changed (build_logic all stages,
  logic_lib recompiles) and the engine; the pack is unchanged.

## Alpha push, part 10 (2026-10-21): GPU particles, the forms, FTUEs, 3D

- GPUParticles2D now runs Godot 4.5.1's ParticleProcessMaterial shader on
  the CPU (`engine/scene/particles.cpp`: start() / process() inside
  particles.glsl's frame loop: restart phases, the particle's NUMBER and
  seed, the parameters drawn from it). A survey of the game's 1403 process
  materials (`work/res_text`, every .tscn / .tres) found these unported:
  turbulence (~145 files), emission shape scale / offset (~100 / 19),
  radial velocity and velocity pivot (~99), damping as friction (78), ring
  emission (65; was drawn as a disc), scale over velocity (67), emission
  curve (47), directional velocity (40), inherit velocity (18), amount
  ratio. All are in now, with the shader's sphere / box / ring shapes, its
  hue rotation (also without a curve) and velocity limit. 1397 of the 1403
  set disable_z, the path ported (the other six run it too).
- The shader text came from the Godot 4.5.1 binary's strings (the
  generator's literals, MIT); `ParticleProcessMaterial::
  set_turbulence_noise_scale`'s slider mapping is applied when loading.
- Particles without local_coords (CPU and GPU) live in the canvas, as in
  Godot: emitted through the global transform and drawn with an identity
  item transform, so they stay behind a moving emitter (the card flight
  trail now leaves its sparks behind) and ignore the emitter's scale.
- GPUParticles2D's seed is new at creation and each restart unless
  use_fixed_seed (a fixed sequence, so runs repeat).
- Cost: the turbulence noise is ~300 4D gradients per particle and step
  (3 us on the PC after a fast floor; the old version 6.8 us). Watch it on
  the console with the turbulent event portraits; halving its rate is the
  fallback.
- Sub-emitters (11 scenes: the heal crosses, dagger spray / throw, Molten
  Fist, Osty's heal, Spiny Toad, ...): the parent's process() sends
  emissions (constant every 1/frequency s, at the end, at a collision, at
  the start) to the system sub_emitter names; that one then only takes
  sent emissions into free slots (transform kept, velocity if
  keep_velocity); unclaimed ones drop each frame. Checked with spire_view
  (`scenes/vfx/vfx_cross_heal.tscn --time T --origin X Y`, cropped): the
  crosses leave their falloff specks four times a second.
- Trails (GPUParticles2D trail_enabled, 11 scenes: card smith, dagger
  spray, Living Fog, Aeonglass, Kin Priest's grenade, ...): the trail mesh
  as GPUParticles2D builds it (rows of +-width / 2 skinned between
  sections + 1 bones, v down the trail), each bone the particle's
  transform i / sections of trail_lifetime ago from a short per-particle
  history. Checked on the dagger spray (tapered red streaks behind the
  daggers).
- Emission points (EMISSION_SHAPE_POINTS / DIRECTED_POINTS: the Regent's
  death explosion, Sovereign Blade's yellow dots, Ceremonial Beast, Test
  Subject): the ImageTexture's inline Image (RGFloat and the other float
  formats, RGB8 / RGBA8 colours; the converter keeps its bytes as a number
  list, which packed_bytes reads now) as texels; emission_texture_position
  drawn after the initial colour ramp picks the point (its position, its
  colour texel; directed points turn the velocity by the normal texel,
  disable_z's mat2). Seen on the blade in a Regent replay
  (`work/bladeplay.sh`, new33 regent-2 screen 66).
- Fixed-fps interpolation (GPUParticles2D.interpolate, on by default):
  each particle is drawn VELOCITY * the fixed-fps remainder ahead, as
  particles_copy does (GLES3 and RD alike), so the 30 fps steps move every
  60 Hz frame (before: every other frame identical; `work/interpview.sh`
  on the heal crosses). Attractors: no scene of the game has one.
- Soak after part 10 (`work/soak.sh`, 9999 HP autoplay, every character):
  all five rc 0, no logic errors, no plain-list screen, no decoder
  failures; Ironclad, Defect and Regent won, Silent ran 2439 screens, the
  Necrobinder fell to the Knight Gang on floor 43 (the random play's long
  fight). Absent audio calls are the known content mismatches.
- Gaps sweep after part 10 (`work/gaps10.sh`: traces/new21 and all 85
  work/coverage-anim encounters, 8959 decisions, 4 h 42 min): left are the
  absent audio events and Aeonglass's stale RaySlot. Three more content
  mismatches showed up, absent from the installed bank like Vine Shambler
  and Slithering Strangler: Crusher's `kaiser_crab_attack_slam` (the bank
  has the left / right slams), Rocket's and Fake Merchant's default
  `<id>_attack` (MonsterModel.AttackSfx). Keep them visible. The 3D nodes
  were still noted as unsupported (scene.cpp's is_native list) and the
  Spine dark colours (setup and two-colour timelines) were noted as gaps,
  but libspine_godot has no shader or dark tint at all (its dark colours
  are only script properties), so drawing the light colour is the game's
  look: both notes are gone (`work/gaps11.sh` on the two traces: 0 gaps).
- Forms: Echo Form's copy had no skeleton (the body was looked up as the
  first SpineSprite under the visuals, which is the form's own copier);
  it is NCreatureVisuals' %Visuals now. All five forms render: Demon's
  aura, Void's swords (larger when active), Serpent's snakes (flare on each
  trigger, fade), Reaper's magenta noise ring, Echo's line fragments
  drifting off the back (its mask and scale centre keep them to the
  upper-left of the copy). Echo and Reaper have no game reference yet.
- Fight scripts: scratchpad `form_fight.py OUT CHARACTER CARD[,CARD...]
  [TURNS] [play]` (play: up to two attacks each turn); Void Form is the
  Regent's, Serpent the Silent's, Reaper the Necrobinder's.
- Fix: special_node_call took any first argument with a '#' as a
  "who#path" stand-in, so ui calls whose first argument is a colour
  ("#ffffff80") were swallowed (NHorizontalLinesVfx, NSmokyVignetteVfx).
  A stand-in needs a creature before the '#'.
- Whirlwind / Ceremonial Beast: NHorizontalLinesVfx (motion lines over
  the VFX container, emission box from the container's height, fading in
  and out over max(duration, 1) s; leftwards turned about the far
  corner). Whirlwind / Haze / Nightmare: NSmokyVignetteVfx in fights
  (`spawn_smoky_vignette`, shared with the full-screen heal); Whirlwind
  adds it to NRun.GlobalUi, Haze to the VFX container, the cue does not
  say which, so both go to the VFX container.
- NGainEpochVfx ("Epoch Discovered", the epoch's portrait sliding in at
  the left, 3 s per toast already up): from a response's steps (the
  logic gives the class name, "Ironclad2Epoch"; ModelDb's id rule makes
  IRONCLAD2_EPOCH) over every screen (the global layer), and from the
  game over screen's score-bar unlock. Seen on screen: the first toast
  in `work/coverage-instr/acts/run_00_ironclad.jsonl` at screen 321
  (`work/epoch.sh`, ~13 min; the portrait in its chains at the left).
- Osty: NCreature.TrackBlockStatus (Osty's health bar takes the block
  outline and colour while its summoner has block, no number of its own),
  NCombatRoom.ShakeOstyIfDead (NCreature.AnimShake on each Osty card:
  10 * sin(4u) * sin(u / 2) px, 1 s, not during "hurt").
- NCreature.DisableInteractionForDeath needs nothing: the client's
  targets come from the decision's living enemies.
- FTUEs (`engine/game/ftue.cpp`, `logic/Spire.Logic/Ftues.cs`): the logic
  answers `ftues` (the profile's unseen ids; none without a profile, so
  test runs show none), `ftue_done {id}` and `accept_tutorials {yes}`.
  The client asks with each main_menu answer and marks a tip as it shows
  (as the game does). FtuePopup: NModalContainer's backstop, the scene's
  texts, "Got it!" pulsing, A closes; the control the tip is about lifted
  over the backstop (ZIndex + 1); while one is open the hand has no
  focused card and the fight's timeline is held (shuffle). Triggers:
  accept_tutorials at the first Embark (ConfirmPopup on its scene, before
  the run), combat_rules at the first fight (three pages: A / right next,
  left back; the start banner after it), map_select once the act's rise
  is over (a press only ends the rise), shuffle before the first discard
  -> draw move, can_play_cards on End Turn with a playable card (three
  ends with none turn it off), cannot_play_card on a card too costly
  (eight plays turn it off), combat_reward on Skip with rewards left,
  merchant on leaving with the rug unopened (opening it marks it),
  rest_site, power_card (the Power's holder lifted), obtain_potion (a
  potion more), obtain_relic (treasure open, a relic reward). All seen
  on screen (spire_play --menu on a fresh profile; spire_play writes
  ftue_<id>.png for each): `work/ftuerun*.sh` for the first seven,
  `work/ftuescout.sh` (seed FTUE1, 999 HP, autoplay: the potion and the
  relic) and `work/ftuecase.sh NAME SCREEN PRESSES STEPS` for what only
  the pad reaches (cannot_play_card: A presses in the first fight;
  combat_reward: Y on screen 21's rewards; merchant: Y in screen 74's
  shop). Fixed on the way: the merchant tip waited for "proceed" but the
  shop leaves with leave_room (it never showed); the relic tip lifts the
  relic's own reward button (or the treasure's SingleplayerRelicHolder,
  else the collection) and puts %Arrow at its bottom centre, as
  NRelicRewardFtue; closing cannot_play_card ends the turn
  (SecretEndTurnLogicViaFtue, no playable-cards tip) and closing merchant
  opens the rug (CloseFtueAndOpenRug). The ascension popup
  (NAscensionSingleplayerFtue: once any character has an ascension, at the
  character select, tutorials on or off; marked as it closes) is in the
  same list; the logic tests FtueCompleted itself (SeenPopup is true under
  TestMode). Not ported: the mouse-only
  SneakyHitboxes. The logic still calls NCombatRulesFtue.Create every
  fight (headless never marks it seen); the client ignores the cue.
- NPowerUpVfx's 3D back layer (Inflame, Corruption, Crimson Mantle,
  Unmovable; the ghostly copy): `engine/scene/node3d.{h,cpp}`. Node3D /
  Camera3D / MeshInstance3D (GPUParticles3D are plain Node3Ds: the two
  rings are hidden, the beam never emits), Transform3D (basis rows, then
  the origin) with top_level, the camera's perspective (fov of the kept
  height, near plane), ArrayMesh surfaces in format version 2, plain or
  compressed (unorm16 positions in the AABB, uvs over +-uv_scale, u16 /
  u32 indices). A SubViewport draws its 3D first (meshes far to near,
  triangles in index order, behind-the-camera ones dropped), then its
  canvas. The aura's VisualShader (inline:9d11d5fdbce06d0e) is effect 59
  (EFFECT_POWERUP_AURA): two colours mixed by a GradientTexture2D, alpha
  a second gradient x the baked noise x aura.png scrolled by TIME *
  FloatParameter * 1.8; every sampler repeats (spatial default).
  Checked against an independent numpy render of the same meshes
  (perspective-correct uvs, Godot's sampling) for both scenes at T 0.5:
  same placement, shape and colours (the engine is a little brighter:
  BackVfx is additive); in a replay (`work/puplay.sh`, Ironclad new33
  screen 31) the crown of flames sits at the creature's feet behind it.
  UVs are affine per triangle (no perspective correction; the meshes are
  small and flat enough).
- spire_play --menu: `--seed S` is the run's seed (GameClient::
  set_run_seed; the menus' embark passes it), `--hp N` applies once the
  run is on, and `--pad-after N --pad-run PRESSES` stops the autoplay at
  the run's screen N for those presses (a shot after each), then it plays
  on. Autoplay's 1.5 s close of a tip only while it is playing.
- The enchant emboss reuses EFFECT_TAB_APPEAR (16), which already ported
  enchantment_tab_appear; the duplicate effect 59 of 4f7134c is gone.
- `tools/check_canvas_shader.py` compiles every effect pixel shader too
  (59 ps_eN plus the two canvas stages, system D3DCompiler).

## Alpha push, part 9 (2026-10-21): flashes, room cues, deck previews

- Relic and power flashes: logic hooks `relic.flashed`
  (Player.OnRelicFlashed) and `power.flashed` (PowerModel.Flash) record
  `relic_flash` / `power_flash` cues. Combat shows NRelicFlashVfx over each
  target's hitbox top, NPowerFlashVfx's big icon, and the top bar holder's
  flash particles (`make_relic_holder_flash`, shared with the top bar).
- Mirror: screen mode SCREEN_AT_UV for screencap.tres; NMirrorVfx script.
- Outside fights (`engine/game/room_cues.cpp`): the hurt vignette, damage
  numbers (the local player's at a quarter of the width, half the height),
  PlayerFullscreenHealVfx (smoky vignette and crosses) and relic holder
  flashes from the response's steps. NDamageNumVfx is one self-running
  node (`make_damage_number`) for combat and rooms.
- Top bar: a relic new since the decision before rises into its holder and
  flashes (PlayNewlyAcquiredAnimation; every RelicCmd.Obtain plays it).
- Deck previews (`engine/game/deck_previews.cpp`, NRun.GlobalUi): the
  logic records `deck_card` cues (add, show, remove, upgrade, transform,
  enchant) with the card's view, from CardCmd.Preview /
  PreviewCardPileAdd, CardPileCmd.RemoveFromDeck, FinalizeUpgradeInternal
  inside CardCmd.Upgrade, AfterTransformedFrom/To inside CardCmd.Transform
  and NCardEnchantVfx.Create. The client's layer stays over the screens
  (previews outlive the page or room that started them) with the row,
  grid, event and messy containers laid out as their scripts do, the
  remove / upgrade / transform (NCardTransformShineVfx) / enchant
  timelines, then NCardFlyVfx into the top bar's deck with the card trail.
  Hooks never enumerate lazy arguments (they may draw from an RNG).
  Not drawn yet: the enchant emboss (EnchantmentVfxOverride's viewport
  icon and progress shader); the enchanted card shows its tab as it is.
- Patcher: `call_before` (Hooks.Before on entry), covered by the patch test
  (`sh logic/tests/run_patcher_test.sh <outdir>`).
- Fixes: a played card counts as failed only if the combat history has no
  play of it (Shining Strike drawn straight back into its slot was
  reported unplayable, 23 errors in the Regent soak); the client's
  last_error is the last command's (it stuck, so every later screen
  counted as an error) and error retries count per decision; shop
  purchase logs read the item before the entry clears (the "Buy potion
  failed" lines were successful purchases).
- spire_play `--frames-after N[:EVERY]`: every EVERY-th frame (default 10)
  while the steps after screen N play (its response, its settle).
  `--settle` is a frame count (integer).
- Audio: a voice that does not start logs why once per sample, and the
  first time all 64 voices are taken the mixer lists what holds them.
- Whole-run soak (`sh work/soak.sh [LANE1 LANE2]`, two lanes): all five
  characters ran clean of crashes and stalls; after the fixes Regent and
  Defect have no logic errors.
- Content mismatches (keep visible, as Vine Shambler / Slithering
  Strangler): Bowlbug Silk and Living Shield use MonsterModel's default
  AttackSfx path, which the installed bank does not have.
- Gaps sweep (work/gaps8, 3724 decisions): left are NPowerUpVfx's 3D back
  layer, the Spine dark colour timelines, Aeonglass's stale RaySlot and
  the absent audio events; the three shader entries were a stale binary
  (current build: 0 gaps on those traces).
- Punch-Off: PunchEachOther runs only with creature nodes, so the event
  screen plays it on its visual-only room (the left construct mirrored,
  an exchange each 2.6 s as synthesized steps) while the INITIAL page is
  up.
- Screen shake outside fights: each room screen names its target
  (`RunScreen::shake_target`: the event layout, the merchant, rest site
  and treasure rooms) and `make_shake_driver` runs the shake while it
  holds it (a fight's room keeps its own); room cues pass the
  NGame.Screen* calls on. The top bar inside a shaken room stays put.
- Speech bubbles (NSpeechBubbleVfx) in fights and event rooms: %TalkPos
  or above the centre to the speaker's front, the colour's hsv, the text
  flying in; a VfxDuration.Forever bubble goes when the next line comes.
- EventModel.OnRoomEnter runs headless now: the UI calls it
  (NEventLayout.SetEvent), so the logic calls it after NEventRoom.Create
  and NRun.SetCurrentRoom (Minigames.cs OnUiCall). The Architect: the hook
  `architect.line` gives it the layout combat's enemy (the game reads the
  room's nodes), so its first line, the head (anim cues on track 1), the
  score and the closing attacks run as in the game; its hits' numbers and
  sparks go to a null container headless, so `architect.parts` records
  DivideWildly's parts and they come with each hit's VFX cue.
  Reflections' mirror and Slippery Bridge's rain are recorded from their
  OnRoomEnter and stay for every page of the event.
- Event rooms with a combat layout play their responses' steps on the
  room; a response that leaves the event (The Architect's game over)
  waits for them. Damage numbers on such a room follow
  requireInteractable (the local player's at the viewport fallback).
- Audio: at 64 voices the mixer steals the oldest one-shot (FMOD steals
  rather than refusing). The Regent soak's 64 were a burst of one-shots
  (2 looping), not a leak.
- Test runs (`start_run`) report game over without victory after WinRun:
  only game-flow runs (`start_game_run`) record RunManager.OnEnded.
- Event portraits: the event's own VFX (scenes/vfx/events/<id>_vfx, 38
  events) on the portrait at EventModel.VfxOffset (InitializeVisuals);
  Trial's accused (`event_portrait` cue from `event.trial_portrait`) is
  kept for the event's later pages, like the mirror and the rain
  (GameClient::collect_room_cues).
- Thought bubbles (NThoughtBubbleVfx, "I don't have enough cards to
  draw.", the bosses' lines); hit-stop (NGame.DoHitStop: the scene tree's
  time scale at 0.1 easing back over 0.15-0.6 s of real time,
  `SceneTree::hit_stop`).
- The Regent's Sovereign Blade (`engine/game/sovereign_blade.cpp`,
  NSovereignBladeVfx): headless SovereignBlade.GetVfxNode finds no node,
  so ForgeCmd.PlayCombatRoomForgeVfx is redirected (the adapter keeps
  which blades exist: forge vs refine sound, flames on a new one, the
  shared orbit) and the card's GetVfxNode calls in OnPlay's BeforeDamage
  and RemoveSovereignBladeNode record blade attack / remove cues. The
  client's blade orbits, grows with its damage, glows and sparks, dives
  at the target with its trail and slash, and shrinks away.
- Patcher: an in_types "Type::Method" scope now also covers the method's
  lambdas (<m>b__N in <>c / <>c__DisplayClassN), as its notes said.
- NCreature.SetAnimationTrigger cues duplicate the anim cues of
  CreatureCmd.TriggerAnim (not a gap).
- Power forms (`engine/game/form_vfx.cpp`): StandIns answers the five
  N*FormVfx.Create calls with a special stand-in ("player#form/NDemonFormVfx")
  and NodeUtil.IsValid is true for special stand-ins (the power's Vfx
  getter checks it), so SetActive / OnEffectTriggered are recorded. The
  client puts the form in the visuals' %FormVfx, with
  NSpineSpriteBoneFollower (SpineSprite::bone_position), NValueRamp and
  the gradients; the Echo Form's copier plays the body's animations.
  All five checked in part 10.
- The enchant emboss: NCardEnchantVfx's %EnchantmentViewport (the
  enchantment's icon and amount) on the card's EnchantmentVfxOverride
  through enchantment_tab_appear (EFFECT_TAB_APPEAR, progress 0 -> 1 over
  1 s, Quad InOut), the card's %Enchantment tab hidden (as the game; the
  override sits 45 px below the tab of a card without a star cost, as in
  the game). ViewportTextureRes can name its viewport directly
  (Viewport.GetTexture from code).
- Still open: NPowerUpVfx's 3D layer, the five FTUEs not seen on screen
  yet (part 10). The other unhandled ui calls of the survey
  (work/ui_calls.txt) are done in part 10.

## Alpha push, part 8 (2026-10-21): effect shaders, card flights

- Effect shaders 42-58 (engine/gfx/effects.h; software + D3D9 each): the
  Doom flame, the poison skull, the demon / reaper / serpent / echo form
  effects, round smoke, scream streaks, card removal, divebomb rings,
  hyperbeam laser and spikes, the Grand Finale spotlights, Aeon Glass's
  ray, Soul Fysh's soundwave, Waterfall Giant's water, the Cubex laser's
  colorize, the epoch chains; the Necrobinder's and the blood wall's
  inline VisualShaders are the simple-noise / molten-fist ones. A
  VisualShader's own node textures (tex_frg_<n>) resolve now.
  `python tools/visual_shader_code.py <pck> <res>` prints the code Godot
  generates for a VisualShader (to port one). `python
  tools/shader_survey.py <pack>`: what is left in the game is the mirror
  (Reflections event, screencap.tres: needs a screen mode that reads the
  copy at UV), NPowerUpVfx's 3D back layer (Node3D in a SubViewport; its
  2D layers draw), the map drawing (mouse); the rest are debug scenes
  (the gaseous screen too: only NCombatVfxSpawner makes it).
- Sprite2D / AnimatedSprite2D: the VFX shaders' pivot_offset shift.
- Cards in flight: NCardFlyVfx to the discard / draw pile (arc, speeding
  up, darkening, shrinking), NCardFlyPowerVfx's swoosh into the player
  for a played Power, both with the character's card_trail scene and the
  card movement sounds. Path2D / PathFollow2D are nodes now.
- DoomVfx works without setup() (it crashed shown on its own). A render
  of every scenes/vfx scene with scripts (spire_view, 311 scenes) shows no
  crash; unsupported nodes left there: Node3D (power up), the gaseous
  screen's AspectRatioContainer (debug only).

## Alpha push, part 7 (2026-10-20): hand selection, soak

- Selection from the hand (CardSelectCmd.FromHand / FromHandForDiscard /
  FromHandForUpgrade: Survivor, Acrobatics, Armaments, ...) runs on the
  fight as NPlayerHand.SelectCards does instead of the deck grid: the
  backstop fades in, the prompt shows as SelectionHeader, the cards the
  filter refuses hide and the hand lays out without them
  (ActiveHolders); A moves a card to SelectedHandCardContainer (300 px
  apart at 0.8, the row rising and shrinking past 3 and 6 cards) and back
  (up / down between the rows), Y confirms once min..max are picked (the
  confirm button slides in then), B cancels a cancelable one. Upgrading
  shows the card in UpgradePreviewContainer (before / after). The logic
  labels FromHandForUpgrade "hand_upgrade" (its header
  CHOOSE_CARD_UPGRADE_HEADER) and gives the select's cards their
  instance_id. `CombatScreen::begin_hand_select / end_hand_select /
  hand_select_input`, `HandSelectScreen` in game_client.cpp.
- Creature focus in combat (NCombatRoom's pad navigation): up from the
  hand focuses the creatures (leftmost first), left / right moves between
  them by their place on screen (wrapping), down or B goes back; Y still
  ends the turn. The focused creature shows its nameplate, its selection
  reticle and its hover tips (Creature.HoverTips: intents and powers; the
  logic sends them as each enemy's `tips` and the decision's
  `player_tips`, presentation-only in trace_compare).
- Soak (`work/soak.sh`, ignored): spire_play `--auto` runs per character,
  no crash, no logic error, no plain-list screen; the auto player dies in
  act 1, so `spire_play --hp N` now sets the player's HP after the start
  for whole-run soaks (`--no-shots`: the log only). A whole run takes
  1,600-2,400 screens, 35-55 minutes; the script runs two characters at a
  time from copies of spire_play and the logic (work/soak-bin,
  work/soak-logic), so builds go on meanwhile.

## Alpha push, part 6 (2026-10-20): creature effects, special nodes

Pushing is on now: commit and `git push origin claude/peaceful-ritchie-zgh7h5`
(Windows git has the user's login). Still no pull requests and no XDK
build until the alpha RC.

- All 52 spine-event scripts on creature visuals run (`creature_vfx.cpp`):
  the characters' four, 31 as data (VFX_TABLES: ops on nodes for "ready",
  "event:<name>", "start", "start!<anim>", "start=<anim>"; restart / emit /
  stop / show / hide / clear a trail / fire the beam / put a bone at the
  target) and the rest as ports (Axebot, Kaiser Crab boss, Soul Fysh,
  Vantom, Sewer Clam, Oil Spill, Living Gas, Entomancer, Waterfall Giant,
  Aeon Glass, Amalgam, Cubex Construct with NLaserVfx, Ceremonial Beast,
  Decimillipede segment, Test Subject, Vine Shambler vines) plus
  NKinPriestBeamVfx and NDecimillipedeSegmentDriver. Every node path is
  checked against the scenes: `python tools/check_vfx_paths.py` (exit 0).
- Special nodes: the patcher appends a generic call's type arguments to
  its hook id; StandIns answers NCreature.GetSpecialNode<T> (and
  Visuals.GetCurrentBody().GetNode<T>) with a stand-in of the script class
  named "<who>#<path>" ("body:<path>"), which Steps passes as the call's
  first argument; the client's `special_node_call` finds the node in the
  creature's visuals and runs the port (VfxAnchors::visuals_of /
  origin_of). TestMode guards that are presentation only now read off /
  on there: Myte.ToxicMove, SnappingJaxfruit.EnergyOrb, Flyconid's two
  spore moves, Creature.GetCreatureNode (testmode.presentation.is_on).
  DecimillipedeSegment.AnimSegmentsAttack too (see below: the rocks
  become a vfx cue).
- Death delay: a dead monster that does not fade is hidden only once no
  IDeathDelayer in its visuals holds it (`death_delayed`: the Ceremonial
  Beast's death particles), at most 10 s later.
- Long press confirmation (settings `long_press`): Y held fills the end turn
  bar (0.5 s, NEndTurnLongPressBar) and then ends the turn; letting go
  drains it.
- Intro logo (`logo_screen.cpp`, NLogoAnimation): before the title unless
  `data/user/settings.save` has `skip_intro_logo` (read at start-up through
  `Loc::data_path`). Fade in, a second, the logo dropping in with its sound,
  the background to teal, fade out; A/B cut it short. SpineSprite::bounds()
  sizes it (a third of the screen). `spire_play --menu` lets it play out
  before the first press; `--shots-at F1,F2,...` saves those frames.
- spire_check: a health bar label hidden by InfiniteWithoutNumbers counts as
  settled (the full encounters replay flagged the Waterfall Giant's stale
  numbers); the encounters replay is clean again.
- Particle collision: LightOccluder2D is a node now (no lights in the
  game: it is only the floor GPU particles hit). Particles whose
  ParticleProcessMaterial has `collision_mode` 1 bounce off the occluder
  polygons of their tree (signed distance to the filled polygons, radius
  `collision_base_size`, the material's friction / bounce response as
  Godot's shader does it); mode 2 hides them on contact. Seen on the coin
  explosions, treasure room, blood wall, rock shatter, sandy / slime
  impacts.
- SpineMesh2D: the 30-36 empty slot meshes saved in the rest-site and
  character-select scenes load as plain Node2D (spine-godot makes its own
  at run time; these draw nothing).
- Spine dark colour and screen blend checked against the game's 164
  skeletons: dark colours are black except Inklet's shadow (0.02 grey), so
  drawing the light colour is exact enough; the two screen-blend skeletons
  (Neow, the Defect's select) sit in scenes that give them a Mix
  screen_material, which we already honour. Neither is noted as a gap now.
- Language setting: the settings' first line is NLanguageDropdown again
  (A opens the list of LocManager.Languages in their own words, a warning
  icon under 90% translated from `data/localization/completion.json`;
  greyed in a run). Picking one sends `set_setting language`; the logic
  saves SettingsSave.Language and sets LocManager up again
  (`ApplyLanguage`; commands without "lang" use the saved language,
  English when none, so traces are unchanged). The client follows the
  settings answer (`Loc::set_language`, English per missing key) and
  MenuFlow rebuilds the title and the settings screen (NGame.Relocalize).
  At start-up both read `data/user/settings.save`. MegaLabel /
  MegaRichTextLabel swap in the FontManager fonts for jpn, kor, pol / rus,
  tha, zhs, zht; the shaper falls back per character to a font's
  fallbacks, then to the languages' regular fonts (Godot's system fallback,
  which the native names in the picker need).
- Fonts per language (DECISIONS D15): `convert.py refont <pck> <pack-in>
  <pack-out> --languages eng,deu,...` re-exports only the fonts, each
  language folder's fonts with that language's characters only (else just
  the native names), the pages packed again, and writes
  `spire/languages.json` (the picker shows only those; English for older
  packs). Test pack: `work/pack-lang/pack.s3p` (eng, deu, esp, fra, ind,
  ita, pol, ptb, rus, spa, tha, tur; fonts 81 MB) with its `data/` copied
  beside it. CJK is left out until font pages can be freed (D15).
- Decimillipede segments attack in combat: AnimSegmentsAttack reads
  TestMode off; the segments' AttackShake comes as special-node cues and
  its rocks as the same "vfx at screen" cue as PlayFullScreenInCombat
  (`vfx.scene_at_screen`: replace_calls now takes instance targets, the
  receiver first).
- spire_play: give the logic its data folder inside --logic
  (`--logic "SPIRE_DATA=work/x dotnet ..."`); SPIRE_DATA set in front of
  spire_play no longer reaches it, and the logic then uses the repo's
  `data/` (git-ignored dev saves; keep its settings.save language null).
- Not done: Godot nodes the game moves itself (bone nodes tweened by
  Tunneler, KinFollower, ...).

## Alpha push, part 5 (2026-10-20): spine events

Paused here at the user's request. Validation: `spire_check` on new16
with the new pack 0 failures (1,421 / 995 / 366 / 181 / 26), D3D9 check
rc=0, strict C++98 PC build clean, converter tests 31 OK. No logic change
since part 4 (traces new16: differences 0).

- Spine animation events: the converter keeps the animations' event
  timelines (engine skeleton binary version 2; the engine reads 1 and 2);
  SpAnimationState records entry starts and event keys passed
  (`fired`, loops and queued entries included); SpineSprite::fired() holds
  a frame's, read by the scripts on its children in their process.
- `convert.py respine <pck> <pack-in> <pack-out>`: a copy of a built pack
  with only its skeletons converted again (6 s). work/pack-sound-effects/
  pack.s3p is that copy now; the previous one is pack.v1.s3p (1 GB, can be
  deleted once nothing needs it).
- Slot nodes' own shader materials take their parameters from the slot
  node (shader_begin's item), so scripts can drive one slot's material
  (`shader_params`, vec2 as "<name>.x/.y").
- `creature_vfx.cpp`: SpineScript (the common part: on_ready / on_event /
  on_start / tick, particles helpers, slot parameters) and the characters'
  NIroncladVfx (slash swept, eye fire), NDefectVfx (sparks),
  NNecrobinderVfx (scythe flames, slash, hurt / low-health particles, head
  flame), NRegentVfx (death particles, explosion, the two weapons).

Next: the 48 monster N*Vfx scripts driven by spine events (none run yet;
work/vfx_scripts.txt has them all, stripped, smallest first; the list is
work/vfx_list.txt). Then the special-node stand-ins (part 4's plan), which
several of them need (SetTarget, Fire, Regenerate, AttackShake).

## Alpha push, part 4 (2026-10-20)

Validation: traces `differences: 0` (base5 vs new16), `spire_check` on
new16 0 failures (1,421 / 995 / 366 / 181 / 26), D3D9 check rc=0, strict
C++98 PC build clean, encounters coverage 85/85 ok.

- Bestiary (`bestiary_screen.*`, from the compendium once a monster was
  beaten): sidebar (act dividers, unseen entries grey and skipped, room
  colours), the selection arrow (in a plain holder so the content's
  container leaves it alone), the three layouts (default; Kaiser Crab: the
  background shifted to the part's arm; Decimillipede: the segments at the
  EncounterSlots markers, placed each frame), move buttons (LB RB RT LT X,
  then the right stick: PadState RS_*), Y moves / stats, LB / RB filters,
  quotes in the character's bubble, B saves the mode (`set_setting
  bestiary_actions`). The client reads the skeleton's revive / hurt / die
  and sends them as `anims`; the logic hands GenerateBestiaryMoveList a
  stand-in NCreatureVisuals whose skeleton answers HasAnimation from them
  (StandIns.SkeletonAnims), so the game's own list comes out (the overrides
  insert at fixed indices). `bestiary_move` plays a move on fresh copies
  (StandIns.InBestiary: NBestiary.Instance / GetCreatureNode / Layout are
  stand-ins); its steps' cues play in the screen (VfxAnchors: player side
  at the origin as VfxCmd does outside a fight). spire_view takes a
  `bestiary` answer with viewer-only `entry` / `move`.
- Kaiser Crab background: `NCombatRoom.Background` is a stand-in and
  `Node.GetNode<T>` asks `Node.Resolve` (GodotStubs), so Crusher / Rocket
  reach `%KaiserCrab` and its calls are cues; NKaiserCrabBossBackground is
  ported (anim_scripts.cpp: body / arm / reaction tracks, the right arm's
  rest / charge state; `kaiser_crab_call`, its Tasks' waits in the steps).
  Spine: `add_empty_animation` queues properly now.
- Creature animators: the logic runs each creature's CreatureAnimator (the
  monster's / character's GenerateAnimator; `Animators.cs`) on every
  TriggerAnim and sends what it plays (`play` on anim cues, `anim_init` in
  the creature view). 83 monsters have their own triggers / states
  (Corpse Slug's attack_double, devouring hurt / die, low-health idles);
  the client stops a trigger's ops at the first animation the skeleton
  lacks (`play_animator_ops`), where the game's SetNextState returns.
- Tweens: TRANS_BOUNCE.

Next: the creatures' special nodes. `NCreature.GetSpecialNode<T>` returns
null in the logic, so the calls the game makes on them never become cues
(NKinPriestBeamVfx.Fire, NDecimillipedeSegmentVfx.Regenerate,
NDecimillipedeSegmentDriver.AttackShake, NMyteVfx / NSnappingJaxfruitVfx
.SetTarget; Flyconid's spores via Visuals.GetCurrentBody). Plan: the
patcher appends a generic call's type arguments to the hook id, StandIns
returns a stand-in of that script class per (creature, path), Steps names
it "eN#path" as the receiver, the client ports each script (several need
spine animation events, which the runtime does not dispatch yet). Godot
nodes the game moves directly (bone nodes tweened by Tunneler, KinFollower,
...) stay out of reach for now.

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

Reconciled 2026-10-21 (part 10) against the alpha push parts above; ticks
name where it landed. Open items stay at the end of each group.

**Flow and combat**
- [x] Persistent combat scene; state changes animated from `steps` (bars,
      powers, intents, energy, piles, card moves, generated cards) at the
      game's timings (alpha push parts 2-4, spire_check's presentation
      checks).
- [x] NPlayerHand: draw / discard / play flights, retained cards, hand
      selection (parts 7-8).
- [x] VFX lifetimes, hit flashes, shakes (NCreature, ScreenShake), hit-stop
      (parts 5-9).
- [x] Orb channel / evoke / passive effects (spire_check orb checks).
- [x] Doom bar; doom deaths and every non-attack death (part 6).
- [x] Creature focus tips (powers, intents).
- [x] Combat VFX shaders (part 8 and the survey; the mirror in part 9;
      NPowerUpVfx's 3D back layer in part 10, the only 3D in the game).
- [x] Turn banners, combat start / end, victory and boss transitions.

**Run flow and screens**
- [x] Room transitions and map travel (the path, the marker, the rise).
- [x] Act transitions, act banners, The Architect's ending, game over.
- [x] Crystal Sphere minigame screen.
- [x] Pause / settings / credits, deck and pile viewers, map overlay,
      inspect card.
- [x] Saving and continue, the main menu's Continue (profiles
      checkpoint; console path untested).
- [x] Compendium, run history, statistics, timeline (single-player
      parts).
- [x] FTUEs and the ascension popup (part 10; all seen on screen).
- [x] No decision shown as the plain list: spire_play marks one with
      "(plain list)" and work/soak.sh counts them (plain=0 on all five,
      part 10's soak).

**Sound**
- [x] Converter: the FMOD Studio banks and temp sfx.
- [x] Mixer, events, the steps' sound cues, music (NRunMusicController),
      UI sounds; the console's XAudio2 output (engine/audio/output.cpp;
      console test pending).

**Performance and robustness**
- [x] GPU texture budget + LRU; idle screen textures released (`2adfce5`).
- [x] Draw call batching (D3D9 renderer).
- [x] Log appended and flushed per line (`2adfce5`).
- [ ] The slow rewards decision (Xenia: ~2 s for the response that ends a
      fight). PC: 33 ms JIT, 26 ms with the native (translated) logic,
      the heaviest regular command but nothing pathological; Xenia's
      emulated PPC is ~75x slower. Measure on the console before acting.
- [ ] Turbulent particles on the console (part 10: ~300 noise gradients
      per particle and step).
- [x] Soak: long spire_play `--auto` runs on every character with no
      plain-list screens, missing textures or hangs (part 9 ran all five;
      rerun after part 10: clean).

**Not ported (by decision)**: multiplayer and everything that only exists
for it (votes, other players' hands and holders).

## Known gaps in the logic adapter

DECISIONS D6: healing between acts re-implemented in the adapter;
`random_runs.py`'s agent answers rewards/treasure differently from the
oldest traces (hence the normalisation in `trace_compare.py`).
`InitProgressData` warns once on a fresh profile (harmless).
