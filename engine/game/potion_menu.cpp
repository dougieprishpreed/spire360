#include "potion_menu.h"
#include "combat_screen.h"
#include "loc.h"
#include "ui_util.h"

#include "../scene/controls.h"
#include "../scene/node.h"
#include "../scene/scene.h"

namespace spire {

/* NPotionHolder: _potionScale, and 1.15 times it focused. */
static const float POTION_SCALE = 0.9f, FOCUS_SCALE = 0.9f * 1.15f;

PotionMenu::PotionMenu(SceneLibrary* lib)
    : lib_(lib), layer_(NULL), popup_(NULL), mode_(NONE), slot_(0), button_(0), target_(-1) {
    buttons_[0] = buttons_[1] = NULL;
    enabled_[0] = enabled_[1] = false;
}

void PotionMenu::attach(Node* screen, const Json& player) {
    detach();
    player_ = player;
    Node* box = find_named(screen, "PotionHolders");
    Node* layer = find_named(screen, "TopBarLayer");
    layer_ = layer && layer->is_control() ? (Control*)layer : NULL;
    for (size_t i = 0; box && i < box->children.size(); ++i)
        if (box->children[i]->is_control()) holders_.push_back((Control*)box->children[i]);
}

void PotionMenu::detach() {
    holders_.clear();
    layer_ = NULL;
    popup_ = NULL; /* in the old screen's tree, deleted with it */
    mode_ = NONE;
}

const Json* PotionMenu::potion_at(int slot) const {
    const Json& ps = player_["potions"];
    for (size_t i = 0; i < ps.size(); ++i)
        if (ps[i]["index"].i() == slot) return &ps[i];
    return NULL;
}

bool PotionMenu::open() {
    if (holders_.empty() || player_["potions"].size() == 0) return false;
    mode_ = SLOTS;
    int first = 0;
    for (int s = (int)holders_.size() - 1; s >= 0; --s)
        if (potion_at(s)) first = s;
    slot_ = -1;
    focus_slot(first);
    return true;
}

void PotionMenu::close() {
    close_popup();
    if (slot_ >= 0 && slot_ < (int)holders_.size()) {
        Control* h = holders_[(size_t)slot_];
        Node* ret = h->find_unique("SelectionReticle");
        if (ret && ret->is_canvas_item()) ((CanvasItem*)ret)->visible = false;
        Node* pot = find_named(h, "Potion");
        if (pot && pot->is_control()) ((Control*)pot)->scale = Vec2(POTION_SCALE, POTION_SCALE);
    }
    mode_ = NONE;
    slot_ = -1;
}

/* OnFocus: the reticle and the potion at 1.15; the slot left goes back. */
void PotionMenu::focus_slot(int s) {
    if (s < 0 || s >= (int)holders_.size()) return;
    if (slot_ >= 0 && slot_ < (int)holders_.size()) {
        Control* h = holders_[(size_t)slot_];
        hide(h->find_unique("SelectionReticle"));
        Node* pot = find_named(h, "Potion");
        if (pot && pot->is_control()) ((Control*)pot)->scale = Vec2(POTION_SCALE, POTION_SCALE);
    }
    slot_ = s;
    Control* h = holders_[(size_t)s];
    Node* ret = h->find_unique("SelectionReticle");
    if (ret && ret->is_canvas_item()) {
        ((CanvasItem*)ret)->visible = true;
        ((CanvasItem*)ret)->modulate.a = 1;
    }
    Node* pot = find_named(h, "Potion");
    if (pot && pot->is_control()) ((Control*)pot)->scale = Vec2(FOCUS_SCALE, FOCUS_SCALE);
}

/* NPotionPopup._Ready: under the slot, Use (drink / throw) and Discard,
 * each enabled as the logic says; the first enabled one focused. */
void PotionMenu::open_popup() {
    const Json* p = potion_at(slot_);
    if (!p || !layer_) return;
    Node* n = instance(lib_, "scenes/potions/potion_popup.tscn");
    if (!n || !n->is_control()) {
        delete n;
        return;
    }
    popup_ = (Control*)n;
    Control* h = holders_[(size_t)slot_];
    Vec2 hp = h->global_transform().xform(Vec2(0, 0));
    float w = popup_->offset[SIDE_RIGHT] - popup_->offset[SIDE_LEFT], ht = popup_->offset[SIDE_BOTTOM] - popup_->offset[SIDE_TOP];
    for (int k = 0; k < 4; ++k) popup_->anchor[k] = 0;
    place(popup_, hp.x + h->size.x * 0.5f - w * 0.5f, hp.y + h->size.y * 1.5f, w, ht);
    buttons_[0] = ctl(popup_, "UseButton");
    buttons_[1] = ctl(popup_, "DiscardButton");
    enabled_[0] = (*p)["usable"].b(true);
    enabled_[1] = (*p)["discardable"].b(true);
    const char* use = (*p)["use_label"].str() == "throw" ? "POTION_POPUP.throw" : "POTION_POPUP.drink";
    if (buttons_[0]) set_label(buttons_[0]->get_node("Label"), Loc::get("gameplay_ui", use));
    if (buttons_[1]) set_label(buttons_[1]->get_node("Label"), Loc::get("gameplay_ui", "POTION_POPUP.discard"));
    for (int b = 0; b < 2; ++b) {
        Node* bg = buttons_[b] ? buttons_[b]->get_node("Background") : NULL;
        if (bg && bg->is_canvas_item())
            ((CanvasItem*)bg)->modulate = enabled_[b] ? Color(1, 1, 1, 0) : Color(0.1f, 0.1f, 0.1f, 0.75f);
    }
    layer_->add_child(popup_);
    mode_ = POPUP;
    button_ = -1;
    focus_button(enabled_[0] ? 0 : (enabled_[1] ? 1 : -1));
}

void PotionMenu::close_popup() {
    if (popup_) {
        if (popup_->parent) popup_->parent->remove_child(popup_);
        delete popup_;
        popup_ = NULL;
    }
    buttons_[0] = buttons_[1] = NULL;
}

/* NPotionPopupButton.OnFocus: its background at 0.25. */
void PotionMenu::focus_button(int b) {
    for (int k = 0; k < 2; ++k) {
        Node* bg = buttons_[k] ? buttons_[k]->get_node("Background") : NULL;
        if (bg && bg->is_canvas_item() && enabled_[k]) ((CanvasItem*)bg)->modulate.a = k == b ? 0.25f : 0.0f;
    }
    button_ = b;
}

std::string PotionMenu::input(const PadState& pad, CombatScreen* combat, const Json& enemies) {
    const Json* p = potion_at(slot_);
    if (mode_ == SLOTS) {
        int n = (int)holders_.size();
        if (pad.pressed & PadState::LEFT) focus_slot((slot_ + n - 1) % n);
        if (pad.pressed & PadState::RIGHT) focus_slot((slot_ + 1) % n);
        if ((pad.pressed & PadState::A) && potion_at(slot_)) open_popup();
        else if (pad.pressed & (PadState::B | PadState::X)) close();
        return std::string();
    }
    if (mode_ == POPUP) {
        if ((pad.pressed & PadState::UP) && enabled_[0]) focus_button(0);
        if ((pad.pressed & PadState::DOWN) && enabled_[1]) focus_button(1);
        if (pad.pressed & PadState::B) {
            close_popup();
            mode_ = SLOTS;
        } else if ((pad.pressed & PadState::A) && p && button_ >= 0) {
            int idx = (*p)["index"].i();
            if (button_ == 1) {
                close();
                return protocol_action("discard_potion", format("\"potion_index\":%d", idx));
            }
            const std::string& type = (*p)["target_type"].str();
            if (type == "AnyEnemy" && combat && enemies.size() > 0) {
                /* The arrow from the slot to the first enemy shown. */
                close_popup();
                mode_ = AIM;
                Control* h = holders_[(size_t)slot_];
                Vec2 from = h->global_transform().xform(h->size * 0.5f);
                target_ = 0;
                combat->begin_aim(from, type, 0);
                for (int t = 0; t < (int)enemies.size(); ++t)
                    if (combat->set_play_target(t)) {
                        target_ = t;
                        break;
                    }
                return std::string();
            }
            close();
            return protocol_action("use_potion", format("\"potion_index\":%d", idx));
        }
        return std::string();
    }
    if (mode_ == AIM) {
        int ne = (int)enemies.size();
        if (pad.pressed & (PadState::LEFT | PadState::RIGHT)) {
            int step = (pad.pressed & PadState::LEFT) ? -1 : 1;
            for (int t = target_ + step; combat && t >= 0 && t < ne; t += step)
                if (combat->set_play_target(t)) {
                    target_ = t;
                    break;
                }
        }
        if (pad.pressed & PadState::B) {
            if (combat) combat->end_play();
            mode_ = SLOTS;
        } else if ((pad.pressed & PadState::A) && p && target_ >= 0 && target_ < ne) {
            int idx = (*p)["index"].i();
            int t = enemies[(size_t)target_]["index"].i();
            if (combat) combat->end_play();
            close();
            return protocol_action("use_potion", format("\"potion_index\":%d,\"target_index\":%d", idx, t));
        }
    }
    return std::string();
}

} /* namespace spire */
