/* PC presentation regressions against local protocol traces. Fixtures and
 * packs stay outside git: this executable reads the player's own traces. */
#include "../../engine/core/pack.h"
#include "../../engine/audio/game_audio.h"
#include "../../engine/game/card_select_screen.h"
#include "../../engine/game/combat_screen.h"
#include "../../engine/game/event_screen.h"
#include "../../engine/game/game.h"
#include "../../engine/game/game_client.h"
#include "../../engine/game/logic_link.h"
#include "../../engine/game/loc.h"
#include "../../engine/game/ui_util.h"
#include "../../engine/gfx/sw_renderer.h"
#include "../../engine/scene/canvas.h"
#include "../../engine/scene/controls.h"
#include "../../engine/scene/line_2d.h"
#include "../../engine/scene/node.h"
#include "../../engine/scene/particles.h"
#include "../../engine/scene/scene.h"
#include "../../engine/scene/shaders.h"
#include "../../engine/scene/text_nodes.h"
#include "../../engine/scene/tree.h"
#include "../../engine/text/font.h"

#include <deque>
#include <fstream>
#include <math.h>
#include <stdio.h>
#include <string.h>

using namespace spire;

class ReplayLink : public LogicLink {
public:
    std::deque<std::string> incoming;
    std::vector<std::string> sent;
    virtual bool send(const std::string& s) { sent.push_back(s); return true; }
    virtual bool poll(std::string& s) {
        if (incoming.empty()) return false;
        s = incoming.front(); incoming.pop_front(); return true;
    }
    virtual bool finished() { return false; }
};

static int failures = 0;
static void check(bool ok, const char* message) {
    if (!ok) { fprintf(stderr, "FAIL: %s\n", message); ++failures; }
}

static void combat_event_case(SceneLibrary& lib, const Json& decision) {
    EventScreen screen(&lib);
    SceneTree tree;
    std::string error;
    Node* root = screen.build(decision, &error);
    check(root != NULL, "combat event did not build its layout");
    if (!root) return;
    tree.root()->add_child(root);
    tree.set_viewport(Vec2(1920, 1080), Xform2D());
    for (int i = 0; i < 60; ++i) tree.process(0.05);
    Node* room = find_named(root, "CombatRoomContainer");
    Node* enemies = find_named(room, "EnemyContainer");
    const Json& es = decision["combat_visuals"]["enemies"];
    check(es.size() > 0 && enemies && !enemies->children.empty(), "combat event lost the encounter's enemies");
    Node* ui = find_named(room, "CombatUi");
    check(ui && ui->is_canvas_item() && !((CanvasItem*)ui)->visible, "combat event enabled the combat hand/UI");
    Node* enemy = enemies && !enemies->children.empty() ? enemies->children[0] : NULL;
    Node* health = enemy ? enemy->find_unique("HealthBar") : NULL;
    check(health && health->is_canvas_item() && !((CanvasItem*)health)->visible, "visual-only enemy health display is visible");
    check(find_named(root, "Portrait") == NULL, "combat event used the portrait layout");
    PadState pad; pad.pressed = PadState::A;
    std::string command = screen.input(pad);
    Json response = Json::parse(command.c_str(), command.size(), &error);
    check(response["action"].str() == "choose_option", "combat event options did not accept controller input");
}

static void advance(SceneTree& tree, GameClient& game) {
    int frames = 0;
    do {
        tree.process(1.0 / 60.0);
        if (SceneLibrary::current() && SceneLibrary::current()->audio()) SceneLibrary::current()->audio()->update(1.0 / 60.0);
        tree.layout(1);
        game.update(PadState());
    } while (game.busy() && ++frames < 3600);
    check(!game.busy(), "presentation did not finish within 60 simulated seconds");
}

static void audio_timing_case(SceneLibrary& lib, const Json& sample) {
    Json d = sample; d.set("steps", Json());
    CombatScreen screen(&lib);
    SceneTree tree;
    std::string error;
    Node* root = screen.build(d, &error);
    check(root != NULL, "audio timing combat fixture did not build");
    if (!root) return;
    tree.root()->add_child(root);
    const char* text = "[[{\"t\":\"sfx\",\"op\":\"play\",\"path\":\"event:/sfx/block_gain\"}]]";
    Json steps = Json::parse(text, strlen(text), &error);
    screen.play_steps(steps);
    check(!screen.busy(), "sound-only steps added a combat input delay");
    tree.process(1.0 / 60.0);
    if (lib.audio()) lib.audio()->update(1.0 / 60.0);
}

