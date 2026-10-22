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
#include "../../engine/audio/game_audio.h"
#include "../../engine/game/game.h"
#include "../../engine/game/game_client.h"
#include "../../engine/game/menu_flow.h"
#include "../../engine/game/loc.h"
#include "../../engine/game/logic_link.h"
#include "../../engine/gfx/d3d9_renderer.h"
#include "../../engine/scene/canvas.h"
#include "../../engine/scene/scene.h"
#include "../../engine/scene/tree.h"
#include "../../engine/text/font.h"

extern "C" int spire_logic_start(const char* root);

using namespace spire;

static FILE* g_log_file = NULL;
static bool g_log_started = false;

static void flush_log() {
    if (g_log_file) fflush(g_log_file);
}

static void logf(const char* fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    _vsnprintf(buf, sizeof(buf) - 2, fmt, ap);
    va_end(ap);
    buf[sizeof(buf) - 2] = 0;
    strcat(buf, "\n");
    OutputDebugStringA(buf);
    if (!g_log_file) {
        g_log_file = fopen("game:\\spire360.txt", g_log_started ? "ab" : "wb");
        if (g_log_file) g_log_started = true;
    }
    if (g_log_file) {
        fputs(buf, g_log_file);
        flush_log(); /* A hung frame must leave the last completed log line. */
        if (ferror(g_log_file)) { fclose(g_log_file); g_log_file = NULL; }
    }
}

static void engine_log(const char* line) { logf("%s", line); }

static double now_ms() {
    LARGE_INTEGER f, c;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart * 1000.0 / (double)f.QuadPart;
}

/* Keep protocol latency separate from scene building and effect playback.
 * A rewards screen may be held until the killing blow has finished. */
class TimedLogicLink : public SrtLink {
public:
    TimedLogicLink() : sent_at_(0), pending_(false) {}
    virtual bool send(const std::string& line) {
        sent_at_ = now_ms();
        pending_ = SrtLink::send(line);
        return pending_;
    }
    virtual bool poll(std::string& line) {
        if (!SrtLink::poll(line)) return false;
        if (pending_ && !line.empty() && line[0] == '{') {
            logf("logic response: %.0f ms, %u bytes", now_ms() - sent_at_, (unsigned)line.size());
            pending_ = false;
        }
        return true;
    }
private:
    double sent_at_;
    bool pending_;
};

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
    if (g.sThumbRX < -16000) b |= PadState::RS_LEFT;
    if (g.sThumbRX > 16000) b |= PadState::RS_RIGHT;
    if (g.sThumbRY > 16000) b |= PadState::RS_UP;
    if (g.sThumbRY < -16000) b |= PadState::RS_DOWN;
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
    GameAudio audio(&pack);
    lib->set_audio(&audio);
    if (!audio.open_output()) logf("audio: XAudio2 output unavailable");
    /* The mix on hardware thread 4 (core 2): the frame loop keeps core 0,
     * the logic core 1 */
    else if (!audio.start_thread(4)) logf("audio: no audio thread (mixed in the frame loop)");
    audio.menu();
    FontLibrary* fonts = new FontLibrary(&pack);
    FontLibrary::set_current(fonts);
    Canvas canvas(&renderer, &pack);
    SceneTree* tree = new SceneTree();
    tree->set_viewport(Vec2(1920, 1080), Xform2D::scaling(2.0f / 3.0f, 2.0f / 3.0f));

    /* The main menu of the last profile until a run starts, back to it
     * after the run (MenuFlow). The client asks the logic for the menu's
     * data at once (the game data loads meanwhile). */
    TimedLogicLink link;
    GameClient* game = new GameClient(lib, tree, &link);
    MenuFlow* flow = new MenuFlow(lib, tree, game, pack.game_version());
    tree->layout();
    unsigned prev = 0;
    double last = now_ms(), acc_ms = 0, acc_draw = 0, acc_present = 0, wait_start = 0;
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
        {
            bool was_busy = game->busy();
            bool was_starting = flow->starting();
            if (!flow->update(pad)) break;
            std::string ev = flow->take_event();
            if (!ev.empty()) logf("%s (logic %.0f ms since start)", ev.c_str(), now_ms() - t0);
            if (!was_starting && flow->starting()) wait_start = now_ms();
            if (game->decision_count() != shown) {
                shown = game->decision_count();
                logf("decision %d: %s (response + presentation %.0f ms)", shown, game->decision()["decision"].str().c_str(),
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
        /* Where a frame's time goes: process + layout, building the draw
         * calls, and Present (which waits for vsync and the GPU). */
        double f0 = now_ms();
        tree->process(dt > 0.1 ? 0.1 : dt);
        audio.update(dt > 0.1 ? 0.1 : dt);
        tree->layout(1);
        double f1 = now_ms();
        renderer.begin(1280, 720, Color(0.0923724f, 0.122398f, 0.116929f, 1.0f));
        tree->render(canvas);
        renderer.end();
        double f2 = now_ms();
        dev->Present(NULL, NULL, NULL, NULL);
        double f3 = now_ms();
        acc_ms += f1 - f0;
        acc_draw += f2 - f1;
        acc_present += f3 - f2;
        if (++frames == 600) {
            logf("per frame: %.2f ms process + layout, %.2f ms draw, %.2f ms present; %ld draw calls", acc_ms / frames,
                 acc_draw / frames, acc_present / frames, renderer.draw_calls());
            frames = 0;
            acc_ms = acc_draw = acc_present = 0;
        }
    }
    delete flow;
    delete game;
    delete tree;
    delete fonts;
    delete lib;
    logf("spire360 done");
    flush_log();
    if (g_log_file) { fclose(g_log_file); g_log_file = NULL; }
}
