/* The treasure room (NTreasureRoom, scenes/rooms/treasure_room.tscn) from a
 * treasure decision: the act's chest (NTreasureButton: its Spine skeleton,
 * the outline skin while focused); A opens it: the banner ("What's
 * Inside?"), the gold, the chest's opening animation, its relic in the
 * holder (NTreasureRoomRelicHolder, with its hover tip). A takes the relic;
 * Y (Skip, after 2.5 s) leaves it. Then the relic goes, the chest comes
 * back and Y (Proceed) goes to the map. */
#ifndef SPIRE_TREASURE_SCREEN_H
#define SPIRE_TREASURE_SCREEN_H

#include "../scene/tween.h"
#include "proceed_button.h"
#include "run_screen.h"
#include "ui_util.h"

namespace spire {

class Control;
class SceneLibrary;
class SpineSprite;

class TreasureScreen : public RunScreen, public FrameListener {
public:
    explicit TreasureScreen(SceneLibrary* lib);
    /* Before build: the stage the room was at before this decision ("" for
     * none): the change between them is animated. */
    void set_previous_stage(const std::string& stage) { previous_ = stage; }
    virtual Node* build(const Json& decision, std::string* error);
    virtual std::string input(const PadState& pad);
    virtual void frame(double dt);

private:
    void setup_chest(const Json& chest, bool opened, bool opening);
    void show_relic(const Json& relic, bool animate);
    SceneLibrary* lib_;
    Json decision_;
    std::string stage_, previous_;
    Control* chest_;
    SpineSprite* chest_visual_;
    Control* holder_;
    Control* relic_;
    Control* tip_;
    ProceedButton proceed_;
    double t_, skip_at_;
    bool sent_;
    TweenSet tweens_;
};

} /* namespace spire */

#endif
