#include "profile_screen.h"
#include "../audio/game_audio.h"
#include "loc.h"

#include "../scene/controls.h"
#include "../scene/node.h"
#include "../scene/scene.h"

namespace spire {

ProfileScreen::ProfileScreen(SceneLibrary* lib) : lib_(lib), option_(NULL), focus_(-1) {
    for (int i = 0; i < 3; ++i) buttons_[i] = NULL;
}

Node* ProfileScreen::build(std::string* error) {
    tweens_.clear();
    focus_ = -1;
    Node* screen = instance(lib_, "scenes/screens/profiles/profile_screen.tscn");
    if (!screen) {
        if (error) *error = "no profile_screen.tscn";
        return NULL;
    }
    hide(ctl(screen, "LoadingOverlay"));
    for (int i = 0; i < 3; ++i) {
        buttons_[i] = ctl(screen, format("ProfileButton%d", i + 1).c_str());
        hide(ctl(screen, format("DeleteProfileButton%d", i + 1).c_str()));
        Control* icon = ctl(buttons_[i], "ProfileIcon");
        set_texture(lib_, icon ? icon->get_node("Icon") : NULL, format("images/ui/profile/profile_icon_%d.png", i + 1));
    }
    set_label(ctl(screen, "ChooseProfileMessage"), Loc::get("main_menu_ui", "PROFILE_SCREEN.BUTTON.chooseProfileMessage"));
    /* The option line: the screen's own message style, under the buttons. */
    option_ = NULL;
    Node* twin = instance(lib_, "scenes/screens/profiles/profile_screen.tscn");
    Control* line = ctl(twin, "ChooseProfileMessage");
    if (line) {
        line->parent->remove_child(line);
        line->name = "UnlockEverything";
        line->offset[SIDE_TOP] = 860;
        line->offset[SIDE_BOTTOM] = 940;
        line->add_theme_font_size_override("font_size", 30);
        screen->add_child(line);
        option_ = line;
        Node* glyph = instance(lib_, "scenes/ui/hotkey_icon.tscn");
        if (glyph && glyph->is_control()) {
            Control* g = (Control*)glyph;
            g->anchor[SIDE_LEFT] = g->anchor[SIDE_RIGHT] = 0;
            g->offset[SIDE_LEFT] = -64;
            g->offset[SIDE_RIGHT] = -8;
            g->offset[SIDE_TOP] = 12;
            g->offset[SIDE_BOTTOM] = 68;
            line->add_child(g);
            show_glyph(lib_, g, "y");
        } else {
            delete glyph;
        }
    }
    delete twin;
    show_glyph(lib_, find_named(screen->get_node("BackButton"), "HotkeyIcon"), "b");
    screen->add_child(frame_driver(this));
    refresh();
    return screen;
}

void ProfileScreen::set_profiles(const Json& profiles) {
    profiles_ = profiles;
    refresh();
}

void ProfileScreen::set_menu(const Json& menu) {
    menu_ = menu;
    refresh();
}

/* NProfileButton.Initialize. */
void ProfileScreen::refresh() {
    int current = menu_["profile"].i();
    const Json& ps = profiles_["profiles"];
    for (int i = 0; i < 3; ++i) {
        Control* b = buttons_[i];
        if (!b) continue;
        Node* bg = ctl(b, "Background");
        set_label(ctl(bg, "Title"), Loc::format("main_menu_ui", "PROFILE_SCREEN.BUTTON.title", "Id", format("%d", i + 1)));
        const Json& p = ps[(size_t)i];
        set_label(ctl(bg, "Info"), p.is_object() ? p["description"].str() : std::string());
        set_visible(ctl(bg, "CurrentProfileIndicator"), current == i + 1);
    }
    if (option_) {
        bool on = menu_["unlock_all"].b();
        set_label(option_, std::string("Unlock Everything: ") + (on ? "On" : "Off"));
        set_visible(option_, current > 0);
    }
    if (focus_ < 0 && current > 0) focus(current - 1);
}

/* NProfileButton.OnFocus / OnUnfocus. */
void ProfileScreen::focus(int i) {
    if (i < 0 || i > 2 || i == focus_ || !buttons_[i]) return;
    if (focus_ >= 0 && buttons_[focus_]) {
        Control* o = buttons_[focus_];
        tweens_.tween(&o->scale, Vec2(1, 1), 0.5f, TRANS_EXPO, EASE_OUT);
        Control* bg = ctl(o, "Background");
        if (bg) tweens_.tween(&bg->shader_params["v"], 1.0f, 0.5f, TRANS_EXPO, EASE_OUT);
    }
    focus_ = i;
    Control* b = buttons_[i];
    b->pivot_offset = b->size * 0.5f;
    tweens_.tween(&b->scale, Vec2(1.03f, 1.03f), 0.05f);
    Control* bg = ctl(b, "Background");
    if (bg) tweens_.tween(&bg->shader_params["v"], 1.3f, 0.05f);
}

ProfileScreen::Action ProfileScreen::input(const PadState& pad, std::string* cmd) {
    if (focus_ < 0) focus(0);
    if (pad.pressed & PadState::LEFT) focus((focus_ + 2) % 3);
    if (pad.pressed & PadState::RIGHT) focus((focus_ + 1) % 3);
    if (lib_->audio() && (pad.pressed & (PadState::LEFT | PadState::RIGHT))) lib_->audio()->ui("event:/sfx/ui/clicks/ui_hover");
    if (pad.pressed & PadState::B) {
        if (lib_->audio()) lib_->audio()->ui("event:/sfx/ui/clicks/ui_back");
        return BACK;
    }
    if ((pad.pressed & PadState::Y) && menu_["profile"].i() > 0) {
        if (lib_->audio()) lib_->audio()->ui("event:/sfx/ui/clicks/ui_click");
        *cmd = std::string("{\"cmd\":\"set_unlock_all\",\"on\":") + (menu_["unlock_all"].b() ? "false" : "true") + "}";
        return NONE;
    }
    if (pad.pressed & PadState::A) {
        if (lib_->audio()) lib_->audio()->ui("event:/sfx/ui/clicks/ui_click");
        /* OnRelease: the current profile goes back; another one is
         * switched to (SwitchToThisProfile, then the menu reloads). */
        if (focus_ + 1 == menu_["profile"].i()) return BACK;
        *cmd = format("{\"cmd\":\"select_profile\",\"profile\":%d}", focus_ + 1);
        return BACK;
    }
    return NONE;
}

void ProfileScreen::frame(double dt) { tweens_.update(dt); }

} /* namespace spire */
