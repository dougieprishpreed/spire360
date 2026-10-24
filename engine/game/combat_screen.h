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
#include "run_screen.h"

#include <algorithm>
#include <map>
#include <set>

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
    /* A creature's node mirrored (its Scale.X negated) or not. */
    void set_creature_flipped(const std::string& who, bool flipped);
    /* A new node tree for the decision (the caller owns it); NULL on error. */
    Node* build(const Json& decision, std::string* error);
    /* A fight's first decision as its steps start (CombatManager.StartTurn's
     * draws, energy and block come in them): build from this, then play its
     * steps. */
    static Json before_steps(const Json& decision);
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
    /* The focused card (an index into the hand's holders; -1: none) and the
     * next one left (-1) or right (+1) of holder `from`, wrapping; the cards
     * waiting in the play queue are not in the hand. */
    int focused_card() const { return focused_holder(); }
    int next_hand_card(int from, int dir) const;
    /* Steps.IdOf of the card in holder i (-1: none). */
    int card_id(int holder) const { return holder >= 0 && holder < (int)holders_.size() ? holders_[(size_t)holder].id : -1; }
    /* No hand card focused (an FTUE has the focus): none raised, no tips. */
    void set_hand_unfocused(bool on) {
        hand_unfocused_ = on;
        layout_hand();
        refresh_tips();
    }
    int hand_focus() const { return hand_focus_; }
    int hand_size() const { return (int)holders_.size(); }
    /* Playing a card with the pad (NControllerCardPlay): the card at hand
     * position `card` goes to the centre; a single-enemy card
     * (target_type AnyEnemy) aims the targeting arrow at enemy `target`
     * (its index in the decision's enemies), other cards mark what they
     * hit (all enemies, the player). A then plays it, B: end_play().
     * set_play_target: false if that enemy cannot be aimed at (not shown). */
    void begin_play(int card, const std::string& target_type, int target);
    /* NThoughtBubbleVfx over the player (a card that cannot be played:
     * NControllerCardPlay.Start's reason line) */
    void think(const std::string& text, double seconds);
    bool set_play_target(int enemy);
    void end_play();
    bool playing() const { return play_card_ >= 0; }
    /* The card being played was sent to the logic (NCardPlay.TryPlayCard):
     * where it flies when its play starts; while the last response still
     * plays (queued), it waits in NCardPlayQueue's place, out of the hand,
     * and the targeting ends. */
    void commit_play(bool queued);
    /* NPlayerHand.AreCardActionsAllowed: the hand takes plays while the
     * steps still run, once the player's turn has started in them (not
     * through the enemies' turn); NCardPlayQueue keeps them in order. */
    bool accepts_play() const;
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
    /* NPlayerHand.SelectCards (CardSelectCmd.FromHand / FromHandForUpgrade):
     * the hand in selection mode over the fight for a card_select decision
     * of kind "hand" or "hand_upgrade". The cards that cannot be picked hide;
     * A moves a card to the selected row above the hand (the upgrade preview
     * when upgrading) and back, Y confirms (min..max picked), B cancels when
     * the selection may be cancelled. end_hand_select puts the hand back as
     * it was (the next decision's steps move its cards). */
    /* NCombatRoom's creature navigation with a pad: up from the hand to the
     * creatures (the leftmost first), left / right between them by their
     * place on screen (wrapping), down or B back to the hand. The focused
     * creature shows its nameplate, its selection reticle and its hover
     * tips (NCreature.OnFocus: its intents' and powers' tips). */
    void focus_creatures();
    void move_creature_focus(int step);
    void unfocus_creatures();
    bool creatures_focused() const { return creature_focus_ != NULL; }
    void begin_hand_select(const Json& decision);
    void end_hand_select();
    /* One frame of pad input while selecting: a command, or "". */
    std::string hand_select_input(const PadState& pad);
    /* The bar filled (0.5 s held) since last asked: the turn is to end. */
    bool take_end_turn_long_press() {
        bool done = long_press_done_;
        long_press_done_ = false;
        return done;
    }
    /* After build: what happened since the decision before (its "steps"),
     * played back over the room as the game shows it: the creatures'
     * animations, the effects (VfxCmd), damage numbers, deaths. */
    /* A response that comes while the last one still plays (a play from the
     * queue) follows it on the same timeline. sync_to: the state the room
     * shows once these steps are done (finishing: the fight's end). */
    void play_steps(const Json& steps, const Json& sync_to = Json(), bool finishing = false);
    /* The fight's first turn: "Battle Start", then the player's turn
     * banner (NCombatStartBanner); without the turn banner after the
     * combat rules FTUE (NCombatRulesFtue closes into NCombatStartBanner). */
    void show_start_banner(bool turn_banner = true);
    /* The steps' timeline held (an FTUE waits for its confirmation). */
    void set_paused(bool paused) { paused_ = paused; }
    /* CardPileCmd.ShuffleFtueCheck: watching, the first shuffle (a card
     * from the discard pile to the draw pile) holds the timeline;
     * take_shuffle answers once that it came. */
    void watch_shuffle(bool on) { watch_shuffle_ = on; }
    bool take_shuffle() {
        bool s = shuffle_hit_;
        shuffle_hit_ = false;
        return s;
    }
    /* The playback is running (the pad waits for it; banners do not
     * count). */
    bool busy() const { return cue_t_ < busy_until_; }
    /* NCombatUi.ShowRewards: once the playback is done, the dying
     * monsters' animations and a second after them (Fast: half a second;
     * a boss, half its animation). */
    bool rewards_ready(bool boss);

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
        bool queued;             /* in NCardPlayQueue, out of the hand's layout */
    };
    void show_card_text(Holder& h, const std::string& text);
    Node* make_background(const Json& bg);
    bool make_creature(const std::string& visuals_id, bool player, Creature& out);
    void update_bounds(Creature& c);
    /* keep: StartDeathAnim(shouldRemove: false), the body stays (AnimDie
     * neither fades nor frees it) */
    void monster_death(Creature& cr, const std::string& who, class SpineSprite* s, bool keep = false);
    void refresh_energy();
    Color energy_outline_; /* CharacterModel.EnergyLabelOutlineColor */
    int shown_energy_;
    class ScriptBehavior* energy_spin_; /* the orb's SpinLayers */
    void enable_ui(Creature& c);
    /* NCreature.AnimDie running (its own effect hides it): the room behind
     * the rewards leaves them be */
    std::vector<std::string> dying_;
    bool is_dying(const std::string& who) const { return std::find(dying_.begin(), dying_.end(), who) != dying_.end(); }
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
    bool apply_state(const Json& change);
    void finish_state(const Json& d, bool finishing);
    bool active(const Holder& h) const;
    void place_queue();
    void track_play();
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
    /* NCreature.TrackBlockStatus: whose block a creature's health bar also
     * shows (Osty: its summoner's), by who */
    std::map<std::string, std::string> block_tracking_;
    /* CreatureCmd.Escape: whose node left the room (its despawn is no death) */
    std::set<std::string> escaped_;
    /* NCreature.ToggleIsInteractable(false): its state display hidden, the
     * effects for interactable creatures skip it */
    std::set<std::string> uninteractable_;
    /* Creature.SetNodeVisible(false): who stays hidden until shown again
     * (InfestedPower's Wrigglers until the elite's death animation is
     * over), whatever a sync shows */
    std::set<std::string> node_hidden_;
    /* SandpitPower, by its owner: where its target stood and the amount it
     * started from (AfterApplied) */
    std::map<std::string, float> sandpit_x_;
    std::map<std::string, int> sandpit_amount_;
    /* creature nodes a tween moves along x (SandpitPower) */
    struct CreatureMove {
        Control* node;
        Vec2 from, to;
        float t, duration;
        TweenTrans trans;
        TweenEase ease;
    };
    std::vector<CreatureMove> creature_moves_;
    /* NCard.PlayRandomizeCostAnim: a hand card's cost label rolling (0 to
     * 50 over 0.4-0.6 s, Sine Out: "?" or a digit), then its cost again */
    struct CostRoll {
        int id;
        float t, duration, offset;
    };
    std::vector<CostRoll> cost_rolls_;
    void update_cost_rolls(double dt);
    void move_creature_x(Creature& c, float dx, float duration, TweenTrans trans, TweenEase ease);
    bool sandpit_call(const std::string& method, const Json& args);
    void special_scripted(const std::string& call, const Json& args);
    /* NCreature.AnimShake running, by who: its time */
    std::map<std::string, double> shakes_;
    TweenSet tweens_;
    int play_card_;                   /* the card being played, -1: none */
    int play_id_;                     /* its Steps id (holders_ may change under it) */
    /* NCardPlayQueue: the cards sent to the logic, by id: where each flies
     * when its play starts; the queued ones in order */
    std::map<int, Vec2> played_;
    std::vector<int> queue_;
    double hand_locked_until_; /* the player's turn starts there on the timeline */
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
    virtual bool side_floor(bool enemies, Vec2* out);
    virtual Node* back_box();
    virtual Node* take_visuals(const std::string& who);
    virtual Node* visuals_of(const std::string& who);
    virtual bool origin_of(const std::string& who, Vec2* out);
    struct Cue {
        Cue() : at(0), kind(0), amount(0), delayed(false) {}
        double at;
        int kind;
        std::string who, path, name, where; /* where: center, feet, side, screen */
        int amount;
        Json data;
        /* part of a presentation task the game runs alongside the turn
         * ("delay"): it neither holds the pad nor the steps after it */
        bool delayed;
    };
    Creature* creature_for(const std::string& who);
    bool vfx_position(const Cue& c, Vec2* out);
    void fire(const Cue& c);
    void show_banner(const std::string& kind, int round);
    void add_cue(const Cue& c);
    static bool cue_before(const Cue& a, const Cue& b);
    std::vector<Cue> cues_;
    size_t next_cue_;
    double cue_t_, busy_until_;
    bool paused_, watch_shuffle_, shuffle_hit_, hand_unfocused_;
    double life_t_; /* keeps running when a new response resets the playhead */
    double radial_off_at_; /* when the radial blur hides (life_t_), 0: off */
    void radial_blur(const std::string& position);
    /* NCreature's own calls the logic made on a creature's node. */
    bool creature_call(const std::string& method, const Json& args);
    bool spine_call(const std::string& method, const Json& args);
    void anim_shake(const std::string& who);
    /* The game's combat events the logic records (UiEvents: "ui_event"):
     * NCombatUi.AnimIn at CombatSetUp, AnimOut at CombatEnded (and the
     * creatures' intents out, the orbs gathered in), NEndTurnButton's state
     * (SetState: 0 hidden, 1 enabled, 2 disabled) with its glow when no card
     * can be played (StartOrStopPulseVfx), NPotionContainer.ShinePotions. */
    void ui_event(const std::string& ev);
    void combat_ui_anim(bool in);
    void end_turn_state(int state);
    void refresh_end_glow();
    int end_state_;
    bool end_shiny_;
    double end_glow_t_;
    std::vector<double> potion_shines_; /* life_t_ at which each slot's potion bounces */
    std::vector<double> slosh_at_;      /* and when its slosh sounds */
    /* RollingBoulderPower: when the boulder started (the steps' time) and
     * when it meets each target / is done, from then */
    /* EncounterModel.HasScene: the encounter's scene (its slots' markers)
     * in the enemy container; NULL for the row layout */
    Control* encounter_slots_;
    /* NCeremonialBeastBgVfx (the boss background's two spine sprites):
     * hidden until its second phase, then glow, skulls, flowers; the
     * music's ringing / progress parameters */
    std::vector<class SpineSprite*> beast_bg_;
    int beast_bg_state_; /* 0 none, 1 glow, 2 skulls, 3 flowers */
    float beast_ringing_, beast_progress_;
    void update_beast_bg();
    /* PositionCreaturesWithSlots: an enemy at its slot's marker */
    bool slot_position(const Json& enemy, Vec2* out) const;
    double boulder_t0_, boulder_done_;
    /* NPower: the row icons pulsing ("who power"), its flash */
    std::set<std::string> pulsing_;
    void power_row_flash(const std::string& who, const std::string& power);
    /* Steps.Who: "player", "osty", "e<combat id>" */
    std::string creature_who(const Creature& c) const;
    std::map<std::string, double> boulder_hits_;
    void scale_visuals(Creature& c, float duration);
    void scale_base(Creature& c);
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
    Json state_, pending_; /* pending_: the last response (its cards' and powers' data) */
    int played_instance_;  /* the last played card that left the hand */
    struct MovingCard {
        MovingCard() : burn_at(-1), arrive(-1) {}
        Holder holder;
        Vec2 from, to;
        float from_scale, to_scale;
        double t, duration;
        bool exhaust; /* at the end, NCardExhaustVfx takes it (no fade) */
        /* 0: straight (cards that just go); 1: NCardFlyVfx to a pile (an
         * arc, speeding up, darkening and shrinking); 2: NCardFlyPowerVfx
         * (along the swoosh path into the player); 3: in the play pile
         * (NCombatUi.PlayContainer) until its play sends it on */
        int kind;
        int phase;
        float speed, accel, fdur, ftime, scale_from, scale_t;
        Vec2 ctrl;
        Node* trail;       /* NCardTrailVfx following the card (owned by the tree) */
        Node* path_scene;  /* the power swoosh's scene (owned here) */
        float from_angle;  /* degrees */
        double mp_t;       /* NCard.AnimMultiCardPlay's time, < 0: none */
        double burn_at;    /* exhausted from a pile: NCardExhaustVfx at this t (< 0: none) */
        int arrive;        /* the pile (0 draw, 1 discard) a flight lands on: counted there then */
    };
    void update_in_play(MovingCard& m, double dt);
    MovingCard* in_play(int id);
    Vec2 play_pile_pos() const;
    void burn_play_pile();
    std::vector<int> burn_ids_; /* RemoveFromCombat: burnt as they leave the hand */
    std::map<int, Json> play_cards_; /* the cards played this fight (a power's copy) */
    /* NCardFlyShuffleVfx: a card's silhouette from the discard pile to the
     * draw pile */
    struct ShuffleFly {
        Control* node;
        Node* trail;
        Vec2 from, to, ctrl; /* on screen */
        float speed, accel, duration, time;
        int phase; /* 0 flying, 1 shrinking, 2 fading */
        int arrive; /* the pile it lands on (0 draw, 1 discard, -1 none) */
    };
    std::vector<ShuffleFly> shuffle_flies_;
    /* NCombatCardPile: a pile's count rises as each card lands on it
     * (CardAddFinished at the end of NCardFlyVfx / NCardFlyShuffleVfx),
     * with a bump; the cards still flying to it are not counted yet. */
    int arriving(int pile) const;
    void land_on_pile(int pile);
    /* A card moved between piles without a node (the shuffle, ...): it
     * leaves its pile's count as its silhouette sets off and joins the
     * other's as it lands. By pile (0 draw, 1 discard, 2 exhaust): the
     * ones moved whose silhouette has not set off yet (reset at a sync). */
    int depart_[3], expect_[3];
    void spawn_shuffle_fly(const std::string& from = "discard", const std::string& to = "draw");
    void create_at_pile(const Json& ch);
    void update_shuffle_flies(double dt);
    Vec2 pile_center(const std::string& pile);
    /* NCardFlyPowerVfx.GetDuration from the play pile to this player */
    float power_fly_seconds();
    float power_fly_seconds_; /* < 0: not measured yet */
    std::map<std::string, double> death_end_; /* when a dying monster's animation ends (life_t_) */
    std::map<std::string, double> fade_gone_at_; /* a fading monster's node goes then at the latest */
    double rewards_at_;                       /* life_t_ the rewards wait for, < 0: not counting */
    void start_flight(MovingCard& m, int kind, const std::string& pile);
    /* RelicModel.Flash / PowerModel.Flash (relic_flash / power_flash cues) */
    void flash_relic(const Json& cue);
    void flash_power(const Json& cue);
    bool fly(MovingCard& m, double dt); /* false once the card is gone */
    struct Trail {
        Node* node;
        double t;
    };
    std::vector<Trail> fading_trails_; /* NCardTrailVfx.FadeOut */
    std::vector<MovingCard> moving_cards_;
    /* dead monsters that do not fade: hidden at "at" once nothing delays them */
    struct DelayedHide {
        CanvasItem* visuals;
        double at;
        Control* node; /* the creature (QueueFreeSafely at AnimDie's end) */
    };
    std::vector<DelayedHide> delayed_hides_;
    /* NEndTurnLongPressBar */
    bool long_press_on_, long_press_ready_, long_press_done_;
    double long_press_t_, long_press_fade_;
    /* creature focus */
    void creature_order(std::vector<Creature*>& out);
    Creature* creature_of(Control* node);
    void show_creature_focus(bool on);
    Control* creature_focus_;  /* the focused creature's node, NULL: none */
    Control* creature_tips_;
    /* hand selection mode */
    int focused_holder() const;
    void sel_pick(int holder);
    void sel_unpick(int k);
    void sel_layout();
    int sel_mode_; /* 0 none, 1 simple, 2 upgrade */
    Json sel_;
    std::vector<Holder> selected_; /* in %SelectedHandCardContainer, picked order */
    bool sel_row_;                 /* the pad on the selected row */
    int sel_focus_;
    Control* sel_box_;
    Control* sel_confirm_;
    float sel_confirm_x_, sel_confirm_y_;
    bool sel_confirm_shown_;
};

} /* namespace spire */

#endif
