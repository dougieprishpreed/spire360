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
    bool inventory_open() const { return open_; }
    std::string focused_kind() const;
    int focused_index() const;
    virtual Node* build(const Json& decision, std::string* error);
    virtual std::string input(const PadState& pad);
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
    void open_inventory(bool animate);
    void close_inventory();
    void focus_slot(int i);
    void focus_merchant(bool on);
    int nearest(int from, int dx, int dy) const;
    Vec2 slot_centre(const Slot& s) const;
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
    bool open_, start_open_;
    std::string start_kind_;
    int start_index_;
    float slots_closed_top_, slots_open_top_, slots_height_;
    TweenSet tweens_;
};

} /* namespace spire */

#endif
