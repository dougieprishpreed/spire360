/* The rest site (NRestSiteRoom, scenes/rooms/rest_site_room.tscn) from a
 * rest_site decision: the act's campfire background, the player resting by
 * it (Character.RestSiteAnimPath, the act's loop), the options' buttons
 * (NRestSiteButton: left / right moves the focus, wrapping; A chooses an
 * enabled one) with the focused one's description below them.
 *
 * Once the option is used (the decision's "done"), the choices fade out,
 * the fire goes out (ExtinguishFireIfAble: the characters darker, the
 * lighting off) and the proceed arrow comes in: Y goes to the map. */
#ifndef SPIRE_REST_SITE_SCREEN_H
#define SPIRE_REST_SITE_SCREEN_H

#include "../scene/tween.h"
#include "proceed_button.h"
#include "run_screen.h"
#include "ui_util.h"

namespace spire {

class Control;
class SceneLibrary;

class RestSiteScreen : public RunScreen, public FrameListener {
public:
    explicit RestSiteScreen(SceneLibrary* lib);
    /* Before build: the option was just chosen (the decision before, with
     * its options): they fade out and the proceed arrow slides in, rather
     * than the room starting that way. */
    void set_after_choice(const Json& before) { before_ = before; }
    /* Before build: the option to focus first. */
    void set_focus(int i) { start_focus_ = i; }
    int focused() const { return focus_; }
    /* Nothing to choose: shown behind another screen (no input). */
    void set_passive() { passive_ = true; }
    virtual Node* build(const Json& decision, std::string* error);
    virtual std::string input(const PadState& pad);
    virtual void frame(double dt);

private:
    struct Option {
        Control* node;
        Control* visuals;
        Control* outline;
        Control* icon; /* hsv s / v */
        Control* label;
        float label_top, label_bottom;
        int index;
        bool enabled;
        std::string description;
    };
    void make_options(Node* room, const Json& options, bool animate_in);
    /* option.DoLocalPostSelectVfx (the logic's rest_vfx cue); its wait in
     * seconds (the proceed arrow comes after it) */
    double post_select(Node* room, const Json& steps);
    void focus(int i);
    void unfocus(int i);
    void extinguish();
    SceneLibrary* lib_;
    Json decision_, before_;
    Control* choices_screen_;
    Control* description_;
    Node* lighting_;
    std::vector<Control*> character_boxes_;
    std::vector<Node*> character_spines_;
    std::vector<Option> options_;
    ProceedButton proceed_;
    int focus_, start_focus_;
    bool done_, passive_, chosen_;
    TweenSet tweens_;
    double t_, proceed_at_; /* -1: none */
    Node* smoke_;           /* NRestSmokeVfx, freed after 4 s */
};

} /* namespace spire */

#endif
