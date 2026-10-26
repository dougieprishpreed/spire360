/* spire_play: the whole game on PC, headless. Runs the logic as a child
 * process (any build that speaks the protocol), shows each decision as the
 * console would, plays by itself, and saves a PNG of every screen.
 *
 *   spire_play <pack.s3p> <out_dir> --logic "dotnet out/logic/jit/Spire.Logic.dll"
 *              [--character Ironclad] [--seed S] [--steps N] [--scale S] [--auto SEED]
 *              [--pad A,RIGHT,A,Y,WAIT,...] [--settle FRAMES]
 *
 * --pad presses those buttons one at a time while a decision is shown (the
 * screen half a second after each is saved, NNNN_pad_KK.png), then
 * auto-play takes over. A+RIGHT presses both; A+RIGHT*40 then holds them
 * for 40 more frames (map drawing: a stroke).
 *
 * --menu starts at the main menu of the logic's last profile instead
 * (MenuFlow, as the console): the --pad buttons drive the menus (title,
 * profiles, timeline, character select, the run's pause menu), one each
 * time the screen has been idle a second, saving menu_KK.png after each;
 * --menu-hold N waits N extra seconds before the first press. Once the
 * buttons are used up, a run that has started plays by itself (with
 * --seed S, the run's seed is S; --hp works as below); --pad-after N
 * --pad-run A,WAIT,... stops that at the run's screen N for those
 * presses, then it plays on (tests of what only the pad reaches). The intro
 * logo (unless the settings skip it) plays out before the first press;
 * --shots-at F1,F2,... saves those frames (60 a second) as frame_FFFF.png.
 *
 * --menu --monkey SEED: random pad input from the logo on (the menus, the
 * run's screens and overlays, held buttons now and then); each screen of a
 * run takes 0-59 presses (also while effects play, as a player mashes),
 * then auto play answers it (overlays closed first) so runs go on. Reports
 * the logic's errors, long frames, a client busy (waiting or playing) a
 * minute on one screen, a run screen or the menus with no progress for
 * 5 / 10 minutes (a PNG of each), memory every minute; runs
 * --monkey-minutes M of play (default 20; Quit starts the menus again).
 * The last presses are printed with each report and a crash.
 *
 * --sync: each command's answer is taken as it is sent (the frames no
 * longer depend on the logic's speed): with --seed and --monkey a session
 * repeats press for press.
 *
 * --replay-run <trace.jsonl>: a recorded run (random_runs.py, coverage)
 * played back in place of the logic, one recorded answer per command.
 *
 * --pad-after N: auto play up to screen N (the number printed for it),
 * then the --pad presses on it.
 *
 * --no-shots: the run without the PNGs (soaks).
 *
 * --mem-every N: the "memory:" line (resident, textures, scene files
 * cached) every N screens (default 50).
 *
 * --hp N: the player's max HP and HP set to N once the run starts (a soak
 * that reaches the end of the run).
 *
 * --fast: the settings' Fast mode on before the run starts (set_setting:
 * the logic saves it in its data folder's profile prefs). Not with
 * --replay-run (a command takes a recorded answer): replay a trace that
 * random_runs.py --fast recorded instead; its settings answer turns it on.
 *
 * --settle: frames each new screen runs before its shot (default 60: a
 * second; fewer catch the effects the decision's cues start).
 *
 * out_dir gets NNNN_<decision>.png and NNNN_<decision>.json (the decision's
 * line) per screen, and logic.log (the logic's stderr). The logic's working
 * directory is ours: give it SPIRE_DATA in the environment for localization. */
#include "../../engine/core/pack.h"
#include "../../engine/audio/game_audio.h"
#include "../../engine/game/game.h"
#include "../../engine/game/loc.h"
#include "../../engine/game/game_client.h"
#include "../../engine/game/menu_flow.h"
#include "../../engine/game/logic_link.h"
#include "../../engine/gfx/sw_renderer.h"
#include "../../engine/scene/canvas.h"
#include "../../engine/scene/scene.h"
#include "../../engine/scene/tree.h"
#include "../../engine/text/font.h"

#include <deque>
#include <fstream>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ucontext.h>
#include <unistd.h>

using namespace spire;

/* The pipe link, keeping the last decision line (saved with its screen). */
class RecordingLink : public LogicLink {
public:
    explicit RecordingLink(PipeLink* inner) : echo(false), sync(false), trace(NULL), inner_(inner) {}
    virtual bool send(const std::string& line) {
        if (echo) printf("> %s\n", line.c_str());
        if (trace) {
            fprintf(trace, ">%s\n", line.c_str());
            fflush(trace);
        }
        bool ok = inner_->send(line);
        if (ok && sync) wait_answer();
        return ok;
    }
    /* --sync: the answer (one line per command) is taken at once, so a
     * session's frames do not depend on how fast the logic answers. */
    void wait_answer() {
        std::string got;
        for (long tries = 0; tries < 600000; ++tries) {
            if (inner_->poll(got)) {
                held_.push_back(got);
                if (!got.empty() && got[0] == '{') return;
                continue;
            }
            if (inner_->finished()) return;
            usleep(200);
        }
        fprintf(stderr, "spire_play: --sync: no answer in 2 minutes\n");
    }
    virtual bool poll(std::string& line) {
        if (!held_.empty()) {
            line = held_.front();
            held_.pop_front();
        } else if (!inner_->poll(line)) {
            return false;
        }
        if (trace && !line.empty() && line[0] == '{') {
            fprintf(trace, "%s\n", line.c_str());
            fflush(trace);
        }
        if (line.find("\"type\": \"decision\"") != std::string::npos ||
            line.find("\"type\":\"decision\"") != std::string::npos)
            last_decision = line;
        return true;
    }
    virtual bool finished() { return inner_->finished(); }
    std::string last_decision;
    bool echo;   /* --menu: the commands sent, on stdout */
    bool sync;   /* --sync: wait_answer after each command */
    FILE* trace; /* --menu: every line both ways (spire_check replays it) */

private:
    PipeLink* inner_;
    std::deque<std::string> held_;
};

