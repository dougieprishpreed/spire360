#include "deck_previews.h"
#include "card_view.h"
#include "combat_vfx.h"
#include "screen_shake.h"
#include "top_bar.h"
#include "ui_util.h"

#include "../audio/game_audio.h"
#include "../scene/controls.h"
#include "../scene/node.h"
#include "../scene/particles.h"
#include "../scene/scene.h"
#include "../scene/script.h"
#include "../scene/tween.h"
#include "../scene/viewport.h"

#include <math.h>

namespace spire {

namespace {

const float PI_F = 3.14159265f;
/* NCard.defaultSize */
const float CARD_W = 300, CARD_H = 422;

enum Box { BOX_ROW, BOX_GRID, BOX_EVENT, BOX_MESSY, BOX_COUNT };

float chaotic_range(float lo, float hi) { return lo + (hi - lo) * (float)(chaotic_rand() % 100001) / 100000.0f; }

void set_pos(Node* n, const Vec2& p) {
    if (n->is_node2d()) ((Node2D*)n)->position = p;
    else if (n->is_control()) ((Control*)n)->set_position(p);
}

Vec2 global_origin(Node* n) { return n && n->is_canvas_item() ? ((CanvasItem*)n)->global_transform().origin : Vec2(0, 0); }

float lerp_angle(float from, float to, float k) {
    float d = fmodf(to - from, 2 * PI_F);
    d = fmodf(2 * d, 2 * PI_F) - d;
    return from + d * k;
}

/* NMessyCardPreviewContainer.PoissonDiscSampler: samples at least radius
 * apart in a w x h rectangle, the first at its centre. */
class Poisson {
public:
    Poisson(float w, float h, float radius)
        : w_(w), h_(h), r2_(radius * radius), cell_(radius / sqrtf(2.0f)), first_(true) {
        gw_ = (int)ceilf(w / cell_);
        gh_ = (int)ceilf(h / cell_);
        grid_.assign((size_t)(gw_ * gh_), Vec2(0, 0));
    }
    bool next(Vec2* out) {
        if (first_) {
            first_ = false;
            *out = add(Vec2(w_ * 0.5f, h_ * 0.5f));
            return true;
        }
        while (!active_.empty()) {
            size_t i = (size_t)(chaotic_range(0, 1) * (float)active_.size());
            if (i >= active_.size()) i = active_.size() - 1;
            Vec2 v = active_[i];
            for (int j = 0; j < 30; ++j) {
                float s = PI_F * 2 * chaotic_range(0, 1);
                float d = sqrtf(chaotic_range(0, 1) * 3 * r2_ + r2_);
                Vec2 p = v + Vec2(cosf(s), sinf(s)) * d;
                if (p.x >= 0 && p.y >= 0 && p.x < w_ && p.y < h_ && far_enough(p)) {
                    *out = add(p);
                    return true;
                }
            }
            active_[i] = active_.back();
            active_.pop_back();
        }
        return false;
    }

private:
    Vec2 add(const Vec2& p) {
        active_.push_back(p);
        int gx = (int)(p.x / cell_), gy = (int)(p.y / cell_);
        if (gx >= 0 && gy >= 0 && gx < gw_ && gy < gh_) grid_[(size_t)(gy * gw_ + gx)] = p;
        return p;
    }
    bool far_enough(const Vec2& p) const {
        int gx = (int)(p.x / cell_), gy = (int)(p.y / cell_);
        for (int y = gy - 2 > 0 ? gy - 2 : 0; y <= gy + 2 && y < gh_; ++y)
            for (int x = gx - 2 > 0 ? gx - 2 : 0; x <= gx + 2 && x < gw_; ++x) {
                Vec2 q = grid_[(size_t)(y * gw_ + x)];
                if (q.x == 0 && q.y == 0) continue;
                Vec2 d = q - p;
                if (d.x * d.x + d.y * d.y < r2_) return false;
            }
        return true;
    }
    float w_, h_, r2_, cell_;
    int gw_, gh_;
    bool first_;
    std::vector<Vec2> grid_, active_;
};

/* NCardTransformShineVfx's exports, read when its scene is made. */
class TransformShine : public ScriptBehavior {
public:
    explicit TransformShine(Node* n)
        : show(value(n, "_overlayShowDuration", 0.75f)), idle(value(n, "_overlayIdleDuration", 0.125f)),
          hide(value(n, "_overlayHideDuration", 0.25f)), glow_fade(value(n, "_glowFadeDuration", 0.25f)),
          glow_top(value(n, "_glowTopScale", 1.5f)), shine_delay(value(n, "_shineDelay", 0.125f)),
          end_delay(value(n, "_endParticlesDelay", 0.4f)) {
        curve(n, "_anticipationScaleCurve", anticipation);
        curve(n, "_revealScaleCurve", reveal);
    }
    float show, idle, hide, glow_fade, glow_top, shine_delay, end_delay;
    CurveData anticipation[2], reveal[2];
    /* The card's scale at k (0..1) along a CurveXyzTexture (1 without). */
    static Vec2 at(const CurveData* c, float k) {
        return Vec2(c[0].valid() ? c[0].sample(k) : 1.0f, c[1].valid() ? c[1].sample(k) : 1.0f);
    }

private:
    static float value(Node* n, const char* key, float fallback) {
        const Json* v = n->get_extra(key);
        return v && v->is_number() ? (float)v->num() : fallback;
    }
    static void curve(Node* n, const char* key, CurveData* out) {
        const Json* v = n->get_extra(key);
        Resource* r = v ? script_resource(*v) : NULL;
        if (!r) return;
        CurveData::from_xyz(r, 0, out[0]);
        CurveData::from_xyz(r, 1, out[1]);
    }
};

ScriptBehavior* make_transform_shine(Node* n) { return new TransformShine(n); }

/* NCardRemoveVfx: the anticipation particles, the slash 0.25 s later, its
 * end and the card burning 0.1 s after that; gone 2 s later. */
class CardRemove : public ScriptBehavior {
public:
    explicit CardRemove(Node* n) : t_(0), step_(0) {
        const Json* a = n->get_extra("_anticipationDuration");
        const Json* s = n->get_extra("_slashEndDelay");
        anticipation_ = a && a->is_number() ? a->num() : 0.25;
        slash_end_ = s && s->is_number() ? s->num() : 0.1;
    }
    virtual void ready(Node* n) {
        vfx_emitting(vfx_exported(n, "_cardParticles"), false);
        vfx_restart(vfx_exported(n, "_anticipationParticles"));
    }
    virtual void process(Node* n, double dt) {
        t_ += dt;
        if (step_ == 0 && t_ >= anticipation_) {
            vfx_restart(vfx_exported(n, "_slashStartParticles"));
            step_ = 1;
        }
        if (step_ == 1 && t_ >= anticipation_ + slash_end_) {
            vfx_restart(vfx_exported(n, "_slashEndParticles"));
            vfx_restart(vfx_exported(n, "_cardParticles"));
            step_ = 2;
        }
        if (step_ == 2 && t_ >= anticipation_ + slash_end_ + 2 && !n->queued_for_deletion()) n->queue_free();
    }

private:
    double t_, anticipation_, slash_end_;
    int step_;
};

ScriptBehavior* make_card_remove(Node* n) { return new CardRemove(n); }

class Layer;

/* One preview: its card (and the effect's scene around it), its timeline,
 * then NCardFlyVfx into the deck. */
class Preview {
public:
    Preview(Layer* layer, const Json& cue) : layer_(layer), cue_(cue), t_(0), root_(NULL), card_(NULL), flying_(false),
                                             phase_(0), speed_(0), accel_(0), dur_(1), ftime_(0), arc_(0),
                                             trail_(NULL), trail_t_(-1) {}
    virtual ~Preview() {}
    /* False once it is over (its nodes are freed). */
    bool update(float dt);

protected:
    /* The timeline before the flight; true when the card is to fly now. */
    virtual bool step(float dt) = 0;
    Control* new_card(const Json& card);
    void grow_in(Control* card);
    void start_flight();
    void free_all();

