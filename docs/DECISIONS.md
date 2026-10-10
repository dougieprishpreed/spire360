# Decisions

Newest last. Each entry: what was decided, why, and what would change it.

## D1. Run the real game logic instead of re-implementing it

The first plan re-created every card, relic, power and event in C++ and
checked it against the real game. Running the player's own `sts2.dll` makes
behaviour exact by construction (hook order, RNG, edge cases) and turns a game
update into a rebuild instead of a re-translation. Proven in this repo: the
adapter plays full runs of v0.111.0 for all five characters headless.

Would change if: M0/M2 show the console cannot run it fast enough or fit it in
memory. The re-implementation plan then becomes the fallback, with this
adapter as its oracle.

## D2. NativeAOT-LLVM to WebAssembly, then w2c2 to C, compiled by the XDK

.NET can't target the Xenon directly, and the XDK compiler can't be shipped to
other people, but here the player builds the XEX locally. So the wasm is not
interpreted: w2c2 translates it to portable C89 with big-endian support, and
the XDK compiler turns that into native PowerPC code. w2c2 measures ~7% slower
than native on coremark; an interpreter would be 10x+.

- Mono-on-WASI was rejected: .NET 8 only (the game targets net9), and an
  interpreter on top of translated wasm.
- A Mono or CoreCLR port to the 360 was rejected: needs a modern C compiler for
  the target and a large OS layer.
- An interpreter (wasm3/WAMR) remains a possible distributable fallback; not
  planned.

## D3. One logic thread, CoreLib's WASI event loop, no blocking waits

NativeAOT on WASI is single-threaded; CoreLib provides a single-threaded
thread pool and timer queue. The adapter installs a FIFO synchronisation
context for every call into the game and pumps it; a player decision is an
unfinished task the next command completes. sts2-cli's Task.Run/Thread.Sleep
retry loops (fighting cross-thread races) are gone, and the same model runs
under JIT, so PC tests mean something for the console.

The one synchronous game prompt (event card rewards) runs a nested command
loop instead of blocking.

## D4. Keep sts2-cli's JSON protocol, add `steps`

Line-based JSON over stdin/stdout, compatible with sts2-cli, so its tools and
any agent work against every build. Extended with `steps`: snapshot diffs taken
after every queued continuation plus UI cues. This gives the presentation the
real resolution order without knowing the game's action classes.

## D5. Godot replaced by generated stubs plus a small hand-written core

`Spire.GodotStubs` = hand-written behaviour (node tree, timers, signals,
StringName, Variant, string helpers, math) + code generated from the real
`GodotSharp.dll` for every other member the game references, so every call the
game makes binds. Game logic is separated from the Godot UI by IL patches:

- `ui`: calls from logic into `MegaCrit.Sts2.Core.Nodes.*` become recorded
  cues with safe defaults (no UI code ever runs);
- `godot.null_safe`: calls on Godot engine objects that don't exist headless
  (scenes from the asset cache, tweens) are skipped when the receiver is null.

Neither changes a game rule; both turned whole classes of crashes (monster
death VFX, event portrait VFX) into presentation cues.

## D6. Known fidelity gaps in the adapter (to remove in M6)

Inherited from sts2-cli; each is UI-driven flow the adapter replicates:

1. Post-combat rewards: gold, potions and relics are auto-collected; only the
   card reward is a choice. Real game: the player picks each reward.
