/* spire_play: the whole game on PC, headless. Runs the logic as a child
 * process (any build that speaks the protocol), shows each decision as the
 * console would, plays by itself, and saves a PNG of every screen.
 *
 *   spire_play <pack.s3p> <out_dir> --logic "dotnet out/logic/jit/Spire.Logic.dll"
 *              [--character Ironclad] [--seed S] [--steps N] [--scale S] [--auto SEED]
 *              [--pad A,RIGHT,A,Y,...]
 *
 * --pad presses those buttons one per frame while a decision is shown (a
 * frame is also saved after each, NNNN_pad_KK.png), then auto-play takes
 * over.
 *
 * out_dir gets NNNN_<decision>.png and NNNN_<decision>.json (the decision)
 * per screen, and logic.log (the logic's stderr). The logic's working
 * directory is ours: give it SPIRE_DATA in the environment for localization. */
#include "../../engine/core/pack.h"
#include "../../engine/game/game.h"
#include "../../engine/game/loc.h"
#include "../../engine/game/game_client.h"
#include "../../engine/game/logic_link.h"
#include "../../engine/gfx/sw_renderer.h"
#include "../../engine/scene/canvas.h"
#include "../../engine/scene/scene.h"
#include "../../engine/scene/tree.h"
#include "../../engine/text/font.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

using namespace spire;

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
    std::string logic_cmd, character = "Ironclad", seed = "spire360";
    int steps = 40;
    float scale = 2.0f / 3.0f;
    unsigned auto_seed = 1;
    std::vector<unsigned> pad_script;
    for (int i = 3; i < argc; ++i) {
        if (strcmp(argv[i], "--logic") == 0 && i + 1 < argc) logic_cmd = argv[++i];
        else if (strcmp(argv[i], "--character") == 0 && i + 1 < argc) character = argv[++i];
        else if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) seed = argv[++i];
        else if (strcmp(argv[i], "--steps") == 0 && i + 1 < argc) steps = atoi(argv[++i]);
        else if (strcmp(argv[i], "--scale") == 0 && i + 1 < argc) scale = (float)atof(argv[++i]);
        else if (strcmp(argv[i], "--auto") == 0 && i + 1 < argc) auto_seed = (unsigned)strtoul(argv[++i], NULL, 10);
        else if (strcmp(argv[i], "--pad") == 0 && i + 1 < argc) {
            static const struct {
                const char* name;
                unsigned bit;
            } names[] = { { "A", PadState::A },       { "B", PadState::B },         { "X", PadState::X },
                          { "Y", PadState::Y },       { "LEFT", PadState::LEFT },   { "RIGHT", PadState::RIGHT },
                          { "UP", PadState::UP },     { "DOWN", PadState::DOWN },   { "START", PadState::START },
                          { "LB", PadState::LB },     { "RB", PadState::RB },       { "LT", PadState::LT },
                          { "RT", PadState::RT } };
            std::string list = argv[++i];
            size_t at = 0;
            while (at <= list.size()) {
                size_t comma = list.find(',', at);
                std::string tok = list.substr(at, comma == std::string::npos ? std::string::npos : comma - at);
                for (size_t k = 0; k < sizeof(names) / sizeof(names[0]); ++k)
                    if (tok == names[k].name) pad_script.push_back(names[k].bit);
                if (comma == std::string::npos) break;
                at = comma + 1;
            }
        }
    }
    if (logic_cmd.empty()) {
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
    FontLibrary fonts(&pack);
    FontLibrary::set_current(&fonts);
    PipeLink link;
    if (!link.start(logic_cmd, (out_dir + "/logic.log").c_str(), &err)) {
        fprintf(stderr, "%s\n", err.c_str());
        return 1;
    }
    SceneTree tree;
    tree.set_viewport(Vec2(1920, 1080), Xform2D::scaling(scale, scale));
    GameClient game(&lib, &tree, &link);
    if (pad_script.empty()) game.set_auto_play(auto_seed);
    size_t pad_at = 0;
    game.start_run(character, seed);
    SwRenderer sw;
    Canvas canvas(&sw, &pack);
    int shown = 0, idle = 0;
    while (shown < steps) {
        PadState pad;
        if (pad_at < pad_script.size() && !game.busy() && game.decision_count() > 0) {
            pad.pressed = pad.held = pad_script[pad_at++];
        } else if (pad_at >= pad_script.size() && !pad_script.empty()) {
            game.set_auto_play(auto_seed);
            pad_script.clear();
        }
        int before = game.decision_count();
        game.update(pad);
        if (pad.pressed && game.decision_count() == before) {
            /* The screen after this press (focus, targeting). */
            tree.layout();
            sw.begin((int)(1920 * scale + 0.5f), (int)(1080 * scale + 0.5f), Color(0.0923724f, 0.122398f, 0.116929f, 1.0f));
            tree.render(canvas);
            sw.end();
            write_png(format("%s/%04d_pad_%02d.png", out_dir.c_str(), shown, (int)pad_at).c_str(), sw.framebuffer());
        }
        if (game.decision_count() != before) {
            /* A new screen: let it settle (Godot's reference: 5th frame;
             * a second for the intro animations), then save it. */
            tree.layout();
            for (int f = 0; f < 60; ++f) {
                tree.process(1.0 / 60.0);
                tree.layout(1);
            }
            sw.begin((int)(1920 * scale + 0.5f), (int)(1080 * scale + 0.5f), Color(0.0923724f, 0.122398f, 0.116929f, 1.0f));
            tree.render(canvas);
            sw.end();
            const std::string& kind = game.decision()["decision"].str();
            std::string base = format("%s/%04d_%s", out_dir.c_str(), shown, kind.c_str());
            if (!write_png((base + ".png").c_str(), sw.framebuffer())) fprintf(stderr, "cannot write %s.png\n", base.c_str());
            /* The decision as JSON lines are not kept by Json; re-serialise
             * what we need: the logic log has the full lines. */
            save_text(base + ".txt", kind + "\n");
            printf("%04d %s%s\n", shown, kind.c_str(), game.last_error().empty() ? "" : "  (logic error before it)");
            fflush(stdout);
            ++shown;
            idle = 0;
            if (game.game_over()) break;
        } else if (link.finished()) {
            fprintf(stderr, "spire_play: the logic exited (see %s/logic.log)\n", out_dir.c_str());
            return 1;
        } else {
            usleep(2000);
            if (++idle > 60000) { /* 2 minutes without an answer */
                fprintf(stderr, "spire_play: no answer from the logic\n");
                return 1;
            }
        }
    }
    printf("%d screens\n", shown);
    return 0;
}
