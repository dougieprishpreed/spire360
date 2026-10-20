/* NInspectCardScreen (scenes/screens/inspect_card_screen.tscn) and
 * NInspectRelicScreen (scenes/screens/inspect_relic_screen) over the screen
 * that opens them: one card / relic of a list at a time, large, with its
 * hover tips; left / right step through the list (the arrows), B closes.
 * The card screen's Upgrade tickbox (Y) shows the upgrade preview.
 *
 * The entries are the logic's: a card with "inspect" {base, upgraded,
 * start_upgraded, upgradable} (RunDriver.InspectCardView), else the card
 * itself and its "upgrade_preview"; a relic with InspectRelicView's fields
 * (unlocked, seen, title, rarity, rarity_text, description, flavor,
 * inspect_tips, icon_base). */
#ifndef SPIRE_INSPECT_SCREENS_H
#define SPIRE_INSPECT_SCREENS_H

#include "../scene/tween.h"
#include "run_screen.h"
#include "ui_util.h"

namespace spire {

class Control;
class SceneLibrary;

class InspectCardScreen : public FrameListener {
public:
    explicit InspectCardScreen(SceneLibrary* lib);
    /* A new node over everything (the caller adds it, and owns it). */
    Node* build(const Json& cards, int index, bool view_all_upgraded, std::string* error);
    /* false: closed (B). */
    bool input(const PadState& pad);
    virtual void frame(double dt);

private:
    void set_card(int index, int slide);
    void show();
    SceneLibrary* lib_;
    Node* screen_;
    Control* holder_;
    Node* card_;
    Control* tips_;
    Json cards_;
    int index_;
    bool ticked_, view_all_;
    TweenSet tweens_;
};

class InspectRelicScreen : public FrameListener {
public:
    explicit InspectRelicScreen(SceneLibrary* lib);
    Node* build(const Json& relics, int index, std::string* error);
    bool input(const PadState& pad);
    virtual void frame(double dt);

private:
    void set_relic(int index, int slide);
    SceneLibrary* lib_;
    Node* screen_;
    Control* popup_;
    Control* tips_;
    Json relics_;
    int index_;
    Vec2 popup_pos_;
    TweenSet tweens_;
};

} /* namespace spire */

#endif