static void selection_cases(SceneLibrary& lib, const Json& sample) {
    if (sample.size() < 2) return;
    static const char* kinds[] = { "simple_grid", "combat_pile", "deck", "upgrade" };
    for (int kind = 0; kind < 8; ++kind) {
        Json d, cards = Json::make_array();
        for (int i = 0; i < 8; ++i) {
            Json card = sample[(size_t)i % sample.size()];
            card.set("index", Json::make_number(i));
            cards.append(card);
        }
        d.set("cards", cards);
        d.set("kind", Json::make_string(kinds[kind % 4]));
        d.set("manual_confirm", Json::make_number(kind >= 4 ? 1 : 0));
        d.set("min_select", Json::make_number(2));
        d.set("max_select", Json::make_number(2));
        CardSelectScreen screen(&lib);
        SceneTree tree;
        std::string error;
        Node* root = screen.build(d, &error);
        check(root != NULL, "two-card selection screen failed to build");
        if (!root) continue;
        tree.root()->add_child(root);
        tree.layout();
        PadState pad;
        pad.pressed = PadState::A;
        check(screen.input(pad).empty(), "two-card selection submitted after only one card");
        pad.pressed = PadState::RIGHT;
        screen.input(pad);
        pad.pressed = PadState::A;
        std::string command = screen.input(pad);
        if (kind >= 4) {
            check(command.empty(), "manual selection submitted without confirmation");
            pad.pressed = PadState::RIGHT; screen.input(pad);
            pad.pressed = PadState::A;
            check(screen.input(pad).empty(), "manual selection exceeded its maximum");
            pad.pressed = PadState::Y;
            command = screen.input(pad);
        }
        if (command.empty()) command = screen.input(pad); /* deck preview confirm */
        if (command.empty()) fprintf(stderr, "selection kind %s, manual %d: no confirmation command\n", kinds[kind % 4], kind >= 4 ? 1 : 0);
        check(!command.empty(), "two-card selection cannot be confirmed with the controller");
        Json result = Json::parse(command.c_str(), command.size(), &error);
        check(result["action"].str() == "select_cards" && result["args"]["indices"].str() == "0,1",
              "two-card selection did not submit both chosen indices");
    }
}

static void actual_selection_case(SceneLibrary& lib, const Json& d, const std::string& line) {
    if (d["min_select"].i() != 2 || d["max_select"].i() != 2 || d["cards"].size() < 2) return;
    SceneTree tree;
    ReplayLink link;
    GameClient game(&lib, &tree, &link);
    link.incoming.push_back("{\"type\":\"ready\"}"); game.update(PadState());
    link.incoming.push_back(line); game.update(PadState());
    advance(tree, game);
    size_t before = link.sent.size();
    PadState pad;
    pad.pressed = PadState::A; game.update(pad);
    check(link.sent.size() == before, "actual selection submitted after only one card");
    pad.pressed = PadState::RIGHT; game.update(pad);
    pad.pressed = PadState::A; game.update(pad);
    if (link.sent.size() == before) { pad.pressed = PadState::Y; game.update(pad); }
    if (link.sent.size() == before) { pad.pressed = PadState::A; game.update(pad); }
    check(link.sent.size() == before + 1, "actual two-card decision did not send a controller command");
    if (link.sent.size() > before) {
        std::string error;
        Json cmd = Json::parse(link.sent.back().c_str(), link.sent.back().size(), &error);
        check(cmd["action"].str() == "select_cards" && cmd["args"]["indices"].str() == "0,1",
              "actual selection sent the wrong cards");
    }
}

static size_t particles_alive(Node* n) {
    size_t count = 0;
    if (n->type == "GPUParticles2D" || n->type == "CPUParticles2D") count += ((Particles2D*)n)->active_count();
    for (size_t i = 0; i < n->children.size(); ++i) count += particles_alive(n->children[i]);
    return count;
}

