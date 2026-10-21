# Third-party code

| Component | Where | License | Notes |
|---|---|---|---|
| w2c2 | `third_party/w2c2/` | MIT, (c) 2021 Bastian Mueller | Vendored at the commit in `UPSTREAM_COMMIT`; local patches in `PATCHES.md`. Compiled into the console build. |
| sts2-cli | `logic/Spire.GodotStubs/Manual/`, `logic/Spire.Logic/RunDriver.cs`, `Selector.cs`, `Loc.cs`, `Hooks.cs`, `Program.cs` | MIT, (c) 2025 Hao Wu (`logic/Spire.GodotStubs/LICENSE.sts2-cli`) | Godot stubs and the run driver were forked and reworked (single-threaded, no Harmony). |
| Godot C# API surface | `logic/Spire.GodotStubs/Generated/GodotApi.g.cs` | MIT, (c) Godot Engine contributors | Signatures only, generated from GodotSharp.dll; bodies are empty. |
| Godot Engine 4.5.1 (ported code) | `engine/scene/`, `engine/text/`, `engine/gfx/shaders/canvas.hlsl` | MIT, (c) Godot Engine contributors | Layout, containers, style boxes, nine patches, canvas order, Label / RichTextLabel, text shaping and line breaking, CPUParticles2D, GPUParticles2D (the ParticleProcessMaterial shader and particles.glsl's frame loop), Line2D's line builder, the MSDF text shader and the default theme's values, ported to C++03 to match the game's rendering. The line builder retains its upstream license header. |
| Godot Engine 4.5.1 (binary, PC only) | downloaded by `convert.py fetch-godot` | MIT | Renders the game's fonts as MSDF glyph pages during conversion; never on the console. |
| Mono.Cecil | NuGet, PC tools only | MIT | Patcher and API dump; never on the console. |
| FModBankParser 1.2.0 | NuGet, `tools/fmod_metadata/` | Apache-2.0 | Offline Studio bank metadata; no FMOD runtime DLLs. Never on the console. |
| Fmod5Sharp 3.0.1 | NuGet, `tools/fmod_metadata/` | MIT | Rebuilds FSB5 Vorbis packets as Ogg at conversion time, without re-encoding. Never on the console. |
| stb_vorbis 1.22 | `third_party/stb/stb_vorbis.c` | MIT or public domain | Streaming Vorbis decoder; pinned revision in `UPSTREAM_COMMIT`. Upstream license is retained in the source. |
| vgmstream | Optional external PC tool | ISC (its optional codec dependencies have their own licenses) | Independent audio conversion checks via `--vgmstream`; never on the console. |
| NativeAOT-LLVM, .NET runtime | NuGet, linked into logic.wasm | MIT | Built on the player's PC. |
| wasi-sdk / wasi-libc | linked into logic.wasm | Apache-2.0 WITH LLVM-exception, MIT, BSD | Built on the player's PC. |

Not included:

- **Spine.** The game's skeletal animations are played by this project's own
  Spine 4.2 runtime (`engine/spine/`, `converter/spire_convert/spine.py`),
  written from the format's public description: no Esoteric Software code.
- **Xbox 360 XDK.** Microsoft licensed it to registered developers only; you
  need your own copy to build the XEX.

Game files (sts2.dll and its dependencies, the .pck and everything extracted
or compiled from them) belong to Mega Crit and stay on the player's machine.
