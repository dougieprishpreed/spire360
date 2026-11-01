#include "character_select.h"
#include "top_bar.h"
#include "../audio/game_audio.h"
#include "loc.h"
#include "screen_shake.h"

#include "../scene/controls.h"
#include "../scene/node.h"
#include "../scene/particles.h"
#include "../scene/scene.h"

#include <math.h>

namespace spire {

/* ModelDb.AllCharacters' order, then the random button (shown: the
 * preview has every character unlocked). Used until the logic answers. */
static const char* const DEFAULT_IDS[] = { "IRONCLAD", "SILENT", "REGENT", "NECROBINDER", "DEFECT" };
static const char* RANDOM_ID = "RANDOM_CHARACTER";

static Control* as_control(Node* n) { return n && n->is_control() ? (Control*)n : NULL; }

CharacterSelectScreen::CharacterSelectScreen(SceneLibrary* lib)
    : lib_(lib), info_(NULL), panel_(NULL), tip_(NULL), ascension_(0), max_ascension_(0), bg_box_(NULL), focus_(-1),
      starting_(false), t_(0), info_left_(0), info_right_(0), backstop_(NULL), unlock_i_(-1), unlock_t_(-1000),
      unlock_played_(false), unlock_charged_(false), unlock_done_(false) {}

const Json* CharacterSelectScreen::progress_for(const std::string& id) const {
    const Json& cs = progress_["characters"];
    for (size_t i = 0; i < cs.size(); ++i)
        if (cs[i]["id"].str() == id) return &cs[i];
    return NULL;
}

/* CharacterModel.GetUnlockText (Random's: the menu's own field) */
std::string CharacterSelectScreen::unlock_text(const std::string& id) const {
    if (id == RANDOM_ID) return progress_["random_unlock_text"].str();
    const Json* p = progress_for(id);
    return p ? (*p)["unlock_text"].str() : std::string();
}

const Json* CharacterSelectScreen::data_for(const std::string& id) const {
    const Json& cs = data_["characters"];
    for (size_t i = 0; i < cs.size(); ++i)
        if (cs[i]["id"].str() == id) return &cs[i];
    return NULL;
}

std::string CharacterSelectScreen::selected() const {
    return focus_ >= 0 && focus_ < (int)buttons_.size() ? buttons_[(size_t)focus_].id : std::string();
}

Node* CharacterSelectScreen::build(const Json& characters, const std::string& selected, std::string* error) {
    data_ = characters;
    buttons_.clear();
    tweens_.clear();
    focus_ = -1;
    starting_ = false;
    Node* screen = instance(lib_, "scenes/screens/character_select_screen.tscn");
    if (!screen) {
        if (error) *error = "no character_select_screen.tscn";
        return NULL;
    }
    /* Singleplayer: no lobby, act choice, ascension (none unlocked yet) or
     * unlock animation. */
    hide(screen->get_node("RemotePlayerContainer"));
    hide(screen->get_node("ActLabel"));
    hide(screen->find_unique("ActDropdown"));
    panel_ = ctl(screen, "AscensionPanel");
    hide(panel_);
    tip_ = NULL;
    ascension_ = max_ascension_ = 0;
    if (panel_) {
        show_glyph(lib_, ctl(panel_, "LeftTabIcon"), "lb");
        show_glyph(lib_, ctl(panel_, "RightTabIcon"), "rb");
    }
    hide(screen->get_node("ReadyAndWaitingPanel"));
    hide(screen->get_node("UnreadyButton"));
    backstop_ = ctl(screen, "CharacterUnlockAnimationBackstop");
    hide(backstop_);
    unlock_t_ = -1000;
    unlock_played_ = false;
    info_ = as_control(screen->get_node("InfoPanel"));
    if (info_) {
        info_left_ = info_->offset[SIDE_LEFT];
        info_right_ = info_->offset[SIDE_RIGHT];
    }
    bg_box_ = screen->get_node("AnimatedBg");
    show_glyph(lib_, find_named(screen->get_node("BackButton"), "HotkeyIcon"), "b");
    /* OverrideHotkeys(MegaInput.select): A embarks. */
    show_glyph(lib_, find_named(screen->get_node("ConfirmButton"), "HotkeyIcon"), "a");

    std::vector<std::string> ids;
    const Json& cs = data_["characters"];
    for (size_t i = 0; i < cs.size(); ++i) ids.push_back(cs[i]["id"].str());
    if (ids.empty())
        for (size_t i = 0; i < sizeof(DEFAULT_IDS) / sizeof(DEFAULT_IDS[0]); ++i) ids.push_back(DEFAULT_IDS[i]);
    ids.push_back(RANDOM_ID);
    Node* box = screen->get_node("CharSelectButtons/ButtonContainer");
    for (size_t i = 0; box && i < ids.size(); ++i) {
        Node* b = instance(lib_, "scenes/screens/char_select/char_select_button.tscn");
        if (!b || !b->is_control()) {
            delete b;
            break;
        }
        Button bt;
        bt.node = (Control*)b;
        bt.id = ids[i];
        bt.icon = ctl(b, "Icon");
        bt.outline = ctl(b, "OutlineLocal");
        bt.lock = ctl(b, "Lock");
        bt.locked = false;
        /* Init: CharacterModel.CharacterSelectIcon. */
        set_texture(lib_, bt.icon, "images/packed/character_select/char_select_" +
                                       (bt.id == RANDOM_ID ? std::string("random") : lower(bt.id)) + ".png");
        /* _Ready: s 0.2, v 0.4 until a state is set; NotSelected: 0.2 / 0.8. */
        if (bt.icon) {
            bt.icon->shader_params["s"] = 0.2f;
            bt.icon->shader_params["v"] = 0.8f;
        }
        bt.node->pivot_offset = Vec2(50, 74);
        box->add_child(b);
        buttons_.push_back(bt);
    }
    screen->add_child(frame_driver(this));
    /* NMainMenu is the screen shake's target (the selections' shakes) */
    if (screen->is_control()) screen->add_child(make_shake_driver((Control*)screen));
    refresh_locks();
    int first = 0;
    for (size_t i = 0; i < buttons_.size(); ++i)
        if (buttons_[i].id == selected && !buttons_[i].locked) first = (int)i;
    select(first, false);
    return screen;
}

void CharacterSelectScreen::detach() {
    buttons_.clear();
    info_ = panel_ = tip_ = NULL;
    bg_box_ = NULL;
    focus_ = -1;
    tweens_.clear();
}

void CharacterSelectScreen::set_progress(const Json& menu) {
    progress_ = menu;
    refresh_locks();
    int i = focus_;
    focus_ = -1;
    if (i >= 0) select(i, false, false);
    /* a character the timeline just unlocked: its animation, once */
    if (!unlock_played_ && menu["pending_unlock"].is_string())
        for (size_t k = 0; k < buttons_.size(); ++k)
            if (buttons_[k].id == menu["pending_unlock"].str()) start_unlock((int)k);
}

std::vector<std::string> CharacterSelectScreen::shown_paths(const Json& characters, const std::string& id) {
    std::vector<std::string> out;
    out.push_back("scenes/screens/char_select/char_select_bg_" + lower(id) + ".tscn");
    out.push_back("images/packed/character_select/char_select_" + lower(id) + ".png");
    const Json& cs = characters["characters"];
    for (size_t i = 0; i < cs.size(); ++i) {
        if (cs[i]["id"].str() != id || !cs[i]["relic"].is_object()) continue;
        std::string rid = lower(relic_icon_base(cs[i]["relic"]));
        out.push_back("images/atlases/relic_atlas.sprites/" + rid + ".tres");
        out.push_back("images/atlases/relic_outline_atlas.sprites/" + rid + ".tres");
    }
    return out;
}

/* LockForAnimation: the locked icon and the lock (white) over the button
 * raised; the info panel hidden behind the backstop; 0.3 s, then
 * AnimateUnlock. */
void CharacterSelectScreen::start_unlock(int i) {
    Button& b = buttons_[(size_t)i];
    unlock_played_ = true;
    unlock_i_ = i;
    unlock_t_ = -0.3;
    unlock_charged_ = unlock_done_ = false;
    set_texture(lib_, b.icon, "images/packed/character_select/char_select_" + lower(b.id) + "_locked.png");
    if (b.lock) {
        b.lock->visible = true;
        b.lock->modulate = Color(1, 1, 1, 1);
        lock_origin_ = offset_pos(b.lock);
    }
    b.node->z_index = 1;
    set_visible(backstop_, true);
    set_visible(info_, false);
}

/* AnimateUnlock: the charge (UnlockChargeParticles, character_unlock_charge
 * .mp3; for 1 s the lock jitters a random way by 1..5 px, Quad Out); then
 * character_unlock.mp3, the lock unlocked, the icon the character's with
 * %IconAdd growing to 1.5 (1 s) as it fades (Expo In), the lock fading
 * after 0.5 s (0.5 s), UnlockParticles; the button selected, the panel back,
 * PendingCharacterUnlock cleared. */
void CharacterSelectScreen::unlock_frame(double dt) {
    if (unlock_t_ < -100 || unlock_i_ < 0 || unlock_i_ >= (int)buttons_.size()) return;
    Button& b = buttons_[(size_t)unlock_i_];
    unlock_t_ += dt;
    if (unlock_t_ < 0) return;
    if (!unlock_charged_) {
        unlock_charged_ = true;
        Node* charge = b.node->find_unique("UnlockChargeParticles");
        if (charge && (charge->type == "GPUParticles2D" || charge->type == "CPUParticles2D")) ((Particles2D*)charge)->emitting = true;
        if (lib_->audio()) lib_->audio()->ui("character_unlock_charge.mp3");
    }
    if (unlock_t_ < 1.0) {
        float k = (float)unlock_t_;
        float amp = 1.0f + 4.0f * (1.0f - (1.0f - k) * (1.0f - k));
        float a = (float)(chaotic_rand() % 6283) / 1000.0f;
        if (b.lock) b.lock->set_position(lock_origin_ + Vec2(cosf(a), sinf(a)) * amp);
        return;
    }
    if (unlock_done_) return;
    unlock_done_ = true;
    if (lib_->audio()) lib_->audio()->ui("character_unlock.mp3");
    if (b.lock) {
        b.lock->set_position(lock_origin_);
        set_texture(lib_, b.lock, "images/packed/character_select/char_select_lock3_unlocked.png");
        tweens_.tween(&b.lock->modulate.a, 0.0f, 0.5f, TRANS_LINEAR, EASE_IN_OUT, 0.5f);
    }
    std::string icon = "images/packed/character_select/char_select_" + lower(b.id) + ".png";
    set_texture(lib_, b.icon, icon);
    Control* add = ctl(b.node, "IconAdd");
    if (add) {
        set_texture(lib_, add, icon);
        add->visible = true;
        add->modulate.a = 1;
        add->pivot_offset = offset_size(add) * 0.5f;
        tweens_.tween(&add->scale, Vec2(1.5f, 1.5f), 1.0f);
        tweens_.tween(&add->modulate.a, 0.0f, 1.0f, TRANS_EXPO, EASE_IN);
    }
    Node* burst = b.node->find_unique("UnlockParticles");
    if (burst && (burst->type == "GPUParticles2D" || burst->type == "CPUParticles2D")) ((Particles2D*)burst)->restart();
    Node* charge = b.node->find_unique("UnlockChargeParticles");
    if (charge && (charge->type == "GPUParticles2D" || charge->type == "CPUParticles2D")) ((Particles2D*)charge)->emitting = false;
    b.node->z_index = 0;
    b.locked = false;
    select(unlock_i_, true);
    set_visible(backstop_, false);
    set_visible(info_, true);
    unlock_t_ = -1000;
    command_ = "{\"cmd\":\"clear_pending_unlock\"}";
}

/* NCharacterSelectButton.Init: locked buttons (Random: while any is). */
void CharacterSelectScreen::refresh_locks() {
    if (progress_.is_null()) return;
    bool any_locked = false;
    for (size_t i = 0; i < buttons_.size(); ++i) {
        Button& b = buttons_[i];
        if (b.id == RANDOM_ID) continue;
        const Json* p = progress_for(b.id);
        b.locked = p && !(*p)["unlocked"].b(true);
        any_locked = any_locked || b.locked;
    }
    for (size_t i = 0; i < buttons_.size(); ++i) {
        Button& b = buttons_[i];
        if (b.id == RANDOM_ID) b.locked = any_locked;
        set_texture(lib_, b.icon, "images/packed/character_select/char_select_" +
                                      (b.id == RANDOM_ID ? std::string("random") : lower(b.id)) +
                                      (b.locked ? "_locked.png" : ".png"));
        set_visible(b.lock, b.locked);
    }
}

/* NAscensionPanel.SetMaxAscension / SetAscensionLevel. */
void CharacterSelectScreen::set_ascension_range(int max, int level) {
    max_ascension_ = max;
    set_visible(panel_, max > 0);
    set_ascension(level < 0 ? 0 : level > max ? max : level);
}

void CharacterSelectScreen::set_ascension(int level) {
    ascension_ = level;
    if (!panel_) return;
    /* RefreshAscensionText, RefreshArrowVisibility. */
    Node* icon = ctl(panel_, "AscensionIcon");
    set_label(icon ? icon->get_node("AscensionLevel") : NULL, format("%d", level));
    const Json& a = progress_["ascensions"][(size_t)level];
    set_label(panel_->get_node("HBoxContainer/AscensionDescription/Description"),
              "[b][gold]" + a["title"].str() + "[/gold][/b]\n" + a["description"].str());
    set_visible(panel_->get_node("HBoxContainer/LeftArrowContainer/LeftArrow"), level != 0);
    set_visible(panel_->get_node("HBoxContainer/RightArrowContainer/RightArrow"), level != max_ascension_);
}

void CharacterSelectScreen::set_data(const Json& characters) {
    data_ = characters;
    refresh_info();
}

/* NCharacterSelectScreen.SelectCharacter's panel: the name, HP and gold,
 * description and starting relic (Random: question marks, no relic; a
 * locked character: "Locked", how to unlock it, its relic darkened). */
void CharacterSelectScreen::refresh_info() {
    if (!info_ || focus_ < 0) return;
    const std::string& id = buttons_[(size_t)focus_].id;
    bool random = id == RANDOM_ID;
    bool locked = buttons_[(size_t)focus_].locked;
    const Json* d = data_for(id);
    std::string key = id + ".title";
    std::string title = d ? (*d)["title"].str() : Loc::get("characters", key.c_str());
    key = id + ".description";
    std::string desc = d ? (*d)["description"].str() : Loc::get("characters", key.c_str());
    if (random) {
        title = Loc::get("characters", "RANDOM_CHARACTER.title");
        desc = Loc::get("characters", "RANDOM_CHARACTER.description");
    }
    if (locked) {
        title = Loc::get("main_menu_ui", "CHARACTER_SELECT.locked.title");
        desc = unlock_text(id);
    }
    set_label(info_->get_node("VBoxContainer/Name"), title);
    set_label(info_->get_node("VBoxContainer/DescriptionLabel"), desc);
    std::string hp = "\?\?/\?\?", gold = "\?\?\?"; /* (no trigraphs) */
    if (d && !random && !locked) {
        hp = format("%d/%d", (*d)["hp"].i(), (*d)["hp"].i());
        gold = format("%d", (*d)["gold"].i());
    }
    set_label(info_->get_node("VBoxContainer/HpGoldSpacer/HpGold/Hp/Label"), hp);
    set_label(info_->get_node("VBoxContainer/HpGoldSpacer/HpGold/Gold/Label"), gold);
    Node* relic = info_->get_node("VBoxContainer/Relic");
    Control* icon = as_control(relic ? relic->get_node("Icon") : NULL);
    Control* outline = as_control(icon ? icon->get_node("Outline") : NULL);
    const Json& r = d ? (*d)["relic"] : Json();
    if (!random && r.is_object()) {
        std::string rid = lower(relic_icon_base(r));
        set_texture(lib_, icon, "images/atlases/relic_atlas.sprites/" + rid + ".tres");
        set_texture(lib_, outline, "images/atlases/relic_outline_atlas.sprites/" + rid + ".tres");
        /* locked: ninetyPercentBlack over halfTransparentWhite */
        if (icon) icon->self_modulate = locked ? Color(0, 0, 0, 0.9f) : Color(1, 1, 1, 1);
        if (outline) outline->self_modulate = locked ? Color(1, 1, 1, 0.5f) : Color(0, 0, 0, 0.5f); /* halfTransparentBlack */
        set_label(relic->get_node("Name/RichTextLabel"),
                  locked ? Loc::get("main_menu_ui", "CHARACTER_SELECT.lockedRelic.title") : r["title"].str());
        set_label(relic->get_node("Description"),
                  locked ? Loc::get("main_menu_ui", "CHARACTER_SELECT.lockedRelic.description") : r["description"].str());
    } else {
        if (icon) icon->self_modulate = Color(0, 0, 0, 0);
        if (outline) outline->self_modulate = Color(0, 0, 0, 0);
        set_label(relic ? relic->get_node("Name/RichTextLabel") : NULL, std::string());
        set_label(relic ? relic->get_node("Description") : NULL, std::string());
    }
}

/* Focus enters a button: NCharacterSelectButton.Select (s 1, v 1.1, the
 * outline pulsing) and the screen's SelectCharacter (the character's
 * sound, a weak shake, the background, the info panel sliding in from
 * 300 px left; a locked one: no background, its unlock text); the others
 * deselect. */
void CharacterSelectScreen::select(int i, bool animate, bool feedback) {
    if (i < 0 || i >= (int)buttons_.size() || i == focus_) return;
    if (tip_) {
        if (tip_->parent) tip_->parent->remove_child(tip_);
        delete tip_;
        tip_ = NULL;
    }
    if (focus_ >= 0) {
        Button& o = buttons_[(size_t)focus_];
        if (o.icon) {
            tweens_.tween(&o.icon->shader_params["s"], 0.2f, 0.5f, TRANS_EXPO, EASE_OUT);
            tweens_.tween(&o.icon->shader_params["v"], 0.8f, 0.5f, TRANS_EXPO, EASE_OUT);
        }
        if (o.outline) o.outline->visible = false;
        tweens_.tween(&o.node->scale, Vec2(1, 1), 0.5f, TRANS_EXPO, EASE_OUT);
    }
    focus_ = i;
    Button& b = buttons_[(size_t)i];
    if (b.icon) {
        tweens_.kill(&b.icon->shader_params["s"]);
        tweens_.kill(&b.icon->shader_params["v"]);
        if (animate) {
            tweens_.tween(&b.icon->shader_params["s"], 1.0f, 0.5f, TRANS_EXPO, EASE_OUT);
            tweens_.tween(&b.icon->shader_params["v"], 1.1f, 0.5f, TRANS_EXPO, EASE_OUT);
        } else {
            b.icon->shader_params["s"] = 1.0f;
            b.icon->shader_params["v"] = 1.1f;
        }
    }
    if (b.outline) b.outline->visible = true;
    /* OnFocus: the hover scale (1.1) while focused. */
    tweens_.kill(&b.node->scale);
    b.node->scale = Vec2(1.1f, 1.1f);
    if (feedback) {
        const Json* d = data_for(b.id);
        if (b.id != RANDOM_ID && d && lib_->audio()) lib_->audio()->ui((*d)["select_sfx"].str());
        ScreenShake::get().shake("Weak", "Short", 90.0f);
    }
    /* The background: CharacterModel.CharacterSelectBg (none while locked). */
    if (bg_box_) {
        while (!bg_box_->children.empty()) {
            Node* c = bg_box_->children.back();
            bg_box_->remove_child(c);
            delete c;
        }
        Node* bg = b.locked ? NULL : instance(lib_, "scenes/screens/char_select/char_select_bg_" + lower(b.id) + ".tscn");
        if (bg) bg_box_->add_child(bg);
    }
    refresh_info();
    if (info_ && animate) {
        tweens_.tween_from(&info_->offset[SIDE_LEFT], info_left_ - 300, info_left_, 0.5f, TRANS_EXPO, EASE_OUT);
        tweens_.tween_from(&info_->offset[SIDE_RIGHT], info_right_ - 300, info_right_, 0.5f, TRANS_EXPO, EASE_OUT);
    }
    if (b.locked) {
        /* OnFocus while locked: "Locked" and how to unlock it, above the
         * button; no ascension (the panel hidden). */
        set_ascension_range(0, 0);
        tip_ = make_hover_tip(lib_, Loc::get("main_menu_ui", "CHARACTER_SELECT.locked.title"), unlock_text(b.id));
        if (tip_) {
            Vec2 at = b.node->global_transform().xform(Vec2(0, 0));
            tip_->set_position(at + Vec2(-90.0f, -180.0f));
            Node* root = b.node;
            while (root->parent) root = root->parent;
            root->add_child(tip_);
        }
        return;
    }
    /* SetSingleplayerAscensionAfterCharacterChanged: the character's
     * maximum and preferred level (Random: the highest of any). */
    if (!progress_.is_null()) {
        int max = 0, level = 0;
        if (b.id == RANDOM_ID) {
            const Json& cs = progress_["characters"];
            for (size_t k = 0; k < cs.size(); ++k)
                if (cs[k]["max_ascension"].i() > max) max = cs[k]["max_ascension"].i();
            level = ascension_;
        } else if (const Json* p = progress_for(b.id)) {
            max = (*p)["max_ascension"].i();
            level = (*p)["ascension"].i();
        }
        set_ascension_range(max, level);
    }
}

CharacterSelectScreen::Action CharacterSelectScreen::input(const PadState& pad) {
    if (starting_ || buttons_.empty() || unlock_t_ > -100) return NONE;
    int n = (int)buttons_.size();
    /* Focus neighbours wrap left / right; the button's OnFocus (its hover
     * sound) runs before the Select its FocusEntered also calls. */
    if ((pad.pressed & (PadState::LEFT | PadState::RIGHT)) && lib_->audio()) lib_->audio()->ui("event:/sfx/ui/clicks/ui_hover");
    if (pad.pressed & PadState::LEFT) select((focus_ + n - 1) % n, true);
    if (pad.pressed & PadState::RIGHT) select((focus_ + 1) % n, true);
    if (pad.pressed & PadState::B) {
        /* NBackButton.ClickedSfx */
        if (lib_->audio()) lib_->audio()->ui("event:/sfx/ui/clicks/ui_back");
        return BACK;
    }
    /* The ascension panel's tab hotkeys. */
    if (max_ascension_ > 0 && (pad.pressed & (PadState::LB | PadState::RB))) {
        int level = ascension_ + ((pad.pressed & PadState::RB) ? 1 : -1);
        if (level >= 0 && level <= max_ascension_) {
            set_ascension(level);
            if (lib_->audio()) lib_->audio()->ui("event:/sfx/ui/clicks/ui_click");
        }
    }
    if ((pad.pressed & (PadState::A | PadState::START)) && buttons_[(size_t)focus_].locked) return NONE;
    if (pad.pressed & (PadState::A | PadState::START)) {
        /* the embark button (NConfirmButton): NButton's ui_click */
        if (lib_->audio()) lib_->audio()->ui("event:/sfx/ui/clicks/ui_click");
        embark_id_ = buttons_[(size_t)focus_].id;
        if (embark_id_ == RANDOM_ID) {
            /* RandomCharacter: one of the others, chosen now. */
            int k = n > 1 ? (int)(chaotic_rand() % (unsigned)(n - 1)) : 0;
            embark_id_ = buttons_[(size_t)k].id;
            /* StartRunLobby.BeginRunLocally: within that character's
             * maximum. */
            if (const Json* p = progress_for(embark_id_))
                if (ascension_ > (*p)["max_ascension"].i()) ascension_ = (*p)["max_ascension"].i();
        }
        return EMBARK;
    }
    return NONE;
}

void CharacterSelectScreen::set_starting() {
    starting_ = true;
    for (size_t i = 0; i < buttons_.size(); ++i) buttons_[i].node->modulate.a = 0.5f;
}

void CharacterSelectScreen::frame(double dt) {
    tweens_.update(dt);
    t_ += dt;
    unlock_frame(dt);
    /* _Process: the selected button's outline pulses (0.35..1, 1.6 pi/s). */
    if (focus_ >= 0 && buttons_[(size_t)focus_].outline) {
        float k = (cosf((float)t_ * 1.6f * 3.14159265f) + 1.0f) * 0.5f;
        buttons_[(size_t)focus_].outline->modulate.a = 0.35f + (1.0f - 0.35f) * k;
    }
}

} /* namespace spire */
