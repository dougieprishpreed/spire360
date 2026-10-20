#include "run_history_screen.h"

#include "../scene/controls.h"
#include "../scene/rich_text.h"
#include "../scene/scene.h"
#include "../scene/value.h"
#include "inspect_screens.h"
#include "top_bar.h"

#include <math.h>
#include <stdlib.h>

namespace spire {

static Control* control_of(Node* n) { return n && n->is_control() ? (Control*)n : NULL; }

static const Color CREAM(1.0f, 0.964706f, 0.886275f, 1.0f);
static const float BASE_SCALE = 0.7f; /* NMapPointHistoryEntry._baseScale */

/* Rng.NextGaussianFloat(0, 1, 0, 5) on the chaotic rng: a normal sample
 * kept within [0, 1], scaled to [0, 5]. */
static float gaussian_angle() {
    double v;
    do {
        double d = 1.0 - (chaotic_rand() % 1000000u) / 1000000.0;
        double n = 1.0 - (chaotic_rand() % 1000000u) / 1000000.0;
        v = sqrt(-2.0 * log(d)) * cos(6.283185307179586 * n);
    } while (v < 0.0 || v > 1.0);
    return (float)(v * 5.0);
}

/* NMapPointHistory's %Acts (unique in its own scene) */
static Node* acts_of(Node* screen) {
    Node* h = screen ? screen->find_unique("MapPointHistory") : NULL;
    return h ? h->find_unique("Acts") : NULL;
}

static void free_children(Node* n) {
    if (!n) return;
    while (!n->children.empty()) {
        Node* c = n->children.back();
        n->remove_child(c);
        delete c;
    }
}

RunHistoryScreen::RunHistoryScreen(SceneLibrary* lib)
    : lib_(lib), root_(NULL), screen_(NULL), contents_(NULL), index_(0), count_(0), focus_(-1), tip_(NULL),
      tip_offset_y_(0), inspect_card_(NULL), inspect_relic_(NULL), inspect_node_(NULL) {}

/* The inspect node is under root_, freed with it by the owner. */
RunHistoryScreen::~RunHistoryScreen() {
    delete inspect_card_;
    delete inspect_relic_;
}

void RunHistoryScreen::close_inspect() {
    if (inspect_node_ && inspect_node_->parent) inspect_node_->parent->remove_child(inspect_node_);
    delete inspect_node_;
    delete inspect_card_;
    delete inspect_relic_;
    inspect_node_ = NULL;
    inspect_card_ = NULL;
    inspect_relic_ = NULL;
}

std::string RunHistoryScreen::query(int index) { return format("{\"cmd\":\"run_history\",\"index\":%d}", index); }

Node* RunHistoryScreen::build(std::string* error) {
    Node* screen = instance(lib_, "scenes/screens/run_history_screen/run_history.tscn");
    if (!screen) {
        if (error) *error = "no run_history.tscn";
        return NULL;
    }
    screen_ = screen;
    Control* root = full_rect_control("RunHistory");
    root_ = root;
    ColorRect* back = (ColorRect*)SceneLibrary::create_node("ColorRect");
    back->name = "Backstop";
    back->anchor[SIDE_RIGHT] = back->anchor[SIDE_BOTTOM] = 1;
    back->color = Color(0, 0, 0, 0.85f);
    root->add_child(back);
    root->add_child(screen);
    show_glyph(lib_, find_named(screen->find_unique("BackButton"), "HotkeyIcon"), "b");
    hide(screen->find_unique("ShareButton"));
    /* NRunHistoryArrowButton: the left one older (LB), the right one newer
     * (RB, its icon flipped) */
    Node* left = screen->get_node("LeftArrow");
    Node* right = screen->get_node("RightArrow");
    show_glyph(lib_, left ? left->find_unique("HotkeyIcon") : NULL, "lb");
    show_glyph(lib_, right ? right->find_unique("HotkeyIcon") : NULL, "rb");
    Node* ri = right ? right->get_node("TextureRect") : NULL;
    if (ri && ri->type == "TextureRect") ((TextureRect*)ri)->flip_h = true;
    hide(left);
    hide(right);
    contents_ = control_of(screen->get_node("ScreenContents"));
    scroll_.attach(contents_);
    scroll_.disable_if_fits();
    if (contents_) contents_->modulate.a = 0;
    root->add_child(frame_driver(this));
    return root;
}

void RunHistoryScreen::clear() {
    unfocus();
    tips_.hide();
    entries_.clear();
    static const char* const lists[] = { "PlayerIconContainer", "PotionHolders", "BadgeContainer", "RelicsContainer",
                                         "CardContainer" };
    free_children(acts_of(screen_));
    for (size_t i = 0; i < sizeof(lists) / sizeof(lists[0]); ++i) free_children(screen_->find_unique(lists[i]));
}

void RunHistoryScreen::set_data(const Json& msg) {
    if (!screen_) return;
    clear();
    data_ = msg;
    index_ = data_["index"].i();
    count_ = data_["count"].i();
    set_visible(screen_->get_node("LeftArrow"), index_ < count_ - 1);
    set_visible(screen_->get_node("RightArrow"), index_ > 0);
    bool ok = data_["ok"].b();
    set_visible(screen_->find_unique("OutOfDateVisual"), !ok);
    if (!ok) return;
    set_label(screen_->find_unique("DateLabel"), data_["date"].str());
    set_label(screen_->find_unique("SeedLabel"), data_["seed"].str());
    set_label(screen_->find_unique("GameModeLabel"), data_["game_mode"].str());
    set_label(screen_->find_unique("BuildLabel"), data_["build"].str());
    set_label(screen_->find_unique("RunTimeLabel"), data_["run_time"].str());
    set_label(screen_->find_unique("FloorNumLabel"), format("%d", data_["floor"].i()));
    set_label(screen_->find_unique("HpLabel"), data_["hp"].str());
    set_label(screen_->find_unique("GoldLabel"), data_["gold"].str());

    /* NRunHistoryPlayerIcon: the first player selected */
    Node* icons = screen_->find_unique("PlayerIconContainer");
    const Json& players = data_["players"];
    for (size_t i = 0; icons && i < players.size(); ++i) {
        Control* p = control_of(instance(lib_, "scenes/screens/run_history_screen/run_history_player_icon.tscn"));
        if (!p) continue;
        icons->add_child(p);
        Node* icon = p->find_unique("Icon");
        set_texture(lib_, icon, "images/ui/top_panel/character_icon_" + players[i]["character"].str() + ".png");
        int asc = players[i]["ascension"].i();
        set_label(p->find_unique("AscensionLabel"), asc > 0 ? format("%d", asc) : std::string());
        set_visible(p->find_unique("AchievementLock"), players[i]["achievements_locked"].b());
        hide(p->find_unique("SelectionReticle"));
        bool selected = i == 0;
        if (icon && icon->is_control()) {
            Control* ic = (Control*)icon;
            ic->shader_params["s"] = selected ? 1.0f : 0.3f;
            ic->shader_params["v"] = selected ? 1.0f : 0.55f;
            ic->scale = selected ? Vec2(1.1f, 1.1f) : Vec2(0.95f, 0.95f);
        }
        set_visible(p->find_unique("AscensionIcon"), selected && asc > 0);
        Entry e = { PLAYER, p, NULL, NULL, NULL, 0, 0, 0, &players[i] };
        entries_.push_back(e);
    }

    /* LoadGoldHpAndPotionInfo: the slots, the potions in them at 0.9 */
    Node* holders = screen_->find_unique("PotionHolders");
    const Json& potions = data_["potions"];
    int slots = data_["potion_slots"].i();
    for (int i = 0; holders && i < slots; ++i) {
        Node* h = instance(lib_, "scenes/potions/potion_holder.tscn");
        if (!h) continue;
        holders->add_child(h);
        hide(h->find_unique("SelectionReticle"));
        if (i < (int)potions.size()) {
            Control* pot = control_of(make_potion(lib_, potions[(size_t)i].str()));
            if (pot) {
                pot->scale = Vec2(0.9f, 0.9f);
                h->add_child(pot);
                Node* empty = h->find_unique("EmptyIcon");
                if (empty && empty->is_canvas_item()) ((CanvasItem*)empty)->modulate.a = 0;
            }
        }
    }

    /* LoadDeathQuote, LoadBadges */
    Node* quote = screen_->find_unique("DeathQuoteLabel");
    set_label(quote, data_["quote"].str());
    if (quote && quote->is_control() && data_["quote_color"].is_string())
        ((Control*)quote)->add_theme_color_override("default_color", data_["quote_color"].str() == "green"
                                                                         ? Color(0.498039f, 1, 0, 1)
                                                                         : Color(1, 0.33333334f, 0.33333334f, 1));
    Node* badges = screen_->find_unique("BadgeContainer");
    const Json& bl = data_["badges"];
    set_visible(badges, bl.size() > 0);
    for (size_t i = 0; badges && i < bl.size(); ++i) {
        Control* b = control_of(instance(lib_, "scenes/screens/game_over_screen/badge.tscn"));
        if (!b) continue;
        Node* holder = b->find_unique("BadgeHolder");
        set_texture(lib_, holder, bl[i]["base"].str());
        set_texture(lib_, b->find_unique("Icon"), bl[i]["icon"].str());
        hide(b->find_unique("SelectionReticle"));
        b->modulate = Color(1, 1, 1, 1);
        badges->add_child(b);
        Entry e = { BADGE, b, NULL, NULL, NULL, 0, 0, 0, &bl[i] };
        entries_.push_back(e);
    }

    /* NMapPointHistory.LoadHistory */
    Node* acts = acts_of(screen_);
    const Json& al = data_["acts"];
    for (size_t a = 0; acts && a < al.size(); ++a) {
        Node* row = instance(lib_, "scenes/screens/run_history_screen/act_history_entry.tscn");
        if (!row) continue;
        acts->add_child(row);
        set_label(row->find_unique("Title"), al[a]["title"].str());
        const Json& pts = al[a]["points"];
        for (size_t k = 0; k < pts.size(); ++k) {
            Control* p = control_of(instance(lib_, "scenes/screens/run_history_screen/map_point_history_entry.tscn"));
            if (!p) continue;
            row->add_child(p);
            Control* icon = control_of(p->find_unique("Icon"));
            Control* outline = control_of(p->find_unique("Outline"));
            if (pts[k]["icon"].is_string()) set_texture(lib_, icon, pts[k]["icon"].str());
            else hide(icon);
            if (pts[k]["outline"].is_string()) set_texture(lib_, outline, pts[k]["outline"].str());
            else hide(outline);
            set_visible(p->find_unique("QuestIcon"), pts[k]["quest"].b());
            float angle = gaussian_angle();
            if (chaotic_rand() % 2) angle = -angle;
            if (icon) icon->rotation = angle * 0.017453293f;
            Entry e = { POINT, p, icon, outline, NULL, angle * 0.017453293f, 0, pts[k]["floor"].i(), &pts[k] };
            entries_.push_back(e);
        }
    }

    /* NRelicHistory.LoadRelics */
    set_label(screen_->find_unique("RelicHistory") ? screen_->find_unique("RelicHistory")->get_node("Header") : NULL,
              data_["relics_header"].str());
    Node* relics = screen_->find_unique("RelicsContainer");
    const Json& rl = data_["relics"];
    for (size_t i = 0; relics && i < rl.size(); ++i) {
        Control* h = control_of(instance(lib_, "scenes/relics/relic_basic_holder.tscn"));
        if (!h) continue;
        Node* relic = h->find_unique("Relic");
        set_relic_icon(lib_, relic ? relic->find_unique("Icon") : NULL, relic_icon_base(rl[i]));
        relics->add_child(h);
        Entry e = { RELIC, h, NULL, NULL, NULL, 0, 0, rl[i]["floor"].i(), &rl[i] };
        entries_.push_back(e);
    }

    /* NDeckHistory.LoadDeck: NDeckHistoryEntry with its NTinyCard */
    set_label(screen_->find_unique("DeckHistory") ? screen_->find_unique("DeckHistory")->get_node("Header") : NULL,
              data_["deck_header"].str());
    Node* cards = screen_->find_unique("CardContainer");
    const Json& dl = data_["deck"];
    for (size_t i = 0; cards && i < dl.size(); ++i) {
        Control* en = control_of(instance(lib_, "scenes/screens/run_history_screen/deck_history_entry.tscn"));
        if (!en) continue;
        const Json& c = dl[i];
        int amount = c["amount"].i(1);
        Node* label = en->find_unique("Label");
        set_label(label, amount > 1 ? format("%dx %s", amount, c["title"].str().c_str()) : c["title"].str());
        if (label && label->is_control()) {
            if (c["enchanted"].b()) ((Control*)label)->add_theme_color_override("font_color", Color(0.933333f, 0.509804f, 0.933333f, 1));
            else if (c["upgraded"].b()) ((Control*)label)->add_theme_color_override("font_color", Color(0.498039f, 1, 0, 1));
        }
        Control* card = control_of(en->find_unique("Card"));
        if (card) {
            Node* back = card->find_unique("CardBack");
            const Json& dc = c["deck_color"];
            if (back && back->is_canvas_item() && dc.size() >= 4)
                ((CanvasItem*)back)->modulate = Color(jv_float(dc[(size_t)0]), jv_float(dc[1]), jv_float(dc[2]), jv_float(dc[3]));
            if (back && back->is_canvas_item() && c["frame_material"].is_string()) {
                Resource* m = lib_->ext("materials/cards/frames/" + c["frame_material"].str() + "_mat.tres");
                ((CanvasItem*)back)->material = m && (m->kind == Resource::RES_SHADER_MATERIAL ||
                                                      m->kind == Resource::RES_CANVAS_ITEM_MATERIAL)
                                                     ? (MaterialRes*)m
                                                     : NULL;
            }
            const std::string& type = c["type"].str();
            std::string shape = type == "Attack" ? "attack" : type == "Power" ? "power" : "skill";
            set_texture(lib_, card->find_unique("Portrait"), "images/packed/run_history/" + shape + "_portrait.png");
            set_texture(lib_, card->find_unique("PortraitShadow"), "images/packed/run_history/" + shape + "_portrait_shadow.png");
            const std::string& r = c["rarity"].str();
            unsigned rgb = r == "Uncommon" ? 0x64FFFF : r == "Rare" ? 0xFFDA36 : r == "Curse" ? 0xE669FF
                         : r == "Event" ? 0x13BE1A : r == "Quest" ? 0xF46836 : (r == "Basic" || r == "Common") ? 0x9C9C9C : 0xFFFFFF;
            Node* banner = card->find_unique("Banner");
            if (banner && banner->is_canvas_item())
                ((CanvasItem*)banner)->modulate = Color(((rgb >> 16) & 255) / 255.0f, ((rgb >> 8) & 255) / 255.0f, (rgb & 255) / 255.0f, 1);
            card->pivot_offset = Vec2(16, 16);
        }
        Node* ench = en->find_unique("Enchantment");
        if (c["enchantment_icon"].is_string()) set_texture(lib_, ench, c["enchantment_icon"].str());
        set_visible(ench, c["enchanted"].b());
        cards->add_child(en);
        Control* box = control_of(en->find_unique("MarginContainer"));
        Entry e = { CARD, en, card, NULL, box, 0, box ? box->offset[SIDE_LEFT] : 0, 0, &c };
        entries_.push_back(e);
    }
    for (size_t i = 0; i < entries_.size(); ++i)
        if (entries_[i].kind == POINT) highlight(entries_[i], false, true);
    scroll_.to_top();
}

/* NMapPointHistoryEntry.Highlight / Unhighlight */
void RunHistoryScreen::highlight(Entry& e, bool on, bool instant) {
    if (e.kind != POINT || !e.icon) return;
    float t = instant ? 0.0001f : (on ? 0.05f : 1.0f);
    float s = on ? BASE_SCALE * 1.5f : BASE_SCALE;
    tweens_.tween(&e.icon->scale, Vec2(s, s), t, TRANS_EXPO, EASE_OUT);
    tweens_.tween(&e.icon->rotation, on ? 0.0f : e.angle, t, TRANS_EXPO, EASE_OUT);
    if (e.outline) {
        Color to = on ? Color(1, 1, 1, 0.5f) : Color(0, 0, 0, 0.25f);
        tweens_.tween(&e.outline->modulate.r, to.r, t, TRANS_EXPO, EASE_OUT);
        tweens_.tween(&e.outline->modulate.g, to.g, t, TRANS_EXPO, EASE_OUT);
        tweens_.tween(&e.outline->modulate.b, to.b, t, TRANS_EXPO, EASE_OUT);
        tweens_.tween(&e.outline->modulate.a, to.a, t, TRANS_EXPO, EASE_OUT);
    }
}

/* NMapPointHistory.HighlightRelevantEntries: the point of that floor. */
void RunHistoryScreen::highlight_floor(int floor, bool on) {
    for (size_t i = 0; i < entries_.size(); ++i)
        if (entries_[i].kind == POINT && (on ? entries_[i].floor == floor : (int)i != focus_)) highlight(entries_[i], on);
}

/* NMapPointHistoryHoverTip */
Control* RunHistoryScreen::make_point_tip(const Json& tip) {
    Control* t = control_of(instance(lib_, "scenes/ui/map_point_history_hover_tip.tscn"));
    if (!t) return NULL;
    set_label(t->find_unique("Title"), tip["title"].str());
    set_label(t->find_unique("PlayerStats"), tip["player"].str());
    set_label(t->find_unique("RoomStats"), tip["room"].str());
    set_label(t->find_unique("CardStats"), tip["actions"].str());
    static const char* const groups[2][3] = { { "RewardStats", "RewardRows", "rewards" },
                                              { "SkippedStats", "SkippedRows", "skipped" } };
    for (int g = 0; g < 2; ++g) {
        Node* box = t->find_unique(groups[g][0]);
        set_label(box ? box->get_node("Header") : NULL, tip[g == 0 ? "rewards_header" : "skipped_header"].str());
        const Json& items = tip[groups[g][2]];
        int n = (int)items.size();
        set_visible(box, n > 0);
        Node* rows = t->find_unique(groups[g][1]);
        std::vector<Node*> labels;
        for (size_t k = 0; rows && k < rows->children.size(); ++k)
            if (rows->children[k]->type == "RichTextLabel") labels.push_back(rows->children[k]);
        if (labels.size() < 2) continue;
        int per = n / 2 + (n % 2);
        if (per < 5) per = 5;
        set_visible(labels[1], n > 5);
        std::string text[2];
        for (int k = 0; k < n; ++k) text[k / per < 2 ? k / per : 1] += "\t" + items[(size_t)k].str() + "\n";
        for (int k = 0; k < 2; ++k) {
            std::string s = text[k];
            while (!s.empty() && s[s.size() - 1] == '\n') s.erase(s.size() - 1);
            set_label(labels[(size_t)k], s);
        }
    }
    return t;
}

void RunHistoryScreen::unfocus() {
    if (focus_ >= 0 && focus_ < (int)entries_.size()) {
        Entry& e = entries_[(size_t)focus_];
        if (e.kind == POINT) {
            focus_ = -1;
            highlight(e, false);
        } else if (e.kind == RELIC) {
            focus_ = -1;
            if (e.floor > 0) highlight_floor(e.floor, false);
        } else if (e.kind == CARD) {
            focus_ = -1;
            if (e.icon) tweens_.tween(&e.icon->scale, Vec2(1, 1), 0.5f, TRANS_EXPO, EASE_OUT);
            if (e.label) {
                float w = e.label->offset[SIDE_RIGHT] - e.label->offset[SIDE_LEFT];
                tweens_.tween(&e.label->offset[SIDE_LEFT], e.label_x, 0.5f, TRANS_EXPO, EASE_OUT);
                tweens_.tween(&e.label->offset[SIDE_RIGHT], e.label_x + w, 0.5f, TRANS_EXPO, EASE_OUT);
            }
            highlight_floor(-1, false);
        } else {
            hide(e.node->find_unique("SelectionReticle"));
        }
    }
    focus_ = -1;
    tips_.hide();
    if (tip_) {
        if (tip_->parent) tip_->parent->remove_child(tip_);
        delete tip_;
        tip_ = NULL;
    }
}

void RunHistoryScreen::focus(int i) {
    if (i < 0 || i >= (int)entries_.size() || i == focus_) return;
    unfocus();
    focus_ = i;
    Entry& e = entries_[(size_t)i];
    const Json& d = *e.data;
    if (e.kind == POINT) {
        highlight(e, true);
        tip_ = make_point_tip(d["tip"]);
        tip_offset_y_ = 96;
        if (tip_) {
            tip_->z_index = 10;
            tip_->modulate.a = 0;
            root_->add_child(tip_);
        }
    } else if (e.kind == RELIC) {
        tips_.show_for_control(lib_, root_, e.node, d["tips"]);
        if (e.floor > 0) highlight_floor(e.floor, true);
    } else if (e.kind == CARD) {
        if (e.icon) tweens_.tween(&e.icon->scale, Vec2(1.5f, 1.5f), 0.05f);
        if (e.label) {
            float w = e.label->offset[SIDE_RIGHT] - e.label->offset[SIDE_LEFT];
            tweens_.tween(&e.label->offset[SIDE_LEFT], e.label_x + 8, 0.05f);
            tweens_.tween(&e.label->offset[SIDE_RIGHT], e.label_x + 8 + w, 0.05f);
        }
        const Json& floors = d["floors"];
        for (size_t k = 0; k < floors.size(); ++k) highlight_floor(floors[k].i(), true);
    } else {
        set_visible(e.node->find_unique("SelectionReticle"), true);
        if (e.kind == BADGE) {
            tip_ = make_hover_tip(lib_, d["title"].str(), d["description"].str());
            tip_offset_y_ = -132;
        } else {
            tip_ = make_tip_set(lib_, d["tips"]);
            tip_offset_y_ = 0;
        }
        if (tip_) {
            tip_->z_index = 10;
            tip_->modulate.a = 0;
            root_->add_child(tip_);
        }
    }
    scroll_.focus(e.node);
}

RunHistoryScreen::Action RunHistoryScreen::input(const PadState& pad, std::string* cmd) {
    cmd->clear();
    if (inspect_card_ || inspect_relic_) {
        bool open = inspect_card_ ? inspect_card_->input(pad) : inspect_relic_->input(pad);
        if (!open) close_inspect();
        return NONE;
    }
    if (pad.pressed & PadState::B) return BACK;
    if ((pad.pressed & PadState::A) && focus_ >= 0 &&
        (entries_[(size_t)focus_].kind == RELIC || entries_[(size_t)focus_].kind == CARD)) {
        /* OnRelicClicked / ShowEntry: the inspect screen over the list */
        Kind kind = entries_[(size_t)focus_].kind;
        Json list = Json::make_array();
        int at = 0;
        for (size_t k = 0; k < entries_.size(); ++k) {
            if (entries_[k].kind != kind) continue;
            if ((int)k == focus_) at = (int)list.size();
            list.append(*entries_[k].data);
        }
        std::string err;
        if (kind == RELIC) {
            inspect_relic_ = new InspectRelicScreen(lib_);
            inspect_node_ = inspect_relic_->build(list, at, &err);
        } else {
            inspect_card_ = new InspectCardScreen(lib_);
            inspect_node_ = inspect_card_->build(list, at, false, &err);
        }
        if (inspect_node_) {
            unfocus();
            root_->add_child(inspect_node_);
        } else {
            close_inspect();
        }
        return NONE;
    }
    /* the arrows: LB the older run, RB the newer, the contents sliding in */
    int step = (pad.pressed & PadState::LB) ? 1 : (pad.pressed & PadState::RB) ? -1 : 0;
    if (step && index_ + step >= 0 && index_ + step < count_) {
        *cmd = query(index_ + step);
        if (contents_) {
            tweens_.tween_from(&contents_->modulate.a, 0.0f, 1.0f, 0.4f);
            float from = step > 0 ? -1000.0f : 1000.0f;
            tweens_.tween_from(&contents_->offset[SIDE_LEFT], from, 0.0f, 0.5f, TRANS_EXPO, EASE_OUT);
            tweens_.tween_from(&contents_->offset[SIDE_RIGHT], from, 0.0f, 0.5f, TRANS_EXPO, EASE_OUT);
        }
        return NONE;
    }
    Vec2 dir;
    if (pad.pressed & PadState::LEFT) dir = Vec2(-1, 0);
    else if (pad.pressed & PadState::RIGHT) dir = Vec2(1, 0);
    else if (pad.pressed & PadState::UP) dir = Vec2(0, -1);
    else if (pad.pressed & PadState::DOWN) dir = Vec2(0, 1);
    else return NONE;
    if (focus_ < 0) {
        /* _Input: a direction with nothing focused focuses the first point */
        for (size_t k = 0; k < entries_.size(); ++k)
            if (entries_[k].kind == POINT) {
                focus((int)k);
                break;
            }
        return NONE;
    }
    Control* cur = entries_[(size_t)focus_].node;
    Vec2 from = cur->global_transform().origin + cur->size * 0.5f;
    int best = -1;
    float best_d = 1e30f;
    for (size_t k = 0; k < entries_.size(); ++k) {
        if ((int)k == focus_ || !entries_[k].node->is_visible_in_tree()) continue;
        Control* c = entries_[k].node;
        Vec2 d = c->global_transform().origin + c->size * 0.5f - from;
        float along = d.x * dir.x + d.y * dir.y;
        if (along <= 1.0f) continue;
        float score = along + fabsf(d.x * dir.y - d.y * dir.x) * 2.0f;
        if (score < best_d) {
            best_d = score;
            best = (int)k;
        }
    }
    if (best >= 0) focus(best);
    return NONE;
}

void RunHistoryScreen::frame(double dt) {
    tweens_.update(dt);
    scroll_.frame(dt);
    tips_.frame();
    if (contents_ && contents_->modulate.a == 0 && data_.is_object()) tweens_.tween(&contents_->modulate.a, 1.0f, 0.4f);
    if (tip_ && focus_ >= 0) {
        Control* e = entries_[(size_t)focus_].node;
        Vec2 at = e->global_transform().origin;
        float x = at.x, y = at.y + tip_offset_y_;
        Kind k = entries_[(size_t)focus_].kind;
        if (k == POINT) x = at.x > 1920.0f * 0.75f ? at.x - tip_->size.x : at.x + e->size.x;
        else if (k == BADGE) x = at.x + 10;
        else if (k == PLAYER) y = at.y + e->size.y + 20;
        if (y + tip_->size.y > 1080.0f) y = 1080.0f - tip_->size.y;
        if (y < 0) y = 0;
        tip_->set_position(Vec2(x, y));
        tip_->modulate.a = 1;
    }
}

} /* namespace spire */