    Layer* layer_;
    Json cue_;
    float t_;
    Node* root_;    /* the container's child */
    Control* card_; /* the NCard */
    TweenSet tweens_;

private:
    bool fly(float dt);
    bool flying_;
    int phase_;
    float speed_, accel_, dur_, ftime_, arc_;
    Vec2 from_, to_;
    Node* trail_;
    float trail_t_; /* fading out (NCardTrailVfx.FadeOut) */
};

class Layer : public ScriptBehavior {
public:
    Layer(SceneLibrary* lib) : lib(lib), layer_node(NULL), trails(NULL), now(0), messy_reset(0), messy(NULL) {
        for (int i = 0; i < BOX_COUNT; ++i) boxes[i] = NULL;
    }
    ~Layer() {
        for (size_t i = 0; i < previews.size(); ++i) delete previews[i];
        delete messy;
    }
    virtual void process(Node*, double dt) {
        now += dt;
        for (size_t i = previews.size(); i-- > 0;)
            if (!previews[i]->update((float)dt)) {
                delete previews[i];
                previews.erase(previews.begin() + (long)i);
            }
    }
    /* A preview's node into a container, laid out as the game's script
     * does when a child enters it. */
    void add(int box, Node* n) {
        Control* c = boxes[box];
        c->add_child(n);
        Vec2 size = sizes[box];
        if (box == BOX_MESSY) {
            /* NMessyCardPreviewContainer.PositionNewChild: a new scatter
             * once 2 s went by without a card */
            Vec2 p;
            if (!messy || now > messy_reset) {
                delete messy;
                messy = new Poisson(size.x, size.y, 150);
            }
            messy_reset = now + 2;
            if (!messy->next(&p)) {
                delete messy;
                messy = new Poisson(size.x, size.y, 150);
                messy->next(&p);
            }
            set_pos(n, p);
            return;
        }
        Vec2 mid = size * 0.5f + Vec2(0, 50);
        int count = (int)c->children.size();
        if (box == BOX_ROW) {
            /* NCardPreviewContainer.ReformatElements */
            for (int i = 0; i < count; ++i)
                set_pos(c->children[(size_t)i], mid + Vec2(-(float)(count - 1) * 325 * 0.5f + 325.0f * (float)i, 0));
            return;
        }
        /* NGridCardPreviewContainer.ReformatElements */
        int cols = (int)floorf(1920.0f / (CARD_W + 25));
        int rows = (count + cols - 1) / cols;
        for (int i = 0; i < count; ++i) {
            int row = i / cols, col = i % cols;
            int in_row = cols < count - row * cols ? cols : count - row * cols;
            float y0 = -(float)(rows - 1) * (CARD_H + 25) * 0.5f;
            float x0 = -(float)(in_row - 1) * (CARD_W + 25) * 0.5f;
            set_pos(c->children[(size_t)i], mid + Vec2(x0 + (CARD_W + 25) * (float)col, y0 + (CARD_H + 25) * (float)row));
        }
    }
    /* PileType.Draw / Discard / Exhaust.GetTargetPosition: the combat UI's
     * pile button's centre. */
    Vec2 pile_position(const std::string& pile) {
        const char* name = pile == "draw" ? "DrawPile" : pile == "exhaust" ? "ExhaustPile" : "DiscardPile";
        Node* root = layer_node ? layer_node->parent : NULL;
        for (size_t i = 0; root && i < root->children.size(); ++i) {
            Node* ui = find_named(root->children[i], "CombatUi");
            Node* p = ui ? find_named(ui, name) : NULL;
            if (p && p->is_control()) {
                Control* c = (Control*)p;
                return c->global_transform().xform(c->size * 0.5f);
            }
        }
        return pile == "draw" ? Vec2(80, 1000) : Vec2(1840, 1000);
    }
    /* PileType.Deck.GetTargetPosition: the top bar's deck button. */
    Vec2 deck_position() {
        Node* root = layer_node ? layer_node->parent : NULL;
        for (size_t i = 0; root && i < root->children.size(); ++i) {
            Node* bar = root->children[i]->get_node("TopBarLayer");
            Node* deck = bar ? find_named(bar, "Deck") : NULL;
            if (deck && deck->is_control()) {
                Control* d = (Control*)deck;
                return d->global_transform().xform(d->size * 0.5f);
            }
        }
        return Vec2(1760, 40);
    }

