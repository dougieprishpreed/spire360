#include "top_bar.h"
#include "ui_util.h"
#include "loc.h"

#include "../core/cue_ledger.h"
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

/* NSaveIndicator.SavedGame: after 0.5 s to white (1 s), held 0.5 s, then
 * transparent (1 s); one tween sequence (a second tween on the same
 * property would replace the first). */
class SavedFade : public ScriptBehavior {
public:
    SavedFade() : t_(0) {}
    virtual void process(Node* n, double dt) {
        t_ += dt;
        if (!n->is_canvas_item()) return;
        float a = t_ < 0.5 ? 0.0f : t_ < 1.5 ? (float)(t_ - 0.5) : t_ < 2.0 ? 1.0f : t_ < 3.0 ? (float)(3.0 - t_) : 0.0f;
        ((CanvasItem*)n)->modulate.a = a;
    }

private:
    double t_;
};

/* The bar's own animations (the potions' and relics'), run by the bar's
 * node: its tweens go with it. A new relic's holder flashes when its icon
 * is in place (PlayNewlyAcquiredAnimation's DoFlash callback). */
/* NTopBarGold.UpdateGoldAnim: a change counts the gold label from what
 * it was, 0.4 s after it (the popup's tweens: the popup itself stays
 * hidden), by 75 while more than 100 is left, 10 while more than 50, else 1,
 * each step after Lerp(10, 20, max(0, 10 - left)) ms; a change while it
 * counts starts from the last total. */
class GoldCount : public ScriptBehavior {
public:
    GoldCount(Node* label, int gold) : label_(label), target_(gold), left_(0), running_(false), t_(0), next_(0) {}
    void set(int gold) {
        if (gold == target_) return;
        left_ = gold - target_;
        target_ = gold;
        set_label(label_, format("%d", target_ - left_));
        if (running_) return;
        running_ = true;
        t_ = 0;
        next_ = 0.4;
    }
    virtual void process(Node*, double dt) {
        if (!running_) return;
        t_ += dt;
        while (running_ && t_ >= next_) {
            if (left_ == 0) {
                running_ = false;
                set_label(label_, format("%d", target_));
                break;
            }
            int a = left_ < 0 ? -left_ : left_;
            int n = a > 100 ? 75 : a > 50 ? 10 : 1;
            if (n > a) n = a;
            left_ += left_ > 0 ? -n : n;
            set_label(label_, format("%d", target_ - left_));
            a = left_ < 0 ? -left_ : left_;
            int w = 10 - a > 0 ? 10 - a : 0;
            next_ += (10.0 + 10.0 * w) / 1000.0;
            if (left_ == 0) next_ += 0.25;
        }
    }

private:
    Node* label_;
    int target_, left_;
    bool running_;
    double t_, next_;
};

/* top_bar_acquire_from: one pending flight */
static std::string g_from_kind, g_from_id;
static Vec2 g_from_at;
static float g_from_scale = 1;

} /* namespace */

void top_bar_acquire_from(const std::string& kind, const std::string& id, const Vec2& at, float scale) {
    g_from_kind = kind;
    g_from_id = id;
    g_from_at = at;
    g_from_scale = scale;
}

