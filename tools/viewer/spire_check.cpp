/* PC presentation regressions against local protocol traces. Fixtures and
 * packs stay outside git: this executable reads the player's own traces. */
#include "../../engine/core/pack.h"
#include "../../engine/game/card_select_screen.h"
#include "../../engine/game/game.h"
#include "../../engine/game/game_client.h"
#include "../../engine/game/logic_link.h"
#include "../../engine/game/loc.h"
#include "../../engine/game/ui_util.h"
#include "../../engine/scene/node.h"
#include "../../engine/scene/particles.h"
#include "../../engine/scene/scene.h"
#include "../../engine/scene/text_nodes.h"
#include "../../engine/scene/tree.h"
#include "../../engine/text/font.h"

#include <deque>
#include <fstream>
#include <stdio.h>

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

static void advance(SceneTree& tree, GameClient& game) {
    int frames = 0;
    do {
        tree.process(1.0 / 60.0);
        tree.layout(1);
        game.update(PadState());
    } while (game.busy() && ++frames < 3600);
    check(!game.busy(), "presentation did not finish within 60 simulated seconds");
}

static void selection_cases(SceneLibrary& lib, const Json& sample) {
    if (sample.size() < 2) return;
    static const char* kinds[] = { "simple_grid", "combat_pile", "deck", "upgrade" };
    for (int kind = 0; kind < 4; ++kind) {
        Json d, cards = Json::make_array();
        for (int i = 0; i < 8; ++i) {
            Json card = sample[(size_t)i % sample.size()];
            card.set("index", Json::make_number(i));
            cards.append(card);
        }
        d.set("cards", cards);
        d.set("kind", Json::make_string(kinds[kind]));
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
        if (command.empty()) command = screen.input(pad); /* deck preview confirm */
        check(!command.empty(), "two-card selection cannot be confirmed with the controller");
        Json result = Json::parse(command.c_str(), command.size(), &error);
        check(result["action"].str() == "select_cards" && result["args"]["indices"].str() == "0,1",
              "two-card selection did not submit both chosen indices");
    }
}

static size_t particles_alive(Node* n) {
    size_t count = 0;
    if (n->type == "GPUParticles2D" || n->type == "CPUParticles2D") count += ((Particles2D*)n)->active_count();
    for (size_t i = 0; i < n->children.size(); ++i) count += particles_alive(n->children[i]);
    return count;
}

static void lifecycle_cases(SceneLibrary& lib) {
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
    FontLibrary fonts(&pack);
    FontLibrary::set_current(&fonts);
    lifecycle_cases(lib);
    int responses = 0, persistent = 0, hp_checks = 0, orb_checks = 0;
    bool selections_checked = false;
    for (int file = 2; file < argc; ++file) {
        std::ifstream input(argv[file]);
        if (!input) { check(false, "cannot open trace"); continue; }
        SceneTree tree;
        ReplayLink link;
        GameClient game(&lib, &tree, &link);
        link.incoming.push_back("{\"type\":\"ready\"}");
        game.update(PadState());
        Json previous;
        std::string line;
        while (std::getline(input, line)) {
            if (line.empty() || line[0] != '{') continue;
            Json d = Json::parse(line.c_str(), line.size(), &error);
            if (d["type"].str() != "decision") continue;
            if (!selections_checked && d["hand"].size() >= 2) {
                selection_cases(lib, d["hand"]);
                selections_checked = true;
            }
            Node* old_room = tree.root()->children.empty() ? NULL : tree.root()->children[0];
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
            ++responses;
        }
    }
    check(persistent > 0, "no consecutive combat decisions were checked");
    check(hp_checks > 0, "no delayed health changes were checked");
    check(selections_checked, "no card data was available for controller selection checks");
    printf("presentation: %d responses, %d persistent rooms, %d impact checks, %d orb checks, %d failures\n",
           responses, persistent, hp_checks, orb_checks, failures);
    return failures ? 1 : 0;
}
