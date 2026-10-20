/* The character select screen (NCharacterSelectScreen, scenes/screens/
 * character_select_screen.tscn) as a singleplayer run opens it from the
 * main menu: the characters' buttons along the bottom (left / right moves
 * the focus, which selects, as NCharacterSelectButton does), the selected
 * one's animated background and info panel (name, HP, gold, description,
 * starting relic), A (the embark button's hotkey, ui_select) starts the
 * run, B goes back to the menu.
 *
 * The info comes from the logic's "characters" answer (set_data); until it
 * arrives the panel shows the localized names and descriptions only.
 *
 * The profile (set_progress, the logic's main_menu answer) locks the
 * characters it has not unlocked (NCharacterSelectButton.Init: the locked
 * icon and the lock; focus shows "Locked" and how to unlock them; they
 * cannot be selected) and gives the ascension panel (NAscensionPanel) its
 * range: shown when the character has one, LB / RB (the tab hotkeys) step
 * the level, the arrows show while there is a level that way. */
#ifndef SPIRE_CHARACTER_SELECT_H
#define SPIRE_CHARACTER_SELECT_H

#include "../core/json.h"
#include "../scene/tween.h"
#include "run_screen.h"
#include "ui_util.h"

namespace spire {

class Control;
class SceneLibrary;

class CharacterSelectScreen : public FrameListener {
public:
    enum Action { NONE, BACK, EMBARK };
    explicit CharacterSelectScreen(SceneLibrary* lib);
    /* A new tree (the caller owns it); `selected`: the character id to
     * select first ("" for the first). NULL without the scene. */
    Node* build(const Json& characters, const std::string& selected, std::string* error);
    /* The logic's characters arrived (or changed). */
    void set_data(const Json& characters);
    /* The profile's characters and ascensions (main_menu answer). */
    void set_progress(const Json& menu);
    /* The ascension to embark at. */
    int ascension() const { return ascension_; }
    Action input(const PadState& pad);
    /* The character to start the run with (Random resolved). */
    std::string character() const { return embark_id_; }
    /* The selected button's id (Random: "RANDOM_CHARACTER"). */
    std::string selected() const;
    /* After A: the buttons disabled while the run starts. */
    void set_starting();
    virtual void frame(double dt);
    /* Its tree was deleted (back to the menu, or a run started): forget
     * the nodes; build() makes a new one. */
    void detach();

private:
    struct Button {
        Control* node;
        Control* icon;    /* hsv s / v */
        Control* outline; /* %OutlineLocal */
        Control* lock;    /* %Lock */
        std::string id;   /* CHARACTER id, or RANDOM_CHARACTER */
        bool locked;
    };
    void select(int i, bool animate);
    void refresh_info();
    void refresh_locks();
    const Json* progress_for(const std::string& id) const;
    void set_ascension_range(int max, int level);
    void set_ascension(int level);
    const Json* data_for(const std::string& id) const;
    SceneLibrary* lib_;
    Json data_;
    Json progress_;
    Control* info_;
    Control* panel_; /* %AscensionPanel */
    Control* tip_;   /* the locked button's hover tip */
    int ascension_, max_ascension_;
    Node* bg_box_;
    std::vector<Button> buttons_;
    int focus_;
    bool starting_;
    double t_;
    std::string embark_id_;
    float info_left_, info_right_;
    TweenSet tweens_;
};

} /* namespace spire */

#endif