2. Treasure rooms auto-pick the first relic.
3. Between-act healing is re-implemented (`HealBetweenActs`).
4. Rest site and some events are forced back to the map after an option.
5. Card-reward alternatives (e.g. Singing Bowl-style options) are not exposed.
6. ~~Crystal Sphere minigame~~: now a `crystal_sphere` decision (tool + cell clicks on the game's own minigame model).
7. Neutralize: sts2-cli's replacement is disabled; verify the original works.
8. Single-target cards are reported unplayable while no enemy can be targeted
   (e.g. Test Subject between death and revive), mirroring what the UI allows.

## D7. The game's AutoSlay bot defines the screen list

The game ships an internal bot (`MegaCrit.Sts2.Core.AutoSlay`) with a handler
per interactive screen: rooms (combat, event, rest, shop, treasure, victory)
and screens (card reward, choose a bundle/card/relic, Crystal Sphere, deck
card select/enchant/transform/upgrade, game over, map, rewards, simple card
select). Protocol coverage is measured against that list.

## D8. Game files never enter git

The player's DLLs, `.pck`, extracted assets and everything built from them
live in ignored folders (`logic/lib/`, `out/`, packs). The repository holds
only original code, vendored MIT code, and the generated Godot API stub
(Godot's own MIT API surface).

## D9. The Godot stubs mirror the real API's metadata, not just its names

The JIT and x64 NativeAOT accept IL that NativeAOT-LLVM rejects ("will
always throw: Invalid IL"): 1,473 game methods in the first wasm build. The
cause was stub metadata that compiled but differed from the real
GodotSharp: `int` enums where Godot's are `long` (call sites push int64),
enum values (`Variant.Type.Color` was 10, real 20), missing intermediate
classes (`InputEventMouseMotion` not an `InputEventMouse`), missing
interfaces (`GodotObject : IDisposable`), and generic constraints.

Rules, enforced by `Spire.ApiDump --check-stubs` in the patch stage:
enums are printed from the real DLL (`--print-enums`); every stub class
derives from its real base (`Manual/Hierarchy.cs` from
`tools/gen_stub_hierarchy.py` holds the intermediates the hand-written
stubs need; the generator plans every real ancestor); generated generic
methods carry the real constraints; and the generator never emits a no-op
member that would hide a working inherited one (`Array<T> : List<T>`).
ILVerify (`dotnet tool install -g dotnet-ilverify`) is the local stand-in
for NativeAOT-LLVM's strictness: StackUnexpected errors went 35,382 -> 2
(both in compiler-generated code real compilers accept). Game behaviour
was unchanged: 25 seeded runs produced byte-identical traces.

## D10. Converter: Python with four wheels; pack.s3p; textures at 2/3

The converter runs once on the player's PC. It uses Pillow (PNG/WebP),
texture2ddecoder (BC1-BC7 decode), etcpak (DXT encode; on a photo it matches
the quality of the game's own S3TC imports) and zstandard (Godot's compressed
resources). All have prebuilt wheels.

The game is laid out for 1920x1080 with `canvas_items` stretch; the console
renders 1280x720, so textures are stored at 2/3 of their size and drawn at
their original size in layout units. Opaque -> DXT1, alpha -> DXT5, tiny ->
ARGB8; resampling on premultiplied alpha. Texture data stays in PC block
order: the console swaps and tiles it for Xenos when creating the texture
(XGEndianSwapSurface/XGTileTextureLevel), which keeps one pack readable by
the PC viewer too.

pack.s3p is one big-endian file: a directory sorted by FNV-1a 64 of the res
path (binary search on the console), CRC32 per entry, 4 KB aligned texture
payloads. Scenes and .tres resources go in as JSON with references already
resolved to pack names. Godot formats are decoded from their source layout
and tested against files Godot 4.5.1 itself produced (tests/fixtures).

## D11. The engine is Godot's 2D layer, ported; text from Godot-rendered MSDF

The presentation draws the game's scenes the way Godot 4.5.1 does, so the
engine (`engine/scene`, `engine/text`) is a C++03 port of the parts of
Godot the scenes use: Control layout and containers, theme lookup with
Godot's default theme, style boxes, nine patches, canvas order, Label and
RichTextLabel. Each port names its Godot source; behaviour is checked by
rendering the same scene in Godot (xvfb, gl_compatibility) and in
`tools/viewer/spire_view` and diffing pixels.

The game imports its fonts as MSDF. The console has no FreeType or
HarfBuzz, so the converter runs Godot 4.5.1 headless
(`tools/godot/font_export.gd`) to render every glyph of each font into MSDF
pages and to measure shaping (advances, kerning and ligatures, from
HarfBuzz through Godot's TextServer). Godot reproduces the game's own
pre-rendered glyphs bit for bit; the engine draws them with Godot's canvas
shader formula. A Godot binary on the player's PC is one more tool for the
converter (`convert.py fetch-godot` downloads the official build). Fonts
the game imports without MSDF are exported as MSDF too (marked
`msdf_substitute`): close, not identical, at small sizes.

Open: glyph pages are big (512x512 RGBA, ~100 MB for the bundle's fonts);
before the console build the converter must subset glyphs to the
characters the selected language uses and repack them.

## D12. Spine: our own runtime for the 4.2 binary format

The game's skeletons are Spine 4.2.39 binary exports (creatures, backgrounds,
the logo). The official runtimes (spine-c) come under the Spine Runtimes
License, which ties their use to a Spine Editor licence. The user chose a
runtime of our own instead: a reader for the 4.2 binary skeleton and the
atlas, and the animation, constraint and attachment evaluation the game's
skeletons use, written from the format's public description and checked
against the game by eye. No Esoteric Software code goes into the repository.
