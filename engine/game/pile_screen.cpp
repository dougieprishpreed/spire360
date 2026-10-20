#include "pile_screen.h"

#include "../scene/controls.h"
#include "../scene/scene.h"
#include "card_view.h"
#include "loc.h"

#include <algorithm>
#include <math.h>
#include <string.h>

namespace spire {

/* NCardGrid (as CardSelectScreen): the holders' small scale, padding and
 * margins, the ScrollContainer's sides. */
static const float CARD_W = 300.0f * 0.8f, CARD_H = 422.0f * 0.8f;
static const float PADDING = 40.0f, BOTTOM_MARGIN = 320.0f;
static const float SMALL_SCALE = 0.8f, HOVER_SCALE = 1.0f;
static const float SCROLL_LEFT = 150.0f, SCROLL_RIGHT = 200.0f, GRID_TOP = 80.0f;

PileScreen::PileScreen(SceneLibrary* lib)
    : lib_(lib), deck_(false), upgrades_(false), root_(NULL), rows_(NULL), focus_(-1), sorter_focus_(-1), columns_(1),
      top_(80), scroll_(0), scroll_to_(0), lim_lo_(0), lim_hi_(0), grid_h_(1000), tickbox_(NULL) {
    memset(sorters_, 0, sizeof(sorters_));
}

Node* PileScreen::build(const Json& msg, std::string* error) {
    msg_ = msg;
    pile_ = msg["pile"].str();
    deck_ = pile_ == "deck";
    upgrades_ = false;
    holders_.clear();
    order_.clear();
    focus_ = -1;
    sorter_focus_ = -1;
    tweens_.clear();
    /* NDeckViewScreen(): obtained, then type, cost, alphabet */
    priority_.clear();
    priority_.push_back(OBTAINED * 2);
    priority_.push_back(TYPE * 2);
    priority_.push_back(COST * 2);
    priority_.push_back(ALPHABET * 2);

    Control* root = full_rect_control(deck_ ? "DeckViewScreen" : "CardPileScreen");
    root_ = root;
    Node* screen = instance(lib_, deck_ ? "scenes/screens/deck_view_screen.tscn" : "scenes/screens/card_pile_screen.tscn");
    if (!screen) {
        if (error) *error = "no pile screen scene";
        delete root;
        return NULL;
    }
    if (deck_) {
        /* the shared backstop (NCapstoneContainer's) */
        ColorRect* back = (ColorRect*)SceneLibrary::create_node("ColorRect");
        back->name = "OverlayBackstop";
        back->anchor[SIDE_RIGHT] = back->anchor[SIDE_BOTTOM] = 1;
        back->color = Color(0, 0, 0, 0.7f);
        root->add_child(back);
    } else {
        /* _background fades in to screenBackdrop over a second */
        Node* bg = screen->get_node("Background");
        if (bg && bg->is_canvas_item()) {
            CanvasItem* b = (CanvasItem*)bg;
            b->modulate = Color(1, 1, 1, 0);
            tweens_.tween(&b->modulate.a, 1.0f, 1.0f, TRANS_EXPO, EASE_OUT);
        }
    }
    root->add_child(screen);
    std::string info = msg["info"].str();
    Node* label = screen->find_unique("BottomLabel");
    if (info.empty()) hide(find_named(screen, "BottomText"));
    else set_label(label, "[center]" + info);
    show_glyph(lib_, find_named(screen->get_node("BackButton"), "HotkeyIcon"), "b");

    /* the sorters and View Upgrades (the deck) */
    static const char* const sorter_nodes[SORTS] = { "ObtainedSorter", "CardTypeSorter", "CostSorter", "AlphabeticalSorter" };
    static const char* const sorter_keys[SORTS] = { "SORT_OBTAINED", "SORT_TYPE", "SORT_COST", "SORT_ALPHABET" };
    for (int i = 0; i < SORTS && deck_; ++i) {
        Node* s = screen->find_unique(sorter_nodes[i]);
        sorters_[i].node = s && s->is_control() ? (Control*)s : NULL;
        if (!s) continue;
        set_label(s->find_unique("Label"), Loc::get("gameplay_ui", sorter_keys[i]));
        Node* img = s->find_unique("Image");
        sorters_[i].arrow = img && img->type == "TextureRect" ? (TextureRect*)img : NULL;
        if (sorters_[i].arrow) sorters_[i].arrow->flip_v = true; /* SetFlipV(!_isDescending) */
        sorters_[i].reticle = s->get_node("SelectionReticle");
        hide(sorters_[i].reticle);
        sorters_[i].descending = false;
    }
    tickbox_ = NULL;
    if (deck_) {
        set_label(screen->find_unique("ViewUpgradesLabel"), Loc::get("gameplay_ui", "VIEW_UPGRADES"));
        show_glyph(lib_, screen->find_unique("HotkeyIcon"), "y");
        tickbox_ = screen->find_unique("TickboxVisuals");
        set_visible(tickbox_ ? tickbox_->get_node("Ticked") : NULL, false);
        set_visible(tickbox_ ? tickbox_->get_node("NotTicked") : NULL, true);
    } else {
        hide(find_named(screen, "ViewUpgrades"));
    }

    /* The grid: under the sorting options for the deck (YOffset 100), under
     * the top bar for a pile (InsetForTopBar). */
    Node* grid = screen->find_unique("CardGrid");
    if (!grid) grid = find_named(screen, "CardGrid");
    Node* scroll = grid ? find_named(grid, "ScrollContainer") : NULL;
    hide(grid ? grid->get_node("Scrollbar") : NULL);
    rows_ = full_rect_control("Rows");
    if (scroll) scroll->add_child(rows_);
    else root->add_child(rows_);
    top_ = deck_ ? 180.0f : 80.0f;
    make_cards();
    for (size_t i = 0; i < holders_.size(); ++i) order_.push_back((int)i);
    sort();
    root->add_child(frame_driver(this));
    return root;
}

/* The cards' holders, the upgraded ones while View Upgrades is ticked. */
void PileScreen::make_cards() {
    for (size_t i = 0; i < holders_.size(); ++i) {
        if (holders_[i]->parent) holders_[i]->parent->remove_child(holders_[i]);
        delete holders_[i];
    }
    holders_.clear();
    const Json& cs = msg_["cards"];
    for (size_t i = 0; i < cs.size(); ++i) {
        Node* h = instance(lib_, "scenes/cards/holders/grid_card_holder.tscn");
        if (!h || !h->is_control()) {
            delete h;
            h = SceneLibrary::create_node("Control");
        }
        Control* hc = (Control*)h;
        const Json& shown = upgrades_ && cs[i]["upgrade_preview"].is_object() ? cs[i]["upgrade_preview"] : cs[i];
        Node* card = make_card(lib_, shown);
        if (card) hc->add_child(card);
        hc->scale = Vec2(SMALL_SCALE, SMALL_SCALE);
        rows_->add_child(hc);
        holders_.push_back(hc);
    }
}

/* NCardGrid.SortingAlgorithms; the card's index is its place in the pile. */
int PileScreen::compare(int a, int b, int key) const {
    const Json& x = msg_["cards"][(size_t)a];
    const Json& y = msg_["cards"][(size_t)b];
    int sort = key / 2, r = 0;
    if (sort == OBTAINED) {
        r = a < b ? -1 : (a > b ? 1 : 0);
    } else if (sort == TYPE) {
        static const char* const types[] = { "None", "Attack", "Skill", "Power", "Status", "Curse", "Quest" };
        int tx = 0, ty = 0;
        for (int i = 0; i < 7; ++i) {
            if (x["type"].str() == types[i]) tx = i;
            if (y["type"].str() == types[i]) ty = i;
        }
        r = tx < ty ? -1 : (tx > ty ? 1 : 0);
    } else if (sort == COST) {
        int cx = x["cost"].i(), cy = y["cost"].i();
        r = cx < cy ? -1 : (cx > cy ? 1 : 0);
    } else {
        const std::string& nx = x["title"].is_string() ? x["title"].str() : x["name"].str();
        const std::string& ny = y["title"].is_string() ? y["title"].str() : y["name"].str();
        r = nx < ny ? -1 : (nx > ny ? 1 : 0);
    }
    return (key & 1) ? -r : r;
}

/* NCardGrid.SetCards: obtained order first is the pile as it is (reversed
 * when descending), else sorted by the priorities, then the id. */
void PileScreen::sort() {
    int n = (int)holders_.size();
    order_.clear();
    for (int i = 0; i < n; ++i) order_.push_back(i);
    if (deck_ && priority_[0] == OBTAINED * 2 + 1) {
        std::reverse(order_.begin(), order_.end());
    } else if (deck_ && priority_[0] / 2 != OBTAINED) {
        /* insertion sort: stable, few cards */
        for (int i = 1; i < n; ++i) {
            int v = order_[(size_t)i], j = i - 1;
            for (; j >= 0; --j) {
                int c = 0;
                for (size_t k = 0; k < priority_.size() && c == 0; ++k) c = compare(order_[(size_t)j], v, priority_[k]);
                if (c == 0) {
                    const std::string& ia = msg_["cards"][(size_t)order_[(size_t)j]]["id"].str();
                    const std::string& ib = msg_["cards"][(size_t)v]["id"].str();
                    c = ia < ib ? -1 : (ia > ib ? 1 : 0);
                }
                if (c <= 0) break;
                order_[(size_t)j + 1] = order_[(size_t)j];
            }
            order_[(size_t)j + 1] = v;
        }
    }
    layout();
    if (sorter_focus_ >= 0) return; /* the sorter keeps the focus */
    if (focus_ < 0 && n > 0) focus(0);
    else if (focus_ >= 0) focus(focus_);
}

/* Columns as fit, centred; the scroll's limits. */
void PileScreen::layout() {
    float width = 1920.0f - SCROLL_LEFT - SCROLL_RIGHT;
    columns_ = (int)((width + PADDING) / (CARD_W + PADDING));
    if (columns_ < 1) columns_ = 1;
    int n = (int)order_.size();
    int rows = (n + columns_ - 1) / columns_;
    float content_w = (float)columns_ * CARD_W + (float)(columns_ - 1) * PADDING;
    float content_h = (float)rows * CARD_H + (float)(rows > 0 ? rows - 1 : 0) * PADDING;
    float scroll_h = content_h + top_ + BOTTOM_MARGIN;
    grid_h_ = 1080.0f - GRID_TOP;
    float lim_top = grid_h_ > scroll_h ? (grid_h_ - scroll_h) * 0.5f : 0.0f;
    float lim_bottom = grid_h_ > scroll_h ? (grid_h_ - scroll_h) * 0.5f : grid_h_ - scroll_h;
    lim_lo_ = minf(minf(lim_top, lim_bottom), 0.0f);
    lim_hi_ = maxf(minf(lim_top, lim_bottom), 0.0f);
    if (lim_top > 0) lim_lo_ = lim_hi_ = lim_top;
    if (scroll_to_ < lim_lo_ || scroll_to_ > lim_hi_ || order_.empty()) scroll_ = scroll_to_ = lim_top > 0 ? lim_top : 0;
    for (int i = 0; i < n; ++i) {
        Control* hc = holders_[(size_t)order_[(size_t)i]];
        int r = i / columns_, c = i % columns_;
        place(hc, (width - content_w) * 0.5f + CARD_W * 0.5f + (float)c * (CARD_W + PADDING),
              top_ + CARD_H * 0.5f + (float)r * (CARD_H + PADDING), 0, 0);
    }
}

void PileScreen::focus(int i) {
    int n = (int)order_.size();
    if (n == 0) return;
    if (i < 0) i = 0;
    if (i >= n) i = n - 1;
    if (focus_ >= 0 && focus_ < n && focus_ != i) {
        Control* old = holders_[(size_t)order_[(size_t)focus_]];
        old->z_index = 0;
        tweens_.tween(&old->scale, Vec2(SMALL_SCALE, SMALL_SCALE), 0.5f, TRANS_EXPO, EASE_OUT);
    }
    focus_ = i;
    Control* h = holders_[(size_t)order_[(size_t)i]];
    tweens_.kill(&h->scale);
    h->scale = Vec2(HOVER_SCALE, HOVER_SCALE);
    h->z_index = 1;
    tips_.show_for_card(lib_, root_, h, msg_["cards"][(size_t)order_[(size_t)i]]["tips"]);
    /* NCardGrid.ProcessGuiFocus: the focused row to the middle */
    float to = -h->offset[SIDE_TOP] + grid_h_ * 0.5f;
    scroll_to_ = to < lim_lo_ ? lim_lo_ : (to > lim_hi_ ? lim_hi_ : to);
}

void PileScreen::focus_sorter(int i) {
    for (int k = 0; k < SORTS; ++k) set_visible(sorters_[k].reticle, k == i);
    sorter_focus_ = i;
    if (i >= 0) {
        if (focus_ >= 0 && focus_ < (int)order_.size()) {
            Control* old = holders_[(size_t)order_[(size_t)focus_]];
            old->z_index = 0;
            tweens_.tween(&old->scale, Vec2(SMALL_SCALE, SMALL_SCALE), 0.5f, TRANS_EXPO, EASE_OUT);
        }
        tips_.hide();
        scroll_to_ = lim_hi_;
    }
}

bool PileScreen::input(const PadState& pad, unsigned close_buttons) {
    if (pad.pressed & (PadState::B | close_buttons)) return false;
    int n = (int)order_.size();
    if (sorter_focus_ >= 0) {
        if ((pad.pressed & PadState::LEFT) && sorter_focus_ > 0) focus_sorter(sorter_focus_ - 1);
        if ((pad.pressed & PadState::RIGHT) && sorter_focus_ + 1 < SORTS) focus_sorter(sorter_focus_ + 1);
        if (pad.pressed & PadState::A) {
            /* OnRelease: the direction turns; this sort leads */
            Sorter& s = sorters_[sorter_focus_];
            s.descending = !s.descending;
            if (s.arrow) s.arrow->flip_v = !s.descending;
            int key = sorter_focus_ * 2;
            for (size_t k = 0; k < priority_.size(); ++k)
                if (priority_[k] / 2 == sorter_focus_) {
                    priority_.erase(priority_.begin() + (long)k);
                    break;
                }
            priority_.insert(priority_.begin(), key + (s.descending ? 1 : 0));
            sort();
        }
        if ((pad.pressed & PadState::DOWN) && n > 0) {
            int s = sorter_focus_;
            focus_sorter(-1);
            int start = focus_ >= 0 ? focus_ % columns_ : 0;
            focus_ = -1;
            (void)s;
            focus(start);
        }
    } else if (n > 0) {
        int col = focus_ % columns_;
        if ((pad.pressed & PadState::LEFT) && col > 0) focus(focus_ - 1);
        if ((pad.pressed & PadState::RIGHT) && col + 1 < columns_ && focus_ + 1 < n) focus(focus_ + 1);
        if (pad.pressed & PadState::UP) {
            if (focus_ - columns_ >= 0) focus(focus_ - columns_);
            else if (deck_ && sorters_[0].node) focus_sorter(0);
        }
        if ((pad.pressed & PadState::DOWN) && focus_ / columns_ < (n - 1) / columns_)
            focus(focus_ + columns_ < n ? focus_ + columns_ : n - 1);
    } else if (deck_ && (pad.pressed & PadState::UP) && sorters_[0].node) {
        focus_sorter(0);
    }
    if (deck_ && (pad.pressed & PadState::Y)) {
        /* NUpgradePreviewTickbox */
        upgrades_ = !upgrades_;
        set_visible(tickbox_ ? tickbox_->get_node("Ticked") : NULL, upgrades_);
        set_visible(tickbox_ ? tickbox_->get_node("NotTicked") : NULL, !upgrades_);
        int f = focus_;
        make_cards();
        focus_ = -1;
        sort();
        if (sorter_focus_ < 0 && f >= 0) focus(f);
        else if (sorter_focus_ >= 0) focus_sorter(sorter_focus_);
    }
    return true;
}

void PileScreen::frame(double dt) {
    tweens_.update(dt);
    tips_.frame();
    if (!rows_) return;
    /* NCardGrid.UpdateScrollPosition: lerp at 15/s, snapping close in */
    if (fabsf(scroll_ - scroll_to_) > 0.1f) {
        float k = (float)dt * 15.0f;
        scroll_ += (scroll_to_ - scroll_) * (k > 1 ? 1 : k);
        if (fabsf(scroll_ - scroll_to_) < 0.5f) scroll_ = scroll_to_;
    }
    rows_->offset[SIDE_TOP] = rows_->offset[SIDE_BOTTOM] = scroll_;
}

} /* namespace spire */
