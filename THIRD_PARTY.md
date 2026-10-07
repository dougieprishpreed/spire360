# Third-party code

| Component | Where | License | Notes |
|---|---|---|---|
| w2c2 | `third_party/w2c2/` | MIT, (c) 2021 Bastian Mueller | Vendored at the commit in `UPSTREAM_COMMIT`; local patches in `PATCHES.md`. Compiled into the console build. |
| sts2-cli | `logic/Spire.GodotStubs/Manual/`, `logic/Spire.Logic/RunDriver.cs`, `Selector.cs`, `Loc.cs`, `Hooks.cs`, `Program.cs` | MIT, (c) 2025 Hao Wu (`logic/Spire.GodotStubs/LICENSE.sts2-cli`) | Godot stubs and the run driver were forked and reworked (single-threaded, no Harmony). |
| Godot C# API surface | `logic/Spire.GodotStubs/Generated/GodotApi.g.cs` | MIT, (c) Godot Engine contributors | Signatures only, generated from GodotSharp.dll; bodies are empty. |
| Mono.Cecil | NuGet, PC tools only | MIT | Patcher and API dump; never on the console. |
| NativeAOT-LLVM, .NET runtime | NuGet, linked into logic.wasm | MIT | Built on the player's PC. |
| wasi-sdk / wasi-libc | linked into logic.wasm | Apache-2.0 WITH LLVM-exception, MIT, BSD | Built on the player's PC. |

Not yet integrated, decide before use:

- **Spine runtime** (spine-c). The Spine Runtimes License requires a Spine
  license to use the runtimes; check it before integrating. Alternative: own
  skeleton player.
- **Xbox 360 XDK**. Microsoft licensed it to registered developers only.

Game files (sts2.dll and its dependencies, the .pck and everything extracted
or compiled from them) belong to Mega Crit and stay on the player's machine.
