#include "shop_screen.h"
#include "inspect_screens.h"
#include "card_view.h"
#include "loc.h"
#include "top_bar.h"

#include "../scene/animation.h"
#include "../scene/controls.h"
#include "../scene/node.h"
#include "../scene/scene.h"
#include "../scene/spine_sprite.h"

#include <math.h>

namespace spire {

/* NMerchantSlot: _smallScale / _hoverScale. */
static const float SMALL_SCALE = 0.65f, HOVER_SCALE = 0.8f;
/* NMerchantInventory: the slots' position:y open and closed. */
static const float OPEN_Y = 80.0f, CLOSED_Y = -1000.0f;
/* StsColors.cream / red / green. */
static const unsigned CREAM = 0xFFF6E2, RED = 0xFF5555, GREEN = 0x7FFF00;

static Control* as_control(Node* n) { return n && n->is_control() ? (Control*)n : NULL; }

/* NSpineAutoPlayer / the characters' _Ready: the idle loop. */
static void play_idle_all(Node* n) {
    if (n->type == "SpineSprite") {
        SpineSprite* s = (SpineSprite*)n;
        if (!s->set_animation("idle_loop", true, 0)) s->set_animation("idle", true, 0);
    }
    for (size_t i = 0; i < n->children.size(); ++i) play_idle_all(n->children[i]);
}

ShopScreen::ShopScreen(SceneLibrary* lib)
    : lib_(lib), root_(NULL), inspect_card_(NULL), inspect_relic_(NULL), inspect_node_(NULL), inventory_(NULL), backstop_(NULL), slots_(NULL), merchant_(NULL), merchant_reticle_(NULL),
      back_button_(NULL), focus_(-1), open_(false), start_open_(false), start_index_(0), slots_closed_top_(0),
      slots_open_top_(0), slots_height_(0) {}

std::string ShopScreen::focused_kind() const {
    return focus_ >= 0 && focus_ < (int)slots_list_.size() ? slots_list_[(size_t)focus_].kind : std::string();
}

int ShopScreen::focused_index() const {
    return focus_ >= 0 && focus_ < (int)slots_list_.size() ? slots_list_[(size_t)focus_].index : 0;
}

static void set_cost(Node* slot, int cost, bool affordable, bool on_sale) {
    Node* label = slot->find_unique("CostLabel");
    set_label(label, format("%d", cost));
    if (label && label->is_canvas_item())
        ((CanvasItem*)label)->modulate = hex(!affordable ? RED : (on_sale ? GREEN : CREAM));
}

/* NMerchantInventory.Initialize: each slot filled from its entry (FillSlot /
 * UpdateVisual); a sold one hidden. */
void ShopScreen::fill_slots(Node* inv, const Json& d) {
    slots_list_.clear();
    int gold = d["player"]["gold"].i();
    Node* rows[2] = { inv->find_unique("CharacterCards"), inv->find_unique("ColorlessCards") };
    int next[2] = { 0, 0 };
    const Json& cards = d["cards"];
    for (size_t i = 0; i < cards.size(); ++i) {
        int r = cards[i]["colorless"].b() ? 1 : 0;
        Node* row = rows[r];
        if (!row || next[r] >= (int)row->children.size()) continue;
        Control* slot = as_control(row->children[(size_t)next[r]++]);
        if (!slot) continue;
        bool stocked = cards[i]["is_stocked"].b(true);
        int cost = cards[i]["cost"].i();
        if (!stocked) {
            slot->visible = false;
            continue;
        }
        Node* holder = slot->find_unique("CardHolder");
        Node* card = make_card(lib_, cards[i]);
        if (holder && card) holder->add_child(card);
        else delete card;
        set_cost(slot, cost, cost <= gold, cards[i]["on_sale"].b());
        set_visible(slot->find_unique("SaleVisual"), cards[i]["on_sale"].b());
        Slot s = { slot, "card", cards[i]["index"].i((int)i), cost, true };
        slots_list_.push_back(s);
    }
    /* Card slots without an entry stay empty (hidden). */
    for (int r = 0; r < 2; ++r)
        for (size_t k = (size_t)next[r]; rows[r] && k < rows[r]->children.size(); ++k) hide(rows[r]->children[k]);
    struct Kind {
        const char* list;
        const char* box;
        const char* kind;
        const char* holder;
    };
    static const Kind kinds[2] = { { "relics", "Relics", "relic", "RelicHolder" },
                                   { "potions", "Potions", "potion", "PotionHolder" } };
    for (int k = 0; k < 2; ++k) {
        Node* box = inv->find_unique(kinds[k].box);
        const Json& items = d[kinds[k].list];
        for (size_t i = 0; box && i < box->children.size(); ++i) {
            Control* slot = as_control(box->children[i]);
            if (!slot) continue;
            if (i >= items.size() || !items[i]["is_stocked"].b(true) || !items[i]["id"].is_string()) {
                slot->visible = false;
                continue;
            }
            Node* holder = slot->find_unique(kinds[k].holder);
            const std::string& id = items[i]["id"].str();
            Node* icon = NULL;
            if (k == 0) {
                /* NRelic.Create (the small icon), in the 2x holder. */
                icon = instance(lib_, "scenes/relics/relic.tscn");
                if (icon) set_relic_icon(lib_, icon->find_unique("Icon"), relic_icon_base(items[i]));
            } else {
                icon = make_potion(lib_, id);
            }
            if (holder && icon) holder->add_child(icon);
            else delete icon;
            int cost = items[i]["cost"].i();
            set_cost(slot, cost, cost <= gold, false);
            Slot s = { slot, kinds[k].kind, items[i]["index"].i((int)i), cost, true };
            slots_list_.push_back(s);
        }
    }
    /* NMerchantCardRemoval: once used, no price (and no focus). */
    Control* removal = ctl(inv, "MerchantCardRemoval");
    if (removal) {
        if (d["card_removal_used"].b(d["card_removal_cost"].is_null())) {
            hide(removal->find_unique("CostLabel"));
            hide(removal->get_node("Cost"));
            /* NMerchantCardRemoval.UpdateVisual: the Used animation. */
            Node* anim = removal->find_unique("Animation");
            if (anim && anim->type == "AnimationPlayer") ((AnimationPlayer*)anim)->play("Used");
        } else {
            int cost = d["card_removal_cost"].i();
            set_cost(removal, cost, cost <= gold, false);
            Slot s = { removal, "removal", 0, cost, true };
            slots_list_.push_back(s);
        }
    }
    for (size_t i = 0; i < slots_list_.size(); ++i) {
        slots_list_[i].node->scale = Vec2(SMALL_SCALE, SMALL_SCALE);
    }
}

Node* ShopScreen::build(const Json& d, std::string* error) {
    decision_ = d;
    tweens_.clear();
    focus_ = -1;
    Node* room = instance(lib_, "scenes/rooms/merchant_room.tscn");
    if (!room) {
        if (error) *error = "no merchant_room.tscn";
        return NULL;
    }
    hide(room->find_unique("CardDetailScreen"));
    /* AfterRoomIsLoaded: the player (Character.MerchantAnimPath) in
     * %CharacterContainer. */
    std::string character = lower(d["player"]["character"].is_string() ? d["player"]["character"].str() : "ironclad");
    Node* box = room->find_unique("CharacterContainer");
    Node* who = instance(lib_, "scenes/merchant/characters/" + character + "_merchant.tscn");
    if (box && who) box->add_child(who);
    else delete who;
    play_idle_all(room);

    merchant_ = ctl(room, "MerchantButton");
    if (merchant_) {
        merchant_reticle_ = as_control(merchant_->get_node("MerchantSelectionReticle"));
        show_glyph(lib_, merchant_->find_unique("HotkeyIcon"), "a");
        if (!merchant_->find_unique("HotkeyIcon")) show_glyph(lib_, find_named(merchant_, "HotkeyIcon"), "a");
    }
    proceed_.attach(lib_, room->find_unique("ProceedButton"));
    proceed_.set_text(Loc::get("gameplay_ui", "PROCEED_BUTTON"));
    proceed_.set_pulse(false);

    inventory_ = ctl(room, "Inventory");
    if (inventory_) {
        backstop_ = as_control(inventory_->get_node("Backstop"));
        slots_ = ctl(inventory_, "SlotsContainer");
        back_button_ = ctl(inventory_, "BackButton");
        show_glyph(lib_, back_button_ ? find_named(back_button_, "HotkeyIcon") : NULL, "b");
        if (back_button_) back_button_->visible = false;
        hide(inventory_->get_node("MerchantHandContainer"));
        hide(inventory_->find_unique("Dialogue"));
        if (slots_) {
            /* Its top-left at y -1000 (closed) or 80 (open); anchored at the
             * screen's centre. */
            slots_height_ = slots_->offset[SIDE_BOTTOM] - slots_->offset[SIDE_TOP];
            slots_closed_top_ = CLOSED_Y - 540.0f;
            slots_open_top_ = OPEN_Y - 540.0f;
            slots_->offset[SIDE_TOP] = slots_closed_top_;
            slots_->offset[SIDE_BOTTOM] = slots_closed_top_ + slots_height_;
        }
        fill_slots(inventory_, d);
    }
    Node* root = room;
    root->add_child(frame_driver(this));
    open_ = false;
    if (start_open_ && inventory_) {
        open_inventory(false);
        int f = 0;
        for (size_t i = 0; i < slots_list_.size(); ++i)
            if (slots_list_[i].kind == start_kind_ && slots_list_[i].index == start_index_) f = (int)i;
        /* The bought one is gone: the slot nearest to where it was. */
        focus_slot(slots_list_.empty() ? -1 : f);
    } else {
        proceed_.enable(start_open_);
        focus_merchant(true);
    }
    root_ = root;
    return root;
}

/* NMerchantButton focused: its selection reticle. */
void ShopScreen::focus_merchant(bool on) {
    if (!merchant_reticle_) return;
    if (on) {
        tweens_.kill(&merchant_reticle_->modulate.a);
        merchant_reticle_->modulate.a = 1;
        merchant_reticle_->pivot_offset = offset_size(merchant_reticle_) * 0.5f;
        tweens_.tween_from(&merchant_reticle_->scale, Vec2(0.9f, 0.9f), Vec2(1, 1), 0.5f, TRANS_EXPO, EASE_OUT);
    } else {
        tweens_.tween(&merchant_reticle_->modulate.a, 0.0f, 0.2f, TRANS_SINE, EASE_OUT);
    }
}

/* NMerchantInventory.Open / DoOpenAnimation: the backstop to 0.8 (1 s),
 * the slots down to y 80 (0.7 s, Quint Out); the proceed arrow goes. */
void ShopScreen::open_inventory(bool animate) {
    open_ = true;
    /* _backButton.Enable / Disable with the inventory. */
    if (back_button_) back_button_->visible = true;
    focus_merchant(false);
    proceed_.disable(!animate);
    if (backstop_) {
        if (animate) tweens_.tween(&backstop_->modulate.a, 0.8f, 1.0f, TRANS_SINE, EASE_IN_OUT);
        else backstop_->modulate.a = 0.8f;
    }
    if (slots_) {
        if (animate) {
            tweens_.tween(&slots_->offset[SIDE_TOP], slots_open_top_, 0.7f, TRANS_QUART, EASE_OUT);
            tweens_.tween(&slots_->offset[SIDE_BOTTOM], slots_open_top_ + slots_height_, 0.7f, TRANS_QUART, EASE_OUT);
        } else {
            slots_->offset[SIDE_TOP] = slots_open_top_;
            slots_->offset[SIDE_BOTTOM] = slots_open_top_ + slots_height_;
        }
    }
}

/* Close: the backstop out (0.8 s), the slots up (0.5 s, Cubic Out); the
 * merchant and the proceed arrow (now pulsing) come back. */
void ShopScreen::close_inventory() {
    open_ = false;
    if (back_button_) back_button_->visible = false;
    focus_slot(-1);
    if (backstop_) tweens_.tween(&backstop_->modulate.a, 0.0f, 0.8f, TRANS_SINE, EASE_IN_OUT);
    if (slots_) {
        tweens_.tween(&slots_->offset[SIDE_TOP], slots_closed_top_, 0.5f, TRANS_CUBIC, EASE_OUT);
        tweens_.tween(&slots_->offset[SIDE_BOTTOM], slots_closed_top_ + slots_height_, 0.5f, TRANS_CUBIC, EASE_OUT);
    }
    proceed_.enable();
    proceed_.set_pulse(true);
    focus_merchant(true);
}

/* NMerchantSlot.OnFocus (the hover scale at once) / OnUnfocus (back over
 * 0.5 s, Expo Out). */
void ShopScreen::focus_slot(int i) {
    if (focus_ >= 0 && focus_ < (int)slots_list_.size() && focus_ != i) {
        Control* o = slots_list_[(size_t)focus_].node;
        tweens_.tween(&o->scale, Vec2(SMALL_SCALE, SMALL_SCALE), 0.5f, TRANS_EXPO, EASE_OUT);
        o->z_index = 0;
    }
    focus_ = i;
    if (i < 0 || i >= (int)slots_list_.size()) return;
    Control* n = slots_list_[(size_t)i].node;
    tweens_.kill(&n->scale);
    n->scale = Vec2(HOVER_SCALE, HOVER_SCALE);
    n->z_index = 1;
}

Vec2 ShopScreen::slot_centre(const Slot& s) const {
    /* The slots' origins are their centres (the hitboxes sit around them). */
    return s.node->global_transform().xform(Vec2(0, 0));
}

/* Godot's focus search, simplified: the nearest slot that way, favouring
 * ones straight ahead. */
int ShopScreen::nearest(int from, int dx, int dy) const {
    if (from < 0 || from >= (int)slots_list_.size()) return from;
    Vec2 p = slot_centre(slots_list_[(size_t)from]);
    int best = from;
    float best_score = 1e30f;
    for (size_t i = 0; i < slots_list_.size(); ++i) {
        if ((int)i == from) continue;
        Vec2 q = slot_centre(slots_list_[i]);
        float along = (q.x - p.x) * (float)dx + (q.y - p.y) * (float)dy;
        float across = fabsf((q.x - p.x) * (float)dy - (q.y - p.y) * (float)dx);
        if (along <= 10.0f) continue;
        float score = along + across * 2.0f;
        if (score < best_score) {
            best_score = score;
            best = (int)i;
        }
    }
    return best;
}

/* The inspect node is under root_, freed with it by the owner. */
ShopScreen::~ShopScreen() {
    delete inspect_card_;
    delete inspect_relic_;
}

void ShopScreen::close_inspect() {
    if (inspect_node_ && inspect_node_->parent) inspect_node_->parent->remove_child(inspect_node_);
    delete inspect_node_;
    delete inspect_card_;
    delete inspect_relic_;
    inspect_node_ = NULL;
    inspect_card_ = NULL;
    inspect_relic_ = NULL;
}

std::string ShopScreen::input(const PadState& pad) {
    if (inspect_card_ || inspect_relic_) {
        if (!(inspect_card_ ? inspect_card_->input(pad) : inspect_relic_->input(pad))) close_inspect();
        return std::string();
    }
    if (open_ && focus_ >= 0 && (pad.pressed & PadState::Y)) {
        /* NMerchantSlot._GuiInput: confirm previews (a card or a relic in
         * its inspect screen; the others nothing) and is handled */
        const Slot& s = slots_list_[(size_t)focus_];
        const char* key = s.kind == "card" ? "cards" : s.kind == "relic" ? "relics" : NULL;
        const Json* entry = NULL;
        for (size_t k = 0; key && k < decision_[key].size(); ++k)
            if (decision_[key][k]["index"].i((int)k) == s.index) entry = &decision_[key][k];
        if (entry && root_) {
            Json list = Json::make_array();
            list.append(*entry);
            std::string err;
            if (s.kind == "card") {
                inspect_card_ = new InspectCardScreen(lib_);
                inspect_node_ = inspect_card_->build(list, 0, false, &err);
            } else {
                inspect_relic_ = new InspectRelicScreen(lib_);
                inspect_node_ = inspect_relic_->build(list, 0, &err);
            }
            if (inspect_node_) root_->add_child(inspect_node_);
            else close_inspect();
        }
        return std::string();
    }
    if (!open_) {
        if ((pad.pressed & PadState::A) && inventory_) {
            open_inventory(true);
            if (!slots_list_.empty()) focus_slot(0);
            return std::string();
        }
        if ((pad.pressed & PadState::Y) && proceed_.enabled()) return protocol_action("leave_room");
        return std::string();
    }
    if (pad.pressed & PadState::B) {
        close_inventory();
        return std::string();
    }
    if (focus_ < 0) {
        if (!slots_list_.empty() && (pad.pressed & (PadState::LEFT | PadState::RIGHT | PadState::UP | PadState::DOWN)))
            focus_slot(0);
        return std::string();
    }
    if (pad.pressed & PadState::LEFT) focus_slot(nearest(focus_, -1, 0));
    if (pad.pressed & PadState::RIGHT) focus_slot(nearest(focus_, 1, 0));
    if (pad.pressed & PadState::UP) focus_slot(nearest(focus_, 0, -1));
    if (pad.pressed & PadState::DOWN) focus_slot(nearest(focus_, 0, 1));
    if (pad.pressed & PadState::A) {
        const Slot& s = slots_list_[(size_t)focus_];
        /* MerchantEntry.OnTryPurchase: not enough gold fails (the merchant
         * says so; here nothing happens). */
        if (s.cost > decision_["player"]["gold"].i()) return std::string();
        if (s.kind == "card") return protocol_action("buy_card", format("\"card_index\":%d", s.index));
        if (s.kind == "relic") return protocol_action("buy_relic", format("\"relic_index\":%d", s.index));
        if (s.kind == "potion") return protocol_action("buy_potion", format("\"potion_index\":%d", s.index));
        if (s.kind == "removal") return protocol_action("remove_card");
    }
    return std::string();
}

void ShopScreen::frame(double dt) {
    tweens_.update(dt);
    proceed_.frame(dt);
}

} /* namespace spire */
