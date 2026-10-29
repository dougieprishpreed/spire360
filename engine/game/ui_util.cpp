#include "ui_util.h"

#include "card_view.h"
#include "loc.h"
#include "../scene/controls.h"
#include "../scene/node.h"
#include "../scene/rich_text.h"
#include "../scene/scene.h"
#include "../scene/text_nodes.h"

#include <ctype.h>
#include <math.h>
#include <time.h>
#include <set>

namespace spire {

static Json g_client_settings;

const Json& client_settings() { return g_client_settings; }

static bool g_capstone_open = false;
bool capstone_open() { return g_capstone_open; }
void set_capstone_open(bool open) { g_capstone_open = open; }
void set_client_settings(const Json& s) { g_client_settings = s; }

bool fast_mode() { return g_client_settings["fast_mode"].b(); }

std::string lower(std::string s) {
    for (size_t i = 0; i < s.size(); ++i) s[i] = (char)tolower((unsigned char)s[i]);
    return s;
}

Node* instance(SceneLibrary* lib, const std::string& path) {
    std::string err;
    Node* n = lib->instantiate(path, &err);
    if (!n) log_warn("screen: %s", err.c_str());
    return n;
}

Control* make_hover_tip(SceneLibrary* lib, const std::string& title, const std::string& description) {
    Node* n = instance(lib, "scenes/ui/hover_tip.tscn");
    if (!n || !n->is_control()) {
        delete n;
        return NULL;
    }
    Node* t = n->find_unique("Title");
    if (title.empty()) set_visible(t, false);
    else set_label(t, title);
    set_label(n->find_unique("Description"), description);
    set_visible(n->find_unique("Icon"), false); /* the relic's own tip: no icon */
    return (Control*)n;
}

Control* make_tip_set(SceneLibrary* lib, const Json& tips, bool split) {
    if (tips.size() == 0) return NULL;
    Control* box = (Control*)SceneLibrary::create_node("VBoxContainer");
    box->name = "HoverTipSet";
    box->add_theme_constant_override("separation", 5);
    box->mouse_filter = 2;
    /* NHoverTipCardContainer: the cards the tips name (card_hover_tip.tscn,
     * the card at 0.75), 4 px apart, beside the text tips */
    Control* cards = NULL;
    for (size_t i = 0; i < tips.size(); ++i) {
        if (!tips[i]["card"].is_object()) continue;
        Node* ct = instance(lib, "scenes/ui/card_hover_tip.tscn");
        Node* slot = ct ? ct->find_unique("Card") : NULL;
        Node* card = slot ? make_card(lib, tips[i]["card"]) : NULL;
        if (!ct || !ct->is_control() || !slot || !card || !card->is_control() || !slot->is_control()) {
            delete card;
            delete ct;
            continue;
        }
        Control* c = (Control*)card;
        Control* s = (Control*)slot;
        for (int k = 0; k < 4; ++k) c->offset[k] = s->offset[k];
        for (int k = 0; k < 4; ++k) c->anchor[k] = s->anchor[k];
        c->scale = s->scale;
        slot->parent->add_child_before(card, slot);
        slot->parent->remove_child(slot);
        delete slot;
        if (!cards) {
            cards = (Control*)SceneLibrary::create_node("VBoxContainer");
            cards->name = "CardHoverTipContainer";
            cards->add_theme_constant_override("separation", 4);
            cards->mouse_filter = 2;
        }
        cards->add_child(ct);
    }
    for (size_t i = 0; i < tips.size(); ++i) {
        if (tips[i]["card"].is_object()) continue;
        Control* t = make_hover_tip(lib, tips[i]["title"].str(), tips[i]["description"].str());
        if (!t) continue;
        Node* bg = tips[i]["debuff"].b() ? t->find_unique("Bg") : NULL;
        if (bg && bg->is_canvas_item()) {
            /* IsDebuff: the background through materials/ui/hover_tip_debuff
             * (the hsv shader: a red tint). */
            Resource* m = lib->ext("materials/ui/hover_tip_debuff.tres");
            if (m && (m->kind == Resource::RES_SHADER_MATERIAL || m->kind == Resource::RES_CANVAS_ITEM_MATERIAL))
                ((CanvasItem*)bg)->material = (MaterialRes*)m;
        }
        box->add_child(t);
    }
    if (cards) {
        if (box->children.empty()) {
            delete box;
            return cards;
        }
        Control* row = (Control*)SceneLibrary::create_node(split ? "Control" : "HBoxContainer");
        row->name = "HoverTipSetWithCards";
        if (!split) row->add_theme_constant_override("separation", 4);
        row->mouse_filter = 2;
        row->add_child(box);
        row->add_child(cards);
        return row;
    }
    if (box->children.empty()) {
        delete box;
        return NULL;
    }
    return box;
}

void tip_parts(Control* set, Control** text, Control** cards) {
    *text = *cards = NULL;
    if (!set) return;
    if (set->name == "HoverTipSetWithCards" && set->children.size() == 2 && set->children[0]->is_control() &&
        set->children[1]->is_control()) {
        *text = (Control*)set->children[0];
        *cards = (Control*)set->children[1];
    } else if (set->name == "CardHoverTipContainer") {
        *cards = set;
    } else {
        *text = set;
    }
}

void place_tip_parts(Control* set, const Vec2& text_at, const Vec2& cards_at) {
    Control *text, *cards;
    tip_parts(set, &text, &cards);
    if (text && cards && set->type == "Control") {
        set->set_position(text_at);
        text->set_position(Vec2(0, 0));
        cards->set_position(cards_at - text_at);
    } else if (text && cards) {
        set->set_position(text_at); /* a row: the cards follow the text */
    } else if (text) {
        text->set_position(text_at);
    } else if (cards) {
        cards->set_position(cards_at);
    }
}

/* NHoverTipSet.CorrectVerticalOverflow and CorrectHorizontalOverflow: the
 * text tips' and the cards' top-lefts, by their sizes. */
static void correct_overflow(Vec2& t, const Vec2& ts, Vec2& c, const Vec2& cs) {
    if (t.y + ts.y > 1080.0f) t.y = 1080.0f - ts.y;
    if (c.y + cs.y > 1080.0f) c.y = 1080.0f - cs.y;
    if (c.x + cs.x <= 1920.0f && t.x + ts.x > 1920.0f) {
        t = Vec2(c.x - ts.x, c.y);
    } else if (c.x + cs.x > 1920.0f || t.x + ts.x > 1920.0f) {
        c = Vec2(t.x + ts.x - cs.x, c.y);
        t.x -= cs.x;
    } else if (c.x < 0 || t.x < 0) {
        c = Vec2(t.x, c.y);
        t.x += cs.x;
    }
}

/* SetAlignment(node, Left / Right) and its overflow corrections: the text's
 * and the cards' top-lefts */
static void beside(Control* set, const Vec2& at, float width, bool left, float gap, Vec2* t, Vec2* c) {
    Control *text, *cards;
    tip_parts(set, &text, &cards);
    Vec2 ts = text ? text->size : Vec2(), cs = cards ? cards->size : Vec2();
    if (left) {
        *t = Vec2(at.x - ts.x - gap, at.y);
        *c = Vec2(at.x + width, at.y);
    } else {
        *c = Vec2(at.x - cs.x, at.y);
        *t = Vec2(at.x + width + gap, at.y);
    }
    correct_overflow(*t, ts, *c, cs);
}

void place_tips_beside(Control* set, const Vec2& at, float width, bool left, float gap) {
    Vec2 t, c;
    beside(set, at, width, left, gap, &t, &c);
    place_tip_parts(set, t, c);
}

void place_tips_for_relic(Control* set, const Vec2& at, const Vec2& icon) {
    Control *text, *cards;
    tip_parts(set, &text, &cards);
    Vec2 ts = text ? text->size : Vec2(), cs = cards ? cards->size : Vec2();
    bool left = at.x > 1920.0f * 0.75f;
    Vec2 t(at.x, at.y + icon.y + 10.0f);
    if (left) t.x -= ts.x - icon.x;
    /* the cards under the text, from its left (Left: moved there) */
    Vec2 c(t.x, t.y + ts.y);
    if (at.y > 1080.0f * 0.75f) t = Vec2(t.x, at.y - ts.y);
    correct_overflow(t, ts, c, cs);
    bool overlap = c.x < t.x + ts.x && t.x < c.x + cs.x && c.y < t.y + ts.y && t.y < c.y + cs.y;
    if (overlap && cs.x > 0 && ts.x > 0) {
        c = left ? Vec2(t.x + ts.x, t.y) : Vec2(t.x - cs.x, t.y);
        if (t.y + ts.y > 1080.0f) t.y = 1080.0f - ts.y;
        if (c.y + cs.y > 1080.0f) c.y = 1080.0f - cs.y;
    }
    place_tip_parts(set, t, c);
}

/* A tip set's size as a row (the text, then the cards 4 px on) */
static Vec2 row_size(Control* set) {
    Control *text, *cards;
    tip_parts(set, &text, &cards);
    Vec2 ts = text ? text->size : Vec2(), cs = cards ? cards->size : Vec2();
    if (!text || !cards) return text ? ts : cs;
    return Vec2(ts.x + 4.0f + cs.x, maxf(ts.y, cs.y));
}

void TipFollower::attach(SceneLibrary* lib, Node* parent, Control* owner, const Json& tips) {
    hide();
    if (!parent || !owner) return;
    tips_ = make_tip_set(lib, tips, true);
    if (!tips_) return;
    owner_ = owner;
    slot_ = NULL;
    card_ = below_ = relic_ = false;
    side_ = SIDE_BY_POSITION;
    merchant_ = 0;
    extra_ = 0;
    tips_->z_index = 10;
    tips_->modulate.a = 0; /* placed on the next frame, after the layout */
    parent->add_child(tips_);
}

void TipFollower::show_for_card(SceneLibrary* lib, Node* parent, Control* holder, const Json& tips) {
    attach(lib, parent, holder, tips);
    card_ = tips_ != NULL;
}

void TipFollower::show_for_control(SceneLibrary* lib, Node* parent, Control* owner, const Json& tips, float extra_x) {
    attach(lib, parent, owner, tips);
    extra_ = extra_x;
}

void TipFollower::show_for_merchant_slot(SceneLibrary* lib, Node* parent, Control* slot, const Json& tips, bool removal) {
    attach(lib, parent, slot, tips);
    if (tips_) merchant_ = removal ? 2 : 1;
}

void TipFollower::show_for_merchant_card(SceneLibrary* lib, Node* parent, Control* slot, Control* hitbox, const Json& tips) {
    attach(lib, parent, hitbox, tips);
    if (!tips_) return;
    merchant_ = 3;
    slot_ = slot;
}

void TipFollower::show_below(SceneLibrary* lib, Node* parent, Control* owner, const Json& tips) {
    show_for_control(lib, parent, owner, tips);
    below_ = tips_ != NULL;
}

void TipFollower::show_for_relic(SceneLibrary* lib, Node* parent, Control* icon, const Json& tips) {
    attach(lib, parent, icon, tips);
    relic_ = tips_ != NULL;
}

void TipFollower::show_left(SceneLibrary* lib, Node* parent, Control* owner, const Json& tips) {
    show_for_control(lib, parent, owner, tips);
    side_ = tips_ ? SIDE_LEFT_OF : SIDE_BY_POSITION;
}

void TipFollower::show_right(SceneLibrary* lib, Node* parent, Control* owner, const Json& tips) {
    show_for_control(lib, parent, owner, tips);
    side_ = tips_ ? SIDE_RIGHT_OF : SIDE_BY_POSITION;
}

void TipFollower::hide() {
    if (tips_) {
        if (tips_->parent) tips_->parent->remove_child(tips_);
        delete tips_;
    }
    tips_ = NULL;
    owner_ = NULL;
    slot_ = NULL;
}

void TipFollower::frame() {
    if (!tips_ || !owner_) return;
    Xform2D xf = owner_->global_transform();
    float s = sqrtf(xf.x.x * xf.x.x + xf.x.y * xf.x.y);
    Vec2 c = xf.origin;
    Vec2 size = row_size(tips_);
    float w = size.x, h = size.y;
    /* not laid out yet (made this frame): placed with no size it showed for
     * a frame beside where it belongs */
    if (w <= 0 || h <= 0) {
        tips_->modulate.a = 0;
        return;
    }
    if (relic_) {
        /* SetAlignmentForRelic (the owner: the relic's icon) */
        place_tips_for_relic(tips_, c, owner_->size * s);
        tips_->modulate.a = 1;
        return;
    }
    if (merchant_ == 3) {
        /* SetAlignment(hitbox, the slot's GetHoverTipAlignment) */
        bool left = slot_ && slot_->global_transform().origin.x > 1920.0f * 0.75f;
        place_tips_beside(tips_, c, owner_->size.x * owner_->scale.x, left, 0);
        tips_->modulate.a = 1;
        return;
    }
    /* SetAlignment(Left): the text left of the owner, the cards right of
     * it; Right: the text right, the cards left (NHoverTipSet.SetAlignment,
     * then its overflow corrections). These were rows of the text and the
     * cards on one side: the cards went with the text (Neow's Dowsing Rod:
     * Dowsing and Abundance left of the option, the game's right of it). */
    if (merchant_) {
        /* the slot's Size * 0.5 * Scale (its own scale: small or hovered) */
        float hx = owner_->size.x * owner_->scale.x * 0.5f, hy = owner_->size.y * owner_->scale.y * 0.5f;
        float slot_w = owner_->size.x * owner_->scale.x;
        Vec2 t, k;
        if (c.x > 1920.0f * 0.5f) {
            beside(tips_, c, slot_w, true, 0, &t, &k); /* SetAlignment(Left) */
            t = t - Vec2(hx, hy);
            k = k - Vec2(hx, hy);
        } else if (merchant_ == 1) {
            beside(tips_, c, slot_w, false, 0, &t, &k); /* SetAlignment(Right) */
            t = t + Vec2(hx, -hy);
            k = k + Vec2(hx, -hy);
        } else {
            /* the removal: no alignment, at the slot (its tip is text) */
            t = k = c + Vec2(hx, -hy);
        }
        place_tip_parts(tips_, t, k);
        tips_->modulate.a = 1;
        return;
    }
    if (below_) {
        /* HoverTipAlignment.Center (NPotionHolder): the set 1.5 of the
         * owner's height under it, the cards under the text; then
         * CorrectVerticalOverflow / CorrectHorizontalOverflow */
        Control *text, *cards;
        tip_parts(tips_, &text, &cards);
        Vec2 ts = text ? text->size : Vec2(), cs = cards ? cards->size : Vec2();
        Vec2 t(c.x, c.y + owner_->size.y * s * 1.5f), k(t.x, t.y + ts.y);
        correct_overflow(t, ts, k, cs);
        place_tip_parts(tips_, t, k);
        tips_->modulate.a = 1;
        return;
    }
    if (!card_) {
        /* GetHoverTipAlignment (left past 3/4 of the screen) unless the
         * owner gives the side; extra_: SetExtraFollowOffset */
        bool left = side_ == SIDE_BY_POSITION ? c.x > 1920.0f * 0.75f : side_ == SIDE_LEFT_OF;
        place_tips_beside(tips_, Vec2(c.x + extra_, c.y), owner_->size.x * s, left, 0);
        tips_->modulate.a = 1;
        return;
    }
    /* SetAlignmentForCardHolder: the card's hitbox, 300 x 422 about the
     * holder's origin */
    place_tips_beside(tips_, Vec2(c.x - 150.0f * s, c.y - 211.0f * s), 300.0f * s, c.x > 1920.0f * 0.75f, 10.0f);
    tips_->modulate.a = 1;
}

Node* find_named(Node* n, const char* name) {
    if (!n) return NULL;
    if (n->name == name) return n;
    for (size_t i = 0; i < n->children.size(); ++i) {
        Node* r = find_named(n->children[i], name);
        if (r) return r;
    }
    return NULL;
}

Control* ctl(Node* scope, const char* unique) {
    Node* n = scope ? scope->find_unique(unique) : NULL;
    return n && n->is_control() ? (Control*)n : NULL;
}

void set_label(Node* n, const std::string& text) {
    if (n && n->type == "Label") ((Label*)n)->set_text(text);
    else if (n && n->type == "RichTextLabel") ((RichTextLabel*)n)->text = text;
}

Xform2D inverse(const Xform2D& t) { return t.affine_inverse(); }

/* MathHelper.SmoothDamp */
float smooth_damp(float current, float target, float* velocity, float smooth_time, float dt) {
    if (dt <= 0) return current;
    smooth_time = smooth_time > 0.0001f ? smooth_time : 0.0001f;
    float omega = 2.0f / smooth_time, x = omega * dt;
    float e = 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x);
    float change = current - target, original = target;
    target = current - change;
    float temp = (*velocity + omega * change) * dt;
    *velocity = (*velocity - omega * temp) * e;
    float out = target + (change + temp) * e;
    if ((original - current > 0.0f) == (out > original)) {
        out = original;
        *velocity = (out - original) / dt;
    }
    return out;
}

