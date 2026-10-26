#include "pause_menu.h"
#include "../audio/game_audio.h"
#include "loc.h"

#include "../scene/controls.h"
#include "../scene/node.h"
#include "../scene/scene.h"

namespace spire {

/* StsColors.cream / gold. */
static const unsigned CREAM = 0xFFF6E2, GOLD = 0xEFC851;

PauseMenu::PauseMenu(SceneLibrary* lib) : lib_(lib), focus_(-1) {}

Node* PauseMenu::build(bool give_up, std::string* error) {
    buttons_.clear();
    tweens_.clear();
    focus_ = -1;
    Node* menu = instance(lib_, "scenes/screens/pause_menu/pause_menu.tscn");
    if (!menu) {
        if (error) *error = "no pause_menu.tscn";
        return NULL;
    }
    /* RefreshLabels. */
    Node* paused = ctl(menu, "PausedText");
    set_label(paused ? paused->get_node("Label") : NULL, Loc::get("gameplay_ui", "PAUSE_MENU.PAUSED"));
    Node* box = ctl(menu, "ButtonContainer");
    static const struct {
        const char* node;
        const char* key;
        Action action;
    } all[] = { { "Resume", "PAUSE_MENU.RESUME", RESUME },
                { "Settings", "PAUSE_MENU.SETTINGS", SETTINGS },
                { "Compendium", "PAUSE_MENU.COMPENDIUM", COMPENDIUM },
                { "GiveUp", "PAUSE_MENU.GIVE_UP", GIVE_UP },
                { "Disconnect", "PAUSE_MENU.DISCONNECT", NONE },
                { "SaveAndQuit", "PAUSE_MENU.SAVE_AND_QUIT", SAVE_AND_QUIT } };
    for (size_t i = 0; i < sizeof(all) / sizeof(all[0]); ++i) {
        Node* b = box ? box->get_node(all[i].node) : NULL;
        if (!b || !b->is_control()) continue;
        set_label(b->get_node("Label"), Loc::get("gameplay_ui", all[i].key));
        if (all[i].action == NONE) {
            hide(b);
            continue;
        }
        Button bt;
        bt.node = (Control*)b;
        Node* img = b->get_node("ButtonImage");
        Node* lab = b->get_node("Label");
        bt.image = img && img->is_control() ? (Control*)img : NULL;
        bt.label = lab && lab->is_control() ? (Control*)lab : NULL;
        bt.action = all[i].action;
        if (bt.action == GIVE_UP && !give_up) {
            /* OnDisable (NButton): dimmed, never focused. */
            bt.node->modulate.a = 0.5f;
            continue;
        }
        buttons_.push_back(bt);
    }
    Node* back = ctl(menu, "BackButton");
    show_glyph(lib_, find_named(back, "HotkeyIcon"), "b");
    menu->add_child(frame_driver(this));
    focus(0);
    /* NCapstoneContainer.Open: the shared backstop (85% black) fades in
     * over 0.5 s (Cubic Out) under the pause menu. */
    Control* root = full_rect_control("CapstoneScreenContainer");
    ColorRect* backstop = new ColorRect();
    backstop->name = "CapstoneBackstop";
    backstop->anchor[SIDE_RIGHT] = backstop->anchor[SIDE_BOTTOM] = 1;
    backstop->color = Color(0, 0, 0, 0.85098f);
    backstop->modulate.a = 0;
    tweens_.tween(&backstop->modulate.a, 1, 0.5f, TRANS_CUBIC, EASE_OUT);
    root->add_child(backstop);
    root->add_child(menu);
    return root;
}

void PauseMenu::unfocus(Button& b) {
    if (b.image) {
        tweens_.tween(&b.image->shader_params["s"], 0.8f, 0.5f, TRANS_EXPO, EASE_OUT);
        tweens_.tween(&b.image->shader_params["v"], 0.9f, 0.5f, TRANS_EXPO, EASE_OUT);
        tweens_.tween(&b.image->scale, Vec2(0.9f, 0.9f), 0.5f, TRANS_EXPO, EASE_OUT);
    }
    if (b.label) b.label->add_theme_color_override("font_color", hex(CREAM));
}

/* NPauseMenuButton.OnFocus / OnUnfocus. */
void PauseMenu::focus(int i) {
    if (i < 0 || i >= (int)buttons_.size() || i == focus_) return;
    if (focus_ >= 0) unfocus(buttons_[(size_t)focus_]);
    focus_ = i;
    Button& b = buttons_[(size_t)i];
    if (b.image) {
        tweens_.kill(&b.image->shader_params["s"]);
        tweens_.kill(&b.image->shader_params["v"]);
        b.image->shader_params["s"] = 1.1f;
        b.image->shader_params["v"] = 1.1f;
        b.image->pivot_offset = b.image->size * 0.5f;
        tweens_.tween(&b.image->scale, Vec2(0.95f, 0.95f), 0.05f);
    }
    if (b.label) b.label->add_theme_color_override("font_color", hex(GOLD));
}

PauseMenu::Action PauseMenu::input(const PadState& pad) {
    if (buttons_.empty()) return (pad.pressed & (PadState::B | PadState::START)) ? RESUME : NONE;
    int n = (int)buttons_.size();
    int was = focus_;
    if (pad.pressed & PadState::UP) focus((focus_ + n - 1) % n);
    if (pad.pressed & PadState::DOWN) focus((focus_ + 1) % n);
    if (lib_->audio() && focus_ != was) lib_->audio()->ui("event:/sfx/ui/clicks/ui_hover");
    if (pad.pressed & (PadState::B | PadState::START)) {
        /* the back button's hotkeys (NBackButton.ClickedSfx), then
         * OnBackOrResumeButtonPressed. */
        if (lib_->audio()) {
            lib_->audio()->ui("event:/sfx/ui/clicks/ui_back");
            lib_->audio()->ui("event:/sfx/ui/map/map_close");
        }
        return RESUME;
    }
    if ((pad.pressed & PadState::A) && focus_ >= 0) {
        Action a = buttons_[(size_t)focus_].action;
        /* NPauseMenuButton: NButton's ui_click; Resume's map_close too */
        if (lib_->audio()) {
            lib_->audio()->ui("event:/sfx/ui/clicks/ui_click");
            if (a == RESUME) lib_->audio()->ui("event:/sfx/ui/map/map_close");
        }
        return a;
    }
    return NONE;
}

void PauseMenu::frame(double dt) { tweens_.update(dt); }

} /* namespace spire */
