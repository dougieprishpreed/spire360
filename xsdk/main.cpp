// spire360 on the open xSDK (xsdk and vulkan4xenon): the game. The
// translated game logic runs in this process on its own hardware thread;
// the engine shows its decisions as the game's screens and the controller
// plays. The XDK build's main (xdk/spire360/main.cpp) on xsdk's calls, the
// Vulkan renderer in D3D9's place.
//
//   game:\spire\pack.s3p   the converter's build output (convert.py build)
//   game:\spire\data\      its data folder (localization, res_dirs.json)
//   game:\spire360.txt     log: startup, logic errors, frame timings
//   game:\spire360.ini     optional debug options (read_ini)
//
// Pad: on the title screen A starts a run. In a run: D-pad picks, A
// confirms, Y ends the turn. Back writes the log now; Back + Start quits.
// Debugging: Back + X cycles the canvas groups' debug modes (off, whole
// passes, normal), Back + RB logs the next frame's draws and saves a
// screenshot.
#include <algorithm>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <xsdk/kernel.h>

#include "../engine/core/pack.h"
#include "../engine/audio/game_audio.h"
#include "../engine/game/game.h"
#include "../engine/game/game_client.h"
#include "../engine/game/menu_flow.h"
#include "../engine/game/loc.h"
#include "../engine/game/logic_link.h"
#include "../engine/gfx/vk_renderer.h"
#include "../engine/scene/canvas.h"
#include "../engine/scene/particles.h"
#include "../engine/scene/scene.h"
#include "../engine/scene/tree.h"
#include "../engine/text/font.h"

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
    vsnprintf(buf, sizeof(buf) - 2, fmt, ap);
    va_end(ap);
    buf[sizeof(buf) - 2] = 0;
    strcat(buf, "\n");
    if (!g_log_file) {
        g_log_file = fopen("game:/spire360.txt", g_log_started ? "ab" : "wb");
        if (g_log_file) g_log_started = true;
    }
    if (g_log_file) {
        fputs(buf, g_log_file);
        flush_log(); /* A hung frame must leave the last completed log line. */
        if (ferror(g_log_file)) {
            fclose(g_log_file);
            g_log_file = NULL;
        }
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
 *   profile=1      every 600 frames, the node types (and scripts) whose
 *                  process and draw cost the most per frame, the time in
 *                  the draw calls, the frames' GPU time; every third, the
 *                  GPU's time for a frame with and without each kind of
 *                  draw and draw by draw (gpu_probe); a frame slow on the
 *                  GPU traced draw by draw (every 20 s at most), and frames
 *                  slow on the CPU logged
 *   workers=N      worker threads for the particles' simulation (0-3, 3 by
 *                  default; 0 runs it on the frame loop's thread)
 *   fps=N          60 or 30: that frame rate always; 0 (the default) 60
 *                  while the frames fit, else a steady 30 (pace_frame)
 * The first two reach the logic as its environment (logic/Spire.Logic/
 * DebugOptions.cs). No file: all off. */
static bool g_trace_loads = false, g_profile = false;
static int g_workers = 3, g_fps = 0;
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
        } else if (key == "workers") {
            g_workers = atoi(val.c_str());
            logf("spire360.ini: workers %d", g_workers);
            continue;
        } else if (key == "fps") {
            g_fps = atoi(val.c_str());
            logf("spire360.ini: fps %d", g_fps);
            continue;
        } else {
            logf("spire360.ini: unknown option '%s'", key.c_str());
            continue;
        }
        logf("spire360.ini: %s %s", key.c_str(), on ? "on" : "off");
    }
    fclose(f);
}

/* Frame pacing: 60 Hz while the frames' work fits a vertical blank, else a
 * steady 30 (two blanks a frame) instead of a rate that jumps between 60,
 * 30 and 20, which looks worse than 30. To 30 when the work's running
 * average passes 16 ms; back to 60 after 2 s of frames under 13 ms (a
 * load's long frame does not hold it at 30). fps=60 / fps=30 in the ini
 * fix it. Returns the interval (1 or 2). */
