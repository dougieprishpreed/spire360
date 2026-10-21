#include "top_bar.h"
#include "ui_util.h"
#include "loc.h"

#include "../scene/controls.h"
#include "../scene/node.h"
#include "../scene/particles.h"
#include "../scene/scene.h"
#include "../scene/script.h"
#include "../scene/tween.h"

#include <math.h>
#include <string.h>
#include <vector>

namespace spire {

namespace {
/* A node that frees itself after its time (one-shot particles). */
class FreeAfter : public ScriptBehavior {
public:
    explicit FreeAfter(double t) : left_(t) {}
    virtual void process(Node* n, double dt) {
        left_ -= dt;
        if (left_ <= 0 && !n->queued_for_deletion()) n->queue_free();
    }

private:
    double left_;
};

/* The bar's own animations (the potions' and relics'), run by the bar's
 * node: its tweens go with it. A new relic's holder flashes when its icon
 * is in place (PlayNewlyAcquiredAnimation's DoFlash callback). */
class BarAnimations : public ScriptBehavior {
public:
    struct Flash {
        Control* holder;
        float at;
    };
    BarAnimations(SceneLibrary* lib) : lib_(lib), time_(0) {}
    TweenSet tweens;
    std::vector<Flash> flashes;
    virtual void process(Node* n, double dt) {
        tweens.update(dt);
        time_ += (float)dt;
        for (size_t i = 0; i < flashes.size();) {
            if (time_ + 1e-4f < flashes[i].at) {
                ++i;
                continue;
            }
            Node* fx = make_relic_holder_flash(lib_, flashes[i].holder);
            if (fx && n->parent) n->parent->add_child(fx);
            else delete fx;
            flashes.erase(flashes.begin() + (long)i);
        }
    }

private:
    SceneLibrary* lib_;
    float time_;
};
} /* namespace */

Node* make_relic_holder_flash(SceneLibrary* lib, Control* holder) {
    Node* fx = instance(lib, "scenes/vfx/relic_inventory_flash_vfx.tscn");
    if (!fx || !fx->is_node2d()) {
        delete fx;
        return NULL;
    }
    ((Node2D*)fx)->position = holder->global_transform().xform(holder->size * 0.5f);
    Node* hi = holder->find_unique("Icon");
    Node* p = fx->get_node("Particles");
    if (p && (p->type == "GPUParticles2D" || p->type == "CPUParticles2D")) {
        if (hi && hi->type == "TextureRect" && ((TextureRect*)hi)->texture) ((Particles2D*)p)->texture = ((TextureRect*)hi)->texture;
        ((Particles2D*)p)->restart();
    }
    fx->behavior = new FreeAfter(3.0);
    return fx;
}

/* ImageHelper.GetRoomIconPath: images/ui/run_history/<suffix>.png, "" for
 * none (the act-start map room). */
static std::string room_icon(const Json& ctx) {
    const std::string& point = ctx["point_type"].str();
    const std::string& room = ctx["room_type"].str();
    if (point.empty() || point == "Unassigned" || room.empty() || room == "Map") return std::string();
    std::string suffix;
    if (point == "Boss") {
        const Json& second = ctx["second_boss"];
        suffix = ctx["at_second_boss"].b() && second.is_string() ? second.str() : ctx["boss"]["id"].str();
    } else if (point == "Ancient") {
        suffix = ctx["ancient"].str();
    } else if (room != "Boss") {
        /* Slugify(roomType): RestSite -> rest_site */
        std::string slug;
        for (size_t i = 0; i < room.size(); ++i) {
            if (i > 0 && room[i] >= 'A' && room[i] <= 'Z') slug += '_';
            slug += room[i];
        }
        suffix = point == "Unknown" && room != "Event" ? "unknown_" + slug : slug;
    }
    if (suffix.empty()) return std::string();
    return "images/ui/run_history/" + lower(suffix) + ".png";
}

/* An icon and its Outline child from a run_history path. */
static void set_room_icon(SceneLibrary* lib, Node* icon, const std::string& path) {
    if (!icon) return;
    if (path.empty()) {
        hide(icon);
        return;
    }
    set_texture(lib, icon, path);
    set_texture(lib, icon->get_node("Outline"), path.substr(0, path.size() - 4) + "_outline.png");
}

/* NRunTimer: shown with PrefsSave.ShowRunTimer, or while the map or a
 * capstone screen is open; RunManager.RunTime (from the decision) counted
 * on, TimeFormatting.Format each second. On the timer's parent (a hidden
 * node does not process). */
class RunTimer : public ScriptBehavior {
public:
    RunTimer(Node* timer, double base, bool map) : timer_(timer), base_(base), t_(0), next_(0), map_(map) {}
    virtual void process(Node*, double dt) {
        Node* n = timer_;
        t_ += dt;
        bool on = client_settings()["run_timer"].b() || capstone_open() || map_;
        set_visible(n, on);
        if (t_ < next_) return;
        next_ = floor(t_) + 1.0;
        long s = (long)(base_ + t_);
        long h = s / 3600, m = (s / 60) % 60, sec = s % 60;
        std::string text = Loc::get("main_menu_ui", h > 0 ? "RUN_TIME_FORMAT_HOURS" : "RUN_TIME_FORMAT");
        replace_var(text, "{Hours}", format("%ld", h));
        replace_var(text, "{Minutes}", format("%02ld", m));
        replace_var(text, "{Seconds}", format("%02ld", sec));
        set_label(n->find_unique("TimerLabel"), text);
    }

private:
    static void replace_var(std::string& s, const char* var, const std::string& value) {
        size_t at = s.find(var);
        if (at != std::string::npos) s.replace(at, strlen(var), value);
    }
    Node* timer_;
    double base_, t_, next_;
    bool map_;
};

/* NTopBarButton glyphs: Map = Back, Deck = LB, Pause = Start; a disabled
 * button is greyed (StsColors.disabledTopBarButton) without its glyph. */
static void top_button(SceneLibrary* lib, Node* button, const char* glyph, bool enabled) {
    if (!button) return;
    Node* hk = button->find_unique("HotkeyIcon");
    if (enabled) show_glyph(lib, hk, glyph);
    else {
        hide(hk);
        if (button->is_canvas_item()) ((CanvasItem*)button)->modulate = Color(0.5f, 0.5f, 0.5f, 0.75f);
    }
}

Node* make_potion(SceneLibrary* lib, const std::string& potion_id) {
    Node* p = instance(lib, "scenes/potions/potion.tscn");
    if (!p || !p->is_control()) {
        delete p;
        return NULL;
    }
    Control* pc = (Control*)p;
    /* At the parent's top-left (the scene centres it in the viewport). */
    for (int k = 0; k < 4; ++k) pc->anchor[k] = 0;
    place(pc, 0, 0, 60, 60);
    pc->pivot_offset = Vec2(30, 30);
    std::string id = lower(potion_id);
    Node* image = p->find_unique("Image");
    set_texture(lib, image, "images/atlases/potion_atlas.sprites/" + id + ".tres");
    if (image) set_texture(lib, image->find_unique("Outline"), "images/atlases/potion_outline_atlas.sprites/" + id + ".tres");
    hide(p->find_unique("Juice"));
    hide(p->find_unique("Overlay"));
    hide(p->find_unique("Bottle"));
    return p;
}

std::string relic_icon_base(const Json& relic, const char* icon_key, const char* id_key) {
    return relic[icon_key].is_string() ? relic[icon_key].str() : relic[id_key].str();
}

void set_relic_icon(SceneLibrary* lib, Node* icon, const std::string& relic_id) {
    std::string id = lower(relic_id);
    set_texture(lib, icon, "images/atlases/relic_atlas.sprites/" + id + ".tres");
    if (icon) set_texture(lib, icon->find_unique("Outline"), "images/atlases/relic_outline_atlas.sprites/" + id + ".tres");
}

static Control* as_ctl(Node* n) { return n && n->is_control() ? (Control*)n : NULL; }

static const Json* potion_in_slot(const Json& potions, int slot) {
    for (size_t i = 0; i < potions.size(); ++i)
        if (potions[i]["index"].i() == slot) return &potions[i];
    return NULL;
}

/* NPotion (potion.tscn) in a holder: at (-30, -30) from its centre, 0.9. */
static Control* holder_potion(SceneLibrary* lib, Node* holder, const std::string& id) {
    Node* p = make_potion(lib, id);
    if (!p) return NULL;
    Control* pc = (Control*)p;
    pc->anchor[SIDE_LEFT] = pc->anchor[SIDE_RIGHT] = 0.5f;
    pc->anchor[SIDE_TOP] = pc->anchor[SIDE_BOTTOM] = 0.5f;
    place(pc, -30, -30, 60, 60);
    pc->scale = Vec2(0.9f, 0.9f);
    holder->add_child(p);
    return pc;
}

/* NPotionContainer: one holder per slot (player.max_potions), a potion
 * (potion.tscn, 0.9 scale, centred) in the slots that have one. With the
 * player as the decision before had them: a potion new in its slot drops
 * in (NPotion.PlayNewlyAcquiredAnimation: its container from 40 px lower,
 * fading in), one gone shrinks away (NPotionHolder.RemoveUsedPotion: to
 * scale 0 in 0.2 s, then the empty slot fades in). */
static void make_potions(SceneLibrary* lib, Node* bar, const Json& player, const Json& before, TweenSet* tweens) {
    Node* holders = bar->find_unique("PotionHolders");
    if (!holders) holders = find_named(bar, "PotionHolders");
    if (!holders) return;
    const Json& potions = player["potions"];
    int slots = player["max_potions"].i(3);
    for (size_t i = 0; i < potions.size(); ++i)
        if (potions[i]["index"].i() + 1 > slots) slots = potions[i]["index"].i() + 1;
    for (int s = 0; s < slots; ++s) {
        Node* h = instance(lib, "scenes/potions/potion_holder.tscn");
        if (!h || !h->is_control()) {
            delete h;
            return;
        }
        const Json* pot = potion_in_slot(potions, s);
        const Json* was = before.is_object() ? potion_in_slot(before["potions"], s) : NULL;
        bool gained = pot && before.is_object() && (!was || (*was)["id"].str() != (*pot)["id"].str());
        bool lost = !pot && was;
        Node* empty = h->find_unique("EmptyIcon");
        if (empty && empty->is_canvas_item()) {
            CanvasItem* e = (CanvasItem*)empty;
            e->modulate.a = pot ? 0.0f : 1.0f;
            if (empty->is_control()) ((Control*)empty)->scale = Vec2(0.9f, 0.9f);
            if (lost && tweens) tweens->tween_from(&e->modulate.a, 0.0f, 1.0f, 0.2f, TRANS_LINEAR, EASE_IN_OUT, 0.2f);
        }
        hide(h->find_unique("SelectionReticle"));
        if (pot) {
            Control* pc = holder_potion(lib, h, (*pot)["id"].str());
            Control* box = pc ? as_ctl(pc->get_node("Container")) : NULL;
            if (gained && tweens && box) {
                tweens->tween_from(&box->modulate.a, 0.0f, 1.0f, 0.1f);
                float top = box->offset[SIDE_TOP], bottom = box->offset[SIDE_BOTTOM];
                tweens->tween_from(&box->offset[SIDE_TOP], top + 40, top, 0.35f, TRANS_BACK, EASE_OUT);
                tweens->tween_from(&box->offset[SIDE_BOTTOM], bottom + 40, bottom, 0.35f, TRANS_BACK, EASE_OUT);
            }
        } else if (lost && tweens) {
            Control* pc = holder_potion(lib, h, (*was)["id"].str());
            if (pc) tweens->tween_from(&pc->scale, Vec2(0.9f, 0.9f), Vec2(0, 0), 0.2f, TRANS_BACK, EASE_IN);
        }
        holders->add_child(h);
    }
}

static int count_id(const Json& relics, const std::string& id, size_t n) {
    int c = 0;
    for (size_t i = 0; i < n && i < relics.size(); ++i)
        if (relics[i]["id"].str() == id) ++c;
    return c;
}

/* NRelicInventory: the relics in acquisition order, 68 px each, in a row
 * under the bar (run.tscn, which places it, is not in the bundles: the
 * game's screenshots show the row from x ~11, y ~81). With the player as
 * the decision before had them, a relic new since rises into place
 * (RelicCmd.Obtain's AnimateRelic: NRelicInventoryHolder.
 * PlayNewlyAcquiredAnimation, the icon from 40 px lower fading in over
 * 0.1 s, Back Out over 0.35 s, then DoFlash). */
static Node* make_relics(SceneLibrary* lib, const Json& player, const Json& before, BarAnimations* anims) {
    FlowContainer* row = (FlowContainer*)SceneLibrary::create_node("HFlowContainer");
    row->name = "RelicInventory";
    place(row, 11, 81, 1898, 68);
    row->mouse_filter = 2;
    const Json& relics = player["relics"];
    for (size_t i = 0; i < relics.size(); ++i) {
        Node* r = instance(lib, "scenes/relics/relic.tscn");
        if (!r || !r->is_control()) {
            delete r;
            break;
        }
        ((Control*)r)->custom_minimum_size = Vec2(68, 68);
        Node* icon = r->find_unique("Icon");
        set_relic_icon(lib, icon, relic_icon_base(relics[i]));
        const std::string& id = relics[i]["id"].str();
        if (anims && icon && icon->is_control() && count_id(relics, id, i + 1) > count_id(before["relics"], id, before["relics"].size())) {
            Control* ic = (Control*)icon;
            float top = ic->offset[SIDE_TOP], bottom = ic->offset[SIDE_BOTTOM];
            anims->tweens.tween_from(&ic->modulate.a, 0.0f, 1.0f, 0.1f);
            anims->tweens.tween_from(&ic->offset[SIDE_TOP], top + 40, top, 0.35f, TRANS_BACK, EASE_OUT);
            anims->tweens.tween_from(&ic->offset[SIDE_BOTTOM], bottom + 40, bottom, 0.35f, TRANS_BACK, EASE_OUT);
            BarAnimations::Flash f = {(Control*)r, 0.35f};
            anims->flashes.push_back(f);
        }
        row->add_child(r);
    }
    return row;
}

Node* make_top_bar(SceneLibrary* lib, const Json& d, bool in_map_room, const Json& before) {
    Node* bar = instance(lib, "scenes/ui/top_bar.tscn");
    if (!bar) return NULL;
    BarAnimations* anims = NULL;
    if (before.is_object() && !bar->behavior) {
        anims = new BarAnimations(lib);
        bar->behavior = anims;
    }
    const Json& player = d["player"];
    const Json& ctx = d["context"];
    std::string character = lower(player["character"].is_string() ? player["character"].str() : std::string("ironclad"));

    /* Portrait (Character.Icon) and ascension. */
    Node* portrait = bar->find_unique("TopBarPortrait");
    if (!portrait) portrait = find_named(bar, "TopBarPortrait");
    Node* icon = instance(lib, "scenes/ui/character_icons/" + character + "_icon.tscn");
    if (portrait && icon) portrait->add_child(icon);
    else delete icon;
    int asc = player["ascension"].i();
    Node* asc_icon = bar->find_unique("AscensionIcon");
    if (asc_icon && asc_icon->is_canvas_item()) {
        ((CanvasItem*)asc_icon)->visible = asc > 0;
        Node* label = asc_icon->get_node("AscensionLabel");
        if (label && label->is_control()) {
            ((Control*)label)->add_theme_color_override("font_outline_color", hex(0x593400));
            set_label(label, format("%d", asc));
        }
    }
    hide(bar->find_unique("AchievementLock"));

    /* HP and gold. */
    set_label(bar->find_unique("HpLabel"), format("%d/%d", player["hp"].i(), player["max_hp"].i()));
    set_label(bar->find_unique("GoldLabel"), format("%d", player["gold"].i()));

    make_potions(lib, bar, player, before, anims ? &anims->tweens : NULL);
    Node* shortcut = find_named(bar, "PotionShortcutButton");
    show_glyph(lib, shortcut ? shortcut->find_unique("HotkeyIcon") : NULL, "x");

    /* Room, floor, boss. */
    Node* room_icon_node = find_named(bar, "RoomIcon");
    set_room_icon(lib, room_icon_node ? room_icon_node->get_node("Icon") : NULL, room_icon(ctx));
    set_label(find_named(bar, "FloorNumLabel"), format("%d", ctx["total_floor"].i(ctx["floor"].i())));
    Node* boss = find_named(bar, "BossIcon");
    if (boss) {
        bool in_boss_room = ctx["room_type"].str() == "Boss";
        if (in_boss_room) hide(boss);
        else if (ctx["boss"]["id"].is_string())
            set_room_icon(lib, boss->get_node("Icon"), "images/ui/run_history/" + lower(ctx["boss"]["id"].str()) + ".png");
    }
    hide(find_named(bar, "Modifiers"));

    /* Right side: the save indicator is transparent at rest, the timer
     * hidden (shown only with the map or a capstone screen open). */
    Node* save = find_named(bar, "SaveIndicator");
    if (save && save->is_canvas_item()) ((CanvasItem*)save)->modulate.a = 0;
    Node* timer = find_named(bar, "TimerContainer");
    hide(timer);
    if (timer && timer->parent && !timer->parent->behavior && ctx["run_time"].is_number())
        timer->parent->behavior = new RunTimer(timer, ctx["run_time"].num(),
                                               ctx["room_type"].str() == "Map" || d["decision"].str() == "map_select");
    top_button(lib, find_named(bar, "Map"), "back", !in_map_room);
    top_button(lib, find_named(bar, "Deck"), "lb", true);
    top_button(lib, find_named(bar, "PauseButton"), "start", true);
    Node* deck = find_named(bar, "Deck");
    set_label(find_named(deck, "DeckCardCount"), format("%d", player["deck_size"].i()));
    hide(find_named(bar, "GoldPopup"));

    Control* root = full_rect_control("TopBarLayer");
    root->add_child(make_relics(lib, player, before, anims));
    root->add_child(bar);
    return root;
}

} /* namespace spire */
