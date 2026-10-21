/* The combat screen, built from a combat_play decision the way the game's
 * NCombatRoom builds it: the room scene, the background the game picked
 * (view.background: the scene plus its layers), the encounter's camera, and
 * one creature scene per combatant with its visuals
 * (scenes/creature_visuals/<id>.tscn), placed by NCombatRoom's
 * PositionEnemies / PositionPlayersAndPets (ported), with each creature's
 * hitbox, intents and state display (NCreature.UpdateBounds,
 * NCreatureStateDisplay, NHealthBar: ported, driven by the decision's hp,
 * block and powers). */
#ifndef SPIRE_COMBAT_SCREEN_H
#define SPIRE_COMBAT_SCREEN_H

#include "../core/base.h"
#include "../core/json.h"
#include "../scene/tween.h"
#include "combat_vfx.h"

#include <map>

namespace spire {

class Node;
class Node2D;
class Control;
class CanvasItem;
class SceneLibrary;
class SpineSprite;

/* NCreature.Create: creature.tscn with the visuals (*visuals) first. */
Control* instance_creature(SceneLibrary* lib, const std::string& visuals_id, Node** visuals);
/* NCreature.UpdateBounds: the hitbox in the creature's coordinates. */
Rect2 creature_hitbox(Node* visuals);
/* NCreatureVisuals.SetUpSkin + UpdatePhobiaMode from the logic's view
 * (spine_ops, phobia_skin), after the idle starts. */
void setup_creature_spine(Node* visuals, const Json& view);
/* What the creature's CreatureAnimator played (the logic's ops: set_animation
 * / add_animation, in order), stopping at the first animation the skeleton
 * lacks (SetNextState / AddNextState return there); true when the first
 * one played. */
bool play_animator_ops(SpineSprite* s, const Json& ops);
/* NCreature.Create + SetupForBestiary: a monster outside a fight (view: id,
 * spine_ops, phobia_skin), idle, its state display and intents hidden. */
Control* make_display_creature(SceneLibrary* lib, const Json& view, Node** visuals);

class CombatScreen : private VfxAnchors {
public:
    explicit CombatScreen(SceneLibrary* lib);
    /* The room after the fight (behind the rewards): the background and
     * the player only (as `player`, a later decision's summary, has them),
     * no enemies, hand or combat UI. Set before build. */
    void set_aftermath(const Json& player) {
        aftermath_ = true;
        aftermath_player_ = player;
    }
    /* Before build: the fight's last decision (its enemies that are gone
     * now stay while they die), where each enemy stands (by combat id:
     * kept for the whole fight, NCombatRoom places them once), the room
     * shown only for the killing blow (no hand or combat UI). */
    void set_previous(const Json& decision) { previous_ = decision; }
    void set_layout_memory(std::map<int, Vec2>* places) { places_ = places; }
    void set_final() { final_ = true; }
    /* NCombatRoom VisualOnly: event scenery, with inactive creature UI. */
    void set_visual_only() { visual_only_ = final_ = true; }
    /* A new node tree for the decision (the caller owns it); NULL on error. */
    Node* build(const Json& decision, std::string* error);
    /* NCombatRoom lives for the fight. Apply a response to that room;
     * state changes are presented when their steps reach the playhead. */
    /* After the setting changed (NGame.PhobiaModeToggled). */
    void update_phobia_mode();
    void update_decision(const Json& decision, bool finishing = false);
    void show_aftermath(const Json& player);
    Node* root_node() const { return root_; }
    /* The controller's focused card in the hand (-1: the middle one); set
     * before build, or after it to move the focus (relays the hand). */
    void set_hand_focus(int index);
    int hand_focus() const { return hand_focus_; }
    int hand_size() const { return (int)holders_.size(); }
    /* Playing a card with the pad (NControllerCardPlay): the card at hand
     * position `card` goes to the centre; a single-enemy card
     * (target_type AnyEnemy) aims the targeting arrow at enemy `target`
     * (its index in the decision's enemies), other cards mark what they
     * hit (all enemies, the player). A then plays it, B: end_play().
     * set_play_target: false if that enemy cannot be aimed at (not shown). */
    void begin_play(int card, const std::string& target_type, int target);
    bool set_play_target(int enemy);
    void end_play();
    bool playing() const { return play_card_ >= 0; }
    /* A potion aimed from `from` (screen point) at the same targets; ended
     * by end_play. */
    void begin_aim(const Vec2& from, const std::string& target_type, int target);
    bool aiming() const { return aiming_; }
    /* Each frame (the screen's root calls it): the cards' moves, the
     * reticles' and nameplates' tweens, the arrow. */
    void process(double dt);
    /* NEndTurnButton with long press confirmation: pressed starts the hold,
     * releasing cancels it (NEndTurnLongPressBar). */
    void hold_end_turn(bool pressed, bool held);
    /* The bar filled (0.5 s held) since last asked: the turn is to end. */
    bool take_end_turn_long_press() {
        bool done = long_press_done_;
        long_press_done_ = false;
        return done;
    }
    /* After build: what happened since the decision before (its "steps"),
     * played back over the room as the game shows it: the creatures'
     * animations, the effects (VfxCmd), damage numbers, deaths. */
    void play_steps(const Json& steps);
    /* The fight's first turn: "Battle Start", then the player's turn
     * banner (NCombatStartBanner). */
    void show_start_banner();
    /* The playback is running (the pad waits for it; banners do not
     * count). */
    bool busy() const { return cue_t_ < busy_until_; }

private:
    struct Creature {
        Control* node;  /* the creature scene (NCreature) */
        Node* visuals;  /* its NCreatureVisuals */
        float width;    /* Visuals.Bounds.Size.X */
        bool player;
        Rect2 hitbox;            /* in the creature's coordinates */
        float hp_bar_reduction;  /* MonsterModel.HpBarSizeReduction */
        float max_fg_width;      /* NHealthBar._expectedMaxFgWidth */
        Control* reticle;        /* %SelectionReticle */
        Control* nameplate;      /* the state display's %NameplateContainer */
        Node* center;            /* the visuals' %CenterPos (VfxSpawnPosition) */
        Json data, powers;       /* state currently shown, before the next cue */
        int shown_hp, shown_max_hp;
        /* NCreature's scale: Visuals.Scale = base * _tempScale *
         * Visuals.DefaultScale (* Osty's size by its HP). */
        Vec2 scale_base;
        float scale_temp, scale_default, scale_osty;
    };
    /* A hand card's holder and where NHandCardHolder is taking it
     * (SetTargetPosition / Angle / Scale: lerps, not tweens). */
    struct Holder {
        Control* node;
        Vec2 pos, to_pos;
        float angle, to_angle; /* degrees */
        float scale, to_scale;
        /* The card's text at rest, against each enemy (single-enemy cards)
         * and while marking all enemies (NCard.SetPreviewTarget). */
        std::string text;
        std::vector<std::string> text_by_target;
        std::string text_multi;
        int id;                  /* Steps.IdOf(card), stable across responses */
        Json data;
        Node* card_node;
    };
    void show_card_text(Holder& h, const std::string& text);
    Node* make_background(const Json& bg);
    bool make_creature(const std::string& visuals_id, bool player, Creature& out);
    void update_bounds(Creature& c);
    void monster_death(Creature& cr, const std::string& who, class SpineSprite* s);
public:
    void refresh_hand_indices();
private:
    Node* exhaust_ui();
    void setup_combat_ui(Node* root, const Json& d);
    void build_hand(Node* ui, const Json& d);
    Holder make_holder(const Json& card);
    void sync_hand(const Json& cards, bool animate);
    void move_card(const Json& change);
    void refresh_state(Creature& c, bool animate);
    void apply_state(const Json& change);
    void finish_state();
    void refresh_counters();
    void layout_hand(bool snap = false);
    void select_reticle(Creature& c, bool on);
    void show_nameplate(Creature& c, bool on);
    Node2D* make_arrow();
    void update_arrow(const Vec2& target);
    void refresh_health_bar(Creature& c, const Json& data, const Json& powers);
    void make_intents(Creature& c, const Json& intents, int creature_index);
    void make_powers(Creature& c, const Json& powers);
    void make_orbs(Creature& c, const Json& orbs, int slots);
    void sync_orbs(const Json& orbs, int slots, bool animate);
    void activate_orb(const Json& cue);
    void orb_effect(const std::string& path, const Vec2& at);
    struct OrbSlot {
        Control* node;
        Node* sprite;
        Json data;
        int id;
        Vec2 from, to;
        double t;
    };
    struct FadingOrb { Control* node; double until; };
    Node* orb_box_;
    std::vector<OrbSlot> orbs_;
    std::vector<FadingOrb> fading_orbs_;
    void position_enemies(std::vector<Creature>& c, float scaling, std::vector<Vec2>* out = NULL);
    void position_players(std::vector<Creature>& c, float scaling, bool fully_center, Creature* osty = NULL,
                          float osty_max_hp = 0, float* pets_x = NULL);
    bool make_pet(const Json& pet, Creature& out);
    void position_pets(float pets_x);
    SceneLibrary* lib_;
    int hand_focus_;
    bool aftermath_;
    Json aftermath_player_;
    Control* hand_box_;               /* %CardHolderContainer */
    std::vector<Holder> holders_;     /* nodes in the built tree (not owned) */
    std::vector<Creature> enemies_;   /* the decision's enemies (node NULL: not shown) */
    Creature player_;
    Creature osty_;                   /* Necrobinder's Osty (node NULL: none) */
    std::vector<Creature> pets_;      /* the other pets (Byrdpip, Pael's Legion) */
    std::vector<std::string> pet_who_; /* their "e<combat id>" */
    float pets_x_;                    /* PositionPlayersAndPets' targetXPos for them */
    TweenSet tweens_;
    int play_card_;                   /* the card being played, -1: none */
    bool aiming_;                     /* a potion aimed (begin_aim) */
    Vec2 aim_from_;
    bool play_single_;                /* aiming at one enemy */
    int play_target_;
    /* NTargetingArrow: 19 segments and the head, drawn along a bezier from
     * the card to the target. */
    Node2D* arrow_;
    std::vector<Node2D*> arrow_segments_;
    Node2D* arrow_head_;
    Vec2 arrow_pos_, arrow_to_;
    /* Step playback. */
    enum CueKind { CUE_ANIM, CUE_VFX, CUE_DAMAGE, CUE_BANNER, CUE_STATE, CUE_SYNC, CUE_AUDIO, CUE_SCRIPTED };
    /* VfxAnchors (the scripted effects' creature points). */
    virtual bool creature(const std::string& who, VfxCreature* out);
    virtual bool side_center(bool enemies, Vec2* out);
    virtual Node* back_box();
    virtual Node* take_visuals(const std::string& who);
    virtual Node* visuals_of(const std::string& who);
    virtual bool origin_of(const std::string& who, Vec2* out);
    struct Cue {
        double at;
        int kind;
        std::string who, path, name, where; /* where: center, feet, side, screen */
        int amount;
        Json data;
    };
    struct FlyingNumber {
        Node2D* node;
        Vec2 velocity;
        double until;
    };
    Creature* creature_for(const std::string& who);
    bool vfx_position(const Cue& c, Vec2* out);
    void fire(const Cue& c);
    void show_banner(const std::string& kind, int round);
    void add_cue(const Cue& c);
    std::vector<Cue> cues_;
    size_t next_cue_;
    double cue_t_, busy_until_;
    double life_t_; /* keeps running when a new response resets the playhead */
    double radial_off_at_; /* when the radial blur hides (life_t_), 0: off */
    void radial_blur(const std::string& position);
    /* NCreature's own calls the logic made on a creature's node. */
    void creature_call(const std::string& method, const Json& args);
    void scale_visuals(Creature& c, float duration);
    Node* root_;
    std::vector<std::string> enemy_who_;  /* "e<combat_id>" per enemies_ */
    struct Ghost {
        Creature c;
        std::string who;
    };
    std::vector<Ghost> ghosts_;           /* enemies dead since the decision before */
    Json previous_;
    std::map<int, Vec2>* places_;
    bool final_;
    bool visual_only_;
    Control* vfx_box_;                    /* %CombatVfxContainer (screen coordinates) */
    std::vector<FlyingNumber> numbers_;
    struct Banner {
        Control* node;
        double until; /* hidden then (QueueFree) */
    };
    std::vector<Banner> banners_;
    /* The focused hand card's hover tips (NHandCardHolder.CreateHoverTips),
     * following the card (SetAlignmentForCardHolder). */
    void refresh_tips();
    Json hand_;
    Control* tips_;
    int tips_for_;
    Json state_, pending_;
    bool pending_finish_;
    int played_instance_;
    Vec2 played_destination_;
    struct MovingCard {
        Holder holder;
        Vec2 from, to;
        float from_scale, to_scale;
        double t, duration;
        bool exhaust; /* at the end, NCardExhaustVfx takes it (no fade) */
    };
    std::vector<MovingCard> moving_cards_;
    /* dead monsters that do not fade: hidden at "at" once nothing delays them */
    struct DelayedHide {
        CanvasItem* visuals;
        double at;
    };
    std::vector<DelayedHide> delayed_hides_;
    /* NEndTurnLongPressBar */
    bool long_press_on_, long_press_ready_, long_press_done_;
    double long_press_t_, long_press_fade_;
};

} /* namespace spire */

#endif
