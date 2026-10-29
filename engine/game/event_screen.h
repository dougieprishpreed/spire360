/* An event room (NEventRoom with its layout) from an event_choice decision.
 *
 * Default layout (NEventLayout): the event's portrait, its title and
 * description fading in, the options (NEventOptionButton) sliding in one
 * after another. Up / down move the focus (wrapping), A chooses.
 *
 * Ancient layout (NAncientEventLayout, Neow at a run's start): the
 * ancient's background scene, its name banner, the dialogue lines one at a
 * time (A: the next line), then the options under them.
 *
 * Combat layouts embed an encounter in VisualOnly mode. Custom layouts
 * (events with their own scenes) show the default layout for now. */
#ifndef SPIRE_EVENT_SCREEN_H
#define SPIRE_EVENT_SCREEN_H

#include "../scene/tween.h"
#include "run_screen.h"
#include "ui_util.h"

namespace spire {

class CanvasItem;
class Control;
class SceneLibrary;
class CombatScreen;

class EventScreen : public RunScreen, public FrameListener {
public:
    explicit EventScreen(SceneLibrary* lib);
    virtual ~EventScreen();
    virtual Node* build(const Json& decision, std::string* error);
    virtual std::string input(const PadState& pad);
    virtual bool own_ui_sounds() const { return true; }
    virtual void frame(double dt);
    void set_combat(CombatScreen* combat) { combat_ = combat; }
    /* Before build: the portrait an earlier page of the event put in place
     * (an event_portrait cue: Trial's accused), null for the event's own. */
    void set_room_portrait(const Json& cue) { room_portrait_ = cue; }
    /* The same event's next page (NEventRoom.RefreshEventState keeps the
     * layout): an ancient's banner as it settled, not its entrance. */
    void set_continued(bool on) { continued_ = on; }
    /* Transfer the visual-only room across event pages or into the fight. */
    CombatScreen* take_combat();
    /* What a choice did to the event's room, before the screen after it;
     * true while that plays. */
    void play_room_steps(const Json& steps);
    bool room_playing() const;

private:
    struct Option {
        Control* node;
        Control* outline;
        CanvasItem* image; /* hsv material: v 0.9, 1.2 focused */
        CanvasItem* kill_glow;
        int index;         /* the logic's option index */
        bool locked, will_kill;
        float v;           /* the hsv shader's v */
        float slide, slid; /* AnimateIn's x offset, and what is applied */
        Json tips;         /* Option.HoverTips */
    };
    Node* build_default(const Json& d);
    Node* build_ancient(const Json& d);
    Node* build_combat(const Json& d);
    void add_option(Node* box, const Json& o, int i, bool ancient);
    void focus(int i);
    void show_line(int i);
    void place_content(float h);
    void settle_banner(bool animate);

    SceneLibrary* lib_;
    CombatScreen* combat_;
    Json decision_;
    Json room_portrait_;
    std::vector<Option> options_;
    int focus_;
    bool enabled_; /* the options take the pad */
    float enable_at_;
    float time_;
    /* PunchOff.PunchEachOther: the constructs spar on the first page (the
     * left one mirrored), one exchange each 2.6 s. */
    bool sparring_;
    float spar_at_;
    /* a combat layout's options, hidden while the room plays the event's
     * steps (The Architect's attacks and pauses): NULL when none wait */
    CanvasItem* steps_options_;
    TweenSet tweens_;
    /* the focused option's tips: left of it, right of it in a combat
     * layout (NEventOptionButton.OnFocus) */
    TipFollower tips_;
    Node* root_;
    bool combat_layout_;

    /* Ancient layout. */
    bool ancient_;
    std::vector<Control*> lines_;
    int line_;
    bool laid_out_; /* a layout pass has measured the content */
    bool continued_;
    Control* content_container_;
    Control* content_;
    Control* options_box_;
    Control* next_button_;
    float content_y_;      /* Content's Position.y in its container */
    float container_h_;
    Control* banner_;
    CanvasItem* banner_title_;
    CanvasItem* banner_epithet_;
};

} /* namespace spire */

#endif
