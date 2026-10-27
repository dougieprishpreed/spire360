// spire360.xex: the game. The translated game logic runs in this process
// on its own hardware thread; the engine shows its decisions as the game's
// screens and the controller plays.
//
//   game:\spire\pack.s3p   the converter's build output (convert.py build)
//   game:\spire\data\      its data folder (localization, res_dirs.json)
//   game:\spire360.txt     log: startup, logic errors, frame timings
//   game:\spire360.ini     optional debug options (read_ini)
//
// Pad: on the title screen A starts a run. In a run: D-pad picks, A
// confirms, Y ends the turn. Back writes the log now; Back + Start quits.
// Debugging: Back + X cycles the canvas group modes (D3D9Renderer::
// set_group_debug), Back + Y cycles the display ramp's presets.
#include <xtl.h>

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../engine/core/pack.h"
#include "../../engine/audio/game_audio.h"
#include "../../engine/game/game.h"
#include "../../engine/game/game_client.h"
#include "../../engine/game/menu_flow.h"
#include "../../engine/game/loc.h"
#include "../../engine/game/logic_link.h"
#include "../../engine/gfx/d3d9_renderer.h"
#include "../../engine/scene/canvas.h"
#include "../../engine/scene/particles.h"
#include "../../engine/scene/scene.h"
#include "../../engine/scene/tree.h"
#include "../../engine/text/font.h"

extern "C" int spire_logic_start(const char* root, const char* const* extra, int extra_count);
extern "C" unsigned spire_logic_memory_kb(void);

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

static std::string ini_word(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n"), z = s.find_last_not_of(" \t\r\n");
    std::string w = a == std::string::npos ? std::string() : s.substr(a, z - a + 1);
    for (size_t i = 0; i < w.size(); ++i)
        if (w[i] >= 'A' && w[i] <= 'Z') w[i] = (char)(w[i] - 'A' + 'a');
    return w;
}

/* game:\spire360.ini: debug options for testing, "name=value" lines (';' or
 * '#' starts a comment; 1, on, true or yes turns an option on):
 *   invincible=1   the player keeps their HP; hits play as usual (numbers,
 *                  hurt animations)
 *   godmode=1      the game's dev console godmode: Strength, Buffer and
 *                  Regen 999999999 on the player at each fight
 *   trace_loads=1  every scene file, texture and audio bank read from the
 *                  pack logged with the ms it took, and frames over 8 ms
 *                  (what the console's first uses and the warm-up cost)
 *   profile=1      every 10 s, the node types (and scripts) whose process
 *                  and draw cost the most per frame, the time in the draw
 *                  calls, and the GPU's time for a frame with and without
 *                  each kind of draw (gpu_probe)
 *   gamma=N        the display ramp's preset to start with (GAMMA_PRESETS;
 *                  Back + Y cycles them)
 * The first two reach the logic as its environment (logic/Spire.Logic/
 * DebugOptions.cs). No file: all off. */
static bool g_trace_loads = false, g_profile = false;
static int g_gamma = 0;
static void read_ini(const char* path, std::vector<std::string>& env) {
    FILE* f = fopen(path, "rb");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        std::string s(line);
        size_t c = s.find_first_of(";#");
        if (c != std::string::npos) s.erase(c);
        size_t eq = s.find('=');
        if (eq == std::string::npos) continue;
        std::string key = ini_word(s.substr(0, eq)), val = ini_word(s.substr(eq + 1));
        bool on = val == "1" || val == "on" || val == "true" || val == "yes";
        if (key == "invincible") {
            if (on) env.push_back("SPIRE_INVINCIBLE=1");
        } else if (key == "godmode") {
            if (on) env.push_back("SPIRE_GODMODE=1");
        } else if (key == "trace_loads") {
            g_trace_loads = on;
        } else if (key == "profile") {
            g_profile = on;
        } else if (key == "gamma") {
            g_gamma = atoi(val.c_str());
            logf("spire360.ini: gamma %d", g_gamma);
            continue;
        } else {
            logf("spire360.ini: unknown option '%s'", key.c_str());
            continue;
        }
        logf("spire360.ini: %s %s", key.c_str(), on ? "on" : "off");
    }
    fclose(f);
}

/* Every allocation on the console is backed by physical memory (no paging):
 * what is left of it, and the logic's linear memory (committed as it grows,
 * never given back). */
static unsigned free_memory_mb() {
    MEMORYSTATUS ms;
    GlobalMemoryStatus(&ms);
    return (unsigned)(ms.dwAvailPhys >> 20);
}

static void log_memory(const char* when) {
    MEMORYSTATUS ms;
    GlobalMemoryStatus(&ms);
    logf("memory %s: %u MB free of %u MB, logic %u MB", when, (unsigned)(ms.dwAvailPhys >> 20),
         (unsigned)(ms.dwTotalPhys >> 20), spire_logic_memory_kb() / 1024);
}

