// spire360.xex: the game. The translated game logic runs in this process
// on its own hardware thread; the engine shows its decisions as the game's
// screens and the controller plays.
//
//   game:\spire\pack.s3p   the converter's build output (convert.py build)
//   game:\spire\data\      its data folder (localization, res_dirs.json)
//   game:\spire360.txt     log: startup, logic errors, frame timings
//
// Pad: on the title screen A starts a run. In a run: D-pad picks, A
// confirms, Y ends the turn. Back writes the log now; Back + Start quits.
// Debugging: Back + X cycles the canvas group modes (D3D9Renderer::
// set_group_debug), Back + Y turns the gamma ramp off and on.
#include <xtl.h>

#include <math.h>
#include <stdarg.h>
#include <stdio.h>

#include "../../engine/core/pack.h"
#include "../../engine/game/game.h"
#include "../../engine/game/game_client.h"
#include "../../engine/game/loc.h"
#include "../../engine/game/logic_link.h"
#include "../../engine/gfx/d3d9_renderer.h"
#include "../../engine/scene/canvas.h"
#include "../../engine/scene/scene.h"
#include "../../engine/scene/tree.h"
#include "../../engine/text/font.h"

extern "C" int spire_logic_start(const char* root);

using namespace spire;

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
    HANDLE h = CreateFileA("game:\\spire360.txt", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD n = 0;
    WriteFile(h, g_log.c_str(), (DWORD)g_log.size(), &n, NULL);
    CloseHandle(h);
}

static void engine_log(const char* line) { logf("%s", line); }

static double now_ms() {
    LARGE_INTEGER f, c;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart * 1000.0 / (double)f.QuadPart;
}

/* XInput -> the engine's pad bits. Triggers count as pressed past half. */
static unsigned pad_bits(const XINPUT_GAMEPAD& g) {
    static const struct {
        WORD x;
        unsigned bit;
    } map[] = { { XINPUT_GAMEPAD_A, PadState::A },          { XINPUT_GAMEPAD_B, PadState::B },
                { XINPUT_GAMEPAD_X, PadState::X },          { XINPUT_GAMEPAD_Y, PadState::Y },
                { XINPUT_GAMEPAD_LEFT_SHOULDER, PadState::LB }, { XINPUT_GAMEPAD_RIGHT_SHOULDER, PadState::RB },
                { XINPUT_GAMEPAD_DPAD_UP, PadState::UP },   { XINPUT_GAMEPAD_DPAD_DOWN, PadState::DOWN },
                { XINPUT_GAMEPAD_DPAD_LEFT, PadState::LEFT }, { XINPUT_GAMEPAD_DPAD_RIGHT, PadState::RIGHT },
                { XINPUT_GAMEPAD_START, PadState::START },  { XINPUT_GAMEPAD_BACK, PadState::BACK } };
    unsigned b = 0;
    for (size_t i = 0; i < sizeof(map) / sizeof(map[0]); ++i)
        if (g.wButtons & map[i].x) b |= map[i].bit;
    if (g.bLeftTrigger > 128) b |= PadState::LT;
    if (g.bRightTrigger > 128) b |= PadState::RT;
    /* The left stick moves the focus like the D-pad. */
    if (g.sThumbLX < -16000) b |= PadState::LEFT;
    if (g.sThumbLX > 16000) b |= PadState::RIGHT;
    if (g.sThumbLY > 16000) b |= PadState::UP;
    if (g.sThumbLY < -16000) b |= PadState::DOWN;
    return b;
}

/* The console's TV output shows the frame darker than a PC monitor (the
 * Xenia screenshots measure about 2.5 / 2.2 in gamma against the game's
 * colours), so the display ramp lifts it back: out = in^(2.2 / 2.5). */
static void set_gamma(IDirect3DDevice9* dev, bool on) {
    D3DGAMMARAMP ramp;
    for (int i = 0; i < 256; ++i) {
        double v = i / 255.0;
        if (on) v = pow(v, 2.2 / 2.5);
        WORD w = (WORD)(v * 65535.0 + 0.5);
        ramp.red[i] = ramp.green[i] = ramp.blue[i] = w;
    }
    dev->SetGammaRamp(0, 0, &ramp);
}