unsigned chaotic_rand() {
    static unsigned state = 0;
    if (!state) {
#if defined(__XSDK__)
        /* xsdk has no clock(): its monotonic clock's nanoseconds */
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        unsigned tick = (unsigned)ts.tv_nsec;
#else
        unsigned tick = (unsigned)clock();
#endif
        state = ((unsigned)time(NULL) * 2654435761u) ^ tick ^ 0x9E3779B9u;
    }
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

void warn_missing(const std::string& path) {
    static std::set<std::string> told;
    if (told.insert(path).second) log_warn("missing in the pack: %s", path.c_str());
}

void set_texture(SceneLibrary* lib, Node* rect, const std::string& path) {
    if (!rect) return;
    Resource* r = path.empty() ? NULL : lib->ext(path);
    Texture2DRes* t = r && r->is_texture() ? (Texture2DRes*)r : NULL;
    if (!t && !path.empty()) warn_missing(path);
    if (rect->type == "TextureRect") ((TextureRect*)rect)->texture = t;
    else if (rect->type == "NinePatchRect") ((NinePatchRect*)rect)->texture = t;
}

void set_visible(Node* n, bool visible) {
    if (n && n->is_canvas_item()) ((CanvasItem*)n)->visible = visible;
}

void show_glyph(SceneLibrary* lib, Node* hotkey_icon, const char* glyph) {
    if (!hotkey_icon) return;
    Node* pad = hotkey_icon->find_unique("ControllerIcon");
    Node* key = hotkey_icon->find_unique("KeyboardIcon");
    if (key && key->is_canvas_item()) ((CanvasItem*)key)->visible = false;
    if (!pad || pad->type != "TextureRect") return;
    Resource* r = lib->ext(std::string("images/atlases/controller_atlas.sprites/xbox_360/") + glyph + ".tres");
    ((TextureRect*)pad)->texture = r && r->is_texture() ? (Texture2DRes*)r : NULL;
    ((CanvasItem*)pad)->visible = r != NULL;
}

void place(Control* c, float x, float y, float w, float h) {
    c->offset[SIDE_LEFT] = x;
    c->offset[SIDE_TOP] = y;
    c->offset[SIDE_RIGHT] = x + w;
    c->offset[SIDE_BOTTOM] = y + h;
}

Vec2 offset_pos(const Control* c) { return Vec2(c->offset[SIDE_LEFT], c->offset[SIDE_TOP]); }

Vec2 offset_size(const Control* c) {
    return Vec2(c->offset[SIDE_RIGHT] - c->offset[SIDE_LEFT], c->offset[SIDE_BOTTOM] - c->offset[SIDE_TOP]);
}

Color hex(unsigned v) {
    return Color(((v >> 16) & 255) / 255.0f, ((v >> 8) & 255) / 255.0f, (v & 255) / 255.0f, 1.0f);
}

void setup_inspect_prompt(SceneLibrary* lib, Node* prompt) {
    if (!prompt) return;
    show_glyph(lib, prompt->find_unique("HotkeyIcon"), "y");
    Node* label = prompt->find_unique("Label");
    if (!label) label = find_named(prompt, "Label");
    set_label(label, Loc::get("gameplay_ui", "TO_INSPECT_PROMPT"));
    set_visible(prompt, true);
}

Control* full_rect_control(const char* name) {
    Control* c = (Control*)SceneLibrary::create_node("Control");
    c->name = name;
    c->anchor[SIDE_LEFT] = c->anchor[SIDE_TOP] = 0;
    c->anchor[SIDE_RIGHT] = c->anchor[SIDE_BOTTOM] = 1;
    for (int i = 0; i < 4; ++i) c->offset[i] = 0;
    c->mouse_filter = 2;
    return c;
}

Node* overlay_behind(Node* background) {
    CanvasLayer* layer = (CanvasLayer*)SceneLibrary::create_node("CanvasLayer");
    layer->name = "OverlayBackground";
    layer->layer = -1;
    layer->add_child(background);
    return layer;
}

namespace {
class FrameDriver : public ScriptBehavior {
public:
    explicit FrameDriver(FrameListener* l) : l_(l) {}
    virtual void process(Node*, double dt) { l_->frame(dt); }

private:
    FrameListener* l_;
};
} /* namespace */

Node* frame_driver(FrameListener* listener) {
    Node* n = SceneLibrary::create_node("Node");
    n->name = "FrameDriver";
    n->behavior = new FrameDriver(listener);
    return n;
}

} /* namespace spire */