static void lifecycle_cases(SceneLibrary& lib) {
    std::string gradient_error;
    const std::string gradient_json = "{\"colors\":{\"t\":\"PackedColorArray\",\"v\":[1,1,1,0,1,1,1,1]},\"interpolation_mode\":2}";
    Json gradient_props = Json::parse(gradient_json.c_str(), gradient_json.size(), &gradient_error);
    Resource gradient_resource(Resource::RES_GENERIC, "Gradient");
    gradient_resource.props = &gradient_props;
    GradientData gradient;
    check(GradientData::from(&gradient_resource, gradient) && gradient.at(0).a == 0 && gradient.at(1).a == 1,
          "gradient lost explicit colors when offsets used their defaults");
    check(fabsf(gradient.at(0.25f).a - 0.203125f) < 0.00001f, "gradient cubic interpolation did not match Godot");
    Particles2D p(false);
    p.amount = 4;
    p.lifetime = 0.1;
    p.fixed_fps = 0;
    p.one_shot = true;
    p.explosiveness = 1;
    p.process(0.05);
    check(p.active_count() > 0, "one-shot particles did not start");
    for (int i = 0; i < 30; ++i) p.process(1.0 / 60.0);
    check(p.finished(), "one-shot particles froze after emission stopped");
    p.restart();
    p.process(0.05);
    check(p.active_count() > 0, "restarted particle system did not emit again");

    float value = 0;
    TweenSet tweens;
    tweens.tween(&value, 1, 1, TRANS_LINEAR, EASE_IN_OUT, 1);
    tweens.update(1.5);
    check(value > 0.49f && value < 0.51f, "delayed tween lost the remainder of a late frame");

    std::string error;
    SceneTree tree;
    Node* fx = lib.instantiate("scenes/vfx/vfx_heal_osty.tscn", &error);
    check(fx != NULL, "Osty heal effect did not load");
    if (fx) {
        tree.root()->add_child(fx);
        for (int i = 0; i < 90; ++i) tree.process(1.0 / 60.0);
        check(particles_alive(fx) == 0, "Osty heal particles remained visible past their lifetimes");
        for (int i = 0; i < 155; ++i) tree.process(1.0 / 60.0);
        check(tree.root()->children.empty(), "expired Osty effect was retained in the room");
    }
    Node* slash = lib.instantiate("scenes/vfx/vfx_attack_slash.tscn", &error);
    check(slash != NULL, "slash effect did not load");
    if (slash) {
        tree.root()->add_child(slash);
        for (int i = 0; i < 12; ++i) tree.process(1.0 / 60.0);
        check(!tree.root()->children.empty(), "slash effect ended before its four frames played");
        for (int i = 0; i < 8; ++i) tree.process(1.0 / 60.0);
        check(tree.root()->children.empty(), "finished slash effect was retained in the room");
    }
}

