#include "card_select_screen.h"
#include "card_view.h"
#include "loc.h"

#include "../scene/controls.h"
#include "../scene/node.h"
#include "../scene/scene.h"

#include <algorithm>
#include <math.h>

namespace spire {

/* NCard.defaultSize * NCardHolder.smallScale; NCardGrid.CardPadding and
 * its margins; NGridCardHolder's scales. */
static const float CARD_W = 300.0f * 0.8f, CARD_H = 422.0f * 0.8f;
static const float PADDING = 40.0f, TOP_MARGIN = 80.0f, BOTTOM_MARGIN = 320.0f;
static const float SMALL_SCALE = 0.8f, HOVER_SCALE = 1.0f;
/* card_grid.tscn: ScrollContainer offsets; the grid sits under the top bar. */
static const float SCROLL_LEFT = 150.0f, SCROLL_RIGHT = 200.0f, GRID_TOP = 80.0f;
/* NChooseACardSelectionScreen: 340 px between cards. */
static const float ROW_STEP = 340.0f;

static const char* scene_for(const std::string& kind) {
    if (kind == "upgrade") return "scenes/screens/card_selection/deck_upgrade_select_screen.tscn";
    if (kind == "transform") return "scenes/screens/card_selection/deck_transform_select_screen.tscn";
    if (kind == "enchant") return "scenes/screens/card_selection/deck_enchant_select_screen.tscn";
    if (kind == "combat_pile") return "scenes/screens/card_selection/combat_pile_card_select_screen.tscn";
    if (kind == "simple_grid") return "scenes/screens/card_selection/simple_card_select_screen.tscn";
    return "scenes/screens/card_selection/deck_card_select_screen.tscn";
}

static Control* control(Node* n) { return n && n->is_control() ? (Control*)n : NULL; }

CardSelectScreen::CardSelectScreen(SceneLibrary* lib)
    : lib_(lib), background_(NULL), row_(false), min_(0), max_(1), cancelable_(false), focus_(-1), columns_(1),
      rows_(NULL), scroll_(0), scroll_to_(0), lim_lo_(0), lim_hi_(0), grid_h_(1000), preview_(false), preview_box_(NULL),
      single_box_(NULL), multi_box_(NULL) {
    Button none = { NULL, 0, 0, 0, false };
    close_ = confirm_ = preview_cancel_ = preview_confirm_ = none;
}

/* NConfirmButton / NBackButton: shown at the window's corner less an
 * offset from the scene's own offsets, hidden 180 px further out. */
CardSelectScreen::Button CardSelectScreen::button(Node* n, bool is_confirm) {
    Button b = { control(n), 0, 0, 0, false };
    if (!b.node) return b;
    Control* c = b.node;
    float w = c->offset[SIDE_RIGHT] - c->offset[SIDE_LEFT], h = c->offset[SIDE_BOTTOM] - c->offset[SIDE_TOP];
    if (is_confirm) {
        b.x = 1920.0f - (c->offset[SIDE_RIGHT] + 120.0f);
        b.hide_dx = 180.0f;
    } else {
        b.x = -(c->offset[SIDE_LEFT] + 80.0f);
        b.hide_dx = -180.0f;
    }
    b.y = 1080.0f - (-c->offset[SIDE_BOTTOM] + 110.0f);
    for (int s = 0; s < 4; ++s) c->anchor[s] = 0;
    place(c, b.x + b.hide_dx, b.y, w, h);
    show_glyph(lib_, find_named(c, "HotkeyIcon"), is_confirm ? "y" : "b");
    return b;
}

void CardSelectScreen::show_button(Button& b, bool shown, bool at_once) {
    if (!b.node) return;
    b.shown = shown;
    float w = b.node->offset[SIDE_RIGHT] - b.node->offset[SIDE_LEFT];
    float x = shown ? b.x : b.x + b.hide_dx;
    if (at_once) {
        tweens_.kill(&b.node->offset[SIDE_LEFT]);
        tweens_.kill(&b.node->offset[SIDE_RIGHT]);
        b.node->offset[SIDE_LEFT] = x;
        b.node->offset[SIDE_RIGHT] = x + w;
        return;
    }
    TweenTrans t = shown ? TRANS_BACK : TRANS_EXPO;
    tweens_.tween(&b.node->offset[SIDE_LEFT], x, 0.35f, t, EASE_OUT);
    tweens_.tween(&b.node->offset[SIDE_RIGHT], x + w, 0.35f, t, EASE_OUT);
}

Node* CardSelectScreen::build(const Json& d, std::string* error) {
    decision_ = d;
    kind_ = d["kind"].is_string() ? d["kind"].str() : std::string("deck");
    min_ = d["min_select"].i(1);
    max_ = d["max_select"].i(1);
    cancelable_ = d["cancelable"].b();
    holders_.clear();
    cards_.clear();
    picked_.assign(d["cards"].size(), false);
    order_.clear();
    preview_ = false;
    focus_ = -1;
    tweens_.clear();

    Control* root = full_rect_control("CardSelectScreen");
    if (background_) {
        root->add_child(background_);
        background_ = NULL;
    }
    /* The overlays' shared backstop (its colour is in run.tscn, not in the
     * bundles). */
    ColorRect* back = (ColorRect*)SceneLibrary::create_node("ColorRect");
    back->name = "OverlayBackstop";
    back->anchor[SIDE_RIGHT] = back->anchor[SIDE_BOTTOM] = 1;
    back->color = Color(0, 0, 0, 0.7f);
    root->add_child(back);

    row_ = kind_ == "choose_a_card";
    Node* screen = row_ ? build_row(d) : NULL;
    if (!row_) {
        screen = instance(lib_, scene_for(kind_));
        if (!screen) {
            if (error) *error = std::string("no ") + scene_for(kind_);
            delete root;
            return NULL;
        }
        hide(screen->find_unique("PeekButton"));
        hide(find_named(screen, "ViewUpgrades"));
        hide(screen->find_unique("CombatPiles"));
        hide(find_named(screen, "CombatPiles"));
        hide(find_named(screen, "EnchantmentDescriptionContainer"));
        set_label(screen->find_unique("BottomLabel"), d["prompt"].is_string() ? d["prompt"].str() : std::string());

        /* The preview containers (closed). */
        const char* single = "PreviewContainer";
        const char* multi = "PreviewContainer";
        if (kind_ == "upgrade") {
            single = "UpgradeSinglePreviewContainer";
            multi = "UpgradeMultiPreviewContainer";
        } else if (kind_ == "enchant") {
            single = multi = "EnchantMultiPreviewContainer";
        } else if (kind_ == "combat_pile" || kind_ == "simple_grid") {
            single = multi = "";
        }
        hide(find_named(screen, "UpgradeSinglePreviewContainer"));
        hide(find_named(screen, "UpgradeMultiPreviewContainer"));
        hide(find_named(screen, "EnchantSinglePreviewContainer"));
        hide(find_named(screen, "EnchantMultiPreviewContainer"));
        hide(find_named(screen, "PreviewContainer"));
        single_box_ = *single ? control(find_named(screen, single)) : NULL;
        multi_box_ = *multi ? control(find_named(screen, multi)) : NULL;

        /* The grid: columns as fit, centred, scrolled to the focus. */
        Node* grid = screen->find_unique("CardGrid");
        Node* scroll = grid ? grid->find_unique("ScrollContainer") : NULL;
        if (!scroll) scroll = find_named(grid, "ScrollContainer");
        hide(grid ? grid->get_node("Scrollbar") : NULL);
        rows_ = full_rect_control("Rows");
        if (scroll) scroll->add_child(rows_);
        else root->add_child(rows_);
        float width = 1920.0f - SCROLL_LEFT - SCROLL_RIGHT;
        columns_ = (int)((width + PADDING) / (CARD_W + PADDING));
        if (columns_ < 1) columns_ = 1;
        const Json& cs = d["cards"];
        int n = (int)cs.size();
        int rows = (n + columns_ - 1) / columns_;
        int cols_used = n < columns_ ? n : columns_;
        float content_w = (float)columns_ * CARD_W + (float)(columns_ - 1) * PADDING;
        float content_h = (float)rows * CARD_H + (float)(rows > 0 ? rows - 1 : 0) * PADDING;
        float scroll_h = content_h + TOP_MARGIN + BOTTOM_MARGIN;
        grid_h_ = 1080.0f - GRID_TOP;
        float lim_top = grid_h_ > scroll_h ? (grid_h_ - scroll_h) * 0.5f : 0.0f;
        float lim_bottom = grid_h_ > scroll_h ? (grid_h_ - scroll_h) * 0.5f : grid_h_ - scroll_h;
        lim_lo_ = std::min(std::min(lim_top, lim_bottom), 0.0f);
        lim_hi_ = std::max(std::min(lim_top, lim_bottom), 0.0f);
        if (lim_top > 0) lim_lo_ = lim_hi_ = lim_top;
        scroll_ = scroll_to_ = lim_top;
        (void)cols_used;
        for (int i = 0; i < n; ++i) {
            Node* h = instance(lib_, "scenes/cards/holders/grid_card_holder.tscn");
            if (!h || !h->is_control()) {
                delete h;
                h = SceneLibrary::create_node("Control");
            }
            Control* hc = (Control*)h;
            Node* card = make_card(lib_, cs[(size_t)i]);
            if (card) hc->add_child(card);
            int r = i / columns_, c = i % columns_;
            place(hc, (width - content_w) * 0.5f + CARD_W * 0.5f + (float)c * (CARD_W + PADDING),
                  TOP_MARGIN + CARD_H * 0.5f + (float)r * (CARD_H + PADDING), 0, 0);
            hc->scale = Vec2(SMALL_SCALE, SMALL_SCALE);
            rows_->add_child(hc);
            holders_.push_back(hc);
            cards_.push_back(card);
        }
        close_ = button(screen->find_unique("Close"), false);
        confirm_ = button(screen->find_unique("Confirm"), true);
        show_button(close_, cancelable_, true);
        show_button(confirm_, min_ != max_ && 0 >= min_, true);
        if (!holders_.empty()) focus(0);
    }
    root->add_child(screen);
    root->add_child(frame_driver(this));
    return root;
}

/* NChooseACardSelectionScreen: the banner, the cards 340 px apart, Skip. */
Node* CardSelectScreen::build_row(const Json& d) {
    Node* screen = instance(lib_, "scenes/screens/card_selection/choose_a_card_selection_screen.tscn");
    if (!screen) screen = full_rect_control("ChooseACardSelectionScreen");
    Node* banner = screen->get_node("Banner");
    if (banner) set_label(banner->find_unique("Label"), Loc::get("gameplay_ui", "CHOOSE_CARD_HEADER"));
    hide(screen->find_unique("CombatPiles"));
    hide(screen->find_unique("PeekButton"));
    hide(screen->find_unique("InspectPrompt"));
    Node* row = screen->get_node("CardRow");
    if (!row) row = screen;
    const Json& cs = d["cards"];
    int n = (int)cs.size();
    for (int i = 0; i < n; ++i) {
        Node* h = instance(lib_, "scenes/cards/holders/grid_card_holder.tscn");
        if (!h || !h->is_control()) {
            delete h;
            h = SceneLibrary::create_node("Control");
        }
        Control* hc = (Control*)h;
        Node* card = make_card(lib_, cs[(size_t)i]);
        if (card) hc->add_child(card);
        float x = -(float)(n - 1) * ROW_STEP * 0.5f + ROW_STEP * (float)i;
        place(hc, x, 0, 0, 0);
        hc->scale = Vec2(SMALL_SCALE, SMALL_SCALE);
        /* Fades in from black. */
        hc->modulate = Color(0, 0, 0, 1);
        tweens_.tween(&hc->modulate.r, 1, 1.0f, TRANS_CUBIC, EASE_OUT);
        tweens_.tween(&hc->modulate.g, 1, 1.0f, TRANS_CUBIC, EASE_OUT);
        tweens_.tween(&hc->modulate.b, 1, 1.0f, TRANS_CUBIC, EASE_OUT);
        row->add_child(hc);
        holders_.push_back(hc);
        cards_.push_back(card);
    }
    Node* skip = screen->get_node("SkipButton");
    if (min_ <= 0) show_glyph(lib_, find_named(skip, "HotkeyIcon"), "b");
    else hide(skip);
    columns_ = n > 0 ? n : 1;
    if (n > 0) focus(n / 2);
    return screen;
}

/* NGridCardHolder focus: HoverScale and in front; the others ease back. */
void CardSelectScreen::focus(int i) {
    if (holders_.empty()) return;
    if (i < 0) i = 0;
    if (i >= (int)holders_.size()) i = (int)holders_.size() - 1;
    if (focus_ >= 0 && focus_ < (int)holders_.size() && focus_ != i) {
        Control* old = holders_[(size_t)focus_];
        old->z_index = 0;
        tweens_.tween(&old->scale, Vec2(SMALL_SCALE, SMALL_SCALE), 0.5f, TRANS_EXPO, EASE_OUT);
    }
    focus_ = i;
    Control* h = holders_[(size_t)i];
    tweens_.kill(&h->scale);
    h->scale = Vec2(HOVER_SCALE, HOVER_SCALE);
    h->z_index = 1;
    if (!row_) {
        /* NCardGrid.ProcessGuiFocus: the focused row to the middle. */
        float y = h->offset[SIDE_TOP];
        float to = -y + grid_h_ * 0.5f;
        scroll_to_ = to < lim_lo_ ? lim_lo_ : (to > lim_hi_ ? lim_hi_ : to);
    }
}

/* NDeckCardSelectScreen.OnCardClicked. */
void CardSelectScreen::toggle(int i) {
    if (i < 0 || i >= (int)picked_.size()) return;
    if (!picked_[(size_t)i]) {
        picked_[(size_t)i] = true;
        order_.push_back(i);
        set_card_highlight(cards_[(size_t)i], true);
    } else {
        picked_[(size_t)i] = false;
        for (size_t k = 0; k < order_.size(); ++k)
            if (order_[k] == i) {
                order_.erase(order_.begin() + (long)k);
                break;
            }
        set_card_highlight(cards_[(size_t)i], false);
    }
    show_button(confirm_, min_ != max_ && (int)order_.size() >= min_);
}

/* PreviewSelection: the picked cards over the dimmed grid, or (one card to
 * upgrade) the card before and after. */
void CardSelectScreen::open_preview() {
    Control* box = order_.size() == 1 && single_box_ ? single_box_ : multi_box_;
    if (!box) return;
    preview_ = true;
    preview_box_ = box;
    /* The grid lets go of the focus. */
    if (focus_ >= 0 && focus_ < (int)holders_.size()) {
        Control* h = holders_[(size_t)focus_];
        h->z_index = 0;
        tweens_.kill(&h->scale);
        h->scale = Vec2(SMALL_SCALE, SMALL_SCALE);
    }
    box->visible = true;
    show_button(close_, false);
    show_button(confirm_, false);
    Node* cancel = box->find_unique("PreviewCancel");
    if (!cancel) cancel = box->get_node("Cancel");
    Node* ok = box->find_unique("PreviewConfirm");
    if (!ok) ok = box->get_node("Confirm");
    preview_cancel_ = button(cancel, false);
    preview_confirm_ = button(ok, true);
    show_button(preview_cancel_, true);
    show_button(preview_confirm_, true);
    for (size_t k = 0; k < order_.size(); ++k) set_card_highlight(cards_[(size_t)order_[k]], false);

    const Json& cs = decision_["cards"];
    Node* before = find_named(box, "Before");
    Node* after = find_named(box, "After");
    if (order_.size() == 1 && before) {
        /* NUpgradePreview / the transform's: before, arrows, after. */
        const Json& c = cs[(size_t)order_[0]];
        Node* card = make_card(lib_, c);
        if (card) before->add_child(card);
        const Json& up = c["upgrade_preview"];
        bool shows_after = after && up.is_object();
        if (shows_after) {
            Node* card2 = make_card(lib_, up);
            if (card2) after->add_child(card2);
        }
        set_visible(find_named(box, "Arrows"), shows_after);
        return;
    }
    /* "Cards": an HBox across the middle, 400 px apart and centred, the
     * whole row scaled (1 up to 3 cards, 0.8 to 6, then 0.55). */
    Node* row = find_named(box, "Cards");
    if (!row) row = box;
    hide(row);
    int n = (int)order_.size();
    float s = n > 6 ? 0.55f : (n > 3 ? 0.8f : 1.0f);
    for (int k = 0; k < n; ++k) {
        Node* hn = instance(lib_, "scenes/cards/holders/grid_card_holder.tscn");
        if (!hn || !hn->is_control()) {
            delete hn;
            hn = SceneLibrary::create_node("Control");
        }
        Control* h = (Control*)hn;
        h->name = "PreviewCard";
        Node* card = make_card(lib_, cs[(size_t)order_[(size_t)k]]);
        if (card) h->add_child(card);
        place(h, 960.0f + ((float)k - (float)(n - 1) * 0.5f) * 400.0f * s, 540.0f, 0, 0);
        h->scale = Vec2(s, s);
        box->add_child(h);
    }
}

/* CancelSelection: back to the grid, nothing picked. */
void CardSelectScreen::close_preview() {
    if (!preview_box_) return;
    preview_ = false;
    preview_box_->visible = false;
    show_button(preview_cancel_, false, true);
    show_button(preview_confirm_, false, true);
    const char* holders[2] = { "Before", "After" };
    for (int k = 0; k < 2; ++k) {
        Node* h = find_named(preview_box_, holders[k]);
        while (h && !h->children.empty()) {
            Node* c = h->children.back();
            h->remove_child(c);
            delete c;
        }
    }
    for (size_t k = preview_box_->children.size(); k-- > 0;) {
        Node* c = preview_box_->children[k];
        if (c->name != "PreviewCard") continue;
        preview_box_->remove_child(c);
        delete c;
    }
    for (size_t k = 0; k < picked_.size(); ++k) picked_[k] = false;
    order_.clear();
    show_button(close_, cancelable_);
    show_button(confirm_, min_ != max_ && 0 >= min_);
    int f = focus_;
    focus_ = -1;
    focus(f);
}

std::string CardSelectScreen::confirm() {
    if (order_.empty()) return protocol_action("skip_select");
    std::string list;
    for (size_t k = 0; k < order_.size(); ++k) list += format(k ? ",%d" : "%d", decision_["cards"][(size_t)order_[k]]["index"].i(order_[k]));
    return protocol_action("select_cards", "\"indices\":\"" + list + "\"");
}

std::string CardSelectScreen::input(const PadState& pad) {
    int n = (int)holders_.size();
    if (row_) {
        if (n > 0 && (pad.pressed & PadState::LEFT)) focus(focus_ - 1);
        if (n > 0 && (pad.pressed & PadState::RIGHT)) focus(focus_ + 1);
        if ((pad.pressed & PadState::A) && focus_ >= 0)
            return protocol_action("select_cards", format("\"indices\":\"%d\"", decision_["cards"][(size_t)focus_]["index"].i(focus_)));
        if ((pad.pressed & (PadState::B | PadState::START)) && min_ <= 0) return protocol_action("skip_select");
        return std::string();
    }
    if (preview_) {
        if (pad.pressed & (PadState::A | PadState::Y)) return confirm();
        if (pad.pressed & PadState::B) close_preview();
        return std::string();
    }
    if (n > 0) {
        int col = focus_ % columns_;
        if ((pad.pressed & PadState::LEFT) && col > 0) focus(focus_ - 1);
        if ((pad.pressed & PadState::RIGHT) && col + 1 < columns_ && focus_ + 1 < n) focus(focus_ + 1);
        if ((pad.pressed & PadState::UP) && focus_ - columns_ >= 0) focus(focus_ - columns_);
        if ((pad.pressed & PadState::DOWN) && focus_ / columns_ < (n - 1) / columns_)
            focus(focus_ + columns_ < n ? focus_ + columns_ : n - 1);
        if ((pad.pressed & PadState::A) && focus_ >= 0) {
            toggle(focus_);
            if (max_ > 0 && (int)order_.size() >= max_) {
                if (single_box_ || multi_box_) open_preview();
                else return confirm();
            }
        }
    }
    if ((pad.pressed & PadState::Y) && confirm_.shown) {
        if (!order_.empty() && (single_box_ || multi_box_)) open_preview();
        else return confirm();
    }
    if ((pad.pressed & PadState::B) && cancelable_) return protocol_action("skip_select");
    return std::string();
}

void CardSelectScreen::frame(double dt) {
    tweens_.update(dt);
    if (!rows_) return;
    /* NCardGrid.UpdateScrollPosition: lerp at 15/s, snapping close in. */
    if (fabsf(scroll_ - scroll_to_) > 0.1f) {
        float k = (float)dt * 15.0f;
        scroll_ += (scroll_to_ - scroll_) * (k > 1 ? 1 : k);
        if (fabsf(scroll_ - scroll_to_) < 0.5f) scroll_ = scroll_to_;
    }
    rows_->offset[SIDE_TOP] = rows_->offset[SIDE_BOTTOM] = scroll_;
}

} /* namespace spire */