/* --monkey's last presses, printed with a report or a crash. */
static std::deque<std::string> g_presses;

#ifdef SPIRE_DEBUG_HEAP
/* tools/viewer/debug_heap.cpp (build_debug.sh): writes into freed blocks */
void debug_heap_check(size_t recent);
#endif

/* --replay: the responses of a recorded menu trace, each group handed out
 * when the client sends the command it answered (a debugging aid: the run
 * repeats exactly, without the logic). */
class ReplayLink : public LogicLink {
public:
    bool load(const char* path) {
        std::ifstream in(path);
        if (!in) return false;
        std::string line;
        groups_.push_back(Group());
        while (std::getline(in, line)) {
            if (line.empty()) continue;
            if (line[0] == '>') {
                groups_.push_back(Group());
                groups_.back().sent = line.substr(1);
            } else {
                groups_.back().replies.push_back(line);
            }
        }
        next_ = 0;
        release();
        return true;
    }
    virtual bool send(const std::string& line) {
        if (next_ < groups_.size() && groups_[next_].sent != line)
            fprintf(stderr, "replay: sent %s, recorded %s\n", line.c_str(), groups_[next_].sent.c_str());
        printf("> %s\n", line.c_str());
        release();
        return true;
    }
    virtual bool poll(std::string& line) {
        if (incoming_.empty()) return false;
        line = incoming_.front();
        incoming_.pop_front();
        return true;
    }
    virtual bool finished() { return false; }
    bool done() const { return next_ >= groups_.size() && incoming_.empty(); }

private:
    struct Group {
        std::string sent;
        std::vector<std::string> replies;
    };
    void release() {
        if (next_ >= groups_.size()) return;
        for (size_t i = 0; i < groups_[next_].replies.size(); ++i) incoming_.push_back(groups_[next_].replies[i]);
        ++next_;
    }
    std::vector<Group> groups_;
    size_t next_;
    std::deque<std::string> incoming_;
};

/* A crash's code addresses (PC, then the frame-pointer chain) for
 * addr2line, with a build that keeps frame pointers. */
static void on_crash(int sig, siginfo_t* info, void* ctx) {
    ucontext_t* uc = (ucontext_t*)ctx;
    fprintf(stderr, "spire_play: signal %d at address %p\n", sig, info ? info->si_addr : NULL);
    for (size_t k = 0; k < g_presses.size(); ++k) fprintf(stderr, "  press %s\n", g_presses[k].c_str());
#if defined(__x86_64__)
    unsigned long long pc = (unsigned long long)uc->uc_mcontext.rip;
    unsigned long long* fp = (unsigned long long*)uc->uc_mcontext.rbp;
    fprintf(stderr, "  pc 0x%llx\n", pc);
    for (int i = 0; i < 24 && fp; ++i) {
        unsigned long long ret = fp[1];
        fprintf(stderr, "  ret 0x%llx\n", ret);
        unsigned long long* next = (unsigned long long*)fp[0];
        if (next <= fp) break;
        fp = next;
    }
#else
    (void)uc;
#endif
    fflush(stderr);
    _exit(139);
}

/* An entry of a --pad list held from the frame before (no press). */
static const unsigned PAD_HELD = 1u << 31;

static void apply_pad(unsigned entry, PadState& pad) {
    pad.held = entry & ~PAD_HELD;
    pad.pressed = (entry & PAD_HELD) ? 0 : pad.held;
}

/* A --pad list ("A,RIGHT,WAIT,...", "A+RIGHT*40": pressed together, then
 * held 40 frames). */