    SceneLibrary* lib;
    Node* layer_node;
    Control* boxes[BOX_COUNT];
    Vec2 sizes[BOX_COUNT];
    Node* trails;   /* TopBar.TrailContainer */
    std::string character;
    std::vector<Preview*> previews;
    double now, messy_reset;
    Poisson* messy;
};

Control* Preview::new_card(const Json& card) {
    Node* n = make_card(layer_->lib, card);
    if (n && !n->is_control()) {
        delete n;
        n = NULL;
    }
    return (Control*)n;
}

/* The card grows in from nothing (0.25 s, Cubic Out). */
void Preview::grow_in(Control* card) {
    tweens_.tween_from(&card->scale, Vec2(0, 0), Vec2(1, 1), 0.25f, TRANS_CUBIC, EASE_OUT);
}

void Preview::free_all() {
    if (trail_ && !trail_->queued_for_deletion()) trail_->queue_free();
    trail_ = NULL;
    if (root_ && !root_->queued_for_deletion()) root_->queue_free();
    root_ = NULL;
    card_ = NULL;
}

/* NCardFlyVfx._Ready / PlayAnim: an arc into the deck, speeding up, the
 * card darkening and shrinking, its trail behind it. */
void Preview::start_flight() {
    if (!card_) return;
    flying_ = true;
    phase_ = 0;
    ftime_ = 0;
    from_ = global_origin(card_);
    const std::string& pile = cue_["pile"].str();
    to_ = pile.empty() ? layer_->deck_position() : layer_->pile_position(pile);
    float offset = chaotic_range(100, 400);
    speed_ = chaotic_range(1.1f, 1.25f);
    accel_ = chaotic_range(2, 2.5f);
    arc_ = to_.y < 540.0f ? -500.0f : 500.0f + offset;
    dur_ = chaotic_range(1, 1.75f);
    if (layer_->lib->audio())
        layer_->lib->audio()->ui(pile.empty() ? "event:/sfx/ui/cards/card_movement_B_into_deck"
                                 : pile == "draw" ? "event:/sfx/ui/cards/card_movement_B_into_draw"
                                                  : "event:/sfx/ui/cards/card_movement_B_into_discard");
    /* NCardTrailVfx._Ready: its sprites fade in and shrink, the card to 0.75 */
    trail_ = layer_->character.empty() ? NULL : instance(layer_->lib, "scenes/vfx/card_trail_" + layer_->character + ".tscn");
    if (trail_ && layer_->trails) {
        layer_->trails->add_child(trail_);
        Node* sprites = trail_->get_node("Sprites");
        if (sprites && sprites->is_canvas_item()) {
            CanvasItem* sp = (CanvasItem*)sprites;
            sp->modulate.a = 0;
            tweens_.tween(&sp->modulate.a, 1.0f, 1.0f, TRANS_CUBIC, EASE_OUT);
            if (sp->is_node2d()) tweens_.tween(&((Node2D*)sp)->scale, Vec2(0.5f, 0.5f), 0.5f, TRANS_CUBIC, EASE_IN, 0.25f);
        }
        tweens_.tween(&card_->modulate.a, 0.75f, 0.5f, TRANS_CUBIC, EASE_OUT);
    } else {
        delete trail_;
        trail_ = NULL;
    }
}

static void stop_particles(Node* n) {
    if (n->type == "GPUParticles2D" || n->type == "CPUParticles2D") ((Particles2D*)n)->emitting = false;
    for (size_t i = 0; i < n->children.size(); ++i) stop_particles(n->children[i]);
}

bool Preview::fly(float k) {
    Control* body = (Control*)NULL;
    Node* b = find_named(card_, "CardContainer");
    if (b && b->is_control()) body = (Control*)b;
    Xform2D to_local = card_->parent && card_->parent->is_canvas_item() ? inverse(((CanvasItem*)card_->parent)->global_transform())
                                                                         : Xform2D();
    if (phase_ == 0) {
        ftime_ += speed_ * k;
        speed_ += accel_ * k;
        Vec2 c = from_ + (to_ - from_) * 0.5f;
        c.y -= arc_;
        float u = ftime_ / dur_;
        if (u > 1) {
            card_->set_position(to_local.xform(to_));
            phase_ = 1;
            ftime_ = 0;
        } else {
            float u2 = (ftime_ + 0.05f) / dur_;
            Vec2 p = from_ * ((1 - u) * (1 - u)) + c * (2 * (1 - u) * u) + to_ * (u * u);
            Vec2 q = from_ * ((1 - u2) * (1 - u2)) + c * (2 * (1 - u2) * u2) + to_ * (u2 * u2);
            card_->set_position(to_local.xform(p));
            card_->rotation = lerp_angle(card_->rotation, atan2f(q.y - p.y, q.x - p.x) + PI_F / 2, k * 12 < 1 ? k * 12 : 1);
            float d = ftime_ * 3 / dur_;
            d = d < 0 ? 0 : d > 1 ? 1 : d;
            if (body) {
                body->modulate = Color(1 - d, 1 - d, 1 - d, body->modulate.a);
                float s = 1 + (0.1f - 1) * d;
                body->scale = Vec2(s, s);
            }
        }
    } else {
        ftime_ += speed_ * k;
        float u = ftime_ / dur_;
        if (u > 0.25f && trail_ && trail_t_ < 0) {
            stop_particles(trail_);
            trail_t_ = 0;
        }
        if (body) {
            float s = 0.1f + (-0.15f - 0.1f) * u;
            s = s < 0 ? 0 : s;
            body->scale = Vec2(s, s);
        }
        if (u > 1) return false;
    }
    if (trail_ && trail_->is_node2d()) {
        Node2D* tr = (Node2D*)trail_;
        tr->position = global_origin(card_);
        tr->rotation = card_->rotation;
    }
    return true;
}

bool Preview::update(float dt) {
    t_ += dt;
    tweens_.update(dt);
    if (trail_t_ >= 0 && trail_) {
        trail_t_ += dt;
        if (trail_->is_canvas_item()) ((CanvasItem*)trail_)->modulate.a = trail_t_ < 0.5f ? 1 - trail_t_ / 0.5f : 0.0f;
        if (trail_t_ >= 0.5f) {
            trail_->queue_free();
            trail_ = NULL;
        }
    }
    if (!flying_) {
        if (step(dt)) {
            if (!card_) {
                free_all();
                return false;
            }
            start_flight();
        }
        return root_ != NULL;
    }
    if (!card_ || !fly(dt)) {
        /* the card is gone into the deck (its trail fades on) */
        if (root_ && !root_->queued_for_deletion()) root_->queue_free();
        root_ = NULL;
        card_ = NULL;
        return trail_ != NULL;
    }
    return true;
}

/* NGlobalUi.ReparentCard: a picked reward card in the top bar's trail
 * container where it was, flying into the deck at once. */
class PickedPreview : public Preview {
public:
    PickedPreview(Layer* l, const Json& card, const Vec2& at, float scale) : Preview(l, Json()) {
        card_ = new_card(card);
        if (!card_) return;
        root_ = card_;
        Node* parent = l->trails ? l->trails : l->layer_node;
        parent->add_child(card_);
        Xform2D pg = parent->is_canvas_item() ? ((CanvasItem*)parent)->global_transform() : Xform2D();
        card_->set_position(inverse(pg).xform(at));
        card_->scale = Vec2(scale, scale);
    }
    virtual bool step(float) { return true; }
};

/* NRelicFlashVfx.StartVfx on a card (NCard.FlashRelicOnCard: twice its
 * size at the card's middle): up 64 px (Expo Out, 1 s), its three icons
 * 0.2 s apart, each growing to 1.25 (Cubic Out, 1 s) as it fades (1.5 s). */
class CardRelicFlash : public ScriptBehavior {
public:
    explicit CardRelicFlash(const Vec2& at) : t_(0), at_(at) {}
    virtual void process(Node* n, double dt) {
        t_ += (float)dt;
        if (n->is_control()) {
            float k = t_ < 1 ? t_ : 1;
            ((Control*)n)->set_position(at_ - Vec2(0, 64 * (k >= 1 ? 1 : 1 - powf(2, -10 * k))));
        }
        for (int i = 0; i < 3; ++i) {
            Node* img = n->get_node(format("Image%d", i + 1));
            if (!img || !img->is_control()) continue;
            Control* c = (Control*)img;
            float lt = t_ - 0.2f * (float)i;
            if (lt < 0) {
                c->modulate.a = 0;
                continue;
            }
            float g = lt < 1 ? lt : 1, u = 1 - g;
            float s = 1 + 0.25f * (1 - u * u * u);
            c->scale = Vec2(s, s);
            c->modulate.a = lt < 1.5f ? 1 - lt / 1.5f : 0.0f;
        }
        if (t_ >= 1.95f && !n->queued_for_deletion()) n->queue_free();
    }

private:
    float t_;
    Vec2 at_;
};

/* CardCmd.PreviewInternal: grows in, flies after the cue's time; the
 * relics that changed the card flash on it and in the top bar as it has
 * grown in (FlashRelics). */
class AddPreview : public Preview {
public:
    AddPreview(Layer* l, const Json& cue, int box) : Preview(l, cue), wait_((float)cue["time"].num(1.2)), flashed_(false) {
        card_ = new_card(cue["card"]);
        if (!card_) return;
        root_ = card_;
        l->add(box, card_);
        grow_in(card_);
    }
    virtual bool step(float) {
        if (!flashed_ && t_ >= 0.25f) {
            flashed_ = true;
            flash_relics();
        }
        return t_ >= 0.25f + wait_;
    }

private:
    void flash_relics() {
        const Json& relics = cue_["relics"];
        Node* root = layer_->layer_node ? layer_->layer_node->parent : NULL;
        Node* body = card_ ? find_named(card_, "CardContainer") : NULL;
        for (size_t i = 0; root && i < relics.size(); ++i) {
            int index = relics[i].i(-1);
            Node* holder = NULL;
            for (size_t k = 0; !holder && k < root->children.size(); ++k) {
                Node* bar = root->children[k]->get_node("TopBarLayer");
                Node* row = bar ? find_named(bar, "RelicInventory") : NULL;
                if (row && index >= 0 && index < (int)row->children.size()) holder = row->children[(size_t)index];
            }
            if (!holder || !holder->is_control()) continue;
            /* RelicModel.Flash: the top bar holder's flash */
            Node* hf = make_relic_holder_flash(layer_->lib, (Control*)holder);
            if (hf) layer_->layer_node->add_child(hf);
            Node* hi = holder->find_unique("Icon");
            Texture2DRes* icon = hi && hi->type == "TextureRect" ? ((TextureRect*)hi)->texture : NULL;
            if (!body || !body->is_control()) continue;
            Node* fx = instance(layer_->lib, "scenes/vfx/relic_flash_vfx.tscn");
            if (!fx || !fx->is_control()) {
                delete fx;
                continue;
            }
            for (int k = 1; k <= 3; ++k) {
                Node* img = fx->get_node(format("Image%d", k));
                if (img && icon && img->type == "TextureRect") ((TextureRect*)img)->texture = icon;
                if (img && img->is_canvas_item()) ((CanvasItem*)img)->modulate.a = 0;
            }
            Control* c = (Control*)fx;
            for (int s = 0; s < 4; ++s) c->anchor[s] = 0;
            c->scale = Vec2(2, 2);
            Vec2 at = ((Control*)body)->size * 0.5f;
            c->set_position(at);
            fx->behavior = new CardRelicFlash(at);
            body->add_child(fx);
        }
    }
    float wait_;
    bool flashed_;
};

/* CardPileCmd.RemoveFromDeck: grows in, NCardRemoveVfx 0.25 s later, the
 * card gone 0.4 s after that. */
class RemovePreview : public Preview {
public:
    RemovePreview(Layer* l, const Json& cue) : Preview(l, cue), burnt_(false) {
        card_ = new_card(cue["card"]);
        if (!card_) return;
        root_ = card_;
        l->add(BOX_ROW, card_);
        grow_in(card_);
    }
    virtual bool step(float) {
        if (!burnt_ && t_ >= 0.5f && card_) {
            burnt_ = true;
            Node* fx = instance(layer_->lib, "scenes/vfx/ui/card/vfx_card_remove.tscn");
            if (fx && fx->is_control() && layer_->layer_node) {
                layer_->layer_node->add_child(fx);
                ((Control*)fx)->set_position(global_origin(card_));
                ((Control*)fx)->rotation = card_->rotation;
            } else {
                delete fx;
            }
        }
        if (t_ >= 0.9f) {
            free_all();
            return true; /* nothing left to fly */
        }
        return false;
    }

private:
    bool burnt_;
};

/* NCardUpgradeVfx: the card (first child) grows in with its sparkles, flies
 * 1.75 s later. */
class UpgradePreview : public Preview {
public:
    UpgradePreview(Layer* l, const Json& cue, int box) : Preview(l, cue) {
        root_ = instance(l->lib, "scenes/vfx/vfx_card_upgrade.tscn");
        card_ = root_ ? new_card(cue["card"]) : NULL;
        if (!card_) {
            delete root_;
            root_ = NULL;
            return;
        }
        if (root_->children.empty()) root_->add_child(card_);
        else root_->add_child_before(card_, root_->children[0]);
        Node* p = root_->find_unique("Particle");
        if (p && (p->type == "CPUParticles2D" || p->type == "GPUParticles2D")) ((Particles2D*)p)->emitting = true;
        l->add(box, root_);
        grow_in(card_);
    }
    virtual bool step(float) { return t_ >= 1.75f; }
};

/* NCardSmithVfx.PlayAnimation(cards) in GlobalUi's CardPreviewContainer
 * (the rest site's smith): the card grows in (0.25 s), card_smith.mp3, then
 * three hammer blows (the scene's Spark1-3 particles, a weak short shake at
 * 180 +-10 degrees, the card swinging to 20, -10, 5 degrees, Elastic Out
 * 0.15 s) at 0.25, 0.5 and 0.75 s; it flies to its pile 0.5 s after the
 * last. */
class SmithPreview : public Preview {
public:
    SmithPreview(Layer* l, const Json& cue, const Json& card) : Preview(l, cue), blows_(0) {
        root_ = instance(l->lib, "scenes/vfx/vfx_card_smith.tscn");
        card_ = root_ ? new_card(card) : NULL;
        Node* box = root_ ? root_->find_unique("CardContainer") : NULL;
        if (!card_ || !box) {
            delete card_;
            delete root_;
            root_ = NULL;
            card_ = NULL;
            return;
        }
        box->add_child(card_);
        l->add(BOX_ROW, root_);
        grow_in(card_);
    }
    virtual bool step(float) {
        static const float DEGREES[3] = { 20, -10, 5 };
        while (blows_ < 3 && t_ >= 0.25f * (float)(blows_ + 1)) {
            if (blows_ == 0 && layer_->lib->audio()) layer_->lib->audio()->ui("card_smith.mp3");
            Node* spark = root_ ? root_->get_node(format("Spark%d", blows_ + 1)) : NULL;
            for (size_t i = 0; spark && i < spark->children.size(); ++i)
                if (spark->children[i]->type == "GPUParticles2D" || spark->children[i]->type == "CPUParticles2D")
                    ((Particles2D*)spark->children[i])->restart();
            ScreenShake::get().shake("Weak", "Short", 180.0f + chaotic_range(-10, 10));
            if (card_) tweens_.tween(&card_->rotation, DEGREES[blows_] * PI_F / 180.0f, 0.15f, TRANS_ELASTIC, EASE_OUT);
            ++blows_;
        }
        return t_ >= 1.25f;
    }

private:
    int blows_;
};

/* NCardTransformVfx with NCardTransformShineVfx: the original grows in;
 * 0.75 s later the overlay shows over it and the card swells
 * (_anticipationScaleCurve), it becomes the new card, the overlay goes, the
 * border glows out and the particles burst (_revealScaleCurve); it flies
 * 0.8 s after the shine. */
class TransformPreview : public Preview {
public:
    TransformPreview(Layer* l, const Json& cue, int box)
        : Preview(l, cue), shine_(NULL), data_(NULL), overlay_(NULL), glow_(NULL), stage_(0), swap_at_(0), fly_at_(0),
          shined_(false) {
        root_ = instance(l->lib, "scenes/vfx/vfx_card_transform.tscn");
        const Json& first = cue["from"].is_object() ? cue["from"] : cue["card"];
        card_ = root_ ? new_card(first) : NULL;
        Node* box_node = root_ ? root_->find_unique("CardContainer") : NULL;
        if (!card_ || !box_node) {
            delete card_;
            delete root_;
            root_ = NULL;
            card_ = NULL;
            return;
        }
        box_node->add_child(card_);
        l->add(box, root_);
        grow_in(card_);
        if (l->lib->audio()) l->lib->audio()->ui("event:/sfx/ui/cards/card_transform");
        if (!cue["from"].is_object()) {
            /* no original to turn: the new card waits as long, then flies */
            stage_ = 3;
            fly_at_ = 0.75f + 0.875f + 0.525f + 0.8f;
        }
    }
    virtual bool step(float) {
        if (stage_ == 0 && t_ >= 0.75f) begin_shine();
        if (stage_ == 1) {
            float k = (t_ - 0.75f) / (data_->show + data_->idle);
            card_->scale = TransformShine::at(data_->anticipation, k < 1 ? k : 1);
            if (k >= 1) reveal();
        }
        if (stage_ == 2) {
            float s = t_ - swap_at_;
            float k = s / data_->glow_fade;
            card_->scale = TransformShine::at(data_->reveal, k < 1 ? k : 1);
            if (s >= data_->shine_delay && !shined_) {
                shined_ = true;
                vfx_restart(vfx_exported(shine_, "_shineParticles"));
            }
            if (s >= data_->shine_delay + data_->end_delay) {
                vfx_restart(vfx_exported(shine_, "_endParticles"));
                stage_ = 3;
                fly_at_ = t_ + 0.8f;
            }
        }
        return stage_ == 3 && t_ >= fly_at_;
    }

private:
    void begin_shine() {
        stage_ = 1;
        Node* vfx_box = find_named(card_, "CardVfxContainer");
        shine_ = vfx_box ? instance(layer_->lib, "scenes/vfx/ui/card/vfx_card_transform.tscn") : NULL;
        data_ = shine_ ? dynamic_shine(shine_) : NULL;
        if (!data_) {
            delete shine_;
            shine_ = NULL;
            swap();
            stage_ = 3;
            fly_at_ = t_ + 0.8f;
            return;
        }
        vfx_box->add_child(shine_);
        Node* o = vfx_exported(shine_, "_overlay");
        Node* g = vfx_exported(shine_, "_borderGlow");
        overlay_ = o && o->is_canvas_item() ? (CanvasItem*)o : NULL;
        glow_ = g && g->is_control() ? (Control*)g : NULL;
        if (glow_) glow_->self_modulate.a = 0;
        if (overlay_) {
            overlay_->self_modulate = Color(1, 1, 1, 0);
            tweens_.tween(&overlay_->self_modulate.a, 1.0f, data_->show, TRANS_QUART, EASE_IN_OUT);
        }
    }
    static TransformShine* dynamic_shine(Node* n) {
        return n->script == "res://src/Core/Nodes/Vfx/Cards/NCardTransformShineVfx.cs" && n->behavior ? (TransformShine*)n->behavior : NULL;
    }
    /* UpdateCard: the same node shows the new card (the shine stays on it) */
    void swap() {
        Control* fresh = new_card(cue_["card"]);
        if (!fresh || !card_->parent) {
            delete fresh;
            return;
        }
        Node* parent = card_->parent;
        Node* old_box = find_named(card_, "CardVfxContainer");
        Node* new_box = find_named(fresh, "CardVfxContainer");
        if (shine_ && old_box && new_box) {
            old_box->remove_child(shine_);
            new_box->add_child(shine_);
        }
        fresh->scale = card_->scale;
        parent->add_child_before(fresh, card_);
        parent->remove_child(card_);
        delete card_;
        card_ = fresh;
    }
    void reveal() {
        swap();
        stage_ = 2;
        swap_at_ = t_;
        if (glow_) {
            glow_->self_modulate.a = 1;
            glow_->scale = Vec2(1, 1);
            tweens_.tween(&glow_->scale, Vec2(data_->glow_top, data_->glow_top), data_->glow_fade, TRANS_QUINT, EASE_OUT);
            tweens_.tween(&glow_->self_modulate.a, 0.0f, data_->glow_fade, TRANS_QUINT, EASE_OUT);
        }
        if (overlay_) {
            tweens_.kill(&overlay_->self_modulate.a);
            tweens_.tween(&overlay_->self_modulate.a, 0.0f, data_->hide, TRANS_QUART, EASE_OUT);
        }
        vfx_restart(vfx_exported(shine_, "_revealParticles"));
    }

