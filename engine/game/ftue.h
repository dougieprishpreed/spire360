/* The FTUEs (NFtue screens, scenes/ftue/<id>.tscn) in NModalContainer: a
 * popup over the screen, NModalContainer's backstop darkening it (85%
 * black in 0.3 s) except the control the tip is about, lifted over it
 * (ZIndex + 1 until it closes). The common one (FtuePopup/Header,
 * /DescriptionContainer/Description, /FtueConfirmButton: "Got it!", its
 * outline pulsing) closes on A (the confirm button's select hotkey).
 * NCombatRulesFtue turns three pages (A or right: the next, the last
 * closes; left: the previous); NMapSelectFtue stands at the first room
 * (%Positioner). NAcceptTutorialsFtue is a ConfirmPopup (menu_flow). The
 * mouse-only SneakyHitboxes (end the turn, open the merchant's rug) are
 * not ported. */
#ifndef SPIRE_FTUE_H
#define SPIRE_FTUE_H

#include "../scene/tween.h"
#include "run_screen.h"
#include "ui_util.h"

#include <string>
#include <vector>

namespace spire {

class CanvasItem;
class ColorRect;
class Control;
class Node;
class Resource;
class SceneLibrary;

class FtuePopup : public FrameListener {
public:
    explicit FtuePopup(SceneLibrary* lib);
    /* id: NxxxFtue.id ("combat_rules_ftue"). spotlights: the controls the
     * tip is about; at: NMapSelectFtue's room centre. The tree (the caller
     * adds it over the screens and frees it after close()); NULL when the
     * FTUE has no scene. */
    Node* build(const std::string& id, const std::vector<CanvasItem*>& spotlights, const Vec2* at,
                std::string* error);
    /* One frame of pad input: true when it closed (NFtue.CloseFtue). */
    bool input(const PadState& pad);
    /* The spotlight's ZIndex back (before its screen goes). */
    void close();
    const std::string& id() const { return id_; }
    virtual void frame(double dt);

private:
    void turn_page(int dir);
    SceneLibrary* lib_;
    std::string id_;
    ColorRect* backstop_;
    Control* root_;
    std::vector<CanvasItem*> spotlights_;
    std::vector<int> spotlight_z_;
    Control* outline_;
    double pulse_t_;
    TweenSet tweens_;
    /* NCombatRulesFtue */
    int page_;
    Control* image_;
    Control* body_;
    Control* prev_;
    Node* page_count_;
    float image_x_, body_x_;
    Resource* images_[3];
};

} /* namespace spire */

#endif
