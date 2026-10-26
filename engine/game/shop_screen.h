/* The shop (NMerchantRoom, scenes/rooms/merchant_room.tscn) from a shop
 * decision: the room with the merchant and the player, the proceed arrow
 * (Y leaves). A on the merchant (NMerchantButton) opens the inventory
 * (NMerchantInventory: two rows of cards, relics, potions and the card
 * removal service, with their prices); there the D-pad moves between the
 * slots (to the nearest one that way, as Godot's focus does), A buys, B
 * closes it.
 *
 * The client rebuilds the screen after each purchase: set_state keeps the
 * inventory open on the slot that was focused. */
#ifndef SPIRE_SHOP_SCREEN_H
#define SPIRE_SHOP_SCREEN_H

#include "../scene/tween.h"
#include "proceed_button.h"
#include "run_screen.h"
#include "ui_util.h"

namespace spire {

class Control;
class Node2D;
class SceneLibrary;

class ShopScreen : public RunScreen, public FrameListener {
public:
    explicit ShopScreen(SceneLibrary* lib);
    ~ShopScreen();
    /* Before build: the inventory open (no opening animation), the slot
     * to focus (its kind and index: "card" 3, "relic" 0, "removal" 0...). */
    void set_state(bool open, const std::string& kind, int index) {
        start_open_ = open;
        start_kind_ = kind;
        start_index_ = index;
    }
    /* The decision before was this room's too (a purchase, the card
     * removal): no welcome again. */
    void set_again(bool again) { again_ = again; }
    /* Behind the card removal's selection (NOverlayStack): its focused
     * slot shows no hover tip (NMerchantSlot.OnSelected cleared it). */
    void set_behind() { behind_ = true; }
    bool inventory_open() const { return open_; }
    /* NMerchantRoom.OpenInventory (the merchant tip's "Got it!"), as A on
     * the merchant does */
    void open_rug();
    /* Before build: the room buys by itself (LordsParasol.PurchaseEverything
     * with its input blocked): steps' NMerchantInventory.Open and auto
     * merchant_purchase cues play at their delays; busy until they have
     * played and the last item has flown. */
    void set_auto(const Json& steps);
    virtual bool busy() const;
    /* An item the auto purchases took from its slot since the last call
     * (kind: card, relic or potion; its id), for the top bar to follow. */
    bool take_bought(std::string* kind, std::string* id);
    std::string focused_kind() const;
    int focused_index() const;
    virtual Node* build(const Json& decision, std::string* error);
    virtual std::string input(const PadState& pad);
    virtual bool own_ui_sounds() const { return true; }
    virtual void frame(double dt);

private:
    struct Slot {
        Control* node;
        std::string kind; /* card, relic, potion, removal */
        int index;        /* the decision's index for buy_* */
        int cost;
        bool stocked;
    };
    void fill_slots(Node* inventory, const Json& d);
    /* NMerchantDialogue.ShowRandom (the inventory's %Dialogue): a random
     * line of its kind (openInventory, purchaseSuccess, purchaseFailureGold,
     * ...) from the merchant's table, or the Fake Merchant's */
    void say(const char* kind);
    std::string line(const char* kind) const;
    void sfx(const char* path);
    /* NMerchantSlot.OnPurchaseFailed: its visual wiggles (0.4 s), the
     * merchant's disappointment, a line of the failure */
    void purchase_failed(const Slot& s, const char* kind);
    /* NMerchant*.OnSuccessfulPurchase: the card flies to the deck, a relic
     * or potion to the top bar (from its slot); the thank-yous; the next
     * screen's success line */
    void purchased(const Slot& s);
    /* NMerchantButton.PlayDialogue: a speech bubble from the merchant */
    void merchant_says(const std::string& text, double seconds);
    bool fake_, again_;
    Node2D* dialogue_;
    Node2D* dialogue_box_;
    Node2D* dialogue_bubble_;
    class RichTextLabel* dialogue_text_;
    struct Wiggle {
        Node* node;
        float x0;
        double t;
    };
    std::vector<Wiggle> wiggles_;
    double welcome_at_; /* NFakeMerchant.ShowWelcomeDialogue, < 0: none */
    void open_inventory(bool animate);
    void close_inventory();
    void focus_slot(int i);
    void focus_merchant(bool on);
    /* NMerchantHand: PointAtTarget (follows the slot) / StopPointing (where
     * it is for `linger` s, then back to its start) */
    void point_hand(int slot);
    void stop_hand(double linger);
    int nearest(int from, int dx, int dy) const;
    Vec2 slot_centre(const Slot& s) const;
    /* The decision's entry of a card, relic or potion slot (NULL: the
     * removal, or none). */
    const Json* entry_of(const Slot& s) const;
    /* NMerchantSlot.CreateHoverTip for the focused slot (none behind the
     * removal's selection) */
    void show_slot_tips();
    TipFollower slot_tips_;
    SceneLibrary* lib_;
    Json decision_;
    Node* root_;
    class InspectCardScreen* inspect_card_; /* OnPreview (Y on a slot) */
    class InspectRelicScreen* inspect_relic_;
    Node* inspect_node_;
    void close_inspect();
    Control* inventory_;
    Control* backstop_;
    Control* slots_;
    Control* merchant_;
    Control* merchant_reticle_;
    Control* back_button_;
    ProceedButton proceed_;
    std::vector<Slot> slots_list_;
    int focus_;
    bool open_, start_open_, behind_;
    std::string start_kind_;
    int start_index_;
    float slots_closed_top_, slots_open_top_, slots_height_;
    TweenSet tweens_;
    class SpineSprite* hand_; /* %MerchantHand's parent, in the inventory */
    Vec2 hand_start_, hand_target_;
    int hand_slot_;           /* the slot pointed at, -1: none */
    double hand_linger_, hand_t_;
    struct AutoCue {
        double at;
        std::string kind; /* "open", or the slot's kind */
        int index;
    };
    std::vector<AutoCue> auto_;
    double auto_t_, auto_tail_;
    std::vector<std::pair<std::string, std::string> > bought_;
};

} /* namespace spire */

#endif