namespace {

/* the pending flight of this kind and id, taken */
static bool take_acquire_from(const char* kind, const std::string& id, Vec2* at, float* scale) {
    if (g_from_kind != kind || g_from_id != id) return false;
    *at = g_from_at;
    *scale = g_from_scale;
    g_from_kind.clear();
    return true;
}

class BarAnimations : public ScriptBehavior {
public:
    struct Flash {
        Control* holder; /* a relic holder, or (potion) a potion's node */
        float at;
        bool potion;
    };
    /* PlayNewlyAcquiredAnimation(startLocation): from there (global) and
     * that scale back to its place in 0.35 s (a relic Sine Out, a potion
     * Quad Out); its place known once the bar is laid out (first frame) */
    struct Fly {
        Control* node;
        Vec2 from;
        float scale;
        TweenTrans trans;
    };
    BarAnimations(SceneLibrary* lib) : lib_(lib), time_(0) {}
    TweenSet tweens;
    std::vector<Flash> flashes;
    std::vector<Fly> flights;
    virtual void process(Node* n, double dt) {
        for (size_t i = 0; i < flights.size(); ++i) {
            Control* c = flights[i].node;
            Vec2 now = c->global_transform().origin, d = flights[i].from - now;
            for (int s = 0; s < 4; ++s) {
                float o = c->offset[s];
                c->offset[s] = o + (s == SIDE_LEFT || s == SIDE_RIGHT ? d.x : d.y);
                tweens.tween(&c->offset[s], o, 0.35f, flights[i].trans, EASE_OUT);
            }
            c->modulate.a = 1;
            tweens.tween_from(&c->scale, Vec2(flights[i].scale, flights[i].scale), Vec2(1, 1), 0.35f, flights[i].trans, EASE_OUT);
        }
        flights.clear();
        tweens.update(dt);
        time_ += (float)dt;
        for (size_t i = 0; i < flashes.size();) {
            if (time_ + 1e-4f < flashes[i].at) {
                ++i;
                continue;
            }
            Node* fx = flashes[i].potion ? make_potion_flash(lib_, flashes[i].holder) : make_relic_holder_flash(lib_, flashes[i].holder);
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

namespace {
/* NPotionContainer.PlayAddFailedAnim: the potions shake (3 sin(5u) sin(u/2)
 * px, u 0 -> 2 pi over 0.5 s; here each potion in its holder, which the
 * holders' row lays out) while PotionErrorBg flashes in (0.15 s) and out
 * (0.5 s after 0.35 s). A child of the bar: gone with it. */
class PotionsFull : public ScriptBehavior {
public:
    PotionsFull(CanvasItem* error, const std::vector<Control*>& potions) : error_(error), potions_(potions), t_(0) {
        for (size_t i = 0; i < potions_.size(); ++i) {
            left_.push_back(potions_[i]->offset[SIDE_LEFT]);
            right_.push_back(potions_[i]->offset[SIDE_RIGHT]);
        }
    }
    virtual void process(Node* n, double dt) {
        t_ += dt;
        float u = (float)(t_ < 0.5 ? t_ / 0.5 : 1.0) * 6.2831853f;
        float dx = t_ < 0.5 ? 3.0f * sinf(u * 5.0f) * sinf(u * 0.5f) : 0.0f;
        for (size_t i = 0; i < potions_.size(); ++i) {
            potions_[i]->offset[SIDE_LEFT] = left_[i] + dx;
            potions_[i]->offset[SIDE_RIGHT] = right_[i] + dx;
        }
        if (error_) error_->modulate.a = t_ < 0.15 ? (float)(t_ / 0.15) : t_ < 0.35 ? 1.0f : t_ < 0.85 ? (float)(1.0 - (t_ - 0.35) / 0.5) : 0.0f;
        if (t_ >= 0.85 && !n->queued_for_deletion()) n->queue_free();
    }

private:
    CanvasItem* error_;
    std::vector<Control*> potions_;
    std::vector<float> left_, right_;
    double t_;
};
} /* namespace */

void top_bar_potions_full(Node* bar) {
    Node* holders = bar ? find_named(bar, "PotionHolders") : NULL;
    if (!holders) return;
    std::vector<Control*> potions;
    for (size_t i = 0; i < holders->children.size(); ++i) {
        Node* h = holders->children[i];
        for (size_t k = 0; k < h->children.size(); ++k)
            if (h->children[k]->name == "Potion" && h->children[k]->is_control()) potions.push_back((Control*)h->children[k]);
    }
    Node* error = find_named(bar, "PotionErrorBg");
    Node* fx = SceneLibrary::create_node("Node");
    fx->name = "PotionsFull";
    fx->behavior = new PotionsFull(error && error->is_canvas_item() ? (CanvasItem*)error : NULL, potions);
    bar->add_child(fx);
}

void set_top_bar_gold(Node* bar, int gold) {
    Node* label = bar ? find_named(bar, "GoldLabel") : NULL;
    Node* holder = label ? label->parent : NULL;
    if (holder && holder->name == "TopBarGold" && holder->behavior) ((GoldCount*)holder->behavior)->set(gold);
    else set_label(label, format("%d", gold));
}

/* NPotionFlashVfx (vfx_potion_flash.tscn): its %Flash particle, the
 * potion's image (the game draws the potion into a SubViewport for it),
 * over the potion's 60 px; freed after 2 s */
Node* make_potion_flash(SceneLibrary* lib, Control* potion) {
    Node* fx = instance(lib, "scenes/vfx/vfx_potion_flash.tscn");
    if (!fx || !fx->is_node2d()) {
        delete fx;
        return NULL;
    }
    ((Node2D*)fx)->position = potion->global_transform().origin;
    ((Node2D*)fx)->scale = potion->scale;
    Node* image = potion->find_unique("Image");
    Node* p = fx->find_unique("Flash");
    if (p && (p->type == "GPUParticles2D" || p->type == "CPUParticles2D")) {
        if (image && image->type == "TextureRect" && ((TextureRect*)image)->texture)
            ((Particles2D*)p)->texture = ((TextureRect*)image)->texture;
        ((Particles2D*)p)->restart();
    }
    fx->behavior = new FreeAfter(2.0);
    return fx;
}

Node* make_relic_holder_flash(SceneLibrary* lib, Control* holder) {
    Node* fx = instance(lib, "scenes/vfx/relic_inventory_flash_vfx.tscn");
    if (!fx || !fx->is_node2d()) {
        delete fx;
        return NULL;
    }
    ((Node2D*)fx)->position = holder->global_transform().xform(holder->size * 0.5f);
    Node* hi = holder_relic_icon(holder);
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

Node* holder_relic_icon(Node* holder) {
    if (!holder) return NULL;
    Node* relic = holder->find_unique("Relic");
    Node* icon = relic ? relic->find_unique("Icon") : NULL;
    return icon ? icon : holder->find_unique("Icon");
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
static void make_potions(SceneLibrary* lib, Node* bar, const Json& player, const Json& before, TweenSet* tweens,
                         BarAnimations* anims) {
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
            Vec2 from;
            float from_scale = 1;
            if (gained && anims && pc) {
                /* PlayNewlyAcquiredAnimation's DoFlash: NPotionFlashVfx */
                BarAnimations::Flash fl = { pc, 0.35f, true };
                anims->flashes.push_back(fl);
            }
            if (gained && anims && box && take_acquire_from("potion", (*pot)["id"].str(), &from, &from_scale)) {
                BarAnimations::Fly f = { box, from, 1.0f, TRANS_QUAD };
                anims->flights.push_back(f);
            } else if (gained && tweens && box) {
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
        Node* r = instance(lib, "scenes/relics/relic_inventory_holder.tscn");
        if (!r || !r->is_control()) {
            delete r;
            break;
        }
        ((Control*)r)->custom_minimum_size = Vec2(68, 68);
        Node* icon = holder_relic_icon(r);
        set_relic_icon(lib, icon, relic_icon_base(relics[i]));
        /* NRelicInventoryHolder.RefreshAmount: the counter while ShowCounter */
        Node* amount = r->find_unique("AmountLabel");
        set_visible(amount, relics[i]["relic_amount"].is_number());
        if (relics[i]["relic_amount"].is_number()) set_label(amount, format("%d", relics[i]["relic_amount"].i()));
        /* RefreshStatus / RelicModel.UpdateTexture: Disabled greys (#808080)
         * and is_used, Active pulses, wax tints; melted is dark red */
        if (icon && icon->is_canvas_item()) {
            CanvasItem* ic = (CanvasItem*)icon;
            const std::string& status = relics[i]["relic_status"].str();
            bool melted = relics[i]["relic_melted"].b();
            ic->modulate = status == "Disabled" ? hex(0x808080) : Color(1, 1, 1, 1);
            ic->shader_params["is_wax"] = relics[i]["relic_wax"].b() ? 1.0f : 0.0f;
            ic->shader_params["pulse"] = !melted && status == "Active" ? 1.0f : 0.0f;
            ic->shader_params["is_used"] = !melted && status == "Disabled" ? 1.0f : 0.0f;
            if (melted) ic->self_modulate = Color(0.545098f, 0, 0, 1); /* Colors.DarkRed */
        }
        const std::string& id = relics[i]["id"].str();
        Vec2 from;
        float from_scale = 1;
        bool gained = anims && icon && icon->is_control() && count_id(relics, id, i + 1) > count_id(before["relics"], id, before["relics"].size());
        if (gained && take_acquire_from("relic", id, &from, &from_scale)) {
            BarAnimations::Fly f = { (Control*)icon, from, from_scale, TRANS_SINE };
            anims->flights.push_back(f);
            BarAnimations::Flash fl = {(Control*)r, 0.35f, false};
            anims->flashes.push_back(fl);
        } else if (gained) {
            Control* ic = (Control*)icon;
            float top = ic->offset[SIDE_TOP], bottom = ic->offset[SIDE_BOTTOM];
            anims->tweens.tween_from(&ic->modulate.a, 0.0f, 1.0f, 0.1f);
            anims->tweens.tween_from(&ic->offset[SIDE_TOP], top + 40, top, 0.35f, TRANS_BACK, EASE_OUT);
            anims->tweens.tween_from(&ic->offset[SIDE_BOTTOM], bottom + 40, bottom, 0.35f, TRANS_BACK, EASE_OUT);
            BarAnimations::Flash f = {(Control*)r, 0.35f, false};
            anims->flashes.push_back(f);
        }
        row->add_child(r);
    }
    return row;
}

Json top_bar_key(const Json& d) {
    static const char* player_keys[] = { "character", "ascension", "hp", "max_hp", "gold", "potions", "max_potions",
                                         "relics", "deck_size" };
    static const char* context_keys[] = { "room_type", "second_boss", "at_boss", "boss", "boss_tint", "floor",
                                          "total_floor" };
    Json player, ctx, key;
    for (size_t i = 0; i < sizeof(player_keys) / sizeof(player_keys[0]); ++i)
        player.set(player_keys[i], d["player"][player_keys[i]]);
    for (size_t i = 0; i < sizeof(context_keys) / sizeof(context_keys[0]); ++i)
        ctx.set(context_keys[i], d["context"][context_keys[i]]);
    key.set("player", player);
    key.set("context", ctx);
    key.set("map", Json::make_bool(d["decision"].str() == "map_select"));
    return key;
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
    Node* gold = bar->find_unique("GoldLabel");
    if (gold && gold->parent && gold->parent->name == "TopBarGold" && !gold->parent->behavior) {
        /* the count starts from the gold the decision before had */
        int was = before.is_object() && before["gold"].is_number() ? before["gold"].i() : player["gold"].i();
        set_label(gold, format("%d", was));
        GoldCount* count = new GoldCount(gold, was);
        gold->parent->behavior = count;
        count->set(player["gold"].i());
    } else {
        set_label(gold, format("%d", player["gold"].i()));
    }

    make_potions(lib, bar, player, before, anims ? &anims->tweens : NULL, anims);
    Node* shortcut = find_named(bar, "PotionShortcutButton");
    show_glyph(lib, shortcut ? shortcut->find_unique("HotkeyIcon") : NULL, "x");

    /* Room, floor, boss. */
    Node* room_icon_node = find_named(bar, "RoomIcon");
    set_room_icon(lib, room_icon_node ? room_icon_node->get_node("Icon") : NULL, room_icon(ctx));
    set_label(find_named(bar, "FloorNumLabel"), format("%d", ctx["total_floor"].i(ctx["floor"].i())));
    Node* boss = find_named(bar, "BossIcon");
    if (boss) {
        /* NTopBarBossIcon.OnRoomEntered / RefreshBossIcon: hidden in a boss
         * room, unless it is the first of two (then the second's icon
         * only); with two bosses the second's icon (second_boss_icon.tscn,
         * its tint the act's traveled colour at a boss, else untraveled)
         * at (30, 22) behind the first */
        bool in_boss_room = ctx["room_type"].str() == "Boss";
        bool two = ctx["second_boss"].is_string();
        bool only_second = two && ctx["at_boss"].b();
        Node* icon = boss->get_node("Icon");
        if (in_boss_room && !only_second) {
            hide(boss);
        } else if (only_second) {
            set_room_icon(lib, icon, "images/ui/run_history/" + lower(ctx["second_boss"].str()) + ".png");
        } else if (ctx["boss"]["id"].is_string()) {
            set_room_icon(lib, icon, "images/ui/run_history/" + lower(ctx["boss"]["id"].str()) + ".png");
            Node* second = two && icon ? instance(lib, "scenes/ui/top_bar/second_boss_icon.tscn") : NULL;
            if (second && second->is_control()) {
                icon->add_child(second);
                place((Control*)second, 30, 22, 44, 44);
                set_room_icon(lib, second, "images/ui/run_history/" + lower(ctx["second_boss"].str()) + ".png");
                set_visible(second, true);
                const Json& tint = ctx["boss_tint"];
                Node* tinted[2] = { second, second->find_unique("Outline") };
                for (int k = 0; k < 2 && tint.size() >= 3; ++k) {
                    if (!tinted[k] || !tinted[k]->is_canvas_item()) continue;
                    CanvasItem* c = (CanvasItem*)tinted[k];
                    c->shader_params["tint_color.r"] = tint[(size_t)0].f(1);
                    c->shader_params["tint_color.g"] = tint[1].f(1);
                    c->shader_params["tint_color.b"] = tint[2].f(1);
                }
            } else {
                delete second;
            }
        }
    }
    hide(find_named(bar, "Modifiers"));

    /* Right side: the save indicator is transparent at rest, the timer
     * hidden (shown only with the map or a capstone screen open). */
    Node* save = find_named(bar, "SaveIndicator");
    if (save && save->is_canvas_item()) {
        CanvasItem* s = (CanvasItem*)save;
        s->modulate = Color(1, 1, 1, 0);
        set_label(save->get_node("Label"), Loc::get("gameplay_ui", "GAME_SAVED"));
        /* NSaveIndicator.SavedGame (SaveManager.Saved): white 0.5 s on (in
         * 1 s), 0.5 s held, then out (1 s) */
        bool saved = false;
        const Json& steps = d["steps"];
        for (size_t i = 0; i < steps.size(); ++i)
            for (size_t k = 0; k < steps[i].size(); ++k)
                if (steps[i][k]["t"].str() == "saved") {
                    saved = true;
                    CueLedger::used(steps[i][k]);
                }
        if (saved && !s->behavior) s->behavior = new SavedFade();
    }
    Node* timer = find_named(bar, "TimerContainer");
    hide(timer);
    if (timer && timer->parent && !timer->parent->behavior && ctx["run_time"].is_number())
        timer->parent->behavior = new RunTimer(timer, ctx["run_time"].num(),
                                               ctx["room_type"].str() == "Map" || d["decision"].str() == "map_select");
    top_button(lib, find_named(bar, "Map"), "back", !in_map_room);
    top_button(lib, find_named(bar, "Deck"), "lb", true);
    top_button(lib, find_named(bar, "PauseButton"), "start", true);
    Node* deck = find_named(bar, "Deck");
    Node* deck_count = find_named(deck, "DeckCardCount");
    set_label(deck_count, format("%d", player["deck_size"].i()));
    if (anims && deck_count && deck_count->is_control() && before["deck_size"].is_number() &&
        player["deck_size"].i() > before["deck_size"].i()) {
        /* NTopBarDeckButton.OnPileContentsChanged: a bigger deck bumps the
         * count (1.5 -> 1, 0.5 s Expo Out, about its centre) */
        Control* c = (Control*)deck_count;
        c->pivot_offset = Vec2(c->offset[SIDE_RIGHT] - c->offset[SIDE_LEFT], c->offset[SIDE_BOTTOM] - c->offset[SIDE_TOP]) * 0.5f;
        anims->tweens.tween_from(&c->scale, Vec2(1.5f, 1.5f), Vec2(1, 1), 0.5f, TRANS_EXPO, EASE_OUT);
    }
    hide(find_named(bar, "GoldPopup"));

    Control* root = full_rect_control("TopBarLayer");
    root->add_child(make_relics(lib, player, before, anims));
    root->add_child(bar);
    return root;
}

} /* namespace spire */
