/* NCrystalSphereScreen (scenes/events/custom/crystal_sphere) for the logic's
 * crystal_sphere decisions: the ball (scry_reveal), the items lying under
 * the fog (each NCrystalSphereItem's picture; its shine when its cells are
 * all clear), the fog (the ScryMask's cells fading out over half a second
 * as they clear, NCrystalSphereMask), the cell under the cursor and the
 * cells the tool would reveal lit (NCrystalSphereCell), the divination
 * buttons (LT small, RT big), the count left, the instructions and the
 * seer's lines (NCrystalSphereDialogue). The d-pad moves over the hidden
 * cells as the game's focus neighbours (skipping clear ones), A divines.
 * Each decision builds the screen again; set_previous animates what the
 * last divination cleared. */
#ifndef SPIRE_CRYSTAL_SPHERE_SCREEN_H
#define SPIRE_CRYSTAL_SPHERE_SCREEN_H

#include "../scene/tween.h"
#include "run_screen.h"
#include "ui_util.h"

namespace spire {

class Control;
class SceneLibrary;

class CrystalSphereScreen : public RunScreen, public FrameListener {
public:
    explicit CrystalSphereScreen(SceneLibrary* lib);
    /* The decision before (a crystal_sphere one: this is after a divination). */
    void set_previous(const Json& prev) { previous_ = prev; }
    virtual Node* build(const Json& decision, std::string* error);
    virtual std::string input(const PadState& pad);
    virtual void frame(double dt);

private:
    bool hidden(int x, int y) const;
    void focus(int x, int y);
    void refresh_highlight();
    void say(const char* kind);

    SceneLibrary* lib_;
    Json decision_, previous_;
    Node* screen_;
    Control* mask_;
    Node* dialogue_;
    int w_, h_;
    std::vector<Control*> cells_; /* x * h + y */
    int fx_, fy_;
    bool big_;
    double t_;
    double say_t_;                /* the seer's line's start (< 0 none) */
    std::vector<Control*> pops_;  /* items just revealed */
    std::vector<float> fade_from_, fade_start_; /* per cell (y * 11 + x); start < 0: none */
    TweenSet tweens_;
};

} /* namespace spire */

#endif