static void parse_pad(const std::string& list, std::vector<unsigned>& out) {
    static const struct {
        const char* name;
        unsigned bit;
    } names[] = { { "A", PadState::A },       { "B", PadState::B },         { "X", PadState::X },
                  { "Y", PadState::Y },       { "LEFT", PadState::LEFT },   { "RIGHT", PadState::RIGHT },
                  { "UP", PadState::UP },     { "DOWN", PadState::DOWN },   { "START", PadState::START },
                  { "LB", PadState::LB },     { "RB", PadState::RB },       { "LT", PadState::LT },
                  { "RT", PadState::RT },     { "RS_UP", PadState::RS_UP }, { "RS_DOWN", PadState::RS_DOWN },
                  { "RS_LEFT", PadState::RS_LEFT }, { "RS_RIGHT", PadState::RS_RIGHT }, { "LS", PadState::LS },
                  { "WAIT", 0 } /* a second with no press */ };
    size_t at = 0;
    while (at <= list.size()) {
        size_t comma = list.find(',', at);
        std::string tok = list.substr(at, comma == std::string::npos ? std::string::npos : comma - at);
        int hold = 0;
        size_t star = tok.find('*');
        if (star != std::string::npos) {
            hold = atoi(tok.c_str() + star + 1);
            tok = tok.substr(0, star);
        }
        unsigned bits = 0;
        bool known = false;
        size_t from = 0;
        while (from <= tok.size()) {
            size_t plus = tok.find('+', from);
            std::string part = tok.substr(from, plus == std::string::npos ? std::string::npos : plus - from);
            for (size_t k = 0; k < sizeof(names) / sizeof(names[0]); ++k)
                if (part == names[k].name) {
                    bits |= names[k].bit;
                    known = true;
                }
            if (plus == std::string::npos) break;
            from = plus + 1;
        }
        if (known) {
            out.push_back(bits);
            for (int h = 0; h < hold; ++h) out.push_back(bits | PAD_HELD);
        }
        if (comma == std::string::npos) break;
        at = comma + 1;
    }
}

/* Where a frame's time goes (the console's "per frame" log): the scene's
 * process, the audio, the layout; averages and the worst frame. */
struct FrameTimes {
    double sum[3], worst[3];
    long frames;
    FrameTimes() : frames(0) {
        for (int i = 0; i < 3; ++i) sum[i] = worst[i] = 0;
    }
};

static double now_ms() {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}

/* --monkey: a random button, weighted towards what the screens use most. */
static unsigned monkey_button(unsigned r) {
    static const struct {
        unsigned bit;
        unsigned weight;
    } table[] = { { PadState::A, 24 },      { PadState::B, 9 },        { PadState::X, 4 },
                  { PadState::Y, 7 },       { PadState::UP, 9 },       { PadState::DOWN, 9 },
                  { PadState::LEFT, 9 },    { PadState::RIGHT, 9 },    { PadState::LB, 3 },
                  { PadState::RB, 3 },      { PadState::LT, 2 },       { PadState::RT, 2 },
                  { PadState::START, 4 },   { PadState::RS_UP, 1 },    { PadState::RS_DOWN, 1 },
                  { PadState::RS_LEFT, 1 }, { PadState::RS_RIGHT, 1 }, { PadState::LS, 1 } };
    unsigned total = 0;
    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); ++i) total += table[i].weight;
    unsigned pick = r % total;
    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); ++i) {
        if (pick < table[i].weight) return table[i].bit;
        pick -= table[i].weight;
    }
    return PadState::A;
}

static const char* button_name(unsigned bits) {
    static const struct {
        unsigned bit;
        const char* name;
    } names[] = { { PadState::A, "A" },         { PadState::B, "B" },           { PadState::X, "X" },
                  { PadState::Y, "Y" },         { PadState::UP, "UP" },         { PadState::DOWN, "DOWN" },
                  { PadState::LEFT, "LEFT" },   { PadState::RIGHT, "RIGHT" },   { PadState::LB, "LB" },
                  { PadState::RB, "RB" },       { PadState::LT, "LT" },         { PadState::RT, "RT" },
                  { PadState::START, "START" }, { PadState::RS_UP, "RS_UP" },   { PadState::RS_DOWN, "RS_DOWN" },
                  { PadState::RS_LEFT, "RS_LEFT" }, { PadState::RS_RIGHT, "RS_RIGHT" }, { PadState::LS, "LS" } };
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
        if (bits == names[i].bit) return names[i].name;
    return "?";
}

/* The process's resident memory in MB (/proc/self/statm), 0 if unknown. */
static long resident_mb() {
    FILE* f = fopen("/proc/self/statm", "r");
    if (!f) return 0;
    long size = 0, resident = 0;
    if (fscanf(f, "%ld %ld", &size, &resident) != 2) resident = 0;
    fclose(f);
    return resident * sysconf(_SC_PAGESIZE) / (1024 * 1024);
}

static void run_frame(SceneTree& tree, GameAudio& audio, FrameTimes& ft) {
    double t0 = now_ms();
    tree.process(1.0 / 60.0);
    double t1 = now_ms();
    audio.update(1.0 / 60.0);
    double t2 = now_ms();
    tree.layout(1);
    double t3 = now_ms();
    double part[3] = { t1 - t0, t2 - t1, t3 - t2 };
    for (int i = 0; i < 3; ++i) {
        ft.sum[i] += part[i];
        if (part[i] > ft.worst[i]) ft.worst[i] = part[i];
    }
    ++ft.frames;
}

