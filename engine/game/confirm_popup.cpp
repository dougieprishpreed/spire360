#include "confirm_popup.h"
#include "../audio/game_audio.h"
#include "loc.h"

#include "../scene/controls.h"
#include "../scene/node.h"
#include "../scene/scene.h"

namespace spire {

ConfirmPopup::ConfirmPopup(SceneLibrary* lib)
    : lib_(lib), backstop_(NULL), popup_(NULL), yes_(NULL), no_(NULL), rest_y_(0) {}

Node* ConfirmPopup::build(std::string* error) {
    tweens_.clear();
    Node* p = instance(lib_, "scenes/ui/abandon_run_confirm_popup.tscn");
    if (!p || !p->is_control()) {
        delete p;
        if (error) *error = "no abandon_run_confirm_popup.tscn";
        return NULL;
    }
    Control* root = full_rect_control("ModalContainer");
    /* NModalContainer.ShowBackstop. */
    backstop_ = new ColorRect();
    backstop_->name = "Backstop";
    backstop_->anchor[SIDE_RIGHT] = backstop_->anchor[SIDE_BOTTOM] = 1;
    backstop_->color = Color(0, 0, 0, 0);
    root->add_child(backstop_);
    tweens_.tween(&backstop_->color.a, 0.85f, 0.3f);
    popup_ = (Control*)p;
    root->add_child(popup_);
    /* SetText, InitYesButton, InitNoButton. */
    Node* v = popup_->get_node("VerticalPopup");
    set_label(v ? v->get_node("Header") : NULL, Loc::get("main_menu_ui", "ABANDON_RUN_CONFIRMATION.header"));
    set_label(ctl(v, "Description"), Loc::get("main_menu_ui", "ABANDON_RUN_CONFIRMATION.body"));
    Control* yes = ctl(v, "YesButton");
    Control* no = ctl(v, "NoButton");
    set_label(ctl(yes, "Label"), Loc::get("main_menu_ui", "GENERIC_POPUP.confirm"));
    set_label(ctl(no, "Label"), Loc::get("main_menu_ui", "GENERIC_POPUP.cancel"));
    show_glyph(lib_, ctl(yes, "HotkeyIcon"), "a");
    show_glyph(lib_, ctl(no, "HotkeyIcon"), "b");
    yes_ = ctl(yes, "Visuals");
    no_ = ctl(no, "Visuals");
    /* _EnterTree: from transparent black, 100 px lower. */
    popup_->modulate = Color(0, 0, 0, 0);
    tweens_.tween(&popup_->modulate.r, 1, 0.1f);
    tweens_.tween(&popup_->modulate.g, 1, 0.1f);
    tweens_.tween(&popup_->modulate.b, 1, 0.1f);
    tweens_.tween(&popup_->modulate.a, 1, 0.1f);
    rest_y_ = popup_->offset[SIDE_TOP];
    float h = popup_->offset[SIDE_BOTTOM] - popup_->offset[SIDE_TOP];
    tweens_.tween_from(&popup_->offset[SIDE_TOP], rest_y_ + 100, rest_y_, 0.3f, TRANS_BACK, EASE_OUT);
    tweens_.tween_from(&popup_->offset[SIDE_BOTTOM], rest_y_ + 100 + h, rest_y_ + h, 0.3f, TRANS_BACK, EASE_OUT);
    root->add_child(frame_driver(this));
    return root;
}

/* NPopupYesNoButton.OnPress: the visuals shrink to 0.975. */
void ConfirmPopup::press(Control* visuals) {
    if (!visuals) return;
    visuals->pivot_offset = visuals->size * 0.5f;
    tweens_.tween(&visuals->scale, Vec2(0.975f, 0.975f), 0.5f, TRANS_EXPO, EASE_OUT);
}

ConfirmPopup::Action ConfirmPopup::input(const PadState& pad) {
    if (pad.pressed & PadState::A) {
        press(yes_);
        if (lib_->audio()) lib_->audio()->ui("event:/sfx/ui/clicks/ui_click");
        return YES;
    }
    if (pad.pressed & (PadState::B | PadState::START)) {
        press(no_);
        if (lib_->audio()) lib_->audio()->ui("event:/sfx/ui/clicks/ui_back");
        return NO;
    }
    return NONE;
}

void ConfirmPopup::frame(double dt) { tweens_.update(dt); }

} /* namespace spire */