static int pace_frame(VkRenderer* renderer, double work) {
    static int interval = 0, fit = 0;
    static double average = 0;
    int want = g_fps == 30 ? 2 : g_fps == 60 ? 1 : interval ? interval : 1;
    if (g_fps != 30 && g_fps != 60) {
        double w = work > 50 ? 50 : work;
        average = average * 0.9 + w * 0.1;
        fit = work < 13 ? fit + 1 : 0;
        if (interval == 1 && average > 16) want = 2;
        else if (interval == 2 && fit >= 120) want = 1;
    }
    if (want != interval) {
        renderer->set_present_interval(want);
        interval = want;
        fit = 0;
    }
    return interval;
}

/* Every allocation on the console is backed by physical memory (no paging):
 * what is left of it, and the logic's linear memory (committed as it grows,
 * never given back). The kernel counts 4 KB pages. */
static void memory_mb(unsigned* free_mb, unsigned* total_mb) {
    MM_QUERY_STATISTICS_RESULT st;
    memset(&st, 0, sizeof(st));
    st.size = sizeof(st);
    if (!NT_SUCCESS(MmQueryStatistics(&st))) {
        *free_mb = *total_mb = 0;
        return;
    }
    *free_mb = (unsigned)(((unsigned long long)st.title.available_pages * 4096) >> 20);
    *total_mb = (unsigned)(((unsigned long long)st.total_physical_pages * 4096) >> 20);
}

static unsigned free_memory_mb() {
    unsigned f, t;
    memory_mb(&f, &t);
    return f;
}

static void log_memory(const char* when) {
    unsigned f, t;
    memory_mb(&f, &t);
    logf("memory %s: %u MB free of %u MB, logic %u MB", when, f, t, spire_logic_memory_kb() / 1024);
}

static double now_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
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
 * what each kind costs the GPU. */
static void gpu_probe(VkRenderer* renderer, SceneTree* tree, Canvas& canvas, const Color& clear) {
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
        renderer->wait_idle();
        double t0 = now_ms();
        SceneTree::set_draw_skip(kinds[i].mask);
        renderer->begin(1280, 720, clear);
        tree->render(canvas);
        renderer->end();
        double t1 = now_ms();
        renderer->finish();
        double t2 = now_ms();
        SceneTree::set_draw_skip(-1);
        char part[160];
        if (!kinds[i].name)
            snprintf(part, sizeof(part), " a frame %.1f ms (its draw calls made in %.1f)", t2 - t0, t1 - t0);
        else
            snprintf(part, sizeof(part), "%s without %s %.1f", i == 1 ? ";" : ",", kinds[i].name, t2 - t0);
        line += part;
    }
    logf("%s", line.c_str());
    /* and the same frame draw by draw */
    renderer->wait_idle();
    renderer->trace_next_frame("the profile's frame");
    renderer->begin(1280, 720, clear);
    tree->render(canvas);
    renderer->end();
    renderer->finish();
}

/* A long frame's loads: how many, their time, and the slowest by kind
 * and name. */
static bool slower(const HeldLoad& a, const HeldLoad& b) { return a.ms > b.ms; }

static void log_loads(std::vector<HeldLoad>& loads) {
    double total = 0;
    for (size_t i = 0; i < loads.size(); ++i) total += loads[i].ms;
    std::sort(loads.begin(), loads.end(), slower);
    std::string line =
        format("  its loads: %u, %.0f ms (nested ones counted in their parents too);", (unsigned)loads.size(), total);
    for (size_t i = 0; i < loads.size() && i < 6; ++i)
        line += format(" %s %s %.0f ms%s", loads[i].kind.c_str(), loads[i].name.c_str(), loads[i].ms,
                       i + 1 < 6 && i + 1 < loads.size() ? "," : "");
    logf("%s", line.c_str());
}