    Node* shine_;
    TransformShine* data_;
    CanvasItem* overlay_;
    Control* glow_;
    int stage_; /* 0 waiting, 1 overlay in, 2 revealed, 3 done */
    float swap_at_, fly_at_;
    bool shined_;
};

/* NCardEnchantVfx: the enchanted card (first child) with its enchantment
 * tab hidden and EnchantmentVfxOverride showing %EnchantmentViewport (the
 * enchantment's icon and amount) through enchantment_tab_appear, its
 * progress 0 -> 1 over 1 s (Quad InOut); the sparkles from 0.2 s moving
 * 72 px right over 0.4 s; it flies 2 s in. */
class EnchantPreview : public Preview {
public:
    EnchantPreview(Layer* l, const Json& cue) : Preview(l, cue), sparkles_(NULL), override_(NULL), lit_(false) {
        root_ = instance(l->lib, "scenes/vfx/vfx_card_enchant.tscn");
        card_ = root_ ? new_card(cue["card"]) : NULL;
        if (!card_) {
            delete root_;
            root_ = NULL;
            return;
        }
        if (root_->children.empty()) root_->add_child(card_);
        else root_->add_child_before(card_, root_->children[0]);
        /* _Ready: the viewport's icon and amount */
        const Json& ench = cue["card"]["enchantment_view"];
        Node* in = root_->find_unique("EnchantmentInViewport");
        if (in) {
            set_texture(l->lib, in->get_node("Icon"), ench["icon"].str());
            set_label(in->get_node("Label"), format("%d", ench["amount"].i()));
            Node* label = in->get_node("Label");
            if (label && label->is_canvas_item()) ((CanvasItem*)label)->visible = ench["show_amount"].b();
        }
        hide(card_->find_unique("Enchantment")); /* NCard.EnchantmentTab */
        Node* over = card_->find_unique("EnchantmentVfxOverride");
        Node* vp = root_->find_unique("EnchantmentViewport");
        if (over && vp && over->type == "TextureRect") {
            /* Viewport.GetTexture */
            ViewportTextureRes* t = new ViewportTextureRes();
            t->direct = vp->type == "SubViewport" ? (SubViewport*)vp : NULL;
            t->user = over;
            over->owned_resources.push_back(t);
            ((TextureRect*)over)->texture = t;
            ((CanvasItem*)over)->visible = true;
            override_ = (CanvasItem*)over;
            override_->shader_params["progress"] = 0;
            tweens_.tween(&override_->shader_params["progress"], 1.0f, 1.0f, TRANS_QUAD, EASE_IN_OUT);
        }
        Node* s = root_->find_unique("EnchantmentAppearSparkles");
        if (s && s->is_node2d()) sparkles_ = (Node2D*)s;
        l->add(BOX_ROW, root_);
        if (l->lib->audio()) l->lib->audio()->ui("event:/sfx/ui/enchant_shimmer");
    }
    virtual bool step(float) {
        if (!lit_ && t_ >= 0.2f && sparkles_) {
            lit_ = true;
            if (sparkles_->type == "GPUParticles2D" || sparkles_->type == "CPUParticles2D") ((Particles2D*)sparkles_)->emitting = true;
            else vfx_emitting(sparkles_, true);
            tweens_.tween(&sparkles_->position.x, sparkles_->position.x + 72, 0.4f);
        }
        return t_ >= 2.0f;
    }

private:
    Node2D* sparkles_;
    CanvasItem* override_;
    bool lit_;
};

int box_of(const std::string& style) {
    if (style == "GridLayout") return BOX_GRID;
    if (style == "EventLayout") return BOX_EVENT;
    if (style == "MessyLayout") return BOX_MESSY;
    return BOX_ROW;
}

} /* namespace */

static DeckPreviews* g_current = NULL;

DeckPreviews::DeckPreviews(SceneLibrary* lib) : lib_(lib), layer_(NULL) { g_current = this; }

DeckPreviews::~DeckPreviews() {
    if (g_current == this) g_current = NULL;
}

DeckPreviews* DeckPreviews::current() { return g_current; }

Node* DeckPreviews::layer() {
    if (layer_) return layer_;
    Control* root = full_rect_control("GlobalUi");
    root->mouse_filter = 2;
    Layer* l = new Layer(lib_);
    l->layer_node = root;
    /* run.tscn's GlobalUi: the containers' rects (the viewport 1920 x 1080) */
    static const char* const NAMES[BOX_COUNT] = { "CardPreviewContainer", "GridCardPreviewContainer",
                                                  "EventCardPreviewContainer", "MessyCardPreviewContainer" };
    static const float OFFSETS[BOX_COUNT][4] = { { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, -931, 0 }, { 263, 196, -302, -122 } };
    Control* trails = full_rect_control("TrailContainer");
    trails->mouse_filter = 2;
    root->add_child(trails);
    l->trails = trails;
    for (int i = 0; i < BOX_COUNT; ++i) {
        Control* c = full_rect_control(NAMES[i]);
        c->mouse_filter = 2;
        c->offset[SIDE_LEFT] = OFFSETS[i][0];
        c->offset[SIDE_TOP] = OFFSETS[i][1];
        c->offset[SIDE_RIGHT] = OFFSETS[i][2];
        c->offset[SIDE_BOTTOM] = OFFSETS[i][3];
        root->add_child(c);
        l->boxes[i] = c;
        l->sizes[i] = Vec2(1920 + OFFSETS[i][2] - OFFSETS[i][0], 1080 + OFFSETS[i][3] - OFFSETS[i][1]);
    }
    root->behavior = l;
    layer_ = root;
    return layer_;
}

void DeckPreviews::play(const Json& steps, const std::string& character) {
    for (size_t i = 0; i < steps.size(); ++i)
        for (size_t k = 0; k < steps[i].size(); ++k)
            if (steps[i][k]["t"].str() == "deck_card" && !steps[i][k]["pile"].is_string()) play_cue(steps[i][k], character);
}

void DeckPreviews::play_cue(const Json& c, const std::string& character) {
    Layer* l = layer_ ? (Layer*)layer_->behavior : NULL;
    if (!l) return;
    l->character = lower(character);
    const std::string& op = c["op"].str();
    int box = box_of(c["style"].str());
    Preview* p = NULL;
    if (op == "add" || op == "show") p = new AddPreview(l, c, box);
    else if (op == "remove") p = new RemovePreview(l, c);
    else if (op == "upgrade") p = new UpgradePreview(l, c, box);
    else if (op == "transform") p = new TransformPreview(l, c, box);
    else if (op == "enchant") p = new EnchantPreview(l, c);
    else if (op == "smith") {
        for (size_t i = 0; i < c["cards"].size(); ++i) l->previews.push_back(new SmithPreview(l, c, c["cards"][i]));
    }
    if (p) l->previews.push_back(p);
}

void DeckPreviews::fly_to_deck(const Json& card, float x, float y, float scale, const std::string& character) {
    Layer* l = layer_ ? (Layer*)layer_->behavior : NULL;
    if (!l) return;
    l->character = lower(character);
    l->previews.push_back(new PickedPreview(l, card, Vec2(x, y), scale));
}

void DeckPreviews::clear() {
    Layer* l = layer_ ? (Layer*)layer_->behavior : NULL;
    if (!l) return;
    for (size_t i = 0; i < l->previews.size(); ++i) delete l->previews[i];
    l->previews.clear();
    for (size_t i = 0; i < layer_->children.size(); ++i) {
        Node* c = layer_->children[i];
        for (size_t j = 0; j < c->children.size(); ++j)
            if (!c->children[j]->queued_for_deletion()) c->children[j]->queue_free();
        if (c->name != "TrailContainer" && c->name.find("PreviewContainer") == std::string::npos && !c->queued_for_deletion())
            c->queue_free();
    }
}

void install_deck_preview_scripts() {
    register_script("res://src/Core/Nodes/Vfx/Cards/NCardTransformShineVfx.cs", make_transform_shine);
    register_script("res://src/Core/Nodes/Vfx/Cards/NCardRemoveVfx.cs", make_card_remove);
}

} /* namespace spire */
