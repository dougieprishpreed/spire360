#include "shop_screen.h"
#include "inspect_screens.h"
#include "card_view.h"
#include "combat_vfx.h"
#include "deck_previews.h"
#include "loc.h"
#include "screen_shake.h"
#include "top_bar.h"

#include "../audio/game_audio.h"
#include "../core/cue_ledger.h"
#include "../scene/animation.h"
#include "../scene/controls.h"
#include "../scene/node.h"
#include "../scene/rich_text.h"
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

/* A purchase's line for the screen the purchase's decision brings
 * (NMerchantInventory.OnPurchaseCompleted: ShowForPurchaseAttempt) */
static std::string g_say_next;

/* NSpineAutoPlayer / the characters' _Ready: the idle loop. */
static void play_idle_all(Node* n) {
    if (n->type == "SpineSprite") {
        SpineSprite* s = (SpineSprite*)n;
        if (!s->set_animation("idle_loop", true, 0)) s->set_animation("idle", true, 0);
    }
    for (size_t i = 0; i < n->children.size(); ++i) play_idle_all(n->children[i]);
}

ShopScreen::ShopScreen(SceneLibrary* lib)
    : fake_(false), again_(false), dialogue_(NULL), dialogue_box_(NULL), dialogue_bubble_(NULL), dialogue_text_(NULL),
      welcome_at_(-1), lib_(lib), root_(NULL), inspect_card_(NULL), inspect_relic_(NULL), inspect_node_(NULL),
      inventory_(NULL), backstop_(NULL), slots_(NULL), merchant_(NULL), merchant_reticle_(NULL), back_button_(NULL),
      focus_(-1), open_(false), start_open_(false), behind_(false), start_index_(0), slots_closed_top_(0),
      slots_open_top_(0), slots_height_(0), hand_(NULL), hand_slot_(-1), hand_linger_(-1), hand_t_(0), auto_t_(0),
      auto_tail_(0) {}

void ShopScreen::set_auto(const Json& steps) {
    auto_.clear();
    auto_t_ = 0;
    auto_tail_ = 0;
    /* a cue's time: the waits before it, then its own delay (a task the
     * game runs alongside; resumed after a selection, it waits inline) */
    double waited = 0;
    for (size_t i = 0; i < steps.size(); ++i)
        for (size_t k = 0; k < steps[i].size(); ++k) {
            const Json& c = steps[i][k];
            if (c["t"].str() == "wait") waited += c["ms"].num() / 1000.0;
            AutoCue a = { waited + c["delay"].num(0) / 1000.0, std::string(), c["index"].i() };
            if (c["t"].str() == "ui" && c["call"].str() == "NMerchantInventory.Open") a.kind = "open";
            else if (c["t"].str() == "merchant_purchase" && c["auto"].b() && c["kind"].str() != "removal") a.kind = c["kind"].str();
            else continue;
            CueLedger::used(c);
            /* in time order (the cues' delays), stable */
            size_t at = auto_.size();
            while (at > 0 && auto_[at - 1].at > a.at) --at;
            auto_.insert(auto_.begin() + (long)at, a);
        }
}

bool ShopScreen::busy() const { return !auto_.empty() || auto_tail_ > 0; }

bool ShopScreen::take_bought(std::string* kind, std::string* id) {
    if (bought_.empty()) return false;
    *kind = bought_.front().first;
    *id = bought_.front().second;
    bought_.erase(bought_.begin());
    return true;
}

std::string ShopScreen::line(const char* kind) const {
    /* MerchantDialogueSet.CreateFromLocStrings: <prefix><kind>.lineN */
    const char* table = fake_ ? "events" : "merchant_room";
    std::string prefix = std::string(fake_ ? "FAKE_MERCHANT" : "MERCHANT") + ".talk." + kind + ".line";
    std::vector<std::string> lines;
    for (int n = 1; n < 32; ++n) {
        std::string key = prefix + format("%d", n);
        std::string text = Loc::get(table, key.c_str());
        if (text == std::string(table) + "." + key) break;
        lines.push_back(text);
    }
    return lines.empty() ? std::string() : lines[chaotic_rand() % lines.size()];
}