/* The controller -> the engine's pad bits. Triggers count as pressed past
 * half. */
static unsigned pad_bits(const INPUT_GAMEPAD& g) {
    static const struct {
        unsigned x;
        unsigned bit;
    } map[] = { { INPUT_GAMEPAD_A, PadState::A },
                { INPUT_GAMEPAD_B, PadState::B },
                { INPUT_GAMEPAD_X, PadState::X },
                { INPUT_GAMEPAD_Y, PadState::Y },
                { INPUT_GAMEPAD_LEFT_SHOULDER, PadState::LB },
                { INPUT_GAMEPAD_RIGHT_SHOULDER, PadState::RB },
                { INPUT_GAMEPAD_DPAD_UP, PadState::UP },
                { INPUT_GAMEPAD_DPAD_DOWN, PadState::DOWN },
                { INPUT_GAMEPAD_DPAD_LEFT, PadState::LEFT },
                { INPUT_GAMEPAD_DPAD_RIGHT, PadState::RIGHT },
                { INPUT_GAMEPAD_START, PadState::START },
                { INPUT_GAMEPAD_BACK, PadState::BACK },
                { INPUT_GAMEPAD_LEFT_THUMB, PadState::LS } };
    unsigned b = 0;
    for (size_t i = 0; i < sizeof(map) / sizeof(map[0]); ++i)
        if (g.buttons & map[i].x) b |= map[i].bit;
    if (g.left_trigger > 128) b |= PadState::LT;
    if (g.right_trigger > 128) b |= PadState::RT;
    /* The left stick moves the focus like the D-pad. */
    if (g.thumb_lx < -16000) b |= PadState::LEFT;
    if (g.thumb_lx > 16000) b |= PadState::RIGHT;
    if (g.thumb_ly > 16000) b |= PadState::UP;
    if (g.thumb_ly < -16000) b |= PadState::DOWN;
    if (g.thumb_rx < -16000) b |= PadState::RS_LEFT;
    if (g.thumb_rx > 16000) b |= PadState::RS_RIGHT;
    if (g.thumb_ry > 16000) b |= PadState::RS_UP;
    if (g.thumb_ry < -16000) b |= PadState::RS_DOWN;
    return b;
}

/* The display ramp, the Vulkan renderer's last pass: out = in^(2.2 / 2.5),
 * the XDK build's D3D gamma ramp (the Xenia screenshots measured about
 * 2.5 / 2.2 in gamma against the game's colours). Hardware runs through
 * the other presets (Back + Y, since removed) found the Underdocks as dark
 * as the game draws it at this one. */
static const double DISPLAY_GAMMA = 2.2 / 2.5;

/* GPU traces of frames slow on the GPU: one every 20 s at most. Frames
 * slow on the CPU (over 40 ms of work, under the long frames' 250):
 * their phases, one a second at most. */
static const double TRACE_GPU_MS = 36, TRACE_EVERY_MS = 20000, SLOW_CPU_MS = 40;

