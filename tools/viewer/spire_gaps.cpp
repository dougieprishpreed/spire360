/* spire_gaps: what the presentation cannot show yet, over protocol traces.
 *
 *   spire_gaps [--dt SECONDS] [--sample N] <pack.s3p> <report.json> <trace.jsonl> [more traces...]
 *
 * --dt: the playback step (default 1/30 s); a coarser one sweeps many
 * traces faster.
 *
 * Each trace's decisions go through the game client as the logic sent them
 * (the replies the client sends are dropped); each screen is laid out,
 * animated for its playback and drawn every few frames at a small size.
 * The report counts, over all traces, the items not drawn (shaders without
 * a port), the node types and Spine features without support, the warnings
 * (pack paths missing, audio events absent, logic errors) and the decision
 * kinds shown as the plain list, each with the first trace that had it. */
#include "../../engine/core/pack.h"
#include "../../engine/audio/game_audio.h"
#include "../../engine/game/game.h"
#include "../../engine/game/game_client.h"
#include "../../engine/game/loc.h"
#include "../../engine/game/logic_link.h"
#include "../../engine/gfx/sw_renderer.h"
#include "../../engine/scene/canvas.h"
#include "../../engine/scene/scene.h"
#include "../../engine/scene/tree.h"
#include "../../engine/text/font.h"

#include <deque>
#include <fstream>
#include <map>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using namespace spire;

class FeedLink : public LogicLink {
public:
    std::deque<std::string> incoming;
    virtual bool send(const std::string&) { return true; }
    virtual bool poll(std::string& s) {
        if (incoming.empty()) return false;
        s = incoming.front();
        incoming.pop_front();
        return true;
    }
    virtual bool finished() { return false; }
};

struct Gap {
    int count;
    std::string first; /* trace:decision */
    Gap() : count(0) {}
};

static std::map<std::string, Gap> g_gaps;
static std::string g_where;

static void add_gap(const std::string& key, int n) {
    Gap& g = g_gaps[key];
    if (g.count == 0) g.first = g_where;
    g.count += n;
}

/* Warnings while replaying: "missing in the pack", absent audio events,
 * scene problems; the logic's own error replies are counted separately. */
static void sink(const char* line) {
    std::string s = line;
    if (s.compare(0, 7, "[warn] ") == 0) s = s.substr(7);
    if (s.compare(0, 7, "logic: ") == 0) s = "logic error: " + s.substr(7);
    add_gap("warn: " + s, 1);
}

static std::string json_escape(const std::string& s) {
    std::string o;
    for (size_t i = 0; i < s.size(); ++i) {
        unsigned char c = (unsigned char)s[i];
        if (c == '"' || c == '\\') { o += '\\'; o += (char)c; }
        else if (c < 0x20) o += format("\\u%04x", c);
        else o += (char)c;
    }
    return o;
}

static double g_dt = 1.0 / 30.0;
static int g_sample = 1;

