#include "title_screen.h"
#include "../audio/game_audio.h"
#include "loc.h"

#include "../scene/controls.h"
#include "../scene/node.h"
#include "../scene/scene.h"
#include "../scene/text_nodes.h"
#include "screen_shake.h"

namespace spire {

/* StsColors.cream / gold. */
static const unsigned CREAM = 0xFFF6E2, GOLD = 0xEFC851;

TitleScreen::TitleScreen(SceneLibrary* lib)
    : lib_(lib), menu_(NULL), reticle_l_(NULL), reticle_r_(NULL), run_info_(NULL), profile_(NULL), buttons_box_(NULL),
      logo_(NULL), blur_(NULL), menu_known_(false), submenu_(false), has_profile_(false), run_info_top_(0), run_info_shown_(false),
      focus_(-1), covered_(false), last_action_(NONE), carets_pending_(0) {}

Node* TitleScreen::build(const std::string& version, std::string* error) {
    buttons_.clear();
    tweens_.clear();
    focus_ = -1;
    menu_ = instance(lib_, "scenes/screens/main_menu.tscn");
    if (!menu_) {
        if (error) *error = "no main_menu.tscn";
        return NULL;
    }
    hide(menu_->get_node("PatchNotesButton"));
    hide(menu_->get_node("ModdedWarning"));
    hide(menu_->get_node("ModWarningContainer"));
    hide(menu_->get_node("PatchNotesScreen"));
    Node* blur = menu_->get_node("BlurBackstop");
    blur_ = blur && blur->is_canvas_item() ? (CanvasItem*)blur : NULL;
    if (blur_) {
        blur_->shader_params["lod"] = 0;
        blur_->shader_params["mix_percentage"] = 0;
    }
    hide(menu_->get_node("Fps"));
    set_label(menu_->get_node("ReleaseInfo"), version.empty() ? std::string("Spire360 preview")
                                                              : "Spire360 preview\n" + version);
    Node* rl = menu_->find_unique("ButtonReticleLeft");
    Node* rr = menu_->find_unique("ButtonReticleRight");
    reticle_l_ = rl && rl->is_control() ? (Control*)rl : NULL;
    reticle_r_ = rr && rr->is_control() ? (Control*)rr : NULL;
    run_info_ = ctl(menu_, "ContinueRunInfo");
    if (run_info_) {
        /* NContinueRunInfo._Ready: transparent until shown. */
        run_info_top_ = run_info_->offset[SIDE_TOP];
        run_info_->modulate.a = 0;
    }
    run_info_shown_ = false;
    Node* p = menu_->get_node("ChangeProfileButton");
    profile_ = p && p->is_control() ? (Control*)p : NULL;
    hide(profile_);
    show_glyph(lib_, ctl(profile_, "HotkeyIcon"), "start");
    buttons_box_ = menu_->get_node("MainMenuTextButtons");
    Node* bg = menu_->get_node("MainMenuBg");
    logo_ = bg ? find_named(bg, "Logo") : NULL;
    menu_->add_child(frame_driver(this));
    /* NMainMenu: the screen shake's target (a change of the setting shakes
     * it) */
    if (menu_->is_control()) menu_->add_child(make_shake_driver((Control*)menu_));
    covered_ = false;
    submenu_ = false;
    /* The buttons wait for the profile's menu (NMainMenu reads the
     * progress before it shows). */
    has_profile_ = false;
    set_menu(Json());
    menu_known_ = false;
    set_visible(buttons_box_, false);
    return menu_;
}

void TitleScreen::set_submenu(bool open) {
    if (!menu_ || open == submenu_) return;
    submenu_ = open;
    set_visible(buttons_box_, !open && menu_known_);
    set_visible(profile_, !open && has_profile_);
    if (logo_ && logo_->is_canvas_item())
        tweens_.tween(&((CanvasItem*)logo_)->modulate.a, open ? 0.0f : 1.0f, 1.0f, TRANS_CUBIC, EASE_OUT);
    if (blur_) {
        /* NMainMenu.EnableBackstop / DisableBackstop */
        tweens_.tween(&blur_->shader_params["lod"], open ? 3.0f : 0.0f, open ? 0.25f : 0.15f);
        tweens_.tween(&blur_->shader_params["mix_percentage"], open ? 0.7f : 0.0f, open ? 0.25f : 0.15f);
    }
    if (open) show_run_info(false);
}

void TitleScreen::detach() {
    menu_ = NULL;
    buttons_.clear();
    reticle_l_ = reticle_r_ = run_info_ = profile_ = NULL;
    buttons_box_ = logo_ = NULL;
    blur_ = NULL;
    focus_ = -1;
    carets_pending_ = 0;
    tweens_.clear();
}

void TitleScreen::set_menu(const Json& m) {
    if (!menu_) return;
    menu_known_ = true;
    set_visible(buttons_box_, !submenu_);
    Node* box = menu_->get_node("MainMenuTextButtons");
    if (!box) return;
    bool has_run = m["has_run_save"].b();
    bool others = m.is_null() || m["singleplayer_enabled"].b(true);
    const Json& tl = m["timeline"];
    static const struct {
        const char* node;
        const char* key;
        Action action;
    } all[] = { { "ContinueButton", "CONTINUE", CONTINUE },     { "AbandonRunButton", "ABANDON_RUN", ABANDON },
                { "SingleplayerButton", "SINGLE_PLAYER", SINGLEPLAYER }, { "MultiplayerButton", "MULTIPLAYER", NONE },
                { "TimelineButton", "TIMELINE", TIMELINE },     { "SettingsButton", "SETTINGS", SETTINGS },
                { "CompendiumButton", "COMPENDIUM", COMPENDIUM },     { "QuitButton", "QUIT", QUIT } };
    Action focused = focus_ >= 0 && focus_ < (int)buttons_.size() ? buttons_[(size_t)focus_].action : NONE;
    if (focus_ >= 0 && focus_ < (int)buttons_.size() && buttons_[(size_t)focus_].label) {
        buttons_[(size_t)focus_].label->self_modulate = hex(CREAM);
        buttons_[(size_t)focus_].label->scale = Vec2(1, 1);
    }
    buttons_.clear();
    focus_ = -1;
    for (size_t i = 0; i < sizeof(all) / sizeof(all[0]); ++i) {
        Node* b = box->get_node(all[i].node);
        if (!b || !b->is_control()) continue;
        bool visible = true, enabled = true;
        switch (all[i].action) {
        case CONTINUE:
        case ABANDON: visible = has_run; break;
        case SINGLEPLAYER: visible = !has_run; enabled = others; break;
        case TIMELINE: visible = tl["visible"].b(); enabled = tl["enabled"].b(); break;
        case NONE: visible = false; break;
        default: break;
        }
        set_visible(b, visible);
        if (!visible) continue;
        Node* l = b->get_node("Label");
        set_label(l, Loc::get("main_menu_ui", all[i].key));
        Button bt;
        bt.node = (Control*)b;
        bt.label = l && l->is_control() ? (Control*)l : NULL;
        bt.action = all[i].action;
        bt.enabled = enabled;
        /* NMainMenuTextButton.OnDisable / OnEnable. */
        if (bt.label) bt.label->modulate = enabled ? Color(1, 1, 1, 1) : Color(1, 1, 1, 0.25f);
        buttons_.push_back(bt);
    }
    set_visible(menu_->find_unique("TimelineNotificationDot"), tl["notification"].b());
    /* NContinueRunInfo.SetResult / ShowInfo. */
    if (run_info_ && has_run) {
        const Json& r = m["run"];
        bool ok = r.is_object() && r["error"].is_null();
        set_visible(ctl(run_info_, "RunInfoContainer"), ok);
        set_visible(ctl(run_info_, "ErrorContainer"), !ok);
        if (ok) {
            set_label(ctl(run_info_, "DateLabel"), r["date_text"].str());
            set_label(ctl(run_info_, "ProgressLabel"), r["progress_text"].str());
            set_label(ctl(run_info_, "HealthLabel"), r["health_text"].str());
            set_label(ctl(run_info_, "GoldLabel"), r["gold_text"].str());
            set_texture(lib_, ctl(run_info_, "CharacterIcon"), r["icon"].str());
            Control* asc = ctl(run_info_, "AscensionLabel");
            set_label(asc, r["ascension_text"].str());
            set_visible(asc, !r["ascension_text"].str().empty());
        }
    }
    /* NOpenProfileScreenButton: the profile's icon and title. */
    has_profile_ = m["profile"].i() > 0;
    if (profile_ && has_profile_) {
        set_visible(profile_, !submenu_);
        int id = m["profile"].i();
        Node* icon = ctl(profile_, "ProfileIcon");
        set_texture(lib_, icon ? icon->get_node("Icon") : NULL, format("images/ui/profile/profile_icon_%d.png", id));
        set_label(ctl(profile_, "Title"), Loc::format("main_menu_ui", "OPEN_PROFILE_SCREEN.title", "Id", format("%d", id)));
        hide(ctl(profile_, "Description")); /* controller: no "Click to edit" */
    }
    /* Keep the focus on the same button when it is still there (the
     * last pressed one, as NMainMenu's _lastHitButton), else the first. */
    if (focused == NONE) focused = last_action_;
    int keep = -1, first = -1;
    for (size_t i = 0; i < buttons_.size(); ++i) {
        if (!buttons_[i].enabled) continue;
        if (first < 0) first = (int)i;
        if (buttons_[i].action == focused) keep = (int)i;
    }
    show_run_info(false);
    if (!covered_ && menu_known_ && !submenu_) focus(keep >= 0 ? keep : first);
}

/* NContinueRunInfo.AnimShow / AnimHide. */
void TitleScreen::show_run_info(bool show) {
    if (!run_info_ || show == run_info_shown_) return;
    run_info_shown_ = show;
    float h = run_info_->offset[SIDE_BOTTOM] - run_info_->offset[SIDE_TOP];
    float y = show ? run_info_top_ - 20 : run_info_top_;
    if (show) {
        tweens_.tween(&run_info_->offset[SIDE_TOP], y, 0.2f, TRANS_BACK, EASE_OUT);
        tweens_.tween(&run_info_->offset[SIDE_BOTTOM], y + h, 0.2f, TRANS_BACK, EASE_OUT);
    } else {
        tweens_.tween(&run_info_->offset[SIDE_TOP], y, 0.2f);
        tweens_.tween(&run_info_->offset[SIDE_BOTTOM], y + h, 0.2f);
    }
    tweens_.tween(&run_info_->modulate.a, show ? 1.0f : 0.0f, 0.2f);
}

/* NMainMenuTextButton.OnFocus (gold, 1.05) and NMainMenu's carets. */
void TitleScreen::focus(int i) {
    if (i < 0 || i >= (int)buttons_.size() || i == focus_) return;
    if (lib_->audio() && focus_ >= 0) lib_->audio()->ui("event:/sfx/ui/clicks/ui_hover");
    if (focus_ >= 0 && buttons_[(size_t)focus_].label) {
        Control* old = buttons_[(size_t)focus_].label;
        old->self_modulate = hex(CREAM);
        tweens_.tween(&old->scale, Vec2(1, 1), 0.5f, TRANS_EXPO, EASE_OUT);
    }
    focus_ = i;
    last_action_ = buttons_[(size_t)i].action;
    /* NMainMenuContinueButton: the run info while Continue is focused. */
    show_run_info(buttons_[(size_t)i].action == CONTINUE);
    Control* l = buttons_[(size_t)i].label;
    if (!l) return;
    l->pivot_offset = l->size * 0.5f;
    tweens_.tween(&l->scale, Vec2(1.05f, 1.05f), 0.05f);
    l->self_modulate = hex(GOLD);
    /* The carets: placed once the layout has the button (NMainMenu defers
     * MainMenuButtonFocused a frame too). */
    carets_pending_ = 2; /* a frame for the layout to place newly shown buttons */
}

/* NMainMenu.MainMenuButtonFocused: the carets beside the label, sliding out
 * 28 px (Back Out, 0.25 s). */
void TitleScreen::place_carets() {
    carets_pending_ = 0;
    if (focus_ < 0 || focus_ >= (int)buttons_.size()) return;
    Control* l = buttons_[(size_t)focus_].label;
    Control* b = buttons_[(size_t)focus_].node;
    if (!l || !b) return;
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
        c->modulate = covered_ ? Color(1, 1, 1, 0) : hex(GOLD);
    }
}

