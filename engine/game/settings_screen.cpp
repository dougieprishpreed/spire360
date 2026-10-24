#include "settings_screen.h"

#include "../audio/game_audio.h"
#include "../core/pack.h"
#include "../scene/controls.h"
#include "../scene/scene.h"
#include "confirm_popup.h"
#include "credits_screen.h"
#include "loc.h"
#include "screen_shake.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

namespace spire {

/* NLanguageDropdown._languageCodeToName: each language in its own words */
static const struct {
    const char* code;
    const char* name;
} LANGUAGE_NAMES[] = {
    { "eng", "English" },
    { "zhs", "\xE4\xB8\xAD\xE6\x96\x87" },
    { "zht", "\xE7\xB9\x81\xE9\xAB\x94\xE4\xB8\xAD\xE6\x96\x87" },
    { "deu", "Deutsch" },
    { "esp", "Espa\xC3\xB1ol (Latinoam\xC3\xA9rica)" },
    { "fra", "Fran\xC3\xA7" "ais" },
    { "ind", "Bahasa Indonesia" },
    { "ita", "Italiano" },
    { "jpn", "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E" },
    { "kor", "\xED\x95\x9C\xEA\xB5\xAD\xEC\x96\xB4" },
    { "pol", "Polski" },
    { "ptb", "Portugu\xC3\xAAs Brasileiro" },
    { "rus", "\xD0\xA0\xD1\x83\xD1\x81\xD1\x81\xD0\xBA\xD0\xB8\xD0\xB9" },
    { "spa", "Espa\xC3\xB1ol (Castellano)" },
    { "tha", "\xE0\xB9\x84\xE0\xB8\x97\xE0\xB8\xA2" },
    { "tur", "T\xC3\xBCrk\xC3\xA7" "e" },
};

static std::string language_name(const std::string& code) {
    for (size_t i = 0; i < sizeof(LANGUAGE_NAMES) / sizeof(LANGUAGE_NAMES[0]); ++i)
        if (code == LANGUAGE_NAMES[i].code) return LANGUAGE_NAMES[i].name;
    return code;
}

/* LocManager.GetLanguageCompletion: the language's keys over English's
 * (data/localization/completion.json) */
static float language_completion(const std::string& code) {
    static Json counts;
    static bool read = false;
    if (!read) {
        read = true;
        std::string path = Loc::data_path("localization/completion.json");
        FILE* f = fopen(path.c_str(), "rb");
        if (f) {
            std::string text;
            char buf[4096];
            size_t got;
            while ((got = fread(buf, 1, sizeof(buf), f)) > 0) text.append(buf, got);
            fclose(f);
            std::string err;
            counts = Json::parse(text.c_str(), text.size(), &err);
        }
    }
    float eng = counts["eng"].f(0);
    return eng > 0 ? counts[code.c_str()].f(0) / eng : 1.0f;
}

/* StsColors.cream / halfTransparentCream */
static const Color CREAM(1.0f, 0.964706f, 0.886275f, 1.0f), HALF_CREAM(1.0f, 0.964706f, 0.886275f, 0.5f);

static Control* control_of(Node* n) { return n && n->is_control() ? (Control*)n : NULL; }

SettingsScreen::SettingsScreen(SceneLibrary* lib)
    : lib_(lib), root_(NULL), focus_(-1), tab_(0), toast_(NULL), toast_t_(-1), confirm_(NULL), confirm_node_(NULL),
      credits_(NULL), credits_node_(NULL), dropdown_(NULL), dropdown_open_(false), language_enabled_(true), item_focus_(0),
      scroll_(0), scroll_target_(0), scroll_limit_(0) {
    for (int i = 0; i < 2; ++i) panels_[i] = boxes_[i] = tabs_[i] = NULL;
}

SettingsScreen::~SettingsScreen() {
    if (credits_node_) {
        if (credits_node_->parent) credits_node_->parent->remove_child(credits_node_);
        delete credits_node_;
    }
    delete credits_;
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
    x.handle = x.handle_velocity = 0;
    x.handle_set = false;
    hide(x.reticle);
    lines_.push_back(x);
}

Node* SettingsScreen::build(const Json& settings, std::string* error) {
    settings_ = settings;
    lines_.clear();
    tweens_.clear();
    focus_ = -1;
    tab_ = 0;
    items_.clear();
    codes_.clear();
    dropdown_open_ = false;
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
    static const char* const pc_lines[] = { "ShowMpDrawingsDivider", "ShowMpDrawings",
                                            "LimiFpsDivider", "LimitFpsInBackground", "UploadDataDivider",
                                            "UploadGameplayData", "SendFeedbackDivider", "SendFeedback", "ModdingDivider",
                                            "Modding", "ScreenshakeDivider" };
    for (size_t i = 0; i < sizeof(pc_lines) / sizeof(pc_lines[0]); ++i) hide(general ? find_named(general, pc_lines[i]) : NULL);
    if (sound) {
        hide(find_named(sound, "MuteIfBackground"));
        hide(find_named(sound, "Line5"));
    }

    /* the lines in the order the pad moves through them */
    dropdown_ = control_of(screen->find_unique("LanguageDropdown"));
    size_t before = lines_.size();
    if (dropdown_ && general) add(0, general, "LanguageLine", NULL, DROPDOWN, "language");
    if (lines_.size() > before) {
        Line& l = lines_.back();
        Node* line = find_named(general, "LanguageLine");
        set_label(line ? line->get_node("Label") : NULL,
                  Loc::get("settings_ui", settings["in_run"].b() ? "LANGUAGE_IN_RUN" : "LANGUAGE"));
        l.control = dropdown_;
        l.reticle = dropdown_->get_node("SelectionReticle");
        hide(l.reticle);
        set_label(dropdown_->find_unique("Label"), language_name(settings["language"].str()));
        /* NSettingsScreen: in a run the language stays as it is */
        language_enabled_ = !settings["in_run"].b();
        if (!language_enabled_) {
            Color gray(0.5f, 0.5f, 0.5f, 1); /* StsColors.gray */
            Node* label = find_named(general, "LanguageLine");
            label = label ? label->get_node("Label") : NULL;
            if (label && label->is_canvas_item()) ((CanvasItem*)label)->modulate = gray;
            dropdown_->modulate = gray;
        }
    }
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
            /* the handle follows in frame() (NSlider._Process); it starts
             * at the value (SetValueWithoutAnimation in _Ready) */
            float v = settings_[l.key].f(0.5f);
            int pct = (int)floorf(v * 100.0f + 0.5f);
            set_label(l.control->get_node("SliderValue"), format("%d%%", pct));
            if (!l.handle_set) {
                l.handle = v * 100.0f;
                l.handle_set = true;
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
    if (dropdown_) {
        /* NSettingsDropdown.OnFocus / OnUnfocus */
        Node* h = dropdown_->find_unique("Highlight");
        bool on = i >= 0 && lines_[(size_t)i].kind == DROPDOWN;
        if (h && h->is_canvas_item()) ((CanvasItem*)h)->modulate = on ? hex(0x3C5B6B) : hex(0x2C434F);
    }
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
        /* NTickbox.ForceToggleTick */
        if (lib_->audio()) lib_->audio()->ui(on ? "event:/sfx/ui/clicks/ui_checkbox_on" : "event:/sfx/ui/clicks/ui_checkbox_off");
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
        /* NScreenshakePaginator.OnIndexChanged: the new multiplier, then a
         * Medium trauma to feel it */
        static const float shake[5] = { 0, 0.5f, 1, 2, 4 };
        ScreenShake::get().set_multiplier(shake[k]);
        ScreenShake::get().trauma("Medium");
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

/* The languages the pack's fonts can draw (the converter's
 * spire/languages.json, from its --languages); English for a pack without
 * it. */
static bool pack_has_language(SceneLibrary* lib, const std::string& code) {
    static Json doc;
    static bool read = false;
    if (!read) {
        read = true;
        std::vector<u8> raw;
        if (lib && lib->pack() && lib->pack()->read("spire/languages.json", raw) && !raw.empty()) {
            std::string err;
            doc = Json::parse((const char*)&raw[0], raw.size(), &err);
        }
    }
    const Json& l = doc["languages"];
    if (!l.size()) return code == "eng";
    for (size_t i = 0; i < l.size(); ++i)
        if (l[i].str() == code) return true;
    return false;
}

/* NLanguageDropdown.PopulateOptions + NDropdown.OpenDropdown: an item per
 * language (a warning icon when under 90% translated), the first focused. */
void SettingsScreen::open_dropdown() {
    Node* container = dropdown_->find_unique("DropdownContainer");
    Control* box = control_of(container ? container->get_node("VBoxContainer") : NULL);
    if (!box) return;
    while (!box->children.empty()) {
        Node* c = box->children.back();
        box->remove_child(c);
        delete c;
    }
    items_.clear();
    codes_.clear();
    const Json& langs = settings_["languages"];
    for (size_t i = 0; i < langs.size(); ++i) {
        Node* item = instance(lib_, "scenes/ui/language_dropdown_item.tscn");
        if (!item || !item->is_control()) {
            delete item;
            continue;
        }
        std::string code = langs[i].str();
        if (!pack_has_language(lib_, code)) {
            delete item;
            continue;
        }
        std::string text = language_name(code);
        if (language_completion(code) < 0.9f) text = "[img]res://images/ui/language_warning.png[/img]" + text;
        set_label(item->get_node("RichLabel"), text);
        hide(item->get_node("Highlight"));
        box->add_child(item);
        items_.push_back((Control*)item);
        codes_.push_back(code);
    }
    /* NDropdownContainer.RefreshLayout: as tall as the items, at most its
     * own 600 px (then it scrolls) */
    Control* c = control_of(container);
    float content = 44.0f * (float)items_.size();
    float max_h = 600.0f;
    if (c) c->offset[SIDE_BOTTOM] = c->offset[SIDE_TOP] + (content < max_h ? content : max_h);
    scroll_limit_ = content > max_h ? max_h - content : 0.0f;
    set_visible(container ? container->get_node("Scrollbar") : NULL, content > max_h);
    scroll_ = scroll_target_ = 0;
    box->offset[SIDE_TOP] = 0;
    box->offset[SIDE_BOTTOM] = content - (content < max_h ? content : max_h);
    set_visible(container, true);
    dropdown_open_ = true;
    focus_item(0);
}

void SettingsScreen::close_dropdown() {
    set_visible(dropdown_ ? dropdown_->find_unique("DropdownContainer") : NULL, false);
    dropdown_open_ = false;
}

/* NDropdownItem.OnFocus, NDropdownContainer.ProcessGuiFocus: the focused
 * item highlighted and scrolled towards the middle */
void SettingsScreen::focus_item(int i) {
    if (items_.empty()) return;
    if (i < 0) i = 0;
    if (i >= (int)items_.size()) i = (int)items_.size() - 1;
    item_focus_ = i;
    for (size_t k = 0; k < items_.size(); ++k) set_visible(items_[k]->get_node("Highlight"), (int)k == i);
    float target = -44.0f * (float)i + 300.0f;
    scroll_target_ = clampf(target, scroll_limit_, 0.0f);
}

SettingsScreen::Action SettingsScreen::input(const PadState& pad, std::string* cmd) {
    cmd->clear();
    if (dropdown_open_) {
        if (pad.pressed & PadState::B) close_dropdown();
        else if (pad.pressed & PadState::UP) focus_item(item_focus_ - 1);
        else if (pad.pressed & PadState::DOWN) focus_item(item_focus_ + 1);
        else if ((pad.pressed & PadState::A) && item_focus_ < (int)codes_.size()) {
            /* NLanguageDropdown.OnDropdownItemSelected: a new language is
             * saved and set (the screens follow the logic's answer) */
            const std::string& code = codes_[(size_t)item_focus_];
            close_dropdown();
            if (code != settings_["language"].str()) {
                set_label(dropdown_->find_unique("Label"), language_name(code));
                settings_.set("language", Json::make_string(code));
                *cmd = format("{\"cmd\":\"set_setting\",\"name\":\"language\",\"value\":\"%s\"}", code.c_str());
            }
        }
        return NONE;
    }
    if (credits_) {
        if (!credits_->input(pad)) {
            if (credits_node_->parent) credits_node_->parent->remove_child(credits_node_);
            delete credits_node_;
            credits_node_ = NULL;
            delete credits_;
            credits_ = NULL;
        }
        return NONE;
    }
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
        if (l.kind == DROPDOWN) {
            if (language_enabled_) open_dropdown();
            return NONE;
        }
        if (l.kind == BUTTON && strcmp(l.key, "credits") == 0) {
            /* NCreditsButton: the credits over the settings */
            std::string err;
            credits_ = new CreditsScreen(lib_);
            credits_node_ = credits_->build(&err);
            if (credits_node_) root_->add_child(credits_node_);
            else {
                delete credits_;
                credits_ = NULL;
            }
            return NONE;
        }
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
    /* NSlider._Process / UpdateHandlePosition: the handle glides to the
     * value (SmoothDamp, 0.05 s) and sits at Size.X * value / max, centred
     * (the bar's size now that it is laid out) */
    for (size_t i = 0; i < lines_.size(); ++i) {
        Line& l = lines_[i];
        if (l.kind != SLIDER || !l.control || !l.handle_set) continue;
        l.handle = smooth_damp(l.handle, settings_[l.key].f(0.5f) * 100.0f, &l.handle_velocity, 0.05f, (float)dt);
        Node* slider = l.control->get_node("Slider");
        Control* handle = control_of(slider ? slider->find_unique("Handle") : NULL);
        Control* sc = control_of(slider);
        if (!handle || !sc) continue;
        float w = handle->offset[SIDE_RIGHT] - handle->offset[SIDE_LEFT];
        float x = sc->size.x * l.handle / 100.0f - w * 0.5f;
        handle->anchor[SIDE_LEFT] = handle->anchor[SIDE_RIGHT] = 0;
        handle->offset[SIDE_LEFT] = x;
        handle->offset[SIDE_RIGHT] = x + w;
    }
    if (dropdown_open_ && scroll_ != scroll_target_) {
        /* NDropdownContainer.UpdateScrollPosition: 15 a second towards the
         * target; the scrollbar's train follows */
        float before = scroll_;
        scroll_ += (scroll_target_ - scroll_) * minf((float)dt * 15.0f, 1.0f);
        if (fabsf(scroll_ - scroll_target_) < 0.5f || (before - scroll_target_) * (scroll_ - scroll_target_) < 0)
            scroll_ = scroll_target_;
        Node* container = dropdown_->find_unique("DropdownContainer");
        Control* box = control_of(container ? container->get_node("VBoxContainer") : NULL);
        if (box) {
            float h = box->offset[SIDE_BOTTOM] - box->offset[SIDE_TOP];
            box->offset[SIDE_TOP] = scroll_;
            box->offset[SIDE_BOTTOM] = scroll_ + h;
        }
        Control* train = control_of(container ? container->get_node("Scrollbar/Train") : NULL);
        if (train && scroll_limit_ < 0) {
            float k = scroll_ / scroll_limit_;
            train->offset[SIDE_TOP] = -291.0f + k * 494.0f;
            train->offset[SIDE_BOTTOM] = train->offset[SIDE_TOP] + 88.0f;
        }
    }
}

} /* namespace spire */