int main() {
    set_log_sink(engine_log);
#if defined(SPIRE_GIT_COMMIT)
    logf("spire360 starting (xSDK build, source %s of %s)", SPIRE_GIT_COMMIT, SPIRE_GIT_DATE);
#else
    logf("spire360 starting (xSDK build)");
#endif
    double started = now_ms();
    VkRenderer* renderer = VkRenderer::create();
    std::string err;
    if (!renderer->init(1280, 720, true, &err)) {
        logf("renderer: %s", err.c_str());
        flush_log();
        return 1;
    }
    Pack pack;
    if (!pack.open("game:/spire/pack.s3p", &err)) {
        logf("pack: %s (copy the converter's output folder to game:\\spire\\)", err.c_str());
        flush_log();
        return 1;
    }
    logf("pack: %u entries, game version %s (%.0f ms after start)", (unsigned)pack.count(), pack.game_version().c_str(),
         now_ms() - started);
    log_memory("at start");
    std::vector<std::string> debug_env;
    read_ini("game:/spire360.ini", debug_env);
    renderer->set_gamma(DISPLAY_GAMMA, false);
    renderer->set_timing(g_profile);
    std::vector<const char*> debug_ptrs;
    for (size_t i = 0; i < debug_env.size(); ++i) debug_ptrs.push_back(debug_env[i].c_str());
    double t0 = now_ms();
    int rc = spire_logic_start("game:/spire", debug_ptrs.empty() ? NULL : &debug_ptrs[0], (int)debug_ptrs.size());
    if (rc != 0) {
        logf("logic: could not start (%d)", rc);
        flush_log();
        return 1;
    }
    logf("logic: started in %.1f ms", now_ms() - t0);
    log_memory("with the logic");
    /* trace_loads logs each load; profile keeps them for the long frames'
     * lines */
    if (g_trace_loads) set_load_trace(now_ms);
    else if (g_profile) set_load_trace(now_ms, true);
    if (g_profile) {
        SceneTree::set_profile(now_ms);
        bool fast = false;
        double us = Particles2D::turbulence_cost_us(now_ms, &fast);
        logf("particles: turbulence %.1f us per particle step (floor %s float-to-int conversions)", us,
             fast ? "without" : "with");
    }
    double d0 = now_ms();
    Loc::init("game:/spire/data");
    install_game_scripts();
    SceneLibrary* lib = new SceneLibrary(&pack);
    /* the warm-up's reads: 4 ms of each frame */
    lib->set_warm_clock(now_ms, 4.0);
    GameAudio audio(&pack);
    lib->set_audio(&audio);
    if (!audio.open_output()) logf("audio: xsdk's audio output unavailable");
    /* The mix on hardware thread 4 (core 2): the frame loop keeps core 0,
     * the logic core 1 */
    else if (!audio.start_thread(4)) logf("audio: no audio thread (mixed in the frame loop)");
    Thread watchdog;
    watchdog.start(watchdog_main, NULL, 5);
    /* The particles' simulation on the hardware threads the frame loop
     * (0), the logic (2) and the mix (4) leave: the other thread of each
     * core (the watchdog on 5 sleeps). */
    static Workers workers;
    static const int worker_hw[3] = { 1, 3, 5 };
    if (g_workers > 3) g_workers = 3;
    if (g_workers > 0 && workers.start(g_workers, worker_hw)) Particles2D::set_workers(&workers);
    logf("particles: %d worker threads", workers.count());
    /* the menu music starts with the main menu, after the logo (MenuFlow) */
    FontLibrary* fonts = new FontLibrary(&pack);
    FontLibrary::set_current(fonts);
    Canvas* canvas = new Canvas(renderer, &pack);
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
    const Color clear(0.0923724f, 0.122398f, 0.116929f, 1.0f);
    unsigned prev = 0;
    double last = now_ms(), acc_ms = 0, acc_draw = 0, acc_present = 0, acc_update = 0, worst = 0, wait_start = 0;
    double acc_process = 0, acc_layout = 0;
    int frames = 0, shown = 0, slow = 0, paced30 = 0;
    unsigned least_free = free_memory_mb();
    VkRenderer::LayerCounts was = renderer->layer_counts(); /* at the 600 frames' start */
    std::vector<HeldLoad> frame_loads;
    bool low_logged = false, logic_ended = false;
    for (;;) {
        /* Any controller plays: on a console the pad can be signed in to
         * any of the four slots. */
        unsigned held = 0;
        for (uint32_t user = 0; user < 4; ++user) {
            INPUT_STATE xs;
            memset(&xs, 0, sizeof(xs));
            if (XamInputGetState(user, 0, &xs) == 0) held |= pad_bits(xs.gamepad);
        }
        PadState pad;
        pad.held = held;
        pad.pressed = held & ~prev;
        prev = held;
        if ((held & PadState::BACK) && (held & PadState::START)) break;
        if (pad.pressed & PadState::BACK) flush_log();
        if ((held & PadState::BACK) && (pad.pressed & PadState::X)) {
            renderer->set_group_debug((renderer->group_debug() + 1) % VkRenderer::GROUP_DEBUG_MODES);
            pad.pressed &= ~PadState::X;
        }
        /* Back + RB: the next frame's draws into the log, and a screenshot */
        bool dumped = false;
        if ((held & PadState::BACK) && (pad.pressed & PadState::RB)) {
            canvas->dump_next_frame();
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
         * calls, and the present (which waits for a buffer to come off
         * screen, and so for vsync and the GPU). */
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
        renderer->begin(1280, 720, clear);
        tree->render(*canvas);
        g_trace_phase = "draw end";
        renderer->end();
        double f2 = now_ms();
        if (dumped) {
            /* and the frame itself, as drawn (before the display ramp) */
            static int shots = 0;
            char path[64];
            snprintf(path, sizeof(path), "game:/spire360_shot_%02d.bmp", ++shots);
            logf("screenshot %s: %s (canvas group mode %d)", path, renderer->save_screenshot(path) ? "saved" : "failed",
                 renderer->group_debug());
        }
        g_trace_phase = "present";
        /* the frame's work before the present (a frame over 16.7 ms misses
         * a vertical blank) */
        double work = update_ms + (f2 - f0);
        if (pace_frame(renderer, work) == 2) ++paced30;
        renderer->present();
        double f3 = now_ms();
        if (dumped) flush_log();
        acc_ms += f1 - f0;
        acc_draw += f2 - f1;
        acc_present += f3 - f2;
        acc_update += update_ms;
        if (work > worst) worst = work;
        if (g_trace_loads && work > 8)
            logf("frame %u: %.1f ms (update %.1f, process + layout %.1f, draw %.1f)", g_frame, work, update_ms, f1 - f0,
                 f2 - f1);
        /* a frame long enough to see: where its time went, and with
         * profile=1 what it loaded */
        if (g_profile && !g_trace_loads) take_held_loads(frame_loads);
        if (work > 250) {
            logf("long frame %u: %.0f ms (update %.0f, process + layout %.0f, draw %.0f), decision %d", g_frame, work,
                 update_ms, f1 - f0, f2 - f1, game->decision_count());
            if (!frame_loads.empty()) log_loads(frame_loads);
        }
        if (g_profile) {
            /* the frames an effect slows: the CPU's phases and the GPU's
             * time of the latest frame it finished; a GPU trace of one */
            static double slow_logged = -1e9, traced = -1e9;
            double now = now_ms();
            if (work > SLOW_CPU_MS && work <= 250 && now - slow_logged > 1000) {
                logf("slow frame %u: %.1f ms (update %.1f, process %.1f, layout %.1f, draw %.1f), %ld draw calls, "
                     "GPU %.1f ms, decision %d",
                     g_frame, work, update_ms, fp - f0, f1 - fa, f2 - f1, renderer->draw_calls(),
                     renderer->last_frame_gpu_ms(), game->decision_count());
                slow_logged = now;
            }
            if (renderer->last_frame_gpu_ms() > TRACE_GPU_MS && now - traced > TRACE_EVERY_MS) {
                char why[64];
                snprintf(why, sizeof(why), "after a frame of %.1f ms on the GPU, frame %u", renderer->last_frame_gpu_ms(),
                         g_frame + 1);
                renderer->trace_next_frame(why);
                traced = now;
            }
        }
        if (work > 16.7) ++slow;
        if (++frames == 600) {
            unsigned free_mb = free_memory_mb();
            if (free_mb < least_free) least_free = free_mb;
            logf("per frame: %.2f ms update, %.2f ms process + layout, %.2f ms draw, %.2f ms present; %ld draw calls; "
                 "worst %.1f ms, %d of %d over 16.7 ms, %d paced at 30 fps; memory %u MB free, logic %u MB",
                 acc_update / frames, acc_ms / frames, acc_draw / frames, acc_present / frames, renderer->draw_calls(),
                 worst, slow, frames, paced30, free_mb, spire_logic_memory_kb() / 1024);
            Canvas::TextureStats ts = canvas->take_texture_stats();
            logf("textures: %u held, %.1f MB (peak %.1f MB of 80), %u created, %u evicted, "
                 "%u frames over the budget (%u by their own)",
                 (unsigned)canvas->texture_count(), canvas->texture_bytes() / 1048576.0, ts.peak_bytes / 1048576.0,
                 ts.created, ts.evicted, ts.over_frames, ts.starved);
            logf("process + layout: %.2f ms process, %.2f ms layout", acc_process / frames, acc_layout / frames);
            double state_ms, submit_ms;
            renderer->take_draw_times(&state_ms, &submit_ms);
            if (g_profile) {
                logf("draw calls: %.2f ms setting their state, %.2f ms recording them per frame", state_ms / frames,
                     submit_ms / frames);
                VkRenderer::FrameStats fs;
                renderer->take_frame_stats(&fs);
                if (fs.frames > 0)
                    logf("gpu: %.1f ms a frame by its clock (worst %.1f); waits a frame: %.1f ms for the GPU, %.1f ms "
                         "for a display buffer, %.1f ms pacing",
                         fs.gpu_ms / fs.frames, fs.gpu_worst, fs.fence_ms / frames, fs.acquire_ms / frames,
                         fs.pace_ms / frames);
                logf("passes a frame: %.1f (groups %.1f, captures %.1f, screen copies %.2f and their blur levels "
                     "%.2f); %.2f texture uploads (%.2f MB), %.2f waits for the GPU mid-frame",
                     (double)fs.passes / frames, (double)fs.groups / frames, (double)fs.captures / frames,
                     (double)fs.screen_copies / frames, (double)fs.screen_levels / frames, (double)fs.uploads / frames,
                     fs.upload_mb / frames, (double)fs.waits / frames);
                VkRenderer::LayerCounts lc = renderer->layer_counts();
                logf("vulkan4xenon: %u shaders compiled on the console, %u read from its cache (since start); a "
                     "frame: %.2f images drawn back into eDRAM, %.2f clear resolves, %.2f resolves put off (%.2f "
                     "made later)",
                     lc.compiled, lc.read, (double)(lc.drawn_back - was.drawn_back) / frames,
                     (double)(lc.clear_resolves - was.clear_resolves) / frames,
                     (double)(lc.put_off - was.put_off) / frames, (double)(lc.late_resolves - was.late_resolves) / frames);
            }
            SceneTree::log_profile(frames);
            /* the probe re-draws the frame 8 times, a hitch: each third
             * 600 frames */
            static int blocks = 0;
            if (g_profile && blocks++ % 3 == 0) {
                gpu_probe(renderer, tree, *canvas, clear);
                /* the next 600 frames' counts without the probe's frames */
                renderer->take_draw_times(&state_ms, &submit_ms);
                VkRenderer::FrameStats probe;
                renderer->take_frame_stats(&probe);
            }
            was = renderer->layer_counts();
            if (free_mb < 24 && !low_logged) {
                log_memory("low");
                low_logged = true;
            }
            frames = slow = paced30 = 0;
            acc_ms = acc_draw = acc_present = acc_update = worst = 0;
            acc_process = acc_layout = 0;
        }
    }
    g_quitting = true;
    watchdog.join();
    delete flow;
    delete game;
    delete tree;
    delete canvas;
    delete fonts;
    delete lib;
    delete renderer;
    logf("spire360 done (least free memory %u MB)", least_free);
    flush_log();
    if (g_log_file) {
        fclose(g_log_file);
        g_log_file = NULL;
    }
    return 0;
}
