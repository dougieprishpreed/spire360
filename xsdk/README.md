# The xSDK build

`spire360.xex` built with the open [xsdk](https://github.com/xenosworks/xsdk) toolchain and
[vulkan4xenon](https://github.com/xenosworks/vulkan4xenon), in place of the Microsoft XDK
(`xdk/`, no longer worked on). It runs on an RGH or JTAG console from the dashboard, as the XDK
build did, and reads the same files: `game:\spire\` (the converter's output), `spire360.ini`, and
writes `spire360.txt`, `spire360_logic.txt` and `spire360_shot_NN.bmp`.

What differs from the XDK build:

- **The renderer is Vulkan** (`engine/gfx/vk_renderer.cpp`): the D3D9 renderer's design in
  Vulkan's terms, drawn by vulkan4xenon on xsdk's GPU layer. The canvas draws into an offscreen
  1280x720 scene, which the last pass shows through the display ramp, out = in^(2.2 / 2.5) (the
  XDK build's D3D gamma ramp; its other presets and Back + Y are gone, the hardware runs having
  settled on this one).
- **Its shaders are GLSL** (`engine/gfx/shaders/vk/`): `canvas.hlsl`'s pixel shader split by its
  branches, and `effects.hlsl`'s shaders as written under a few names (`tools/xsdk/vk_shaders.py`
  generates them at build time, so the HLSL stays the one source for the D3D9 and software
  renderers too). xost, xsdk's shader compiler, takes a subset of GLSL; `vk_shaders.py` writes
  every `?:` as a select and `!=` as `!(==)`, and `common.glsl` stands in for `atan2`, `sin`,
  `cos` and `length`. xost's select is `f + (t - f) * c`, so a division by 0 on the side not
  taken still makes NaN: divisions by a texel's alpha go through `spire_div`. The build compiles
  them all through xost, so a shader it refuses stops the build rather than the console.
- **The platform calls are xsdk's**: its POSIX layer (threads, files, `nanosleep`) with
  `xsdk_thread_move` for the hardware threads, its audio feeder (`engine/audio/output.cpp`), the
  kernel's reserve and commit for the logic's memory (`runtime/src/srt_plat_posix.c`), and
  `XamInputGetState` for the pads (`xsdk/main.cpp`).
- **Frame pacing**: vulkan4xenon's swapchain shows each frame for one vblank; at 30 fps the
  renderer queues a frame two vblanks after the last (xsdk's present statistics), as Balatro360
  does.

## Build

On Linux, or in WSL on Windows, with xsdk's toolchain built (its `docs/GETTING_STARTED.md`) and
checkouts of xsdk, vulkan4xenon and glslang-xenon (its host build gives `glslang`); here they
sit in `/opt/ctr360-xsdk`:

```
export XSDK=/opt/ctr360-xsdk/xsdk VULKAN4XENON=/opt/ctr360-xsdk/vulkan4xenon \
  GLSLANG=/opt/ctr360-xsdk/glslang/build-host/StandAlone/glslang XSDK_TOOLCHAIN=/opt/ctr360-xsdk/toolchain
tools/xsdk/build.sh
```

The translated logic comes from `out/logic/c` (`tools/build_logic.py`, as for the XDK build).
Spire360 follows the latest xsdk and vulkan4xenon: `build.sh` builds on whatever the checkouts
hold and says when their origin has moved on. Output: `/opt/spire360-xsdk/output/default.xex`
(`SPIRE_XSDK_BUILD` moves it), with the licences it ships under in `licenses/`. `JOBS` compilers
run at once, 2 by default (GCC takes about 1 GB on each of the logic's large files). The first
build compiles the logic (the longest part); later builds compile what changed, a minute or so.

The XEX holds the translated logic, the player's own game code, so it is built on the player's
PC and never in CI or shared.

## Check on the PC first

xSDK titles don't run in Xenia. The Vulkan renderer runs on the PC too, on Mesa's lavapipe:

```
tools/xsdk/host_check.sh       # the same environment as build.sh
/opt/spire360-xsdk/host/spire_view work/pack-80/pack.s3p <scene> out.png --scale 0.5 --vk
```

`spire_view` draws a scene through the Vulkan renderer with `--vk`, through the software
renderer without; the two should match but for edges. `--vk-trace` draws it again with the GPU
trace on (below), lavapipe's times standing in for the console's. `tools/xsdk/check_shaders.py
/opt/spire360-xsdk/output/shaders` (with `XSDK` and `GLSLANG` set) runs every shader on xost's
simulator as the console runs it: optimised and plain builds must agree, the vertex and plain
pixel shaders must give what they should, and none may put out a NaN or infinity for transparent
texels, its parameters random or 0. `SPIRE_VK_VALIDATE=1` turns on Khronos's
validation layer, `SPIRE_VK_TRACE=1` logs each render pass and draw.

## Run

Copy `default.xex` beside the `spire\` folder on the console's drive (where the XDK build's was),
with `spire360.ini` if you want its options, and start it from the dashboard. The first line of
`spire360.txt` names the source commit.

With `profile=1` in `spire360.ini` the log has, every 600 frames: the frames' GPU time by the
GPU's clock (time stamps) and where the CPU waited (for the GPU, a display buffer, the 30 fps
pacing); the render passes, uploads and vulkan4xenon's eDRAM work a frame; and every third 600,
the GPU probe (the frame drawn again without each kind of draw, a hitch of a few hundred ms) and
a GPU trace of that frame, each draw timed on its own (`gpu trace item/shader/pass/draw` lines:
the scene's items by type, name and parent, then by shader, by pass, and the slowest draws). A
frame over 36 ms on the
GPU is traced too (one every 20 s at most); a frame over 40 ms of CPU work gets a `slow frame`
line, and a frame over 250 ms (a `long frame`) the loads it made. Back + X cycles the canvas
groups' debug modes: off (children drawn unmasked), each group's pass over its whole target, and
normal.