void TitleScreen::set_covered(bool covered) {
    covered_ = covered;
    if (covered) {
        tweens_.clear();
        if (focus_ >= 0 && focus_ < (int)buttons_.size() && buttons_[(size_t)focus_].label) {
            buttons_[(size_t)focus_].label->self_modulate = hex(CREAM);
            buttons_[(size_t)focus_].label->scale = Vec2(1, 1);
        }
        focus_ = -1; /* focused again (carets back) when uncovered */
        if (run_info_) {
            run_info_shown_ = false;
            run_info_->modulate.a = 0;
            float h = run_info_->offset[SIDE_BOTTOM] - run_info_->offset[SIDE_TOP];
            run_info_->offset[SIDE_TOP] = run_info_top_;
            run_info_->offset[SIDE_BOTTOM] = run_info_top_ + h;
        }
    }
    if (reticle_l_) reticle_l_->modulate.a = covered ? 0.0f : 1.0f;
    if (reticle_r_) reticle_r_->modulate.a = covered ? 0.0f : 1.0f;
    if (!covered && menu_known_) {
        for (size_t i = 0; i < buttons_.size(); ++i)
            if (buttons_[i].action == last_action_ && buttons_[i].enabled) focus((int)i);
    }
}

TitleScreen::Action TitleScreen::input(const PadState& pad) {
    if (covered_) return NONE;
    int n = (int)buttons_.size();
    int first = -1;
    for (int i = 0; i < n && first < 0; ++i)
        if (buttons_[(size_t)i].enabled) first = i;
    if (first < 0) return (pad.pressed & PadState::A) ? SINGLEPLAYER : NONE;
    if (focus_ < 0) {
        focus(first);
        return NONE;
    }
    /* Up / down to the next enabled button, wrapping. */
    int step = (pad.pressed & PadState::UP) ? -1 : (pad.pressed & PadState::DOWN) ? 1 : 0;
    if (step) {
        int j = focus_;
        for (int k = 0; k < n; ++k) {
            j = (j + step + n) % n;
            if (buttons_[(size_t)j].enabled) break;
        }
        focus(j);
    }
    if ((pad.pressed & PadState::START) && profile_ && profile_->visible) {
        if (lib_->audio()) lib_->audio()->ui("event:/sfx/ui/clicks/ui_click");
        return PROFILES;
    }
    if (pad.pressed & PadState::A) {
        if (lib_->audio()) lib_->audio()->ui("event:/sfx/ui/clicks/ui_click");
        return buttons_[(size_t)focus_].action;
    }
    return NONE;
}

void TitleScreen::frame(double dt) {
    if (carets_pending_ > 0 && --carets_pending_ == 0) place_carets();
    tweens_.update(dt);
}

} /* namespace spire */
