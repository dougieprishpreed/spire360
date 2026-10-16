/* spire_view: renders a scene from pack.s3p to a PNG with the software
 * renderer, for comparing against Godot's own render.
 *
 *   spire_view <pack.s3p> <scene> <out.png> [--scale S] [--no-scripts] [--time T]
 *              [--spine-anim NAME] [--origin X Y]
 * --spine-anim plays an animation (looping) on every SpineSprite (creatures
 * get theirs from the game's logic); --origin moves the scene's root (a
 * creature is drawn upward from 0, 0).
 *   spire_view <pack.s3p> <text> <out.png> --text <font> <size>
 *
 * --scale 1 renders at the design size (1920x1080) for diffs against a
 * reference; the console uses 2/3 (1280x720). */
#include "../../engine/core/pack.h"
#include "../../engine/game/combat_screen.h"
#include "../../engine/game/card_select_screen.h"
#include "../../engine/game/event_screen.h"
#include "../../engine/game/title_screen.h"
#include "../../engine/game/map_screen.h"
#include "../../engine/game/reward_screen.h"
#include "../../engine/game/rewards_screen.h"
#include "../../engine/game/game_over_screen.h"
#include "../../engine/game/top_bar.h"
#include "../../engine/game/game.h"
#include "../../engine/game/loc.h"
#include "../../engine/gfx/sw_renderer.h"
#include "../../engine/scene/canvas.h"
#include "../../engine/scene/controls.h"
#include "../../engine/scene/scene.h"
#include "../../engine/scene/script.h"
#include "../../engine/scene/spine_sprite.h"
#include "../../engine/scene/text_nodes.h"
#include "../../engine/scene/tree.h"
#include "../../engine/text/font.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using namespace spire;

/* --text: one Label on black, like tools/godot/glyph.gd, for comparing
 * text rendering with Godot's. */
static Node* text_scene(SceneLibrary& lib, const char* font, int size, const char* text, Vec2* design) {
    Control* root = new Control();
    root->type = "Control";
    ColorRect* bg = new ColorRect();
    bg->type = "ColorRect";
    bg->color = Color(0, 0, 0, 1);
    bg->offset[2] = 400;
    bg->offset[3] = 300;
    root->add_child(bg);
    Label* l = new Label();
    l->type = "Label";
    l->text = text;
    l->offset[0] = l->offset[2] = 10;
    l->offset[1] = l->offset[3] = 10;
    Resource* f = lib.ext(font);
    if (f) l->override_resources["fonts/font"] = f;
    static Json size_json;
    size_json = Json::make_number(size);
    l->override_values["font_sizes/font_size"] = &size_json;
    root->add_child(l);
    *design = Vec2(400, 300);
    return root;
}

static void play_everywhere(Node* n, const char* anim) {
    if (n->type == "SpineSprite" && !((SpineSprite*)n)->set_animation(anim, true, 0))
        fprintf(stderr, "%s: no animation %s\n", n->path().c_str(), anim);
    for (size_t i = 0; i < n->children.size(); ++i) play_everywhere(n->children[i], anim);
}

