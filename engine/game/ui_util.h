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
 * description), stacked 5 px apart; NULL when there are none. */
Control* make_tip_set(SceneLibrary* lib, const Json& tips);
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
/* An InspectPrompt (the card selection screens'): Y and "to inspect". */
void setup_inspect_prompt(SceneLibrary* lib, Node* prompt);

/* A card's hover tips beside it (NHoverTipSet.SetAlignmentForCardHolder):
 * right of the card, left of it past three quarters of the screen or when
 * they would leave it; following the card (its holder's origin is the
 * card's centre). The tips are nodes of the screen's tree. */
class TipFollower {
public:
    TipFollower() : tips_(NULL), owner_(NULL), card_(true), below_(false), left_(false), extra_(0) {}
    void show_for_card(SceneLibrary* lib, Node* parent, Control* holder, const Json& tips);
    /* NHoverTipSet.CreateAndShow(owner, tips, GetHoverTipAlignment(owner))
     * with SetFollowOwner: right of the owner, or left of it past 3/4 of
     * the screen; extra_x: SetExtraFollowOffset's x. */
    void show_for_control(SceneLibrary* lib, Node* parent, Control* owner, const Json& tips, float extra_x = 0);
    /* HoverTipAlignment.Center set at the owner's position plus 1.5 of its
     * height down (NPotionHolder.OnFocus). */
    void show_below(SceneLibrary* lib, Node* parent, Control* owner, const Json& tips);
    /* HoverTipAlignment.Left: the tips' right edge at the owner's left. */
    void show_left(SceneLibrary* lib, Node* parent, Control* owner, const Json& tips);
    void hide();
    /* Each frame (after the layout). */
    void frame();

private:
    Control* tips_;
    Control* owner_;
    bool card_, below_, left_;
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