static void orb_effect_cases(SceneLibrary& lib) {
    SceneTree tree;
    std::string error;
    Node* orb = lib.instantiate("scenes/orbs/orb_visuals/lightning_orb.tscn", &error);
    check(orb != NULL, "lightning orb did not load for shaker check");
    if (orb) {
        tree.root()->add_child(orb);
        Node* shaker = orb->get_node("spine_shaker");
        Node* target = orb->get_node("SpineSkeleton");
        check(shake_vfx(shaker, 1, 0.4), "orb shaker script was unavailable");
        bool moved = false;
        for (int i = 0; i < 15; ++i) {
            tree.process(1.0 / 60.0);
            if (target && target->is_node2d()) {
                Vec2 p = ((Node2D*)target)->position;
                if (p.x != 0 || p.y != 0) moved = true;
            }
        }
        check(moved, "orb passive shaker did not move its Spine target");
        for (int i = 0; i < 30; ++i) tree.process(1.0 / 60.0);
        check(target && target->is_node2d() && ((Node2D*)target)->position.x == 0 && ((Node2D*)target)->position.y == 0,
              "orb shaker did not return to rest");
        tree.root()->remove_child(orb); delete orb;
    }
    static const char* kinds[] = { "dark", "plasma" };
    for (int kind = 0; kind < 2; ++kind) {
        Control* box = full_rect_control("VfxBox");
        tree.root()->add_child(box);
        std::string path = std::string("vfx/orbs/") + kinds[kind] + "/vfx_" + kinds[kind] + "_orb_";
        if (kind == 0) path += "evoke_";
        Node* handler = make_vfx_projectile(&lib, path + "projectile_handler", path + "projectile", Vec2(100, 200), Vec2(500, 300));
        check(handler != NULL, "orb projectile handler did not load");
        if (handler) {
            box->add_child(handler);
            std::string name = handler->name;
            tree.process(1.0 / 60.0);
            check(!handler->children.empty(), "projectile handler did not instantiate its projectile");
            tree.process(1.0 / 60.0); tree.process(1.0 / 60.0);
            Node* trail_node = find_named(handler, "trail");
            check(trail_node && trail_node->type == "Line2D" && ((Line2D*)trail_node)->points.size() >= 2,
                  "projectile trail did not retain its movement points");
            const char* shots = getenv("SPIRE_CHECK_SHOTS");
            if (shots) {
                for (int frame = 0; frame < 3; ++frame) tree.process(1.0 / 60.0);
                tree.layout();
                SwRenderer renderer; Canvas canvas(&renderer, lib.pack());
                renderer.begin(640, 360, Color(0, 0, 0, 1)); tree.render(canvas); renderer.end();
                write_png(format("%s/%s_projectile.png", shots, kinds[kind]).c_str(), renderer.framebuffer());
                check(tree.skipped.empty(), "orb projectile render used an unsupported shader");
            }
            const Json* travel = handler->get_extra("_travelTimeRange");
            double maximum = travel ? (*travel)["v"][1].num(1) : 1;
            int frames = (int)ceil(maximum * 60) + 2;
            for (int i = 0; i < frames; ++i) tree.process(1.0 / 60.0);
            if (!handler->children.empty() && handler->children[0]->is_node2d()) {
                Vec2 at = ((Node2D*)handler->children[0])->global_transform().origin;
                check(fabsf(at.x - 500) < 0.01f && fabsf(at.y - 300) < 0.01f,
                      "orb projectile did not reach its target on time");
            }
            for (int i = 0; i < 180; ++i) tree.process(1.0 / 60.0);
            check(find_named(box, name.c_str()) == NULL, "projectile handler outlived its trail delay");
        }
        tree.root()->remove_child(box); delete box;
    }
}

static void line_cases(const Pack& pack) {
    SceneTree tree;
    tree.set_viewport(Vec2(240, 64), Xform2D());
    Line2D* line = (Line2D*)SceneLibrary::create_node("Line2D");
    line->width = 20;
    line->points.push_back(Vec2(20, 32)); line->points.push_back(Vec2(220, 32));
    line->gradient.offsets.push_back(0); line->gradient.offsets.push_back(1);
    line->gradient.colors.push_back(Color(1, 0, 0, 1)); line->gradient.colors.push_back(Color(0, 0, 1, 1));
    tree.root()->add_child(line);
    SwRenderer renderer; Canvas canvas(&renderer, &pack);
    renderer.begin(240, 64, Color(0, 0, 0, 1)); tree.render(canvas); renderer.end();
    const Image& image = renderer.framebuffer();
    check(image.at(40, 32)[0] > image.at(40, 32)[2] && image.at(200, 32)[2] > image.at(200, 32)[0],
          "Line2D did not draw its lengthwise gradient");
    check(image.at(40, 10)[0] == 0 && image.at(40, 24)[0] > 0, "Line2D did not respect its width");
    line->begin_cap = line->end_cap = Line2D::LINE_CAP_ROUND;
    for (int joint = 0; joint < 3; ++joint) {
        line->joint_mode = joint;
        renderer.begin(240, 64, Color(0, 0, 0, 1)); tree.render(canvas); renderer.end();
        check(renderer.framebuffer().at(12, 32)[0] > 0 && renderer.framebuffer().at(228, 32)[2] > 0,
              "Line2D round caps did not extend past the endpoints");
        line->points.insert(line->points.begin() + 1, Vec2(120, 12));
        renderer.begin(240, 64, Color(0, 0, 0, 1)); tree.render(canvas); renderer.end();
        check(renderer.triangles_drawn() > 4, "Line2D did not build its corner/cap geometry");
        line->points.erase(line->points.begin() + 1);
    }
    line->closed = true;
    line->points.clear();
    line->points.push_back(Vec2(40, 20)); line->points.push_back(Vec2(200, 20));
    line->points.push_back(Vec2(200, 50)); line->points.push_back(Vec2(40, 50));
    renderer.begin(240, 64, Color(0, 0, 0, 1)); tree.render(canvas); renderer.end();
    const u8* closure = renderer.framebuffer().at(40, 35);
    check(closure[0] > 0 || closure[2] > 0, "closed Line2D omitted its final segment");
}