void ShopScreen::sfx(const char* path) {
    if (!lib_->audio()) return;
    Json cue;
    cue.set("t", Json::make_string("sfx"));
    cue.set("op", Json::make_string("play"));
    cue.set("path", Json::make_string(path));
    cue.set("volume", Json::make_number(1));
    lib_->audio()->cue(cue);
}

void ShopScreen::say(const char* kind) {
    std::string text = line(kind);
    if (!dialogue_ || text.empty()) return;
    /* ShowRandom: fades in (0.25 s) growing to 1 (Back Out), the text
     * typed in 0.4 s, the bubble from 0.25 to 0.75 (0.5 s, Expo Out), the
     * box down from -80 (0.5 s, Back Out); 1 s later out (0.5 s, Sine Out);
     * at a random x in 450..1450 */
    if (dialogue_text_) set_label(dialogue_text_, "[fly_in]" + text + "[/fly_in]");
    dialogue_->position.x = 450.0f + (float)(chaotic_rand() % 1001);
    tweens_.kill(&dialogue_->modulate.a);
    dialogue_->modulate.a = 0;
    tweens_.tween(&dialogue_->modulate.a, 1.0f, 0.25f);
    tweens_.tween(&dialogue_->scale, Vec2(1, 1), 0.25f, TRANS_BACK, EASE_OUT);
    if (dialogue_text_) tweens_.tween_from(&dialogue_text_->visible_ratio, 0.0f, 1.0f, 0.4f);
    if (dialogue_bubble_)
        tweens_.tween_from(&dialogue_bubble_->scale, Vec2(0.25f, 0.25f), Vec2(0.75f, 0.75f), 0.5f, TRANS_EXPO, EASE_OUT);
    if (dialogue_box_) tweens_.tween_from(&dialogue_box_->position.y, -80.0f, 0.0f, 0.5f, TRANS_BACK, EASE_OUT);
    tweens_.tween(&dialogue_->modulate.a, 0.0f, 0.5f, TRANS_SINE, EASE_OUT, 1.5f);
}

void ShopScreen::merchant_says(const std::string& text, double seconds) {
    /* NSpeechBubbleVfx.Create(line, Right, GlobalPosition + Size.X *
     * Left, seconds, Blue) in the button's parent */
    if (!merchant_ || !merchant_->parent || text.empty()) return;
    Vec2 at = merchant_->global_transform().origin - Vec2(merchant_->size.x, 0);
    speech_bubble_at(lib_, merchant_->parent, text, at, true, seconds, "Blue");
}

void ShopScreen::purchase_failed(const Slot& s, const char* kind) {
    static const char* const visuals[] = { "CardHolder", "RelicHolder", "PotionHolder", "Visual" };
    Node* v = NULL;
    for (size_t i = 0; i < sizeof(visuals) / sizeof(visuals[0]) && !v; ++i) v = s.node->find_unique(visuals[i]);
    if (v && (v->is_node2d() || v->is_control())) {
        bool known = false;
        for (size_t i = 0; i < wiggles_.size(); ++i)
            if (wiggles_[i].node == v) {
                wiggles_[i].t = 0;
                known = true;
            }
        if (!known) {
            Wiggle w = { v, v->is_node2d() ? ((Node2D*)v)->position.x : offset_pos((Control*)v).x, 0 };
            wiggles_.push_back(w);
        }
    }
    sfx("event:/sfx/npcs/merchant/merchant_dissapointment");
    say(kind);
}

