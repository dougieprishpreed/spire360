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
 * --vk (a Linux build with SPIRE_VK, tools/xsdk/host_check.sh) draws through
 * the Vulkan renderer, offscreen, in place of the software one; --vk-trace
 * then draws it again with the GPU trace on (the log's "gpu trace" lines).
 * --scale 1 renders at the design size (1920x1080) for diffs against a
 * reference; the console uses 2/3 (1280x720). */
#include "../../engine/core/pack.h"
#include "../../engine/game/combat_screen.h"
#include "../../engine/game/card_select_screen.h"
#include "../../engine/game/pile_screen.h"
#include "../../engine/game/settings_screen.h"
#include "../../engine/game/card_library_screen.h"
#include "../../engine/game/collection_screen.h"
#include "../../engine/game/stats_screen.h"
#include "../../engine/game/run_history_screen.h"
#include "../../engine/game/bestiary_screen.h"
#include "../../engine/game/crystal_sphere_screen.h"
#include "../../engine/game/event_screen.h"
#include "../../engine/game/title_screen.h"
#include "../../engine/game/map_screen.h"
#include "../../engine/game/reward_screen.h"
#include "../../engine/game/rewards_screen.h"
#include "../../engine/game/game_over_screen.h"
#include "../../engine/game/character_select.h"
#include "../../engine/game/bundle_select_screen.h"
#include "../../engine/game/rest_site_screen.h"
#include "../../engine/game/shop_screen.h"
#include "../../engine/game/treasure_screen.h"
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
#if defined(SPIRE_VK)
#include "../../engine/gfx/vk_renderer.h"
#endif

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
    bool use_vk = false, vk_trace = false;
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
                          { "LB", PadState::LB },   { "RB", PadState::RB },     { "LT", PadState::LT },
                          { "RT", PadState::RT },   { "RS_UP", PadState::RS_UP }, { "RS_DOWN", PadState::RS_DOWN },
                          { "RS_LEFT", PadState::RS_LEFT }, { "RS_RIGHT", PadState::RS_RIGHT },
                          { "LS", PadState::LS } };
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
        else if (strcmp(argv[i], "--vk") == 0) use_vk = true;
        else if (strcmp(argv[i], "--vk-trace") == 0) use_vk = vk_trace = true;
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
    CharacterSelectScreen* charsel = NULL;
    PileScreen* piles = NULL;
    SettingsScreen* settings = NULL;
    CardLibraryScreen* library = NULL;
    CollectionScreen* collection = NULL;
    StatsScreen* stats = NULL;
    RunHistoryScreen* history = NULL;
    BestiaryScreen* bestiary = NULL;
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
        if (d["type"].str() == "library") {
            /* a library answer: NCardLibrary on that pool (--pad drives it). */
            library = new CardLibraryScreen(&lib); /* lives as long as the scene */
            scene = library->build(d["pool"].str(), &err);
            if (scene) library->set_cards(d);
        } else if (CollectionScreen::is_answer(d["type"].str())) {
            /* a relic_collection / potion_lab answer (--pad drives it). */
            collection = new CollectionScreen(&lib, d["type"].str() == "relic_collection" ? CollectionScreen::RELICS
                                                                                            : CollectionScreen::POTIONS);
            scene = collection->build(&err);
            if (scene) collection->set_data(d);
        } else if (RunHistoryScreen::is_answer(d["type"].str())) {
            /* a run_history answer (--pad drives it). */
            history = new RunHistoryScreen(&lib);
            scene = history->build(&err);
            if (scene) history->set_data(d);
        } else if (d["type"].str() == "bestiary") {
            /* the bestiary answer; viewer-only keys: "entry" (the selected
             * one's bestiary_entry answer), "move" (a bestiary_move answer
             * played from the start); --pad drives it. */
            bestiary = new BestiaryScreen(&lib);
            scene = bestiary->build(&err);
            if (scene) {
                bestiary->set_data(d);
                PadState none;
                std::string q;
                bestiary->input(none, &q);
                if (d["entry"].is_object()) bestiary->set_data(d["entry"]);
                if (d["move"].is_object()) bestiary->set_data(d["move"]);
            }
        } else if (StatsScreen::is_answer(d["type"].str())) {
            /* the statistics answer (--pad drives it). */
            stats = new StatsScreen(&lib);
            scene = stats->build(&err);
            if (scene) stats->set_data(d);
        } else if (d["type"].str() == "settings") {
            /* the logic's settings answer: NSettingsScreen (--pad drives it). */
            settings = new SettingsScreen(&lib); /* lives as long as the scene */
            scene = settings->build(d, &err);
        } else if (d["type"].str() == "pile") {
            /* view_pile's answer: the deck / pile screen (--pad drives it). */
            piles = new PileScreen(&lib); /* lives as long as the scene */
            scene = piles->build(d, &err);
        } else if (d["type"].str() == "characters") {
            /* The logic's characters answer: the character select screen. */
            charsel = new CharacterSelectScreen(&lib); /* lives as long as the scene */
            scene = charsel->build(d, "", &err);
        } else if (d["decision"].str() == "combat_play") {
            /* Lives as long as the scene (it drives its animations). */
            CombatScreen* cs = new CombatScreen(&lib);
            cs->set_hand_focus(hand_focus);
            scene = cs->build(d, &err);
            /* Its steps played back (--time picks the moment). */
            if (scene) cs->play_steps(d["steps"]);
        } else if (d["decision"].str() == "card_reward") {
            CardRewardScreen* rs = new CardRewardScreen(&lib); /* lives as long as the scene */
            scene = rs->build(d, &err);
            ported = rs;
        } else if (d["decision"].str() == "shop") {
            ShopScreen* ss = new ShopScreen(&lib); /* lives as long as the scene */
            scene = ss->build(d, &err);
            ported = ss;
        } else if (d["decision"].str() == "bundle_select") {
            BundleSelectScreen* bs = new BundleSelectScreen(&lib); /* lives as long as the scene */
            scene = bs->build(d, &err);
            ported = bs;
        } else if (d["decision"].str() == "rest_site") {
            RestSiteScreen* rs = new RestSiteScreen(&lib); /* lives as long as the scene */
            scene = rs->build(d, &err);
            ported = rs;
        } else if (d["decision"].str() == "treasure") {
            /* "previous_stage" (a viewer-only key): the stage it comes from. */
            TreasureScreen* ts = new TreasureScreen(&lib); /* lives as long as the scene */
            ts->set_previous_stage(d["previous_stage"].str());
            scene = ts->build(d, &err);
            ported = ts;
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
        } else if (d["decision"].str() == "crystal_sphere") {
            /* "previous" (a viewer-only key): the decision it comes after. */
            CrystalSphereScreen* cs = new CrystalSphereScreen(&lib); /* lives as long as the scene */
            if (d["previous"].is_object()) cs->set_previous(d["previous"]);
            scene = cs->build(d, &err);
            ported = cs;
        } else if (d["decision"].str() == "map_select") {
            MapScreen* ms = new MapScreen(&lib); /* lives as long as the scene */
            scene = ms->build(d, &err);
            ported = ms;
        } else if (err.empty()) {
            err = "no screen for decision " + d["decision"].str();
        }
    }
    if (scene && alen > 5 && strcmp(argv[2] + alen - 5, ".json") == 0) {
        /* As the game client shows a run's screens: with the top bar
         * ("previous_player", a viewer-only key: the player before, for the
         * potion animations). */
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
        Node* bar = d["player"].is_object() && d["decision"].str() != "game_over" ? make_top_bar(&lib, d, d["context"]["room_type"].str() == "Map", d["previous_player"]) : NULL;
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
    for (size_t k = 0; (ported || title || charsel || piles || settings || library || collection || stats || history || bestiary) && k < pad_script.size(); ++k) {
        PadState pad;
        pad.pressed = pad.held = pad_script[k];
        std::string cmd = ported ? ported->input(pad) : std::string();
        if (piles && !piles->input(pad, 0)) cmd = "close";
        if (settings && settings->input(pad, &cmd) == SettingsScreen::BACK) cmd = "back";
        if (library && library->input(pad, &cmd) == CardLibraryScreen::BACK) cmd = "back";
        if (collection && collection->input(pad, &cmd) == CollectionScreen::BACK) cmd = "back";
        if (stats && stats->input(pad, &cmd) == StatsScreen::BACK) cmd = "back";
        if (history && history->input(pad, &cmd) == RunHistoryScreen::BACK) cmd = "back";
        if (bestiary && bestiary->input(pad, &cmd) == BestiaryScreen::BACK) cmd = "back";
        if (title) {
            TitleScreen::Action a = title->input(pad);
            if (a != TitleScreen::NONE) cmd = a == TitleScreen::SINGLEPLAYER ? "singleplayer" : "quit";
        }
        if (charsel) {
            CharacterSelectScreen::Action a = charsel->input(pad);
            if (a == CharacterSelectScreen::BACK) cmd = "back";
            if (a == CharacterSelectScreen::EMBARK) cmd = "start " + charsel->character();
        }
        if (!cmd.empty()) printf("command: %s\n", cmd.c_str());
        for (int f = 0; f < (k + 1 < pad_script.size() ? 30 : frames); ++f) {
            tree.process(1.0 / 60.0);
            tree.layout(1);
        }
    }
    if (use_vk) {
#if defined(SPIRE_VK)
        int w = (int)(design.x * scale + 0.5f), h = (int)(design.y * scale + 0.5f);
        VkRenderer* vk = VkRenderer::create();
        if (!vk->init(w, h, false, &err)) {
            fprintf(stderr, "vk: %s\n", err.c_str());
            return 1;
        }
        Image img;
        long calls = 0;
        {
            Canvas canvas(vk, &pack);
            vk->begin(w, h, Color(0.0923724f, 0.122398f, 0.116929f, 1.0f));
            tree.render(canvas);
            vk->end();
            img.resize(w, h);
            if (!vk->read_frame(img.px)) {
                fprintf(stderr, "vk: the frame not read back\n");
                return 1;
            }
            calls = canvas.draw_calls();
            if (vk_trace) {
                /* the frame again, draw by draw on the GPU (the console's
                 * profile=1 trace) */
                vk->trace_next_frame("spire_view");
                vk->begin(w, h, Color(0.0923724f, 0.122398f, 0.116929f, 1.0f));
                tree.render(canvas);
                vk->end();
                vk->finish();
            }
        }
        int failed = vk->failed_shaders();
        delete vk;
        if (!write_png(argv[3], img)) {
            fprintf(stderr, "cannot write %s\n", argv[3]);
            return 1;
        }
        printf("%s: %d items, %ld draw calls (Vulkan), %d shaders failed\n", argv[3], tree.items_drawn, calls, failed);
        return 0;
#else
        fprintf(stderr, "--vk: a build without SPIRE_VK\n");
        return 2;
#endif
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
