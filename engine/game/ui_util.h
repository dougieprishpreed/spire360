/* Small helpers the ported screens share: scene instances, lookups, labels,
 * pad glyphs, placing controls. */
#ifndef SPIRE_UI_UTIL_H
#define SPIRE_UI_UTIL_H

#include "../core/base.h"
#include "../core/json.h"
#include "../scene/script.h"

namespace spire {

class Control;
class Node;
class SceneLibrary;

std::string lower(std::string s);
/* A scene instance; logs and returns NULL if it cannot be made. */
Node* instance(SceneLibrary* lib, const std::string& path);
/* The first node named `name` under n (depth first, n included). */
Node* find_named(Node* n, const char* name);
/* %Unique under scope, if it is a Control. */
Control* ctl(Node* scope, const char* unique);
/* Label / RichTextLabel text. */
void set_label(Node* n, const std::string& text);
/* A TextureRect's or NinePatchRect's texture from a pack path ("" or a
 * missing one: none). */
void set_texture(SceneLibrary* lib, Node* rect, const std::string& path);
/* Logs a pack path the screens wanted but the pack lacks (once each). */
void warn_missing(const std::string& path);
/* A canvas item shown or hidden (NULL: nothing). */
void set_visible(Node* n, bool visible);
inline void hide(Node* n) { set_visible(n, false); }
/* NHotkeyIcon.UpdateInput with a controller: the xbox_360 glyph shown
 * (controller_atlas sprites: "a", "b", "y", "lt", ...), the key hidden. */
void show_glyph(SceneLibrary* lib, Node* hotkey_icon, const char* glyph);
/* A control's offsets as its rect (anchors together). */
void place(Control* c, float x, float y, float w, float h);
Vec2 offset_pos(const Control* c);
Vec2 offset_size(const Control* c);
Color hex(unsigned rgb);
/* Rng.Chaotic: a different number every time (seeded from the clock). */
unsigned chaotic_rand();
/* MathHelper.SmoothDamp: current towards target over about smooth_time
 * seconds; *velocity carries over from frame to frame. */
float smooth_damp(float current, float target, float* velocity, float smooth_time, float dt);
/* Godot's Transform2D.affine_inverse. */
Xform2D inverse(const Xform2D& t);
/* NHoverTipSet's text tip (scenes/ui/hover_tip.tscn): a title (none:
 * "") and a description (BBCode); NULL without the scene. Place it with
 * place() or set_position on the screen's root. */
Control* make_hover_tip(SceneLibrary* lib, const std::string& title, const std::string& description);
/* NHoverTipSet's text tips for a model (a decision's "tips": title,
 * description), stacked 5 px apart, and the cards they name
 * (NHoverTipCardContainer), 4 px apart; NULL when there are none. With
 * both: a row of the two, or (split) a plain control holding them that
 * place_tip_parts places part by part. */
Control* make_tip_set(SceneLibrary* lib, const Json& tips, bool split = false);
/* A tip set's text and card parts (NULL when it has none; a set with one
 * kind is that part). */
void tip_parts(Control* set, Control** text, Control** cards);
/* Each part's top-left on the screen (its parent's space must be the
 * screen's, unscaled). */
void place_tip_parts(Control* set, const Vec2& text_at, const Vec2& cards_at);
/* NHoverTipSet.SetAlignmentForCardHolder (gap 10) and SetAlignment(node,
 * Right / Left) (gap 0): the text tips on one side of the owner (at: its
 * top-left on the screen, width: its width there), the cards on the other
 * (Right: text right of it), then CorrectVerticalOverflow and
 * CorrectHorizontalOverflow. */
void place_tips_beside(Control* set, const Vec2& at, float width, bool left, float gap);
/* NHoverTipSet.SetAlignmentForRelic: the text under the relic's icon (at:
 * its top-left on the screen, icon: its size there; past 3/4 of the screen
 * its right edge at the icon's), the cards under the text, above the relic
 * low on the screen. */
void place_tips_for_relic(Control* set, const Vec2& at, const Vec2& icon);
/* The settings the logic last answered (NSettingsScreen's values: PrefsSave
 * and SettingsSave), for the screens that follow them; null before. */
const Json& client_settings();
void set_client_settings(const Json& s);
/* PrefsSave.FastMode is Fast (the settings' Fast mode tickbox): the game's
 * screens take their shorter times. */
bool fast_mode();
/* A capstone screen (pause, settings, deck or piles) or the map is open
 * over the run: NRunTimer shows then. */
bool capstone_open();
void set_capstone_open(bool open);

/* A full-screen Control (anchors 0..1), named. */
Control* full_rect_control(const char* name);
/* An overlay's background (the room as it is) in a CanvasLayer under the
 * default canvas: nothing in the room (its CombatUi's absolute z: a
 * focused hand card, a played card) is drawn over the overlay, which the
 * game keeps on a canvas layer above the room. */
Node* overlay_behind(Node* background);
/* An InspectPrompt (the card selection screens'): Y and "to inspect". */
void setup_inspect_prompt(SceneLibrary* lib, Node* prompt);

/* A card's hover tips beside it (NHoverTipSet.SetAlignmentForCardHolder):
 * right of the card, left of it past three quarters of the screen or when
 * they would leave it; following the card (its holder's origin is the
 * card's centre). The tips are nodes of the screen's tree. */
class TipFollower {
public:
    TipFollower()
        : tips_(NULL), owner_(NULL), slot_(NULL), card_(true), below_(false), left_(false), relic_(false), merchant_(0),
          extra_(0) {}
    void show_for_card(SceneLibrary* lib, Node* parent, Control* holder, const Json& tips);
    /* NHoverTipSet.CreateAndShow(owner, tips, GetHoverTipAlignment(owner))
     * with SetFollowOwner: right of the owner, or left of it past 3/4 of
     * the screen; extra_x: SetExtraFollowOffset's x. */
    void show_for_control(SceneLibrary* lib, Node* parent, Control* owner, const Json& tips, float extra_x = 0);
    /* HoverTipAlignment.Center set at the owner's position plus 1.5 of its
     * height down (NPotionHolder.OnFocus). */
    void show_below(SceneLibrary* lib, Node* parent, Control* owner, const Json& tips);
    /* NRelicInventoryHolder.OnFocus: SetAlignmentForRelic, the owner the
     * relic's icon. */
    void show_for_relic(SceneLibrary* lib, Node* parent, Control* icon, const Json& tips);
    /* HoverTipAlignment.Left: the tips' right edge at the owner's left. */
    void show_left(SceneLibrary* lib, Node* parent, Control* owner, const Json& tips);
    /* NMerchantRelic / NMerchantPotion / NMerchantCardRemoval.CreateHoverTip:
     * past the screen's middle, the tips' right edge at the slot, then up
     * and left by half the slot; else right of it (the removal: from its
     * position), then right and up by half the slot. */
    void show_for_merchant_slot(SceneLibrary* lib, Node* parent, Control* slot, const Json& tips, bool removal);
    /* NMerchantCard.CreateHoverTip: SetAlignment(hitbox, the slot's
     * GetHoverTipAlignment): right of the hitbox (its unscaled width), or
     * left of it with the slot past 3/4 of the screen. */
    void show_for_merchant_card(SceneLibrary* lib, Node* parent, Control* slot, Control* hitbox, const Json& tips);
    void hide();
    /* Each frame (after the layout). */
    void frame();

private:
    void attach(SceneLibrary* lib, Node* parent, Control* owner, const Json& tips);
    Control* tips_;
    Control* owner_;
    Control* slot_; /* show_for_merchant_card: the slot that picks the side */
    bool card_, below_, left_, relic_;
    int merchant_; /* 0, 1: relic or potion, 2: card removal, 3: card */
    float extra_;
};

/* Something run each frame by the scene tree (a screen's animations). */
class FrameListener {
public:
    virtual ~FrameListener() {}
    virtual void frame(double dt) = 0;
};
/* A plain Node (always processed) that calls listener->frame each frame;
 * add it to the screen's tree. The listener must outlive the tree. */
Node* frame_driver(FrameListener* listener);

} /* namespace spire */

#endif