int main(int argc, char** argv) {
    while (argc > 2 && (strcmp(argv[1], "--dt") == 0 || strcmp(argv[1], "--sample") == 0)) {
        if (strcmp(argv[1], "--dt") == 0) {
            g_dt = atof(argv[2]);
            if (g_dt <= 0) g_dt = 1.0 / 30.0;
        } else {
            g_sample = atoi(argv[2]) > 0 ? atoi(argv[2]) : 1;
        }
        argv += 2;
        argc -= 2;
    }
    if (argc < 4) {
        fprintf(stderr, "usage: spire_gaps [--dt SECONDS] [--sample N] <pack.s3p> <report.json> <trace.jsonl> [more traces...]\n");
        return 2;
    }
    Pack pack;
    std::string error;
    if (!pack.open(argv[1], &error)) {
        fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    install_game_scripts();
    std::string pack_path = argv[1];
    size_t slash = pack_path.find_last_of("/\\");
    Loc::init(slash == std::string::npos ? "data" : pack_path.substr(0, slash) + "/data");
    SceneLibrary lib(&pack);
    GameAudio audio(&pack);
    if (audio.events().available()) lib.set_audio(&audio);
    FontLibrary fonts(&pack);
    FontLibrary::set_current(&fonts);
    SwRenderer sw;
    Canvas canvas(&sw, &pack);
    set_log_sink(sink);
    int decisions = 0;
    std::map<std::string, int> kinds;
    for (int file = 3; file < argc; ++file) {
        std::ifstream input(argv[file]);
        if (!input) {
            fprintf(stderr, "cannot open %s\n", argv[file]);
            continue;
        }
        SceneTree tree;
        tree.set_viewport(Vec2(1920, 1080), Xform2D::scaling(0.25f, 0.25f));
        FeedLink link;
        audio.events().stop_all();
        unsigned missing_before = audio.events().missing_events();
        GameClient game(&lib, &tree, &link);
        link.incoming.push_back("{\"type\":\"ready\"}");
        game.update(PadState());
        std::string line;
        int n = 0;
        while (std::getline(input, line)) {
            if (line.empty() || line[0] != '{') continue;
            if (line.find("\"type\":\"decision\"") == std::string::npos &&
                line.find("\"type\": \"decision\"") == std::string::npos)
                continue;
            ++n;
            ++decisions;
            g_where = format("%s:%d", argv[file], n);
            int seen;
            {
                std::string err;
                Json d = Json::parse(line.c_str(), line.size(), &err);
                seen = ++kinds[d["decision"].str()];
            }
            /* --sample N: a kind seen before plays out fully every Nth time
             * (the client still takes every decision, a few frames each). */
            bool full = g_sample <= 1 || seen % g_sample == 1;
            link.incoming.push_back(line);
            /* The screen and its playback (up to 20 s), drawn every 0.25 s. */
            lib.unsupported.clear();
            int frames = full ? (int)(20.0 / g_dt) : 3, every = (int)(0.25 / g_dt) > 0 ? (int)(0.25 / g_dt) : 1;
            for (int f = 0; f < frames; ++f) {
                game.update(PadState());
                tree.layout();
                tree.process(g_dt);
                audio.update(g_dt);
                tree.layout(1);
                if (f % every == 0) {
                    sw.begin(480, 270, Color(0, 0, 0, 1));
                    tree.render(canvas);
                    sw.end();
                    for (std::map<std::string, int>::iterator it = tree.skipped.begin(); it != tree.skipped.end(); ++it)
                        add_gap("not drawn: " + it->first, 1);
                }
                if (f * g_dt > 1.0 && !game.busy()) break;
            }
            for (std::map<std::string, int>::iterator it = lib.unsupported.begin(); it != lib.unsupported.end(); ++it)
                add_gap("unsupported: " + it->first, 1);
            if (game.showing_plain_list()) add_gap("plain list: " + game.decision()["decision"].str(), 1);
        }
        unsigned missing = audio.events().missing_events() - missing_before;
        if (missing) add_gap("audio events absent (count in this trace)", (int)missing);
        printf("%s: %d decisions\n", argv[file], n);
        fflush(stdout);
    }
    set_log_sink(NULL);
    FILE* out = fopen(argv[2], "wb");
    if (!out) {
        fprintf(stderr, "cannot write %s\n", argv[2]);
        return 1;
    }
    fprintf(out, "{\n \"decisions\": %d,\n \"kinds\": {", decisions);
    bool first = true;
    for (std::map<std::string, int>::iterator it = kinds.begin(); it != kinds.end(); ++it) {
        fprintf(out, "%s\"%s\": %d", first ? "" : ", ", json_escape(it->first).c_str(), it->second);
        first = false;
    }
    fprintf(out, "},\n \"gaps\": [\n");
    first = true;
    for (std::map<std::string, Gap>::iterator it = g_gaps.begin(); it != g_gaps.end(); ++it) {
        fprintf(out, "%s  {\"what\": \"%s\", \"count\": %d, \"first\": \"%s\"}", first ? "" : ",\n",
                json_escape(it->first).c_str(), it->second.count, json_escape(it->second.first).c_str());
        first = false;
    }
    fprintf(out, "\n ]\n}\n");
    fclose(out);
    printf("%d decisions, %d gaps -> %s\n", decisions, (int)g_gaps.size(), argv[2]);
    return 0;
}
