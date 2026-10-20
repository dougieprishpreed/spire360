#include "crystal_sphere_screen.h"

#include "../scene/controls.h"
#include "../scene/scene.h"
#include "loc.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

namespace spire {

static Control* control_of(Node* n) { return n && n->is_control() ? (Control*)n : NULL; }

static const float CELL = 57.0f;

/* The focused cell between screens (each decision builds a new one). */
static int g_focus_x = -1, g_focus_y = -1;

CrystalSphereScreen::CrystalSphereScreen(SceneLibrary* lib)
    : lib_(lib), screen_(NULL), mask_(NULL), dialogue_(NULL), w_(0), h_(0), fx_(0), fy_(0), big_(false), t_(0), say_t_(-1) {}

bool CrystalSphereScreen::hidden(int x, int y) const {
    if (x < 0 || y < 0 || x >= w_ || y >= h_) return false;
    const Json& cs = decision_["cells"];
    for (size_t i = 0; i < cs.size(); ++i)
        if (cs[i]["x"].i() == x && cs[i]["y"].i() == y) return cs[i]["hidden"].b();
    return false;
}

static bool cell_hidden(const Json& d, int x, int y) {
    const Json& cs = d["cells"];
    for (size_t i = 0; i < cs.size(); ++i)
        if (cs[i]["x"].i() == x && cs[i]["y"].i() == y) return cs[i]["hidden"].b();
    return true;
}

static void set_material(SceneLibrary* lib, Node* n, const Json& path) {
    if (!n || !n->is_canvas_item() || !path.is_string()) return;
    Resource* r = lib->ext(path.str());
    ((CanvasItem*)n)->material = r && (r->kind == Resource::RES_SHADER_MATERIAL || r->kind == Resource::RES_CANVAS_ITEM_MATERIAL)
                                     ? (MaterialRes*)r
                                     : NULL;
}

Node* CrystalSphereScreen::build(const Json& d, std::string* error) {
    decision_ = d;
    Node* screen = instance(lib_, "scenes/events/custom/crystal_sphere/crystal_sphere_screen.tscn");
    if (!screen || !screen->is_control()) {
        delete screen;
        if (error) *error = "no crystal_sphere_screen.tscn";
        return NULL;
    }
    screen_ = screen;
    bool again = previous_["decision"].str() == "crystal_sphere";
    w_ = d["width"].i(11);
    h_ = d["height"].i(11);
    big_ = d["tool"].str() == "Big";
    hide(screen->find_unique("ProceedButton"));
    set_label(screen->find_unique("InstructionsTitle"), Loc::get("events", "CRYSTAL_SPHERE.minigame.instructions.title"));
    set_label(screen->find_unique("InstructionsDescription"), Loc::get("events", "CRYSTAL_SPHERE.minigame.instructions.description"));
    set_label(screen->find_unique("DivinationsLeft"),
              Loc::format("events", "CRYSTAL_SPHERE.minigame.divinationsRemain", "Count", format("%d", d["divinations"].i())));
    /* the divination buttons: labels, LT / RT, the tool's outline */
    static const char* const buttons[2][4] = {
        { "SmallDivinationButton", "CRYSTAL_SPHERE.button.DIVINATION_LABEL_SMALL", "lt", "Small" },
        { "BigDivinationButton", "CRYSTAL_SPHERE.button.DIVINATION_LABEL_BIG", "rt", "Big" } };
    for (int k = 0; k < 2; ++k) {
        Node* b = screen->find_unique(buttons[k][0]);
        if (!b) continue;
        set_label(b->find_unique("Label"), Loc::get("events", buttons[k][1]));
        show_glyph(lib_, b->find_unique("HotkeyIcon"), buttons[k][2]);
        set_visible(b->find_unique("Outline"), d["tool"].str() == buttons[k][3]);
    }

    float vx = -(CELL * (float)w_) * 0.5f;
    /* the cells (NCrystalSphereCell), x by column */
    Node* cells = screen->find_unique("Cells");
    cells_.assign((size_t)(w_ * h_), (Control*)NULL);
    for (int x = 0; x < w_; ++x)
        for (int y = 0; y < h_; ++y) {
            Control* c = control_of(instance(lib_, "scenes/events/custom/crystal_sphere/crystal_sphere_cell.tscn"));
            if (!c || !cells) {
                delete c;
                continue;
            }
            place(c, vx + CELL * (float)x, vx + CELL * (float)y, CELL, CELL);
            c->modulate = Color(1, 1, 1, 0);
            hide(c->get_node("HoveredFg"));
            cells->add_child(c);
            cells_[(size_t)(x * h_ + y)] = c;
        }

    /* the items (NCrystalSphereItem) under the fog */
    Node* items = screen->find_unique("Items");
    const Json& iv = d["item_views"];
    for (size_t i = 0; items && i < iv.size(); ++i) {
        const Json& it = iv[i];
        Control* n = control_of(instance(lib_, "scenes/events/custom/crystal_sphere/crystal_sphere_item.tscn"));
        if (!n) continue;
        float iw = CELL * (float)it["w"].i(1), ih = CELL * (float)it["h"].i(1);
        place(n, vx + CELL * (float)it["x"].i(), vx + CELL * (float)it["y"].i(), iw, ih);
        n->pivot_offset = Vec2(iw, ih) * 0.5f;
        Control* icon = control_of(n->get_node("Icon"));
        Node* card = n->find_unique("Card");
        bool is_card = it["banner_material"].is_string();
        set_visible(card, is_card);
        set_visible(icon, !is_card);
        if (is_card) {
            set_material(lib_, n->find_unique("CardBanner"), it["banner_material"]);
            set_material(lib_, n->find_unique("CardFrame"), it["frame_material"]);
        } else if (icon && it["texture"].is_string()) {
            set_texture(lib_, icon, it["texture"].str());
        }
        if (icon) icon->pivot_offset = Vec2(iw, ih) * 0.5f;
        /* OnRevealed: the shine (val 0 to 1 after a quarter second), the
         * icon popping; one revealed before stays shone */
        bool now = it["revealed"].b();
        bool before = false;
        const Json& pv = previous_["item_views"];
        for (size_t k = 0; k < pv.size(); ++k)
            if (pv[k]["x"].i() == it["x"].i() && pv[k]["y"].i() == it["y"].i()) before = pv[k]["revealed"].b();
        static const char* const shiny[3] = { "Icon", "CardShadow", "CardBanner" };
        for (int k = 0; k < 3; ++k) {
            Node* s = find_named(n, shiny[k]);
            if (!s || !s->is_canvas_item()) continue;
            float& val = ((CanvasItem*)s)->shader_params["val"];
            val = now && before ? 1.0f : 0.0f;
            if (now && !before) tweens_.tween_from(&val, 0.0f, 1.0f, 0.5f, TRANS_LINEAR, EASE_IN_OUT, 0.25f);
        }
        if (now && !before && icon) pops_.push_back(icon);
        if (now && !before) say(it["good"].b() ? "REVEAL_GOOD" : "REVEAL_BAD");
        items->add_child(n);
    }

    /* the fog: NCrystalSphereMask.UpdateMat, each cleared cell from its
     * alpha to 0 over half a second */
    mask_ = control_of(screen->find_unique("ScryMask"));
    fade_from_.assign(121, 1.0f);
    fade_start_.assign(121, -1.0f);
    for (int x = 0; x < w_ && x < 11; ++x)
        for (int y = 0; y < h_ && y < 11; ++y) {
            if (hidden(x, y)) continue;
            int i = y * 11 + x;
            if (again && cell_hidden(previous_, x, y)) fade_start_[(size_t)i] = 0;
            else fade_from_[(size_t)i] = 0;
        }

    /* AfterOverlayOpened: the screen fades in, then the seer and the items */
    if (!again) {
        ((Control*)screen)->modulate.a = 0;
        tweens_.tween(&((Control*)screen)->modulate.a, 1.0f, 0.5f);
        hide(items);
        g_focus_x = g_focus_y = -1;
    }
    dialogue_ = screen->find_unique("Dialogue");
    if (dialogue_) {
        Node* bubble = dialogue_->find_unique("Bubble");
        if (bubble && bubble->is_canvas_item()) {
            CanvasItem* b = (CanvasItem*)bubble;
            b->shader_params["h"] = 0.25f;
            b->shader_params["s"] = 0.75f;
            b->shader_params["v"] = 1.0f;
        }
    }

    /* the focus: where it was (the nearest hidden cell to it), at first the
     * middle hidden cell (DefaultFocusedControl) */
    int bx = -1, by = -1;
    if (g_focus_x >= 0) {
        float best = 1e30f;
        for (int x = 0; x < w_; ++x)
            for (int y = 0; y < h_; ++y) {
                if (!hidden(x, y)) continue;
                float dd = (float)((x - g_focus_x) * (x - g_focus_x) + (y - g_focus_y) * (y - g_focus_y));
                if (dd < best) {
                    best = dd;
                    bx = x;
                    by = y;
                }
            }
    } else {
        std::vector<int> list;
        for (int x = 0; x < w_; ++x)
            for (int y = 0; y < h_; ++y)
                if (hidden(x, y)) list.push_back(x * h_ + y);
        if (!list.empty()) {
            bx = list[list.size() / 2] / h_;
            by = list[list.size() / 2] % h_;
        }
    }
    if (bx >= 0) focus(bx, by);
    screen->add_child(frame_driver(this));
    (void)error;
    return screen;
}

/* NCrystalSphereDialogue.Play: a random line of the kind */
void CrystalSphereScreen::say(const char* kind) {
    int n = !strcmp(kind, "START") ? 2 : !strcmp(kind, "REVEAL_BAD") ? 3 : !strcmp(kind, "REVEAL_GOOD") ? 5 : 2;
    std::string key = format("CRYSTAL_SPHERE.banter.%s.%d", kind, (int)(chaotic_rand() % (unsigned)n) + 1);
    if (!dialogue_) return;
    set_label(dialogue_->find_unique("Text"), "[fly_in]" + Loc::get("events", key.c_str()) + "[/fly_in]");
    if (dialogue_->is_node2d()) {
        Node2D* d = (Node2D*)dialogue_;
        d->position.x = 1100.0f + (float)(chaotic_rand() % 1001) / 10.0f;
        tweens_.tween(&d->scale, Vec2(1, 1), 0.25f, TRANS_BACK, EASE_OUT);
    }
    Node* bubble = dialogue_->find_unique("Bubble");
    if (bubble && bubble->is_node2d())
        tweens_.tween_from(&((Node2D*)bubble)->scale, Vec2(0.25f, 0.25f), Vec2(0.75f, 0.75f), 0.5f, TRANS_EXPO, EASE_OUT);
    Node* box = dialogue_->find_unique("DialogueBox");
    if (box && box->is_node2d()) tweens_.tween_from(&((Node2D*)box)->position.y, -80.0f, 0.0f, 0.5f, TRANS_BACK, EASE_OUT);
    say_t_ = t_;
}

void CrystalSphereScreen::focus(int x, int y) {
    fx_ = x;
    fy_ = y;
    g_focus_x = x;
    g_focus_y = y;
    refresh_highlight();
}

/* SetHoveredCell / OnEntityHighlightUpdated: the tool's cells lit (hidden
 * ones), the hovered one's foreground */
void CrystalSphereScreen::refresh_highlight() {
    bool done = decision_["divinations"].i() <= 0;
    for (int x = 0; x < w_; ++x)
        for (int y = 0; y < h_; ++y) {
            Control* c = cells_[(size_t)(x * h_ + y)];
            if (!c) continue;
            bool lit = !done && (big_ ? abs(x - fx_) <= 1 && abs(y - fy_) <= 1 : x == fx_ && y == fy_);
            tweens_.tween(&c->modulate.a, lit && hidden(x, y) ? 1.0f : 0.0f, 0.15f);
            set_visible(c->get_node("HoveredFg"), lit && x == fx_ && y == fy_);
        }
}

std::string CrystalSphereScreen::input(const PadState& pad) {
    if (decision_["divinations"].i() <= 0) return std::string();
    /* SetSmallDivination / SetBigDivination (LT / RT) */
    if ((pad.pressed & PadState::LT) && decision_["tool"].str() != "Small")
        return protocol_action("crystal_sphere_tool", "\"tool\":\"Small\"");
    if ((pad.pressed & PadState::RT) && decision_["tool"].str() != "Big")
        return protocol_action("crystal_sphere_tool", "\"tool\":\"Big\"");
    int dx = (pad.pressed & PadState::LEFT) ? -1 : (pad.pressed & PadState::RIGHT) ? 1 : 0;
    int dy = (pad.pressed & PadState::UP) ? -1 : (pad.pressed & PadState::DOWN) ? 1 : 0;
    if (dx || dy) {
        /* the neighbour that way, past cleared cells (not focusable) */
        for (int x = fx_ + dx, y = fy_ + dy; x >= 0 && y >= 0 && x < w_ && y < h_; x += dx, y += dy)
            if (hidden(x, y)) {
                focus(x, y);
                break;
            }
        return std::string();
    }
    if ((pad.pressed & PadState::A) && hidden(fx_, fy_))
        return protocol_action("crystal_sphere_click", format("\"x\":%d,\"y\":%d", fx_, fy_));
    return std::string();
}

void CrystalSphereScreen::frame(double dt) {
    tweens_.update(dt);
    t_ += dt;
    /* the opening: the seer starts and the items show after the fade */
    if (previous_["decision"].str() != "crystal_sphere" && t_ >= 0.5 && t_ - dt < 0.5) {
        set_visible(screen_ ? screen_->find_unique("Items") : NULL, true);
        say("START");
    }
    /* the line: in over 0.25 s, held, out over 0.5 s after 2 s */
    if (dialogue_ && dialogue_->is_canvas_item()) {
        double s = say_t_ >= 0 ? t_ - say_t_ : 1e9;
        float a = s < 0.25 ? (float)(s / 0.25) : s < 2.0 ? 1.0f : s < 2.5 ? (float)sin((1.0 - (s - 2.0) / 0.5) * 1.5707963) : 0.0f;
        ((CanvasItem*)dialogue_)->modulate.a = a;
    }
    /* OnRevealed's pop: after a quarter second, to 1.2 in 0.15 s, back in 0.5 s */
    for (size_t k = 0; k < pops_.size(); ++k) {
        double s = t_ - 0.25;
        float sc = s < 0 ? 1.0f : s < 0.15 ? 1.0f + 0.2f * (float)(s / 0.15) : s < 0.65 ? 1.2f - 0.2f * (float)((s - 0.15) / 0.5) : 1.0f;
        pops_[k]->scale = Vec2(sc, sc);
    }
    /* the fog's cells */
    if (mask_) {
        for (int i = 0; i < 121; ++i) {
            float a = fade_from_[(size_t)i];
            if (fade_start_[(size_t)i] >= 0) {
                float k = (float)((t_ - fade_start_[(size_t)i]) / 0.5);
                a = 1.0f - (k < 0 ? 0 : k > 1 ? 1 : k);
            }
            mask_->shader_params[format("cell_%d", i)] = a;
        }
    }
}

} /* namespace spire */
