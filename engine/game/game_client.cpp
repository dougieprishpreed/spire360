#include "game_client.h"
#include "../audio/game_audio.h"

#include "../scene/controls.h"
#include "../scene/node.h"
#include "../scene/scene.h"
#include "../scene/text_nodes.h"
#include "../scene/tree.h"
#include "../scene/canvas.h"
#include "../scene/particles.h"
#include "../scene/value.h"
#include "combat_screen.h"
#include "bundle_select_screen.h"
#include "confirm_popup.h"
#include "pause_menu.h"
#include "pile_screen.h"
#include "screen_shake.h"
#include "settings_screen.h"
#include "compendium_screen.h"
#include "../scene/rich_text.h"
#include "card_select_screen.h"
#include "event_screen.h"
#include "game_over_screen.h"
#include "map_screen.h"
#include "potion_menu.h"
#include "reward_screen.h"
#include "rest_site_screen.h"
#include "rewards_screen.h"
#include "shop_screen.h"
#include "deck_previews.h"
#include "room_cues.h"
#include "top_bar.h"
#include "treasure_screen.h"
#include "crystal_sphere_screen.h"
#include "logic_link.h"
#include "loc.h"

#include <stdio.h>

namespace spire {

/* NTransition.RoomFadeOut/RoomFadeIn, normal speed. The scene's gradient
 * sweeps down after 0.5 s while the backdrop reaches black over 0.6 s.
 * The next room then fades in over 0.8 s. */
class RoomTransition : public Control {
public:
    explicit RoomTransition(SceneLibrary* lib) : phase(0), elapsed(0), initial_y(-2564), gradient_height(2280) {
        name = "RoomTransition";
        z_index = 4096;
        visible = false;
        const SceneFile* scene = lib->file("scenes/game.tscn");
        if (!scene) return;
        const Json& nodes = scene->doc["nodes"];
        for (size_t i = 0; i < nodes.size(); ++i) if (nodes[i]["name"].str() == "GradientTransition") {
            const Json& props = nodes[i]["props"];
            initial_y = props["offset_top"].f(-2564);
            gradient_height = 1080 + props["offset_bottom"].f(-1364) - initial_y;
            Resource* texture = lib->resolve(props["texture"], scene);
            GradientData::from(texture, gradient);
            if (texture && texture->props) {
                jv_vec2((*texture->props)["fill_from"], fill_from);
                jv_vec2((*texture->props)["fill_to"], fill_to);
            }
        }
    }
    int phase; /* 0 idle, 1 waiting to exchange rooms, 2 revealing the new room */
    double elapsed;
    float initial_y, gradient_height;
    Vec2 fill_from, fill_to;
    GradientData gradient;
    void start_out() { phase = 1; elapsed = 0; visible = true; }
    void start_in() { phase = 2; elapsed = 0; visible = true; }
    bool ready() const { return phase == 1 && elapsed >= 1.1; }
    virtual void process(double dt) {
        if (phase == 0) return;
        elapsed += dt;
        if (phase == 2 && elapsed >= 0.8) { phase = 0; visible = false; }
    }
    virtual void draw(Canvas& canvas) {
        if (!phase) return;
        float progress = clampf((float)((elapsed - 0.5) / 0.6), 0, 1);
        if (phase == 1 && gradient.valid()) {
            float y = initial_y * (1 - progress);
            std::vector<Vec2> vertices;
            std::vector<Color> colors;
            std::vector<int> indices;
            Vec2 direction = fill_to - fill_from;
            float length_sq = direction.x * direction.x + direction.y * direction.y;
            for (int i = 0; i <= 128; ++i) {
                float v = (float)i / 128;
                float t = length_sq > 0 ? ((0 - fill_from.x) * direction.x + (v - fill_from.y) * direction.y) / length_sq : 0;
                float alpha = gradient.at(clampf(t, 0, 1)).a;
                vertices.push_back(Vec2(0, y + v * gradient_height));
                vertices.push_back(Vec2(1920, y + v * gradient_height));
                colors.push_back(Color(0, 0, 0, alpha)); colors.push_back(Color(0, 0, 0, alpha));
                if (i) {
                    int k = (i - 1) * 2;
                    indices.push_back(k); indices.push_back(k + 1); indices.push_back(k + 3);
                    indices.push_back(k); indices.push_back(k + 3); indices.push_back(k + 2);
                }
            }
            canvas.draw_colored_triangles(vertices, indices, colors);
        }
        float alpha = phase == 1 ? progress : clampf(1 - (float)(elapsed / 0.8), 0, 1);
        canvas.draw_rect(Rect2(0, 0, 1920, 1080), Color(0, 0, 0, alpha));
    }
};

static std::string quoted(const std::string& s) {
    std::string out = "\"";
    for (size_t i = 0; i < s.size(); ++i) {
        unsigned char c = (unsigned char)s[i];
        if (c == '"' || c == '\\') {
            out += '\\';
            out += (char)c;
        } else if (c < 0x20) {
            out += format("\\u%04x", c);
        } else {
            out += (char)c;
        }
    }
    return out + "\"";
}

std::string protocol_action(const char* name, const std::string& args) {
    std::string s = std::string("{\"cmd\":\"action\",\"action\":\"") + name + "\"";
    if (!args.empty()) s += ",\"args\":{" + args + "}";
    return s + "}";
}

static std::string action(const char* name, const std::string& args = std::string()) {
    return protocol_action(name, args);
}

static Choice choice(const std::string& label, const std::string& command, int card = -1, int target = -1) {
    Choice c;
    c.label = label;
    c.command = command;
    c.card = card;
    c.target = target;
    return c;
}

std::vector<Choice> GameClient::choices(const Json& d) {
    std::vector<Choice> out;
    const std::string& kind = d["decision"].str();
    if (kind == "combat_play") {
        int energy = d["energy"].i();
        const Json& hand = d["hand"];
        const Json& enemies = d["enemies"];
        for (size_t i = 0; i < hand.size(); ++i) {
            const Json& c = hand[i];
            if (!c["can_play"].b() || c["cost"].i() > energy) continue;
            int idx = c["index"].i();
            if (c["target_type"].str() == "AnyEnemy") {
                for (size_t t = 0; t < enemies.size(); ++t)
                    out.push_back(choice(c["name"].str() + " -> " + enemies[t]["name"].str(),
                                         action("play_card", format("\"card_index\":%d,\"target_index\":%d", idx,
                                                                    enemies[t]["index"].i())),
                                         idx, enemies[t]["index"].i()));
            } else {
                out.push_back(choice(c["name"].str(), action("play_card", format("\"card_index\":%d", idx)), idx));
            }
        }
        const Json& potions = d["player"]["potions"];
        for (size_t i = 0; i < potions.size(); ++i) {
            const Json& p = potions[i];
            int idx = p["index"].i();
            if (p["target_type"].str() == "AnyEnemy") {
                for (size_t t = 0; t < enemies.size(); ++t)
                    out.push_back(choice(p["name"].str() + " -> " + enemies[t]["name"].str(),
                                         action("use_potion", format("\"potion_index\":%d,\"target_index\":%d", idx,
                                                                     enemies[t]["index"].i()))));
            } else {
                out.push_back(choice(p["name"].str(), action("use_potion", format("\"potion_index\":%d", idx))));
            }
        }
        out.push_back(choice("End turn", action("end_turn")));
    } else if (kind == "map_select") {
        const Json& cs = d["choices"];
        for (size_t i = 0; i < cs.size(); ++i)
            out.push_back(choice(format("%s (column %d, row %d)", cs[i]["type"].str().c_str(), cs[i]["col"].i(),
                                        cs[i]["row"].i()),
                                 action("select_map_node", format("\"col\":%d,\"row\":%d", cs[i]["col"].i(), cs[i]["row"].i()))));
    } else if (kind == "event_choice" || kind == "rest_site") {
        const Json& os = d["options"];
        for (size_t i = 0; i < os.size(); ++i) {
            if (os[i]["is_locked"].b() || !os[i]["is_enabled"].b(true)) continue;
            std::string label = os[i]["title"].is_string() ? os[i]["title"].str() : os[i]["option_id"].str();
            out.push_back(choice(label, action("choose_option", format("\"option_index\":%d", os[i]["index"].i()))));
        }
        if (out.empty()) out.push_back(choice("Leave", action("leave_room")));
    } else if (kind == "treasure") {
        const std::string& stage = d["stage"].str();
        if (stage == "closed") out.push_back(choice("Open the chest", action("open_chest")));
        const Json& rs = d["relics"];
        for (size_t i = 0; stage == "open" && i < rs.size(); ++i)
            out.push_back(choice(rs[i]["title"].str(), action("pick_relic", format("\"relic_index\":%d", rs[i]["index"].i()))));
        if (stage != "closed") out.push_back(choice(stage == "open" ? "Skip" : "Proceed", action("proceed")));
    } else if (kind == "rewards") {
        const Json& rs = d["rewards"];
        for (size_t i = 0; i < rs.size(); ++i)
            out.push_back(choice(rs[i]["label"].str() + (rs[i]["skipped"].b() ? " (skipped)" : ""),
                                 action("take_reward", format("\"reward_index\":%d", rs[i]["index"].i()))));
        if (d["can_skip"].b(true) || rs.size() == 0) out.push_back(choice("Proceed", action("proceed")));
    } else if (kind == "card_reward") {
        const Json& cs = d["cards"];
        for (size_t i = 0; i < cs.size(); ++i)
            out.push_back(choice(cs[i]["name"].str(), action("select_card_reward", format("\"card_index\":%d", (int)i))));
        if (d["can_skip"].b(true)) out.push_back(choice("Skip", action("skip_card_reward")));
    } else if (kind == "bundle_select") {
        const Json& bs = d["bundles"];
        for (size_t i = 0; i < bs.size(); ++i) {
            std::string label;
            for (size_t k = 0; k < bs[i]["cards"].size(); ++k)
                label += (k ? ", " : "") + bs[i]["cards"][k]["name"].str();
            out.push_back(choice(label, action("select_bundle", format("\"bundle_index\":%d", (int)i))));
        }
    } else if (kind == "card_select") {
        /* One card at a time for now (the plain screen); multi-select comes
         * with the ported screen. */
        const Json& cs = d["cards"];
        if (d["max_select"].i(1) >= 1)
            for (size_t i = 0; i < cs.size(); ++i)
                out.push_back(choice(cs[i]["name"].str(),
                                     action("select_cards", "\"indices\":" + quoted(format("%d", (int)i)))));
        if (d["min_select"].i(1) <= 0 || cs.size() == 0) out.push_back(choice("Skip", action("skip_select")));
    } else if (kind == "crystal_sphere") {
        const Json& cells = d["cells"];
        for (size_t i = 0; i < cells.size(); ++i)
            if (cells[i]["hidden"].b())
                out.push_back(choice(format("Cell %d, %d", cells[i]["x"].i(), cells[i]["y"].i()),
                                     action("crystal_sphere_click", format("\"x\":%d,\"y\":%d", cells[i]["x"].i(), cells[i]["y"].i()))));
    } else if (kind == "shop") {
        int gold = d["player"]["gold"].i();
        static const char* const kinds[3][3] = { { "cards", "buy_card", "card_index" },
                                                 { "relics", "buy_relic", "relic_index" },
                                                 { "potions", "buy_potion", "potion_index" } };
        for (int k = 0; k < 3; ++k) {
            const Json& items = d[kinds[k][0]];
            for (size_t i = 0; i < items.size(); ++i)
                if (items[i]["is_stocked"].b(true) && items[i]["cost"].i(1 << 30) <= gold)
                    out.push_back(choice(format("%s (%d gold)", items[i]["name"].str().c_str(), items[i]["cost"].i()),
                                         action(kinds[k][1], format("\"%s\":%d", kinds[k][2], items[i]["index"].i()))));
        }
        out.push_back(choice("Leave", action("leave_room")));
    } else if (kind != "game_over") {
        out.push_back(choice("Proceed", action("proceed")));
    }
    return out;
}

GameClient::GameClient(SceneLibrary* lib, SceneTree* tree, LogicLink* link)
    : lib_(lib), tree_(tree), link_(link), potions_(lib), screen_(NULL), combat_(NULL), ported_(NULL), room_(NULL), behind_(NULL), reward_focus_(0), shop_open_(false), shop_index_(0), hand_focus_(-1), target_(-1), last_target_(-1), focus_(0), waiting_(true), ready_(false), auto_(false), auto_until_(0), menu_requested_(false),
      rng_(1), decisions_(0), errors_(0), want_characters_(false), characters_version_(0), transition_(NULL), deck_previews_(NULL),
      answers_(0), pause_(NULL), pause_node_(NULL), piles_(NULL), piles_node_(NULL), map_view_(NULL),
      settings_version_(0), language_version_(0), settings_screen_(NULL), settings_node_(NULL), compendium_(NULL), compendium_node_(NULL),
      piles_button_(0), piles_asked_(false),
      confirm_(NULL), confirm_node_(NULL), quitting_(false), plain_(false) {}

GameClient::~GameClient() {
    if (compendium_node_) {
        if (compendium_node_->parent) compendium_node_->parent->remove_child(compendium_node_);
        delete compendium_node_;
    }
    delete compendium_;
    if (settings_node_) {
        if (settings_node_->parent) settings_node_->parent->remove_child(settings_node_);
        delete settings_node_;
    }
    delete settings_screen_;
    close_pause();
    close_piles();
    if (transition_) {
        if (transition_->parent) transition_->parent->remove_child(transition_);
        delete transition_;
    }
    if (deck_previews_) {
        Node* layer = deck_previews_->layer();
        if (layer->parent) layer->parent->remove_child(layer);
        delete layer;
        delete deck_previews_;
    }
    if (screen_) {
        screen_->parent->remove_child(screen_);
        delete screen_;
    }
    delete combat_;
    delete ported_;
    delete room_;
    delete behind_;
}

void GameClient::start_run(const std::string& character, const std::string& seed) {
    std::string cmd = "{\"cmd\":\"start_run\",\"character\":" + quoted(character) + ",\"seed\":" + quoted(seed) + "}";
    if (ready_) send(cmd);
    else pending_start_ = cmd; /* sent after the logic's "ready" */
}

void GameClient::start_game_run(const std::string& character, int ascension, const std::string& seed) {
    std::string cmd = "{\"cmd\":\"start_game_run\",\"character\":" + quoted(character) + format(",\"ascension\":%d", ascension);
    if (!seed.empty()) cmd += ",\"seed\":" + quoted(seed);
    queue_or_send(cmd + "}");
}

void GameClient::continue_run() { queue_or_send("{\"cmd\":\"continue_run\"}"); }

void GameClient::request(const std::string& command) { queue_or_send(command); }

bool GameClient::take_answer(Json* out) {
    if (unread_.empty()) return false;
    *out = unread_.front();
    unread_.erase(unread_.begin());
    return true;
}

void GameClient::queue_or_send(const std::string& command) {
    if (ready_) send(command);
    else queued_.push_back(command);
}

void GameClient::request_characters() {
    if (ready_) send("{\"cmd\":\"characters\"}");
    else want_characters_ = true; /* sent after the logic's "ready" */
}

void GameClient::send(const std::string& command) {
    if (ported_ && decision_["decision"].str() == "map_select") {
        std::string error;
        Json cmd = Json::parse(command.c_str(), command.size(), &error);
        if (cmd["action"].str() == "select_map_node") {
            if (lib_->audio()) lib_->audio()->ui("event:/sfx/ui/map/map_select");
            ((MapScreen*)ported_)->begin_travel(cmd["args"]["col"].i(), cmd["args"]["row"].i());
            start_room_transition();
        }
    }
    waiting_ = true;
    /* last_error: what the last command (not the retry after it) did */
    if (command != "{\"cmd\":\"get_state\"}") error_.clear();
    if (!link_->send(command)) error_ = "the logic is not running";
}

unsigned GameClient::next_rand() {
    rng_ = rng_ * 1103515245u + 12345u;
    return (rng_ >> 8) & 0xFFFFFF;
}

bool GameClient::busy() const {
    return waiting_ || !finishing_.is_null() || (combat_ && combat_->busy()) || (room_ && room_->busy()) ||
           (ported_ && ported_->busy()) || (transition_ && transition_->phase != 0);
}

void GameClient::start_room_transition() {
    if (!transition_) transition_ = new RoomTransition(lib_);
    if (!transition_->parent) tree_->root()->add_child(transition_);
    transition_->start_out();
}

void GameClient::update(const PadState& pad) {
    std::string line;
    while (link_->poll(line)) {
        if (line.empty() || line[0] != '{') continue; /* not protocol */
        std::string err;
        Json msg = Json::parse(line.c_str(), line.size(), &err);
        if (!err.empty()) {
            log_warn("logic: unreadable line (%s)", err.c_str());
            continue;
        }
        const std::string& type = msg["type"].str();
        if (type == "ready") {
            ready_ = true;
            waiting_ = false;
            if (want_characters_) {
                want_characters_ = false;
                send("{\"cmd\":\"characters\"}");
            }
            if (!pending_start_.empty()) {
                send(pending_start_);
                pending_start_.clear();
            }
            for (size_t i = 0; i < queued_.size(); ++i) send(queued_[i]);
            queued_.clear();
            request_settings();
        } else if (type == "characters") {
            waiting_ = false;
            characters_ = msg;
            ++characters_version_;
        } else if (type == "decision") {
            waiting_ = false;
            bool changed_room = !decision_.is_null() && msg["decision"].str() != "map_select" &&
                msg["context"]["total_floor"].i() != decision_["context"]["total_floor"].i();
            if ((!transition_ || transition_->phase == 0) && changed_room) start_room_transition();
            if (transition_ && transition_->phase == 1) transition_pending_ = msg;
            else show(msg);
        } else if (type == "settings") {
            apply_settings(msg);
        } else if (compendium_ && CompendiumScreen::is_answer(type)) {
            compendium_->set_answer(msg);
        } else if (type == "map_view") {
            waiting_ = false;
            if (piles_asked_) open_map_view(msg);
            piles_asked_ = false;
        } else if (type == "pile") {
            /* view_pile's answer: the pile screen (a query, not a step) */
            waiting_ = false;
            if (piles_asked_) open_piles(msg);
            piles_asked_ = false;
        } else if (type == "error") {
            error_ = msg["message"].str();
            log_warn("logic: %s", error_.c_str());
            if (decision_.is_null()) {
                /* A menu command failed: its answer. */
                waiting_ = false;
                answer_ = msg;
                ++answers_;
                unread_.push_back(msg);
            } else if (++errors_ < 25) {
                /* Ask for the current decision again (as random_runs does). */
                send("{\"cmd\":\"get_state\"}");
            } else {
                waiting_ = false;
            }
        } else {
            /* main_menu, profiles, timeline, ...: a menu command's answer. */
            waiting_ = false;
            answer_ = msg;
            ++answers_;
            unread_.push_back(msg);
            /* a profile's prefs came with its menu */
            if (type == "main_menu") request_settings();
            if (quitting_ && type == "main_menu") {
                quitting_ = false;
                menu_requested_ = true;
            }
        }
    }
    if (transition_ && transition_->phase == 1 && !transition_pending_.is_null() && transition_->ready()) {
        Json next = transition_pending_;
        transition_pending_ = Json();
        show(next);
        transition_->start_in();
        tree_->root()->remove_child(transition_); tree_->root()->add_child(transition_);
        return;
    }
    if (transition_ && transition_->phase != 0) return;
    if (!finishing_.is_null()) {
        /* The killing blow is playing: then the decision after it. */
        if ((room_ && room_->busy()) || (combat_ && combat_->busy())) return;
        Json d = finishing_;
        finishing_ = Json();
        d.set("steps", Json()); /* already played on the persistent room */
        show(d);
        return;
    }
    if (waiting_ || decision_.is_null()) return;
    if (combat_ && combat_->take_end_turn_long_press()) {
        send(action("end_turn"));
        return;
    }
    if (combat_ && combat_->busy()) return;
    if (room_ && room_->busy()) return;
    if (ported_ && ported_->busy()) return;
    if (auto_ && (auto_until_ <= 0 || decisions_ < auto_until_)) choose_automatically();
    else handle_input(pad);
}

/* The finished Crystal Sphere under its rewards (behind_), changing from
 * the state before. */
Node* GameClient::crystal_sphere_behind(const Json& sphere, const Json& before) {
    std::string err;
    CrystalSphereScreen* s = new CrystalSphereScreen(lib_);
    s->set_previous(before);
    behind_ = s;
    Node* n = s->build(sphere, &err);
    if (!n) {
        log_warn("crystal sphere screen: %s", err.c_str());
        delete behind_;
        behind_ = NULL;
    }
    return n;
}

/* What shows behind an overlay screen (rewards, card reward): the fight's
 * room after it (aftermath: the player's state as it is now), else the
 * event's room; NULL for none. */
/* NPlayerHand.SelectCards: a card_select from the hand runs on the fight
 * itself (the combat screen in its selection mode). */
class HandSelectScreen : public RunScreen {
public:
    explicit HandSelectScreen(CombatScreen* combat) : combat_(combat) {}
    virtual Node* build(const Json& d, std::string*) {
        combat_->begin_hand_select(d);
        return combat_->root_node();
    }
    virtual std::string input(const PadState& pad) { return combat_->hand_select_input(pad); }

private:
    CombatScreen* combat_;
};

Node* GameClient::overlay_background(const Json& d, bool aftermath) {
    std::string err;
    /* NCrystalSphereScreen stays under the rewards it gives, as it is. */
    if (d["crystal_sphere"].is_object()) return crystal_sphere_behind(d["crystal_sphere"], d["crystal_sphere"]);
    const std::string& rt = d["context"]["room_type"].str();
    bool in_fight = rt == "Monster" || rt == "Elite" || rt == "Boss";
    if (room_ && room_->root_node() && !room_->root_node()->parent) {
        if (aftermath) room_->show_aftermath(d["player"]);
        return room_->root_node();
    }
    if (in_fight && !last_combat_.is_null()) {
        room_ = new CombatScreen(lib_);
        if (aftermath) room_->set_aftermath(d["player"]);
        return room_->build(last_combat_, &err);
    }
    if (rt == "RestSite" && !last_rest_.is_null()) {
        /* The rest site as it was (the smith's upgrade, a relic's choice). */
        RestSiteScreen* r = new RestSiteScreen(lib_);
        r->set_passive();
        behind_ = r;
        Node* n = behind_->build(last_rest_, &err);
        if (!n) {
            delete behind_;
            behind_ = NULL;
        }
        return n;
    }
    if (rt == "Shop" && !last_shop_.is_null()) {
        /* The card removal: over the shop, its inventory open. */
        ShopScreen* sh = new ShopScreen(lib_);
        sh->set_state(true, "removal", 0);
        behind_ = sh;
        Node* n = behind_->build(last_shop_, &err);
        if (!n) {
            delete behind_;
            behind_ = NULL;
        }
        return n;
    }
    if (rt == "Event" && !last_event_.is_null()) {
        behind_ = new EventScreen(lib_);
        Node* n = behind_->build(last_event_, &err);
        if (!n) {
            delete behind_;
            behind_ = NULL;
        }
        return n;
    }
    return NULL;
}

void GameClient::show(const Json& d) {
    errors_ = 0; /* the retries after an error count per decision */
    /* The deck's changes (NRun.GlobalUi's previews) play over whatever
     * screen comes next. */
    if (!deck_previews_) deck_previews_ = new DeckPreviews(lib_);
    Node* previews = deck_previews_->layer();
    if (!previews->parent) tree_->root()->add_child(previews);
    deck_previews_->play(d["steps"], d["player"]["character"].str());
    std::string prev_kind = decision_["decision"].str();
    std::string prev_stage = decision_["stage"].str();
    Json prev_player = decision_["player"];
    const std::string& kind = d["decision"].str();
    if (combat_ && prev_kind == "combat_play" && kind == "combat_play") {
        if (lib_->audio()) lib_->audio()->decision(d);
        potions_.detach();
        decision_ = d;
        choices_ = choices(d);
        ++decisions_;
        last_combat_ = d;
        target_ = -1;
        combat_->update_decision(d);
        Node* old_bar = screen_->get_node("TopBarLayer");
        if (old_bar) { screen_->remove_child(old_bar); delete old_bar; }
        Json bar_state = d;
        bar_state.set("player", prev_player);
        Node* bar = make_top_bar(lib_, bar_state, false, prev_player);
        if (bar) screen_->add_child(bar);
        potions_.attach(screen_, d["player"]);
        return;
    }
    if (combat_ && prev_kind == "combat_play" && (kind == "rewards" || kind == "game_over") && d["steps"].size() > 0) {
        /* Keep the actual room through its killing blow, including deaths
         * caused by powers and orbs. Open the overlay after playback. */
        combat_->update_decision(d, true);
        finishing_ = d;
        return;
    }
    if (combat_ && prev_kind == "combat_play" && kind == "card_select" && d["steps"].size() > 0) {
        combat_->play_steps(d["steps"]);
        finishing_ = d;
        return;
    }
    if (lib_->audio()) {
        lib_->audio()->decision(d);
        if (kind != "combat_play") lib_->audio()->steps(d["steps"]);
        if (kind == "map_select" && prev_kind != "map_select") lib_->audio()->ui("event:/sfx/ui/map/map_open");
    }
    potions_.detach();
    /* NOverlayStack keeps the current combat room behind rewards and
     * selections. Detach it before deleting the old overlay, then reparent
     * that same room into the next screen. */
    CombatScreen* kept_room = NULL;
    bool event_room = prev_kind == "event_choice" && decision_["layout"].str() == "Combat" && ported_ &&
        ((kind == "event_choice" && d["event_id"].str() == decision_["event_id"].str()) ||
         (kind == "combat_play" && d["view"]["encounter"] == decision_["combat_visuals"]["view"]["encounter"]));
    if (event_room) kept_room = ((EventScreen*)ported_)->take_combat();
    const std::string& room_type = d["context"]["room_type"].str();
    bool in_fight = room_type == "Monster" || room_type == "Elite" || room_type == "Boss";
    bool combat_screen = kind == "combat_play" || kind == "card_select" || kind == "rewards" || kind == "card_reward";
    if (((in_fight && combat_screen) || kind == "game_over") && (combat_ || room_)) {
        kept_room = combat_ ? combat_ : room_;
        Node* room_node = kept_room->root_node();
        if (room_node && room_node->parent) room_node->parent->remove_child(room_node);
        if (screen_ == room_node) screen_ = NULL;
        if (kept_room == combat_) combat_ = NULL;
        if (kept_room == room_) room_ = NULL;
        Node* bar = room_node ? room_node->get_node("TopBarLayer") : NULL;
        if (bar) { room_node->remove_child(bar); delete bar; }
    }
    /* A selection from the hand ends with its decision */
    if (kept_room) kept_room->end_hand_select();
    if (prev_kind == "rewards" && ported_) reward_focus_ = ((RewardsScreen*)ported_)->focused();
    if (prev_kind == "shop" && ported_) {
        ShopScreen* shop = (ShopScreen*)ported_;
        shop_open_ = shop->inventory_open();
        shop_kind_ = shop->focused_kind();
        shop_index_ = shop->focused_index();
    }
    decision_ = d;
    choices_ = choices(d);
    focus_ = 0;
    ++decisions_;
    if (screen_) {
        screen_->parent->remove_child(screen_);
        delete screen_;
        screen_ = NULL;
    }
    delete combat_;
    combat_ = NULL;
    delete ported_;
    ported_ = NULL;
    delete room_;
    room_ = kept_room;
    delete behind_;
    behind_ = NULL;
    target_ = -1;
    std::string err;
    if (kind == "rewards") {
        /* Over the room; a new rewards screen continues the last one (a
         * reward taken) without its slide-in; back from the card reward
         * screen it slides in again (AfterOverlayShown). */
        RewardsScreen* s = new RewardsScreen(lib_);
        if (d["crystal_sphere"].is_object() && prev_kind == "crystal_sphere") {
            /* Just after the last divination: what it revealed, and the
             * rewards 0.75 s later (CompleteMinigame's Cmd.Wait). */
            s->set_delay(0.75);
            s->set_background(crystal_sphere_behind(d["crystal_sphere"], last_crystal_));
        } else {
            s->set_background(overlay_background(d, true));
        }
        bool again = prev_kind == "rewards" || prev_kind == "card_reward";
        s->set_continuing(prev_kind != "rewards", again ? reward_focus_ : 0);
        ported_ = s;
        screen_ = s->build(d, &err);
        if (!screen_) {
            log_warn("rewards screen: %s", err.c_str());
            delete ported_;
            ported_ = NULL;
        }
    }
    if (kind == "card_reward") {
        /* Over the room (the fight's or the event's). */
        CardRewardScreen* s = new CardRewardScreen(lib_);
        s->set_background(overlay_background(d, true));
        ported_ = s;
        screen_ = s->build(d, &err);
        if (!screen_) {
            log_warn("card reward screen: %s", err.c_str());
            delete ported_;
            ported_ = NULL;
        }
    }
    const std::string& select_kind = d["kind"].str();
    if (kind == "card_select" && room_ && (select_kind == "hand" || select_kind == "hand_upgrade")) {
        /* NPlayerHand.SelectCards: on the fight itself */
        ported_ = new HandSelectScreen(room_);
        screen_ = ported_->build(d, &err);
    } else if (kind == "card_select") {
        /* In a fight, over the fight (as it is); at the rest site or the
         * shop, over the room. */
        const std::string& rt = d["context"]["room_type"].str();
        bool in_fight = rt == "Monster" || rt == "Elite" || rt == "Boss";
        CardSelectScreen* s = new CardSelectScreen(lib_);
        s->set_background(in_fight || rt == "RestSite" || rt == "Shop" ? overlay_background(d, false) : NULL);
        ported_ = s;
        screen_ = s->build(d, &err);
        if (!screen_) {
            log_warn("card select screen: %s", err.c_str());
            delete ported_;
            ported_ = NULL;
        }
    }
    if (kind == "bundle_select") {
        /* Over the event's room. */
        BundleSelectScreen* s = new BundleSelectScreen(lib_);
        s->set_background(overlay_background(d, false));
        ported_ = s;
        screen_ = s->build(d, &err);
        if (!screen_) {
            log_warn("bundle select screen: %s", err.c_str());
            delete ported_;
            ported_ = NULL;
        }
    }
    if (kind == "event_choice") {
        last_event_ = d;
        EventScreen* event = new EventScreen(lib_);
        if (event_room) { event->set_combat(room_); room_ = NULL; }
        ported_ = event;
        screen_ = ported_->build(d, &err);
        if (!screen_) {
            log_warn("event screen: %s", err.c_str());
            delete ported_;
            ported_ = NULL;
        }
    }
    if (kind == "rest_site") {
        /* Just chosen (from the room, or after the smith's selection): the
         * choices fade out and the fire goes out. */
        RestSiteScreen* s = new RestSiteScreen(lib_);
        if ((prev_kind == "rest_site" || prev_kind == "card_select") && last_rest_["options"].size() > 0)
            s->set_after_choice(last_rest_);
        last_rest_ = d;
        ported_ = s;
        screen_ = s->build(d, &err);
        if (!screen_) {
            log_warn("rest site screen: %s", err.c_str());
            delete ported_;
            ported_ = NULL;
        }
    }
    if (kind == "treasure") {
        /* The change from the stage before (closed -> open -> done) is
         * animated. */
        TreasureScreen* s = new TreasureScreen(lib_);
        if (prev_kind == "treasure") s->set_previous_stage(prev_stage);
        ported_ = s;
        screen_ = s->build(d, &err);
        if (!screen_) {
            log_warn("treasure screen: %s", err.c_str());
            delete ported_;
            ported_ = NULL;
        }
    }
    if (kind == "crystal_sphere") {
        CrystalSphereScreen* s = new CrystalSphereScreen(lib_);
        if (prev_kind == "crystal_sphere") s->set_previous(last_crystal_);
        last_crystal_ = d;
        ported_ = s;
        screen_ = s->build(d, &err);
        if (!screen_) {
            log_warn("crystal sphere screen: %s", err.c_str());
            delete ported_;
            ported_ = NULL;
        }
    }
    if (kind == "map_select") {
        ported_ = new MapScreen(lib_);
        screen_ = ported_->build(d, &err);
        if (!screen_) {
            log_warn("map screen: %s", err.c_str());
            delete ported_;
            ported_ = NULL;
        }
    }
    if (kind == "shop") {
        last_shop_ = d;
        /* After a purchase (or the card removal's selection) the inventory
         * stays open where it was. */
        ShopScreen* s = new ShopScreen(lib_);
        bool again = prev_kind == "shop" || prev_kind == "card_select";
        s->set_state(again && shop_open_, shop_kind_, shop_index_);
        ported_ = s;
        screen_ = s->build(d, &err);
        if (!screen_) {
            log_warn("shop screen: %s", err.c_str());
            delete ported_;
            ported_ = NULL;
        }
    }
    if (kind == "game_over") {
        GameOverScreen* s = new GameOverScreen(lib_);
        s->set_background(overlay_background(d, true));
        ported_ = s;
        screen_ = s->build(d, &err);
        if (!screen_) {
            log_warn("game over screen: %s", err.c_str());
            delete ported_;
            ported_ = NULL;
        }
    }
    if (kind == "combat_play") {
        bool continuing = room_ != NULL;
        if (prev_kind != "combat_play" && !continuing) enemy_places_.clear(); /* a new fight */
        Json before = prev_kind == "combat_play" ? last_combat_ : Json();
        last_combat_ = d;
        if (continuing) {
            combat_ = room_;
            room_ = NULL;
            screen_ = combat_->root_node();
            combat_->update_decision(d);
            if (event_room) combat_->show_start_banner();
        } else {
            combat_ = new CombatScreen(lib_);
            combat_->set_hand_focus(hand_focus_);
            combat_->set_layout_memory(&enemy_places_);
            combat_->set_previous(before);
            screen_ = combat_->build(d, &err);
        }
        if (!screen_) {
            log_warn("combat screen: %s", err.c_str());
            delete combat_;
            combat_ = NULL;
        } else if (!continuing && prev_kind == "combat_play") {
            /* What the last command did: the cards' effects, the enemies'
             * turn. */
            combat_->play_steps(d["steps"]);
        } else if (!continuing) {
            combat_->show_start_banner();
        }
    }
    plain_ = !screen_;
    if (!screen_) screen_ = plain_screen(d);
    /* The top bar over every screen of the run. */
    if (d["player"].is_object() && kind != "game_over") {
        Node* bar = make_top_bar(lib_, d, d["context"]["room_type"].str() == "Map", prev_player);
        if (bar) screen_->add_child(bar);
    }
    /* The room's screen shake target, unless a fight's room holds it. */
    if (ported_ && ported_->shake_target() && !ScreenShake::get().target())
        screen_->add_child(make_shake_driver(ported_->shake_target()));
    /* What the choice did outside a fight (the fights play their own). */
    if (!in_fight && kind != "game_over") play_room_cues(lib_, screen_, d);
    potions_.attach(screen_, d["player"]);
    tree_->root()->add_child(screen_);
    if (deck_previews_) {
        Node* previews = deck_previews_->layer();
        tree_->root()->remove_child(previews);
        tree_->root()->add_child(previews);
    }
    if (transition_ && transition_->parent) {
        tree_->root()->remove_child(transition_); tree_->root()->add_child(transition_);
    }
    refresh_plain_screen();
}

void GameClient::clear_screen() {
    if (lib_->audio()) lib_->audio()->menu();
    close_pause();
    quitting_ = false;
    if (transition_) { transition_->phase = 0; transition_->visible = false; }
    transition_pending_ = Json();
    potions_.detach();
    if (deck_previews_) deck_previews_->clear();
    if (screen_) {
        if (screen_->parent) screen_->parent->remove_child(screen_);
        delete screen_;
        screen_ = NULL;
    }
    delete combat_;
    combat_ = NULL;
    delete ported_;
    ported_ = NULL;
    delete room_;
    room_ = NULL;
    delete behind_;
    behind_ = NULL;
    decision_ = Json();
    choices_.clear();
    menu_requested_ = false;
    last_combat_ = Json();
    last_event_ = Json();
    last_rest_ = Json();
    last_shop_ = Json();
    finishing_ = Json();
    enemy_places_.clear();
}

void GameClient::pick(size_t i) {
    if (i >= choices_.size()) return;
    send(choices_[i].command);
}

/* Like tools/random_runs.py: mostly play cards, then end the turn. */
void GameClient::choose_automatically() {
    if (decision_["decision"].str() == "card_select") {
        int available = (int)decision_["cards"].size();
        int count = decision_["min_select"].i(1);
        if (count < 0) count = 0;
        if (count > available) count = available;
        if (count == 0) { send(action("skip_select")); return; }
        std::vector<int> indices;
        for (int i = 0; i < available; ++i) indices.push_back(i);
        std::string selected;
        for (int i = 0; i < count; ++i) {
            size_t pick = next_rand() % indices.size();
            int index = indices[pick];
            selected += format(i ? ",%d" : "%d", decision_["cards"][(size_t)index]["index"].i(index));
            indices.erase(indices.begin() + (long)pick);
        }
        send(action("select_cards", "\"indices\":" + quoted(selected)));
        return;
    }
    if (game_over()) {
        /* The game over screen's buttons as soon as each is enabled. */
        PadState a;
        a.pressed = a.held = PadState::A;
        handle_input(a);
        return;
    }
    if (choices_.empty()) return;
    size_t n = choices_.size();
    if (decision_["decision"].str() == "combat_play" && n > 1 && next_rand() % 10 != 0) n -= 1; /* not "End turn" */
    pick(next_rand() % n);
}

/* Combat with a pad, as the game plays it (NPlayerHand, NControllerCardPlay):
 * the hand has the focus (left / right); A takes the focused card to the
 * centre: a single-enemy card aims at an enemy (the last one aimed at, else
 * the first; left / right change it), other cards show what they hit; A
 * again plays it, B puts it back. Y ends the turn. */
void GameClient::combat_input(const PadState& pad) {
    const Json& hand = decision_["hand"];
    const Json& enemies = decision_["enemies"];
    int n = combat_->hand_size();
    int ne = (int)enemies.size();
    int focus = combat_->hand_focus() < 0 ? n / 2 : combat_->hand_focus();
    if (focus >= n) focus = n - 1;
    if (combat_->playing()) {
        const Json& card = hand[(size_t)focus];
        bool single = card["target_type"].str() == "AnyEnemy";
        if (single && ne > 0 && (pad.pressed & (PadState::LEFT | PadState::RIGHT))) {
            /* Focus moves between the targets' hitboxes (no wrap). */
            int step = (pad.pressed & PadState::LEFT) ? -1 : 1;
            for (int t = target_ + step; t >= 0 && t < ne; t += step)
                if (combat_->set_play_target(t)) {
                    target_ = t;
                    break;
                }
        }
        if (pad.pressed & PadState::B) {
            combat_->end_play();
            target_ = -1;
        } else if (pad.pressed & PadState::A) {
            /* NCardPlay.TryPlayCard: LastTargetedCreature. */
            const std::string& type = card["target_type"].str();
            if (single) last_target_ = enemies[(size_t)target_]["combat_id"].i(-1);
            else if (type == "AllEnemies" || type == "AllAllies") last_target_ = -1;
            if (single)
                send(action("play_card", format("\"card_index\":%d,\"target_index\":%d", card["index"].i(),
                                                enemies[(size_t)target_]["index"].i())));
            else
                send(action("play_card", format("\"card_index\":%d", card["index"].i())));
        }
        return;
    }
    if (combat_->creatures_focused()) {
        /* NCreature hitboxes: left / right between them, down or B back;
         * the end turn button still answers */
        if (pad.pressed & (PadState::LEFT | PadState::RIGHT)) {
            combat_->move_creature_focus((pad.pressed & PadState::LEFT) ? -1 : 1);
            return;
        }
        if (pad.pressed & (PadState::DOWN | PadState::B)) {
            combat_->unfocus_creatures();
            return;
        }
        if (pad.pressed & (PadState::A | PadState::UP)) return;
    }
    if (pad.pressed & PadState::UP) {
        /* CardHolderContainer.FocusNeighborTop: the creatures */
        combat_->focus_creatures();
        return;
    }
    if (n > 0 && (pad.pressed & (PadState::LEFT | PadState::RIGHT))) {
        focus = (pad.pressed & PadState::LEFT) ? (focus + n - 1) % n : (focus + 1) % n;
        hand_focus_ = focus;
        combat_->set_hand_focus(focus);
    }
    if ((pad.pressed & PadState::A) && focus >= 0 && focus < n) {
        const Json& card = hand[(size_t)focus];
        /* NControllerCardPlay.Start: an unplayable card stays in the hand. */
        if (!card["can_play"].b() || card["cost"].i() > decision_["energy"].i()) return;
        const std::string& type = card["target_type"].str();
        if (type == "AnyEnemy") {
            if (ne == 0) return;
            target_ = 0;
            for (int i = 0; i < ne; ++i)
                if (enemies[(size_t)i]["combat_id"].i(-2) == last_target_) target_ = i;
        } else if (type == "AnyAlly") {
            return; /* no other player to aim at */
        } else {
            target_ = -1;
        }
        combat_->begin_play(focus, type, target_);
        if (type == "AnyEnemy" && !combat_->set_play_target(target_)) {
            /* Not shown (its scene failed to load): the first that is. */
            for (int t = 0; t < ne; ++t)
                if (combat_->set_play_target(t)) {
                    target_ = t;
                    break;
                }
        }
    } else if (client_settings()["long_press"].b()) {
        /* Long press confirmation: Y held until the bar fills */
        combat_->hold_end_turn((pad.pressed & PadState::Y) != 0, (pad.held & PadState::Y) != 0);
    } else if (pad.pressed & PadState::Y) {
        send(action("end_turn"));
    }
}

/* NTopBarPauseButton (the pause hotkey): the pause menu over the run, in
 * NCapstoneContainer. Screens where Start means back keep it. */
void GameClient::open_pause() {
    std::string err;
    pause_ = new PauseMenu(lib_);
    pause_node_ = pause_->build(!game_over(), &err);
    if (!pause_node_) {
        log_warn("pause menu: %s", err.c_str());
        delete pause_;
        pause_ = NULL;
        return;
    }
    /* NCapstoneSubmenuStack.ShowScreen. */
    if (lib_->audio()) lib_->audio()->ui("event:/sfx/ui/pause_open");
    tree_->root()->add_child(pause_node_);
}

void GameClient::close_pause() {
    if (confirm_node_) {
        if (confirm_node_->parent) confirm_node_->parent->remove_child(confirm_node_);
        delete confirm_node_;
        confirm_node_ = NULL;
    }
    delete confirm_;
    confirm_ = NULL;
    if (pause_node_) {
        if (pause_node_->parent) pause_node_->parent->remove_child(pause_node_);
        delete pause_node_;
        pause_node_ = NULL;
    }
    delete pause_;
    pause_ = NULL;
}

/* The pause menu's input while it is open; true when it took the pad. */
bool GameClient::pause_input(const PadState& pad) {
    if (settings_input(pad)) return true;
    if (!pause_) return false;
    if (confirm_) {
        /* NAbandonRunConfirmPopup: Confirm gives up (RunManager.Abandon). */
        ConfirmPopup::Action a = confirm_->input(pad);
        if (a == ConfirmPopup::YES) {
            close_pause();
            send("{\"cmd\":\"give_up\"}");
        } else if (a == ConfirmPopup::NO) {
            if (confirm_node_->parent) confirm_node_->parent->remove_child(confirm_node_);
            delete confirm_node_;
            confirm_node_ = NULL;
            delete confirm_;
            confirm_ = NULL;
        }
        return true;
    }
    PauseMenu::Action a = pause_->input(pad);
    if (a == PauseMenu::RESUME) {
        if (lib_->audio()) lib_->audio()->ui("event:/sfx/ui/pause_close");
        close_pause();
    } else if (a == PauseMenu::GIVE_UP) {
        std::string err;
        confirm_ = new ConfirmPopup(lib_);
        confirm_node_ = confirm_->build(&err);
        if (confirm_node_) tree_->root()->add_child(confirm_node_);
        else {
            log_warn("abandon popup: %s", err.c_str());
            delete confirm_;
            confirm_ = NULL;
        }
    } else if (a == PauseMenu::SETTINGS) {
        std::string err;
        settings_screen_ = new SettingsScreen(lib_);
        Json s = settings_;
        s.set("in_run", Json::make_bool(true));
        settings_node_ = settings_screen_->build(s, &err);
        if (settings_node_) tree_->root()->add_child(settings_node_);
        else {
            log_warn("settings: %s", err.c_str());
            delete settings_screen_;
            settings_screen_ = NULL;
        }
    } else if (a == PauseMenu::COMPENDIUM) {
        std::string err;
        compendium_ = new CompendiumScreen(lib_);
        compendium_node_ = compendium_->build(lower(decision_["player"]["character"].str()), &err);
        if (compendium_node_) {
            tree_->root()->add_child(compendium_node_);
            send_query(CompendiumScreen::first_query());
        } else {
            log_warn("compendium: %s", err.c_str());
            delete compendium_;
            compendium_ = NULL;
        }
    } else if (a == PauseMenu::SAVE_AND_QUIT) {
        /* NPauseMenu.CloseToMenu: the run was saved as its room was
         * entered; the logic lets it go and answers with the menu. */
        close_pause();
        if (lib_->audio()) lib_->audio()->menu();
        quitting_ = true;
        send("{\"cmd\":\"return_to_menu\"}");
    }
    return true;
}

/* ---- the settings ---- */

/* Asked of the logic past the command queue (an answer of its own type,
 * no decision waits on it). */
void GameClient::request_settings() { send_query("{\"cmd\":\"settings\"}"); }

void GameClient::send_query(const std::string& command) {
    if (link_) link_->send(command);
}

void GameClient::apply_settings(const Json& s) {
    settings_ = s;
    /* LocManager.SetLanguage: the logic's language is the client's */
    const std::string& lang = s["language"].str();
    if (!lang.empty() && lang != Loc::language()) {
        Loc::set_language(lang);
        ++language_version_;
    }
    set_client_settings(s);
    set_rtl_text_effects(s["text_effects"].b(true));
    if (combat_) combat_->refresh_hand_indices();
    ++settings_version_;
    if (lib_->audio()) {
        AudioEvents& e = lib_->audio()->events();
        float m = s["master"].f(0.5f), b = s["bgm"].f(0.5f), x = s["sfx"].f(0.5f), a = s["ambience"].f(0.5f);
        e.bus_volume("bus:/", m * m);
        e.bus_volume("bus:/Music", b * b);
        e.bus_volume("bus:/SFX", x * x);
        e.bus_volume("bus:/Ambience", a * a);
    }
    /* NScreenshakePaginator.GetShakeMultiplier */
    static const float shake[5] = { 0, 0.5f, 1, 2, 4 };
    int k = s["screenshake"].i(2);
    ScreenShake::get().set_multiplier(k >= 0 && k < 5 ? shake[k] : (float)k);
    if (settings_screen_) settings_screen_->set_settings(s);
    if (combat_) combat_->update_phobia_mode();
    if (room_) room_->update_phobia_mode();
}

/* NSettingsScreen over the run (the pause menu's Settings); true when it
 * took the pad. */
bool GameClient::settings_input(const PadState& pad) {
    if (compendium_) {
        /* the pause menu's Compendium */
        std::string cmd;
        CompendiumScreen::Action a = compendium_->input(pad, &cmd);
        if (!cmd.empty()) send_query(cmd);
        if (a == CompendiumScreen::BACK) {
            if (compendium_node_->parent) compendium_node_->parent->remove_child(compendium_node_);
            delete compendium_node_;
            compendium_node_ = NULL;
            delete compendium_;
            compendium_ = NULL;
        }
        return true;
    }
    if (!settings_screen_) return false;
    std::string cmd;
    SettingsScreen::Action a = settings_screen_->input(pad, &cmd);
    if (!cmd.empty() && link_) link_->send(cmd);
    if (a == SettingsScreen::BACK) {
        if (settings_node_) {
            if (settings_node_->parent) settings_node_->parent->remove_child(settings_node_);
            delete settings_node_;
            settings_node_ = NULL;
        }
        delete settings_screen_;
        settings_screen_ = NULL;
    }
    return true;
}

/* ---- the deck and pile screens ---- */

void GameClient::open_piles(const Json& msg) {
    close_piles();
    std::string err;
    piles_ = new PileScreen(lib_);
    piles_node_ = piles_->build(msg, &err);
    if (!piles_node_) {
        log_warn("pile screen: %s", err.c_str());
        delete piles_;
        piles_ = NULL;
        return;
    }
    tree_->root()->add_child(piles_node_);
}

void GameClient::close_piles() {
    if (piles_node_) {
        if (piles_node_->parent) piles_node_->parent->remove_child(piles_node_);
        delete piles_node_;
        piles_node_ = NULL;
    }
    delete piles_;
    piles_ = NULL;
    delete map_view_;
    map_view_ = NULL;
}

void GameClient::open_map_view(const Json& msg) {
    close_piles();
    std::string err;
    map_view_ = new MapScreen(lib_);
    piles_node_ = map_view_->build(msg, &err);
    if (!piles_node_) {
        log_warn("map view: %s", err.c_str());
        delete map_view_;
        map_view_ = NULL;
        return;
    }
    tree_->root()->add_child(piles_node_);
}

/* The pile screens' input while one is open, or the buttons that open
 * them: LB the deck (NTopBarDeckButton) on any run screen with the top
 * bar; LT, RT, RB the draw, discard and exhaust piles in combat
 * (NCombatPileButton). True when it took the pad. */
bool GameClient::piles_input(const PadState& pad) {
    if (piles_) {
        if (!piles_->input(pad, piles_button_)) {
            close_piles();
        }
        return true;
    }
    if (map_view_) {
        if (pad.pressed & (PadState::B | PadState::BACK)) {
            if (lib_->audio()) lib_->audio()->ui("event:/sfx/ui/map/map_close");
            close_piles();
        } else {
            map_view_->input(pad);
        }
        return true;
    }
    if (piles_asked_ || waiting_ || decision_.is_null() || game_over() || potions_.active()) return false;
    if (combat_ && (combat_->busy() || target_ >= 0)) return false;
    const std::string& kind = decision_["decision"].str();
    if (kind == "card_select" || kind == "card_reward") return false;
    const char* pile = NULL;
    unsigned button = 0;
    if ((pad.pressed & PadState::BACK) && kind != "map_select") {
        if (lib_->audio()) lib_->audio()->ui("event:/sfx/ui/map/map_open");
        piles_button_ = PadState::BACK;
        piles_asked_ = true;
        send("{\"cmd\":\"view_map\"}");
        return true;
    }
    if (pad.pressed & PadState::LB) {
        pile = "deck";
        button = PadState::LB;
    } else if (combat_ && (pad.pressed & PadState::LT)) {
        pile = "draw";
        button = PadState::LT;
    } else if (combat_ && (pad.pressed & PadState::RT)) {
        pile = "discard";
        button = PadState::RT;
    } else if (combat_ && (pad.pressed & PadState::RB) && decision_["exhaust_pile_count"].i() > 0) {
        pile = "exhaust";
        button = PadState::RB;
    }
    if (!pile) return false;
    if (lib_->audio()) lib_->audio()->ui("map_open.mp3");
    piles_button_ = button;
    piles_asked_ = true;
    send(format("{\"cmd\":\"view_pile\",\"pile\":\"%s\"}", pile));
    return true;
}

void GameClient::handle_input(const PadState& pad) {
    set_capstone_open(pause_ || piles_ || map_view_ || settings_screen_ || compendium_);
    if (pause_input(pad)) return;
    if (piles_input(pad)) return;
    /* The last command's effects play out first (the game takes no input
     * while its actions run). */
    if (combat_ && combat_->busy()) return;
    const std::string& kind = decision_["decision"].str();
    if ((pad.pressed & PadState::START) && !game_over() && kind != "card_select" && kind != "card_reward" &&
        !potions_.active() && !(combat_ && target_ >= 0)) {
        open_pause();
        return;
    }
    /* NButton's default click/hover sounds; hand cards have their own cues. */
    if (!combat_ && lib_->audio()) {
        if (pad.pressed & (PadState::UP | PadState::DOWN | PadState::LEFT | PadState::RIGHT)) lib_->audio()->ui("event:/sfx/ui/clicks/ui_hover");
        if (pad.pressed & PadState::A) lib_->audio()->ui("event:/sfx/ui/clicks/ui_click");
        if (pad.pressed & PadState::B) lib_->audio()->ui("event:/sfx/ui/clicks/ui_back");
    }
    /* The potions (X) on any screen with the top bar. */
    if (potions_.active()) {
        std::string cmd = potions_.input(pad, combat_, decision_["enemies"]);
        if (!cmd.empty()) send(cmd);
        return;
    }
    if ((pad.pressed & PadState::X) && !(combat_ && combat_->playing()) && potions_.open()) return;
    if (combat_) {
        combat_input(pad);
        return;
    }
    if (ported_) {
        std::string cmd = ported_->input(pad);
        if (!cmd.empty()) send(cmd);
        if (game_over() && ((GameOverScreen*)ported_)->done()) menu_requested_ = true;
        return;
    }
    if (game_over()) {
        if (pad.pressed & (PadState::A | PadState::START)) menu_requested_ = true;
        return;
    }
    if (choices_.empty()) return;
    size_t n = choices_.size();
    if (pad.pressed & (PadState::DOWN | PadState::RIGHT)) focus_ = (focus_ + 1) % n;
    if (pad.pressed & (PadState::UP | PadState::LEFT)) focus_ = (focus_ + n - 1) % n;
    if (pad.pressed & (PadState::UP | PadState::DOWN | PadState::LEFT | PadState::RIGHT)) refresh_plain_screen();
    if (pad.pressed & PadState::A) pick(focus_);
    else if ((pad.pressed & PadState::Y) && decision_["decision"].str() == "combat_play") pick(n - 1); /* End turn */
}

/* ---- the plain screen: the decision's choices as a list ---- */

static Control* full_rect(Control* c) {
    c->anchor[SIDE_LEFT] = c->anchor[SIDE_TOP] = 0;
    c->anchor[SIDE_RIGHT] = c->anchor[SIDE_BOTTOM] = 1;
    for (int i = 0; i < 4; ++i) c->offset[i] = 0;
    return c;
}

static Label* text(const std::string& s, float x, float y, int size) {
    Label* l = (Label*)SceneLibrary::create_node("Label");
    l->text = s;
    l->offset[SIDE_LEFT] = x;
    l->offset[SIDE_TOP] = y;
    l->offset[SIDE_RIGHT] = 1800;
    l->offset[SIDE_BOTTOM] = y + size * 1.5f;
    l->add_theme_font_size_override("font_size", size);
    return l;
}

Node* GameClient::plain_screen(const Json& d) {
    Control* root = full_rect((Control*)SceneLibrary::create_node("Control"));
    root->name = "PlainScreen";
    ColorRect* bg = (ColorRect*)SceneLibrary::create_node("ColorRect");
    full_rect(bg);
    bg->color = Color(0.08f, 0.09f, 0.11f);
    root->add_child(bg);
    std::string title = d["decision"].str();
    if (d["event_name"].is_string()) title += ": " + d["event_name"].str();
    const Json& ctx = d["context"];
    if (ctx.is_object()) title += format("   (act %d, floor %d)", ctx["act"].i(), ctx["floor"].i());
    if (title == "game_over") {
        /* The game over screen is not ported: the outcome, and A back to
         * the main menu (the console app's loop). */
        title = d["victory"].b() ? "Victory!" : format("Defeated on floor %d (act %d)", d["floor"].i(), d["act"].i(1));
        root->add_child(text(title, 120, 400, 64));
        root->add_child(text("A: main menu", 120, 520, 40));
        return root;
    }
    /* Under the top bar (80 px) and the relic row (68 px). */
    root->add_child(text(title, 120, 170, 40));
    for (size_t i = 0; i < choices_.size(); ++i) {
        Label* l = text(choices_[i].label, 160, 260 + 56.0f * (float)i, 32);
        l->name = format("Choice%d", (int)i);
        root->add_child(l);
    }
    return root;
}

void GameClient::refresh_plain_screen() {
    if (!screen_ || screen_->name != "PlainScreen") return;
    for (size_t i = 0; i < choices_.size(); ++i) {
        Node* n = screen_->get_node(format("Choice%d", (int)i));
        if (n && n->is_control())
            ((Control*)n)->add_theme_color_override("font_color", i == focus_ ? Color(0.94f, 0.78f, 0.32f)
                                                                                : Color(1, 0.965f, 0.886f));
    }
}

} /* namespace spire */
