#include "game_client.h"

#include "../scene/controls.h"
#include "../scene/node.h"
#include "../scene/scene.h"
#include "../scene/text_nodes.h"
#include "../scene/tree.h"
#include "combat_screen.h"
#include "card_select_screen.h"
#include "event_screen.h"
#include "game_over_screen.h"
#include "map_screen.h"
#include "potion_menu.h"
#include "reward_screen.h"
#include "rewards_screen.h"
#include "shop_screen.h"
#include "top_bar.h"
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
    : lib_(lib), tree_(tree), link_(link), potions_(lib), screen_(NULL), combat_(NULL), ported_(NULL), room_(NULL), behind_(NULL), reward_focus_(0), shop_open_(false), shop_index_(0), hand_focus_(-1), target_(-1), last_target_(-1), focus_(0), waiting_(true), ready_(false), auto_(false), menu_requested_(false),
      rng_(1), decisions_(0), errors_(0), want_characters_(false), characters_version_(0) {}

GameClient::~GameClient() {
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

void GameClient::request_characters() {
    if (ready_) send("{\"cmd\":\"characters\"}");
    else want_characters_ = true; /* sent after the logic's "ready" */
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
            if (want_characters_) {
                want_characters_ = false;
                send("{\"cmd\":\"characters\"}");
            }
            if (!pending_start_.empty()) {
                send(pending_start_);
                pending_start_.clear();
            }
        } else if (type == "characters") {
            waiting_ = false;
            characters_ = msg;
            ++characters_version_;
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

/* What shows behind an overlay screen (rewards, card reward): the fight's
 * room after it (aftermath: the player's state as it is now), else the
 * event's room; NULL for none. */
Node* GameClient::overlay_background(const Json& d, bool aftermath) {
    std::string err;
    const std::string& rt = d["context"]["room_type"].str();
    bool in_fight = rt == "Monster" || rt == "Elite" || rt == "Boss";
    if (in_fight && !last_combat_.is_null()) {
        room_ = new CombatScreen(lib_);
        if (aftermath) room_->set_aftermath(d["player"]);
        return room_->build(last_combat_, &err);
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
    potions_.detach();
    std::string prev_kind = decision_["decision"].str();
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
    room_ = NULL;
    delete behind_;
    behind_ = NULL;
    target_ = -1;
    std::string err;
    const std::string& kind = d["decision"].str();
    if (kind == "rewards") {
        /* Over the room; a new rewards screen continues the last one (a
         * reward taken) without its slide-in; back from the card reward
         * screen it slides in again (AfterOverlayShown). */
        RewardsScreen* s = new RewardsScreen(lib_);
        s->set_background(overlay_background(d, true));
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
    if (kind == "card_select") {
        /* In a fight, over the fight (as it is). */
        const std::string& rt = d["context"]["room_type"].str();
        bool in_fight = rt == "Monster" || rt == "Elite" || rt == "Boss";
        CardSelectScreen* s = new CardSelectScreen(lib_);
        s->set_background(in_fight ? overlay_background(d, false) : NULL);
        ported_ = s;
        screen_ = s->build(d, &err);
        if (!screen_) {
            log_warn("card select screen: %s", err.c_str());
            delete ported_;
            ported_ = NULL;
        }
    }
    if (kind == "event_choice") {
        last_event_ = d;
        ported_ = new EventScreen(lib_);
        screen_ = ported_->build(d, &err);
        if (!screen_) {
            log_warn("event screen: %s", err.c_str());
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
        last_combat_ = d;
        combat_ = new CombatScreen(lib_);
        combat_->set_hand_focus(hand_focus_);
        screen_ = combat_->build(d, &err);
        if (!screen_) {
            log_warn("combat screen: %s", err.c_str());
            delete combat_;
            combat_ = NULL;
        }
    }
    if (!screen_) screen_ = plain_screen(d);
    /* The top bar over every screen of the run. */
    if (d["player"].is_object() && kind != "game_over") {
        Node* bar = make_top_bar(lib_, d, d["context"]["room_type"].str() == "Map");
        if (bar) screen_->add_child(bar);
    }
    potions_.attach(screen_, d["player"]);
    tree_->root()->add_child(screen_);
    refresh_plain_screen();
}

void GameClient::clear_screen() {
    potions_.detach();
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
    } else if (pad.pressed & PadState::Y) {
        send(action("end_turn"));
    }
}

void GameClient::handle_input(const PadState& pad) {
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