static double now_ms() {
    LARGE_INTEGER f, c;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart * 1000.0 / (double)f.QuadPart;
}

/* A frame that stops (an endless loop, a lock, the GPU): every 10 s while
 * it lasts, the log says where the main thread is (the frame loop's phase,
 * the client's step, the node being processed or drawn) and the audio
 * thread. */
static volatile unsigned g_frame = 0;
static volatile bool g_quitting = false;

static void watchdog_main(void*) {
    unsigned seen = g_frame;
    double since = now_ms(), reported = 0;
    while (!g_quitting) {
        sleep_ms(250);
        unsigned f = g_frame;
        if (f != seen) {
            seen = f;
            since = now_ms();
            reported = 0;
            continue;
        }
        double stuck = now_ms() - since;
        if (stuck < 3000 || (reported > 0 && now_ms() - reported < 10000)) continue;
        reported = now_ms();
        const Node* n = (const Node*)g_trace_node;
        logf("watchdog: frame %u stopped %.0f s ago: %s, %s; node %s (%s %s); audio thread %s", f, stuck / 1000,
             g_trace_phase, g_trace_step, n ? n->name.c_str() : "-", n ? n->type.c_str() : "", n ? n->script.c_str() : "",
             g_trace_audio);
    }
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

/* profile=1: the frame drawn again with nothing left out, then without
 * each kind of draw, each from an idle GPU to an idle GPU and not shown:
 * what each kind costs the GPU (a frame whose draw calls wait on a full
 * command buffer is the GPU's, not the CPU's). */
static void gpu_probe(IDirect3DDevice9* dev, D3D9Renderer& renderer, SceneTree* tree, Canvas& canvas, const Color& clear) {
    static const struct {
        int mask;
        const char* name;
    } kinds[] = { { 0, NULL },
                  { SceneTree::SKIP_TEXT, "text" },
                  { SceneTree::SKIP_PARTICLES, "particles" },
                  { SceneTree::SKIP_SPINE, "spine" },
                  { SceneTree::SKIP_IMAGES, "images" },
                  { SceneTree::SKIP_SHADERS, "shader materials" },
                  { SceneTree::SKIP_BACKGROUND, "the fight's background" } };
    std::string line = "gpu:";
    for (size_t i = 0; i < sizeof(kinds) / sizeof(kinds[0]); ++i) {
        dev->BlockUntilIdle();
        double t0 = now_ms();
        SceneTree::set_draw_skip(kinds[i].mask);
        renderer.begin(1280, 720, clear);
        tree->render(canvas);
        renderer.end();
        double t1 = now_ms();
        dev->BlockUntilIdle();
        double t2 = now_ms();
        SceneTree::set_draw_skip(-1);
        char part[160];
        if (!kinds[i].name)
            _snprintf(part, sizeof(part), " a frame %.1f ms (its draw calls made in %.1f)", t2 - t0, t1 - t0);
        else
            _snprintf(part, sizeof(part), "%s without %s %.1f", i == 1 ? ";" : ",", kinds[i].name, t2 - t0);
        part[sizeof(part) - 1] = 0;
        line += part;
    }
    logf("%s", line.c_str());
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
                { XINPUT_GAMEPAD_START, PadState::START },  { XINPUT_GAMEPAD_BACK, PadState::BACK },
                { XINPUT_GAMEPAD_LEFT_THUMB, PadState::LS } };
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

/* The display ramp: out = in^exponent, squeezed into 16-235 for a display
 * that expands that range itself. The frame as drawn is right (Back + RB's
 * screenshots match the PC), yet on the user's display the dark fight
 * backgrounds come out far darker than on PC: the presets to find the
 * output that matches. 0 is the earlier one (the Xenia screenshots measured
 * about 2.5 / 2.2 in gamma against the game's colours). */
static const struct {
    double exponent;
    bool limited;
    const char* name;
} GAMMA_PRESETS[] = { { 2.2 / 2.5, false, "0 (in^0.88)" },  { 1.0, false, "1 (none)" },
                      { 0.75, false, "2 (in^0.75)" },     { 0.62, false, "3 (in^0.62)" },
                      { 2.2 / 2.5, true, "4 (in^0.88 in 16-235)" } };
static const int GAMMA_COUNT = (int)(sizeof(GAMMA_PRESETS) / sizeof(GAMMA_PRESETS[0]));

static void set_gamma(IDirect3DDevice9* dev, int preset) {
    if (preset < 0 || preset >= GAMMA_COUNT) preset = 0;
    D3DGAMMARAMP ramp;
    for (int i = 0; i < 256; ++i) {
        double v = pow(i / 255.0, GAMMA_PRESETS[preset].exponent);
        if (GAMMA_PRESETS[preset].limited) v = (16.0 + v * 219.0) / 255.0;
        WORD w = (WORD)(v * 65535.0 + 0.5);
        ramp.red[i] = ramp.green[i] = ramp.blue[i] = w;
    }
    dev->SetGammaRamp(0, 0, &ramp);
}

void __cdecl main() {
    set_log_sink(engine_log);
    logf("spire360 starting");
    double started = now_ms();
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
    set_gamma(dev, 0);
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
    logf("pack: %u entries, game version %s (%.0f ms after start)", (unsigned)pack.count(), pack.game_version().c_str(),
         now_ms() - started);
    log_memory("at start");
    std::vector<std::string> debug_env;
    read_ini("game:\\spire360.ini", debug_env);
    if (g_gamma < 0 || g_gamma >= GAMMA_COUNT) g_gamma = 0;
    set_gamma(dev, g_gamma);
    logf("gamma ramp %s (Back + Y: the next)", GAMMA_PRESETS[g_gamma].name);
    std::vector<const char*> debug_ptrs;
    for (size_t i = 0; i < debug_env.size(); ++i) debug_ptrs.push_back(debug_env[i].c_str());
    double t0 = now_ms();
    int rc = spire_logic_start("game:/spire", debug_ptrs.empty() ? NULL : &debug_ptrs[0], (int)debug_ptrs.size());
    if (rc != 0) {
        logf("logic: could not start (%d)", rc);
        flush_log();
        return;
    }
    logf("logic: started in %.1f ms", now_ms() - t0);
    log_memory("with the logic");
    if (g_trace_loads) set_load_trace(now_ms);
    if (g_profile) {
        SceneTree::set_profile(now_ms);
        bool fast = false;
        double us = Particles2D::turbulence_cost_us(now_ms, &fast);
        logf("particles: turbulence %.1f us per particle step (floor %s float-to-int conversions)", us,
             fast ? "without" : "with");
    }
    double d0 = now_ms();
    Loc::init("game:\\spire\\data");
    install_game_scripts();
    SceneLibrary* lib = new SceneLibrary(&pack);
    /* the warm-up's reads: 4 ms of each frame */
    lib->set_warm_clock(now_ms, 4.0);
    GameAudio audio(&pack);
    lib->set_audio(&audio);
    if (!audio.open_output()) logf("audio: XAudio2 output unavailable");
    /* The mix on hardware thread 4 (core 2): the frame loop keeps core 0,
     * the logic core 1 */
    else if (!audio.start_thread(4)) logf("audio: no audio thread (mixed in the frame loop)");
    Thread watchdog;
    watchdog.start(watchdog_main, NULL, 5);
    /* the menu music starts with the main menu, after the logo (MenuFlow) */
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
    logf("start-up: data, fonts and the logo in %.0f ms; the first frame %.0f ms after start", now_ms() - d0,
         now_ms() - started);
    unsigned prev = 0;
    double last = now_ms(), acc_ms = 0, acc_draw = 0, acc_present = 0, acc_update = 0, worst = 0, wait_start = 0;
    double acc_process = 0, acc_layout = 0;
    int frames = 0, shown = 0, slow = 0;
    unsigned least_free = free_memory_mb();
    bool low_logged = false, logic_ended = false;
    for (;;) {
        /* Any controller plays: on a console the pad can be signed in to
         * any of the four slots. */
        unsigned held = 0;
        for (DWORD user = 0; user < XUSER_MAX_COUNT; ++user) {
            XINPUT_STATE xs;
            ZeroMemory(&xs, sizeof(xs));
            if (XInputGetState(user, &xs) == ERROR_SUCCESS) held |= pad_bits(xs.Gamepad);
        }
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
            g_gamma = (g_gamma + 1) % GAMMA_COUNT;
            set_gamma(dev, g_gamma);
            logf("gamma ramp %s", GAMMA_PRESETS[g_gamma].name);
            pad.pressed &= ~PadState::Y;
        }
        /* Back + RB: the next frame's draws into the log, and a screenshot */
        bool dumped = false;
        if ((held & PadState::BACK) && (pad.pressed & PadState::RB)) {
            canvas.dump_next_frame();
            dumped = true;
            pad.pressed &= ~PadState::RB;
        }
        double u0 = now_ms();
        ++g_frame;
        g_trace_phase = "update";
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
            if (link.finished() && !logic_ended) {
                /* a trap or an exit with no error answer (out of memory, ...) */
                log_memory("as the logic ended");
                logf("logic: its thread has ended (its output: spire360_logic.txt)");
                logic_ended = true;
            }
        }

        double t = now_ms();
        double update_ms = t - u0;
        double dt = (t - last) / 1000.0;
        last = t;
        /* Where a frame's time goes: process + layout, building the draw
         * calls, and Present (which waits for vsync and the GPU). */
        double f0 = now_ms();
        g_trace_phase = "process";
        tree->process(dt > 0.1 ? 0.1 : dt);
        double fp = now_ms();
        g_trace_phase = "audio";
        audio.update(dt > 0.1 ? 0.1 : dt);
        double fa = now_ms();
        g_trace_phase = "layout";
        tree->layout(1);
        double f1 = now_ms();
        acc_process += fp - f0;
        acc_layout += f1 - fa;
        g_trace_phase = "draw";
        renderer.begin(1280, 720, Color(0.0923724f, 0.122398f, 0.116929f, 1.0f));
        tree->render(canvas);
        g_trace_phase = "draw end";
        renderer.end();
        double f2 = now_ms();
        if (dumped) {
            /* and the frame itself, as drawn (the gamma ramp is the display's) */
            static int shots = 0;
            char path[64];
            _snprintf(path, sizeof(path), "game:\\spire360_shot_%02d.bmp", ++shots);
            path[sizeof(path) - 1] = 0;
            logf("screenshot %s: %s (gamma ramp %s)", path, renderer.save_screenshot(path) ? "saved" : "failed",
                 GAMMA_PRESETS[g_gamma].name);
        }
        g_trace_phase = "present";
        dev->Present(NULL, NULL, NULL, NULL);
        double f3 = now_ms();
        if (dumped) flush_log();
        acc_ms += f1 - f0;
        acc_draw += f2 - f1;
        acc_present += f3 - f2;
        acc_update += update_ms;
        /* the frame's work before Present (a frame over 16.7 ms misses a
         * vertical blank) */
        double work = update_ms + (f2 - f0);
        if (work > worst) worst = work;
        if (g_trace_loads && work > 8)
            logf("frame %u: %.1f ms (update %.1f, process + layout %.1f, draw %.1f)", g_frame, work, update_ms, f1 - f0,
                 f2 - f1);
        /* a frame long enough to see: where its time went */
        if (work > 250)
            logf("long frame %u: %.0f ms (update %.0f, process + layout %.0f, draw %.0f), decision %d", g_frame, work,
                 update_ms, f1 - f0, f2 - f1, game->decision_count());
        if (work > 16.7) ++slow;
        if (++frames == 600) {
            unsigned free_mb = free_memory_mb();
            if (free_mb < least_free) least_free = free_mb;
            logf("per frame: %.2f ms update, %.2f ms process + layout, %.2f ms draw, %.2f ms present; %ld draw calls; "
                 "worst %.1f ms, %d of %d over 16.7 ms; memory %u MB free, logic %u MB",
                 acc_update / frames, acc_ms / frames, acc_draw / frames, acc_present / frames, renderer.draw_calls(), worst,
                 slow, frames, free_mb, spire_logic_memory_kb() / 1024);
            Canvas::TextureStats ts = canvas.take_texture_stats();
            logf("textures: %u held, %.1f MB (peak %.1f MB of 80), %u created, %u evicted, "
                 "%u frames over the budget (%u by their own)",
                 (unsigned)canvas.texture_count(), canvas.texture_bytes() / 1048576.0, ts.peak_bytes / 1048576.0,
                 ts.created, ts.evicted, ts.over_frames, ts.starved);
            logf("process + layout: %.2f ms process, %.2f ms layout", acc_process / frames, acc_layout / frames);
            double state_ms, submit_ms;
            renderer.take_draw_times(&state_ms, &submit_ms);
            if (g_profile)
                logf("draw calls: %.2f ms setting their state, %.2f ms in DrawPrimitiveUP per frame", state_ms / frames,
                     submit_ms / frames);
            SceneTree::log_profile(frames);
            if (g_profile) {
                gpu_probe(dev, renderer, tree, canvas, Color(0.0923724f, 0.122398f, 0.116929f, 1.0f));
                renderer.take_draw_times(&state_ms, &submit_ms);
            }
            if (free_mb < 24 && !low_logged) {
                log_memory("low");
                low_logged = true;
            }
            frames = slow = 0;
            acc_ms = acc_draw = acc_present = acc_update = worst = 0;
            acc_process = acc_layout = 0;
        }
    }
    g_quitting = true;
    watchdog.join();
    delete flow;
    delete game;
    delete tree;
    delete fonts;
    delete lib;
    logf("spire360 done (least free memory %u MB)", least_free);
    flush_log();
    if (g_log_file) { fclose(g_log_file); g_log_file = NULL; }
}
