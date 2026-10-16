#include "character_select.h"
#include "loc.h"

#include "../scene/controls.h"
#include "../scene/node.h"
#include "../scene/scene.h"

#include <math.h>

namespace spire {

/* ModelDb.AllCharacters' order, then the random button (shown: the
 * preview has every character unlocked). Used until the logic answers. */
static const char* const DEFAULT_IDS[] = { "IRONCLAD", "SILENT", "REGENT", "NECROBINDER", "DEFECT" };
static const char* RANDOM_ID = "RANDOM_CHARACTER";

static Control* as_control(Node* n) { return n && n->is_control() ? (Control*)n : NULL; }

CharacterSelectScreen::CharacterSelectScreen(SceneLibrary* lib)
    : lib_(lib), info_(NULL), bg_box_(NULL), focus_(-1), starting_(false), t_(0), info_left_(0), info_right_(0) {}

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
    hide(screen->find_unique("AscensionPanel"));
    hide(screen->get_node("ReadyAndWaitingPanel"));
    hide(screen->get_node("UnreadyButton"));
    hide(screen->find_unique("CharacterUnlockAnimationBackstop"));
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
        /* Init: CharacterModel.CharacterSelectIcon. */
        set_texture(lib_, bt.icon, "images/packed/character_select/char_select_" + lower(bt.id) + ".png");
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
    int first = 0;
    for (size_t i = 0; i < buttons_.size(); ++i)
        if (buttons_[i].id == selected) first = (int)i;
    select(first, false);
    return screen;
}

void CharacterSelectScreen::set_data(const Json& characters) {
    data_ = characters;
    refresh_info();
}

/* NCharacterSelectScreen.SelectCharacter's panel: the name, HP and gold,
 * description and starting relic (Random: question marks, no relic). */
void CharacterSelectScreen::refresh_info() {
    if (!info_ || focus_ < 0) return;
    const std::string& id = buttons_[(size_t)focus_].id;
    bool random = id == RANDOM_ID;
    const Json* d = data_for(id);
    std::string key = id + ".title";
    std::string title = d ? (*d)["title"].str() : Loc::get("characters", key.c_str());
    key = id + ".description";
    std::string desc = d ? (*d)["description"].str() : Loc::get("characters", key.c_str());
    if (random) {
        title = Loc::get("characters", "RANDOM_CHARACTER.title");
        desc = Loc::get("characters", "RANDOM_CHARACTER.description");
    }
    set_label(info_->get_node("VBoxContainer/Name"), title);
    set_label(info_->get_node("VBoxContainer/DescriptionLabel"), desc);
    std::string hp = "\?\?/\?\?", gold = "\?\?\?"; /* (no trigraphs) */
    if (d && !random) {
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
        std::string rid = lower(r["id"].str());
        set_texture(lib_, icon, "images/atlases/relic_atlas.sprites/" + rid + ".tres");
        set_texture(lib_, outline, "images/atlases/relic_outline_atlas.sprites/" + rid + ".tres");
        if (icon) icon->self_modulate = Color(1, 1, 1, 1);
        if (outline) outline->self_modulate = Color(0, 0, 0, 0.5f); /* halfTransparentBlack */
        set_label(relic->get_node("Name/RichTextLabel"), r["title"].str());
        set_label(relic->get_node("Description"), r["description"].str());
    } else {
        if (icon) icon->self_modulate = Color(0, 0, 0, 0);
        if (outline) outline->self_modulate = Color(0, 0, 0, 0);
        set_label(relic ? relic->get_node("Name/RichTextLabel") : NULL, std::string());
        set_label(relic ? relic->get_node("Description") : NULL, std::string());
    }
}

/* Focus enters a button: NCharacterSelectButton.Select (s 1, v 1.1, the
 * outline pulsing) and the screen's SelectCharacter (the background, the
 * info panel sliding in from 300 px left); the others deselect. */
void CharacterSelectScreen::select(int i, bool animate) {
    if (i < 0 || i >= (int)buttons_.size() || i == focus_) return;
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
    /* The background: CharacterModel.CharacterSelectBg. */
    if (bg_box_) {
        while (!bg_box_->children.empty()) {
            Node* c = bg_box_->children.back();
            bg_box_->remove_child(c);
            delete c;
        }
        Node* bg = instance(lib_, "scenes/screens/char_select/char_select_bg_" + lower(b.id) + ".tscn");
        if (bg) bg_box_->add_child(bg);
    }
    refresh_info();
    if (info_ && animate) {
        tweens_.tween_from(&info_->offset[SIDE_LEFT], info_left_ - 300, info_left_, 0.5f, TRANS_EXPO, EASE_OUT);
        tweens_.tween_from(&info_->offset[SIDE_RIGHT], info_right_ - 300, info_right_, 0.5f, TRANS_EXPO, EASE_OUT);
    }
}

CharacterSelectScreen::Action CharacterSelectScreen::input(const PadState& pad) {
    if (starting_ || buttons_.empty()) return NONE;
    int n = (int)buttons_.size();
    /* Focus neighbours wrap left / right. */
    if (pad.pressed & PadState::LEFT) select((focus_ + n - 1) % n, true);
    if (pad.pressed & PadState::RIGHT) select((focus_ + 1) % n, true);
    if (pad.pressed & PadState::B) return BACK;
    if (pad.pressed & (PadState::A | PadState::START)) {
        embark_id_ = buttons_[(size_t)focus_].id;
        if (embark_id_ == RANDOM_ID) {
            /* RandomCharacter: one of the others, chosen now. */
            int k = n > 1 ? (int)(chaotic_rand() % (unsigned)(n - 1)) : 0;
            embark_id_ = buttons_[(size_t)k].id;
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
    /* _Process: the selected button's outline pulses (0.35..1, 1.6 pi/s). */
    if (focus_ >= 0 && buttons_[(size_t)focus_].outline) {
        float k = (cosf((float)t_ * 1.6f * 3.14159265f) + 1.0f) * 0.5f;
        buttons_[(size_t)focus_].outline->modulate.a = 0.35f + (1.0f - 0.35f) * k;
    }
}

} /* namespace spire */