void ShopScreen::purchased(const Slot& s) {
    sfx("event:/sfx/npcs/merchant/merchant_thank_yous");
    g_say_next = "purchaseSuccess";
    const char* key = s.kind == "card" ? "cards" : s.kind == "relic" ? "relics" : s.kind == "potion" ? "potions" : NULL;
    const Json* entry = NULL;
    for (size_t k = 0; key && k < decision_[key].size(); ++k)
        if (decision_[key][k]["index"].i((int)k) == s.index) entry = &decision_[key][k];
    if (!entry) return;
    const char* holder_name = s.kind == "card" ? "CardHolder" : s.kind == "relic" ? "RelicHolder" : "PotionHolder";
    Node* holder = s.node->find_unique(holder_name);
    Node* item = holder && !holder->children.empty() ? holder->children[0] : NULL;
    if (!item || !item->is_canvas_item()) return;
    Xform2D g = ((CanvasItem*)item)->global_transform();
    float scale = sqrtf(g.x.x * g.x.x + g.x.y * g.x.y);
    if (s.kind == "card") {
        /* NGlobalUi.ReparentCard, NCardFlyVfx to the deck */
        if (DeckPreviews::current())
            DeckPreviews::current()->fly_to_deck(*entry, g.origin.x, g.origin.y, scale, decision_["player"]["character"].str());
        item->parent->remove_child(item);
        delete item;
    } else {
        /* RelicInventory.AnimateRelic / PotionContainer.AnimatePotion from
         * the slot (the next top bar's) */
        top_bar_acquire_from(s.kind, (*entry)["id"].str(), g.origin, scale);
    }
}

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
    /* FakeMerchant (an event): NFakeMerchant's room, a merchant's */
    fake_ = d["fake_merchant"].b();
    Node* room = instance(lib_, fake_ ? "scenes/events/custom/fake_merchant.tscn" : "scenes/rooms/merchant_room.tscn");
    if (!room) {
        if (error) *error = "no merchant room scene";
        return NULL;
    }
    hide(room->find_unique("CardDetailScreen"));
    /* AfterRoomIsLoaded: the player (Character.MerchantAnimPath) in
     * %CharacterContainer; the Fake Merchant's, its combat visuals in
     * relaxed_loop (StartCharacterAnimation) */
    std::string character = lower(d["player"]["character"].is_string() ? d["player"]["character"].str() : "ironclad");
    Node* box = room->find_unique("CharacterContainer");
    Node* who = instance(lib_, fake_ ? "scenes/creature_visuals/" + character + ".tscn"
                                     : "scenes/merchant/characters/" + character + "_merchant.tscn");
    if (box && who) box->add_child(who);
    else delete who;
    play_idle_all(room);
    if (fake_ && who) {
        std::vector<Node*> stack(1, who);
        while (!stack.empty()) {
            Node* n = stack.back();
            stack.pop_back();
            if (n->type == "SpineSprite") ((SpineSprite*)n)->set_animation("relaxed_loop", true, 0);
            for (size_t k = 0; k < n->children.size(); ++k) stack.push_back(n->children[k]);
        }
    }

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
        /* NMerchantHand._Ready: the "default" animation where the scene
         * puts it (off the top) */
        Node* hand = inventory_->get_node("MerchantHandContainer");
        hand_ = hand && hand->type == "SpineSprite" ? (SpineSprite*)hand : NULL;
        if (hand_) {
            hand_->set_animation("default", true, 0);
            hand_start_ = hand_target_ = hand_->position;
        }
        /* NMerchantDialogue._Ready: transparent until it speaks */
        Node* dl = inventory_->find_unique("Dialogue");
        dialogue_ = dl && dl->is_node2d() ? (Node2D*)dl : NULL;
        if (dialogue_) {
            dialogue_->modulate.a = 0;
            Node* db = dialogue_->find_unique("DialogueBox");
            Node* bb = dialogue_->find_unique("Bubble");
            Node* tx = dialogue_->find_unique("Text");
            dialogue_box_ = db && db->is_node2d() ? (Node2D*)db : NULL;
            dialogue_bubble_ = bb && bb->is_node2d() ? (Node2D*)bb : NULL;
            dialogue_text_ = tx && tx->type == "RichTextLabel" ? (RichTextLabel*)tx : NULL;
        }
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
    shake_target_ = room->is_control() ? (Control*)room : NULL; /* NMerchantRoom */
    /* the purchase's line, after its decision */
    if (!g_say_next.empty() && again_) say(g_say_next.c_str());
    g_say_next.clear();
    /* NFakeMerchant.ShowWelcomeDialogue: 0.75 s in, its laugh and a line */
    welcome_at_ = fake_ && !again_ ? 0.75 : -1;
    /* FoulPotion.OnUse at the merchant: NMerchantRoom.FoulPotionThrown (the
     * thank-yous, a foulPotion line, a medium rumble) and ShowPotionVfx
     * (vfx_slime_impact at the merchant button) */
    const Json& steps = d["steps"];
    for (size_t i = 0; i < steps.size(); ++i)
        for (size_t k = 0; k < steps[i].size(); ++k) {
            if (steps[i][k]["t"].str() != "foul_potion" || !merchant_) continue;
            Node* fx = instance(lib_, "scenes/vfx/vfx_slime_impact.tscn");
            if (fx && fx->is_node2d() && merchant_->parent && merchant_->parent->is_canvas_item()) {
                merchant_->parent->add_child(fx);
                ((Node2D*)fx)->position = inverse(((CanvasItem*)merchant_->parent)->global_transform()).xform(merchant_->global_transform().origin);
            } else {
                delete fx;
            }
            sfx("event:/sfx/npcs/merchant/merchant_thank_yous");
            merchant_says(line("foulPotion"), 2.0);
            ScreenShake::get().rumble("Medium", "Short", false);
        }
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
    /* NMerchantInventory.Open: the welcome and a line (not when the screen
     * comes back open after a purchase) */
    if (animate) {
        sfx("event:/sfx/npcs/merchant/merchant_welcome");
        say("openInventory");
    }
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
    stop_hand(0); /* NMerchantInventory.Close */
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
        stop_hand(2); /* OnMerchantHandUnhovered */
    }
    focus_ = i;
    if (i < 0 || i >= (int)slots_list_.size()) return;
    point_hand(i); /* OnMerchantHandHovered */
    Control* n = slots_list_[(size_t)i].node;
    tweens_.kill(&n->scale);
    n->scale = Vec2(HOVER_SCALE, HOVER_SCALE);
    n->z_index = behind_ ? 0 : 1;
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

