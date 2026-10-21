#include "ftue.h"
#include "loc.h"

#include "../audio/game_audio.h"
#include "../scene/controls.h"
#include "../scene/node.h"
#include "../scene/rich_text.h"
#include "../scene/scene.h"
#include "../scene/script.h"

#include <math.h>

namespace spire {

namespace {

/* The common FTUEs: header and description keys (NxxxFtue._Ready). */
struct Tip {
    const char* id;
    const char* title;
    const char* description;
    const char* disclaimer; /* %Disclaimer (the ascension popup) */
};
const Tip TIPS[] = {
    { "shuffle_ftue", "SHUFFLE_FTUE_TITLE", "SHUFFLE_FTUE_DESCRIPTION", NULL },
    { "can_play_cards_ftue", "CAN_PLAY_CARDS_FTUE_TITLE", "CAN_PLAY_CARDS_FTUE_DESCRIPTION", NULL },
    { "cannot_play_card_ftue", "CANNOT_PLAY_CARD_FTUE_TITLE", "CANNOT_PLAY_CARD_FTUE_DESCRIPTION", NULL },
    { "combat_reward_ftue", "REWARDS_FTUE_TITLE", "REWARDS_FTUE_DESCRIPTION", NULL },
    { "merchant_ftue", "MERCHANT_FTUE_TITLE", "MERCHANT_FTUE_DESCRIPTION", NULL },
    { "obtain_potion_ftue", "POTION_FTUE_TITLE", "POTION_FTUE_DESCRIPTION", NULL },
    { "obtain_relic_ftue", "RELIC_FTUE_TITLE", "RELIC_FTUE_DESCRIPTION", NULL },
    { "power_card_ftue", "POWER_FTUE_TITLE", "POWER_FTUE_DESCRIPTION", NULL },
    { "rest_site_ftue", "REST_SITE_FTUE_TITLE", "REST_SITE_FTUE_DESCRIPTION", NULL },
    { "map_select_ftue", "MAP_SELECT_TITLE", "MAP_SELECT_DESCRIPTION", NULL },
    { "ascension_singleplayer_ftue", "ASCENSION_SINGLEPLAYER_FTUE_TITLE", "ASCENSION_SINGLEPLAYER_FTUE_DESCRIPTION",
      "ASCENSION_SINGLEPLAYER_FTUE_DISCLAIMER" },
};

const Tip* tip_of(const std::string& id) {
    for (size_t i = 0; i < sizeof(TIPS) / sizeof(TIPS[0]); ++i)
        if (id == TIPS[i].id) return &TIPS[i];
    return NULL;
}

/* COMBAT_BASICS_FTUE_PAGE_COUNT: "({currentPage} of {totalPages})" */
std::string page_count(int page) {
    std::string s = Loc::format("ftues", "COMBAT_BASICS_FTUE_PAGE_COUNT", "currentPage", format("%d", page));
    size_t at = s.find("{totalPages}");
    if (at != std::string::npos) s.replace(at, 12, "3");
    return s;
}

void set_body(Node* n, const std::string& text) {
    set_label(n, text);
    if (n && n->type == "RichTextLabel") ((RichTextLabel*)n)->visible_ratio = 1;
}

} /* namespace */

FtuePopup::FtuePopup(SceneLibrary* lib)
    : lib_(lib), backstop_(NULL), root_(NULL), outline_(NULL), pulse_t_(0),
      page_(0), image_(NULL), body_(NULL), prev_(NULL), page_count_(NULL), image_x_(0), body_x_(0) {
    for (int i = 0; i < 3; ++i) images_[i] = NULL;
}

Node* FtuePopup::build(const std::string& id, const std::vector<CanvasItem*>& spotlights, const Vec2* at,
                       std::string* error) {
    id_ = id;
    tweens_.clear();
    std::string scene = "scenes/ftue/" + id.substr(0, id.size() - 5) + "_ftue.tscn";
    Node* p = instance(lib_, scene);
    if (!p || !p->is_control()) {
        delete p;
        if (error) *error = "no " + scene;
        return NULL;
    }
    root_ = full_rect_control("ModalContainer");
    /* NModalContainer.ShowBackstop */
    backstop_ = new ColorRect();
    backstop_->name = "Backstop";
    backstop_->anchor[SIDE_RIGHT] = backstop_->anchor[SIDE_BOTTOM] = 1;
    backstop_->color = Color(0, 0, 0, 0);
    root_->add_child(backstop_);
    tweens_.tween(&backstop_->color.a, 0.85f, 0.3f);
    root_->add_child(p);
    /* the controls the tip is about over the backstop (ZIndex + 1) */
    for (size_t i = 0; i < spotlights.size(); ++i) {
        if (!spotlights[i]) continue;
        spotlights_.push_back(spotlights[i]);
        spotlight_z_.push_back(spotlights[i]->z_index);
        spotlights[i]->z_index += 1;
    }
    if (id == "combat_rules_ftue") {
        /* NCombatRulesFtue.Start: page 1, the image and text fading in */
        page_ = 1;
        Node* image = p->get_node("Image");
        image_ = image && image->is_control() ? (Control*)image : NULL;
        Node* body = p->find_unique("Description");
        body_ = body && body->is_control() ? (Control*)body : NULL;
        Node* prev = p->get_node("LeftArrow");
        prev_ = prev && prev->is_control() ? (Control*)prev : NULL;
        page_count_ = p->get_node("PageCount");
        static const char* const KEYS[3] = { "_image1", "_image2", "_image3" };
        for (int i = 0; i < 3; ++i) {
            const Json* v = p->get_extra(KEYS[i]);
            images_[i] = v ? lib_->resolve(*v, lib_->file(scene)) : NULL; /* its ExtResources */
        }
        set_label(p->get_node("Header"), Loc::get("ftues", "COMBAT_BASICS_FTUE_HEADER"));
        set_label(page_count_, page_count(1));
        if (prev_) prev_->visible = false;
        if (image_) {
            image_x_ = image_->offset[SIDE_LEFT];
            image_->modulate.a = 0;
            tweens_.tween(&image_->modulate.a, 1.0f, 0.5f, TRANS_CUBIC, EASE_OUT);
            if (images_[0] && images_[0]->is_texture() && image_->type == "TextureRect")
                ((TextureRect*)image_)->texture = (Texture2DRes*)images_[0];
        }
        if (body_) {
            body_x_ = body_->offset[SIDE_LEFT];
            set_body(body_, Loc::get("ftues", "TUTORIAL_FTUE_BODY_1"));
            body_->modulate.a = 0;
            tweens_.tween(&body_->modulate.a, 1.0f, 1.0f);
            if (body_->type == "RichTextLabel")
                tweens_.tween_from(&((RichTextLabel*)body_)->visible_ratio, 0, 1, 0.6f, TRANS_SINE, EASE_OUT);
        }
    } else {
        const Tip* t = tip_of(id);
        Node* header = p->find_unique("Header");
        Node* body = p->find_unique("Description");
        Node* confirm = p->find_unique("FtueConfirmButton");
        if (!header) header = p->get_node("FtuePopup/Header");
        if (!body) body = p->get_node("FtuePopup/DescriptionContainer/Description");
        if (!confirm) confirm = p->get_node("FtuePopup/FtueConfirmButton");
        if (t) {
            set_label(header, Loc::get("ftues", t->title));
            set_body(body, Loc::get("ftues", t->description));
            if (t->disclaimer) set_body(p->find_unique("Disclaimer"), Loc::get("ftues", t->disclaimer));
        }
        if (id == "ascension_singleplayer_ftue") {
            /* NAscensionSingleplayerFtue._Ready: after 1 s it rises 100 px
             * (0.3 s, Back Out) and fades in */
            Control* c = (Control*)p;
            c->modulate.a = 0;
            tweens_.tween(&c->modulate.a, 1.0f, 0.3f, TRANS_LINEAR, EASE_OUT, 1.0f);
            tweens_.tween_from(&c->offset[SIDE_TOP], c->offset[SIDE_TOP] + 100, c->offset[SIDE_TOP], 0.3f, TRANS_BACK,
                               EASE_OUT, 1.0f);
            tweens_.tween_from(&c->offset[SIDE_BOTTOM], c->offset[SIDE_BOTTOM] + 100, c->offset[SIDE_BOTTOM], 0.3f,
                               TRANS_BACK, EASE_OUT, 1.0f);
        }
        /* NFtueConfirmButton: "Got it!", the outline pulsing 1 <-> 0.25 */
        set_label(confirm ? confirm->find_unique("Label") : NULL, Loc::get("ftues", "CONFIRM_BUTTON"));
        Node* outline = confirm ? confirm->get_node("Outline") : NULL;
        outline_ = outline && outline->is_control() ? (Control*)outline : NULL;
        /* NRelicRewardFtue: %Arrow's top left at the relic's bottom centre */
        Node* arrow = id == "obtain_relic_ftue" ? p->find_unique("Arrow") : NULL;
        if (at && arrow && arrow->is_control()) {
            Control* c = (Control*)arrow;
            float w = c->offset[SIDE_RIGHT] - c->offset[SIDE_LEFT], h = c->offset[SIDE_BOTTOM] - c->offset[SIDE_TOP];
            for (int k = 0; k < 4; ++k) c->anchor[k] = 0;
            c->offset[SIDE_LEFT] = at->x;
            c->offset[SIDE_TOP] = at->y;
            c->offset[SIDE_RIGHT] = at->x + w;
            c->offset[SIDE_BOTTOM] = at->y + h;
        }
        /* NMapSelectFtue: %Positioner at the first room's centre */
        Node* pos = p->find_unique("Positioner");
        if (at && pos && pos->is_control()) {
            /* GlobalPosition, before the tree is laid out: from the top
             * left (its centre anchors would count the screen twice) */
            Control* c = (Control*)pos;
            for (int k = 0; k < 4; ++k) c->anchor[k] = 0;
            c->offset[SIDE_LEFT] = c->offset[SIDE_RIGHT] = at->x;
            c->offset[SIDE_TOP] = c->offset[SIDE_BOTTOM] = at->y;
        }
    }
    root_->add_child(frame_driver(this));
    return root_;
}

void FtuePopup::turn_page(int dir) {
    page_ += dir;
    if (prev_) prev_->visible = page_ > 1;
    set_label(page_count_, page_count(page_));
    static const char* const BODIES[3] = { "TUTORIAL_FTUE_BODY_1", "TUTORIAL_FTUE_BODY_2", "TUTORIAL_FTUE_BODY_3" };
    float from = dir > 0 ? 200.0f : -200.0f; /* _imageAnimOffset */
    if (image_) {
        Resource* r = images_[page_ - 1];
        if (r && r->is_texture() && image_->type == "TextureRect") ((TextureRect*)image_)->texture = (Texture2DRes*)r;
        float w = image_->offset[SIDE_RIGHT] - image_->offset[SIDE_LEFT];
        tweens_.tween_from(&image_->modulate.a, 0.5f, 1.0f, 0.5f, TRANS_CUBIC, EASE_OUT);
        tweens_.tween_from(&image_->offset[SIDE_LEFT], image_x_ + from, image_x_, 0.5f, TRANS_EXPO, EASE_OUT);
        tweens_.tween_from(&image_->offset[SIDE_RIGHT], image_x_ + from + w, image_x_ + w, 0.5f, TRANS_EXPO, EASE_OUT);
    }
    if (body_) {
        set_body(body_, Loc::get("ftues", BODIES[page_ - 1]));
        float w = body_->offset[SIDE_RIGHT] - body_->offset[SIDE_LEFT];
        tweens_.tween_from(&body_->offset[SIDE_LEFT], body_x_ + from, body_x_, 0.5f, TRANS_EXPO, EASE_OUT);
        tweens_.tween_from(&body_->offset[SIDE_RIGHT], body_x_ + from + w, body_x_ + w, 0.5f, TRANS_EXPO, EASE_OUT);
        tweens_.tween_from(&body_->modulate.a, 0, 1, 0.6f);
        if (body_->type == "RichTextLabel")
            tweens_.tween_from(&((RichTextLabel*)body_)->visible_ratio, 0, 1, 0.6f, TRANS_SINE, EASE_OUT);
    }
}

bool FtuePopup::input(const PadState& pad) {
    if (page_ > 0) {
        /* the gold arrows: A or right the next page (the last closes),
         * left the previous */
        if (pad.pressed & (PadState::A | PadState::RIGHT)) {
            if (lib_->audio()) lib_->audio()->ui("event:/sfx/ui/clicks/ui_click");
            if (page_ == 3) return true;
            turn_page(1);
        } else if ((pad.pressed & PadState::LEFT) && page_ > 1) {
            if (lib_->audio()) lib_->audio()->ui("event:/sfx/ui/clicks/ui_click");
            turn_page(-1);
        }
        return false;
    }
    if (pad.pressed & PadState::A) {
        if (lib_->audio()) lib_->audio()->ui("event:/sfx/ui/clicks/ui_click");
        return true;
    }
    return false;
}

void FtuePopup::close() {
    for (size_t i = 0; i < spotlights_.size(); ++i) spotlights_[i]->z_index = spotlight_z_[i];
    spotlights_.clear();
    spotlight_z_.clear();
}

void FtuePopup::frame(double dt) {
    tweens_.update(dt);
    if (outline_) {
        pulse_t_ = fmod(pulse_t_ + dt, 1.2);
        outline_->modulate.a =
            (float)(pulse_t_ < 0.6 ? 0.25 + 0.75 * pulse_t_ / 0.6 : 1.0 - 0.75 * (pulse_t_ - 0.6) / 0.6);
    }
}

} /* namespace spire */