class CountRenderer : public SwRenderer {
public:
    CountRenderer() : created(0), destroyed(0) {}
    virtual Texture* create_texture(const TextureData& data) { ++created; return SwRenderer::create_texture(data); }
    virtual void destroy_texture(Texture* t) { ++destroyed; SwRenderer::destroy_texture(t); }
    int created, destroyed;
};

static void texture_cache_cases(const Pack& pack) {
    CountRenderer renderer;
    {
        Canvas canvas(&renderer, &pack);
        TextureData td;
        td.format = TEX_ARGB8; td.flags = 0;
        td.width = td.height = td.orig_width = td.orig_height = 16;
        td.levels.resize(1); td.levels[0].assign(16 * 16 * 4, 255);
        canvas.set_texture_budget(2 * 16 * 16 * 4);
        canvas.begin_frame();
        Texture* a = canvas.cache_texture("test:a", td);
        canvas.cache_texture("test:b", td);
        check(canvas.cache_texture("test:a", td) == a && renderer.created == 2, "texture cache uploaded a duplicate");
        canvas.end_frame();
        canvas.begin_frame();
        canvas.cache_texture("test:c", td);
        check(renderer.destroyed == 0, "texture cache evicted a sampler before the frame finished");
        canvas.end_frame();
        check(canvas.cached_texture("test:a") == a && !canvas.cached_texture("test:b"), "texture cache did not evict the least recently used texture");
        check(canvas.texture_bytes() <= 2 * 16 * 16 * 4 && canvas.texture_count() == 2, "texture budget was exceeded after the frame");
        for (int i = 0; i < 122; ++i) { canvas.begin_frame(); canvas.end_frame(); }
        check(canvas.texture_count() == 0, "unused screen textures were retained indefinitely");
        canvas.cache_texture("test:final", td);
    }
    check(renderer.created == renderer.destroyed, "canvas destruction leaked textures");
    /* Resource aliases and renderer lifetimes must not share raw GPU pointers. */
    ImageTextureRes first, alias;
    for (size_t i = 0; i < pack.count(); ++i) if (pack.entry(i).type == PACK_TEXTURE) {
        first.path = alias.path = pack.name_of(&pack.entry(i)); break;
    }
    check(!first.path.empty(), "no pack texture available for alias check");
    int before = renderer.created;
    {
        Canvas canvas(&renderer, &pack);
        Texture* t = canvas.gpu_texture(&first);
        check(t && canvas.gpu_texture(&alias) == t && renderer.created == before + 1,
              "texture aliases uploaded separate GPU copies");
    }
    {
        Canvas canvas(&renderer, &pack);
        check(canvas.gpu_texture(&first) != NULL && renderer.created == before + 2,
              "a new canvas reused a freed resource GPU pointer");
    }
    check(renderer.created == renderer.destroyed, "pack texture teardown leaked GPU copies");
}

static void doom_cases(SceneLibrary& lib, const Pack& pack) {
    std::string error;
    Node* bar = lib.instantiate("scenes/combat/health_bar.tscn", &error);
    Node* node = bar ? bar->find_unique("DoomForeground") : NULL;
    check(node && node->is_control(), "doom foreground did not load");
    if (!node || !node->is_control()) { delete bar; return; }
    node->parent->remove_child(node);
    {
        SceneTree tree;
        tree.set_viewport(Vec2(320, 180), Xform2D());
        Control* doom = (Control*)node;
        for (int i = 0; i < 4; ++i) doom->anchor[i] = 0;
        place(doom, 20, 80, 280, 28);
        doom->visible = true;
        tree.root()->add_child(doom);
        tree.layout();
        SwRenderer renderer;
        Canvas canvas(&renderer, &pack);
        double old_time = g_shader_time;
        std::vector<u8> previous;
        for (int frame = 0; frame < 2; ++frame) {
            g_shader_time = frame * 100;
            renderer.begin(320, 180, Color(0, 0, 0, 1));
            tree.render(canvas); renderer.end();
            check(tree.skipped.empty(), "doom bar shader was skipped");
            const Image& image = renderer.framebuffer();
            size_t purple = 0, different = 0;
            for (size_t i = 0; i < image.px.size(); i += 4) {
                if (image.px[i + 2] > image.px[i + 1] && image.px[i] > image.px[i + 1]) ++purple;
                if (frame && image.px[i] != previous[i]) ++different;
            }
            check(purple > 5000, "doom bar did not draw its purple gradient");
            if (frame) check(different > 100, "doom noise did not animate in screen space");
            previous = image.px;
            const char* shots = getenv("SPIRE_CHECK_SHOTS");
            if (shots) write_png(format("%s/doom_%d.png", shots, frame).c_str(), image);
        }
        g_shader_time = old_time;
    }
    delete bar;
}