void ShopScreen::open_rug() {
    if (open_ || !inventory_) return;
    open_inventory(true);
    if (!slots_list_.empty()) focus_slot(0);
}

std::string ShopScreen::input(const PadState& pad) {
    if (busy()) return std::string(); /* NMerchantInventory.BlockInput */
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
            open_rug();
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
        /* MerchantEntry.OnTryPurchase: not enough gold (FailureGold), a full
         * potion belt (FailureSpace) fails where it is */
        if (s.cost > decision_["player"]["gold"].i()) {
            purchase_failed(s, "purchaseFailureGold");
            return std::string();
        }
        if (s.kind == "potion" && decision_["player"]["potions"].size() >= (size_t)decision_["player"]["max_potions"].i(3)) {
            purchase_failed(s, "purchaseFailureSpace");
            return std::string();
        }
        if (s.kind != "removal") purchased(s);
        if (s.kind == "card") return protocol_action("buy_card", format("\"card_index\":%d", s.index));
        if (s.kind == "relic") return protocol_action("buy_relic", format("\"relic_index\":%d", s.index));
        if (s.kind == "potion") return protocol_action("buy_potion", format("\"potion_index\":%d", s.index));
        if (s.kind == "removal") return protocol_action("remove_card");
    }
    return std::string();
}

void ShopScreen::point_hand(int slot) {
    hand_slot_ = slot;
    hand_linger_ = -1;
}

