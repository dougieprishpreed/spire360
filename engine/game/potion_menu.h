/* The potions on the top bar with a pad (NTopBar's potion shortcut, X):
 * the slots take the focus, the first one whether or not it holds a
 * potion (left / right, the holder's reticle and the potion a little
 * larger); A opens the potion's popup (NPotionPopup: Use, Discard; up /
 * down, A), B goes back, X leaves. Down goes to the relic row (left / right
 * around it, each relic larger with its tips; up back to the first slot).
 * A potion aimed at an enemy then aims like a card (the combat screen's
 * arrow from the slot; left / right, A throws, B cancels). */
#ifndef SPIRE_POTION_MENU_H
#define SPIRE_POTION_MENU_H

#include "run_screen.h"
#include "ui_util.h"

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
    /* Along the potion slots or the relic row (no popup, nothing aimed). */
    bool browsing() const { return mode_ == SLOTS || mode_ == RELICS; }
    /* X: the first slot takes the focus; false with no top bar. */
    bool open();
    void close();
    /* While active: a command for the logic or "". `combat` (may be NULL)
     * aims at its enemies. */
    std::string input(const PadState& pad, CombatScreen* combat, const Json& enemies);

private:
    enum Mode { NONE, SLOTS, POPUP, AIM, RELICS };
    void focus_slot(int s);
    void unfocus_slot();
    void focus_relic(int r);
    void unfocus_relic();
    void open_popup();
    void close_popup();
    void focus_button(int b);
    const Json* potion_at(int slot) const;
    SceneLibrary* lib_;
    Json player_;
    std::vector<Control*> holders_;
    std::vector<Control*> relics_; /* the relic row's holders */
    int relic_;
    Control* layer_; /* where the popup goes (the top bar's layer) */
    Control* popup_;
    Control* buttons_[2];
    bool enabled_[2];
    Mode mode_;
    int slot_, button_, target_;
    TipFollower tips_; /* the focused potion's (NPotionHolder.OnFocus) */
};

} /* namespace spire */

#endif
