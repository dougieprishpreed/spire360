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

Control* make_tip_set(SceneLibrary* lib, const Json& tips) {
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
        Control* row = (Control*)SceneLibrary::create_node("HBoxContainer");
        row->name = "HoverTipSetWithCards";
        row->add_theme_constant_override("separation", 4);
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

void TipFollower::attach(SceneLibrary* lib, Node* parent, Control* owner, const Json& tips) {
    hide();
    if (!parent || !owner) return;
    tips_ = make_tip_set(lib, tips);
    if (!tips_) return;
    owner_ = owner;
    slot_ = NULL;
    card_ = below_ = left_ = false;
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

void TipFollower::show_left(SceneLibrary* lib, Node* parent, Control* owner, const Json& tips) {
    show_for_control(lib, parent, owner, tips);
    left_ = tips_ != NULL;
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
    float w = tips_->size.x, h = tips_->size.y;
    /* not laid out yet (made this frame): placed with no size it showed for
     * a frame beside where it belongs */
    if (w <= 0 || h <= 0) {
        tips_->modulate.a = 0;
        return;
    }
    if (merchant_ == 3) {
        /* SetAlignment(hitbox, Right / Left), CorrectVerticalOverflow; past
         * the right edge, CorrectHorizontalOverflow puts them left of it */
        bool left = slot_ && slot_->global_transform().origin.x > 1920.0f * 0.75f;
        float x = left ? c.x - w : c.x + owner_->size.x * owner_->scale.x;
        if (!left && x + w > 1920.0f) x = c.x - w;
        float y = c.y;
        if (y + h > 1080.0f) y = 1080.0f - h;
        tips_->set_position(Vec2(x, y));
        tips_->modulate.a = 1;
        return;
    }
    if (merchant_) {
        /* the slot's Size * 0.5 * Scale (its own scale: small or hovered) */
        float hx = owner_->size.x * owner_->scale.x * 0.5f, hy = owner_->size.y * owner_->scale.y * 0.5f;
        float x, y = c.y;
        if (c.x > 1920.0f * 0.5f) {
            x = c.x - w; /* SetAlignment(Left), then its overflow check */
            if (y + h > 1080.0f) y = 1080.0f - h;
            x -= hx;
        } else if (merchant_ == 1) {
            x = c.x + hx * 2.0f; /* SetAlignment(Right) */
            if (y + h > 1080.0f) y = 1080.0f - h;
            x += hx;
        } else {
            x = c.x + hx; /* the removal: no alignment, at the slot */
        }
        tips_->set_position(Vec2(x, y - hy));
        tips_->modulate.a = 1;
        return;
    }
    if (left_) {
        float x = c.x - w, y = c.y;
        if (x < 0) x = 0;
        if (y + h > 1080.0f) y = 1080.0f - h;
        tips_->set_position(Vec2(x, y));
        tips_->modulate.a = 1;
        return;
    }
    if (below_) {
        /* then CorrectHorizontalOverflow / CorrectVerticalOverflow */
        float x = c.x, y = c.y + owner_->size.y * s * 1.5f;
        if (x + w > 1920.0f) x = 1920.0f - w;
        if (y + h > 1080.0f) y = 1080.0f - h;
        tips_->set_position(Vec2(x, y));
        tips_->modulate.a = 1;
        return;
    }
    if (!card_) {
        /* SetAlignment, then CorrectVerticalOverflow */
        float x = c.x > 1920.0f * 0.75f ? c.x - w : c.x + owner_->size.x * s;
        float y = c.y;
        if (y + h > 1080.0f) y = 1080.0f - h;
        if (y < 0) y = 0;
        tips_->set_position(Vec2(x + extra_, y));
        tips_->modulate.a = 1;
        return;
    }
    /* The card: 300 x 422 about the holder's origin. */
    float x = c.x + 150.0f * s + 10.0f;
    if (c.x > 1920.0f * 0.75f || x + w > 1920.0f) x = c.x - 150.0f * s - 10.0f - w;
    float y = c.y - 211.0f * s;
    if (y + h > 1080.0f) y = 1080.0f - h;
    if (y < 0) y = 0;
    tips_->set_position(Vec2(x, y));
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
    if (!state) state = ((unsigned)time(NULL) * 2654435761u) ^ (unsigned)clock() ^ 0x9E3779B9u;
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