void ShopScreen::stop_hand(double linger) {
    hand_slot_ = -1;
    hand_linger_ = linger;
}

void ShopScreen::frame(double dt) {
    tweens_.update(dt);
    proceed_.frame(dt);
    if (auto_tail_ > 0) auto_tail_ -= dt;
    if (!auto_.empty()) {
        /* LordsParasol.PurchaseEverything: Open after 0.75 s; each item
         * 0.25 s apart, NMerchantInventory.OnPurchaseCompleted (the thank-
         * yous, a purchaseSuccess line) with the slot's own flight */
        auto_t_ += dt;
        while (!auto_.empty() && auto_.front().at <= auto_t_) {
            AutoCue c = auto_.front();
            auto_.erase(auto_.begin());
            if (auto_.empty()) auto_tail_ = 0.6; /* the last item's flight */
            if (c.kind == "open") {
                if (!open_) open_inventory(true);
                continue;
            }
            for (size_t i = 0; i < slots_list_.size(); ++i) {
                Slot& s = slots_list_[i];
                if (s.kind != c.kind || s.index != c.index || !s.stocked) continue;
                const char* key = s.kind == "card" ? "cards" : s.kind == "relic" ? "relics" : "potions";
                std::string id;
                for (size_t k = 0; k < decision_[key].size(); ++k)
                    if (decision_[key][k]["index"].i((int)k) == s.index) id = decision_[key][k]["id"].str();
                purchased(s);
                g_say_next.clear();
                say("purchaseSuccess");
                s.stocked = false;
                s.node->visible = false;
                bought_.push_back(std::make_pair(s.kind, id));
                break;
            }
        }
    }
    /* WiggleAnimation: x + sin(p * 2 pi) * 10, p 0..2 over 0.4 s (Quad Out) */
    for (size_t i = 0; i < wiggles_.size();) {
        Wiggle& w = wiggles_[i];
        w.t += dt;
        float k = tween_interpolate(TRANS_QUAD, EASE_OUT, (float)(w.t < 0.4 ? w.t : 0.4), 0, 2, 0.4f);
        float x = w.x0 + sinf(k * 6.2831853f) * 10.0f;
        if (w.node->is_node2d()) ((Node2D*)w.node)->position.x = x;
        else {
            Control* c = (Control*)w.node;
            c->set_position(Vec2(x, offset_pos(c).y));
        }
        if (w.t >= 0.4) wiggles_.erase(wiggles_.begin() + (long)i);
        else ++i;
    }
    if (welcome_at_ >= 0 && (welcome_at_ -= dt) < 0) {
        sfx("event:/sfx/npcs/reverse_merchant/reverse_merchant_laugh");
        merchant_says(line("welcome"), 4.0);
    }
    if (hand_ && inventory_) {
        /* NMerchantHand._Process: towards the slot (its origin) with a
         * Perlin wobble, lerped at 4 per second; the wrist turns -10..10
         * degrees with its place across the rug */
        if (hand_slot_ >= 0 && hand_slot_ < (int)slots_list_.size())
            hand_target_ = inverse(inventory_->global_transform()).xform(slot_centre(slots_list_[(size_t)hand_slot_]));
        else if (hand_linger_ >= 0 && (hand_linger_ -= dt) <= 0) {
            hand_linger_ = -1;
            hand_target_ = hand_start_;
        }
        hand_t_ += dt;
        Vec2 wobble(fbm((float)hand_t_ * 0.1f, 1.0f, 0) + 0.4f, fbm((float)(hand_t_ + 0.25) * 0.1f, 1.0f, 0) - 0.5f);
        Vec2 to = hand_target_ + wobble * 100.0f;
        hand_->position = hand_->position + (to - hand_->position) * (float)(dt * 4.0);
        float w = (hand_->position.x - inventory_->size.x * 0.5f - 50.0f) * 0.01f;
        hand_->set_bone_rotation("rotate_me", -10.0f + 20.0f * w);
    }
}

} /* namespace spire */