int main(int argc, char** argv) {
    if (argc < 4) {
        fprintf(stderr, "usage: spire_view <pack.s3p> <scene> <out.png> [--scale S] [--no-scripts] [--time T] [--spine-anim NAME] [--origin X Y] [--hand-focus I] [--pad A,RIGHT,...]\n");
        return 2;
    }
    float scale = 1.0f;
    int frames = 5; /* --time: seconds of process at 60 fps */
    std::vector<unsigned> pad_script;
    const char* spine_anim = NULL;
    float origin_x = 0, origin_y = 0;
    int hand_focus = -1;
    bool has_origin = false;
    for (int i = 4; i < argc; ++i) {
        if (strcmp(argv[i], "--scale") == 0 && i + 1 < argc) scale = (float)atof(argv[++i]);
        else if (strcmp(argv[i], "--spine-anim") == 0 && i + 1 < argc) spine_anim = argv[++i];
        else if (strcmp(argv[i], "--hand-focus") == 0 && i + 1 < argc) hand_focus = atoi(argv[++i]);
        else if (strcmp(argv[i], "--origin") == 0 && i + 2 < argc) {
            origin_x = (float)atof(argv[++i]);
            origin_y = (float)atof(argv[++i]);
            has_origin = true;
        }
        else if (strcmp(argv[i], "--time") == 0 && i + 1 < argc) frames = (int)(atof(argv[++i]) * 60.0 + 0.5);
        else if (strcmp(argv[i], "--pad") == 0 && i + 1 < argc) {
            static const struct {
                const char* name;
                unsigned bit;
            } names[] = { { "A", PadState::A },     { "B", PadState::B },       { "X", PadState::X },
                          { "Y", PadState::Y },     { "LEFT", PadState::LEFT }, { "RIGHT", PadState::RIGHT },
                          { "UP", PadState::UP },   { "DOWN", PadState::DOWN }, { "START", PadState::START },
                          { "LB", PadState::LB },   { "RB", PadState::RB } };
            std::string list = argv[++i];
            size_t at = 0;
            while (at <= list.size()) {
                size_t comma = list.find(',', at);
                if (comma == std::string::npos) comma = list.size();
                std::string tok = list.substr(at, comma - at);
                for (size_t k = 0; k < sizeof(names) / sizeof(names[0]); ++k)
                    if (tok == names[k].name) pad_script.push_back(names[k].bit);
                at = comma + 1;
            }
        }
        else if (strcmp(argv[i], "--no-scripts") == 0) set_scripts_enabled(false); /* as Godot's render, without the game's C# */
    }
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
    Vec2 design(1920, 1080);
    Node* scene = NULL;
    for (int i = 4; i + 2 < argc; ++i)
        if (strcmp(argv[i], "--text") == 0) scene = text_scene(lib, argv[i + 1], atoi(argv[i + 2]), argv[2], &design);
    /* A decision JSON (one line of the logic's output): its screen. */
    RunScreen* ported = NULL;
    size_t alen = strlen(argv[2]);
    if (!scene && alen > 5 && strcmp(argv[2] + alen - 5, ".json") == 0) {
        FILE* jf = fopen(argv[2], "rb");
        std::string text;
        if (jf) {
            char buf[65536];
            size_t got;
            while ((got = fread(buf, 1, sizeof(buf), jf)) > 0) text.append(buf, got);
            fclose(jf);
        }
        Json d = Json::parse(text.c_str(), text.size(), &err);
        if (d["decision"].str() == "combat_play") {
            /* Lives as long as the scene (it drives its animations). */
            CombatScreen* cs = new CombatScreen(&lib);
            cs->set_hand_focus(hand_focus);
            scene = cs->build(d, &err);
        } else if (d["decision"].str() == "card_reward") {
            CardRewardScreen* rs = new CardRewardScreen(&lib); /* lives as long as the scene */
            scene = rs->build(d, &err);
            ported = rs;
        } else if (d["decision"].str() == "game_over") {
            GameOverScreen* gs = new GameOverScreen(&lib); /* lives as long as the scene */
            scene = gs->build(d, &err);
            ported = gs;
        } else if (d["decision"].str() == "rewards") {
            RewardsScreen* rs = new RewardsScreen(&lib); /* lives as long as the scene */
            scene = rs->build(d, &err);
            ported = rs;
        } else if (d["decision"].str() == "card_select") {
            CardSelectScreen* cs = new CardSelectScreen(&lib); /* lives as long as the scene */
            scene = cs->build(d, &err);
            ported = cs;
        } else if (d["decision"].str() == "event_choice") {
            EventScreen* es = new EventScreen(&lib); /* lives as long as the scene */
            scene = es->build(d, &err);
            ported = es;
        } else if (d["decision"].str() == "map_select") {
            MapScreen* ms = new MapScreen(&lib); /* lives as long as the scene */
            scene = ms->build(d, &err);
            ported = ms;
        } else if (err.empty()) {
            err = "no screen for decision " + d["decision"].str();
        }
    }
    if (scene && alen > 5 && strcmp(argv[2] + alen - 5, ".json") == 0) {
        /* As the game client shows a run's screens: with the top bar. */
        FILE* jf = fopen(argv[2], "rb");
        std::string text;
        if (jf) {
            char buf[65536];
            size_t got;
            while ((got = fread(buf, 1, sizeof(buf), jf)) > 0) text.append(buf, got);
            fclose(jf);
        }
        std::string e2;
        Json d = Json::parse(text.c_str(), text.size(), &e2);
        Node* bar = d["player"].is_object() && d["decision"].str() != "game_over" ? make_top_bar(&lib, d, d["context"]["room_type"].str() == "Map") : NULL;
        if (bar) scene->add_child(bar);
    }
    /* "title": the preview's main menu (TitleScreen), --pad driving it. */
    TitleScreen* title = NULL;
    if (!scene && strcmp(argv[2], "title") == 0) {
        title = new TitleScreen(&lib);
        scene = title->build(pack.game_version(), &err);
    }
    if (!scene && err.empty()) scene = lib.instantiate(argv[2], &err);
    if (!scene) {
        fprintf(stderr, "%s\n", err.c_str());
        return 1;
    }
    SceneTree tree;
    tree.set_viewport(design, Xform2D::scaling(scale, scale));
    tree.root()->add_child(scene);
    if (has_origin && scene->is_node2d()) ((Node2D*)scene)->position = Vec2(origin_x, origin_y);
    if (has_origin && scene->is_control()) {
        tree.layout(1);
        ((Control*)scene)->set_position(Vec2(origin_x, origin_y));
    }
    if (spine_anim) play_everywhere(scene, spine_anim);
    tree.layout();
    /* Godot's reference render is taken on its 5th frame (tools/godot/shot.gd). */
    /* As the console loop does: process, then one layout pass, per frame. */
    for (int f = 0; f < frames; ++f) {
        tree.process(1.0 / 60.0);
        tree.layout(1);
    }
    /* --pad A,RIGHT,...: each press after half a second, then --time again
     * before the picture; the screen's commands are printed. */
    for (size_t k = 0; (ported || title) && k < pad_script.size(); ++k) {
        PadState pad;
        pad.pressed = pad.held = pad_script[k];
        std::string cmd = ported ? ported->input(pad) : std::string();
        if (title) {
            TitleScreen::Action a = title->input(pad);
            if (a != TitleScreen::NONE) cmd = a == TitleScreen::START_RUN ? std::string("start ") + title->character() : "quit";
        }
        if (!cmd.empty()) printf("command: %s\n", cmd.c_str());
        for (int f = 0; f < (k + 1 < pad_script.size() ? 30 : frames); ++f) {
            tree.process(1.0 / 60.0);
            tree.layout(1);
        }
    }
    SwRenderer sw;
    Canvas canvas(&sw, &pack);
    /* project.binary: rendering/environment/defaults/default_clear_color */
    sw.begin((int)(design.x * scale + 0.5f), (int)(design.y * scale + 0.5f),
             Color(0.0923724f, 0.122398f, 0.116929f, 1.0f));
    tree.render(canvas);
    sw.end();
    if (!write_png(argv[3], sw.framebuffer())) {
        fprintf(stderr, "cannot write %s\n", argv[3]);
        return 1;
    }
    printf("%s: %d items, %ld draw calls, %ld triangles\n", argv[3], tree.items_drawn, canvas.draw_calls(),
           sw.triangles_drawn());
    for (std::map<std::string, int>::iterator it = tree.skipped.begin(); it != tree.skipped.end(); ++it)
        printf("  not drawn: %s x%d\n", it->first.c_str(), it->second);
    for (std::map<std::string, int>::iterator it = lib.unsupported.begin(); it != lib.unsupported.end(); ++it)
        printf("  unsupported: %s x%d\n", it->first.c_str(), it->second);
    return 0;
}