static void atlas_fallback_cases(SceneLibrary& lib) {
    const char* path = "images/atlases/card_atlas.sprites/silent/sidestep.tres";
    Resource* r = lib.ext(path);
    check(r && r->is_texture(), "unpacked beta card portrait did not resolve through the atlas loader");
    if (r && r->is_texture()) {
        Texture2DRes* texture = (Texture2DRes*)r;
        Rect2 dst, src;
        ImageTextureRes* image = NULL;
        texture->get_rect_region(Rect2(Vec2(), texture->size()), Rect2(Vec2(), texture->size()), dst, src, &image);
        check(image && image->path == "images/packed/card_portraits/silent/beta/sidestep.png",
              "card portrait fallback selected the wrong image");
    }
    check(lib.unsupported.find(std::string("missing ") + path) == lib.unsupported.end(), "valid beta portrait was reported missing");
}

static void transition_case(SceneLibrary& lib, const Pack& pack, const std::string& map_line, const std::string& room_line) {
    for (int slow = 0; slow < 2; ++slow) {
        SceneTree tree;
        ReplayLink link;
        GameClient game(&lib, &tree, &link);
        link.incoming.push_back("{\"type\":\"ready\"}"); game.update(PadState());
        link.incoming.push_back(map_line); game.update(PadState());
        check(game.busy(), "map opening had no animation");
        advance(tree, game);
        Node* map = tree.root()->children[0];
        int count = game.decision_count();
        PadState pad; pad.pressed = PadState::A; game.update(pad);
        check(game.busy() && !link.sent.empty(), "map selection did not start travel and send the command");
        int wait_frames = slow ? 90 : 12;
        for (int i = 0; i < wait_frames; ++i) { tree.process(1.0 / 60.0); tree.layout(1); game.update(PadState()); }
        check(tree.root()->children[0] == map && game.decision_count() == count,
              "map disappeared before the destination response arrived");
        link.incoming.push_back(room_line); game.update(PadState());
        if (!slow) {
            check(tree.root()->children[0] == map && game.decision_count() == count,
                  "fast logic response cut off the map travel animation");
            for (int i = 0; i < 30; ++i) { tree.process(1.0 / 60.0); tree.layout(1); game.update(PadState()); }
            check(game.decision_count() == count, "room swapped before the fade reached black");
            const char* shots = getenv("SPIRE_CHECK_SHOTS");
            if (shots) {
                tree.set_viewport(Vec2(1920, 1080), Xform2D::scaling(1.0f / 3, 1.0f / 3)); tree.layout();
                SwRenderer renderer; Canvas canvas(&renderer, &pack);
                renderer.begin(640, 360, Color(0, 0, 0, 1)); tree.render(canvas); renderer.end();
                write_png(format("%s/map_travel.png", shots).c_str(), renderer.framebuffer());
            }
        }
        advance(tree, game);
        check(game.decision_count() == count + 1 && game.decision()["decision"].str() == "combat_play",
              "room transition did not finish at the destination decision");
        Node* overlay = find_named(tree.root(), "RoomTransition");
        check(overlay && !((Control*)overlay)->visible, "transition overlay remained after room fade-in");
    }
}

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: spire_check <pack.s3p> <trace.jsonl> [more traces...]\n");
        return 2;
    }
    Pack pack;
    std::string error;
    if (!pack.open(argv[1], &error)) { fprintf(stderr, "%s\n", error.c_str()); return 1; }
    install_game_scripts();
    std::string pack_path = argv[1];
    size_t slash = pack_path.find_last_of("/\\");
    Loc::init(slash == std::string::npos ? "data" : pack_path.substr(0, slash) + "/data");
    SceneLibrary lib(&pack);
    GameAudio audio(&pack);
    if (audio.events().available()) lib.set_audio(&audio);
    FontLibrary fonts(&pack);
    FontLibrary::set_current(&fonts);
    lifecycle_cases(lib);
    orb_effect_cases(lib);
    line_cases(pack);
    texture_cache_cases(pack);
    doom_cases(lib, pack);
    atlas_fallback_cases(lib);
    int responses = 0, persistent = 0, hp_checks = 0, orb_checks = 0, death_checks = 0;
    bool selections_checked = false;
    bool transition_checked = false;
    for (int file = 2; file < argc; ++file) {
        std::ifstream input(argv[file]);
        if (!input) { check(false, "cannot open trace"); continue; }
        SceneTree tree;
        ReplayLink link;
        audio.events().stop_all();
        GameClient game(&lib, &tree, &link);
        link.incoming.push_back("{\"type\":\"ready\"}");
        game.update(PadState());
        Json previous;
        std::string line, previous_line;
        while (std::getline(input, line)) {
            if (line.empty() || line[0] != '{') continue;
            Json d = Json::parse(line.c_str(), line.size(), &error);
            if (d["type"].str() != "decision") continue;
            if (!transition_checked && previous["decision"].str() == "map_select" && d["decision"].str() == "combat_play") {
                transition_case(lib, pack, previous_line, line);
                transition_checked = true;
            }
            if (d["decision"].str() == "card_select") actual_selection_case(lib, d, line);
            if (!selections_checked && d["hand"].size() >= 2) {
                audio_timing_case(lib, d);
                selection_cases(lib, d["hand"]);
                selections_checked = true;
            }
            Node* old_room = tree.root()->children.empty() ? NULL : tree.root()->children[0];
            Node* old_event_allies = previous["layout"].str() == "Combat" ? find_named(old_room, "AllyContainer") : NULL;
            bool continuing_event = old_event_allies &&
                ((d["layout"].str() == "Combat" && d["event_id"] == previous["event_id"]) ||
                 (d["decision"].str() == "combat_play" && d["view"]["encounter"] == previous["combat_visuals"]["view"]["encounter"]));
            bool same_fight = previous["decision"].str() == "combat_play" && d["decision"].str() == "combat_play";
            Node* hp_label = NULL;
            std::string old_hp;
            int expected = -1, max_hp = 0;
            std::string hp_who;
            if (same_fight && old_room) {
                const Json& steps = d["steps"];
                const Json& es = previous["enemies"];
                Node* enemies = old_room->find_unique("EnemyContainer");
                for (size_t s = 0; s < steps.size() && !hp_label; ++s)
                    for (size_t k = 0; k < steps[s].size() && !hp_label; ++k) {
                        const Json& ch = steps[s][k];
                        if (ch["t"].str() != "hp" || ch["to"].i() <= 0) continue;
                        for (size_t e = 0; enemies && e < es.size(); ++e) {
                            if (format("e%d", es[e]["combat_id"].i()) != ch["who"].str()) continue;
                            Node* enemy = enemies->get_node(format("CombatEnemy_%d", es[e]["combat_id"].i()));
                            Node* label = find_named(enemy, "HpLabel");
                            if (label && label->type == "Label") {
                                hp_label = label;
                                old_hp = ((Label*)label)->text;
                                expected = ch["to"].i();
                                max_hp = es[e]["max_hp"].i();
                                hp_who = ch["who"].str();
                            }
                        }
                    }
            }
            link.incoming.push_back(line);
            game.update(PadState());
            if (same_fight) {
                check(!tree.root()->children.empty() && tree.root()->children[0] == old_room,
                      "combat room was rebuilt between consecutive decisions");
                ++persistent;
                if (hp_label) {
                    check(((Label*)hp_label)->text == old_hp, "HP jumped to its final value before the impact cue");
                    ++hp_checks;
                }
            }
            std::vector<Node*> old_orbs;
            for (size_t o = 0; same_fight && old_room && o < previous["orbs"].size(); ++o)
                old_orbs.push_back(find_named(old_room, format("CombatOrb_%d", previous["orbs"][o]["instance_id"].i((int)o + 1)).c_str()));
            advance(tree, game);
            if (d["layout"].str() == "Combat") combat_event_case(lib, d);
            if (continuing_event) {
                check(find_named(tree.root(), "AllyContainer") == old_event_allies, "combat event rebuilt its embedded encounter between pages/fight");
                if (d["decision"].str() == "combat_play") {
                    Node* ui = find_named(tree.root(), "CombatUi");
                    check(ui && ui->is_canvas_item() && ((CanvasItem*)ui)->visible, "combat event did not activate the combat UI");
                    Node* es = find_named(tree.root(), "EnemyContainer");
                    Node* health = es && !es->children.empty() ? es->children[0]->find_unique("HealthBar") : NULL;
                    check(health && health->is_canvas_item() && ((CanvasItem*)health)->visible, "combat event did not restore enemy interaction/state display");
                }
            }
            if (same_fight && old_room) {
                for (size_t s = 0; s < d["steps"].size(); ++s)
                    for (size_t k = 0; k < d["steps"][s].size(); ++k) {
                        const Json& ch = d["steps"][s][k];
                        if (ch["t"].str() != "death" && ch["t"].str() != "despawn") continue;
                        const std::string& who = ch["who"].str();
                        if (who.size() < 2 || who[0] != 'e') continue;
                        int id = atoi(who.c_str() + 1);
                        bool present = false;
                        for (size_t e = 0; e < d["enemies"].size(); ++e)
                            if (d["enemies"][e]["combat_id"].i(-1) == id) present = true;
                        if (present) continue; /* revived or still part of the encounter */
                        Node* node = find_named(old_room, format("CombatEnemy_%d", id).c_str());
                        check(!node || !node->is_control() || !((Control*)node)->visible || ((Control*)node)->modulate.a < 0.01f,
                              "dead enemy remained visible after its response finished");
                        ++death_checks;
                    }
            }
            if (d["decision"].str() == "combat_play" && !tree.root()->children.empty()) {
                Node* room = tree.root()->children[0];
                for (size_t o = 0; o < d["orbs"].size(); ++o) {
                    const Json& orb = d["orbs"][o];
                    int id = orb["instance_id"].i((int)o + 1);
                    Node* node = find_named(room, format("CombatOrb_%d", id).c_str());
                    check(node != NULL, "channelled orb was absent after its response played");
                    ++orb_checks;
                    for (size_t p = 0; p < old_orbs.size(); ++p)
                        if (previous["orbs"][p]["instance_id"].i((int)p + 1) == id)
                            check(node == old_orbs[p], "surviving orb sprite was rebuilt between responses");
                }
                for (size_t p = 0; p < old_orbs.size(); ++p) {
                    int id = previous["orbs"][p]["instance_id"].i((int)p + 1);
                    bool present = false;
                    for (size_t o = 0; o < d["orbs"].size(); ++o)
                        if (d["orbs"][o]["instance_id"].i((int)o + 1) == id) present = true;
                    if (!present) check(find_named(room, format("CombatOrb_%d", id).c_str()) == NULL,
                                        "evoked orb stayed in the room after its fade");
                }
            }
            if (hp_label && expected > 0) {
                /* More than one hit may occur in the response: compare the
                 * final decision rather than the first damage step. */
                const Json& es = d["enemies"];
                bool present = false, matched = false;
                for (size_t e = 0; e < es.size(); ++e)
                    if (format("e%d", es[e]["combat_id"].i()) == hp_who) {
                        present = true;
                        matched = ((Label*)hp_label)->text == format("%d/%d", es[e]["hp"].i(), es[e]["max_hp"].i());
                    }
                (void)max_hp;
                if (present && !matched) fprintf(stderr, "%s response %d: %s\n", argv[file], responses, hp_who.c_str());
                check(matched || !present, "health bar did not settle to the decision's HP");
            }
            previous = d;
            previous_line = line;
            ++responses;
            if (responses % 100 == 0) { printf("presentation: %d responses checked\n", responses); fflush(stdout); }
        }
    }
    check(persistent > 0, "no consecutive combat decisions were checked");
    check(hp_checks > 0, "no delayed health changes were checked");
    check(selections_checked, "no card data was available for controller selection checks");
    printf("presentation: %d responses, %d persistent rooms, %d impact checks, %d orb checks, %d death checks, %d failures\n",
           responses, persistent, hp_checks, orb_checks, death_checks, failures);
    if (audio.events().available()) {
        check(audio.events().failures() == 0, "audio replay had decoder failures");
        printf("audio replay: %.0f voices started, %u absent event calls, %u decoder failures\n",
            (double)audio.events().started_voices(), audio.events().missing_events(), audio.events().failures());
    }
    return failures ? 1 : 0;
}