static bool save_text(const std::string& path, const std::string& text) {
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    fwrite(text.data(), 1, text.size(), f);
    fclose(f);
    return true;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: spire_play <pack.s3p> <out_dir> --logic CMD [--character C] [--seed S] [--steps N] "
                        "[--scale S] [--auto SEED]\n");
        return 2;
    }
    std::string logic_cmd, character = "Ironclad", seed = "spire360", audio_wav;
    bool audio_device = false, audio_thread = false, menu = false, seed_given = false;
    double menu_hold = 0;
    const char* replay = NULL;
    int steps = 40;
    int pad_after = 0; /* --pad-after N: auto play up to screen N (as printed), then the --pad script */
    int hp = 0;
    bool fast = false;
    bool no_shots = false; /* --no-shots: no PNGs (soaks keep the log only) */ /* --hp: the player's max HP and HP after the start (soaks reach the end) */
    int settle = 60; /* frames before a decision's shot */
    float scale = 2.0f / 3.0f;
    unsigned auto_seed = 1;
    std::vector<unsigned> pad_script, run_script; /* --pad-run: in a --menu run, from --pad-after on */
    /* --mash: auto play to the first fight, then these presses every
     * --mash-every frames whatever is playing (queued plays); a shot every
     * 10 frames */
    std::vector<unsigned> mash;
    int mash_every = 8;
    unsigned monkey = 0; /* --monkey SEED: random presses (--menu) */
    bool sync = false;   /* --sync: each answer taken with its command (repeatable sessions) */
    int mem_every = 50;  /* --mem-every N: the memory line every N screens */
    int monkey_minutes = 20;
    std::vector<int> shots_at;
    int frames_after = -1, frames_every = 10; /* screen after which every so many frames are saved */
    for (int i = 3; i < argc; ++i) {
        if (strcmp(argv[i], "--logic") == 0 && i + 1 < argc) logic_cmd = argv[++i];
        else if (strcmp(argv[i], "--audio") == 0) audio_device = true;
        else if (strcmp(argv[i], "--audio-thread") == 0) audio_thread = true; /* the console's audio thread, muted unless --audio */
        else if (strcmp(argv[i], "--menu") == 0) menu = true;
        else if (strcmp(argv[i], "--replay") == 0 && i + 1 < argc) {
            replay = argv[++i];
            menu = true;
        }
        else if (strcmp(argv[i], "--replay-run") == 0 && i + 1 < argc) replay = argv[++i]; /* a run trace */
        else if (strcmp(argv[i], "--menu-hold") == 0 && i + 1 < argc) menu_hold = atof(argv[++i]);
        else if (strcmp(argv[i], "--shots-at") == 0 && i + 1 < argc) {
            /* menu frames (60 a second) to save as frame_NNNN.png */
            for (const char* c = argv[++i]; *c;) {
                shots_at.push_back(atoi(c));
                while (*c && *c != ',') ++c;
                if (*c) ++c;
            }
        }
        else if (strcmp(argv[i], "--audio-wav") == 0 && i + 1 < argc) audio_wav = argv[++i];
        else if (strcmp(argv[i], "--character") == 0 && i + 1 < argc) character = argv[++i];
        else if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            seed = argv[++i];
            seed_given = true;
        }
        else if (strcmp(argv[i], "--steps") == 0 && i + 1 < argc) steps = atoi(argv[++i]);
        else if (strcmp(argv[i], "--hp") == 0 && i + 1 < argc) hp = atoi(argv[++i]);
        else if (strcmp(argv[i], "--no-shots") == 0) no_shots = true;
        else if (strcmp(argv[i], "--fast") == 0) fast = true;
        else if (strcmp(argv[i], "--pad-after") == 0 && i + 1 < argc) pad_after = atoi(argv[++i]);
        else if (strcmp(argv[i], "--frames-after") == 0 && i + 1 < argc) {
            /* N[:EVERY] */
            const char* a = argv[++i];
            frames_after = atoi(a);
            if (strchr(a, ':')) frames_every = atoi(strchr(a, ':') + 1);
            if (frames_every < 1) frames_every = 1;
        }
        else if (strcmp(argv[i], "--settle") == 0 && i + 1 < argc) settle = atoi(argv[++i]);
        else if (strcmp(argv[i], "--scale") == 0 && i + 1 < argc) scale = (float)atof(argv[++i]);
        else if (strcmp(argv[i], "--auto") == 0 && i + 1 < argc) auto_seed = (unsigned)strtoul(argv[++i], NULL, 10);
        else if (strcmp(argv[i], "--pad") == 0 && i + 1 < argc) parse_pad(argv[++i], pad_script);
        else if (strcmp(argv[i], "--pad-run") == 0 && i + 1 < argc) parse_pad(argv[++i], run_script);
        else if (strcmp(argv[i], "--mash") == 0 && i + 1 < argc) parse_pad(argv[++i], mash);
        else if (strcmp(argv[i], "--mash-every") == 0 && i + 1 < argc) mash_every = atoi(argv[++i]);
        else if (strcmp(argv[i], "--monkey") == 0 && i + 1 < argc) {
            monkey = (unsigned)strtoul(argv[++i], NULL, 10);
            if (!monkey) monkey = 1;
        }
        else if (strcmp(argv[i], "--monkey-minutes") == 0 && i + 1 < argc) monkey_minutes = atoi(argv[++i]);
        else if (strcmp(argv[i], "--sync") == 0) sync = true;
        else if (strcmp(argv[i], "--mem-every") == 0 && i + 1 < argc) mem_every = atoi(argv[++i]) > 0 ? atoi(argv[i]) : 50;
    }
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = on_crash;
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &sa, NULL);
    if (logic_cmd.empty() && !replay) {
        fprintf(stderr, "spire_play: --logic is required\n");
        return 2;
    }
    std::string out_dir = argv[2];
    install_game_scripts();
    Pack pack;
    std::string err;
    if (!pack.open(argv[1], &err)) {
        fprintf(stderr, "%s\n", err.c_str());
        return 1;
    }
    /* The localization tables: the converter writes data/ next to the pack. */
    std::string pack_dir = argv[1];
    size_t slash = pack_dir.find_last_of('/');
    Loc::init(slash == std::string::npos ? std::string("data") : pack_dir.substr(0, slash) + "/data");
    SceneLibrary lib(&pack);
    GameAudio audio(&pack);
    if (audio.events().available()) lib.set_audio(&audio);
    if ((audio_device || audio_thread) && !audio.open_output(!audio_device)) fprintf(stderr, "spire_play: audio output unavailable\n");
    else if (audio_thread && !audio.start_thread(-1)) fprintf(stderr, "spire_play: no audio thread\n");
    if (!audio_wav.empty() && !audio.record(audio_wav.c_str())) { fprintf(stderr, "spire_play: cannot record audio\n"); return 1; }
    FontLibrary fonts(&pack);
    FontLibrary::set_current(&fonts);
    PipeLink pipe;
    ReplayLink replayed;
    if (replay) {
        if (!replayed.load(replay)) {
            fprintf(stderr, "spire_play: cannot read %s\n", replay);
            return 1;
        }
    } else if (!pipe.start(logic_cmd, (out_dir + "/logic.log").c_str(), &err)) {
        fprintf(stderr, "%s\n", err.c_str());
        return 1;
    }
    RecordingLink link(&pipe);
    if (sync && !replay) {
        link.sync = true;
        link.wait_answer(); /* the logic's "ready" */
    }
    SceneTree tree;
    tree.set_viewport(Vec2(1920, 1080), Xform2D::scaling(scale, scale));
    GameClient game(&lib, &tree, replay ? (LogicLink*)&replayed : (LogicLink*)&link);
    SwRenderer sw;
    Canvas canvas(&sw, &pack);
    if (menu) {
        /* The menus, driven by the pad script; then the run plays itself. */
        MenuFlow* flow = new MenuFlow(&lib, &tree, &game, pack.game_version());
        if (seed_given) game.set_run_seed(seed);
        size_t run_at = 0;
        bool hp_set = false;
        if (!replay) {
            link.echo = !monkey;
            link.trace = fopen((out_dir + "/trace.jsonl").c_str(), "wb");
        }
        size_t at = 0;
        int idle = 0, quiet = 0, shots = 0, decisions = 0;
        double hold = menu_hold;
        bool autoplay = false;
        /* --monkey's state: its generator, the screen's press budget, the
         * last presses (printed with each report) */
        unsigned mrng = monkey * 2654435761u + 12345u;
        int m_seen = -1, m_budget = 0, m_presses = 0, m_wait = 60, m_hold = 0, m_still = 0, m_menu_still = 0,
            m_reports = 0;
        unsigned m_hold_bits = 0;
        bool m_was_in_run = false;
        double m_busy_since = 0;
        std::string m_error;
        int frame_limit = monkey ? 60 * 60 * monkey_minutes : 60 * 60 * 30;
        for (int frame = 0; frame < frame_limit; ++frame) {
            PadState pad;
            /* the intro logo plays out untouched */
            bool settled = !game.busy() && !flow->in_logo() && idle >= 60;
            if (hold > 0 && settled) {
                hold -= 1.0 / 60.0;
                settled = false;
            }
            if (hp > 0 && !hp_set && flow->in_run() && game.decision_count() > 0) {
                game.request(format("{\"cmd\":\"set_player\",\"max_hp\":%d,\"hp\":%d}", hp, hp));
                hp_set = true;
            }
            if (monkey) {
                if (flow->in_run() && !m_was_in_run && game.decision_count() > 0) {
                    /* auto play only once a screen's presses are used up */
                    game.set_auto_play(monkey);
                    game.set_auto_play_until(game.decision_count());
                    m_was_in_run = true;
                }
                if (!flow->in_run()) m_was_in_run = false;
                if (game.decision_count() != m_seen) {
                    m_seen = game.decision_count();
                    mrng = mrng * 1103515245u + 12345u;
                    m_budget = (int)((mrng >> 16) % 60);
                    m_presses = m_still = 0;
                    m_busy_since = 0; /* busy across decisions is the game going on */
                }
                if (m_hold > 0) {
                    /* a held button (long presses, the map's drawing) */
                    pad.held = m_hold_bits;
                    --m_hold;
                } else if (flow->in_run() && m_presses >= m_budget && !flow->starting() && !game.busy()) {
                    /* auto play answers the screen; a pad cannot pick under
                     * an overlay, so it is closed first */
                    if (game.overlay_open() || m_presses == m_budget) {
                        /* B once anyway: a card raised to aim, an open
                         * inventory go back first */
                        if (++m_wait >= 20) {
                            pad.pressed = pad.held = PadState::B;
                            m_wait = 0;
                            if (m_presses == m_budget) ++m_presses;
                        }
                    } else if (!game.auto_playing()) {
                        game.set_auto_play_until(game.decision_count() + 1);
                    }
                } else if (!flow->in_logo() && --m_wait <= 0) {
                    mrng = mrng * 1103515245u + 12345u;
                    unsigned bits = monkey_button(mrng >> 8);
                    mrng = mrng * 1103515245u + 12345u;
                    if ((mrng >> 16) % 20 == 0) {
                        m_hold_bits = bits;
                        m_hold = 30 + (int)((mrng >> 20) % 60);
                    }
                    pad.pressed = pad.held = bits;
                    mrng = mrng * 1103515245u + 12345u;
                    m_wait = 3 + (int)((mrng >> 16) % 18);
                    ++m_presses;
                    g_presses.push_back(format("frame %d, decision %d %s: %s%s", frame, game.decision_count(),
                                            flow->in_run() ? game.decision()["decision"].str().c_str() : "(menus)",
                                            button_name(bits), m_hold > 0 ? " (held)" : ""));
                    if (g_presses.size() > 30) g_presses.pop_front();
                }
            } else if (settled && at < pad_script.size()) {
                apply_pad(pad_script[at++], pad);
                idle = 0;
                quiet = 0; /* the last screen is shot once its press has settled */
            } else if (settled && autoplay && run_at < run_script.size() && game.decision_count() >= pad_after) {
                apply_pad(run_script[run_at++], pad);
                idle = 0;
                quiet = 0;
                if (run_at == run_script.size()) game.set_auto_play_until(0);
            } else if (at >= pad_script.size() && flow->in_run() && !autoplay) {
                game.set_auto_play(auto_seed);
                if (!run_script.empty()) game.set_auto_play_until(pad_after > 0 ? pad_after : 1);
                autoplay = true;
            } else if (at >= pad_script.size() && quiet >= 300) {
                /* The buttons are used up and no run is on: the last
                 * screen, then stop. */
                sw.begin((int)(1920 * scale + 0.5f), (int)(1080 * scale + 0.5f), Color(0.0923724f, 0.122398f, 0.116929f, 1.0f));
                tree.render(canvas);
                sw.end();
                write_png(format("%s/menu_final.png", out_dir.c_str()).c_str(), sw.framebuffer());
                break;
            }
            double frame_start = now_ms();
            if (!flow->update(pad)) {
                if (!monkey) break;
                /* Quit: the game again from its start (the logic stays) */
                printf("monkey: Quit chosen at frame %d: the menus again\n", frame);
                delete flow;
                flow = new MenuFlow(&lib, &tree, &game, pack.game_version());
                m_was_in_run = false;
            }
            std::string ev = flow->take_event();
            if (!ev.empty()) printf("menu: %s\n", ev.c_str());
            tree.layout();
            tree.process(1.0 / 60.0);
            audio.update(1.0 / 60.0);
            tree.layout(1);
#ifdef SPIRE_DEBUG_HEAP
            debug_heap_check(4096);
#endif
            if (!flow->in_logo()) ++idle;
            for (size_t s = 0; s < shots_at.size(); ++s)
                if (shots_at[s] == frame) {
                    sw.begin((int)(1920 * scale + 0.5f), (int)(1080 * scale + 0.5f), Color(0, 0, 0, 1));
                    tree.render(canvas);
                    sw.end();
                    write_png(format("%s/frame_%04d.png", out_dir.c_str(), frame).c_str(), sw.framebuffer());
                }
            if (flow->in_run() || flow->starting() || game.busy() || flow->in_logo()) quiet = 0;
            else ++quiet;
            /* A shot of the first settled menu, then 45 frames after each
             * press (--pad-run's too). */
            int presses = (int)(at + run_at);
            bool shot = !monkey && (idle == 45 && shots < presses + 1 && (at > 0 || !game.busy()));
            if (shot) {
                sw.begin((int)(1920 * scale + 0.5f), (int)(1080 * scale + 0.5f), Color(0.0923724f, 0.122398f, 0.116929f, 1.0f));
                tree.render(canvas);
                sw.end();
                write_png(format("%s/menu_%02d.png", out_dir.c_str(), shots).c_str(), sw.framebuffer());
                printf("menu_%02d after %d presses%s\n", shots, presses, flow->in_run() ? " (in a run)" : "");
                fflush(stdout);
                ++shots;
            }
            /* each FTUE once it has come in (tests of the tips) */
            if (!game.ftue_open().empty() && game.ftue_frames() == 40) {
                sw.begin((int)(1920 * scale + 0.5f), (int)(1080 * scale + 0.5f), Color(0, 0, 0, 1));
                tree.render(canvas);
                sw.end();
                write_png(format("%s/ftue_%s.png", out_dir.c_str(), game.ftue_open().c_str()).c_str(), sw.framebuffer());
            }
            if (game.decision_count() != decisions) {
                decisions = game.decision_count();
                printf("%04d %s%s\n", decisions, game.decision()["decision"].str().c_str(),
                       game.showing_plain_list() ? "  (plain list)" : "");
                fflush(stdout);
                if (decisions >= steps && !monkey) break;
            }
            if (monkey) {
                double frame_ms = now_ms() - frame_start;
                std::string why;
                if (game.last_error() != m_error) {
                    m_error = game.last_error();
                    if (!m_error.empty()) why = "logic error: " + m_error;
                }
                if (frame_ms > 250)
                    printf("monkey: long frame %d: %.0f ms, decision %d %s\n", frame, frame_ms, game.decision_count(),
                           game.decision()["decision"].str().c_str());
                if (!game.busy()) m_busy_since = 0;
                else if (m_busy_since == 0) m_busy_since = now_ms();
                else if (now_ms() - m_busy_since > 60000) {
                    why = "busy (waiting for the logic or playing) for 60 s";
                    m_busy_since = now_ms();
                }
                if (flow->in_run()) {
                    m_menu_still = 0;
                    if (++m_still == 60 * 300) why = "no new decision in 5 minutes";
                } else if (flow->starting() || !ev.empty()) {
                    m_menu_still = 0;
                } else if (++m_menu_still == 60 * 600) {
                    why = "the menus: nothing happened in 10 minutes";
                }
                if (frame % 3600 == 0) {
                    std::vector<std::string> scenes;
                    lib.loaded_scenes(scenes);
                    printf("monkey: minute %d, %d decisions, resident %ld MB, %u scene files cached\n", frame / 3600,
                           game.decision_count(), resident_mb(), (unsigned)scenes.size());
                }
                if (!why.empty()) {
                    ++m_reports;
                    printf("monkey: REPORT %d at frame %d, decision %d %s: %s\n", m_reports, frame, game.decision_count(),
                           flow->in_run() ? game.decision()["decision"].str().c_str() : "(menus)", why.c_str());
                    for (size_t k = 0; k < g_presses.size(); ++k) printf("monkey:   %s\n", g_presses[k].c_str());
                    sw.begin((int)(1920 * scale + 0.5f), (int)(1080 * scale + 0.5f), Color(0.0923724f, 0.122398f, 0.116929f, 1.0f));
                    tree.render(canvas);
                    sw.end();
                    write_png(format("%s/monkey_report_%02d.png", out_dir.c_str(), m_reports).c_str(), sw.framebuffer());
                    fflush(stdout);
                    if (m_reports >= 40) break;
                }
            }
            if (!replay && link.finished()) {
                fprintf(stderr, "spire_play: the logic exited (see %s/logic.log)\n", out_dir.c_str());
                return 1;
            }
            if (!replay) usleep(500);
        }
        printf("%d menu shots, %d decisions\n", shots, decisions);
        if (monkey) printf("monkey: %d reports, resident %ld MB at the end\n", m_reports, resident_mb());
        return 0;
    }
    if (pad_script.empty() || pad_after > 0) game.set_auto_play(auto_seed);
    size_t pad_at = 0;
    FrameTimes frame_times;
    if (fast && !replay) game.request("{\"cmd\":\"set_setting\",\"name\":\"fast_mode\",\"value\":true}");
    game.start_run(character, seed);
    if (hp > 0) game.request(format("{\"cmd\":\"set_player\",\"max_hp\":%d,\"hp\":%d}", hp, hp));
    if (!mash.empty()) {
        game.set_auto_play(auto_seed);
        int f = 0;
        while (game.decision()["decision"].str() != "combat_play" && f < 60 * 900) {
            game.update(PadState());
            run_frame(tree, audio, frame_times);
            ++f;
            if (!replay) usleep(500);
            if (!replay && link.finished()) return 1;
        }
        game.set_auto_play_until(game.decision_count());
        printf("mash: the fight at frame %d, decision %d\n", f, game.decision_count());
        size_t at = 0;
        int last = game.decision_count();
        double press_sum = 0, press_worst = 0, idle_sum = 0, frame_worst = 0;
        int presses = 0, idles = 0;
        for (int k = 0; k < (int)mash.size() * mash_every + 600; ++k) {
            PadState pad;
            if (k % mash_every == 0 && at < mash.size()) pad.pressed = pad.held = mash[at++];
            double u0 = now_ms();
            game.update(pad);
            double u = now_ms() - u0;
            if (pad.pressed) {
                press_sum += u;
                if (u > press_worst) press_worst = u;
                ++presses;
            } else {
                idle_sum += u;
                ++idles;
            }
            double f0 = now_ms();
            run_frame(tree, audio, frame_times);
            double f = now_ms() - f0 + u;
            if (f > frame_worst) frame_worst = f;
            if (game.decision_count() != last) {
                last = game.decision_count();
                printf("mash: frame %d decision %d %s\n", k, last, game.decision()["decision"].str().c_str());
            }
            if (pad.pressed) printf("mash: frame %d press %u busy %d (update + frame %.3f ms)\n", k, pad.pressed, game.busy() ? 1 : 0, f);
            if (k % 10 == 0 && !no_shots) {
                sw.begin((int)(1920 * scale + 0.5f), (int)(1080 * scale + 0.5f), Color(0, 0, 0, 1));
                tree.render(canvas);
                sw.end();
                write_png(format("%s/mash_%04d.png", out_dir.c_str(), k).c_str(), sw.framebuffer());
            }
            if (!replay) usleep(500);
        }
        printf("mash: update %.3f ms per press (worst %.3f), %.3f ms otherwise; worst frame %.3f ms\n",
               presses ? press_sum / presses : 0.0, press_worst, idles ? idle_sum / idles : 0.0, frame_worst);
        fflush(stdout);
        return 0;
    }
    int shown = 0, idle = 0, frame_n = 0, waited = 0;
    while (shown < steps) {
        PadState pad;
        if (pad_at < pad_script.size() && !game.busy() &&
            (pad_after == 0 ? game.decision_count() > 0 : shown > pad_after)) {
            /* WAIT: a second with no press */
            if (pad_script[pad_at] != 0 || ++waited >= 60) {
                waited = 0;
                apply_pad(pad_script[pad_at++], pad);
            }
        } else if (pad_at >= pad_script.size() && !pad_script.empty()) {
            game.set_auto_play(auto_seed);
            game.set_auto_play_until(0);
            pad_script.clear();
        }
        int before = game.decision_count();
        game.update(pad);
        if (pad.pressed && game.decision_count() == before) {
            /* The screen half a second after this press (focus, targeting:
             * their moves and tweens). */
            tree.layout();
            for (int f = 0; f < 30; ++f) {
                run_frame(tree, audio, frame_times);
            }
            if (!no_shots) {
                sw.begin((int)(1920 * scale + 0.5f), (int)(1080 * scale + 0.5f), Color(0.0923724f, 0.122398f, 0.116929f, 1.0f));
                tree.render(canvas);
                sw.end();
                write_png(format("%s/%04d_pad_%02d.png", out_dir.c_str(), shown, (int)pad_at).c_str(), sw.framebuffer());
            }
        }
        if (game.decision_count() != before) {
            /* A new screen: let it settle (Godot's reference: 5th frame;
             * a second for the intro animations), then save it. */
            tree.layout();
            for (int f = 0; f < settle; ++f) {
                run_frame(tree, audio, frame_times);
                if (frames_after >= 0 && shown == frames_after + 1 && (++frame_n) % frames_every == 0 && frame_n <= 900) {
                    /* the steps after that screen as they play */
                    sw.begin((int)(1920 * scale + 0.5f), (int)(1080 * scale + 0.5f), Color(0.0923724f, 0.122398f, 0.116929f, 1.0f));
                    tree.render(canvas);
                    sw.end();
                    write_png(format("%s/%04d_f%03d.png", out_dir.c_str(), shown, frame_n).c_str(), sw.framebuffer());
                }
            }
            if (!no_shots) { /* --no-shots: no rendering (soaks, profiles) */
                sw.begin((int)(1920 * scale + 0.5f), (int)(1080 * scale + 0.5f), Color(0.0923724f, 0.122398f, 0.116929f, 1.0f));
                tree.render(canvas);
                sw.end();
            }
            const std::string& kind = game.decision()["decision"].str();
            std::string base = format("%s/%04d_%s", out_dir.c_str(), shown, kind.c_str());
            if (!no_shots && !write_png((base + ".png").c_str(), sw.framebuffer()))
                fprintf(stderr, "cannot write %s.png\n", base.c_str());
            if (!replay) save_text(base + ".json", link.last_decision + "\n");
            printf("%04d %s%s%s\n", shown, kind.c_str(), game.last_error().empty() ? "" : "  (logic error before it)",
                   game.showing_plain_list() ? "  (plain list)" : "");
            if (shown % mem_every == 0) {
                std::vector<std::string> scenes;
                lib.loaded_scenes(scenes);
                printf("memory: %ld MB resident, %lu MB of textures, %u scene files cached\n", resident_mb(),
                       (unsigned long)(canvas.texture_bytes() >> 20), (unsigned)scenes.size());
            }
            /* the screen before the pad's: auto play answers it, not the next */
            if (pad_after > 0 && shown == pad_after - 1 && !pad_script.empty())
                game.set_auto_play_until(game.decision_count() + 1);
            fflush(stdout);
            ++shown;
            idle = 0;
            if (game.game_over()) break;
        } else if (!replay && link.finished()) {
            fprintf(stderr, "spire_play: the logic exited (see %s/logic.log)\n", out_dir.c_str());
            return 1;
        } else {
            /* Time goes on while waiting (a fight's last blow plays before
             * the screen after it). */
            run_frame(tree, audio, frame_times);
            if (frames_after >= 0 && shown == frames_after + 1 && (++frame_n) % frames_every == 0 && frame_n <= 900) {
                /* the steps after that screen as they play */
                sw.begin((int)(1920 * scale + 0.5f), (int)(1080 * scale + 0.5f), Color(0.0923724f, 0.122398f, 0.116929f, 1.0f));
                tree.render(canvas);
                sw.end();
                write_png(format("%s/%04d_f%03d.png", out_dir.c_str(), shown, frame_n).c_str(), sw.framebuffer());
            }
            usleep(2000);
            if (++idle > 60000) { /* 2 minutes without an answer */
                fprintf(stderr, "spire_play: no answer from the logic\n");
                return 1;
            }
        }
    }
    printf("%d screens\n", shown);
    if (frame_times.frames > 0)
        printf("per frame (%ld): process %.3f ms (worst %.2f), audio %.3f ms (worst %.2f), layout %.3f ms (worst %.2f)\n",
               frame_times.frames, frame_times.sum[0] / frame_times.frames, frame_times.worst[0],
               frame_times.sum[1] / frame_times.frames, frame_times.worst[1], frame_times.sum[2] / frame_times.frames,
               frame_times.worst[2]);
    if (audio.events().available()) printf("audio: %.0f voices started, %u absent event calls, %u decoder failures\n",
        (double)audio.events().started_voices(), audio.events().missing_events(), audio.events().failures());
    return 0;
}
