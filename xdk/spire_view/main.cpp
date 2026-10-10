// spire_view.xex: renders the game's scenes from pack.s3p with the engine's
// D3D9 renderer at 1280x720, the first console check of the presentation
// layer (layout, textures, MSDF text, particles, ported shaders).
//
//   game:\spire\pack.s3p     the converter's build output (convert.py build)
//   game:\spire360_view.txt  log: load and frame timings, problems
//
// Pad: Left/Right = previous/next scene, A = toggle scripts (reload),
// Back = write the log now, Start = quit.
#include <xtl.h>

#include <stdarg.h>
#include <stdio.h>

#include "../../engine/core/pack.h"
#include "../../engine/game/game.h"
#include "../../engine/gfx/d3d9_renderer.h"
#include "../../engine/scene/canvas.h"
#include "../../engine/scene/scene.h"
#include "../../engine/scene/script.h"
#include "../../engine/scene/tree.h"
#include "../../engine/text/font.h"

using namespace spire;

static const char* const SCENES[] = {
    "scenes/screens/main_menu.tscn", "scenes/rooms/combat_room.tscn", "scenes/cards/card.tscn",
    "scenes/combat/creature.tscn", "scenes/combat/player_hand.tscn", "scenes/ui/top_bar.tscn",
    "scenes/screens/map/map_screen.tscn", "scenes/screens/rewards_screen.tscn", NULL
};

static std::string g_log;

static void logf(const char* fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    _vsnprintf(buf, sizeof(buf) - 2, fmt, ap);
    va_end(ap);
    buf[sizeof(buf) - 2] = 0;
    strcat(buf, "\n");
    g_log += buf;
    OutputDebugStringA(buf);
}

static void flush_log() {
    HANDLE h = CreateFileA("game:\\spire360_view.txt", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD n = 0;
    WriteFile(h, g_log.c_str(), (DWORD)g_log.size(), &n, NULL);
    CloseHandle(h);
}

static double now_ms() {
    LARGE_INTEGER f, c;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart * 1000.0 / (double)f.QuadPart;
}

struct View {
    SceneTree* tree;
    double load_ms;
};

static View load_scene(SceneLibrary& lib, int idx) {
    View v;
    v.tree = new SceneTree();
    double t0 = now_ms();
    std::string err;
    Node* n = lib.instantiate(SCENES[idx], &err);
    if (!n) logf("%s: %s", SCENES[idx], err.c_str());
    v.tree->set_viewport(Vec2(1920, 1080), Xform2D::scaling(2.0f / 3.0f, 2.0f / 3.0f));
    if (n) v.tree->root()->add_child(n);
    v.tree->layout();
    v.load_ms = now_ms() - t0;
    logf("%s: instantiated and laid out in %.1f ms", SCENES[idx], v.load_ms);
    return v;
}

void __cdecl main() {
    logf("spire_view starting");
    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    D3DPRESENT_PARAMETERS pp;
    ZeroMemory(&pp, sizeof(pp));
    pp.BackBufferWidth = 1280;
    pp.BackBufferHeight = 720;
    pp.BackBufferFormat = D3DFMT_A8R8G8B8;
    pp.BackBufferCount = 1;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    IDirect3DDevice9* dev = NULL;
    if (!d3d || FAILED(d3d->CreateDevice(0, D3DDEVTYPE_HAL, NULL, D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &dev))) {
        logf("CreateDevice failed");
        flush_log();
        return;
    }
    D3D9Renderer renderer;
    std::string err;
    if (!renderer.init(dev, &err)) {
        logf("renderer: %s", err.c_str());
        flush_log();
        return;
    }
    Pack pack;
    if (!pack.open("game:\\spire\\pack.s3p", &err)) {
        logf("pack: %s (copy the converter's output to game:\\spire\\)", err.c_str());
        flush_log();
        return;
    }
    logf("pack: %u entries, game version %s", (unsigned)pack.count(), pack.game_version().c_str());
    install_game_scripts();
    SceneLibrary* lib = new SceneLibrary(&pack);
    FontLibrary* fonts = new FontLibrary(&pack);
    FontLibrary::set_current(fonts);
    Canvas canvas(&renderer, &pack);
    int scene = 0;
    bool scripts = true;
    View view = load_scene(*lib, scene);
    WORD prev_buttons = 0;
    double last = now_ms(), acc_ms = 0;
    int frames = 0;
    for (;;) {
        XINPUT_STATE pad;
        ZeroMemory(&pad, sizeof(pad));
        WORD b = XInputGetState(0, &pad) == ERROR_SUCCESS ? pad.Gamepad.wButtons : 0;
        WORD pressed = b & ~prev_buttons;
        prev_buttons = b;
        if (pressed & XINPUT_GAMEPAD_START) break;
        if (pressed & XINPUT_GAMEPAD_BACK) flush_log();
        int next = scene;
        if (pressed & XINPUT_GAMEPAD_DPAD_RIGHT) next = SCENES[scene + 1] ? scene + 1 : 0;
        if (pressed & XINPUT_GAMEPAD_DPAD_LEFT) next = scene > 0 ? scene - 1 : (int)(sizeof(SCENES) / sizeof(SCENES[0])) - 2;
        if (pressed & XINPUT_GAMEPAD_A) {
            scripts = !scripts;
            set_scripts_enabled(scripts);
            logf("scripts %s", scripts ? "on" : "off");
            next = -1;
        }
        if (next != scene) {
            delete view.tree;
            if (next >= 0) scene = next;
            view = load_scene(*lib, scene);
        }
        double t = now_ms();
        double dt = (t - last) / 1000.0;
        last = t;
        double f0 = now_ms();
        view.tree->process(dt > 0.1 ? 0.1 : dt);
        view.tree->layout(1);
        renderer.begin(1280, 720, Color(0.0923724f, 0.122398f, 0.116929f, 1.0f));
        view.tree->render(canvas);
        renderer.end();
        acc_ms += now_ms() - f0;
        dev->Present(NULL, NULL, NULL, NULL);
        if (++frames == 120) {
            logf("%s: %.2f ms per frame (CPU, layout + draw), %ld draw calls", SCENES[scene], acc_ms / frames,
                 renderer.draw_calls());
            frames = 0;
            acc_ms = 0;
        }
    }
    delete view.tree;
    delete fonts;
    delete lib;
    logf("spire_view done");
    flush_log();
}