void __cdecl main() {
    set_log_sink(engine_log);
    logf("spire360 starting");
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
    bool gamma = true;
    set_gamma(dev, gamma);
    D3D9Renderer renderer;
    std::string err;
    if (!renderer.init(dev, &err)) {
        logf("renderer: %s", err.c_str());
        flush_log();
        return;
    }
    Pack pack;
    if (!pack.open("game:\\spire\\pack.s3p", &err)) {
        logf("pack: %s (copy the converter's output folder to game:\\spire\\)", err.c_str());
        flush_log();
        return;
    }
    logf("pack: %u entries, game version %s", (unsigned)pack.count(), pack.game_version().c_str());
    double t0 = now_ms();
    int rc = spire_logic_start("game:/spire");
    if (rc != 0) {
        logf("logic: could not start (%d)", rc);
        flush_log();
        return;
    }
    logf("logic: started in %.1f ms", now_ms() - t0);
    Loc::init("game:\\spire\\data");
    install_game_scripts();
    SceneLibrary* lib = new SceneLibrary(&pack);
    FontLibrary* fonts = new FontLibrary(&pack);
    FontLibrary::set_current(fonts);
    Canvas canvas(&renderer, &pack);
    SceneTree* tree = new SceneTree();
    tree->set_viewport(Vec2(1920, 1080), Xform2D::scaling(2.0f / 3.0f, 2.0f / 3.0f));

    /* The title screen until A; then the run's screens. */
    Node* title = lib->instantiate("scenes/screens/main_menu.tscn", &err);
    if (title) tree->root()->add_child(title);
    else logf("main menu: %s", err.c_str());
    tree->layout();
    SrtLink link;
    GameClient* game = NULL;
    unsigned prev = 0;
    double last = now_ms(), acc_ms = 0, wait_start = 0;
    int frames = 0, shown = 0;
    for (;;) {
        XINPUT_STATE xs;
        ZeroMemory(&xs, sizeof(xs));
        unsigned held = XInputGetState(0, &xs) == ERROR_SUCCESS ? pad_bits(xs.Gamepad) : 0;
        PadState pad;
        pad.held = held;
        pad.pressed = held & ~prev;
        prev = held;
        if ((held & PadState::BACK) && (held & PadState::START)) break;
        if (pad.pressed & PadState::BACK) flush_log();
        if ((held & PadState::BACK) && (pad.pressed & PadState::X)) {
            renderer.set_group_debug((renderer.group_debug() + 1) % D3D9Renderer::GROUP_DEBUG_MODES);
            pad.pressed &= ~PadState::X;
        }
        if ((held & PadState::BACK) && (pad.pressed & PadState::Y)) {
            gamma = !gamma;
            set_gamma(dev, gamma);
            logf("gamma ramp %s", gamma ? "on" : "off");
            pad.pressed &= ~PadState::Y;
        }

        if (!game && (pad.pressed & (PadState::A | PadState::START))) {
            if (title) {
                tree->root()->remove_child(title);
                delete title;
                title = NULL;
            }
            game = new GameClient(lib, tree, &link);
            std::string seed = format("%08X", (unsigned)GetTickCount());
            logf("run: Ironclad, seed %s", seed.c_str());
            game->start_run("Ironclad", seed);
            wait_start = now_ms();
            pad.pressed = 0;
        }
        if (game) {
            bool was_busy = game->busy();
            game->update(pad);
            if (game->decision_count() != shown) {
                shown = game->decision_count();
                logf("decision %d: %s (logic %.0f ms)", shown, game->decision()["decision"].str().c_str(),
                     now_ms() - wait_start);
                tree->layout();
            }
            if (!was_busy && game->busy()) wait_start = now_ms();
            if (!game->last_error().empty() && link.finished()) {
                logf("logic stopped: %s", game->last_error().c_str());
                flush_log();
                break;
            }
        }

        double t = now_ms();
        double dt = (t - last) / 1000.0;
        last = t;
        double f0 = now_ms();
        tree->process(dt > 0.1 ? 0.1 : dt);
        tree->layout(1);
        renderer.begin(1280, 720, Color(0.0923724f, 0.122398f, 0.116929f, 1.0f));
        tree->render(canvas);
        renderer.end();
        acc_ms += now_ms() - f0;
        dev->Present(NULL, NULL, NULL, NULL);
        if (++frames == 600) {
            logf("%.2f ms per frame (CPU, layout + draw), %ld draw calls", acc_ms / frames, renderer.draw_calls());
            frames = 0;
            acc_ms = 0;
        }
    }
    delete game;
    delete tree;
    delete fonts;
    delete lib;
    logf("spire360 done");
    flush_log();
}
