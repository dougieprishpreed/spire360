/* Choosing a pack of cards (bundle_select decisions: the Scroll Boxes
 * event), as NChooseABundleSelectionScreen: "Choose a Pack" over the room,
 * the packs (NCardBundle: their cards fanned, darker towards the back)
 * 400 px apart; left / right move the focus, A opens the focused pack: its
 * cards in a row with Cancel (B) and Confirm (Y, or A) to take it. */
#ifndef SPIRE_BUNDLE_SELECT_SCREEN_H
#define SPIRE_BUNDLE_SELECT_SCREEN_H

#include "../scene/tween.h"
#include "run_screen.h"
#include "ui_util.h"

namespace spire {

class Control;
class SceneLibrary;

class BundleSelectScreen : public RunScreen, public FrameListener {
public:
    explicit BundleSelectScreen(SceneLibrary* lib);
    /* What shows behind (the event's room); the tree takes it. */
    void set_background(Node* room) { background_ = room; }
    virtual Node* build(const Json& decision, std::string* error);
    virtual std::string input(const PadState& pad);
    virtual void frame(double dt);

private:
    void focus(int i);
    void open_preview(int i);
    void close_preview();
    void slide_button(Control* b, float x, bool shown);
    SceneLibrary* lib_;
    Node* background_;
    Json decision_;
    Control* banner_;
    Control* row_;
    Control* preview_;
    Control* preview_cards_;
    Control* cancel_;
    Control* confirm_;
    float cancel_x_, confirm_x_;
    std::vector<Control*> bundles_;
    int focus_, open_;
    TweenSet tweens_;
};

} /* namespace spire */

#endif
