#include "game_client.h"

#include "../scene/controls.h"
#include "../scene/node.h"
#include "../scene/scene.h"
#include "../scene/text_nodes.h"
#include "../scene/tree.h"
#include "combat_screen.h"
#include "logic_link.h"

namespace spire {

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

/* {"cmd":"action","action":name,"args":{args}}; args: "k":v pairs. */
static std::string action(const char* name, const std::string& args = std::string()) {
    std::string s = std::string("{\"cmd\":\"action\",\"action\":\"") + name + "\"";
    if (!args.empty()) s += ",\"args\":{" + args + "}";
    return s + "}";
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
    : lib_(lib), tree_(tree), link_(link), screen_(NULL), focus_(0), waiting_(true), ready_(false), auto_(false),
      rng_(1), decisions_(0), errors_(0) {}

GameClient::~GameClient() {
    if (screen_) {
        screen_->parent->remove_child(screen_);
        delete screen_;
    }
}

void GameClient::start_run(const std::string& character, const std::string& seed) {
    std::string cmd = "{\"cmd\":\"start_run\",\"character\":" + quoted(character) + ",\"seed\":" + quoted(seed) + "}";
    if (ready_) send(cmd);
    else pending_start_ = cmd; /* sent after the logic's "ready" */
}

void GameClient::send(const std::string& command) {
    waiting_ = true;
    if (!link_->send(command)) error_ = "the logic is not running";
}

unsigned GameClient::next_rand() {
    rng_ = rng_ * 1103515245u + 12345u;
    return (rng_ >> 8) & 0xFFFFFF;
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
            if (!pending_start_.empty()) {
                send(pending_start_);
                pending_start_.clear();
            }
        } else if (type == "decision") {
            waiting_ = false;
            show(msg);
        } else if (type == "error") {
            error_ = msg["message"].str();
            log_warn("logic: %s", error_.c_str());
            /* Ask for the current decision again (as random_runs does). */
            if (++errors_ < 25) send("{\"cmd\":\"get_state\"}");
            else waiting_ = false;
        } else {
            waiting_ = false;
        }
    }
    if (waiting_ || decision_.is_null()) return;
    if (auto_) choose_automatically();
    else handle_input(pad);
}

void GameClient::show(const Json& d) {
    decision_ = d;
    choices_ = choices(d);
    focus_ = 0;
    ++decisions_;
    if (screen_) {
        screen_->parent->remove_child(screen_);
        delete screen_;
        screen_ = NULL;
    }
    std::string err;
    if (d["decision"].str() == "combat_play") {
        CombatScreen cs(lib_);
        screen_ = cs.build(d, &err);
        if (!screen_) log_warn("combat screen: %s", err.c_str());
    }
    if (!screen_) screen_ = plain_screen(d);
    tree_->root()->add_child(screen_);
    refresh_plain_screen();
}

void GameClient::pick(size_t i) {
    if (i >= choices_.size()) return;
    send(choices_[i].command);
}

/* Like tools/random_runs.py: mostly play cards, then end the turn. */
void GameClient::choose_automatically() {
    if (choices_.empty()) return;
    size_t n = choices_.size();
    if (decision_["decision"].str() == "combat_play" && n > 1 && next_rand() % 10 != 0) n -= 1; /* not "End turn" */
    pick(next_rand() % n);
}

void GameClient::handle_input(const PadState& pad) {
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
    root->add_child(text(title, 120, 80, 40));
    for (size_t i = 0; i < choices_.size(); ++i) {
        Label* l = text(choices_[i].label, 160, 180 + 56.0f * (float)i, 32);
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
