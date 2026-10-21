/* NBestiary (scenes/screens/bestiary/bestiary.tscn) from the compendium,
 * with the logic's bestiary answers: the sidebar's acts and events (label
 * dividers) with their monsters (unseen ones greyed and skipped), the
 * selection arrow beside the chosen one, the monster in its layout (the
 * default one, the Kaiser Crab's arms, the Decimillipede's segments), its
 * name, and either its actions (move buttons: LB RB RT LT X, then the right
 * stick) or its statistics (the character filters, LB / RB between the
 * unlocked ones; the character's quote in its dialogue bubble). Y switches
 * between the two (NBestiaryModeButton), up / down pick a monster in the
 * list, A selects it, B goes back. An action the logic plays
 * (bestiary_move) comes back as cues: sounds, animation triggers, effects. */
#ifndef SPIRE_BESTIARY_SCREEN_H
#define SPIRE_BESTIARY_SCREEN_H

#include "../scene/tween.h"
#include "combat_vfx.h"
#include "run_screen.h"
#include "scroll_area.h"
#include "ui_util.h"

namespace spire {

class Control;
class SceneLibrary;

class BestiaryScreen : public FrameListener, private VfxAnchors {
public:
    enum Action { NONE, BACK };
    explicit BestiaryScreen(SceneLibrary* lib);
    Node* build(std::string* error);
    static std::string query() { return "{\"cmd\":\"bestiary\"}"; }
    static bool is_answer(const std::string& type) {
        return type == "bestiary" || type == "bestiary_entry" || type == "bestiary_move";
    }
    void set_data(const Json& msg);
    Action input(const PadState& pad, std::string* cmd);
    virtual void frame(double dt);

private:
    struct Entry {
        Control* node;
        Control* label;
        const Json* data;
    };
    struct Filter {
        Control* node;
        Control* image;
        bool locked;
    };
    struct Shown {
        Control* node;  /* the NCreature */
        Node* visuals;
        Json view;      /* bestiary_entry's monster */
        std::string who; /* how the last move's cues name it */
        std::string slot; /* the Decimillipede's: its encounter slot */
    };
    struct Cue {
        double at;
        Json data;
    };
    void fill_list();
    void focus(int i);
    void select(int i);
    void show_entry(const Json& msg);
    void clear_layout();
    void make_moves();
    void play_move(int i, std::string* cmd);
    void play_cues(const Json& steps, const std::string& who);
    void fire(const Json& cue);
    void set_mode(bool stats, bool animate);
    void select_filter(int i);
    void step_filter(int dir);
    void refresh_stats();
    Shown* shown_for(const std::string& who);

    /* VfxAnchors */
    virtual bool creature(const std::string& who, VfxCreature* out);
    virtual bool side_center(bool enemies, Vec2* out);
    virtual Node* back_box();
    virtual Node* take_visuals(const std::string& who);
    virtual Node* visuals_of(const std::string& who);
    virtual bool origin_of(const std::string& who, Vec2* out);

    SceneLibrary* lib_;
    Node* root_;
    Node* screen_;
    Json data_, entry_, move_data_;
    std::vector<Entry> entries_;
    std::vector<Filter> filters_;
    std::vector<Shown> shown_;
    std::vector<Control*> moves_;
    std::vector<Cue> cues_;
    Control* layout_;
    Control* arrow_;
    std::string layout_kind_;
    std::string anims_;   /* the selected creature's skeleton's revive / hurt / die, as a JSON list */
    std::string pending_; /* a query for the next input() */
    int focus_, selected_, filter_, arrow_target_;
    bool stats_mode_, arrow_placed_;
    double t_, cue_t_;
    size_t next_cue_;
    ScrollArea scroll_;
    TweenSet tweens_;
};

} /* namespace spire */

#endif
