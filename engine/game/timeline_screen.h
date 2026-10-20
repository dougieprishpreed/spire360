/* The Timeline (NTimelineScreen, scenes/timeline_screen/timeline_screen.tscn)
 * from the main menu, from the logic's timeline answers.
 *
 * The epochs stand in era columns along the line (NEraColumn: the era's
 * icon, name and year; the slots stacked by era position), each slot as
 * NEpochSlot.SetState draws it: Complete (the portrait), Obtained (the
 * portrait dimmed, s 0, v 0.5, in chains, the outline pulsing purple: it
 * waits to be revealed) or NotObtained (the portrait desaturated, s 0.25;
 * the game blurs it with dark_blur, not ported). Slots pop up from 64 px
 * below in 0.5 s (Back Out) after up to 0.3 s. The pad moves between slots
 * (left / right between columns, up / down within one) with the slot's
 * hover tip (its title and how it is unlocked); the line pans to keep the
 * focused slot in view.
 *
 * A on an obtained slot reveals it (reveal_epoch): the inspect screen
 * (NEpochInspectScreen) shows its portrait, chapter, story text and what it
 * unlocks, the chains falling away; Continue then shows the unlock screens
 * its QueueUnlocks queued (cards, relics, potions, a character, text, the
 * timeline expanding with new slots), each fading in over 0.5 s and out to
 * black over 1 s on Confirm. A on a complete slot inspects it again. B goes
 * back. A profile that has not revealed Neow first sees the tutorial
 * (NTimelineTutorial); its Proceed opens Neow's slot. */
#ifndef SPIRE_TIMELINE_SCREEN_H
#define SPIRE_TIMELINE_SCREEN_H

#include "../core/json.h"
#include "../scene/resource.h"
#include "../scene/tween.h"
#include "run_screen.h"
#include "ui_util.h"

namespace spire {

class Control;
class SceneLibrary;

class TimelineScreen : public FrameListener {
public:
    enum Action { NONE, BACK };
    explicit TimelineScreen(SceneLibrary* lib);
    virtual ~TimelineScreen();
    Node* build(std::string* error);
    /* A timeline answer (the screen, a reveal, the tutorial's end). */
    void set_data(const Json& timeline);
    /* cmd: a command for the logic ("" for none). */
    Action input(const PadState& pad, std::string* cmd);
    virtual void frame(double dt);

private:
    enum Mode { LOADING, TUTORIAL, BROWSE, WAITING, INSPECT, UNLOCKS };
    struct Slot {
        std::string id, state;
        int era, position;
        Json data;
        Control* node;
        Control* image;
        Control* portrait;
        Control* chains;
        Control* outline;
        Control* reticle;
        bool pulsing;
    };
    struct Column {
        int era;
        Control* node;
    };
    Column* column_for(const Json& epoch, bool animated);
    void add_slot(const Json& epoch, bool animated, float delay);
    void set_slot_state(Slot& s, const std::string& state);
    void spawn_slot(Slot& s, float delay);
    void show_name_and_year(Column& c);
    void sync_slots(const Json& slots, bool animated);
    void focus(int i);
    int neighbour(int dx, int dy) const;
    void clear_tip();
    void open_inspect(const Json& epoch, bool revealed);
    void close_inspect();
    void next_unlock();
    void close_unlock();
    void refresh_reminder();
    /* A NotObtained slot's portrait: its SubViewport's blurred copy. */
    struct Blurred {
        DerivedImage recipe;
        ImageTextureRes image;
    };
    Texture2DRes* blurred_portrait(Slot& s, const std::string& path);
    std::map<std::string, Blurred*> blurred_;
    SceneLibrary* lib_;
    Node* root_;
    Control* slots_box_;  /* %EpochSlots */
    Control* line_;       /* %Line */
    Control* moved_;      /* %WhatsMoved: panned */
    Control* inspect_;    /* %EpochInspectScreen */
    Control* holder_;     /* %UnlockScreenHolder */
    Control* backstop_;   /* %SharedBackstop */
    Control* reminder_;   /* %EpochReminderText */
    Control* tutorial_;
    Control* tip_;
    Control* unlock_;     /* the unlock screen shown */
    std::vector<Slot> slots_;
    std::vector<Column> columns_;
    Json data_, queue_, after_;
    size_t queue_at_;
    Mode mode_;
    int focus_;
    float pan_, pan_target_;
    double t_, unlock_t_;
    bool closing_unlock_;
    bool tutorial_done_; /* the tutorial was acknowledged: its answer spawns Neow */
    TweenSet tweens_;
};

} /* namespace spire */

#endif
