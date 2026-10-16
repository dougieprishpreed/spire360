/* The potions on the top bar with a pad (NTopBar's potion shortcut, X):
 * the slots take the focus (left / right, the holder's reticle and the
 * potion a little larger); A opens the potion's popup (NPotionPopup: Use,
 * Discard; up / down, A), B goes back, X leaves. A potion aimed at an enemy
 * then aims like a card (the combat screen's arrow from the slot; left /
 * right, A throws, B cancels). */
#ifndef SPIRE_POTION_MENU_H
#define SPIRE_POTION_MENU_H

#include "run_screen.h"

namespace spire {

class CombatScreen;
class Control;
class SceneLibrary;

class PotionMenu {
public:
    explicit PotionMenu(SceneLibrary* lib);
    /* The screen just shown (its top bar's PotionHolders) and the player. */
    void attach(Node* screen, const Json& player);
    void detach();
    bool active() const { return mode_ != NONE; }
    /* X: the slots take the focus; false when there are no potions. */
    bool open();
    void close();
    /* While active: a command for the logic or "". `combat` (may be NULL)
     * aims at its enemies. */
    std::string input(const PadState& pad, CombatScreen* combat, const Json& enemies);

private:
    enum Mode { NONE, SLOTS, POPUP, AIM };
    void focus_slot(int s);
    void open_popup();
    void close_popup();
    void focus_button(int b);
    const Json* potion_at(int slot) const;
    SceneLibrary* lib_;
    Json player_;
    std::vector<Control*> holders_;
    Control* layer_; /* where the popup goes (the top bar's layer) */
    Control* popup_;
    Control* buttons_[2];
    bool enabled_[2];
    Mode mode_;
    int slot_, button_, target_;
};

} /* namespace spire */

#endif
