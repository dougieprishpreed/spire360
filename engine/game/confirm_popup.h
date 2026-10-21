/* NAbandonRunConfirmPopup (scenes/ui/abandon_run_confirm_popup.tscn, an
 * NVerticalPopup) in NModalContainer: "Abandon Run?" over a backstop that
 * darkens to 85% black in 0.3 s. Confirm takes the select hotkey (A),
 * Cancel the cancel and pause hotkeys (B, Start); the popup fades in from
 * transparent black over 0.1 s while it rises the last 100 px (Back Out,
 * 0.3 s). The main menu's Abandon Run and the pause menu's Give Up both
 * open it. */
#ifndef SPIRE_CONFIRM_POPUP_H
#define SPIRE_CONFIRM_POPUP_H

#include "../scene/tween.h"
#include "run_screen.h"
#include "ui_util.h"

namespace spire {

class Control;
class ColorRect;
class SceneLibrary;

class ConfirmPopup : public FrameListener {
public:
    enum Action { NONE, YES, NO };
    explicit ConfirmPopup(SceneLibrary* lib);
    /* A new tree (the caller owns it): the backstop and the popup. */
    Node* build(std::string* error);
    /* NGenericPopup.WaitForConfirmation (scenes/ui/generic_popup.tscn):
     * header and body, Cancel and Confirm. */
    Node* build_generic(const std::string& header, const std::string& body, std::string* error);
    /* Another scene with a VerticalPopup (NAcceptTutorialsFtue's). */
    Node* build_scene(const char* scene, const std::string& header, const std::string& body, std::string* error) {
        return make(scene, header, body, error);
    }
    Action input(const PadState& pad);
    virtual void frame(double dt);

private:
    void press(Control* visuals);
    Node* make(const char* scene, const std::string& header, const std::string& body, std::string* error);
    SceneLibrary* lib_;
    ColorRect* backstop_;
    Control* popup_;
    Control* yes_;
    Control* no_;
    float rest_y_;
    TweenSet tweens_;
};

} /* namespace spire */

#endif
