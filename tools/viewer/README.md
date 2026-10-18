# spire_view and reference diffs

`spire_view` renders a scene from `pack.s3p` with the engine's software
renderer; `refdiff.py` compares those renders with Godot's own.

Build (PC, strict C++03 like the XDK compiler): `tools/viewer/build.sh`
-> `out/engine/spire_view`. `--time T` runs T seconds of frames (60 fps)
before the shot (default 5 frames, as `shot.gd`), to see animations move.

On Windows, use MSYS2's **MSYS** environment and its `gcc` package
(`pacman -S gcc`), which supplies the POSIX pipes used by `spire_play`.
Keep Windows Python and .NET on PATH (`MSYS2_PATH_TYPE=inherit`). The
build runs one compiler at a time and enables the POSIX declarations
that MSYS2 otherwise hides with `-std=c++98`.

`out/engine/spire_check <pack.s3p> <trace.jsonl> [more traces...]` replays
local `random_runs.py` traces and checks that combat rooms survive across
decisions, HP changes wait for their impact cues, playback finishes, and
two-card selections can be confirmed with a controller. It also checks
one-shot particle completion/restart, VFX node cleanup and delayed tween
timing. It reads the
pack's sibling `data/` folder for localization. Keep traces out of git.

    out/engine/spire_view <pack.s3p> scenes/screens/main_menu.tscn out.png [--scale 0.6667] [--no-scripts] [--time T]
    out/engine/spire_view <pack.s3p> "Some text" out.png --text fonts/kreon_bold.ttf 24

Reference renders come from Godot 4.5.1 (official build) running the game
files under a virtual display, so they need the extracted files
(`convert.py bundle` output, copied into a folder with an empty
`project.godot`... the bundle already has `project.binary`):

    xvfb-run -a -s "-screen 0 1920x1080x24" godot --path <bundle dir> \
        --rendering-driver opengl3 --rendering-method gl_compatibility \
        --resolution 1920x1080 --script tools/godot/shot.gd -- res://scenes/x.tscn refs/x.png

(copy `tools/godot/shot.gd` into the bundle folder first). Then:

    python3 tools/viewer/refdiff.py <pack.s3p> refs/ diffs/

Godot's render runs without the game's C#, so the diff uses `--no-scripts`.
Things the engine does not draw yet are listed per scene.
