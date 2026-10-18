#include "title_screen.h"
#include "../audio/game_audio.h"
#include "loc.h"

#include "../scene/controls.h"
#include "../scene/node.h"
#include "../scene/scene.h"
#include "../scene/text_nodes.h"

namespace spire {

/* StsColors.cream / gold. */
static const unsigned CREAM = 0xFFF6E2, GOLD = 0xEFC851;

TitleScreen::TitleScreen(SceneLibrary* lib)
    : lib_(lib), reticle_l_(NULL), reticle_r_(NULL), focus_(-1), covered_(false) {}

Node* TitleScreen::build(const std::string& version, std::string* error) {
    buttons_.clear();
    labels_.clear();
    tweens_.clear();
    focus_ = -1;
    Node* menu = instance(lib_, "scenes/screens/main_menu.tscn");
    if (!menu) {
        if (error) *error = "no main_menu.tscn";
        return NULL;
    }
    Node* box = menu->get_node("MainMenuTextButtons");
    static const char* const hidden[] = { "ContinueButton", "AbandonRunButton", "MultiplayerButton", "TimelineButton",
                                          "SettingsButton", "CompendiumButton" };
    for (size_t i = 0; i < sizeof(hidden) / sizeof(hidden[0]); ++i) hide(box ? box->get_node(hidden[i]) : NULL);
    hide(menu->get_node("ChangeProfileButton"));
    hide(menu->get_node("PatchNotesButton"));
    hide(menu->get_node("ModdedWarning"));
    hide(menu->get_node("ModWarningContainer"));
    hide(menu->get_node("PatchNotesScreen"));
    hide(menu->get_node("BlurBackstop"));
    hide(menu->get_node("Fps"));
    set_label(menu->get_node("ReleaseInfo"), version.empty() ? std::string("Spire360 preview")
                                                             : "Spire360 preview\n" + version);
    static const struct {
        const char* node;
        const char* key;
    } shown[] = { { "SingleplayerButton", "SINGLE_PLAYER" }, { "QuitButton", "QUIT" } };
    for (size_t i = 0; i < sizeof(shown) / sizeof(shown[0]); ++i) {
        Node* b = box ? box->get_node(shown[i].node) : NULL;
        if (!b || !b->is_control()) continue;
        Node* l = b->get_node("Label");
        set_label(l, Loc::get("main_menu_ui", shown[i].key));
        buttons_.push_back((Control*)b);
        labels_.push_back(l && l->is_control() ? (Control*)l : NULL);
    }
    Node* rl = menu->find_unique("ButtonReticleLeft");
    Node* rr = menu->find_unique("ButtonReticleRight");
    reticle_l_ = rl && rl->is_control() ? (Control*)rl : NULL;
    reticle_r_ = rr && rr->is_control() ? (Control*)rr : NULL;
    menu->add_child(frame_driver(this));
    covered_ = false;
    return menu;
}

/* NMainMenuTextButton.OnFocus (gold, 1.05) and NMainMenu's carets. */
void TitleScreen::focus(int i) {
    if (i < 0 || i >= (int)buttons_.size() || i == focus_) return;
    if (lib_->audio() && focus_ >= 0) lib_->audio()->ui("event:/sfx/ui/clicks/ui_hover");
    if (focus_ >= 0 && labels_[(size_t)focus_]) {
        Control* old = labels_[(size_t)focus_];
        old->self_modulate = hex(CREAM);
        tweens_.tween(&old->scale, Vec2(1, 1), 0.5f, TRANS_EXPO, EASE_OUT);
    }
    focus_ = i;
    Control* l = labels_[(size_t)i];
    if (!l) return;
    l->pivot_offset = l->size * 0.5f;
    tweens_.tween(&l->scale, Vec2(1.05f, 1.05f), 0.05f);
    l->self_modulate = hex(GOLD);
    /* The carets beside the label, sliding out 28 px (Back Out). */
    Control* b = buttons_[(size_t)i];
    Vec2 bpos = b->global_transform().xform(Vec2(0, 0)), lpos = l->global_transform().xform(Vec2(0, 0));
    float left = lpos.x - 26.0f, right = lpos.x + l->size.x - 14.0f;
    Control* carets[2] = { reticle_l_, reticle_r_ };
    float from[2] = { left, right }, to[2] = { left - 28.0f, right + 28.0f };
    for (int k = 0; k < 2; ++k) {
        Control* c = carets[k];
        if (!c) continue;
        float w = c->offset[SIDE_RIGHT] - c->offset[SIDE_LEFT], h = c->offset[SIDE_BOTTOM] - c->offset[SIDE_TOP];
        c->offset[SIDE_TOP] = bpos.y + 5.0f;
        c->offset[SIDE_BOTTOM] = bpos.y + 5.0f + h;
        tweens_.tween_from(&c->offset[SIDE_LEFT], from[k], to[k], 0.25f, TRANS_BACK, EASE_OUT);
        tweens_.tween_from(&c->offset[SIDE_RIGHT], from[k] + w, to[k] + w, 0.25f, TRANS_BACK, EASE_OUT);
        c->modulate = hex(GOLD);
    }
}

void TitleScreen::set_covered(bool covered) {
    covered_ = covered;
    if (covered) {
        tweens_.clear();
        focus_ = -1; /* focused again (carets back) when uncovered */
    }
    if (reticle_l_) reticle_l_->modulate.a = covered ? 0.0f : 1.0f;
    if (reticle_r_) reticle_r_->modulate.a = covered ? 0.0f : 1.0f;
}

TitleScreen::Action TitleScreen::input(const PadState& pad) {
    if (covered_) return NONE;
    if (buttons_.empty()) return (pad.pressed & (PadState::A | PadState::START)) ? SINGLEPLAYER : NONE;
    if (focus_ < 0) {
        focus(0);
        return NONE;
    }
    int n = (int)buttons_.size();
    if (pad.pressed & PadState::UP) focus((focus_ + n - 1) % n);
    if (pad.pressed & PadState::DOWN) focus((focus_ + 1) % n);
    if (pad.pressed & (PadState::A | PadState::START)) {
        if (lib_->audio()) lib_->audio()->ui("event:/sfx/ui/clicks/ui_click");
        return focus_ == 0 ? SINGLEPLAYER : QUIT;
    }
    return NONE;
}

void TitleScreen::frame(double dt) { tweens_.update(dt); }

} /* namespace spire */
