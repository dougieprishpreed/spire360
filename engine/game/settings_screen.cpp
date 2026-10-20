#include "settings_screen.h"

#include "../scene/controls.h"
#include "../scene/scene.h"
#include "confirm_popup.h"
#include "loc.h"

#include <math.h>
#include <string.h>

namespace spire {

/* StsColors.cream / halfTransparentCream */
static const Color CREAM(1.0f, 0.964706f, 0.886275f, 1.0f), HALF_CREAM(1.0f, 0.964706f, 0.886275f, 0.5f);

static Control* control_of(Node* n) { return n && n->is_control() ? (Control*)n : NULL; }

SettingsScreen::SettingsScreen(SceneLibrary* lib)
    : lib_(lib), root_(NULL), focus_(-1), tab_(0), toast_(NULL), toast_t_(-1), confirm_(NULL), confirm_node_(NULL) {
    for (int i = 0; i < 2; ++i) panels_[i] = boxes_[i] = tabs_[i] = NULL;
}

SettingsScreen::~SettingsScreen() {
    if (confirm_node_) {
        if (confirm_node_->parent) confirm_node_->parent->remove_child(confirm_node_);
        delete confirm_node_;
    }
    delete confirm_;
}

void SettingsScreen::add(int tab, Node* panel, const char* line, const char* control, Kind kind, const char* key) {
    Node* l = panel ? find_named(panel, line) : NULL;
    if (!l || !l->is_control()) return;
    Node* c = control ? find_named(l, control) : l;
    Line x;
    x.kind = kind;
    x.key = key;
    x.line = (Control*)l;
    x.control = control_of(c);
    x.reticle = c ? find_named(c, "SelectionReticle") : NULL;
    x.tab = tab;
    hide(x.reticle);
    lines_.push_back(x);
}

Node* SettingsScreen::build(const Json& settings, std::string* error) {
    settings_ = settings;
    lines_.clear();
    tweens_.clear();
    focus_ = -1;
    tab_ = 0;
    Node* screen = instance(lib_, "scenes/screens/settings_screen.tscn");
    if (!screen) {
        if (error) *error = "no settings_screen.tscn";
        return NULL;
    }
    Control* root = full_rect_control("SettingsScreen");
    root_ = root;
    /* the submenu's backstop (NMainMenu / NCapstoneContainer) */
    ColorRect* back = (ColorRect*)SceneLibrary::create_node("ColorRect");
    back->name = "Backstop";
    back->anchor[SIDE_RIGHT] = back->anchor[SIDE_BOTTOM] = 1;
    back->color = Color(0, 0, 0, 0.85f);
    root->add_child(back);
    root->add_child(screen);

    Node* general = screen->find_unique("GeneralSettings");
    Node* sound = screen->find_unique("SoundSettings");
    panels_[0] = control_of(general);
    panels_[1] = control_of(sound);
    boxes_[0] = control_of(general ? general->get_node("VBoxContainer") : NULL);
    boxes_[1] = control_of(sound ? sound->get_node("VBoxContainer") : NULL);
    hide(screen->find_unique("GraphicsSettings"));
    hide(screen->find_unique("InputSettings"));

    /* LocalizeLabels */
    static const struct {
        int tab;
        const char* line;
        const char* key;
    } labels[] = { { 0, "FastMode", "FASTMODE" }, { 0, "PhobiaMode", "PHOBIA_MODE" }, { 0, "Screenshake", "SCREENSHAKE" },
                   { 0, "ShowRunTimer", "SHOW_RUN_TIMER_HEADER" }, { 0, "ShowHandCardCount", "SHOW_HAND_CARD_COUNT_HEADER" },
                   { 0, "LongPressConfirmations", "LONG_PRESS_CONFIRMATION_HEADER" },
                   { 0, "SkipIntroLogo", "SKIP_INTRO_LOGO_HEADER" }, { 0, "TextEffects", "TEXT_EFFECTS" },
                   { 0, "Credits", "CREDITS" }, { 0, "ResetGameplay", "RESET_DEFAULT" },
                   { 1, "MasterVolume", "MASTER_VOLUME" }, { 1, "BgmVolume", "MUSIC_VOLUME" },
                   { 1, "SfxVolume", "SFX_VOLUME" }, { 1, "AmbienceVolume", "AMBIENCE_VOLUME" } };
    for (size_t i = 0; i < sizeof(labels) / sizeof(labels[0]); ++i) {
        Node* panel = labels[i].tab == 0 ? general : sound;
        Node* l = panel ? find_named(panel, labels[i].line) : NULL;
        set_label(l ? l->get_node("Label") : NULL, Loc::get("settings_ui", labels[i].key));
    }
    /* the PC's lines (and the dividers above them) */
    static const char* const pc_lines[] = { "LanguageLine", "LanguageDropdown", "ShowMpDrawingsDivider", "ShowMpDrawings",
                                            "LimiFpsDivider", "LimitFpsInBackground", "UploadDataDivider",
                                            "UploadGameplayData", "SendFeedbackDivider", "SendFeedback", "ModdingDivider",
                                            "Modding", "ScreenshakeDivider" };
    for (size_t i = 0; i < sizeof(pc_lines) / sizeof(pc_lines[0]); ++i) hide(general ? find_named(general, pc_lines[i]) : NULL);
    if (sound) {
        hide(find_named(sound, "MuteIfBackground"));
        hide(find_named(sound, "Line5"));
    }

    /* the lines in the order the pad moves through them */
    add(0, general, "Screenshake", "Paginator", PAGINATOR, "screenshake");
    add(0, general, "FastMode", "FastModeTickbox", TICKBOX, "fast_mode");
    add(0, general, "PhobiaMode", "PhobiaModeTickbox", TICKBOX, "phobia_mode");
    add(0, general, "ShowRunTimer", "SettingsTickbox", TICKBOX, "run_timer");
    add(0, general, "ShowHandCardCount", "SettingsTickbox", TICKBOX, "card_indices");
    add(0, general, "LongPressConfirmations", "SettingsTickbox", TICKBOX, "long_press");
    add(0, general, "SkipIntroLogo", "SettingsTickbox", TICKBOX, "skip_intro");
    add(0, general, "TextEffects", "TextEffectsTickbox", TICKBOX, "text_effects");
    add(0, general, "Credits", "CreditsButton", BUTTON, "credits");
    add(0, general, "ResetGameplay", "ResetGameplayButton", BUTTON, "reset");
    add(1, sound, "MasterVolume", "MasterVolumeSlider", SLIDER, "master");
    add(1, sound, "BgmVolume", "BgmVolumeSlider", SLIDER, "bgm");
    add(1, sound, "SfxVolume", "SfxSlider", SLIDER, "sfx");
    add(1, sound, "AmbienceVolume", "AmbienceSlider", SLIDER, "ambience");

    /* the tabs: General and Sound, LB / RB */
    static const char* const tab_nodes[4] = { "General", "Graphics", "Sound", "Input" };
    static const char* const tab_keys[4] = { "TAB_GENERAL", "TAB_GRAPHICS", "TAB_SOUND", "TAB_INPUT" };
    for (int i = 0; i < 4; ++i) {
        Node* t = find_named(screen, tab_nodes[i]);
        if (!t) continue;
        if (i == 1 || i == 3) {
            hide(t);
            continue;
        }
        set_label(t->get_node("Label"), Loc::get("settings_ui", tab_keys[i]));
        tabs_[i == 0 ? 0 : 1] = control_of(t);
    }
    show_glyph(lib_, screen->find_unique("LeftTabIcon"), "lb");
    show_glyph(lib_, screen->find_unique("RightTabIcon"), "rb");
    show_glyph(lib_, find_named(screen->get_node("BackButton"), "HotkeyIcon"), "b");
    hide(find_named(screen, "Scrollbar"));
    toast_ = control_of(screen->find_unique("Toast"));
    if (toast_) toast_->modulate.a = 0;

    refresh();
    show_tab(0);
    root->add_child(frame_driver(this));
    return root;
}

void SettingsScreen::set_settings(const Json& settings) {
    settings_ = settings;
    refresh();
}

/* SetFromSettings: each control shows its value. */
void SettingsScreen::refresh() {
    static const char* const shakes[5] = { "SCREENSHAKE_NONE", "SCREENSHAKE_SOME", "SCREENSHAKE_NORMAL", "SCREENSHAKE_LOTS",
                                           "SCREENSHAKE_CAAAW" };
    for (size_t i = 0; i < lines_.size(); ++i) {
        Line& l = lines_[i];
        if (!l.control) continue;
        if (l.kind == TICKBOX) {
            bool on = settings_[l.key].b();
            Node* v = l.control->find_unique("TickboxVisuals");
            if (!v) v = find_named(l.control, "TickboxVisuals");
            set_visible(v ? v->get_node("Ticked") : NULL, on);
            set_visible(v ? v->get_node("NotTicked") : NULL, !on);
        } else if (l.kind == PAGINATOR) {
            int k = settings_[l.key].i(2);
            set_label(l.control->find_unique("Label"), k >= 0 && k < 5 ? Loc::get("settings_ui", shakes[k]) : std::string(">:P"));
        } else if (l.kind == SLIDER) {
            /* NSlider: the handle at value / max across the bar */
            float v = settings_[l.key].f(0.5f);
            int pct = (int)floorf(v * 100.0f + 0.5f);
            set_label(l.control->get_node("SliderValue"), format("%d%%", pct));
            Node* slider = l.control->get_node("Slider");
            Control* handle = control_of(slider ? slider->find_unique("Handle") : NULL);
            Control* sc = control_of(slider);
            if (handle && sc) {
                float w = handle->offset[SIDE_RIGHT] - handle->offset[SIDE_LEFT];
                float x = sc->size.x * (float)pct / 100.0f - w * 0.5f;
                handle->anchor[SIDE_LEFT] = handle->anchor[SIDE_RIGHT] = 0;
                handle->offset[SIDE_LEFT] = x;
                handle->offset[SIDE_RIGHT] = x + w;
            }
        } else if (l.kind == BUTTON) {
            Node* label = find_named(l.control, "Label");
            if (label && strcmp(l.key, "credits") == 0) set_label(label, Loc::get("settings_ui", "CREDITS"));
        }
    }
}

void SettingsScreen::show_tab(int tab) {
    tab_ = tab;
    for (int i = 0; i < 2; ++i) {
        set_visible(panels_[i], i == tab);
        if (!tabs_[i]) continue;
        /* NSettingsTab.Select / Deselect */
        set_visible(tabs_[i]->get_node("Outline"), i == tab);
        Node* label = tabs_[i]->get_node("Label");
        if (label && label->is_canvas_item()) ((CanvasItem*)label)->modulate = i == tab ? CREAM : HALF_CREAM;
    }
    for (size_t i = 0; i < lines_.size(); ++i)
        if (lines_[i].tab == tab) {
            focus((int)i);
            return;
        }
    focus(-1);
}

void SettingsScreen::focus(int i) {
    for (size_t k = 0; k < lines_.size(); ++k) set_visible(lines_[k].reticle, (int)k == i);
    focus_ = i;
    if (i < 0 || !boxes_[tab_]) return;
    /* NSettingsPanel: the focused line kept inside the panel */
    Control* box = boxes_[tab_];
    Control* line = lines_[(size_t)i].line;
    float y = line->offset[SIDE_TOP], h = line->offset[SIDE_BOTTOM] - line->offset[SIDE_TOP];
    float view = panels_[tab_] ? panels_[tab_]->size.y : 700.0f;
    float scroll = -box->offset[SIDE_TOP];
    if (y + h > scroll + view) scroll = y + h - view;
    if (y < scroll) scroll = y;
    if (scroll < 0) scroll = 0;
    float bh = box->offset[SIDE_BOTTOM] - box->offset[SIDE_TOP];
    tweens_.tween(&box->offset[SIDE_TOP], -scroll, 0.2f, TRANS_QUAD, EASE_OUT);
    tweens_.tween(&box->offset[SIDE_BOTTOM], -scroll + bh, 0.2f, TRANS_QUAD, EASE_OUT);
}

/* NSettingsToast.Show: up 120 px (Back Out, 0.25 s), a second, then it
 * fades and shrinks (0.5 s). */
void SettingsScreen::toast(const std::string& text) {
    if (!toast_) return;
    set_label(toast_->get_node("Label"), text);
    tweens_.kill(&toast_->modulate.a);
    toast_->modulate.a = 1;
    toast_->scale = Vec2(1, 1);
    tweens_.tween(&toast_->modulate.a, 0.0f, 0.5f, TRANS_LINEAR, EASE_IN_OUT, 1.25f);
    tweens_.tween(&toast_->scale, Vec2(0.8f, 0.8f), 0.5f, TRANS_LINEAR, EASE_IN_OUT, 1.25f);
}

/* A control changed by step (-1 / 1: left / right; 0: A). */
std::string SettingsScreen::change(Line& l, int step) {
    if (l.kind == TICKBOX && step == 0) {
        bool on = !settings_[l.key].b();
        settings_.set(l.key, Json::make_bool(on));
        static const struct {
            const char* key;
            const char* toast;
        } toasts[] = { { "fast_mode", "TOAST_FAST_MODE" }, { "phobia_mode", "TOAST_PHOBIA_MODE" },
                       { "run_timer", "TOAST_RUN_TIMER" }, { "card_indices", "TOAST_HAND_CARD_COUNT" },
                       { "skip_intro", "TOAST_SKIP_INTRO_LOGO" }, { "text_effects", "TOAST_TEXT_EFFECTS" } };
        for (size_t i = 0; i < sizeof(toasts) / sizeof(toasts[0]); ++i)
            if (strcmp(toasts[i].key, l.key) == 0)
                toast(Loc::get("settings_ui", (std::string(toasts[i].toast) + (on ? "_ON" : "_OFF")).c_str()));
        refresh();
        return format("{\"cmd\":\"set_setting\",\"name\":\"%s\",\"value\":%s}", l.key, on ? "true" : "false");
    }
    if (l.kind == PAGINATOR && step != 0) {
        int k = settings_[l.key].i(2) + step;
        if (k < 0) k = 4;
        if (k > 4) k = 0;
        settings_.set(l.key, Json::make_number(k));
        refresh();
        return format("{\"cmd\":\"set_setting\",\"name\":\"%s\",\"value\":%d}", l.key, k);
    }
    if (l.kind == SLIDER && step != 0) {
        float v = settings_[l.key].f(0.5f) + 0.05f * (float)step;
        v = floorf(v * 20.0f + 0.5f) / 20.0f;
        v = clampf(v, 0, 1);
        settings_.set(l.key, Json::make_number(v));
        refresh();
        return format("{\"cmd\":\"set_setting\",\"name\":\"%s\",\"value\":%.2f}", l.key, v);
    }
    return std::string();
}

SettingsScreen::Action SettingsScreen::input(const PadState& pad, std::string* cmd) {
    cmd->clear();
    if (confirm_) {
        ConfirmPopup::Action a = confirm_->input(pad);
        if (a != ConfirmPopup::NONE) {
            if (a == ConfirmPopup::YES) *cmd = "{\"cmd\":\"reset_gameplay_settings\"}";
            if (confirm_node_->parent) confirm_node_->parent->remove_child(confirm_node_);
            delete confirm_node_;
            confirm_node_ = NULL;
            delete confirm_;
            confirm_ = NULL;
        }
        return NONE;
    }
    if (pad.pressed & PadState::B) return BACK;
    if (pad.pressed & (PadState::LB | PadState::RB)) {
        show_tab(tab_ == 0 ? 1 : 0);
        return NONE;
    }
    if (focus_ < 0) return NONE;
    int n = (int)lines_.size();
    if (pad.pressed & (PadState::UP | PadState::DOWN)) {
        int step = (pad.pressed & PadState::UP) ? -1 : 1;
        for (int k = focus_ + step; k >= 0 && k < n; k += step)
            if (lines_[(size_t)k].tab == tab_) {
                focus(k);
                break;
            }
        return NONE;
    }
    Line& l = lines_[(size_t)focus_];
    if (pad.pressed & (PadState::LEFT | PadState::RIGHT)) *cmd = change(l, (pad.pressed & PadState::LEFT) ? -1 : 1);
    else if (pad.pressed & PadState::A) {
        if (l.kind == BUTTON && strcmp(l.key, "credits") == 0) return CREDITS;
        if (l.kind == BUTTON && strcmp(l.key, "reset") == 0) {
            /* NResetGameplayButton: its confirmation first */
            std::string err;
            confirm_ = new ConfirmPopup(lib_);
            confirm_node_ = confirm_->build_generic(Loc::get("settings_ui", "RESET_CONFIRMATION.header"),
                                                    Loc::get("settings_ui", "RESET_GAMEPLAY_CONFIRMATION.body"), &err);
            if (confirm_node_) root_->add_child(confirm_node_);
            else {
                delete confirm_;
                confirm_ = NULL;
            }
            return NONE;
        }
        *cmd = change(l, 0);
    }
    return NONE;
}

void SettingsScreen::frame(double dt) {
    tweens_.update(dt);
}

} /* namespace spire */
