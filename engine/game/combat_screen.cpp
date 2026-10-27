#include "combat_screen.h"
#include "../core/cue_ledger.h"
#include "screen_shake.h"
#include "game.h"
#include "card_view.h"
#include "loc.h"
#include "../audio/game_audio.h"
#include "ui_util.h"
#include "form_vfx.h"
#include "sovereign_blade.h"
#include "top_bar.h"
#include "deck_previews.h"

#include "../scene/controls.h"
#include "../scene/tree.h"
#include "../scene/node.h"
#include "../scene/particles.h"
#include "../scene/path_2d.h"
#include "../scene/scene.h"
#include "../scene/spine_sprite.h"
#include "../scene/rich_text.h"
#include "../scene/script.h"
#include "../scene/shaders.h"
#include "../scene/text_nodes.h"
#include "../scene/value.h"
#include "../spine/spine.h"

#include <algorithm>
#include <ctype.h>
#include <math.h>

namespace spire {

static const float PI_F = 3.14159265f;

/* Calls CombatScreen::process from the scene tree's frame (the screen's
 * root holds it; the screen outlives its tree), and runs the screen shake
 * while the room's scene container is its target (NCombatRoom._Ready:
 * SetScreenShakeTarget(SceneContainer)); the tree going away clears it. */
class CombatDriver : public ScriptBehavior {
public:
    CombatDriver(CombatScreen* s, Control* shake) : screen_(s), shake_(shake) {}
    virtual ~CombatDriver() { ScreenShake::get().clear_target(shake_); }
    virtual void process(Node*, double dt) {
        screen_->process(dt);
        if (shake_ && ScreenShake::get().target() == shake_) ScreenShake::get().update(dt);
    }

private:
    CombatScreen* screen_;
    Control* shake_;
};

CombatScreen::CombatScreen(SceneLibrary* lib)
    : lib_(lib), hand_focus_(-1), aftermath_(false), hand_box_(NULL), play_card_(-1), aiming_(false), play_single_(false), play_target_(-1), arrow_(NULL),
      arrow_head_(NULL) {
    player_.node = NULL;
    osty_.node = NULL;
    paused_ = watch_shuffle_ = shuffle_hit_ = hand_unfocused_ = false;
    end_state_ = 1;
    end_shiny_ = false;
    boulder_t0_ = boulder_done_ = 0;
    encounter_slots_ = NULL;
    beast_bg_state_ = 0;
    beast_ringing_ = beast_progress_ = -1;
    end_glow_t_ = 0;
    creature_focus_ = NULL;
    creature_tips_ = NULL;
    sel_mode_ = 0;
    sel_hand_index_ = -1;
    sel_row_ = false;
    sel_focus_ = 0;
    sel_box_ = sel_confirm_ = sel_confirm_placed_ = NULL;
    sel_confirm_x_ = sel_confirm_y_ = 0;
    sel_confirm_shown_ = false;
    long_press_on_ = false;
    long_press_ready_ = true;
    long_press_done_ = false;
    long_press_t_ = 0;
    long_press_fade_ = 0;
    next_cue_ = 0;
    cue_t_ = 0;
    life_t_ = 0;
    radial_off_at_ = 0;
    busy_until_ = 0;
    root_ = NULL;
    vfx_box_ = NULL;
    places_ = NULL;
    final_ = false;
    visual_only_ = false;
    tips_ = NULL;
    tips_for_ = -2;
    played_instance_ = -1;
    play_id_ = -1;
    hand_locked_until_ = 0;
    orb_box_ = NULL;
    power_fly_seconds_ = -1;
    rewards_at_ = -1;
    energy_spin_ = NULL;
    shown_energy_ = 0;
    for (int i = 0; i < 3; ++i) depart_[i] = expect_[i] = 0;
}

/* NCombatBackground.Create: the background scene, each picked layer under
 * Layer_NN, the foreground under Foreground. */
Node* CombatScreen::make_background(const Json& bg) {
    if (!bg.is_object()) return NULL;
    Node* root = instance(lib_, bg["scene"].str());
    if (!root) return NULL;
    const Json& layers = bg["layers"];
    for (size_t i = 0; i < layers.size(); ++i) {
        Node* slot = root->get_node(format("Layer_%02d", (int)i));
        Node* layer = slot ? instance(lib_, layers[i].str()) : NULL;
        if (!slot) log_warn("combat: background has no Layer_%02d", (int)i);
        if (layer) {
            if (layer->is_canvas_item()) ((CanvasItem*)layer)->visible = true;
            slot->add_child(layer);
        }
    }
    if (bg["fg"].is_string()) {
        Node* slot = root->get_node("Foreground");
        Node* layer = slot ? instance(lib_, bg["fg"].str()) : NULL;
        if (layer) {
            if (layer->is_canvas_item()) ((CanvasItem*)layer)->visible = true;
            slot->add_child(layer);
        }
    }
    return root;
}

static void play_idle(Node* n);
static SpineSprite* first_spine(Node* n);
static Node* orb_export(Node* n, const char* key);
static void segment_phobia(SceneLibrary* lib, Node* visuals, const Json& data, bool dead);

/* NCreature.Create + _Ready: the creature scene with the visuals as its first
 * child at its origin; the visuals' %Bounds give the width placement uses. */
bool CombatScreen::make_creature(const std::string& visuals_id, bool player, Creature& out) {
    out.node = NULL;
    out.visuals = NULL;
    out.width = 0;
    out.player = player;
    out.hp_bar_reduction = 0;
    out.max_fg_width = 0;
    out.reticle = NULL;
    out.nameplate = NULL;
    out.center = NULL;
    out.shown_hp = out.shown_max_hp = -1;
    out.scale_temp = out.scale_default = out.scale_osty = 1;
    out.scale_base = Vec2(0, 0);
    Node* v = NULL;
    Control* n = instance_creature(lib_, visuals_id, &v);
    if (!n) return false;
    Node* bounds = v ? v->find_unique("Bounds") : NULL;
    if (bounds && bounds->is_control()) {
        Control* b = (Control*)bounds;
        out.width = b->offset[SIDE_RIGHT] - b->offset[SIDE_LEFT]; /* anchors are 0 */
    }
    out.node = n;
    out.visuals = v;
    return true;
}

/* NCreature.Create: creature.tscn with the creature's visuals as its first
 * child, at its origin. */
Control* instance_creature(SceneLibrary* lib, const std::string& visuals_id, Node** visuals) {
    *visuals = NULL;
    Node* n = instance(lib, "scenes/combat/creature.tscn");
    if (!n || !n->is_control()) {
        delete n;
        return NULL;
    }
    Node* v = instance(lib, "scenes/creature_visuals/" + lower(visuals_id) + ".tscn");
    if (!v) v = instance(lib, "scenes/creature_visuals/fallback.tscn");
    if (v) {
        n->add_child(v);
        /* MoveChild(Visuals, 0) */
        n->children.pop_back();
        n->children.insert(n->children.begin(), v);
        if (v->is_node2d()) ((Node2D*)v)->position = Vec2(0, 0);
    }
    *visuals = v;
    return (Control*)n;
}

/* The hitbox (NCreature.UpdateBounds): %Bounds scaled by the visuals', in
 * the creature's coordinates. */
Rect2 creature_hitbox(Node* visuals) {
    Node* bnode = visuals ? visuals->find_unique("Bounds") : NULL;
    if (!bnode || !bnode->is_control()) return Rect2(Vec2(0, 0), Vec2(0, 0));
    Control* b = (Control*)bnode;
    Vec2 vs = visuals->is_node2d() ? ((Node2D*)visuals)->scale : Vec2(1, 1);
    return Rect2(Vec2(b->offset[SIDE_LEFT] * vs.x, b->offset[SIDE_TOP] * vs.y),
                 Vec2(offset_size(b).x * vs.x, offset_size(b).y * vs.y));
}

/* NCreature.Create + SetupForBestiary: a monster outside a fight, its
 * state display and intents hidden, its idle and spine setup started. */
Control* make_display_creature(SceneLibrary* lib, const Json& view, Node** visuals) {
    Control* n = instance_creature(lib, view["id"].str(), visuals);
    if (!n) return NULL;
    hide(n->find_unique("HealthBar"));
    hide(n->find_unique("StateDisplay"));
    hide(n->find_unique("Intents"));
    hide(n->find_unique("SelectionReticle"));
    play_idle(n);
    setup_creature_spine(*visuals, view);
    return n;
}

static const float HP_BAR_INSET = 10.0f; /* NHealthBar._foregroundContainerInset */

/* NCreature.UpdateBounds + NCreatureStateDisplay.SetCreatureBounds +
 * NHealthBar.UpdateLayoutForCreatureBounds, in the creature's coordinates
 * (the visuals sit at its origin). */
void CombatScreen::update_bounds(Creature& c) {
    Node* vis = c.visuals;
    Node* bnode = vis ? vis->find_unique("Bounds") : NULL;
    if (!bnode || !bnode->is_control()) return;
    Control* b = (Control*)bnode;
    Vec2 vs = vis->is_node2d() ? ((Node2D*)vis)->scale : Vec2(1, 1);
    Vec2 size(offset_size(b).x * vs.x, offset_size(b).y * vs.y);
    Vec2 pos(b->offset[SIDE_LEFT] * vs.x, b->offset[SIDE_TOP] * vs.y);
    c.hitbox = Rect2(pos, size);
    Control* hit = ctl(c.node, "Hitbox");
    if (hit) place(hit, pos.x, pos.y, size.x, size.y);
    Control* ret = ctl(c.node, "SelectionReticle");
    c.reticle = ret;
    c.center = vis->find_unique("CenterPos");
    if (ret) {
        place(ret, pos.x, pos.y, size.x, size.y);
        ret->pivot_offset = size * 0.5f;
        ret->modulate.a = 0; /* NSelectionReticle._Ready */
    }
    apply_bounds(c, Json());

    Control* sd = ctl(c.node, "HealthBar"); /* NCreatureStateDisplay */
    Control* hb = ctl(sd, "HealthBar");     /* NHealthBar */
    if (!sd || !hb) return;
    Control* name = ctl(sd, "NameplateContainer");
    c.nameplate = name;
    if (name) name->modulate.a = 0; /* shown on hover */
    /* NHealthBar's origin in the creature: the state display's, plus its
     * own (anchored to the state display's empty rect). */
    Vec2 hb_origin = offset_pos(sd) + offset_pos(hb);
    float v = 24.0f - c.hp_bar_reduction;
    Control* bar = ctl(hb, "HpBarContainer");
    if (bar) {
        place(bar, pos.x - v * 0.5f - hb_origin.x, bar->offset[SIDE_TOP], size.x + v, offset_size(bar).y);
        c.max_fg_width = size.x + v - HP_BAR_INSET;
    }
    Control* block = ctl(hb, "BlockContainer");
    if (block) {
        Vec2 bs = offset_size(block);
        place(block, pos.x - bs.x * 0.5f - hb_origin.x, block->offset[SIDE_TOP], bs.x, bs.y);
    }
}

void CombatScreen::apply_bounds(Creature& c, const Json& ops) {
    /* no ops: the creature's state (anim_bounds) as it is built */
    bool moved = !ops.is_array();
    if (moved && c.data["anim_bounds"].is_string()) c.bounds = c.data["anim_bounds"].str();
    for (size_t i = 0; i < ops.size(); ++i)
        if (ops[i]["op"].str() == "bounds") {
            c.bounds = ops[i]["name"].str();
            moved = true;
        }
    if (!moved) return;
    Node* vis = c.visuals;
    Control* intents = c.node ? ctl(c.node, "Intents") : NULL;
    if (!vis || !intents) return;
    /* the container's IntentPos, the visuals' own without one (%Bounds,
     * the hitbox's, is always the root's: GetNode resolves the unique name
     * in the scene's owner) */
    Node* box = c.bounds.empty() ? NULL : vis->get_node(c.bounds);
    Node* ipos = (box ? box : vis)->get_node("IntentPos");
    if (!ipos || !ipos->is_node2d()) return;
    Vec2 vs = vis->is_node2d() ? ((Node2D*)vis)->scale : Vec2(1, 1);
    Vec2 isz = offset_size(intents);
    Vec2 p = ((Node2D*)ipos)->position - isz * 0.5f;
    place(intents, p.x, p.y * vs.x, isz.x, isz.y);
}

/* A power by id in the creature's list (NULL if it has none). */
static const Json* find_power(const Json& powers, const char* id) {
    for (size_t i = 0; i < powers.size(); ++i)
        if (powers[i]["id"].str() == id) return &powers[i];
    return NULL;
}

static float patch_margin(Control* c, int side) {
    return c && c->type == "NinePatchRect" ? ((NinePatchRect*)c)->margin[side] : 0.0f;
}

/* NHealthBar.RefreshValues (RefreshBlockUi, RefreshForeground,
 * RefreshMiddleground, RefreshText) at rest: the tweens' end values.
 * e: the creature's decision data; powers: its power list. */
void CombatScreen::refresh_health_bar(Creature& c, const Json& e, const Json& powers) {
    Control* sd = ctl(c.node, "HealthBar");
    Control* hb = ctl(sd, "HealthBar");
    if (!hb) return;
    int hp = e["hp"].i(), max_hp = e["max_hp"].i(), blk = e["block"].i();
    /* the block of the creature it tracks (Osty: its summoner's) */
    int tracked = 0;
    for (std::map<std::string, std::string>::const_iterator it = block_tracking_.begin(); it != block_tracking_.end(); ++it)
        if (creature_for(it->first) == &c) {
            Creature* t = creature_for(it->second);
            if (t) tracked = t->data["block"].i();
        }
    Control* fg = ctl(hb, "HpForeground");
    Control* mid = ctl(hb, "HpMiddleground");
    Control* poison = ctl(hb, "PoisonForeground");
    Control* doom = ctl(hb, "DoomForeground");
    Control* outline = ctl(hb, "BlockOutline");
    Control* block = ctl(hb, "BlockContainer");
    Control* infinity = ctl(hb, "InfinityTex");
    Node* hp_label = hb->find_unique("HpLabel");
    Node* block_label = hb->find_unique("BlockLabel");
    if (!fg || !mid || !poison || !doom) return;
    float max_w = c.max_fg_width;
    /* GetFgWidth */
    float fg_w = max_hp <= 0 ? 0.0f : maxf((float)hp / (float)max_hp * max_w, hp > 0 ? 12.0f : 0.0f);
    const std::string& hp_display = e["hp_display"].str();
    bool infinite = hp_display.compare(0, 8, "Infinite") == 0;
    bool shows_numbers = hp_display != "InfiniteWithoutNumbers";
    const Json* poison_power = find_power(powers, "POISON_POWER");
    const Json* doom_power = find_power(powers, "DOOM_POWER");
    int poison_dmg = poison_power ? e["poison_next_turn"].i() : 0;
    int doom_amount = doom_power ? (*doom_power)["amount"].i() : 0;
    bool poison_lethal = poison_power && poison_dmg > 0 && poison_dmg >= hp;
    bool doom_lethal = doom_power && doom_amount > 0 && doom_amount >= hp - poison_dmg;

    /* RefreshBlockUi */
    if (blk <= 0 && tracked <= 0) {
        if (block && block->visible) {
            /* NBlockBrokenVfx at the block icon's centre. */
            Node* fx = instance(lib_, "scenes/vfx/vfx_block_broken.tscn");
            if (fx && fx->is_node2d()) {
                ((Node2D*)fx)->position = block->position + block->size * 0.5f;
                hb->add_child(fx);
            } else {
                delete fx;
            }
        }
        if (block) block->visible = false;
        if (outline) outline->visible = false;
        fg->self_modulate = hex(0xF1373E);
    } else {
        if (outline) outline->visible = true;
        fg->self_modulate = hex(0x3B6FA3);
        if (blk > 0) {
            if (block) block->visible = true;
            if (block_label && block_label->type == "Label") ((Label*)block_label)->set_text(format("%d", blk));
        }
    }

    /* RefreshForeground */
    if (hp <= 0) {
        poison->visible = doom->visible = fg->visible = false;
    } else {
        fg->visible = true;
        float fg_right = fg_w - max_w;
        fg->offset[SIDE_RIGHT] = fg_right;
        if (infinite) {
            fg->self_modulate = hex(0xC5BBED);
            poison->visible = doom->visible = false;
        } else {
            if (poison_power && poison_dmg > 0) {
                poison->visible = true;
                if (poison_lethal) {
                    poison->offset[SIDE_LEFT] = 0;
                    poison->offset[SIDE_RIGHT] = fg_right;
                    fg->visible = false;
                } else {
                    float w = max_hp <= 0 ? 0.0f : maxf((float)(hp - poison_dmg) / (float)max_hp * max_w, 12.0f);
                    fg->offset[SIDE_RIGHT] = w - max_w;
                    fg->visible = true;
                    poison->offset[SIDE_LEFT] = maxf(0.0f, w - patch_margin(poison, SIDE_LEFT));
                    poison->offset[SIDE_RIGHT] = fg_right;
                }
            } else {
                poison->visible = false;
                if (!poison_power) poison->offset[SIDE_LEFT] = 0;
            }
            if (doom_power && doom_amount > 0) {
                doom->visible = true;
                float dw = (max_hp <= 0 ? 0.0f : maxf((float)doom_amount / (float)max_hp * max_w, 12.0f)) - max_w;
                if (doom_lethal) {
                    if (!poison_lethal) {
                        doom->offset[SIDE_RIGHT] = fg->offset[SIDE_RIGHT];
                    } else {
                        doom->visible = false;
                    }
                    fg->visible = false;
                } else {
                    doom->offset[SIDE_RIGHT] = minf(0.0f, dw + patch_margin(doom, SIDE_RIGHT));
                    fg->visible = true;
                }
            } else {
                doom->visible = false;
            }
        }
    }

    /* RefreshMiddleground: where its tween ends */
    mid->visible = hp > 0;
    if (hp > 0) {
        mid->offset[SIDE_LEFT] = 1.0f;
        mid->offset[SIDE_RIGHT] = (poison->visible ? poison->offset[SIDE_RIGHT] : fg->offset[SIDE_RIGHT]) - 2.0f;
    }

    /* RefreshText */
    if (!hp_label || hp_label->type != "Label") return;
    Label* l = (Label*)hp_label;
    Color cream(1, 0.964706f, 0.886275f); /* StsColors.cream */
    if (hp <= 0) {
        l->add_theme_color_override("font_color", cream);
        l->add_theme_color_override("font_outline_color", hex(0x900000));
        l->set_text(Loc::get("gameplay_ui", "HEALTH_BAR.DEAD"));
        return;
    }
    doom->modulate.a = infinite ? 0.0f : 1.0f;
    if (infinity) infinity->visible = hp_display == "InfiniteWithoutNumbers";
    l->visible = shows_numbers;
    if (!shows_numbers) return;
    Color font = cream, out;
    if (hp_display == "InfiniteWithNumbers") out = hex(0x4C4370);
    else if (poison_lethal) {
        font = hex(0x76FF40);
        out = hex(0x074700);
    } else if (doom_lethal) {
        font = hex(0xFB8DFF);
        out = hex(0x2D1263);
    } else out = blk > 0 ? hex(0x1B3045) : hex(0x900000);
    l->add_theme_color_override("font_color", font);
    l->add_theme_color_override("font_outline_color", out);
    l->set_text(format("%d/%d", hp, max_hp));
}


/* ---- powers (NPowerContainer, NPower) ---- */

static Color json_color(const Json& c, const Color& fallback) {
    if (c.size() < 3) return fallback;
    return Color((float)c[(size_t)0].num(), (float)c[1].num(), (float)c[2].num(), c.size() > 3 ? (float)c[3].num() : 1.0f);
}

/* NPowerContainer.SetCreatureBounds + Add + UpdatePositions, NPower at rest
 * (its fade-in done): the visible powers in rows under the health bar,
 * as wide as the creature's bounds plus 25 px, centred when wider. */
void CombatScreen::make_powers(Creature& c, const Json& powers) {
    Control* sd = ctl(c.node, "HealthBar");
    Control* box = ctl(sd, "PowerContainer");
    if (!box) return;
    for (size_t i = 0; i < box->children.size(); ++i) set_visible(box->children[i], false);
    std::vector<Control*> nodes;
    for (size_t i = 0; i < powers.size(); ++i) {
        const Json& pw = powers[i];
        if (!pw["visible"].b(true)) continue;
        std::string name = "Power_" + pw["id"].str();
        Node* n = box->get_node(name);
        bool fresh = n == NULL;
        if (!n) n = instance(lib_, "scenes/combat/power.tscn");
        if (!n || !n->is_control()) {
            delete n;
            continue;
        }
        Node* icon = n->find_unique("Icon");
        set_texture(lib_, icon, "images/atlases/power_atlas.sprites/" + lower(pw["id"].str()) + ".tres");
        if (icon && icon->is_canvas_item())
            ((CanvasItem*)icon)->shader_params["pulse"] = pulsing_.count(creature_who(c) + " " + pw["id"].str()) ? 1.0f : 0.0f;
        /* RefreshAmount */
        Node* label = n->find_unique("AmountLabel");
        if (label && label->is_control()) {
            ((Control*)label)->add_theme_color_override("font_color", json_color(pw["amount_color"], Color(1, 1, 1)));
            set_label(label, pw["counter"].b() ? format("%d", pw["display_amount"].i(pw["amount"].i())) : std::string());
        }
        n->name = name;
        set_visible(n, true);
        if (fresh) {
            box->add_child(n);
            ((Control*)n)->modulate.a = 0;
            tweens_.tween(&((Control*)n)->modulate.a, 1, 0.25f, TRANS_SINE, EASE_OUT);
        }
        nodes.push_back((Control*)n);
    }
    /* SetCreatureBounds: x at the bounds' left, width bounds + 25. */
    float width = c.hitbox.size.x + 25.0f;
    float x0 = c.hitbox.pos.x - offset_pos(sd).x;
    if (nodes.empty()) {
        place(box, x0, box->offset[SIDE_TOP], width, 0);
        return;
    }
    float w = offset_size(nodes[0]).x;
    if (w <= 0) w = 48;
    int n = (int)nodes.size();
    int per_row = (int)ceilf(width / w);
    if ((n + 1) / 2 > per_row) per_row = (n + 1) / 2;
    for (int i = 0; i < n; ++i) {
        Vec2 sz = offset_size(nodes[i]);
        place(nodes[i], w * (float)(i % per_row), floorf((float)i / (float)per_row) * w, sz.x, sz.y);
    }
    float used = w * (float)(n < per_row ? n : per_row);
    place(box, x0 - maxf(0.0f, used - width) / 2.0f, box->offset[SIDE_TOP], width, 0);
}

/* ---- intents (NCreature.UpdateIntent, NIntent) ---- */

/* IntentAnimData: an animation's frames in intent_atlas (prefix_NN, or one
 * sprite). */
static std::vector<std::string> intent_frames(const std::string& anim) {
    static const struct {
        const char* name;
        const char* prefix;
        int frames; /* 0: one sprite, no number */
    } data[] = { { "attack_1", "attack/intent_attack_1", 0 }, { "attack_2", "attack/intent_attack_2", 0 },
                 { "attack_3", "attack/intent_attack_3", 0 }, { "attack_4", "attack/intent_attack_4", 0 },
                 { "attack_5", "attack/intent_attack_5", 0 }, { "buff", "buff/intent_buff", 30 },
                 { "card_debuff", "card_debuff/intent_carddebuff", 15 }, { "death_blow", "intent_death_blow", 0 },
                 { "debuff", "debuff/intent_megadebuff", 11 }, { "defend", "defend/intent_defend", 45 },
                 { "escape", "escape/intent_escape", 40 }, { "heal", "heal/intent_heal", 45 },
                 { "hidden", "intent_hidden", 0 }, { "sleep", "sleep/intent_sleep", 16 },
                 { "status", "status/intent_statuscard", 19 }, { "stun", "stun/intent_stunned", 16 },
                 { "summon", "summon/intent_summon", 25 }, { "unknown", "unknown/intent_unknown", 30 } };
    std::vector<std::string> out;
    for (size_t i = 0; i < sizeof(data) / sizeof(data[0]); ++i) {
        if (anim != data[i].name) continue;
        std::string base = std::string("images/atlases/intent_atlas.sprites/") + data[i].prefix;
        if (data[i].frames == 0) out.push_back(base + ".tres");
        for (int f = 0; f < data[i].frames; ++f) out.push_back(format("%s_%02d.tres", base.c_str(), f));
    }
    return out;
}

/* NIntent._Process: the holder bobs (sin(t * pi + offset) * 10 + 8 px up)
 * and the sprite runs its frames at 15 fps. On the shaders' clock (Godot's
 * Time.GetTicksMsec), so a rebuilt screen carries on where it was. */
class IntentAnim : public ScriptBehavior {
public:
    IntentAnim(Control* holder, Sprite2D* sprite, float offset) : holder_(holder), sprite_(sprite), offset_(offset) {}
    std::vector<Texture2DRes*> frames;
    virtual void process(Node*, double) {
        float t = (float)g_shader_time;
        float y = -(sinf(t * PI_F + offset_) * 10.0f + 8.0f);
        holder_->offset[SIDE_TOP] = y;
        holder_->offset[SIDE_BOTTOM] = y;
        if (!frames.empty() && sprite_) sprite_->texture = frames[(size_t)(t * 15.0f) % frames.size()];
    }

private:
    Control* holder_;
    Sprite2D* sprite_;
    float offset_;
};

/* NCreature.UpdateIntent: one intent.tscn per intent of the move in the
 * creature's %Intents row; the label (damage, status cards) as the game
 * formats it. Shown at rest (RevealIntents' fade is done). */
void CombatScreen::make_intents(Creature& c, const Json& intents, int creature_index) {
    Node* row = c.node ? c.node->find_unique("Intents") : NULL;
    if (!row || !row->is_control()) return;
    while (!row->children.empty()) {
        Node* child = row->children.back();
        row->remove_child(child);
        delete child;
    }
    ((Control*)row)->modulate.a = 1;
    for (size_t i = 0; i < intents.size(); ++i) {
        const Json& in = intents[i];
        Node* n = instance(lib_, "scenes/combat/intent.tscn");
        if (!n) return;
        Control* holder = ctl(n, "IntentHolder");
        Node* sprite = n->find_unique("Intent");
        if (!holder) {
            delete n;
            return;
        }
        IntentAnim* anim = new IntentAnim(holder, sprite && sprite->type == "Sprite2D" ? (Sprite2D*)sprite : NULL,
                                          (float)creature_index * 1.7f + (float)i * 0.3f);
        std::vector<std::string> frames = intent_frames(in["animation"].str());
        if (frames.empty() && in["sprite"].is_string()) frames.push_back(in["sprite"].str());
        for (size_t f = 0; f < frames.size(); ++f) {
            Resource* r = lib_->ext(frames[f]);
            if (r && r->is_texture()) anim->frames.push_back((Texture2DRes*)r);
        }
        n->behavior = anim;
        set_label(n->find_unique("Value"), in["label"].is_string() ? in["label"].str() : std::string());
        row->add_child(n);
    }
}

/* NCombatRoom.PositionEnemies. */
void CombatScreen::position_enemies(std::vector<Creature>& cs, float scaling, std::vector<Vec2>* out) {
    if (cs.empty()) return;
    float half = 960.0f / scaling, gap = 70.0f, sum = 0;
    for (size_t i = 0; i < cs.size(); ++i) sum += cs[i].width;
    float total = sum + (float)(cs.size() - 1) * gap;
    float x = maxf((half - total) * 0.5f, 150.0f);
    float stagger = 0;
    if (x + total > half && cs.size() > 1) {
        gap = maxf((half - 150.0f - sum) / (float)(cs.size() - 1), 5.0f);
        total = sum + (float)(cs.size() - 1) * gap;
        x = (half - total) * 0.5f;
        if (gap < 30.0f) stagger = 60.0f + (40.0f - 60.0f) * ((gap - 5.0f) / 25.0f);
    }
    for (size_t i = 0; i < cs.size(); ++i) {
        Vec2 at(x + cs[i].width * 0.5f, 200.0f - ((i % 2) != 0 ? stagger : 0.0f));
        cs[i].node->set_position(at);
        if (out) out->push_back(at);
        x += cs[i].width + gap;
    }
}

/* NCombatRoom.PositionPlayersAndPets, for the local player (pets come
 * later: Necrobinder's Osty). */
void CombatScreen::position_players(std::vector<Creature>& cs, float scaling, bool fully_center, Creature* osty,
                                    float osty_max_hp, float* pets_x) {
    if (cs.empty()) return;
    float half = 960.0f / scaling, gap = 70.0f;
    int n = (int)cs.size();
    int cols = (int)ceil(sqrt((double)n));
    int rows = (int)ceil((double)n / (double)cols);
    float sum = 0;
    for (int i = 0; i < cols && i < n; ++i) sum += cs[i].width;
    float total = sum + (float)(cols - 1) * gap;
    float shift = sum * 0.33f;
    float row_shift = rows > 1 ? shift / (float)(rows - 1) : 0.0f;
    float row_rise = rows > 1 ? 120.0f / (float)(rows - 1) : 0.0f;
    float x0;
    if (fully_center) {
        x0 = cs[0].width * -0.5f;
    } else {
        x0 = maxf((half - total) * 0.5f, 150.0f);
        if (n >= cols * 2) sum += shift;
        if (x0 + total > half && cols > 1) {
            gap = (half - 150.0f - sum) / (float)(cols - 1);
            total = sum + (float)(cols - 1) * gap;
            x0 = (half - total) * 0.5f;
        }
    }
    for (int r = 0; r < cols; ++r) {
        float x = x0 + row_shift * (float)r;
        for (int c = 0; c < cols; ++c) {
            int k = r * cols + c;
            if (k >= n) break;
            cs[k].node->set_position(Vec2(-x - cs[k].width * 0.5f, 200.0f - row_rise * (float)r));
            if (k == 0 && osty && osty->node) {
                /* PositionLocalPlayerOsty: the player 150 px further left,
                 * Osty right of where the player was (GetOstyOffsetFromPlayer:
                 * half the player's hitbox, plus 150..250 px by its max HP,
                 * 75 px up). */
                float t = clampf(osty_max_hp / 150.0f, 0.0f, 1.0f);
                Vec2 off(cs[k].hitbox.size.x * 0.5f + 150.0f + 100.0f * t, -75.0f);
                cs[k].node->set_position(Vec2(-x - cs[k].width * 0.5f - 150.0f, 200.0f - row_rise * (float)r));
                osty->node->set_position(Vec2(-x, 200.0f - row_rise * (float)r) + off);
                x += 100.0f;
            }
            if (k == 0 && pets_x) *pets_x = x;
            if (r > 0 && cs[k].visuals && cs[k].visuals->is_canvas_item())
                ((CanvasItem*)cs[k].visuals)->modulate = Color(0.5f, 0.5f, 0.5f);
            x += cs[k].width + gap;
        }
    }
}

/* ---- the combat UI (NCombatUi and its children at rest, player turn) ---- */

static Control* child_ctl(Node* n, const char* path) {
    Node* c = n ? n->get_node(path) : NULL;
    return c && c->is_control() ? (Control*)c : NULL;
}

/* A control of the full-screen combat UI moved to a screen position, as the
 * scripts' Position = ... (its size kept). */
static void place_on_screen(Control* c, const Vec2& pos) {
    float w = (c->anchor[SIDE_RIGHT] - c->anchor[SIDE_LEFT]) * 1920.0f + c->offset[SIDE_RIGHT] - c->offset[SIDE_LEFT];
    float h = (c->anchor[SIDE_BOTTOM] - c->anchor[SIDE_TOP]) * 1080.0f + c->offset[SIDE_BOTTOM] - c->offset[SIDE_TOP];
    for (int i = 0; i < 4; ++i) c->anchor[i] = 0;
    place(c, pos.x, pos.y, w, h);
}

/* NEnergyCounter._Process / NStarCounter: the orb's rotation layers turn,
 * layer i at (i + 1) * speed degrees a second. */
class SpinLayers : public ScriptBehavior {
public:
    SpinLayers(Node* layers, float speed) : layers_(layers), speed_(speed) {}
    void set_speed(float speed) { speed_ = speed; }
    virtual void process(Node*, double dt) {
        for (size_t i = 0; i < layers_->children.size(); ++i) {
            Node* c = layers_->children[i];
            if (c->is_control())
                ((Control*)c)->rotation += (float)(dt * speed_ * (double)(i + 1)) * 3.14159265f / 180.0f;
        }
    }

private:
    Node* layers_;
    float speed_;
};

/* NCardSmithVfx.PlayAnimation() on a hand card: Spark1..3's particles at
 * 0, 0.25 and 0.5 s, freed at 0.9 s */
class SmithSparks : public ScriptBehavior {
public:
    SmithSparks() : t_(0), next_(0) {}
    virtual void process(Node* n, double dt) {
        t_ += dt;
        while (next_ < 3 && t_ >= 0.25 * next_) {
            Node* spark = n->get_node(format("Spark%d", next_ + 1));
            for (size_t i = 0; spark && i < spark->children.size(); ++i)
                if (spark->children[i]->type == "GPUParticles2D" || spark->children[i]->type == "CPUParticles2D")
                    ((Particles2D*)spark->children[i])->restart();
            ++next_;
        }
        if (t_ >= 0.9 && !n->queued_for_deletion()) n->queue_free();
    }

private:
    double t_;
    int next_;
};

/* NStarCounter: the rotation layers turn (layer i at (i + 1) * 5 degrees
 * a second with no stars, else 30); the label counts towards the stars
 * (MathHelper.SmoothDamp, 0.1 s), red at 0 with the icon's hsv s 0.5 and
 * v 0.85; a gain (UpdateStarCount) flashes the icon's v from 2 to 1 in
 * 0.2 s; a loss sets the count at once. */
class StarCounterAnim : public ScriptBehavior {
public:
    StarCounterAnim(Node* layers, Node* label, CanvasItem* icon, int stars)
        : layers_(layers), label_(label), icon_(icon), stars_(stars), shown_(-1), lerp_((float)stars), velocity_(0),
          flash_(-1) {
        show(stars);
    }
    void set_stars(int stars) {
        if (stars < stars_) {
            flash_ = -1;
            lerp_ = (float)stars;
            velocity_ = 0;
            stars_ = stars;
            show(stars);
            return;
        }
        if (stars > stars_) flash_ = 0;
        stars_ = stars;
    }
    virtual void process(Node*, double dt) {
        float speed = stars_ == 0 ? 5.0f : 30.0f;
        for (size_t i = 0; layers_ && i < layers_->children.size(); ++i) {
            Node* c = layers_->children[i];
            if (c->is_control()) ((Control*)c)->rotation += (float)(dt * speed * (double)(i + 1)) * 3.14159265f / 180.0f;
        }
        lerp_ = smooth_damp(lerp_, (float)stars_, &velocity_, 0.1f, (float)dt);
        show((int)floorf(lerp_ + 0.5f));
        if (flash_ >= 0 && icon_) {
            flash_ += dt;
            float k = flash_ >= 0.2 ? 1.0f : (float)(flash_ / 0.2);
            icon_->shader_params["v"] = 2.0f - k;
            if (flash_ >= 0.2) flash_ = -1;
        }
    }

private:
    void show(int stars) {
        if (stars == shown_) return;
        shown_ = stars;
        if (label_ && label_->is_control())
            ((Control*)label_)->add_theme_color_override("default_color", stars > 0 ? Color(1, 0.964706f, 0.886275f) : hex(0xFF5555));
        set_label(label_, format("[center]%d[/center]", stars));
        if (icon_ && flash_ < 0) {
            icon_->shader_params["s"] = stars == 0 ? 0.5f : 1.0f;
            icon_->shader_params["v"] = stars == 0 ? 0.85f : 1.0f;
        }
    }
    Node* layers_;
    Node* label_;
    CanvasItem* icon_;
    int stars_, shown_;
    float lerp_, velocity_;
    double flash_;
};

static void set_counter_label(Node* label, int count) {
    set_label(label, format("%d", count));
}

void CombatScreen::setup_combat_ui(Node* root, const Json& d) {
    Node* ui = find_named(root, "CombatUi");
    if (!ui) return;
    const Json& player = d["player"];
    std::string character = lower(player["character"].is_string() ? player["character"].str() : std::string("ironclad"));
    int energy = d["energy"].i(), stars = d["stars"].i();

    /* Energy counter: CharacterModel.EnergyCounterPath, at the container's
     * origin (Regent: 22 px higher, room for the star counter). */
    Control* box = child_ctl(ui, "EnergyCounterContainer");
    Node* orb = instance(lib_, "scenes/combat/energy_counters/" + character + "_energy_counter.tscn");
    if (box && orb) {
        if (d["always_show_stars"].b()) {
            box->offset[SIDE_TOP] -= 22;
            box->offset[SIDE_BOTTOM] -= 22;
        }
        box->add_child(orb);
        static const struct {
            const char* id;
            unsigned outline;
        } outlines[] = { { "ironclad", 0x801212 }, { "silent", 0x004F04 }, { "defect", 0x163E64 },
                         { "necrobinder", 0x702D6F }, { "regent", 0x784000 } };
        energy_outline_ = Color(0, 0, 0, 0.05f); /* CharacterModel default #0000000D */
        for (size_t i = 0; i < sizeof(outlines) / sizeof(outlines[0]); ++i)
            if (character == outlines[i].id) energy_outline_ = hex(outlines[i].outline);
        Node* spin = orb->find_unique("RotationLayers");
        energy_spin_ = NULL;
        if (spin && !orb->behavior) orb->behavior = energy_spin_ = new SpinLayers(spin, 30.0f);
        shown_energy_ = energy;
        refresh_energy();
    }

    /* Star counter: Regent always, others once they have stars. */
    Control* star = child_ctl(ui, "StarCounter");
    if (star) {
        star->visible = d["always_show_stars"].b() || stars > 0;
        Node* count = star->find_unique("CountLabel");
        if (count && count->is_control()) {
            ((Control*)count)->add_theme_color_override("default_color",
                                                        stars > 0 ? Color(1, 0.964706f, 0.886275f) : hex(0xFF5555));
            set_label(count, format("[center]%d[/center]", stars));
        }
        Node* icon = star->get_node("Icon");
        if (!star->behavior)
            star->behavior = new StarCounterAnim(star->get_node("Icon/RotationLayers"), count,
                                                 icon && icon->is_canvas_item() ? (CanvasItem*)icon : NULL, stars);
    }

    /* Piles: counts and their pad glyphs (LT, RT, RB). */
    Node* piles = find_named(ui, "CombatPileContainer");
    Node* draw = piles ? piles->get_node("DrawPile") : NULL;
    Node* discard = piles ? piles->get_node("DiscardPile") : NULL;
    Node* exhaust = piles ? piles->get_node("ExhaustPile") : NULL;
    if (draw) {
        set_counter_label(draw->get_node("CountContainer/Count"), d["draw_pile_count"].i());
        show_glyph(lib_, draw->get_node("HotkeyIcon"), "lt");
    }
    if (discard) {
        set_counter_label(discard->get_node("CountContainer/Count"), d["discard_pile_count"].i());
        show_glyph(lib_, discard->get_node("HotkeyIcon"), "rt");
    }
    if (exhaust && exhaust->is_control()) {
        int n = d["exhaust_pile_count"].i();
        Control* e = (Control*)exhaust;
        e->visible = n > 0;
        place_on_screen(e, Vec2(1920 - 90, 1080 - 290)); /* NExhaustPileButton._showPosition */
        set_counter_label(exhaust->get_node("CountContainer/Count"), n);
        show_glyph(lib_, exhaust->get_node("HotkeyIcon"), "rb");
    }

    /* End Turn: ShowPos (1604, 846), "End Turn {turnNumber}", Y. */
    Control* end = child_ctl(ui, "EndTurnButton");
    if (end) {
        place_on_screen(end, Vec2(1604, 846));
        Node* label = end->get_node("Visuals/Label");
        if (label && label->is_control()) {
            ((Control*)label)->modulate = Color(1, 0.964706f, 0.886275f); /* StsColors.cream */
            set_label(label, Loc::format("gameplay_ui", "END_TURN_BUTTON", "turnNumber",
                                         format("%d", d["turn_number"].i(d["round"].i(1)))));
        }
        show_glyph(lib_, end->get_node("Visuals/HotkeyIcon"), "y");
    }

    /* At rest these are hidden or off screen (multiplayer, selection modes). */
    /* (the peek button shows in the hand's selection mode) */
    static const char* const hidden[] = { "PingButton", "PeekButton", "SelectionHeader", "SelectModeConfirmButton",
                                          "SelectModeBackstop", "UpgradePreviewContainer" };
    for (size_t i = 0; i < sizeof(hidden) / sizeof(hidden[0]); ++i) {
        Node* n = find_named(ui, hidden[i]);
        if (n && n->is_canvas_item()) ((CanvasItem*)n)->visible = false;
    }
}

/* ---- the hand (NPlayerHand + HandPosHelper + NHandCardHolder, at rest) ---- */

/* HandPosHelper: card centres relative to the bottom centre of the screen
 * and angles in degrees, for 1..10 cards. */
static const float HAND_POS[10][10][2] = {
    { { 0, -50 } },
    { { -100, -50 }, { 100, -50 } },
    { { -180, -50 }, { 0, -59 }, { 180, -50 } },
    { { -240, -25 }, { -80, -50 }, { 80, -50 }, { 240, -25 } },
    { { -340, 10 }, { -170, -30 }, { 0, -50 }, { 170, -30 }, { 340, 10 } },
    { { -460, 13 }, { -273, -25 }, { -90, -50 }, { 90, -50 }, { 273, -25 }, { 460, 13 } },
    { { -534, 18 }, { -365, -14 }, { -189, -39 }, { 0, -50 }, { 189, -39 }, { 365, -14 }, { 534, 18 } },
    { { -565, 28 }, { -400, -14 }, { -231, -39 }, { -80, -50 }, { 80, -50 }, { 231, -39 }, { 400, -14 }, { 565, 28 } },
    { { -600, 37 }, { -445, -2 }, { -300, -29 }, { -150, -45 }, { 0, -50 }, { 150, -45 }, { 300, -29 }, { 445, -2 },
      { 600, 37 } },
    { { -610, 38 }, { -472, 5 }, { -340, -21 }, { -200, -41 }, { -64, -50 }, { 64, -50 }, { 200, -41 }, { 340, -21 },
      { 472, 5 }, { 610, 38 } }
};
static const float HAND_ANGLE[10][10] = {
    { 0 },
    { -2, 2 },
    { -3, 0, 3 },
    { -8, -4, 4, 8 },
    { -8, -4, 0, 4, 8 },
    { -9, -6, -3, 3, 6, 9 },
    { -9, -6, -3, 0, 3, 6, 9 },
    { -12, -9, -6, -3, 3, 6, 9, 12 },
    { -12, -9, -6, -3, 0, 3, 6, 9, 12 },
    { -15, -12, -9, -6, -3, 3, 6, 9, 12, 15 }
};

/* NPlayerHand.RefreshLayout with a controller: the focused card upright,
 * full size and raised to its hitbox's half height; the others pushed
 * aside (75, 50, 25 px for distances 1..3). */
CombatScreen::Holder CombatScreen::make_holder(const Json& card) {
    Holder h;
    h.node = NULL;
    h.card_node = NULL;
    h.id = card["instance_id"].i(-card["index"].i() - 1);
    h.data = card;
    h.angle = h.to_angle = 0;
    h.scale = h.to_scale = 1;
    h.queued = false;
    h.text = card["description_bbcode"].str();
    h.text_multi = card["description_multi"].is_string() ? card["description_multi"].str() : h.text;
    const Json& by = card["description_by_target"];
    for (size_t i = 0; i < by.size(); ++i) h.text_by_target.push_back(by[i].str());
    Node* node = instance(lib_, "scenes/cards/holders/hand_card_holder.tscn");
    if (!node || !node->is_control()) { delete node; return h; }
    h.node = (Control*)node;
    h.card_node = make_card(lib_, card);
    if (h.card_node) node->add_child(h.card_node);
    Node* flash = node->get_node("Flash");
    if (flash && flash->is_canvas_item()) ((CanvasItem*)flash)->modulate.a = 0;
    hide(node->find_unique("HandIndex"));
    return h;
}

void CombatScreen::build_hand(Node* ui, const Json& d) {
    holders_.clear();
    Node* hand = ui ? find_named(ui, "Hand") : NULL;
    Node* box = hand ? hand->find_unique("CardHolderContainer") : NULL;
    hand_box_ = box && box->is_control() ? (Control*)box : NULL;
    sync_hand(d["hand"], false);
}

/* NPlayerHand keeps each NHandCardHolder while that card stays in hand.
 * New holders start at the draw pile; the existing ones keep their pose. */
void CombatScreen::sync_hand(const Json& cards, bool animate) {
    if (!hand_box_) return;
    std::vector<Holder> next;
    std::vector<bool> kept(holders_.size(), false);
    for (size_t i = 0; i < cards.size() && i < 10; ++i) {
        int id = cards[i]["instance_id"].i(-cards[i]["index"].i() - 1);
        Holder h;
        bool found = false;
        for (size_t j = 0; j < holders_.size(); ++j) {
            if (kept[j] || holders_[j].id != id) continue;
            h = holders_[j];
            kept[j] = true;
            found = true;
            if (h.data != cards[i]) {
                if (h.card_node) { h.node->remove_child(h.card_node); delete h.card_node; }
                h.card_node = make_card(lib_, cards[i]);
                if (h.card_node) h.node->add_child(h.card_node);
                h.data = cards[i];
                h.text = cards[i]["description_bbcode"].str();
                h.text_multi = cards[i]["description_multi"].is_string() ? cards[i]["description_multi"].str() : h.text;
                h.text_by_target.clear();
                const Json& by = cards[i]["description_by_target"];
                for (size_t k = 0; k < by.size(); ++k) h.text_by_target.push_back(by[k].str());
            }
            break;
        }
        if (!found) {
            h = make_holder(cards[i]);
            if (!h.node) continue;
            hand_box_->add_child(h.node);
            if (animate) {
                h.pos = Vec2(-850, -70);
                h.scale = 0.15f;
                h.angle = -25;
            }
        }
        next.push_back(h);
    }
    for (size_t j = 0; j < holders_.size(); ++j) {
        if (kept[j]) continue;
        if (!animate) {
            hand_box_->remove_child(holders_[j].node);
            delete holders_[j].node;
        } else {
            MovingCard m;
            m.holder = holders_[j];
            m.from = m.holder.pos;
            m.to = Vec2(850, -70);
            m.from_scale = m.holder.scale;
            m.to_scale = 0.15f;
            m.t = 0;
            m.duration = 0.4;
            m.exhaust = false;
            m.kind = 0;
            m.trail = m.path_scene = NULL;
            m.from_angle = m.holder.angle;
            m.mp_t = -1;
            moving_cards_.push_back(m);
        }
    }
    holders_ = next;
    hand_ = cards;
    tips_for_ = -2;
    track_play();
    layout_hand(!animate);
    refresh_tips();
}

/* The card being played stays the same card when holders_ changes under
 * it (a draw, a discard from the steps still playing); gone from the hand,
 * its play ends. */
void CombatScreen::track_play() {
    if (play_card_ < 0) return;
    for (size_t i = 0; i < holders_.size(); ++i)
        if (holders_[i].id == play_id_ && !holders_[i].queued) {
            play_card_ = (int)i;
            return;
        }
    play_card_ = -1;
    play_id_ = -1;
    end_play();
}

bool CombatScreen::active(const Holder& h) const { return h.node && h.node->visible && !h.queued; }

int CombatScreen::next_hand_card(int from, int dir) const {
    int n = (int)holders_.size();
    for (int k = 1; k <= n; ++k) {
        int i = ((from + dir * k) % n + n) % n;
        if (active(holders_[(size_t)i])) return i;
    }
    return from;
}

/* NCardPlayQueue.GetPositionForQueueIndex / GetScaleForQueueIndex: left of
 * the play pile (the screen's middle, 100 px up) by 300 * (i+1)/(i+3), at
 * 0.8 / (i+2) of the card. */
void CombatScreen::place_queue() {
    for (size_t k = 0; k < queue_.size(); ++k)
        for (size_t i = 0; i < holders_.size(); ++i) {
            if (holders_[i].id != queue_[k]) continue;
            Holder& h = holders_[i];
            float u = (float)(k + 1) / (float)(k + 3);
            Vec2 at(960.0f - 300.0f * u, 440.0f);
            h.to_pos = hand_box_ ? inverse(hand_box_->global_transform()).xform(at) : at;
            h.to_angle = 0;
            h.to_scale = 0.8f / (float)(k + 2);
            h.node->z_index = 1;
        }
}

void CombatScreen::commit_play(bool queued) {
    if (play_card_ < 0 || play_card_ >= (int)holders_.size()) return;
    Holder& h = holders_[(size_t)play_card_];
    /* played: to the play pile when the logic moves it there */
    played_[h.id] = play_pile_pos();
    play_cards_[h.id] = h.data;
    if (!queued) return;
    h.queued = true;
    queue_.push_back(h.id);
    end_play();
    place_queue();
}

bool CombatScreen::accepts_play() const {
    return root_ && hand_box_ && !final_ && !visual_only_ && !aftermath_ && sel_mode_ == 0 && !paused_ &&
           cue_t_ >= hand_locked_until_;
}

void CombatScreen::set_hand_focus(int index) {
    hand_focus_ = index;
    layout_hand();
    refresh_tips();
}

/* The tips of the focused card (none while a card is being played). */
void CombatScreen::refresh_tips() {
    int focus = play_card_ >= 0 || aiming_ || (sel_mode_ && sel_row_) || creature_focus_ ? -1 : focused_holder();
    if (focus == tips_for_) return;
    tips_for_ = focus;
    if (tips_) {
        if (tips_->parent) tips_->parent->remove_child(tips_);
        delete tips_;
        tips_ = NULL;
    }
    if (focus < 0 || !root_) return;
    tips_ = make_tip_set(lib_, holders_[(size_t)focus].data["tips"], true);
    if (!tips_) return;
    tips_->z_index = 10;
    tips_->modulate.a = 0; /* placed on the next frame, after the layout */
    root_->add_child(tips_);
}

static float hitbox_height(Control* holder) {
    Control* hit = ctl(holder, "Hitbox");
    return hit ? offset_size(hit).y : 422.0f;
}

/* The holders' targets; snap: there at once (a new screen). */
/* NHandCardHolder.SetIndexLabel: each card's place among the hand's
 * active holders (NPlayerHand.RefreshLayout; a queued play is out of
 * them) above it when the settings show them (PrefsSave.ShowCardIndices). */
void CombatScreen::refresh_hand_indices() {
    bool show = client_settings()["card_indices"].b();
    int n = 0;
    for (size_t i = 0; i < holders_.size(); ++i) {
        Node* label = holders_[i].node ? holders_[i].node->find_unique("HandIndex") : NULL;
        bool on = active(holders_[i]);
        if (on) ++n;
        set_label(label, format("%d", n));
        set_visible(label, show && on);
    }
}

/* The focused holder (an index into holders_) among the visible ones
 * (NPlayerHand.ActiveHolders); -1 when none is shown. */
int CombatScreen::focused_holder() const {
    int first = -1, count = 0, mid = -1;
    for (size_t i = 0; i < holders_.size(); ++i)
        if (active(holders_[i])) {
            if (first < 0) first = (int)i;
            ++count;
        }
    if (count == 0 || hand_unfocused_) return -1;
    int want = count / 2, k = 0;
    for (size_t i = 0; i < holders_.size(); ++i)
        if (active(holders_[i])) {
            if (k == want) mid = (int)i;
            ++k;
        }
    if (hand_focus_ < 0) return mid;
    int last = -1;
    for (size_t i = 0; i < holders_.size(); ++i)
        if (active(holders_[i])) {
            if ((int)i >= hand_focus_) return (int)i;
            last = (int)i;
        }
    return last;
}

void CombatScreen::layout_hand(bool snap) {
    refresh_hand_indices();
    /* NPlayerHand.RefreshLayout: the visible holders (ActiveHolders) */
    std::vector<int> act;
    for (size_t i = 0; i < holders_.size(); ++i)
        if (active(holders_[i])) act.push_back((int)i);
    int n = (int)act.size();
    place_queue();
    if (n == 0) return;
    int fh = (sel_mode_ && sel_row_) || creature_focus_ ? -1 : focused_holder();
    int focus = -1;
    for (int k = 0; k < n; ++k)
        if (act[(size_t)k] == fh) focus = k;
    float scale = 0.8f * (n <= 7 ? 1.0f : n == 8 ? 0.95f : n == 9 ? 0.9f : 0.85f);
    for (int k = 0; k < n; ++k) {
        int i = act[(size_t)k];
        Holder& h = holders_[i];
        if (i == play_card_) continue; /* centred by begin_play */
        /* the game's hand holds 10 at most; more holders here is a stale
         * one, never a read past the layout tables */
        int row = n <= 10 ? n - 1 : 9, col = k < 10 ? k : 9;
        float x = HAND_POS[row][col][0], y = HAND_POS[row][col][1];
        float angle = HAND_ANGLE[row][col], sc = scale;
        h.node->z_index = 0;
        if (k == focus) {
            y = -hitbox_height(h.node) * 0.5f + 2.0f;
            angle = 0;
            sc = 1;
            h.node->z_index = 1;
        } else if (focus >= 0) {
            int dist = k < focus ? focus - k : k - focus;
            float push = 100.0f * (1.0f - minf(1.0f, (float)dist / 4.0f));
            x += k < focus ? -push : push;
        }
        h.to_pos = Vec2(x, y);
        h.to_angle = angle;
        h.to_scale = sc;
        if (snap) {
            h.pos = h.to_pos;
            h.angle = angle;
            h.scale = sc;
        }
    }
    if (snap) process(0);
}

static Vec2 global_pos(Node* n) {
    return n && n->is_canvas_item() ? ((CanvasItem*)n)->global_transform().origin : Vec2(0, 0);
}

/* NCard.UpdateVisuals' description, as card_view sets it. */
void CombatScreen::show_card_text(Holder& h, const std::string& text) {
    Node* label = find_named(h.node, "DescriptionLabel");
    if (label) set_label(label, "[center]" + text + "[/center]");
}

/* NSelectionReticle.OnSelect / OnDeselect. */
void CombatScreen::select_reticle(Creature& c, bool on) {
    Control* r = c.reticle;
    if (!r) return;
    if (on) {
        tweens_.kill(&r->modulate.a);
        r->modulate = Color(1, 1, 1, 1);
        tweens_.tween_from(&r->scale, Vec2(0.9f, 0.9f), Vec2(1, 1), 0.5f, TRANS_EXPO, EASE_OUT);
    } else {
        tweens_.tween(&r->modulate.a, 0.0f, 0.2f, TRANS_SINE, EASE_OUT);
        tweens_.tween(&r->scale, Vec2(1.05f, 1.05f), 0.2f, TRANS_SINE, EASE_OUT);
    }
}

/* NCreatureStateDisplay.ShowNameplate / HideNameplate. */
void CombatScreen::show_nameplate(Creature& c, bool on) {
    if (c.nameplate) tweens_.tween(&c.nameplate->modulate.a, on ? 1.0f : 0.0f, on ? 0.15f : 0.2f);
}

void CombatScreen::think(const std::string& text, double seconds) {
    Json args = Json::make_array();
    args.append(Json::make_string(text));
    args.append(Json::make_string("player"));
    args.append(Json::make_number(seconds));
    spawn_combat_vfx(lib_, vfx_box_, "NThoughtBubbleVfx.Create", args, *this);
}

void CombatScreen::begin_play(int card, const std::string& target_type, int target) {
    if (card < 0 || card >= (int)holders_.size()) return;
    end_play();
    play_card_ = card;
    play_id_ = holders_[card].id;
    /* NCardPlay.CenterCard: the bottom centre of the screen, 3/4 size. */
    Holder& h = holders_[card];
    Vec2 centre(960.0f, 1080.0f - hitbox_height(h.node) * 0.75f / 2.0f);
    h.to_pos = hand_box_ ? inverse(hand_box_->global_transform()).xform(centre) : centre;
    h.to_angle = 0;
    h.to_scale = 0.75f;
    h.node->z_index = 1;
    if (target_type == "AnyEnemy") {
        /* NTargetManager.StartTargeting (controller): the arrow starts at the
         * card and follows the hovered creature. */
        play_single_ = true;
        if (arrow_) {
            arrow_->visible = true;
            arrow_pos_ = arrow_to_ = global_pos(h.node);
            arrow_->modulate = Color(1, 1, 1, 1);
            arrow_head_->scale = Vec2(0.95f, 0.95f);
        }
        set_play_target(target);
        return;
    }
    /* NCardPlay.ShowMultiCreatureTargetingVisuals. */
    play_single_ = false;
    if (target_type == "AllEnemies" || target_type == "RandomEnemy") {
        show_card_text(h, h.text_multi);
        for (size_t i = 0; i < enemies_.size(); ++i) select_reticle(enemies_[i], true);
    } else if (target_type == "Self" || target_type == "AllAllies") {
        select_reticle(player_, true);
    }
    refresh_tips();
}

/* A potion aimed with the pad (NTargetManager from the potion's holder):
 * the arrow from that point, the same targets as a card's. */
void CombatScreen::begin_aim(const Vec2& from, const std::string& target_type, int target) {
    end_play();
    aiming_ = true;
    aim_from_ = from;
    if (target_type == "AnyEnemy") {
        play_single_ = true;
        if (arrow_) {
            arrow_->visible = true;
            arrow_pos_ = arrow_to_ = from;
            arrow_->modulate = Color(1, 1, 1, 1);
            arrow_head_->scale = Vec2(0.95f, 0.95f);
        }
        set_play_target(target);
        return;
    }
    play_single_ = false;
    if (target_type == "AllEnemies" || target_type == "RandomEnemy")
        for (size_t i = 0; i < enemies_.size(); ++i) select_reticle(enemies_[i], true);
    else
        select_reticle(player_, true);
    refresh_tips();
}

bool CombatScreen::set_play_target(int enemy) {
    if (!play_single_ || enemy < 0 || enemy >= (int)enemies_.size() || !enemies_[enemy].node) return false;
    if (enemy == play_target_) return true;
    if (play_target_ >= 0) {
        /* NTargetManager.OnCreatureUnhovered + NCreature.OnUnfocus */
        select_reticle(enemies_[play_target_], false);
        show_nameplate(enemies_[play_target_], false);
    }
    play_target_ = enemy;
    /* OnCreatureHovered: highlight, reticle, the arrow to VfxSpawnPosition. */
    Creature& c = enemies_[enemy];
    if (play_card_ >= 0) {
        Holder& h = holders_[play_card_];
        if (enemy < (int)h.text_by_target.size()) show_card_text(h, h.text_by_target[enemy]);
    }
    select_reticle(c, true);
    show_nameplate(c, true);
    if (arrow_) {
        arrow_->modulate = Color(0xE6 / 255.0f, 0x1E / 255.0f, 0x1B / 255.0f); /* StsColors.targetingArrowEnemy */
        arrow_head_->scale = Vec2(0.95f, 0.95f);
        tweens_.tween(&arrow_head_->scale, Vec2(1.05f, 1.05f), 1.0f, TRANS_ELASTIC, EASE_OUT);
        arrow_to_ = c.center ? global_pos(c.center) : global_pos(c.node);
    }
    return true;
}

void CombatScreen::end_play() {
    if (play_card_ < 0 && !aiming_) return;
    for (size_t i = 0; i < enemies_.size(); ++i)
        if (enemies_[i].reticle && enemies_[i].reticle->modulate.a > 0) select_reticle(enemies_[i], false);
    if (player_.reticle && player_.reticle->modulate.a > 0) select_reticle(player_, false);
    if (play_target_ >= 0 && play_target_ < (int)enemies_.size()) show_nameplate(enemies_[play_target_], false);
    if (arrow_) {
        tweens_.kill(&arrow_head_->scale);
        arrow_->visible = false;
    }
    if (play_card_ >= 0) show_card_text(holders_[play_card_], holders_[play_card_].text);
    aiming_ = false;
    play_card_ = -1;
    play_id_ = -1;
    play_target_ = -1;
    play_single_ = false;
    layout_hand();
    refresh_tips();
}

/* NTargetingArrow._Ready: the segments and the head (StopDrawing: hidden). */
Node2D* CombatScreen::make_arrow() {
    Resource* seg = lib_->ext("images/ui/combat/targeting_arrow_segment.png");
    Resource* head = lib_->ext("images/ui/combat/targeting_arrow_head.png");
    Node2D* arrow = (Node2D*)SceneLibrary::create_node("Node2D");
    arrow->name = "TargetingArrow";
    arrow->visible = false;
    arrow->z_index = 2; /* over the played card (its holder: 1) */
    arrow_segments_.clear();
    for (int i = 0; i < 20; ++i) {
        Sprite2D* s = (Sprite2D*)SceneLibrary::create_node("Sprite2D");
        Resource* tex = i < 19 ? seg : head;
        s->texture = tex && tex->is_texture() ? (Texture2DRes*)tex : NULL;
        arrow->add_child(s);
        if (i < 19) arrow_segments_.push_back(s);
        else arrow_head_ = s;
    }
    arrow_head_->scale = Vec2(0.95f, 0.95f);
    return arrow;
}

static Vec2 rotated(const Vec2& v, float a) {
    float c = cosf(a), s = sinf(a);
    return Vec2(v.x * c - v.y * s, v.x * s + v.y * c);
}

static float angle_of(const Vec2& v) { return atan2f(v.y, v.x); }

static Vec2 bezier(const Vec2& v0, const Vec2& v1, const Vec2& c0, float t) {
    return v0 * ((1 - t) * (1 - t)) + c0 * (2 * (1 - t) * t) + v1 * (t * t);
}

/* NTargetingArrow.UpdateArrowPosition + UpdateSegments. */
void CombatScreen::update_arrow(const Vec2& target) {
    Vec2 from = play_card_ >= 0 ? global_pos(holders_[play_card_].node) : aim_from_;
    float rot = arrow_head_->rotation;
    arrow_head_->position = target + rotated(Vec2(0, 88), rot);
    Vec2 final_pos = target + rotated(Vec2(0, 40), rot);
    Vec2 head = arrow_head_->position, control;
    control.x = from.x - (head.x - from.x) * 0.25f;
    control.y = from.y > 540.0f ? head.y + (head.y - from.y) * 0.5f : head.y * 0.75f + from.y * 0.25f;
    arrow_head_->rotation = angle_of(target - control) + PI_F / 2;
    for (int i = 0; i < 19; ++i) {
        Node2D* s = arrow_segments_[i];
        float sc = 0.28f + (0.42f - 0.28f) * ((float)i * 2.0f / 19.0f); /* Mathf.Lerp, unclamped */
        s->scale = Vec2(sc, sc);
        s->position = bezier(from, final_pos, control, (float)i / 20.0f);
        if (i > 0) s->rotation = angle_of(s->position - arrow_segments_[i - 1]->position) + PI_F / 2;
    }
    arrow_segments_[0]->rotation = angle_of(arrow_segments_[0]->position - arrow_segments_[1]->position) - PI_F / 2;
}

/* NHandCardHolder.AnimPosition / AnimAngle / AnimScale (lerps at 7, 10
 * and 8 per second), the tweens, NTargetingArrow._Process (14). */
void CombatScreen::hold_end_turn(bool pressed, bool held) {
    if (pressed) long_press_on_ = true;
    if (!held) long_press_on_ = false;
}

/* NEndTurnLongPressBar._Process: held, the bar fills over 0.5 s (its width
 * to 204, its colour from blue-grey to gold, its alpha CubicOut); let go, it
 * drains; full, the turn ends and the bar fades (0.25 s). */
static void long_press_bar(Node* bar, double t, float fade) {
    if (!bar || bar->type != "ColorRect") return;
    ColorRect* r = (ColorRect*)bar;
    float k = (float)(t / 0.5);
    r->set_size(Vec2(k * 204.0f, 6.0f));
    r->color = Color(k * 2.5f, 0.6f + k, 0.6f, 1);
    float x = k * 0.75f - 1;
    r->modulate.a = (x * x * x + 1) * fade;
}

/* NPotion.DoBounce: the container 12 px up (0.125 s, Sine Out) and back
 * (0.125 s, Sine In); a child node of the potion, gone with it. */
class PotionBounce : public ScriptBehavior {
public:
    explicit PotionBounce(Control* c) : c_(c), top_(c->offset[SIDE_TOP]), bottom_(c->offset[SIDE_BOTTOM]), t_(0) {}
    virtual void process(Node* n, double dt) {
        t_ += dt;
        float k = t_ < 0.125 ? tween_interpolate(TRANS_SINE, EASE_OUT, (float)(t_ / 0.125), 0, 1, 1)
                             : 1.0f - tween_interpolate(TRANS_SINE, EASE_IN, clampf((float)((t_ - 0.125) / 0.125), 0, 1), 0, 1, 1);
        c_->offset[SIDE_TOP] = top_ - 12.0f * k;
        c_->offset[SIDE_BOTTOM] = bottom_ - 12.0f * k;
        if (t_ >= 0.25 && !n->queued_for_deletion()) {
            c_->offset[SIDE_TOP] = top_;
            c_->offset[SIDE_BOTTOM] = bottom_;
            n->queue_free();
        }
    }

private:
    Control* c_;
    float top_, bottom_;
    double t_;
};

void CombatScreen::process(double dt) {
    for (std::map<std::string, double>::iterator it = shakes_.begin(); it != shakes_.end();) {
        Creature* c = creature_for(it->first);
        it->second += dt;
        if (!c || !c->visuals || !c->visuals->is_node2d()) {
            shakes_.erase(it++);
            continue;
        }
        float k = (float)(it->second < 1.0 ? it->second : 1.0);
        float u = (1.0f - (1.0f - k) * (1.0f - k) * (1.0f - k)) * 6.2831853f;
        ((Node2D*)c->visuals)->position = Vec2(10.0f * sinf(u * 4.0f) * sinf(u * 0.5f), 0);
        if (it->second >= 1.0) shakes_.erase(it++);
        else ++it;
    }
    {
        Node* ui = root_ ? find_named(root_, "CombatUi") : NULL;
        Node* end = ui ? ui->get_node("EndTurnButton") : NULL;
        Node* bar = end ? find_named(end, "Bar") : NULL;
        if (long_press_fade_ > 0) {
            long_press_fade_ -= dt;
            if (bar && bar->is_canvas_item()) ((CanvasItem*)bar)->modulate.a = (float)(long_press_fade_ / 0.25 > 0 ? long_press_fade_ / 0.25 : 0);
            if (long_press_fade_ <= 0) {
                long_press_ready_ = true;
                long_press_on_ = false;
                if (bar && bar->type == "ColorRect") ((ColorRect*)bar)->color = Color(1, 0.85f, 0.36f, 1);
            }
        } else if (long_press_ready_ && long_press_on_) {
            long_press_t_ += dt;
            if (long_press_t_ > 0.5) {
                long_press_ready_ = false;
                long_press_t_ = 0;
                long_press_done_ = true;
                long_press_fade_ = 0.25;
                long_press_bar(bar, 0.5, 1);
            } else {
                long_press_bar(bar, long_press_t_, 1);
            }
        } else if (long_press_ready_ && long_press_t_ > 0) {
            long_press_t_ -= dt;
            if (long_press_t_ < 0) {
                long_press_t_ = 0;
                if (bar && bar->is_canvas_item()) ((CanvasItem*)bar)->modulate.a = 0;
            } else {
                long_press_bar(bar, long_press_t_, 1);
            }
        }
    }
    if (end_shiny_) {
        /* GlowPulse: GlowVfx's scale 0.5 -> 0.7 (Quart Out) and alpha
         * 0.4 -> 0 over 1.5 s, looping */
        end_glow_t_ += dt;
        Node* ui = root_ ? find_named(root_, "CombatUi") : NULL;
        Node* vfx = ui ? ui->get_node("EndTurnButton/Visuals/GlowVfx") : NULL;
        if (vfx && vfx->is_control()) {
            float p = (float)(fmod(end_glow_t_, 1.5) / 1.5);
            float s = 0.5f + 0.2f * tween_interpolate(TRANS_QUART, EASE_OUT, p, 0, 1, 1);
            ((Control*)vfx)->scale = Vec2(s, s);
            ((Control*)vfx)->modulate.a = 0.4f * (1.0f - p);
        }
    }
    for (size_t i = 0; i < potion_shines_.size(); ++i) {
        if (potion_shines_[i] < 0 || life_t_ < potion_shines_[i]) continue;
        potion_shines_[i] = -1;
        /* NPotionHolder.ShineOnStartOfCombat: NPotion.DoBounce (12 px up and
         * back, 0.125 s each, Sine), a slosh 0.25 s later (0.3, Large) */
        Node* bar = root_ ? root_->get_node("TopBarLayer") : NULL;
        Node* holders = bar ? find_named(bar, "PotionHolders") : NULL;
        Node* holder = holders && i < holders->children.size() ? holders->children[i] : NULL;
        Node* potion = holder ? find_named(holder, "Container") : NULL;
        if (!potion || !potion->is_control()) continue;
        /* the bounce lives on the potion (the top bar is rebuilt with
         * each decision) */
        Node* b = SceneLibrary::create_node("Node");
        b->name = "Bounce";
        b->behavior = new PotionBounce((Control*)potion);
        potion->add_child(b);
        slosh_at_.push_back(life_t_ + 0.25);
    }
    for (size_t i = slosh_at_.size(); i-- > 0;) {
        if (life_t_ < slosh_at_[i]) continue;
        slosh_at_.erase(slosh_at_.begin() + (long)i);
        if (lib_->audio()) lib_->audio()->ui(format("potion_slosh_%d.mp3", (int)(chaotic_rand() % 3) + 1), 0.3f);
    }
    float t = (float)dt;
    tweens_.update(dt);
    peek_.frame(dt);
    for (size_t i = fading_trails_.size(); i-- > 0;) {
        Trail& tr = fading_trails_[i];
        tr.t += dt;
        if (tr.node->is_canvas_item()) ((CanvasItem*)tr.node)->modulate.a = tr.t < 0.5 ? (float)(1.0 - tr.t / 0.5) : 0.0f;
        if (tr.t >= 0.5) {
            if (tr.node->parent) tr.node->parent->remove_child(tr.node);
            delete tr.node;
            fading_trails_.erase(fading_trails_.begin() + (long)i);
        }
    }
    update_shuffle_flies(dt);
    for (size_t i = moving_cards_.size(); i-- > 0;) {
        MovingCard& m = moving_cards_[i];
        if (m.kind == 3) {
            update_in_play(m, dt);
            if (m.burn_at >= 0 && m.t >= m.burn_at) {
                m.holder.node->modulate = Color(1, 1, 1, 1);
                spawn_card_exhaust(lib_, exhaust_ui(), m.holder.node, false);
                moving_cards_.erase(moving_cards_.begin() + (long)i);
            }
            continue;
        }
        if (m.kind != 0) {
            if (!fly(m, dt)) moving_cards_.erase(moving_cards_.begin() + (long)i);
            continue;
        }
        m.t += dt;
        float u = clampf((float)(m.t / m.duration), 0, 1);
        float k = tween_interpolate(TRANS_CUBIC, EASE_OUT, u, 0, 1, 1);
        Vec2 at = m.from + (m.to - m.from) * k;
        place(m.holder.node, at.x, at.y, 0, 0);
        float sc = m.from_scale + (m.to_scale - m.from_scale) * k;
        m.holder.node->scale = Vec2(sc, sc);
        if (!m.exhaust) m.holder.node->modulate.a = 1 - u;
        if (u >= 1) {
            if (m.exhaust) {
                spawn_card_exhaust(lib_, exhaust_ui(), m.holder.node, false);
            } else {
                if (m.holder.node->parent) m.holder.node->parent->remove_child(m.holder.node);
                delete m.holder.node;
            }
            moving_cards_.erase(moving_cards_.begin() + (long)i);
        }
    }
    for (size_t i = 0; i < holders_.size(); ++i) {
        Holder& h = holders_[i];
        h.pos = h.pos + (h.to_pos - h.pos) * minf(1.0f, t * 7.0f);
        Vec2 d = h.to_pos - h.pos;
        if (d.x * d.x + d.y * d.y < 1.0f) h.pos = h.to_pos;
        h.angle += (h.to_angle - h.angle) * minf(1.0f, t * 10.0f);
        if (fabsf(h.angle - h.to_angle) < 0.1f) h.angle = h.to_angle;
        h.scale += (h.to_scale - h.scale) * minf(1.0f, t * 8.0f);
        if (fabsf(h.to_scale - h.scale) < 0.002f) h.scale = h.to_scale;
        place(h.node, h.pos.x, h.pos.y, 0, 0);
        h.node->rotation = h.angle * PI_F / 180.0f;
        h.node->scale = Vec2(h.scale, h.scale);
    }
    for (size_t i = 0; i < selected_.size(); ++i) {
        Holder& h = selected_[i];
        h.pos = h.pos + (h.to_pos - h.pos) * minf(1.0f, t * 12.0f);
        Vec2 d = h.to_pos - h.pos;
        if (d.x * d.x + d.y * d.y < 1.0f) h.pos = h.to_pos;
        h.angle += (h.to_angle - h.angle) * minf(1.0f, t * 12.0f);
        h.scale += (h.to_scale - h.scale) * minf(1.0f, t * 12.0f);
        place(h.node, h.pos.x, h.pos.y, 0, 0);
        h.node->rotation = h.angle * PI_F / 180.0f;
        h.node->scale = Vec2(h.scale, h.scale);
    }
    /* The tips follow their card (SetAlignmentForCardHolder): the text right
     * of its hitbox and the cards they name left of it (the sides swapped
     * past three quarters of the screen; a star cost moves the cards 15 px
     * further), kept on the screen. */
    if (tips_ && tips_for_ >= 0 && tips_for_ < (int)holders_.size()) {
        const Holder& th = holders_[(size_t)tips_for_];
        Control* hit = ctl(th.node, "Hitbox");
        Control *text, *cards;
        tip_parts(tips_, &text, &cards);
        bool sized = (!text || text->size.y > 0) && (!cards || cards->size.y > 0);
        if (hit && sized) {
            Xform2D xf = hit->global_transform();
            float sx = sqrtf(xf.x.x * xf.x.x + xf.x.y * xf.x.y);
            Vec2 at = xf.origin;
            bool left = th.node->global_transform().origin.x > 1920.0f * 0.75f;
            float star = !left && th.data["star_cost"].i() > 0 ? 15.0f : 0.0f;
            place_tips_beside(tips_, Vec2(at.x - star, at.y), offset_size(hit).x * sx + star, left, 10.0f);
            tips_->modulate.a = 1;
        }
    }
    /* A focused creature's tips: beside its hitbox, on the side towards the
     * middle of the screen (HoverTip.GetHoverTipAlignment), the cards they
     * name on the other side (SetAlignment). */
    if (creature_tips_ && creature_focus_) {
        Creature* c = creature_of(creature_focus_);
        Control *text, *cards;
        tip_parts(creature_tips_, &text, &cards);
        bool sized = (!text || text->size.y > 0) && (!cards || cards->size.y > 0);
        if (c && sized) {
            Xform2D xf = c->node->global_transform();
            Vec2 a = xf.xform(c->hitbox.pos), b = xf.xform(c->hitbox.pos + c->hitbox.size);
            bool left = (a.x + b.x) * 0.5f > 960.0f;
            Vec2 at(a.x, a.y < 0 ? 0 : a.y);
            place_tips_beside(creature_tips_, at, b.x - a.x, left, 10.0f);
            creature_tips_->modulate.a = 1;
        }
    }
    /* The steps' cues as their time comes; the damage numbers fly
     * (NDamageNumVfx._Process: velocity, gravity 2000 px/s2). */
    if (!paused_) cue_t_ += dt;
    life_t_ += dt;
    if (radial_off_at_ > 0 && life_t_ >= radial_off_at_) {
        radial_off_at_ = 0;
        hide(root_ ? find_named(root_, "RadialBlur") : NULL);
    }
    update_cost_rolls(dt);
    for (size_t i = creature_moves_.size(); i-- > 0;) {
        CreatureMove& m = creature_moves_[i];
        m.t += (float)dt;
        float k = tween_interpolate(m.trans, m.ease, m.t < m.duration ? m.t : m.duration, 0, 1, m.duration);
        m.node->set_position(m.from + (m.to - m.from) * k);
        if (m.t >= m.duration) creature_moves_.erase(creature_moves_.begin() + (long)i);
    }
    for (size_t i = 0; i < orbs_.size(); ++i) {
        OrbSlot& o = orbs_[i];
        o.t += dt;
        float k = tween_interpolate(TRANS_SINE, EASE_IN_OUT, (float)(o.t < 0.45 ? o.t : 0.45), 0, 1, 0.45f);
        o.node->set_position(o.from + (o.to - o.from) * k);
        if (o.bg_t < 0.25f) {
            o.bg_t = minf(o.bg_t + (float)dt, 0.25f);
            Node* dark = orb_export(o.sprite, "_darkBg");
            float s = o.bg_from + (o.bg_to - o.bg_from) * o.bg_t / 0.25f;
            if (dark && dark->is_node2d()) ((Node2D*)dark)->scale = Vec2(s, s);
        }
    }
    for (size_t i = fading_orbs_.size(); i-- > 0;) {
        FadingOrb& o = fading_orbs_[i];
        if (life_t_ >= o.until) {
            o.node->queue_free();
            fading_orbs_.erase(fading_orbs_.begin() + (long)i);
        } else o.node->modulate.a = (float)(o.until - life_t_) / 0.25f;
    }
    while (!paused_ && next_cue_ < cues_.size() && cues_[next_cue_].at <= cue_t_) {
        const Json& ch = cues_[next_cue_].data;
        if (watch_shuffle_ && ch["t"].str() == "card" && ch["from"].str() == "discard" && ch["to"].str() == "draw") {
            /* CardPileCmd.ShuffleFtueCheck: the FTUE before the shuffle */
            watch_shuffle_ = false;
            shuffle_hit_ = paused_ = true;
            break;
        }
        fire(cues_[next_cue_++]);
    }
    for (size_t i = delayed_hides_.size(); i-- > 0;)
        if (life_t_ >= delayed_hides_[i].at && (!death_delayed(delayed_hides_[i].visuals) || life_t_ >= delayed_hides_[i].at + 10)) {
            delayed_hides_[i].visuals->modulate.a = 0;
            if (delayed_hides_[i].node) delayed_hides_[i].node->visible = false;
            delayed_hides_.erase(delayed_hides_.begin() + (long)i);
        }
    /* AnimDie's end: a dying monster whose body NMonsterDeathVfx took is
     * gone (QueueFreeSafely; the rest of it had faded) */
    for (size_t i = 0; i < dying_.size(); ++i) {
        Creature* c = creature_for(dying_[i]);
        if (!c || !c->node || !c->node->visible) continue;
        std::map<std::string, double>::iterator g = fade_gone_at_.find(dying_[i]);
        if (c->visuals && (g == fade_gone_at_.end() || life_t_ < g->second)) continue;
        c->node->visible = false;
    }
    for (size_t i = banners_.size(); i-- > 0;)
        if (life_t_ >= banners_[i].until) {
            banners_[i].node->queue_free();
            banners_.erase(banners_.begin() + (long)i);
        }
    if (arrow_ && arrow_->visible && (play_card_ >= 0 || aiming_)) {
        arrow_pos_ = arrow_pos_ + (arrow_to_ - arrow_pos_) * minf(1.0f, t * 14.0f);
        update_arrow(arrow_pos_);
    }
}

static void play_idle(Node* n) {
    if (n->type == "SpineSprite") ((SpineSprite*)n)->set_animation("idle_loop", true, 0);
    for (size_t i = 0; i < n->children.size(); ++i) play_idle(n->children[i]);
}

static SpineSprite* first_spine(Node* n) {
    if (!n) return NULL;
    if (n->type == "SpineSprite") return (SpineSprite*)n;
    for (size_t i = 0; i < n->children.size(); ++i) {
        SpineSprite* s = first_spine(n->children[i]);
        if (s) return s;
    }
    return NULL;
}

/* NCreatureVisuals.SetUpSkin and UpdatePhobiaMode, after the idle starts:
 * what the monster's SetupSkins did to its spine (the logic records it:
 * spine_ops), then phobia mode: the %PhobiaModeVisuals body shown instead
 * of %Visuals when there is one, and a spine with "normal" / "phobia"
 * skins in the mode's (MonsterModel.OnPhobiaModeToggled). */
static void phobia_mode(Node* visuals, const Json& data) {
    if (!visuals) return;
    bool phobia = client_settings()["phobia_mode"].b();
    Node* alt = visuals->find_unique("PhobiaModeVisuals");
    Node* body = visuals->find_unique("Visuals");
    if (alt && body) {
        set_visible(alt, phobia);
        set_visible(body, !phobia);
    }
    SpineSprite* s = first_spine(body ? body : visuals);
    if (s && data["phobia_skin"].b()) s->set_skins(std::vector<std::string>(1, phobia ? "phobia" : "normal"));
}

/* CreatureAnimator.OffsetLoopingAnimation on the entry just set or queued
 * ("offset": a speed of 0.9-1.1, its start within 0.1 s of its loop point)
 * and its constructor's idle_loop started anywhere ("random_start"), rolled
 * here as the game rolls Rng.Chaotic. */
static void time_animator_op(SpineSprite* s, const Json& op) {
    bool offset = op["offset"].b(), anywhere = op["random_start"].b();
    SpAnimationState* st = offset || anywhere ? s->animation_state() : NULL;
    int track = op["track"].i();
    float end = st ? st->last_duration(track) : 0;
    if (end <= 0) return;
    float speed = offset ? 0.9f + 0.2f * (float)(chaotic_rand() % 10001) / 10000.0f : -1.0f;
    float at = anywhere ? end * (float)(chaotic_rand() % 10000) / 10000.0f
                        : fmodf(end + 0.2f * (float)(chaotic_rand() % 10001) / 10000.0f - 0.1f, end);
    st->time_last(track, speed, at);
}

bool play_animator_ops(SpineSprite* s, const Json& ops) {
    if (!s) return false;
    size_t played = 0;
    for (size_t i = 0; i < ops.size(); ++i) {
        const Json& op = ops[i];
        const std::string& kind = op["op"].str();
        if (kind != "set_animation" && kind != "add_animation") continue; /* "bounds": the screen's */
        bool ok = kind == "set_animation" ? s->set_animation(op["name"].str(), op["loop"].b(true), op["track"].i())
                                          : s->add_animation(op["name"].str(), op["loop"].b(true), op["delay"].f(), op["track"].i());
        if (!ok) return played > 0;
        time_animator_op(s, op);
        ++played;
    }
    return played > 0;
}

void setup_creature_spine(Node* visuals, const Json& data) {
    if (!visuals) return;
    Node* body = visuals->find_unique("Visuals");
    SpineSprite* s = first_spine(body ? body : visuals);
    const Json& ops = s ? data["spine_ops"] : Json();
    for (size_t i = 0; i < ops.size(); ++i) {
        const Json& op = ops[i];
        const std::string& kind = op["op"].str();
        /* "node": a spine sprite under the body's (the Bowlbug Egg's
         * cocoon, CocoonSlotNode/Cocoon) */
        SpineSprite* t = s;
        if (op["node"].is_string()) {
            Node* sub = s->get_node(op["node"].str());
            if (!sub || sub->type != "SpineSprite") continue;
            t = (SpineSprite*)sub;
        }
        if (kind == "skin") {
            std::vector<std::string> names;
            for (size_t k = 0; k < op["skins"].size(); ++k) names.push_back(op["skins"][k].str());
            t->set_skins(names);
        } else if (kind == "set_animation") {
            t->set_animation(op["name"].str(), op["loop"].b(true), op["track"].i());
        } else if (kind == "add_animation") {
            t->add_animation(op["name"].str(), op["loop"].b(true), op["delay"].f(), op["track"].i());
        } else if (kind == "add_empty_animation") {
            t->add_empty_animation(op["track"].i());
        } else if (kind == "time_scale") {
            t->set_state_time_scale(op["scale"].f(1));
        }
    }
    /* the animator's first state (low health, ...) over the idle */
    if (s && data["anim_init"].is_array()) play_animator_ops(s, data["anim_init"]);
    phobia_mode(visuals, data);
}

/* NGame.PhobiaModeToggled: the creatures and the background layers change
 * (NCreature / NCombatBackgroundLayer.UpdatePhobiaMode). */
static void phobia_layers(Node* n, bool phobia) {
    Node* alt = n->get_node("PhobiaModeVisual");
    Node* normal = n->get_node("Visual");
    if (alt && normal) {
        set_visible(alt, phobia);
        set_visible(normal, !phobia);
    }
    for (size_t i = 0; i < n->children.size(); ++i) phobia_layers(n->children[i], phobia);
}

void CombatScreen::update_phobia_mode() {
    for (size_t i = 0; i < enemies_.size(); ++i) phobia_mode(enemies_[i].visuals, enemies_[i].data);
    if (root_) phobia_layers(root_, client_settings()["phobia_mode"].b());
}

/* A pet other than Osty (NCombatRoom.AddCreature, NCreature._Ready): its
 * visuals in the skin its relic picked (MonsterModel.SetupSkins), no
 * state display and not targetable (IsHealthBarVisible is false). */
bool CombatScreen::make_pet(const Json& pet, Creature& out) {
    if (!make_creature(pet["id"].str(), false, out)) return false;
    SpineSprite* s = first_spine(out.visuals);
    if (s && pet["skin"].is_string()) s->set_skin(pet["skin"].str());
    hide(out.node->find_unique("HealthBar"));
    hide(out.node->find_unique("Intents"));
    out.data = pet;
    return true;
}

/* PositionPlayersAndPets / AddCreature: the pets at the player's feet,
 * 20 px in from where the player's slot began, spread over the player's
 * width when there are several, 10 px lower; after the player in draw
 * order. */
void CombatScreen::position_pets(float pets_x) {
    if (pets_.empty() || !player_.node) return;
    float step = pets_.size() > 1 ? player_.width / (float)(pets_.size() - 1) : 0.0f;
    float y = offset_pos(player_.node).y + 10.0f;
    Node* allies = player_.node->parent;
    for (size_t i = 0; i < pets_.size(); ++i) {
        Creature& p = pets_[i];
        if (!p.node) continue;
        p.node->set_position(Vec2(-pets_x + 20.0f - (float)i * step - p.width * 0.5f, y));
        if (allies && p.node->parent == allies) {
            /* MoveChild(pet, player's index + 1 + i). */
            std::vector<Node*>& ch = allies->children;
            ch.erase(std::find(ch.begin(), ch.end(), (Node*)p.node));
            std::vector<Node*>::iterator at = std::find(ch.begin(), ch.end(), (Node*)player_.node);
            ch.insert(at == ch.end() ? ch.end() : at + 1 + (std::ptrdiff_t)i, p.node);
        }
    }
}

/* NOrbManager (scenes/orbs/orb_manager.tscn) on the player: one NOrb per
 * slot (OrbQueue.Capacity), the channelled orbs first, on an arc above the
 * creature (TweenLayout's places at rest). */
void CombatScreen::make_orbs(Creature& c, const Json& orbs, int slots) {
    if (slots <= 0 || !c.node || !c.visuals) return;
    Node* manager = instance(lib_, "scenes/orbs/orb_manager.tscn");
    if (!manager || !manager->is_control()) {
        delete manager;
        return;
    }
    Control* m = (Control*)manager;
    /* NCreature.SetOrbManagerPosition: the visuals' %OrbPos (else the
     * intents' place), scaled as the visuals are (at most 1.25). */
    Vec2 vscale(1, 1);
    if (c.visuals->is_node2d()) vscale = ((Node2D*)c.visuals)->scale;
    Node* pos = c.visuals->find_unique("OrbPos");
    if (!pos) pos = c.visuals->find_unique("IntentPos");
    Vec2 at = pos && pos->is_node2d() ? ((Node2D*)pos)->position : Vec2(0, 0);
    float k = vscale.x < 1.25f ? vscale.x : 1.25f;
    for (int s = 0; s < 4; ++s) m->anchor[s] = 0;
    m->scale = vscale.x > 1.0f ? Vec2(1, 1) : Vec2(vscale.x + (1 - vscale.x) * 0.5f, vscale.y + (1 - vscale.y) * 0.5f);
    m->set_position(at * k);
    c.node->add_child(m);
    Node* box = m->find_unique("Orbs");
    if (!box) box = m;
    orb_box_ = box;
    sync_orbs(orbs, slots, false);
}

static Node* orb_export(Node* n, const char* key) {
    const Json* p = n ? n->get_extra(key) : NULL;
    return p ? n->get_node((*p)["np"].str()) : NULL;
}

/* NParticlesContainer.Restart / SetEmitting use the exported particle list. */
static void orb_particles(Node* n, bool restart, bool emitting = true) {
    if (!n) return;
    if (n->type == "GPUParticles2D" || n->type == "CPUParticles2D") {
        Particles2D* p = (Particles2D*)n;
        if (restart) p->restart();
        else p->emitting = emitting;
        return;
    }
    const Json* paths = n->get_extra("_particles");
    for (size_t i = 0; paths && i < paths->size(); ++i)
        orb_particles(n->get_node((*paths)[i]["np"].str()), restart, emitting);
}

void CombatScreen::sync_orbs(const Json& models, int slots, bool animate) {
    if (!orb_box_) {
        if (slots > 0) make_orbs(player_, models, slots);
        return;
    }
    if (slots < 0) slots = 0;
    if (slots > 10) slots = 10; /* OrbQueue.maxCapacity */
    std::vector<OrbSlot> next;
    std::vector<bool> kept(orbs_.size(), false);
    float radius = 225.0f + 75.0f * ((float)slots - 3.0f) / 7.0f;
    float step = slots > 1 ? 125.0f / (float)(slots - 1) : 0.0f;
    for (int i = 0; i < slots; ++i) {
        Json model = i < (int)models.size() ? models[(size_t)i] : Json();
        int id = model.is_null() ? -1 - i : model["instance_id"].i(i + 1);
        OrbSlot o;
        o.node = NULL;
        o.sprite = NULL;
        o.bg_from = o.bg_to = 1;
        o.bg_t = 1;
        for (size_t j = 0; j < orbs_.size(); ++j)
            if (!kept[j] && orbs_[j].id == id && orbs_[j].data["id"] == model["id"]) {
                o = orbs_[j]; kept[j] = true; break;
            }
        if (!o.node) {
            Node* n = instance(lib_, "scenes/orbs/orb.tscn");
            if (!n || !n->is_control()) { delete n; continue; }
            o.node = (Control*)n;
            o.node->scale = o.node->scale * 0.85f;
            for (int s = 0; s < 4; ++s) o.node->anchor[s] = 0;
            o.node->set_position(Vec2(0, 0));
            if (!model.is_null()) {
                o.sprite = instance(lib_, "scenes/orbs/orb_visuals/" + lower(model["id"].str()) + ".tscn");
                Node* vc = n->find_unique("VisualContainer");
                if (o.sprite && vc) { vc->add_child(o.sprite); play_idle(o.sprite); }
                else { delete o.sprite; o.sprite = NULL; }
            }
            orb_box_->add_child(n);
        }
        o.id = id;
        o.node->name = format("CombatOrb_%d", id);
        o.data = model;
        o.from = Vec2(o.node->offset[SIDE_LEFT], o.node->offset[SIDE_TOP]);
        float a = (-150.0f + (float)i * step) * PI_F / 180.0f;
        o.to = Vec2(-cosf(a), sinf(a)) * radius;
        o.t = animate ? 0 : 0.45;
        if (!animate) o.node->set_position(o.to);
        set_visible(o.node->find_unique("SelectionReticle"), false);
        set_visible(o.node->find_unique("Outline"), model.is_null());
        set_visible(o.node->find_unique("LabelContainer"), !model.is_null());
        std::string kind = lower(model["id"].str());
        set_label(o.node->find_unique("PassiveAmount"), format("%d", model["passive"].i()));
        set_label(o.node->find_unique("EvokeAmount"), format("%d", model["evoke"].i()));
        set_visible(o.node->find_unique("PassiveAmount"), !model.is_null() && kind != "plasma_orb");
        set_visible(o.node->find_unique("EvokeAmount"), kind == "dark_orb");
        const Json* focus_power = find_power(player_.powers, "FOCUS_POWER");
        bool focus = focus_power && (*focus_power)["amount"].i() > 0;
        orb_particles(orb_export(o.sprite, "_focusedParticles"), false, focus && (kind != "glass_orb" || model["passive"].i() > 0));
        orb_particles(orb_export(o.sprite, "_superchargedParticles"), false, model["evoke"].i() >= 24);
        /* NDarkOrbVfx.UpdateDarkBgSize: its _darkBg at 1.2 supercharged, 1
         * otherwise (0.25 s) */
        float bg = kind == "dark_orb" && model["evoke"].i() >= 24 ? 1.2f : 1.0f;
        if (bg != o.bg_to) {
            Node* dark = kind == "dark_orb" ? orb_export(o.sprite, "_darkBg") : NULL;
            o.bg_from = dark && dark->is_node2d() ? ((Node2D*)dark)->scale.x : o.bg_to;
            o.bg_to = bg;
            o.bg_t = 0;
        }
        next.push_back(o);
    }
    for (size_t i = 0; i < orbs_.size(); ++i) if (!kept[i]) {
        FadingOrb f = { orbs_[i].node, life_t_ + 0.25 };
        fading_orbs_.push_back(f);
    }
    orbs_ = next;
}

void CombatScreen::orb_effect(const std::string& path, const Vec2& at) {
    if (!vfx_box_ || path.empty()) return;
    Node* fx = instance(lib_, "scenes/" + path + ".tscn");
    if (!fx) return;
    Vec2 local = inverse(vfx_box_->global_transform()).xform(at);
    if (fx->is_node2d()) ((Node2D*)fx)->position = local;
    else if (fx->is_control()) ((Control*)fx)->set_position(local);
    vfx_box_->add_child(fx);
}

/* NOrbVfx receives the activation before damage or block is applied.
 * The sprite stays alive during NOrbManager's 0.25 s evoke fade. */
void CombatScreen::activate_orb(const Json& cue) {
    for (size_t i = 0; i < orbs_.size(); ++i) {
        OrbSlot& o = orbs_[i];
        if (o.id != cue["instance_id"].i(-100)) continue;
        bool evoke = cue["evoking"].b();
        std::string kind = lower(cue["id"].str());
        if (evoke) {
            const Json* scene = o.sprite ? o.sprite->get_extra("_evokeVfxSceneName") : NULL;
            if (scene && o.sprite->is_canvas_item())
                orb_effect("vfx/orbs/" + scene->str(), ((CanvasItem*)o.sprite)->global_transform().origin);
        } else if (kind != "glass_orb" || cue["passive"].i() > 0) {
            orb_particles(orb_export(o.sprite, "_passiveActivatedParticles"), true);
            const Json* focus = find_power(player_.powers, "FOCUS_POWER");
            if (focus && (*focus)["amount"].i() > 0)
                orb_particles(orb_export(o.sprite, "_passiveActivatedFocusedParticles"), true);
            float strength = kind == "glass_orb" ? (float)cue["passive"].num() / 4 : 1;
            double duration = kind == "lightning_orb" ? 0.4 : kind == "dark_orb" ? 0.55 : 0.5;
            if (kind == "dark_orb" && !(focus && (*focus)["amount"].i() > 0)) strength = 0.65f;
            shake_vfx(orb_export(o.sprite, "_spineShaker"), strength, duration);
        }
        if (kind == "frost_orb" && player_.center)
            orb_effect(evoke ? "vfx/orbs/frost/vfx_frost_orb_evoke_shield" : "vfx/orbs/frost/vfx_frost_orb_passive_shield",
                       ((CanvasItem*)player_.center)->global_transform().origin);
        if (evoke && kind == "glass_orb")
            for (size_t j = 0; j < cue["targets"].size(); ++j) {
                Creature* target = creature_for(cue["targets"][j].str());
                if (target && target->center)
                    orb_effect("vfx/orbs/glass/vfx_glass_orb_evoke_impact", ((CanvasItem*)target->center)->global_transform().origin);
            }
        if (evoke && kind == "dark_orb" && o.sprite && o.sprite->is_canvas_item())
            for (size_t j = 0; j < cue["targets"].size(); ++j) {
                Creature* target = creature_for(cue["targets"][j].str());
                if (!target || !target->center) continue;
                Node* projectile = make_vfx_projectile(lib_, "vfx/orbs/dark/vfx_dark_orb_evoke_projectile_handler",
                    "vfx/orbs/dark/vfx_dark_orb_evoke_projectile", ((CanvasItem*)o.sprite)->global_transform().origin,
                    ((CanvasItem*)target->center)->global_transform().origin);
                if (projectile && vfx_box_) vfx_box_->add_child(projectile); else delete projectile;
            }
        if (kind == "plasma_orb" && o.sprite && o.sprite->is_canvas_item() && player_.center) {
            Vec2 range;
            const Json* offsets = o.sprite->get_extra("_projectileOffsetRange");
            if (offsets) jv_vec2(*offsets, range);
            const Json* interval = o.sprite->get_extra("_projectileSpawnInterval");
            for (int j = 0; j < (evoke ? 2 : 1); ++j) {
                float angle = (float)(chaotic_rand() % 100000) / 100000 * 2 * PI_F;
                float radius = range.x + (range.y - range.x) * ((float)(chaotic_rand() % 100000) / 100000);
                Vec2 source = ((CanvasItem*)o.sprite)->global_transform().origin + Vec2(cosf(angle), sinf(angle)) * radius;
                Node* projectile = make_vfx_projectile(lib_, "vfx/orbs/plasma/vfx_plasma_orb_projectile_handler",
                    "vfx/orbs/plasma/vfx_plasma_orb_projectile", source, ((CanvasItem*)player_.center)->global_transform().origin,
                    j * (interval ? interval->num(0.05) : 0.05));
                if (projectile && vfx_box_) vfx_box_->add_child(projectile); else delete projectile;
            }
        }
        break;
    }
}

/* NCreatureStateDisplay.SetCreature: the nameplate's text. */
static void set_nameplate_text(Control* nameplate, const std::string& name) {
    Node* label = nameplate ? nameplate->find_unique("NameplateLabel") : NULL;
    if (label && label->type == "Label") ((Label*)label)->set_text(name);
}

Node* CombatScreen::build(const Json& d, std::string* error) {
    Node* root = instance(lib_, "scenes/rooms/combat_room.tscn");
    if (!root) {
        if (error) *error = "no combat room scene";
        return NULL;
    }
    const Json& view = d["view"];
    Node* scene = root->find_unique("CombatSceneContainer");
    Node* bg_box = root->find_unique("BgContainer");
    Node* allies = root->find_unique("AllyContainer");
    Node* enemies = root->find_unique("EnemyContainer");
    if (!scene || !bg_box || !allies || !enemies) {
        if (error) *error = "combat room scene: missing containers";
        delete root;
        return NULL;
    }
    float scaling = (float)view["camera_scaling"].num(1.0);
    if (scaling <= 0) scaling = 1;
    if (scene->is_control()) {
        Control* sc = (Control*)scene;
        sc->scale = Vec2(scaling, scaling);
        const Json& off = view["camera_offset"];
        if (off.size() == 2) {
            sc->offset[SIDE_LEFT] += (float)off[(size_t)0].num();
            sc->offset[SIDE_RIGHT] += (float)off[(size_t)0].num();
            sc->offset[SIDE_TOP] += (float)off[1].num();
            sc->offset[SIDE_BOTTOM] += (float)off[1].num();
        }
    }
    Node* bg = make_background(view["background"]);
    if (bg) bg_box->add_child(bg);

    /* Allies: the player (pets later). */
    std::vector<Creature> ally_nodes, enemy_nodes;
    enemies_.clear();
    /* After the fight: the player as the reward decision has them, no powers. */
    static const Json no_powers;
    const Json& player = aftermath_ && aftermath_player_.is_object() ? aftermath_player_ : d["player"];
    const Json& player_powers = aftermath_ ? no_powers : d["player_powers"];
    Creature c;
    if (make_creature(player["character"].is_string() ? player["character"].str() : std::string("ironclad"), true, c)) {
        allies->add_child(c.node);
        update_bounds(c);
        c.data = player;
        c.powers = player_powers;
        refresh_state(c, false);
        set_nameplate_text(c.nameplate, player["name"].str());
        make_powers(c, player_powers);
        if (!aftermath_) make_orbs(c, d["orbs"], d["orb_slots"].i(0));
        ally_nodes.push_back(c);
        player_ = c;
    }
    /* Necrobinder's Osty, while alive: its size by its max HP
     * (OstyScaleToSize: 1x to 2x at 150). */
    osty_.node = NULL;
    const Json& osty = d["osty"];
    float osty_max_hp = (float)osty["max_hp"].num(0);
    if (!aftermath_ && osty.is_object() && osty["alive"].b() && make_creature("osty", false, c)) {
        if (c.visuals && c.visuals->is_node2d()) {
            float k = 1.0f + clampf(osty_max_hp / 150.0f, 0.0f, 1.0f);
            c.scale_base = ((Node2D*)c.visuals)->scale;
            c.scale_osty = k;
            ((Node2D*)c.visuals)->scale = ((Node2D*)c.visuals)->scale * k;
        }
        allies->add_child(c.node);
        update_bounds(c);
        c.data = osty;
        c.powers = no_powers;
        refresh_state(c, false);
        set_nameplate_text(c.nameplate, osty["name"].str());
        osty_ = c;
    }
    pets_.clear();
    pet_who_.clear();
    const Json& ps = d["pets"];
    for (size_t i = 0; !aftermath_ && i < ps.size(); ++i) {
        if (!ps[i]["alive"].b(true) || !make_pet(ps[i], c)) continue;
        allies->add_child(c.node);
        update_bounds(c);
        play_idle(c.node);
        pets_.push_back(c);
        pet_who_.push_back(format("e%d", ps[i]["combat_id"].i(-1)));
    }
    static const Json no_enemies;
    const Json& es = aftermath_ ? no_enemies : d["enemies"];
    enemy_who_.clear();
    cues_.clear();
    next_cue_ = 0;
    cue_t_ = 0;
    busy_until_ = 0;
    power_fly_seconds_ = -1;
    rewards_at_ = -1;
    death_end_.clear();
    fade_gone_at_.clear();
    sel_confirm_placed_ = NULL;
    root_ = root;
    state_ = d;
    vfx_box_ = ctl(root, "CombatVfxContainer");
    hand_ = d["hand"];
    tips_ = NULL;
    tips_for_ = -2;
    std::vector<int> enemy_node_ids;
    ghosts_.clear();
    /* NCombatRoom.CreateEnemyNodes: an encounter with its own scene
     * (scenes/encounters/<id>) puts it first in the enemy container, 960,
     * 540 up and to the left; its enemies stand at their slots' markers */
    encounter_slots_ = NULL;
    beast_bg_.clear();
    beast_bg_state_ = 0;
    beast_ringing_ = beast_progress_ = -1;
    if (view["has_scene"].b() && view["encounter"].is_string()) {
        Node* s = instance(lib_, "scenes/encounters/" + lower(view["encounter"].str()) + ".tscn");
        if (s && s->is_control()) {
            encounter_slots_ = (Control*)s;
            encounter_slots_->set_position(Vec2(-960, -540));
            if (enemies->children.empty()) enemies->add_child(s);
            else enemies->add_child_before(s, enemies->children[0]);
        } else {
            delete s;
        }
    }
    for (size_t i = 0; i < es.size(); ++i) {
        enemy_who_.push_back(format("e%d", es[i]["combat_id"].i(-1)));
        if (!make_creature(es[i]["id"].str(), false, c)) {
            enemies_.push_back(c); /* node NULL: keeps enemies_[i] = enemy i */
            continue;
        }
        c.hp_bar_reduction = es[i]["hp_bar_reduction"].f();
        c.node->name = format("CombatEnemy_%d", es[i]["combat_id"].i(-1));
        enemies->add_child(c.node);
        update_bounds(c);
        c.data = es[i];
        c.powers = es[i]["powers"];
        refresh_state(c, false);
        set_nameplate_text(c.nameplate, es[i]["name"].str());
        make_intents(c, es[i]["intents"], (int)i);
        make_powers(c, es[i]["powers"]);
        enemy_nodes.push_back(c);
        enemy_node_ids.push_back(es[i]["combat_id"].i(-1));
        enemies_.push_back(c);
    }
    /* The enemies of the decision before that are gone now: they stay
     * where they stood while their death plays (play_steps). */
    const Json& before = previous_["enemies"];
    for (size_t i = 0; !aftermath_ && places_ && i < before.size(); ++i) {
        int id = before[i]["combat_id"].i(-1);
        bool here = false;
        for (size_t k = 0; k < es.size(); ++k)
            if (es[k]["combat_id"].i(-2) == id) here = true;
        if (here || places_->find(id) == places_->end()) continue;
        Ghost g;
        if (!make_creature(before[i]["id"].str(), false, g.c)) continue;
        g.c.hp_bar_reduction = before[i]["hp_bar_reduction"].f();
        enemies->add_child(g.c.node);
        update_bounds(g.c);
        g.c.data = before[i];
        g.c.powers = before[i]["powers"];
        refresh_state(g.c, false);
        g.c.node->set_position((*places_)[id]);
        g.who = format("e%d", id);
        ghosts_.push_back(g);
    }
    if (aftermath_ || final_) {
        Node* ui = find_named(root, "CombatUi");
        if (ui && ui->is_canvas_item()) ((CanvasItem*)ui)->visible = false;
    } else {
        setup_combat_ui(root, d);
        build_hand(find_named(root, "CombatUi"), d);
    }
    pets_x_ = 0;
    /* BigMushroom.AfterRoomEntered: Grow (the player's ScaleTo(1.5, 0)) */
    if (player_.node) {
        const Json& relics = d["player"]["relics"];
        for (size_t i = 0; i < relics.size(); ++i)
            if (relics[i]["id"].str() == "BIG_MUSHROOM") {
                scale_base(player_);
                player_.scale_temp = 1.5f;
                scale_visuals(player_, 0);
                break;
            }
    }
    position_players(ally_nodes, scaling, view["fully_center_players"].b(), &osty_, osty_max_hp, &pets_x_);
    position_pets(pets_x_);
    /* Where the enemies stand: as before in this fight; laid out again
     * (all of them) when one has no place yet. */
    bool known = places_ != NULL && !enemy_nodes.empty();
    for (size_t i = 0; known && i < enemy_node_ids.size(); ++i)
        if (places_->find(enemy_node_ids[i]) == places_->end()) known = false;
    if (encounter_slots_) {
        /* PositionCreaturesWithSlots */
        for (size_t i = 0; i < enemy_nodes.size(); ++i) {
            Vec2 at;
            if (!slot_position(enemy_nodes[i].data, &at)) at = Vec2(250.0f + (float)i * 220.0f, 200);
            enemy_nodes[i].node->set_position(at);
            if (places_) (*places_)[enemy_node_ids[i]] = at;
        }
    } else if (known) {
        for (size_t i = 0; i < enemy_nodes.size(); ++i) enemy_nodes[i].node->set_position((*places_)[enemy_node_ids[i]]);
    } else {
        std::vector<Vec2> at;
        position_enemies(enemy_nodes, scaling, &at);
        for (size_t i = 0; places_ && i < at.size() && i < enemy_node_ids.size(); ++i)
            (*places_)[enemy_node_ids[i]] = at[i];
    }
    play_idle(root);
    /* NCeremonialBeastBgVfx._Ready: its parent hidden */
    {
        std::vector<Node*> stack(1, root);
        while (!stack.empty()) {
            Node* n = stack.back();
            stack.pop_back();
            if (n->script == "res://src/Core/Nodes/Vfx/Backgrounds/NCeremonialBeastBgVfx.cs" && n->parent &&
                n->parent->type == "SpineSprite") {
                beast_bg_.push_back((SpineSprite*)n->parent);
                set_visible(n->parent, false);
            }
            /* NQueenRepyBgVfx.OnCombatSetUp: Repy hidden once the run
             * freed him */
            if (n->script == "res://src/Core/Nodes/Vfx/Backgrounds/NQueenRepyBgVfx.cs")
                set_visible(n, !view["freed_repy"].b());
            for (size_t k = 0; k < n->children.size(); ++k) stack.push_back(n->children[k]);
        }
    }
    for (size_t i = 0; i < enemies_.size(); ++i) {
        setup_creature_spine(enemies_[i].visuals, enemies_[i].data);
        hold_card(enemies_[i], enemies_[i].data["held_card"]);
    }
    for (size_t i = 0; i < ghosts_.size(); ++i) setup_creature_spine(ghosts_[i].c.visuals, ghosts_[i].c.data);
    if (player_.node && !aftermath_) setup_creature_spine(player_.visuals, player_.data);
    if (visual_only_) {
        /* NCreature.ToggleIsInteractable(false) hides the state display. */
        hide(player_.node ? player_.node->find_unique("HealthBar") : NULL);
        hide(osty_.node ? osty_.node->find_unique("HealthBar") : NULL);
        for (size_t i = 0; i < enemies_.size(); ++i) {
            Node* n = enemies_[i].node;
            hide(n ? n->find_unique("HealthBar") : NULL);
            hide(n ? n->find_unique("Intents") : NULL);
        }
    }
    arrow_ = make_arrow();
    root->add_child(arrow_);
    Node* driver = SceneLibrary::create_node("Node");
    driver->name = "CombatDriver";
    Control* scene_box = ctl(root, "CombatSceneContainer");
    ScreenShake::get().set_target(scene_box);
    driver->behavior = new CombatDriver(this, scene_box);
    root->add_child(driver);
    if (!aftermath_ && !final_) refresh_tips();
    return root;
}

Json CombatScreen::before_steps(const Json& d) {
    Json out = d;
    const Json& steps = d["steps"];
    static const char* piles[] = { "draw", "discard", "exhaust" };
    static const char* counts[] = { "draw_pile_count", "discard_pile_count", "exhaust_pile_count" };
    int n[3];
    for (int p = 0; p < 3; ++p) n[p] = d[counts[p]].i();
    std::vector<int> brought;
    std::vector<std::string> seen;
    Json player = d["player"];
    Json enemies = d["enemies"];
    for (size_t i = 0; i < steps.size(); ++i)
        for (size_t k = 0; k < steps[i].size(); ++k) {
            const Json& ch = steps[i][k];
            const std::string& t = ch["t"].str();
            if (t == "card") {
                if (ch["to"].str() == "hand" && ch["from"].str() != "hand") brought.push_back(ch["card"].i(-1));
                for (int p = 0; p < 3; ++p) {
                    if (ch["from"].str() == piles[p]) ++n[p];
                    if (ch["to"].str() == piles[p]) --n[p];
                }
                continue;
            }
            bool counter = t == "energy" || t == "max_energy" || t == "stars" || t == "round";
            bool creature = t == "hp" || t == "max_hp" || t == "block";
            if (!counter && !creature) continue;
            std::string key = creature ? ch["who"].str() + "." + t : t;
            if (std::find(seen.begin(), seen.end(), key) != seen.end()) continue;
            seen.push_back(key);
            if (counter) {
                out.set(t, ch["from"]);
            } else if (ch["who"].str() == "player") {
                player.set(t, ch["from"]);
            } else {
                Json next = Json::make_array();
                for (size_t e = 0; e < enemies.size(); ++e) {
                    Json en = enemies[e];
                    if (format("e%d", en["combat_id"].i(-1)) == ch["who"].str()) en.set(t, ch["from"]);
                    next.append(en);
                }
                enemies = next;
            }
        }
    out.set("player", player);
    out.set("enemies", enemies);
    for (int p = 0; p < 3; ++p) out.set(counts[p], Json::make_number(n[p] < 0 ? 0 : n[p]));
    Json hand = Json::make_array();
    const Json& cards = d["hand"];
    for (size_t i = 0; i < cards.size(); ++i)
        if (std::find(brought.begin(), brought.end(), cards[i]["instance_id"].i(-2)) == brought.end()) hand.append(cards[i]);
    out.set("hand", hand);
    return out;
}

/* NHealthBar delays the orange middleground for one second after damage,
 * then eases it to the new foreground over one second. */
void CombatScreen::refresh_state(Creature& c, bool animate) {
    if (!c.node) return;
    Control* hb = ctl(ctl(c.node, "HealthBar"), "HealthBar");
    Control* mid = ctl(hb, "HpMiddleground");
    float old = mid ? mid->offset[SIDE_RIGHT] : 0;
    bool changed = c.shown_hp != c.data["hp"].i() || c.shown_max_hp != c.data["max_hp"].i();
    refresh_health_bar(c, c.data, c.powers);
    /* NCreatureStateDisplay.OnBlockTrackingCreatureBlockChanged */
    for (std::map<std::string, std::string>::const_iterator it = block_tracking_.begin(); it != block_tracking_.end(); ++it)
        if (creature_for(it->second) == &c) {
            Creature* t = creature_for(it->first);
            if (t && t != &c && t->node) refresh_health_bar(*t, t->data, t->powers);
        }
    if (animate && mid && c.shown_hp >= 0 && c.data["hp"].i() > 0) {
        float target = mid->offset[SIDE_RIGHT];
        mid->offset[SIDE_RIGHT] = old;
        if (changed) tweens_.tween(&mid->offset[SIDE_RIGHT], target, 1, TRANS_EXPO, EASE_OUT, target < old ? 1.0f : 0.0f);
    }
    c.shown_hp = c.data["hp"].i();
    c.shown_max_hp = c.data["max_hp"].i();
}

/* NEnergyCounter.RefreshLabel (each combat state change): the count in
 * cream with the character's outline, red at 0; the orb's layers in
 * energy_orb_dark and DarkGray at 0; its rotation 30 degrees a second per
 * layer (0: 5). OnEnergyChanged: a rise restarts EnergyVfxBack / Front. */
void CombatScreen::refresh_energy() {
    Node* ui = root_ ? find_named(root_, "CombatUi") : NULL;
    Node* box = ui ? ui->get_node("EnergyCounterContainer") : NULL;
    if (!box || box->children.empty()) return;
    Node* orb = box->children[0];
    int energy = state_["energy"].i();
    Node* label = orb->get_node("Label");
    if (label && label->is_control()) {
        Control* l = (Control*)label;
        l->add_theme_color_override("font_color", energy != 0 ? Color(1, 0.964706f, 0.886275f) : hex(0xFF5555));
        l->add_theme_color_override("font_outline_color", energy != 0 ? energy_outline_ : hex(0x501717));
        set_label(label, format("%d/%d", energy, state_["max_energy"].i()));
    }
    Resource* r = energy == 0 ? lib_->ext("materials/ui/energy_orb_dark.tres") : NULL;
    MaterialRes* dark = r && r->kind == Resource::RES_SHADER_MATERIAL ? (MaterialRes*)r : NULL;
    Node* layers = orb->find_unique("Layers");
    Node* spin = orb->find_unique("RotationLayers");
    Node* groups[2] = { layers, spin };
    for (int g = 0; g < 2; ++g)
        for (size_t i = 0; groups[g] && i < groups[g]->children.size(); ++i)
            if (groups[g]->children[i]->is_control()) ((Control*)groups[g]->children[i])->material = dark;
    if (layers && layers->is_control())
        ((Control*)layers)->modulate = energy == 0 ? Color(0.663f, 0.663f, 0.663f) : Color(1, 1, 1, 1); /* Colors.DarkGray */
    if (energy_spin_ && orb->behavior == energy_spin_) ((SpinLayers*)energy_spin_)->set_speed(energy == 0 ? 5.0f : 30.0f);
    if (energy > shown_energy_) {
        vfx_restart(orb->find_unique("EnergyVfxBack"));
        vfx_restart(orb->find_unique("EnergyVfxFront"));
    }
    shown_energy_ = energy;
}

void CombatScreen::refresh_counters() {
    Node* ui = find_named(root_, "CombatUi");
    refresh_energy();
    Node* star = ui ? ui->get_node("StarCounter") : NULL;
    if (star) {
        /* RefreshVisibility: once shown it stays */
        if (state_["always_show_stars"].b() || state_["stars"].i() > 0) set_visible(star, true);
        if (star->behavior) ((StarCounterAnim*)star->behavior)->set_stars(state_["stars"].i());
        else set_label(star->find_unique("CountLabel"), format("[center]%d[/center]", state_["stars"].i()));
    }
    Node* piles = find_named(ui, "CombatPileContainer");
    static const char* names[] = { "DrawPile", "DiscardPile", "ExhaustPile" };
    static const char* fields[] = { "draw_pile_count", "discard_pile_count", "exhaust_pile_count" };
    for (int i = 0; piles && i < 3; ++i) {
        Node* pile = piles->get_node(names[i]);
        if (!pile) continue;
        int shown = state_[fields[i]].i() + depart_[i] - arriving(i);
        set_counter_label(pile->get_node("CountContainer/Count"), shown < 0 ? 0 : shown);
        if (i == 2) set_visible(pile, state_[fields[i]].i() > 0);
    }
    Node* end = ui ? ui->get_node("EndTurnButton") : NULL;
    if (end) set_label(end->get_node("Visuals/Label"), Loc::format("gameplay_ui", "END_TURN_BUTTON", "turnNumber",
                        format("%d", state_["turn_number"].i(state_["round"].i(1)))));
    refresh_end_glow();
    update_beast_bg();
    Node* top = root_ ? root_->get_node("TopBarLayer") : NULL;
    if (top) {
        set_label(find_named(top, "HpLabel"), format("%d/%d", player_.data["hp"].i(), player_.data["max_hp"].i()));
        set_top_bar_gold(top, state_["player"]["gold"].i());
    }
}

/* Keep the background, Spine tracks, particles and surviving holders alive.
 * Only new combatants are instanced; absent enemies remain through death. */
void CombatScreen::update_decision(const Json& d, bool finishing) {
    unfocus_creatures(); /* the creatures may change */
    if (visual_only_ && !finishing) {
        /* NCombatRoom.TransitionToActiveCombat keeps all creature nodes. */
        visual_only_ = false;
        setup_combat_ui(root_, d);
        build_hand(find_named(root_, "CombatUi"), d);
        set_visible(player_.node ? player_.node->find_unique("HealthBar") : NULL, true);
        for (size_t i = 0; i < enemies_.size(); ++i) {
            Node* n = enemies_[i].node;
            set_visible(n ? n->find_unique("HealthBar") : NULL, d["enemies"][i]["health_bar_visible"].b(true));
            set_visible(n ? n->find_unique("Intents") : NULL, true);
        }
    }
    if (!finishing) {
        aftermath_ = final_ = false;
        set_visible(find_named(root_, "CombatUi"), true);
    }
    pending_ = d;
    previous_ = state_;
    end_play();
    if (tips_) { root_->remove_child(tips_); delete tips_; tips_ = NULL; }
    tips_for_ = -2;
    if (!finishing) {
        std::vector<Creature> next;
        std::vector<std::string> who;
        std::vector<bool> kept(enemies_.size(), false);
        const Json& es = d["enemies"];
        Node* container = root_->find_unique("EnemyContainer");
        for (size_t i = 0; i < es.size(); ++i) {
            std::string id = format("e%d", es[i]["combat_id"].i(-1));
            Creature c;
            c.node = NULL;
            bool found = false;
            for (size_t j = 0; j < enemy_who_.size(); ++j) {
                if (enemy_who_[j] == id) { c = enemies_[j]; kept[j] = true; found = true; break; }
            }
            for (size_t j = 0; !found && j < ghosts_.size(); ++j) {
                if (ghosts_[j].who != id) continue;
                c = ghosts_[j].c;
                ghosts_.erase(ghosts_.begin() + (long)j);
                found = true;
                break;
            }
            if (!found && container && make_creature(es[i]["id"].str(), false, c)) {
                c.node->name = format("CombatEnemy_%d", es[i]["combat_id"].i(-1));
                container->add_child(c.node);
                c.hp_bar_reduction = es[i]["hp_bar_reduction"].f();
                c.data = es[i];
                c.powers = es[i]["powers"];
                update_bounds(c);
                refresh_state(c, false);
                set_nameplate_text(c.nameplate, es[i]["name"].str());
                Vec2 slot;
                if (slot_position(es[i], &slot)) c.node->set_position(slot);
                else if (places_ && places_->find(es[i]["combat_id"].i()) != places_->end()) c.node->set_position((*places_)[es[i]["combat_id"].i()]);
                else c.node->set_position(Vec2(250.0f + (float)i * 220.0f, 200));
                play_idle(c.node);
                setup_creature_spine(c.visuals, c.data);
                c.node->visible = false; /* revealed by spawn or the final sync */
            }
            next.push_back(c);
            who.push_back(id);
        }
        for (size_t j = 0; j < enemies_.size(); ++j) {
            if (kept[j] || !enemies_[j].node) continue;
            Ghost g;
            g.c = enemies_[j];
            g.who = enemy_who_[j];
            ghosts_.push_back(g);
        }
        /* A creature summoned and killed within these steps (not among the
         * decision's living): from its spawn's view, hidden until the spawn
         * shows it, gone with its death as the others. */
        const Json& steps = d["steps"];
        for (size_t s = 0; container && s < steps.size(); ++s)
            for (size_t k = 0; k < steps[s].size(); ++k) {
                const Json& ch = steps[s][k];
                if (ch["t"].str() != "spawn" || !ch["view"].is_object()) continue;
                const std::string& id = ch["who"].str();
                if (std::find(who.begin(), who.end(), id) != who.end() || creature_for(id)) continue;
                const Json& v = ch["view"];
                Ghost g;
                if (!make_creature(v["id"].str(), false, g.c)) continue;
                g.c.node->name = format("CombatEnemy_%d", v["combat_id"].i(-1));
                container->add_child(g.c.node);
                g.c.hp_bar_reduction = v["hp_bar_reduction"].f();
                g.c.data = v;
                g.c.powers = v["powers"];
                update_bounds(g.c);
                refresh_state(g.c, false);
                set_nameplate_text(g.c.nameplate, v["name"].str());
                Vec2 slot;
                if (slot_position(v, &slot)) g.c.node->set_position(slot);
                else if (places_ && places_->find(v["combat_id"].i()) != places_->end()) g.c.node->set_position((*places_)[v["combat_id"].i()]);
                else g.c.node->set_position(Vec2(250.0f + (float)(next.size() + ghosts_.size()) * 220.0f, 200));
                play_idle(g.c.node);
                setup_creature_spine(g.c.visuals, g.c.data);
                g.c.node->visible = false;
                g.who = id;
                ghosts_.push_back(g);
            }
        enemies_ = next;
        enemy_who_ = who;
        /* A pet that joined during the fight (Byrdpip hatched mid-combat,
         * Pael's Legion): beside the player, revealed by its spawn. */
        const Json& ps = d["pets"];
        bool added = false;
        for (size_t i = 0; i < ps.size(); ++i) {
            std::string id = format("e%d", ps[i]["combat_id"].i(-1));
            if (std::find(pet_who_.begin(), pet_who_.end(), id) != pet_who_.end() || !ps[i]["alive"].b(true)) continue;
            Node* allies = root_->find_unique("AllyContainer");
            Creature pc;
            if (!allies || !make_pet(ps[i], pc)) continue;
            allies->add_child(pc.node);
            update_bounds(pc);
            play_idle(pc.node);
            pets_.push_back(pc);
            pet_who_.push_back(id);
            added = true;
        }
        if (added) position_pets(pets_x_);
        if (!osty_.node && d["osty"]["alive"].b()) {
            Node* allies = root_->find_unique("AllyContainer");
            if (allies && make_creature("osty", false, osty_)) {
                allies->add_child(osty_.node);
                osty_.data = d["osty"];
                update_bounds(osty_);
                refresh_state(osty_, false);
                play_idle(osty_.node);
                osty_.node->visible = false;
            }
        }
    }
    play_steps(d["steps"], d, finishing);
    if (!finishing) refresh_tips();
}

void CombatScreen::show_aftermath(const Json& player) {
    aftermath_ = true;
    hide(find_named(root_, "CombatUi"));
    if (player.is_object()) {
        player_.data = player;
        player_.powers = Json();
        refresh_state(player_, true);
        make_powers(player_, Json());
    }
    for (size_t i = 0; i < enemies_.size(); ++i)
        if (i >= enemy_who_.size() || !is_dying(enemy_who_[i])) hide(enemies_[i].node);
    for (size_t i = 0; i < ghosts_.size(); ++i)
        if (!is_dying(ghosts_[i].who)) hide(ghosts_[i].c.node);
}

void CombatScreen::move_card(const Json& change) {
    const std::string& from = change["from"].str();
    const std::string& to = change["to"].str();
    static const char* piles[] = { "draw", "discard", "exhaust" };
    static const char* fields[] = { "draw_pile_count", "discard_pile_count", "exhaust_pile_count" };
    for (int p = 0; p < 3; ++p) {
        int n = state_[fields[p]].i();
        if (from == piles[p]) --n;
        if (to == piles[p]) ++n;
        state_.set(fields[p], Json::make_number(n < 0 ? 0 : n));
    }
    int id = change["card"].i(-1);
    /* between piles with no node: counted as its silhouette goes */
    int pf = from == "draw" ? 0 : from == "discard" ? 1 : from == "exhaust" ? 2 : -1;
    int pt = to == "draw" ? 0 : to == "discard" ? 1 : -1;
    if (pf >= 0 && pt >= 0) {
        ++depart_[pf];
        ++expect_[pt];
    }
    /* a card in the play pile that its play sends on (CardPileCmd.Add's
     * tween: NCardFlyVfx to a pile, NCardExhaustVfx where it is) */
    Vec2 back_from(0, -500);
    bool from_play = false;
    if (from == "none") {
        for (size_t i = 0; i < moving_cards_.size(); ++i) {
            MovingCard& m = moving_cards_[i];
            if (m.kind != 3 || m.holder.id != id) continue;
            m.holder.node->modulate = Color(1, 1, 1, 1);
            if (to == "discard" || to == "draw") {
                played_instance_ = id;
                start_flight(m, 1, to);
            } else if (to == "exhaust") {
                spawn_card_exhaust(lib_, exhaust_ui(), m.holder.node, false);
                moving_cards_.erase(moving_cards_.begin() + (long)i);
            } else if (to == "hand") {
                back_from = m.holder.pos;
                from_play = true;
                if (m.holder.node->parent) m.holder.node->parent->remove_child(m.holder.node);
                delete m.holder.node;
                moving_cards_.erase(moving_cards_.begin() + (long)i);
            }
            break;
        }
    }
    if (from == "hand" && to != "hand") {
        std::map<int, Vec2>::iterator played = played_.find(id);
        bool was_played = played != played_.end();
        if (was_played) {
            played_.erase(played);
            played_instance_ = id;
            std::vector<int>::iterator q = std::find(queue_.begin(), queue_.end(), id);
            if (q != queue_.end()) queue_.erase(q);
        }
        std::vector<int>::iterator burn = std::find(burn_ids_.begin(), burn_ids_.end(), id);
        bool burnt = burn != burn_ids_.end();
        if (burnt) burn_ids_.erase(burn);
        for (size_t i = 0; i < holders_.size(); ++i) {
            if (holders_[i].id != id) continue;
            if ((to == "exhaust" && !was_played) || burnt) {
                /* CardPileCmd: from the hand, NCardExhaustQuickVfx where it
                 * is; RemoveFromCombat, NCardExhaustVfx */
                spawn_card_exhaust(lib_, exhaust_ui(), holders_[i].node, !burnt);
                holders_.erase(holders_.begin() + (long)i);
                break;
            }
            MovingCard m;
            m.holder = holders_[i];
            m.from = m.holder.pos;
            m.from_scale = m.holder.scale;
            m.from_angle = m.holder.angle;
            m.t = 0;
            m.mp_t = -1;
            /* played and exhausted: NCardExhaustVfx where it ends */
            m.exhaust = to == "exhaust";
            m.kind = 0;
            m.trail = m.path_scene = NULL;
            if (to == "none") {
                /* CardPileCmd.AddDuringManualCardPlay (or Add to the play
                 * pile): into NCombatUi.PlayContainer, at the pile's place in
                 * the lerp time (0.25 s, Fast 0.1), upright, to 0.8 in 0.25 s */
                m.kind = 3;
                m.to = play_pile_pos();
                m.to_scale = 0.8f;
                m.duration = fast_mode() ? 0.1 : 0.25;
                m.holder.node->z_index = 2;
                if (!was_played) play_cards_[id] = m.holder.data;
            } else {
                m.to = was_played ? play_pile_pos() : Vec2(850, -70.0f);
                m.to_scale = was_played ? 0.8f : 0.15f;
                m.duration = was_played ? (fast_mode() ? 0.1 : 0.25) : 0.4;
                if (to == "discard" || to == "draw") start_flight(m, 1, to);
            }
            moving_cards_.push_back(m);
            holders_.erase(holders_.begin() + (long)i);
            break;
        }
        track_play();
        Json hand = Json::make_array();
        for (size_t i = 0; i < hand_.size(); ++i)
            if (hand_[i]["instance_id"].i(-1) != id) hand.append(hand_[i]);
        hand_ = hand;
        layout_hand();
        /* its tips go with it (tips_for_ is a place in the hand: the focus
         * there now is another card, or none) */
        tips_for_ = -2;
        refresh_tips();
    }
    if (to == "hand" && from != "hand") {
        /* the decision's hand; a card that leaves it again within these
         * steps (Hellraiser plays it as it is drawn) comes with its view */
        Json passing = Json::make_array();
        if (change["view"].is_object()) passing.append(change["view"]);
        const Json& cards = change["view"].is_object() ? passing : pending_["hand"];
        for (size_t i = 0; hand_box_ && i < cards.size(); ++i) {
            if (cards[i]["instance_id"].i(-2) != id) continue;
            bool exists = false;
            for (size_t j = 0; j < holders_.size(); ++j) if (holders_[j].id == id) exists = true;
            if (exists) break;
            Holder h = make_holder(cards[i]);
            if (!h.node) break;
            h.pos = from_play ? back_from : Vec2(from == "draw" ? -850.0f : 0.0f, from == "draw" ? -70.0f : -500.0f);
            if (!from_play && (from == "discard" || from == "exhaust")) {
                /* CreateCardNodeAndUpdateVisuals at the old pile */
                Vec2 at = pile_center(from);
                if (at.x >= 0) h.pos = inverse(hand_box_->global_transform()).xform(at);
            }
            h.scale = from_play ? 0.8f : 0.15f;
            h.angle = from_play ? 0.0f : -25.0f;
            hand_box_->add_child(h.node);
            holders_.push_back(h);
            hand_.append(cards[i]);
            layout_hand();
            refresh_tips();
            break;
        }
    }
    refresh_counters();
}

bool CombatScreen::apply_state(const Json& ch) {
    const std::string& type = ch["t"].str();
    Creature* c = creature_for(ch["who"].str());
    if (type == "card") { move_card(ch); return true; }
    if (type == "power_card") {
        /* PlayPowerCardFlyVfx for a replay: a new card in the play pile,
         * growing in over 0.1 s */
        std::map<int, Json>::iterator d = play_cards_.find(ch["card"].i(-1));
        if (d == play_cards_.end() || in_play(d->first) || !hand_box_) return true;
        Holder h = make_holder(d->second);
        if (!h.node) return true;
        hand_box_->add_child(h.node);
        MovingCard m;
        m.holder = h;
        m.from = m.to = m.holder.pos = play_pile_pos();
        m.from_scale = 0;
        m.to_scale = 1;
        m.from_angle = 0;
        m.t = 0;
        m.duration = 0;
        m.mp_t = -1;
        m.exhaust = false;
        m.kind = 3;
        m.trail = m.path_scene = NULL;
        h.node->z_index = 2;
        moving_cards_.push_back(m);
        return true;
    }
    if (type == "power_fly") {
        /* CardModel.PlayPowerCardFlyVfx: NCardFlyPowerVfx from the play pile */
        MovingCard* m = in_play(ch["card"].i(-1));
        if (m) {
            m->holder.node->modulate = Color(1, 1, 1, 1);
            start_flight(*m, 2, "");
        }
        return true;
    }
    if (type == "multi_play") {
        MovingCard* m = in_play(ch["card"].i(-1));
        if (m) m->mp_t = 0;
        return true;
    }
    if (type == "card_burn") {
        /* CardPileCmd.RemoveFromCombat: NCardExhaustVfx where the card is */
        int id = ch["card"].i(-1);
        for (size_t i = 0; i < moving_cards_.size(); ++i)
            if (moving_cards_[i].kind == 3 && moving_cards_[i].holder.id == id) {
                moving_cards_[i].holder.node->modulate = Color(1, 1, 1, 1);
                spawn_card_exhaust(lib_, exhaust_ui(), moving_cards_[i].holder.node, false);
                moving_cards_.erase(moving_cards_.begin() + (long)i);
                return true;
            }
        burn_ids_.push_back(id);
        return true;
    }
    if (type == "shuffle_fly") { spawn_shuffle_fly(); return true; }
    if (type == "card_fly") { spawn_shuffle_fly(ch["from"].str(), ch["to"].str()); return true; }
    if (type == "card_flash") {
        /* NHandCardHolder.Flash: its Flash in the glow's colour (playable
         * cyan, gold, red), to 0.6 in 0.15 s, out in 0.3 s */
        for (size_t i = 0; i < holders_.size(); ++i) {
            if (holders_[i].id != ch["card"].i(-1)) continue;
            Node* f = holders_[i].node->get_node("Flash");
            if (!f || !f->is_canvas_item()) break;
            CanvasItem* fl = (CanvasItem*)f;
            const std::string& glow = holders_[i].data["glow"].str();
            fl->modulate = glow == "gold" ? Color(1, 0.784f, 0, 0) : glow == "red" ? Color(0.83f, 0, 0.33f, 0) : Color(0, 0.957f, 0.988f, 0);
            if (f->is_node2d()) ((Node2D*)f)->scale = Vec2(1, 1);
            else if (f->is_control()) ((Control*)f)->scale = Vec2(1, 1);
            tweens_.kill(&fl->modulate.a);
            tweens_.tween(&fl->modulate.a, 0.6f, 0.15f);
            tweens_.tween(&fl->modulate.a, 0.0f, 0.3f, TRANS_LINEAR, EASE_IN_OUT, 0.15f);
            break;
        }
        return true;
    }
    if (type == "card_randomize_cost") {
        CostRoll r;
        r.id = ch["card"].i(-1);
        r.t = 0;
        r.duration = 0.4f + 0.2f * (float)(chaotic_rand() % 1001) / 1000.0f;
        r.offset = 10.0f * (float)(chaotic_rand() % 1001) / 1000.0f;
        for (size_t i = 0; i < cost_rolls_.size(); ++i)
            if (cost_rolls_[i].id == r.id) cost_rolls_.erase(cost_rolls_.begin() + (long)i);
        cost_rolls_.push_back(r);
        return true;
    }
    if (type == "card_create") { create_at_pile(ch); return true; }
    if (type == "held_card") {
        if (c) hold_card(*c, ch["card"]);
        return true;
    }
    if (type == "escape") {
        /* CreatureCmd.Escape: RemoveCreatureNode, ToggleIsInteractable(false),
         * Visible = false */
        if (c && c->node) c->node->visible = false;
        if (lib_->audio()) lib_->audio()->stop_room_loops(ch["who"].str());
        return true;
    }
    if (type == "node_visible") {
        /* Creature.SetNodeVisible */
        if (ch["visible"].b()) node_hidden_.erase(ch["who"].str());
        else node_hidden_.insert(ch["who"].str());
        if (c && c->node) c->node->visible = ch["visible"].b();
        return true;
    }
    if (type == "relic_flash") { flash_relic(ch); return true; }
    if (type == "forge_preview") {
        /* ForgeCmd.PreviewSovereignBlade: NCardSmithVfx(card) over each
         * blade in the hand (its sparks at 0, 0.25 and 0.5 s, gone at
         * 0.9 s); the others in one preview, then to their piles; silent */
        const Json& hand = ch["hand"];
        for (size_t i = 0; i < hand.size(); ++i)
            for (size_t h = 0; h < holders_.size(); ++h) {
                if (holders_[h].id != hand[i].i(-1) || !holders_[h].card_node || !holders_[h].card_node->is_canvas_item()) continue;
                Node* fx = instance(lib_, "scenes/vfx/vfx_card_smith.tscn");
                if (!fx || !fx->is_canvas_item()) {
                    delete fx;
                    break;
                }
                Xform2D g = ((CanvasItem*)holders_[h].card_node)->global_transform();
                float s = sqrtf(g.x.x * g.x.x + g.x.y * g.x.y);
                hide(fx->find_unique("CardContainer"));
                Node* ui = exhaust_ui();
                if (!ui) {
                    delete fx;
                    break;
                }
                ui->add_child(fx);
                if (fx->is_control()) {
                    ((Control*)fx)->set_position(inverse(((CanvasItem*)ui)->global_transform()).xform(g.origin));
                    ((Control*)fx)->scale = Vec2(s, s);
                } else if (fx->is_node2d()) {
                    ((Node2D*)fx)->position = inverse(((CanvasItem*)ui)->global_transform()).xform(g.origin);
                    ((Node2D*)fx)->scale = Vec2(s, s);
                }
                fx->behavior = new SmithSparks();
                break;
            }
        const Json& others = ch["others"];
        for (size_t i = 0; i < others.size() && DeckPreviews::current(); ++i) {
            Json cue;
            cue.set("op", Json::make_string("smith"));
            Json cards = Json::make_array();
            cards.append(others[i]["card"]);
            cue.set("cards", cards);
            cue.set("pile", others[i]["pile"]);
            cue.set("silent", Json::make_bool(true));
            DeckPreviews::current()->play_cue(cue, player_.data["character"].str());
        }
        return true;
    }
    if (type == "foul_potion" && ch["where"].str() == "fake_merchant" && !enemy_who_.empty()) {
        /* NFakeMerchant.FoulPotionThrown: its foulPotion line (the fight
         * comes with it), from the merchant it turned into */
        std::vector<std::string> lines;
        for (int n = 1; n < 32; ++n) {
            std::string key = format("FAKE_MERCHANT.talk.foulPotion.line%d", n);
            std::string text = Loc::get("events", key.c_str());
            if (text == "events." + key) break;
            lines.push_back(text);
        }
        if (lines.empty() || !vfx_box_) return false;
        Json args = Json::make_array();
        args.append(Json::make_string(lines[chaotic_rand() % lines.size()]));
        args.append(Json::make_string(enemy_who_[0]));
        args.append(Json::make_number(2.0));
        args.append(Json::make_string("Blue"));
        return spawn_combat_vfx(lib_, vfx_box_, "NSpeechBubbleVfx.Create", args, *this);
    }
    if (type == "gold") {
        /* NTopBarGold.UpdateGold on Player.GoldChanged (a thief's steal):
         * the count runs to it when it happens */
        Node* top = root_ ? root_->get_node("TopBarLayer") : NULL;
        if (!top) return false;
        set_top_bar_gold(top, ch["to"].i());
        Json p = state_["player"];
        p.set("gold", ch["to"]);
        state_.set("player", p);
        return true;
    }
    if (type == "blade") {
        VfxCreature t;
        bool aimed = ch["target"].is_string() && creature(ch["target"].str(), &t);
        sovereign_blade_cue(lib_, player_.node, ch, aimed ? &t.center : NULL);
        return true;
    }
    if (type == "power_flash") {
        /* NCreature.OnPowerFlashed (the big icon), NPower.OnPowerFlashed */
        flash_power(ch);
        power_row_flash(ch["who"].str(), ch["power"].str());
        return true;
    }
    if (type == "power_pulse") {
        /* NPower.OnPulsingStarted / Stopped: the icon's pulse parameter */
        std::string key = ch["who"].str() + " " + ch["power"].str();
        if (ch["on"].b()) pulsing_.insert(key);
        else pulsing_.erase(key);
        if (c) make_powers(*c, c->powers);
        return true;
    }
    if (type == "power_vfx") {
        /* NCreature.OnPowerIncreased: NPowerAppliedVfx and the burst
         * (ShouldPlayVfx), the buff / debuff sound, AnimShake on a debuff */
        VfxCreature at;
        bool buff = ch["buff"].b();
        if (ch["vfx"].b(true) && vfx_box_ && creature(ch["who"].str(), &at)) {
            std::string icon = lib_->ext(ch["icon"].str()) ? ch["icon"].str() : ch["beta_icon"].str();
            spawn_power_applied(lib_, vfx_box_, at.center, ch["title"].str(), icon, buff);
        }
        if (ch["sound"].b(false) && lib_->audio()) lib_->audio()->ui(buff ? "event:/sfx/buff" : "event:/sfx/debuff");
        if (!buff) anim_shake(ch["who"].str());
        return true;
    }
    if (type == "ui_event") {
        ui_event(ch["ev"].str());
        return true;
    }
    if (type == "power_removed") {
        /* NCreature.OnPowerRemoved: NPowerRemovedVfx at GetTopOfHitbox */
        VfxCreature at;
        if (vfx_box_ && creature(ch["who"].str(), &at)) {
            std::string icon = lib_->ext(ch["icon"].str()) ? ch["icon"].str() : ch["beta_icon"].str();
            spawn_power_removed(lib_, vfx_box_, at.top, ch["title"].str(), icon);
        }
        return true;
    }
    if (type == "deck_card") {
        /* CardCmd.PreviewCardPileAdd in the fight (a status card an enemy
         * gives): shown in NCombatRoom.Ui's preview row, then to its pile */
        if (DeckPreviews::current()) DeckPreviews::current()->play_cue(ch, player_.data["character"].str());
        return true;
    }
    if (type == "orb_activate") { activate_orb(ch); return true; }
    if (type == "orbs" && ch["orbs"].is_array()) {
        sync_orbs(ch["orbs"], ch["slots"].i(), true);
        state_.set("orbs", ch["orbs"]);
        state_.set("orb_slots", ch["slots"]);
        return true;
    }
    if (type == "stars" && ch["to"].i() > ch["from"].i()) {
        /* NStarCounter.UpdateStarCount: star_gain_vfx behind the counter */
        Node* ui = find_named(root_, "CombatUi");
        Control* star = ui ? child_ctl(ui, "StarCounter") : NULL;
        Node* fx = star ? instance(lib_, "scenes/vfx/star_gain_vfx.tscn") : NULL;
        if (fx && fx->is_node2d()) {
            /* NVfxParticleSystem emits and frees it */
            ((Node2D*)fx)->position = star->size * 0.5f;
            if (star->children.empty()) star->add_child(fx);
            else star->add_child_before(fx, star->children[0]);
        } else {
            delete fx;
        }
    }
    if (type == "energy" || type == "max_energy" || type == "stars" || type == "round") {
        state_.set(type, ch["to"]);
        if (type == "round") state_.set("turn_number", ch["to"]);
        refresh_counters();
        return true;
    }
    if (!c) return false;
    if (type == "hp" || type == "max_hp" || type == "block") {
        c->data.set(type, ch["to"]);
        refresh_state(*c, true);
        refresh_counters();
        if (type == "block" && ch["from"].i() == 0 && ch["to"].i() > 0) {
            /* NHealthBar.AnimateInBlock: the block in from 20 px higher
             * (Back Out) as it fades in (Sine Out), 0.5 s */
            Control* hb = ctl(c->node, "HealthBar");
            Control* block = hb ? ctl(hb, "BlockContainer") : NULL;
            if (block) {
                float top = block->offset[SIDE_TOP], bottom = block->offset[SIDE_BOTTOM];
                tweens_.kill(&block->modulate.a);
                tweens_.kill(&block->offset[SIDE_TOP]);
                tweens_.kill(&block->offset[SIDE_BOTTOM]);
                tweens_.tween_from(&block->modulate.a, 0.0f, 1.0f, 0.5f, TRANS_SINE, EASE_OUT);
                tweens_.tween_from(&block->offset[SIDE_TOP], top - 20.0f, top, 0.5f, TRANS_BACK, EASE_OUT);
                tweens_.tween_from(&block->offset[SIDE_BOTTOM], bottom - 20.0f, bottom, 0.5f, TRANS_BACK, EASE_OUT);
            }
        }
    } else if (type == "power") {
        Json next = Json::make_array();
        bool exists = false;
        for (size_t i = 0; i < c->powers.size(); ++i) {
            Json p = c->powers[i];
            if (p["id"].str() == ch["power"].str()) {
                exists = true;
                if (ch["removed"].b()) continue;
                p.set("amount", ch["to"]);
                p.set("display_amount", ch["to"]);
            }
            next.append(p);
        }
        if (!exists && !ch["removed"].b()) {
            /* its look: the decision's (the cue's own for a power gone
             * again within these steps) */
            const Json* metadata = ch["view"].is_object() ? &ch["view"] : NULL;
            const Json& es = pending_["enemies"];
            const Json& powers = ch["who"].str() == "player" ? pending_["player_powers"] : c->powers;
            if (!metadata) metadata = find_power(powers, ch["power"].str().c_str());
            for (size_t i = 0; !metadata && i < es.size(); ++i)
                if (format("e%d", es[i]["combat_id"].i()) == ch["who"].str()) metadata = find_power(es[i]["powers"], ch["power"].str().c_str());
            Json p = metadata ? *metadata : Json();
            p.set("id", ch["power"]);
            p.set("amount", ch["to"]);
            p.set("display_amount", ch["to"]);
            p.set("counter", Json::make_number(1));
            next.append(p);
        }
        c->powers = next;
        make_powers(*c, next);
        refresh_state(*c, true);
        /* NPower.OnDisplayAmountChanged: its flash */
        if (exists && !ch["removed"].b() && ch["to"].i() != ch["from"].i()) power_row_flash(ch["who"].str(), ch["power"].str());
        /* NOrbVfx.OnPowerApplied / Increased / Decreased: the orbs' focused
         * particles follow the player's Focus as it changes */
        if (c == &player_ && ch["power"].str() == "FOCUS_POWER") {
            bool focus = ch["to"].i() > 0 && !ch["removed"].b();
            for (size_t i = 0; i < orbs_.size(); ++i) {
                const Json& m = orbs_[i].data;
                if (m.is_null()) continue;
                bool glass = lower(m["id"].str()) == "glass_orb";
                orb_particles(orb_export(orbs_[i].sprite, "_focusedParticles"), false, focus && (!glass || m["passive"].i() > 0));
            }
        }
    } else if (type == "spawn" || type == "revive") {
        std::vector<std::string>::iterator d = std::find(dying_.begin(), dying_.end(), ch["who"].str());
        if (d != dying_.end()) dying_.erase(d);
        death_end_.erase(ch["who"].str());
        fade_gone_at_.erase(ch["who"].str());
        c->node->visible = true;
        tweens_.kill(&c->node->modulate.a);
        c->node->modulate.a = 1;
        enable_ui(*c);
        play_idle(c->node);
    } else {
        return false;
    }
    return true;
}

void CombatScreen::finish_state(const Json& d, bool finishing) {
    if (d.is_null()) return;
    if (d["player"].is_object()) {
        player_.data = d["player"];
        player_.powers = finishing ? Json() : d["player_powers"];
        refresh_state(player_, true);
        make_powers(player_, player_.powers);
    }
    if (!finishing) {
        const Json& es = d["enemies"];
        for (size_t i = 0; i < enemies_.size() && i < es.size(); ++i) {
            Creature& c = enemies_[i];
            if (!c.node) continue;
            if (c.data["intents"] != es[i]["intents"]) {
                make_intents(c, es[i]["intents"], (int)i);
                Control* row = ctl(c.node, "Intents");
                if (row) { row->modulate.a = 0; tweens_.tween(&row->modulate.a, 1, 0.25f); }
            }
            c.data = es[i];
            c.powers = es[i]["powers"];
            c.node->visible = !(i < enemy_who_.size() && node_hidden_.count(enemy_who_[i]));
            tweens_.kill(&c.node->modulate.a);
            c.node->modulate.a = 1;
            refresh_state(c, true);
            make_powers(c, c.powers);
        }
        if (osty_.node && d["osty"].is_object()) {
            osty_.data = d["osty"];
            refresh_state(osty_, true);
        }
        sync_hand(d["hand"], true);
        if (state_["orbs"] != d["orbs"] || state_["orb_slots"] != d["orb_slots"])
            sync_orbs(d["orbs"], d["orb_slots"].i(), true);
        state_ = d;
        /* a creature focused while the cues played: its tips as they are now */
        Creature* focused = creature_of(creature_focus_);
        if (focused) show_creature_tips(*focused);
    } else {
        sync_hand(Json(), true);
        hide(find_named(root_, "CombatUi"));
        state_.set("player", d["player"]);
    }
    for (int i = 0; i < 3; ++i) depart_[i] = expect_[i] = 0;
    refresh_counters();
    /* the last response's cards stay looked up until its own sync */
    bool later = false;
    for (size_t i = next_cue_; i < cues_.size(); ++i)
        if (cues_[i].kind == CUE_SYNC) later = true;
    if (!later) pending_ = Json();
}

/* ---- step playback ---- */

CombatScreen::Creature* CombatScreen::creature_for(const std::string& who) {
    if (who == "player") return player_.node ? &player_ : NULL;
    if (who == "osty") return osty_.node ? &osty_ : NULL;
    for (size_t i = 0; i < pet_who_.size() && i < pets_.size(); ++i)
        if (pet_who_[i] == who) return pets_[i].node ? &pets_[i] : NULL;
    for (size_t i = 0; i < enemy_who_.size() && i < enemies_.size(); ++i)
        if (enemy_who_[i] == who) return enemies_[i].node ? &enemies_[i] : NULL;
    for (size_t i = 0; i < ghosts_.size(); ++i)
        if (ghosts_[i].who == who) return &ghosts_[i].c;
    return NULL;
}

static Control* as_ctl(Node* n) { return n && n->is_control() ? (Control*)n : NULL; }

static Vec2 global_origin(Node* n) {
    return n && n->is_canvas_item() ? ((CanvasItem*)n)->global_transform().origin : Vec2(0, 0);
}

/* NCreature's points for the scripted effects: VfxSpawnPosition, and
 * GetTopOfHitbox / GetBottomOfHitbox (the hitbox's top and bottom middle). */
bool CombatScreen::creature(const std::string& who, VfxCreature* out) {
    Creature* cr = creature_for(who);
    if (!cr || !cr->node) return false;
    Xform2D xf = cr->node->global_transform();
    Rect2 h = cr->hitbox;
    out->center = cr->center ? global_origin(cr->center) : xf.xform(h.pos + Vec2(h.size.x * 0.5f, h.size.y * 0.5f));
    out->top = xf.xform(h.pos + Vec2(h.size.x * 0.5f, 0));
    out->hitbox = Rect2(xf.xform(h.pos), Vec2(h.size.x * sqrtf(xf.x.x * xf.x.x + xf.x.y * xf.x.y),
                                              h.size.y * sqrtf(xf.y.x * xf.y.x + xf.y.y * xf.y.y)));
    out->bottom = xf.xform(h.pos + Vec2(h.size.x * 0.5f, h.size.y));
    out->enemy = !cr->player;
    out->interactable = (cr->player || cr->data["hp"].i(1) > 0) && !uninteractable_.count(who);
    out->defect = cr == &player_ && lower(player_.data["character"].str()) == "defect";
    return true;
}

void CombatScreen::set_creature_flipped(const std::string& who, bool flipped) {
    Creature* cr = creature_for(who);
    if (cr && cr->node && (cr->node->scale.x < 0) != flipped) cr->node->scale.x = -cr->node->scale.x;
}

Node* CombatScreen::visuals_of(const std::string& who) {
    Creature* cr = creature_for(who);
    return cr ? cr->visuals : NULL;
}

bool CombatScreen::origin_of(const std::string& who, Vec2* out) {
    Creature* cr = creature_for(who);
    if (!cr || !cr->node) return false;
    *out = global_origin(cr->node);
    return true;
}

bool CombatScreen::side_floor(bool enemies, Vec2* out) {
    std::vector<std::string> side;
    if (enemies) side = enemy_who_;
    else side.push_back("player");
    Vec2 floor(0, 0);
    int n = 0;
    for (size_t i = 0; i < side.size(); ++i) {
        VfxCreature c;
        if (!creature(side[i], &c) || !c.interactable) continue;
        if (c.bottom.y > floor.y) floor.y = c.bottom.y;
        floor.x += c.bottom.x;
        ++n;
    }
    if (n == 0) return false;
    floor.x /= (float)n;
    *out = floor;
    return true;
}

bool CombatScreen::side_center(bool enemies, Vec2* out) {
    Cue c;
    c.where = "side";
    c.who = enemies ? "enemy" : "player";
    return vfx_position(c, out);
}

/* NCreature.ScaleTo / SetDefaultScaleTo / OstyScaleToSize: the visuals'
 * scale eases (Sine InOut) to base * temp * default (* Osty's size). */
void CombatScreen::scale_base(Creature& c) {
    if (!c.visuals || !c.visuals->is_node2d() || c.scale_base.x != 0 || c.scale_base.y != 0) return;
    c.scale_base = ((Node2D*)c.visuals)->scale * (1.0f / (c.scale_temp * c.scale_default * c.scale_osty));
}

void CombatScreen::scale_visuals(Creature& c, float duration) {
    if (!c.visuals || !c.visuals->is_node2d()) return;
    Node2D* v = (Node2D*)c.visuals;
    scale_base(c);
    float k = c.scale_temp * c.scale_default * c.scale_osty;
    tweens_.kill(&v->scale);
    if (duration <= 0) v->scale = c.scale_base * k;
    else tweens_.tween(&v->scale, c.scale_base * k, duration, TRANS_SINE, EASE_IN_OUT);
}

/* The game's calls on a creature's spine (NCreature.SpineAnimation's
 * animation state): SetAnimation(name, loop, track), AddAnimation(name,
 * delay, loop, track), SetTimeScale(scale); the receiver is args[0]. */
void CombatScreen::update_cost_rolls(double dt) {
    for (size_t i = cost_rolls_.size(); i-- > 0;) {
        CostRoll& r = cost_rolls_[i];
        r.t += (float)dt;
        Holder* h = NULL;
        for (size_t k = 0; k < holders_.size(); ++k)
            if (holders_[k].id == r.id) h = &holders_[k];
        Node* label = h && h->card_node ? h->card_node->find_unique("EnergyLabel") : NULL;
        bool done = r.t >= r.duration;
        if (label) {
            if (done) {
                /* UpdateEnergyCostVisuals: the cost it has now */
                const Json& c = h->data;
                set_label(label, c["cost_display"].is_string() ? c["cost_display"].str() : format("%d", c["cost"].i()));
            } else {
                float v = tween_interpolate(TRANS_SINE, EASE_OUT, r.t, 0, 50, r.duration);
                int n = (int)(r.offset + v) % 8;
                set_label(label, n > 3 ? std::string("?") : format("%d", (int)fmodf(v, 4)));
            }
        }
        if (done || !label) cost_rolls_.erase(cost_rolls_.begin() + (long)i);
    }
}

/* A creature's node moved along x by dx (TweenProperty "global_position:x"),
 * a move it was making replaced. */
void CombatScreen::move_creature_x(Creature& c, float dx, float duration, TweenTrans trans, TweenEase ease) {
    if (!c.node) return;
    for (size_t i = creature_moves_.size(); i-- > 0;)
        if (creature_moves_[i].node == c.node) creature_moves_.erase(creature_moves_.begin() + (long)i);
    CreatureMove m;
    m.node = c.node;
    m.from = c.node->position;
    m.to = m.from + Vec2(dx, 0);
    m.t = 0;
    m.duration = duration > 0 ? duration : 0.001f;
    m.trans = trans;
    m.ease = ease;
    creature_moves_.push_back(m);
}

/* SandpitPower (The Insatiable): Begin, where its target stood and the
 * amount it started from (AfterApplied); Update, UpdateCreaturePositions
 * (the target, or its Osty, drawn towards the owner as the amount counts
 * down: 0.25 s Cubic Out); Pull, AfterRemoved's pull of everyone it
 * holds to 450 px before it (0.7 s Sine InOut). */
bool CombatScreen::sandpit_call(const std::string& method, const Json& args) {
    const std::string& owner = args[(size_t)0].str();
    Creature* o = creature_for(owner);
    if (!o || !o->node) return false;
    float ox = global_origin(o->node).x;
    if (method == "Pull") {
        const Json& who = args[1];
        for (size_t i = 0; i < who.size(); ++i) {
            Creature* c = creature_for(who[i].str());
            if (c && c->node) move_creature_x(*c, ox - 450.0f - global_origin(c->node).x, 0.7f, TRANS_SINE, EASE_IN_OUT);
        }
        return true;
    }
    Creature* t = args.size() > 1 ? creature_for(args[1].str()) : NULL;
    if (!t || !t->node) return false;
    if (method == "Begin") {
        sandpit_x_[owner] = global_origin(t->node).x;
        sandpit_amount_[owner] = args[2].i();
        return true;
    }
    if (method != "Update") return false;
    if (!sandpit_amount_.count(owner) || sandpit_amount_[owner] <= 0) return true;
    int initial = sandpit_amount_[owner], amount = args[2].i();
    float num = ox - 450.0f, num2 = sandpit_x_[owner] + 50.0f;
    float num3 = (num2 - num) / (float)initial;
    int num4 = amount < initial ? amount : initial, num5 = amount - initial > 0 ? amount - initial : 0;
    Creature* lead = t;
    float num6 = 0;
    Creature* osty = args.size() > 4 && args[4].is_string() ? creature_for(args[4].str()) : NULL;
    if (osty && osty->node && t->player) {
        /* NCreature.GetOstyOffsetFromPlayer: where it stands from the player */
        float x = global_origin(osty->node).x - global_origin(t->node).x;
        lead = osty;
        num2 += x;
        num3 = (num2 - num) / (float)initial;
        num6 = x - 100.0f * (1.0f - (float)num4 / (float)initial);
    }
    float num8 = ox - 400.0f + num3 * (float)num4 + num3 * ((float)num5 / ((float)num5 + 2.0f));
    float lead_x = global_origin(lead->node).x, target_x = global_origin(t->node).x;
    const Json& who = args[3];
    for (size_t i = 0; i < who.size(); ++i) {
        Creature* c = creature_for(who[i].str());
        if (!c || !c->node) continue;
        float d = c == lead ? num8 - lead_x : num8 - num6 - target_x;
        if (fabsf(d) > 5.0f) move_creature_x(*c, d, 0.25f, TRANS_CUBIC, EASE_OUT);
    }
    return true;
}

/* The scripted calls that need the screen's creatures: Vantom.ScaleTo
 * (ShrinkPower), Visuals/ScalingBone's scale to the size given, Sine
 * InOut; FabricatorNormal.SetBotFallPosition, Visuals/FallControl's
 * Position = Down * ((125 - the node's y) / the body's scale y);
 * SurroundedPower.FaceDirection, the creatures' bodies and form effects
 * flipped to face that way. */
void CombatScreen::special_scripted(const std::string& call, const Json& args) {
    if (call == "NCreature.FaceDirection") {
        bool left = args.size() > 1 && args[1].b();
        const Json& who = args[(size_t)0];
        for (size_t i = 0; i < who.size(); ++i) {
            Creature* c = creature_for(who[i].str());
            if (!c || !c->visuals) continue;
            Node* parts[2] = { c->visuals->find_unique("Visuals"), c->visuals->find_unique("FormVfx") };
            Node* phobia = c->visuals->find_unique("PhobiaModeVisuals");
            if (phobia && phobia->is_canvas_item() && ((CanvasItem*)phobia)->visible) parts[0] = phobia;
            for (int k = 0; k < 2; ++k) {
                Vec2* scale = !parts[k] ? NULL : parts[k]->is_node2d() ? &((Node2D*)parts[k])->scale
                            : parts[k]->is_control() ? &((Control*)parts[k])->scale : NULL;
                if (scale && ((!left && scale->x < 0) || (left && scale->x > 0))) scale->x = -scale->x;
            }
        }
        return;
    }
    const std::string& name = args[(size_t)0].str();
    size_t hash = name.find('#');
    if (hash == std::string::npos) return;
    Creature* cr = creature_for(name.substr(0, hash));
    Node* n = cr && cr->visuals ? cr->visuals->get_node(name.substr(hash + 1)) : NULL;
    if (!n || !n->is_node2d()) return;
    Node2D* nd = (Node2D*)n;
    if (call == "SpecialNode.ScaleTo") {
        float s = args[1].f(1);
        tweens_.kill(&nd->scale);
        tweens_.tween(&nd->scale, Vec2(s, s), args[2].f(), TRANS_SINE, EASE_IN_OUT);
    } else if (call == "SpecialNode.BotFall" && cr->node) {
        Node* body = cr->visuals->find_unique("Visuals");
        float sy = body && body->is_node2d() ? ((Node2D*)body)->scale.y : 1.0f;
        if (sy != 0) nd->position = Vec2(0, (125.0f - cr->node->position.y) / sy);
    }
}

bool CombatScreen::spine_call(const std::string& method, const Json& args) {
    Creature* cr = args.size() > 0 ? creature_for(args[(size_t)0].str()) : NULL;
    Node* body = cr && cr->visuals ? cr->visuals->find_unique("Visuals") : NULL;
    SpineSprite* s = cr ? first_spine(body ? body : cr->visuals) : NULL;
    if (!s) return false;
    if (method == "SetTimeScale") {
        s->set_state_time_scale(args[1].f(1));
        return true;
    }
    Json op;
    if (method == "SetAnimation") {
        op.set("op", Json::make_string("set_animation"));
        op.set("name", args[1]);
        op.set("loop", Json::make_bool(args[2].b(true)));
        op.set("track", Json::make_number(args[3].i()));
    } else if (method == "AddAnimation" || method == "AddAnimationTracked") {
        op.set("op", Json::make_string("add_animation"));
        op.set("name", args[1]);
        op.set("delay", Json::make_number(args[2].f()));
        op.set("loop", Json::make_bool(args[3].b(true)));
        op.set("track", Json::make_number(args[4].i()));
    } else {
        return false;
    }
    Json ops = Json::make_array();
    ops.append(op);
    play_animator_ops(s, ops);
    return true;
}

bool CombatScreen::creature_call(const std::string& method, const Json& args) {
    Creature* c = args.size() > 0 ? creature_for(args[(size_t)0].str()) : NULL;
    if (!c || !c->node) return false;
    float a1 = args.size() > 1 ? (float)args[(size_t)1].num(0) : 0, a2 = args.size() > 2 ? (float)args[(size_t)2].num(0) : 0;
    /* the unscaled size from the factors as they are, before one changes */
    if (method == "ScaleTo" || method == "SetDefaultScaleTo" || method == "OstyScaleToSize") scale_base(*c);
    if (method == "ScaleTo") {
        c->scale_temp = a1;
        scale_visuals(*c, a2);
    } else if (method == "SetDefaultScaleTo") {
        c->scale_default = a1;
        scale_visuals(*c, a2);
    } else if (method == "OstyScaleToSize") {
        /* Osty.ScaleRange 1..2 over 0..150 HP */
        c->scale_osty = 1.0f + clampf(a1 / 150.0f, 0.0f, 1.0f);
        scale_visuals(*c, a2);
    } else if (method == "StartReviveAnim") {
        /* the Revive trigger where the creature has one, else (the player)
         * a blink back to idle; its UI comes back. */
        c->node->visible = true;
        tweens_.kill(&c->node->modulate.a);
        c->node->modulate.a = 1;
        enable_ui(*c);
        segment_phobia(lib_, c->visuals, c->data, false);
        SpineSprite* s = first_spine(c->visuals);
        if (!(s && s->set_animation("revive", false, 0))) play_idle(c->node);
        else if (s->skeleton() && s->animation_state()) {
            const SpAnimation* idle = s->skeleton()->data()->find_animation("idle_loop");
            if (idle) s->animation_state()->add_animation(0, idle, true, 0);
        }
    } else if (method == "FadeIn") {
        /* OstyCmd.Summon: the new Osty's node from transparent to white */
        tweens_.kill(&c->node->modulate.a);
        c->node->modulate.a = 0;
        tweens_.tween(&c->node->modulate.a, 1.0f, a1, TRANS_LINEAR, EASE_IN_OUT, a2);
    } else if (method == "AnimDisableUi") {
        /* the state display out (Expo Out, 0.5 s after 0.5 s) */
        Control* state = ctl(c->node, "HealthBar");
        if (state) tweens_.tween(&state->modulate.a, 0.0f, 0.5f, TRANS_EXPO, EASE_OUT, 0.5f);
    } else if (method == "ToggleIsInteractable") {
        /* its state display (health bar, block, powers) shown or hidden: a
         * fleeing Fat Gremlin or Thieving Hopper, a Decimillipede segment
         * waiting to reattach (NCombatRoom.SetCreatureIsInteractable) */
        bool on = args.size() > 1 && args[(size_t)1].b();
        set_visible(c->node->find_unique("StateDisplay"), on);
        if (on) uninteractable_.erase(args[(size_t)0].str());
        else uninteractable_.insert(args[(size_t)0].str());
    } else if (method == "AnimHideIntent") {
        /* the intents out over 0.5 s (after the delay given) */
        Control* row = ctl(c->node, "Intents");
        if (row) {
            tweens_.kill(&row->modulate.a);
            tweens_.tween(&row->modulate.a, 0.0f, 0.5f, TRANS_LINEAR, EASE_IN_OUT, a1);
        }
    } else if (method == "HideVisuals") {
        /* SandpitPower.AfterRemoved: Visuals.Visible = false */
        if (c->visuals) set_visible(c->visuals, false);
    } else if (method == "ShowVisuals") {
        /* the debug option invincible spared the player Sandpit's kill
         * (logic DebugOptions.cs): the body back */
        if (c->visuals) set_visible(c->visuals, true);
    } else if (method == "PerformIntent") {
        /* AnimHideIntent(0.4): the intents fade out over 0.5 s after 0.4 s. */
        Control* row = ctl(c->node, "Intents");
        if (row) tweens_.tween(&row->modulate.a, 0.0f, 0.5f, TRANS_LINEAR, EASE_IN_OUT, 0.4f);
    } else if (method == "TrackBlockStatus") {
        if (args.size() > 1) {
            block_tracking_[args[(size_t)0].str()] = args[(size_t)1].str();
            refresh_health_bar(*c, c->data, c->powers);
        }
    } else if (method == "RefreshIntents") {
        /* RevealIntents: from transparent to opaque over 1 s, after 0-0.3 s. */
        Control* row = ctl(c->node, "Intents");
        if (row) {
            tweens_.kill(&row->modulate.a);
            row->modulate.a = 0;
            tweens_.tween(&row->modulate.a, 1.0f, 1.0f, TRANS_LINEAR, EASE_IN_OUT,
                          (float)(chaotic_rand() % 301) / 1000.0f);
        }
    } else {
        return false;
    }
    return true;
}

void CombatScreen::ui_event(const std::string& ev) {
    if (ev == "CombatSetUp") {
        combat_ui_anim(true);
        /* NOrbManager.OnCombatSetup: AddSlotAnim, the slots out from the
         * middle to the arc (TweenLayout: 0.45 s, Sine InOut) */
        for (size_t i = 0; i < orbs_.size(); ++i) {
            if (!orbs_[i].node) continue;
            orbs_[i].from = Vec2(0, 0);
            orbs_[i].t = 0;
            orbs_[i].node->set_position(Vec2(0, 0));
        }
        /* ShinePotions: after 1 s, each holder with a potion shines in turn
         * (0.25 s apart), by slot */
        const Json& potions = state_["player"]["potions"];
        potion_shines_.clear();
        double at = life_t_ + 1.0;
        for (int slot = 0; slot < 10; ++slot) {
            bool here = false;
            for (size_t i = 0; i < potions.size(); ++i)
                if (potions[i]["index"].i(-1) == slot && !potions[i]["id"].str().empty()) here = true;
            potion_shines_.push_back(here ? at : -1);
            if (here) at += 0.25;
        }
    } else if (ev == "CombatEnded") {
        combat_ui_anim(false);
        /* NCreature.OnCombatEnded: AnimHideIntent; OrbManager.ClearOrbs: the
         * orbs to the middle (1 s, Sine InOut) as they fade (0.25 s) */
        for (size_t i = 0; i < enemies_.size(); ++i) {
            Control* row = enemies_[i].node ? ctl(enemies_[i].node, "Intents") : NULL;
            if (row) {
                tweens_.kill(&row->modulate.a);
                tweens_.tween(&row->modulate.a, 0.0f, 0.5f);
            }
        }
        for (size_t i = 0; i < orbs_.size(); ++i) {
            if (!orbs_[i].node) continue;
            FadingOrb f = { orbs_[i].node, life_t_ + 0.25 };
            fading_orbs_.push_back(f);
        }
        orbs_.clear();
    } else if (ev == "AboutToSwitchToEnemyTurn") {
        end_turn_state(0);
    } else if (ev == "PlayerEndedTurn") {
        if (end_state_ != 0) end_turn_state(2);
    } else if (ev == "TurnStarted") {
        end_turn_state(1);
    }
}

/* NCombatUi.AnimIn / AnimOut: NPlayerHand from / to 500 px below (0.8 s,
 * Expo Out / Back In), NEnergyCounter from / to (-480, 128) (0.6 s), the
 * piles from / to their hide offsets (draw (-150, 100), discard (150, 100),
 * exhaust (150, 0); 0.5 s); going out, the end turn button hides and the
 * play pile fades (PostCombatCleanUp, 0.25 s). */
void CombatScreen::combat_ui_anim(bool in) {
    Node* ui = root_ ? find_named(root_, "CombatUi") : NULL;
    if (!ui) return;
    struct Part {
        Control* c;
        Vec2 hide;
        float seconds;
    } parts[5];
    int n = 0;
    Node* hand = find_named(ui, "Hand");
    if (hand && hand->is_control()) { parts[n].c = (Control*)hand; parts[n].hide = Vec2(0, 500); parts[n++].seconds = 0.8f; }
    Node* box = ui->get_node("EnergyCounterContainer");
    if (box && !box->children.empty() && box->children[0]->is_control()) {
        parts[n].c = (Control*)box->children[0]; parts[n].hide = Vec2(-480, 128); parts[n++].seconds = 0.6f;
    }
    Node* piles = find_named(ui, "CombatPileContainer");
    static const char* names[] = { "DrawPile", "DiscardPile", "ExhaustPile" };
    static const float hide[][2] = { { -150, 100 }, { 150, 100 }, { 150, 0 } };
    for (int i = 0; piles && i < 3; ++i) {
        Node* p = piles->get_node(names[i]);
        if (!p || !p->is_control()) continue;
        parts[n].c = (Control*)p; parts[n].hide = Vec2(hide[i][0], hide[i][1]); parts[n++].seconds = 0.5f;
    }
    for (int i = 0; i < n; ++i) {
        Control* c = parts[i].c;
        static const Side sides[4] = { SIDE_LEFT, SIDE_TOP, SIDE_RIGHT, SIDE_BOTTOM };
        for (int s = 0; s < 4; ++s) {
            float* v = &c->offset[sides[s]];
            float d = (s % 2 == 0) ? parts[i].hide.x : parts[i].hide.y;
            tweens_.kill(v);
            if (in) tweens_.tween_from(v, *v + d, *v, parts[i].seconds, TRANS_EXPO, EASE_OUT);
            else tweens_.tween(v, *v + d, parts[i].seconds, TRANS_BACK, EASE_IN);
        }
    }
    if (!in) {
        end_turn_state(0);
        Node* play = find_named(ui, "PlayContainer");
        if (play && play->is_canvas_item()) tweens_.tween(&((CanvasItem*)play)->modulate.a, 0.0f, 0.25f);
    }
}

/* NEndTurnButton.SetState: hidden slides it 250 px down (AnimOut, 0.5 s
 * Expo Out); enabled from hidden back to ShowPos (AnimIn, 0.5 s Back Out);
 * RefreshEnabled: enabled is white and cream, else StsColors.gray. */
void CombatScreen::end_turn_state(int state) {
    Node* ui = root_ ? find_named(root_, "CombatUi") : NULL;
    Control* end = ui ? child_ctl(ui, "EndTurnButton") : NULL;
    if (!end || state == end_state_) return;
    float y = end->offset[SIDE_TOP], h = end->offset[SIDE_BOTTOM] - end->offset[SIDE_TOP];
    if (state == 0) {
        tweens_.kill(&end->offset[SIDE_TOP]);
        tweens_.kill(&end->offset[SIDE_BOTTOM]);
        tweens_.tween(&end->offset[SIDE_TOP], 846.0f + 250.0f, 0.5f, TRANS_EXPO, EASE_OUT);
        tweens_.tween(&end->offset[SIDE_BOTTOM], 846.0f + 250.0f + h, 0.5f, TRANS_EXPO, EASE_OUT);
    } else if (state == 1 && end_state_ == 0) {
        tweens_.kill(&end->offset[SIDE_TOP]);
        tweens_.kill(&end->offset[SIDE_BOTTOM]);
        tweens_.tween_from(&end->offset[SIDE_TOP], y, 846.0f, 0.5f, TRANS_BACK, EASE_OUT);
        tweens_.tween_from(&end->offset[SIDE_BOTTOM], y + h, 846.0f + h, 0.5f, TRANS_BACK, EASE_OUT);
    }
    end_state_ = state;
    bool on = state == 1 && !sel_mode_;
    Node* image = end->get_node("Visuals/Image");
    Node* label = end->get_node("Visuals/Label");
    if (image && image->is_canvas_item()) ((CanvasItem*)image)->modulate = on ? Color(1, 1, 1, 1) : Color(0.5f, 0.5f, 0.5f, 1);
    if (label && label->is_canvas_item())
        ((CanvasItem*)label)->modulate = on ? Color(1, 0.964706f, 0.886275f, 1) : Color(0.5f, 0.5f, 0.5f, 1);
    refresh_end_glow();
}

/* NEndTurnButton.StartOrStopPulseVfx: enabled with no card that can be
 * played, Glow to 0.75 (0.8 s, Back Out) growing 0.45 -> 0.5 and GlowVfx
 * pulsing; otherwise both out in 0.5 s (Expo Out). */
void CombatScreen::refresh_end_glow() {
    Node* ui = root_ ? find_named(root_, "CombatUi") : NULL;
    Node* end = ui ? ui->get_node("EndTurnButton") : NULL;
    Node* gn = end ? end->get_node("Visuals/Glow") : NULL;
    Node* vn = end ? end->get_node("Visuals/GlowVfx") : NULL;
    if (!gn || !gn->is_control() || !vn || !vn->is_control()) return;
    bool playable = false;
    const Json& hand = state_["hand"];
    for (size_t i = 0; i < hand.size(); ++i)
        if (hand[i]["can_play"].b() && hand[i]["cost"].i() <= state_["energy"].i()) playable = true;
    bool shiny = !playable && end_state_ == 1 && !sel_mode_;
    if (shiny == end_shiny_) return;
    end_shiny_ = shiny;
    Control* glow = (Control*)gn;
    Control* vfx = (Control*)vn;
    tweens_.kill(&glow->modulate.a);
    tweens_.kill(&glow->scale);
    if (shiny) {
        end_glow_t_ = 0;
        tweens_.tween(&glow->modulate.a, 0.75f, 0.8f, TRANS_BACK, EASE_OUT);
        tweens_.tween_from(&glow->scale, Vec2(0.45f, 0.45f), Vec2(0.5f, 0.5f), 0.8f, TRANS_BACK, EASE_OUT);
    } else {
        tweens_.tween(&glow->modulate.a, 0.0f, 0.5f, TRANS_EXPO, EASE_OUT);
        tweens_.tween(&vfx->modulate.a, 0.0f, 0.5f, TRANS_EXPO, EASE_OUT);
    }
}

/* NCeremonialBeastBgVfx.UpdateState at each change of the fight:
 * UpdateRingingSfx (the player's Ringing), UpdateVfxAndMusic (the beast:
 * none or dead, flowers and progress 5; before its second phase hidden;
 * above a third of its HP the glow, below it the skulls, progress 1). */
void CombatScreen::update_beast_bg() {
    if (beast_bg_.empty()) return;
    float ringing = find_power(player_.powers, "RINGING_POWER") ? 1.0f : 0.0f;
    const Json* beast = NULL;
    const Json& es = state_["enemies"];
    for (size_t i = 0; i < es.size(); ++i)
        if (es[i]["id"].str() == "CEREMONIAL_BEAST") beast = &es[i];
    int next = 0;
    float progress = beast_progress_;
    bool shown = true;
    if (!beast || (*beast)["hp"].i() <= 0) {
        next = 3;
        progress = 5;
    } else if (!(*beast)["second_phase"].b()) {
        shown = false;
    } else if ((float)(*beast)["hp"].i() > (float)(*beast)["max_hp"].i() * 0.33f) {
        next = 1;
        progress = 1;
    } else {
        next = 2;
        progress = 1;
    }
    for (size_t i = 0; i < beast_bg_.size(); ++i) set_visible(beast_bg_[i], shown || next == 3);
    if (lib_->audio()) {
        const char* names[2] = { "ringing", "ceremonial_beast_progress" };
        float values[2] = { ringing, progress };
        float* last[2] = { &beast_ringing_, &beast_progress_ };
        for (int k = 0; k < 2; ++k) {
            if (values[k] < 0 || values[k] == *last[k]) continue;
            *last[k] = values[k];
            Json cue, args = Json::make_array();
            cue.set("t", Json::make_string("ui"));
            cue.set("call", Json::make_string("NRunMusicController.UpdateMusicParameter"));
            args.append(Json::make_string(names[k]));
            args.append(Json::make_number(values[k]));
            cue.set("args", args);
            lib_->audio()->cue(cue);
        }
    }
    /* PlayGlow / PlaySkulls once each; PlayFlowers */
    if (next == 0 || (next == beast_bg_state_ && next != 3) || (next == 3 && beast_bg_state_ == 3)) return;
    if (next < beast_bg_state_ && next != 3) return;
    beast_bg_state_ = next;
    static const char* const spawn[4] = { "", "glow_spawn", "skulls_spawn", "glow_and_skulls_idle" };
    static const char* const then[4] = { "", "glow_idle", "glow_and_skulls_idle", "plants_spawn" };
    for (size_t i = 0; i < beast_bg_.size(); ++i) {
        SpineSprite* s = beast_bg_[i];
        if (!s->set_animation(spawn[next], next == 3, 0)) continue;
        SpSkeleton* sk = s->skeleton();
        const SpAnimation* a = sk ? sk->data()->find_animation(then[next]) : NULL;
        if (a && s->animation_state()) s->animation_state()->add_animation(0, a, next != 3, next == 3 ? 4.5f : 0);
    }
}

bool CombatScreen::slot_position(const Json& enemy, Vec2* out) const {
    if (!encounter_slots_ || !enemy["slot"].is_string()) return false;
    Node* m = encounter_slots_->get_node(enemy["slot"].str());
    if (!m || !m->is_node2d()) return false;
    *out = Vec2(-960, -540) + ((Node2D*)m)->position;
    return true;
}

std::string CombatScreen::creature_who(const Creature& c) const {
    if (c.player) return "player";
    if (&c == &osty_) return "osty";
    return format("e%d", c.data["combat_id"].i(-1));
}

/* NPower.FlashPower: the row icon's PowerFlash particles */
void CombatScreen::power_row_flash(const std::string& who, const std::string& power) {
    Creature* c = creature_for(who);
    Control* box = c ? ctl(ctl(c->node, "HealthBar"), "PowerContainer") : NULL;
    Node* n = box ? box->get_node("Power_" + power) : NULL;
    if (n) vfx_restart(n->find_unique("PowerFlash"));
}

/* NCreature.AnimShake: unless one runs or it plays "hurt", the visuals
 * from x 0 by 10 * sin(4u) * sin(u / 2), u 0 -> 2 pi over 1 s (Cubic Out);
 * process() runs it. */
void CombatScreen::anim_shake(const std::string& who) {
    Creature* c = creature_for(who);
    if (!c || !c->node || !c->visuals || shakes_.count(who)) return;
    SpineSprite* s = first_spine(c->visuals);
    const SpAnimation* a = s && s->animation_state() ? s->animation_state()->current(0) : NULL;
    if (a && a->name == "hurt") return;
    shakes_[who] = 0;
}

/* NRadialBlurVfx.Activate: the room's RadialBlur (a BackBufferCopy whose
 * Rect reads the screen through radial_blur) shows, its centre's x by the
 * position (Left 0.3, Right 0.6, else 0.45), its power up to 0.005 in 0.1 s
 * then down to 0 in 0.9 s (Cubic In); then it hides. Not while showing. */
void CombatScreen::radial_blur(const std::string& position) {
    Node* rb = root_ ? find_named(root_, "RadialBlur") : NULL;
    Node* rect = rb ? rb->get_node("Rect") : NULL;
    if (!rb || !rect || !rect->is_canvas_item() || radial_off_at_ > 0) return;
    set_visible(rb, true);
    CanvasItem* r = (CanvasItem*)rect;
    r->shader_params["blur_center.x"] = position == "Left" ? 0.3f : position == "Right" ? 0.6f : 0.45f;
    float* power = &r->shader_params["blur_power"];
    tweens_.tween(power, 0.005f, 0.1f);
    tweens_.tween(power, 0.0f, 0.9f, TRANS_CUBIC, EASE_IN, 0.1f);
    radial_off_at_ = life_t_ + 1.0;
}

/* NCreature.AnimDie: the state display fades (AnimDisableUi); a player
 * stays. A monster waits for its death animation (and half a second, or
 * its DeathAnimLengthOverride without one), then fades through
 * NMonsterDeathVfx (ShouldFadeAfterDeath) or is gone. */
/* AnimEnableUi: the state display back in 0.5 s after 0.5 s (a revive, or
 * a creature spawned where one died). */
void CombatScreen::enable_ui(Creature& c) {
    Control* state = ctl(c.node, "HealthBar");
    if (!state || state->modulate.a >= 1) return;
    tweens_.kill(&state->modulate.a);
    tweens_.tween(&state->modulate.a, 1.0f, 0.5f, TRANS_EXPO, EASE_OUT, 0.5f);
}

/* DecimillipedeSegment.ChangePhobiaModeTexture (AfterDeath / ReattachMove):
 * in phobia mode a segment's body is its shriveled image while it is down,
 * its own once it reattaches */
static void segment_phobia(SceneLibrary* lib, Node* visuals, const Json& data, bool dead) {
    const std::string& id = data["id"].str();
    if (!visuals || id.compare(0, 21, "DECIMILLIPEDE_SEGMENT") != 0 || !client_settings()["phobia_mode"].b()) return;
    Node* body = visuals->find_unique("PhobiaModeVisuals");
    if (!body || body->type != "Sprite2D") return;
    Resource* r = lib->ext("images/monsters/phobia_mode/" + lower(id) + (dead ? "_shriveled_phobia.png" : "_phobia.png"));
    if (r && r->is_texture()) ((Sprite2D*)body)->texture = (Texture2DRes*)r;
}

void CombatScreen::monster_death(Creature& cr, const std::string& who, SpineSprite* s, bool keep) {
    segment_phobia(lib_, cr.visuals, cr.data, true);
    /* AnimDisableUi: the state display (%HealthBar) only; the body plays
     * its death animation */
    Control* state = ctl(cr.node, "HealthBar");
    if (state) tweens_.tween(&state->modulate.a, 0.0f, 0.5f, TRANS_EXPO, EASE_OUT, 0.5f);
    if (cr.player || who.empty() || who[0] != 'e' || keep) return;
    dying_.push_back(who);
    double wait = 0;
    SpAnimationState* st = s ? s->animation_state() : NULL;
    const SpAnimation* a = st ? st->current(0) : NULL;
    if (a) wait = minf(a->duration - maxf(st->track_time(0), 0.0f) + 0.5f, 20.0f);
    else wait = cr.data["death_anim_length"].f(0);
    if (wait < 0) wait = 0;
    /* ShowRewards: GetCurrentAnimationTimeRemaining, else
     * DeathAnimLengthOverride */
    double left = a ? a->duration - maxf(st->track_time(0), 0.0f) : cr.data["death_anim_length"].f(0);
    death_end_[who] = life_t_ + (left > 0 ? left : 0);
    bool fade = cr.data["death_fade"].is_bool() ? cr.data["death_fade"].b() : true;
    if (fade && cr.node->parent) {
        Vec2 pad(1.2f, 1.2f);
        const Json& p = cr.data["death_padding"];
        if (p.size() >= 2) pad = Vec2(p[(size_t)0].f(1.2f), p[(size_t)1].f(1.2f));
        spawn_monster_death(lib_, cr.node->parent, *this, who, wait, pad);
        /* a body the effect found nothing to fade in goes too (after the
         * effect's moment: it hides what it takes) */
        fade_gone_at_[who] = life_t_ + wait + 0.5;
    } else if (cr.visuals && cr.visuals->is_canvas_item()) {
        /* freed at once (the end of its animation leaves nothing), after any
         * IDeathDelayer of it is done */
        DelayedHide h = { (CanvasItem*)cr.visuals, life_t_ + wait, cr.node };
        delayed_hides_.push_back(h);
    }
}

/* NCombatRoom.Ui, where the exhaust effects go. */
Node* CombatScreen::exhaust_ui() {
    Node* ui = root_ ? find_named(root_, "CombatUi") : NULL;
    return ui ? ui : (hand_box_ ? hand_box_->parent : NULL);
}

Node* CombatScreen::take_visuals(const std::string& who) {
    Creature* cr = creature_for(who);
    if (!cr) return NULL;
    Node* v = cr->visuals;
    cr->visuals = NULL;
    cr->center = NULL; /* inside the visuals */
    return v;
}

Node* CombatScreen::back_box() { return root_ ? find_named(root_, "BackCombatVfxContainer") : NULL; }

/* Where VfxCmd puts an effect: a creature's VfxSpawnPosition (%CenterPos)
 * or its position (its feet), the middle of a side's creatures, the
 * screen's centre. */
bool CombatScreen::vfx_position(const Cue& c, Vec2* out) {
    if (c.where == "viewport") {
        *out = Vec2(480, 540);
        return true;
    }
    if (c.where == "screen") {
        *out = Vec2(960, 540);
        return true;
    }
    if (c.where == "side") {
        Vec2 sum(0, 0);
        int n = 0;
        if (c.who == "player") {
            if (player_.node && player_.center) {
                sum = sum + global_origin(player_.center);
                ++n;
            }
        } else {
            for (size_t i = 0; i < enemies_.size(); ++i)
                if (enemies_[i].node && enemies_[i].center) {
                    sum = sum + global_origin(enemies_[i].center);
                    ++n;
                }
        }
        if (!n) return false;
        *out = sum * (1.0f / (float)n);
        return true;
    }
    Creature* cr = creature_for(c.who);
    if (!cr) return false;
    *out = c.where == "feet" || !cr->center ? global_origin(cr->node) : global_origin(cr->center);
    return true;
}

/* CreatureAnimator's default states (MonsterModel.GenerateAnimator): the
 * trigger's animation, then idle_loop again (not after dying). */
static const char* trigger_animation(const std::string& trigger) {
    if (trigger == "Attack") return "attack";
    if (trigger == "Cast") return "cast";
    if (trigger == "Hit") return "hurt";
    if (trigger == "Dead") return "die";
    if (trigger == "Idle") return "idle_loop";
    return NULL;
}

void CombatScreen::fire(const Cue& c) {
    if (c.kind == CUE_AUDIO) { if (lib_->audio()) lib_->audio()->cue(c.data); return; }
    if (c.kind == CUE_STATE) {
        if (apply_state(c.data)) CueLedger::used(c.data);
        else CueLedger::dropped(c.data);
        return;
    }
    if (c.kind == CUE_SCRIPTED) CueLedger::used(c.data);
    if (c.kind == CUE_SYNC) { finish_state(c.data, c.amount != 0); return; }
    if (c.kind == CUE_BANNER) {
        if (c.name == "enemy") burn_play_pile();
        show_banner(c.name, c.amount);
        return;
    }
    if (c.kind == CUE_ANIM) {
        if (c.name == "Dead" && lib_->audio()) lib_->audio()->stop_room_loops(c.who);
        Creature* cr = creature_for(c.who);
        SpineSprite* s = cr ? first_spine(cr->visuals) : NULL;
        const char* name = trigger_animation(c.name);
        std::string anim = name ? name : lower(c.name);
        bool dying = c.name == "Dead" && cr && cr->node;
        if (dying) {
            cr->data.set("hp", Json::make_number(0));
            refresh_state(*cr, true);
            hide(cr->node->find_unique("Intents"));
        }
        /* the creature's CreatureAnimator (the logic ran it) */
        if (c.data["play"].is_array()) {
            bool played = play_animator_ops(s, c.data["play"]);
            if (cr && cr->node && !dying) apply_bounds(*cr, c.data["play"]);
            if (dying) monster_death(*cr, c.who, played ? s : NULL, c.data["keep"].b());
            return;
        }
        bool played = s && s->set_animation(anim, false, 0);
        if (dying) monster_death(*cr, c.who, played ? s : NULL, c.data["keep"].b());
        if (!played) return;
        SpSkeleton* sk = s->skeleton();
        const SpAnimation* idle = sk && anim != "die" ? sk->data()->find_animation("idle_loop") : NULL;
        if (idle && s->animation_state()) s->animation_state()->add_animation(0, idle, true, 0);
        return;
    }
    if (c.kind == CUE_SCRIPTED && ScreenShake::get().cue(c.data["call"].str(), c.data["args"])) return;
    if (c.kind == CUE_SCRIPTED && c.data["args"].size() > 0) {
        /* a power's form (Demon Form, ...) on the creature's visuals */
        const std::string& who = c.data["args"][(size_t)0].str();
        if (form_vfx_create(lib_, c.data["call"].str(), visuals_of(who),
                            who == "player" ? player_.data["character"].str() : std::string()))
            return;
    }
    if (c.kind == CUE_SCRIPTED && c.data["call"].str() == "NGame.DoHitStop") {
        /* NHitStop.EaseForStrength / SecondsForDuration */
        const std::string& strength = c.data["args"][(size_t)0].str();
        const std::string& duration = c.data["args"][1].str();
        TweenTrans trans = strength == "VeryWeak" ? TRANS_CIRC : strength == "Weak" ? TRANS_SINE
                         : strength == "Medium" ? TRANS_QUAD : strength == "Strong" ? TRANS_QUART : TRANS_EXPO;
        float seconds = duration == "Short" ? 0.15f : duration == "Normal" ? 0.3f : duration == "Long" ? 0.6f : 2.0f;
        SceneTree::hit_stop(seconds, trans);
        return;
    }
    double crab_waits;
    if (c.kind == CUE_SCRIPTED && kaiser_crab_call(c.data["call"].str(), c.data["args"], &crab_waits)) return;
    if (c.kind == CUE_SCRIPTED && (c.data["call"].str() == "SpecialNode.ScaleTo" || c.data["call"].str() == "SpecialNode.BotFall" ||
                                   c.data["call"].str() == "NCreature.FaceDirection")) {
        special_scripted(c.data["call"].str(), c.data["args"]);
        return;
    }
    if (c.kind == CUE_SCRIPTED && special_node_call(c.data["call"].str(), c.data["args"], *this)) return;
    if (c.kind == CUE_SCRIPTED && c.data["call"].str().compare(0, 19, "MegaAnimationState.") == 0) {
        if (!spine_call(c.data["call"].str().substr(19), c.data["args"])) CueLedger::retract(c.data);
        return;
    }
    if (c.kind == CUE_SCRIPTED && c.data["call"].str().compare(0, 21, "SpineAnimationAccess.") == 0) {
        if (!spine_call(c.data["call"].str().substr(21), c.data["args"])) CueLedger::retract(c.data);
        return;
    }
    if (c.kind == CUE_SCRIPTED && c.data["call"].str().compare(0, 8, "Sandpit.") == 0) {
        if (!sandpit_call(c.data["call"].str().substr(8), c.data["args"])) CueLedger::retract(c.data);
        return;
    }
    if (c.kind == CUE_SCRIPTED && c.data.has("intents") &&
        (c.data["call"].str() == "NCreature.RefreshIntents" || c.data["call"].str() == "NCreature.UpdateIntent")) {
        /* NCreature.UpdateIntent: the row shows the intents the game's call
         * saw (a turn's new moves, a power changing their numbers) at once,
         * its fade left as it is; RefreshIntents then reveals them */
        const Json& args = c.data["args"];
        std::string who = args.size() > 0 ? args[(size_t)0].str() : "";
        for (size_t i = 0; i < enemy_who_.size() && i < enemies_.size(); ++i) {
            if (enemy_who_[i] != who || !enemies_[i].node) continue;
            Json intents = c.data["intents"].size() > 0 ? c.data["intents"] : Json();
            if (enemies_[i].data["intents"] != intents) {
                Control* row = ctl(enemies_[i].node, "Intents");
                float alpha = row ? row->modulate.a : 1.0f;
                make_intents(enemies_[i], intents, (int)i);
                if (row) row->modulate.a = alpha;
                enemies_[i].data.set("intents", intents);
            }
            break;
        }
        if (c.data["call"].str() == "NCreature.UpdateIntent") return;
    }
    if (c.kind == CUE_SCRIPTED && c.data["call"].str().compare(0, 10, "NCreature.") == 0) {
        if (!creature_call(c.data["call"].str().substr(10), c.data["args"])) CueLedger::retract(c.data);
        return;
    }
    if (c.kind == CUE_SCRIPTED && c.data["call"].str() == "NMonsterDeathVfx.Create") {
        /* ReattachPower.DoFadeOutOnAllSegments: the intents out (their own
         * cues), then after 0.25 s (PlayVfxAndThenRemoveNodes) one fade over
         * every segment's body */
        const Json& list = c.data["args"][(size_t)0];
        std::vector<std::string> who;
        std::vector<Vec2> pads;
        Node* parent = NULL;
        for (size_t i = 0; i < list.size(); ++i) {
            Creature* cr = creature_for(list[i].str());
            if (!cr || !cr->node) continue;
            const Json& p = cr->data["death_padding"];
            who.push_back(list[i].str());
            pads.push_back(p.size() >= 2 ? Vec2(p[(size_t)0].f(1.2f), p[(size_t)1].f(1.2f)) : Vec2(1.2f, 1.2f));
            if (!parent) parent = cr->node->parent;
        }
        if (parent && !who.empty()) {
            spawn_monster_death_group(lib_, parent, *this, who, 0.25, pads);
            /* their DeathAnimationTask: what the rewards wait for (the
             * fade is at most 2.5 s) */
            for (size_t i = 0; i < who.size(); ++i) death_end_[who[i]] = life_t_ + 0.25 + 2.5;
        } else {
            CueLedger::retract(c.data);
        }
        return;
    }
    if (c.kind == CUE_SCRIPTED && c.data["call"].str() == "NCombatRoom.SetCreatureIsInteractable") {
        if (!creature_call("ToggleIsInteractable", c.data["args"])) CueLedger::retract(c.data);
        return;
    }
    if (c.kind == CUE_SCRIPTED && c.data["call"].str() == "NCombatRoom.ShakeOstyIfDead") {
        /* the owner's Osty node shakes (Osty.CheckMissingWithAnim) */
        anim_shake("osty");
        return;
    }
    if (c.kind == CUE_SCRIPTED && c.data["call"].str() == "NCombatRoom.RadialBlur") {
        radial_blur(c.data["args"][(size_t)0].str());
        return;
    }
    if (c.kind == CUE_SCRIPTED && c.data["call"].str() == "NDoomVfx.Create" && c.data["pose"].is_array()) {
        /* DoomPower.StartDoomAnim before the effect takes the body: the Hit
         * trigger, and a hurt animation stopped HurtAnimationTrackOffsetForDoom
         * into it (SetTrackTime, SetTimeScale(0)) */
        const std::string& at = c.data["args"][(size_t)0].str();
        Creature* cr = creature_for(at.substr(0, at.find('@')));
        SpineSprite* s = cr ? first_spine(cr->visuals) : NULL;
        if (play_animator_ops(s, c.data["pose"])) {
            SpAnimationState* st = s->animation_state();
            const SpAnimation* now = st ? st->current(0) : NULL;
            if (now && now->name == "hurt") {
                st->set_track_time(0, c.data["hurt_offset"].f(0.1f));
                st->set_track_speed(0, 0);
            }
        }
    }
    if (c.kind == CUE_SCRIPTED) {
        if (!spawn_combat_vfx(lib_, vfx_box_, c.data["call"].str(), c.data["args"], *this)) CueLedger::retract(c.data);
        return;
    }
    if (!vfx_box_) return;
    Vec2 at;
    if (!vfx_position(c, &at)) return;
    if (c.kind == CUE_VFX) {
        Node* fx = instance(lib_, "scenes/" + c.path + ".tscn");
        if (!fx) return;
        if (fx->is_node2d()) ((Node2D*)fx)->position = at;
        else if (fx->is_control()) ((Control*)fx)->set_position(at);
        vfx_box_->add_child(fx);
        return;
    }
    /* CUE_DAMAGE: NDamageNumVfx.Create(target): 100 px above the centre,
     * +-10 x +-5 px. */
    if (c.where == "viewport") {
        Node* num = make_damage_number(lib_, Vec2(480, 540), c.amount);
        if (num) vfx_box_->add_child(num);
        return;
    }
    float rx = (float)(chaotic_rand() % 2001) / 1000.0f - 1.0f, ry = (float)(chaotic_rand() % 2001) / 1000.0f - 1.0f;
    Node* num = make_damage_number(lib_, at + Vec2(rx * 10.0f, -100.0f + ry * 5.0f), c.amount);
    if (num) vfx_box_->add_child(num);
}

void CombatScreen::add_cue(const Cue& c) {
    /* Kept in time order (a banner may come after cues already added). */
    std::vector<Cue>::iterator it = cues_.end();
    while (it != cues_.begin() && (it - 1)->at > c.at) --it;
    cues_.insert(it, c);
}

/* NPlayerTurnBanner / NEnemyTurnBanner / NCombatStartBanner (the scenes in
 * scenes/combat), over everything. */
void CombatScreen::show_banner(const std::string& kind, int round) {
    /* NCombatStartBanner: Rng.Chaotic.NextItem(TmpSfx.BattleStart) */
    if (lib_->audio()) lib_->audio()->ui(kind == "start" ? format("battle_start_%d.mp3", (int)(chaotic_rand() % 2) + 1)
                                         : kind == "enemy" ? "enemy_turn.mp3" : "player_turn.mp3");
    if (!root_) return;
    std::string path = "scenes/combat/" + kind + "_banner.tscn";
    if (kind == "start") path = "scenes/combat/combat_start_banner.tscn";
    if (kind == "player") path = "scenes/combat/player_turn_banner.tscn";
    if (kind == "enemy") path = "scenes/combat/enemy_turn_banner.tscn";
    Node* n = instance(lib_, path);
    if (!n || !n->is_control()) {
        delete n;
        return;
    }
    Control* b = (Control*)n;
    Control* label = as_ctl(n->get_node("Label"));
    double life = 2.5;
    if (kind == "player") {
        /* Display: in (1 s), the title up and the turn number down 50 px
         * (1.5 s), 0.4 s, out (0.3 s). */
        set_label(label, Loc::get("gameplay_ui", "PLAYER_TURN"));
        Control* turn = as_ctl(n->get_node("TurnNumber"));
        std::string count = Loc::get("gameplay_ui", "TURN_COUNT");
        size_t p = count.find("{turnNumber}");
        if (p != std::string::npos) count.replace(p, 12, format("%d", round));
        set_label(turn, count);
        b->modulate.a = 0;
        tweens_.tween(&b->modulate.a, 1.0f, 1.0f, TRANS_EXPO, EASE_OUT);
        if (label) {
            tweens_.tween(&label->offset[SIDE_TOP], label->offset[SIDE_TOP] - 50, 1.5f, TRANS_EXPO, EASE_OUT);
            tweens_.tween(&label->offset[SIDE_BOTTOM], label->offset[SIDE_BOTTOM] - 50, 1.5f, TRANS_EXPO, EASE_OUT);
        }
        if (turn) {
            tweens_.tween(&turn->offset[SIDE_TOP], turn->offset[SIDE_TOP] + 50, 1.5f, TRANS_EXPO, EASE_OUT);
            tweens_.tween(&turn->offset[SIDE_BOTTOM], turn->offset[SIDE_BOTTOM] + 50, 1.5f, TRANS_EXPO, EASE_OUT);
        }
        tweens_.tween(&b->modulate.a, 0.0f, 0.3f, TRANS_SINE, EASE_OUT, 1.9f);
        life = 2.2;
    } else if (kind == "enemy") {
        /* The title from 2x (0.75 s) fading in (1.3 s), then red while
         * everything fades (1 s). */
        set_label(label, Loc::get("gameplay_ui", "ENEMY_TURN"));
        b->modulate.a = 0;
        tweens_.tween(&b->modulate.a, 1.0f, 1.3f, TRANS_EXPO, EASE_OUT);
        if (label) {
            tweens_.tween_from(&label->scale, Vec2(2, 2), Vec2(1, 1), 0.75f, TRANS_EXPO, EASE_OUT);
            tweens_.tween(&label->modulate.g, 0.0f, 1.0f, TRANS_EXPO, EASE_OUT, 1.3f);
            tweens_.tween(&label->modulate.b, 0.0f, 1.0f, TRANS_EXPO, EASE_OUT, 1.3f);
        }
        tweens_.tween(&b->modulate.a, 0.0f, 1.0f, TRANS_CUBIC, EASE_OUT, 1.3f);
        life = 2.3;
    } else {
        /* "Battle Start": the band and the title in after 0.3 s, the title
         * out after 1.6 s; the player's turn banner at 2.6 s; the band out
         * from 4.1 s. */
        set_label(label, Loc::get("gameplay_ui", "BATTLE_START"));
        Control* band = as_ctl(n->get_node("ColorRect"));
        if (band) {
            band->modulate.a = 0;
            tweens_.tween(&band->modulate.a, 0.5f, 0.75f, TRANS_EXPO, EASE_OUT, 0.3f);
            tweens_.tween_from(&band->scale, Vec2(1.2f, 1.2f), Vec2(1, 1), 0.75f, TRANS_EXPO, EASE_OUT, 0.3f);
            tweens_.tween(&band->modulate.a, 0.0f, 1.0f, TRANS_CUBIC, EASE_OUT, 4.1f);
        }
        if (label) {
            label->modulate.a = 0;
            tweens_.tween(&label->modulate.a, 1.0f, 1.3f, TRANS_EXPO, EASE_OUT, 0.3f);
            tweens_.tween_from(&label->scale, Vec2(2, 2), Vec2(1, 1), 0.75f, TRANS_EXPO, EASE_OUT, 0.3f);
            tweens_.tween(&label->modulate.a, 0.0f, 1.0f, TRANS_CUBIC, EASE_OUT, 1.6f);
        }
        life = 5.1;
    }
    root_->add_child(n);
    Banner bn = { b, life_t_ + life + 0.05 };
    banners_.push_back(bn);
}

bool CombatScreen::cue_before(const Cue& a, const Cue& b) { return a.at < b.at; }

void CombatScreen::show_start_banner(bool turn_banner) {
    /* NCombatStartBanner: the player's turn banner after its 2.6 s; the
     * turn itself (the pad) starts with the logic's steps (CombatManager:
     * CustomScaledWait(0.5, 1) after the banner, then StartTurn). */
    Cue c;
    c.kind = CUE_BANNER;
    c.amount = 1;
    c.at = cue_t_;
    c.name = "start";
    add_cue(c);
    if (!turn_banner) return;
    c.at = cue_t_ + 2.6;
    c.name = "player";
    add_cue(c);
}

/* The steps as a timeline. CreatureCmd.TriggerAnim's pause precedes its
 * impact; VFX, damage numbers and state changes at that impact are parallel.
 * Cmd.Wait supplies the remaining pacing without an extra per-step delay. */
void CombatScreen::play_steps(const Json& steps, const Json& sync_to, bool finishing) {
    double t = 0;
    if (next_cue_ < cues_.size()) {
        /* NCardPlayQueue: after the actions still playing (not after the
         * tasks running alongside them) */
        cues_.erase(cues_.begin(), cues_.begin() + (long)next_cue_);
        next_cue_ = 0;
        t = cue_t_;
        for (size_t i = 0; i < cues_.size(); ++i)
            if (!cues_[i].delayed && cues_[i].at > t) t = cues_[i].at;
    } else {
        cues_.clear();
        next_cue_ = 0;
        cue_t_ = 0;
        hand_locked_until_ = 0;
    }
    const double base = t;
    /* The turn banners where the logic's cues put them (CombatManager:
     * NEnemyTurnBanner.Create / NPlayerTurnBanner.Create, each followed by
     * its CustomScaledWait); a trace without those cues: the enemies' turn
     * came first (its banner, then their actions), the player's turn banner
     * at the new round. */
    bool cued_banners = false;
    for (size_t i = 0; i < steps.size() && !cued_banners; ++i)
        for (size_t k = 0; k < steps[i].size(); ++k)
            if (steps[i][k]["t"].str() == "ui" && (steps[i][k]["call"].str() == "NEnemyTurnBanner.Create" ||
                                                   steps[i][k]["call"].str() == "NPlayerTurnBanner.Create"))
                cued_banners = true;
    bool new_round = false;
    for (size_t i = 0; i < steps.size() && !new_round && !cued_banners; ++i)
        for (size_t k = 0; k < steps[i].size(); ++k)
            if (steps[i][k]["t"].str() == "round") new_round = true;
    if (new_round) {
        Cue b;
        b.kind = CUE_BANNER;
        b.name = "enemy";
        b.amount = 0;
        b.at = base;
        cues_.push_back(b);
        t = base + 0.9;
    }
    for (size_t i = 0; i < steps.size(); ++i) {
        const Json& step = steps[i];
        for (size_t k = 0; k < step.size() && !cued_banners; ++k)
            if (step[k]["t"].str() == "round") {
                Cue b;
                b.kind = CUE_BANNER;
                b.name = "player";
                b.amount = step[k]["to"].i();
                b.at = t + 0.3;
                cues_.push_back(b);
                t += 0.3;
                if (b.at > hand_locked_until_) hand_locked_until_ = b.at;
            }
        for (size_t k = 0; k < step.size(); ++k) {
            const Json& ch = step[k];
            const std::string& type = ch["t"].str();
            Cue c;
            c.at = t;
            c.amount = 0;
            /* a presentation task the game runs alongside (the logic's
             * branches): so long after this point, the rest not waiting */
            double delay = ch["delay"].num(0) / 1000.0;
            if (delay > 0) {
                c.at = t + delay;
                c.delayed = true;
            }
            if (type == "ui" && ch["call"].str() == "NRollingBoulderVfx.Create") {
                /* RollingBoulderPower: the boulder's path times the damage
                 * that follows each boulder_hit mark (HitCreature) */
                std::vector<VfxCreature> targets;
                std::vector<std::string> who;
                const Json& list = ch["args"][(size_t)0];
                for (size_t i = 0; i < list.size(); ++i) {
                    VfxCreature vc;
                    if (creature(list[i].str(), &vc)) {
                        targets.push_back(vc);
                        who.push_back(list[i].str());
                    }
                }
                std::vector<double> hits;
                rolling_boulder_times((float)ch["args"][1].num(0), targets, &hits, &boulder_done_);
                boulder_t0_ = t;
                boulder_hits_.clear();
                for (size_t i = 0; i < who.size(); ++i) boulder_hits_[who[i]] = hits[i];
            }
            if (type == "boulder_hit" || type == "boulder_done") {
                std::map<std::string, double>::iterator h = boulder_hits_.find(ch["who"].str());
                double at = boulder_t0_ + (type == "boulder_done" ? boulder_done_ : h != boulder_hits_.end() ? h->second : 0);
                if (at > t) t = at;
                CueLedger::used(ch);
                continue;
            }
            if (type == "ui" && (ch["call"].str() == "NEnemyTurnBanner.Create" || ch["call"].str() == "NPlayerTurnBanner.Create")) {
                c.kind = CUE_BANNER;
                c.name = ch["call"].str() == "NEnemyTurnBanner.Create" ? "enemy" : "player";
                c.amount = ch["args"].size() > 0 ? ch["args"][(size_t)0].i() : 0;
                cues_.push_back(c);
                CueLedger::used(ch);
                /* CombatManager.StartTurn: player actions allowed again */
                if (c.name == "player" && t > hand_locked_until_) hand_locked_until_ = t;
            } else if (type == "sfx" || (type == "ui" && (ch["call"].str().find("Audio") != std::string::npos ||
                                                    ch["call"].str().find("Music") != std::string::npos))) {
                c.kind = CUE_AUDIO; c.data = ch; cues_.push_back(c);
            } else if (type == "anim") {
                c.kind = CUE_ANIM;
                c.who = ch["who"].str();
                c.name = ch["name"].str();
                c.data = ch;
                if (!creature_for(c.who)) { CueLedger::dropped(ch); continue; }
                cues_.push_back(c);
                CueLedger::used(ch);
                double w = ch["wait"].num(0);
                if (w > 0) t += w;
            } else if (type == "vfx") {
                c.kind = CUE_VFX;
                c.path = ch["path"].str();
                if (ch["side"].is_string()) {
                    c.where = "side";
                    c.who = ch["side"].str();
                } else {
                    c.who = ch["who"].str();
                    c.where = ch["at"].str();
                }
                cues_.push_back(c);
                CueLedger::used(ch);
            } else if (type == "ui" && (is_combat_vfx_call(ch["call"].str()) || ch["call"].str() == "NCombatRoom.RadialBlur" ||
                                        ch["call"].str() == "NCreature.ScaleTo" ||
                                        ch["call"].str() == "NCreature.TrackBlockStatus" ||
                                        ch["call"].str() == "NCombatRoom.ShakeOstyIfDead" ||
                                        ch["call"].str() == "NMonsterDeathVfx.Create" ||
                                        ch["call"].str() == "NCreature.SetDefaultScaleTo" ||
                                        ch["call"].str() == "NCreature.OstyScaleToSize" ||
                                        ch["call"].str() == "NCreature.StartReviveAnim" ||
                                        ch["call"].str() == "NCreature.PerformIntent" ||
                                        ch["call"].str().compare(0, 19, "MegaAnimationState.") == 0 ||
                                        ch["call"].str().compare(0, 21, "SpineAnimationAccess.") == 0 ||
                                        ch["call"].str().compare(0, 8, "Sandpit.") == 0 ||
                                        ch["call"].str() == "NCreature.FaceDirection" ||
                                        ch["call"].str() == "NCreature.FadeIn" ||
                                        ch["call"].str() == "NCreature.HideVisuals" ||
                                        ch["call"].str() == "NCreature.ShowVisuals" ||
                                        ch["call"].str() == "NCreature.AnimDisableUi" ||
                                        ch["call"].str() == "NCreature.AnimHideIntent" ||
                                        ch["call"].str() == "NCreature.RefreshIntents" ||
                                        ch["call"].str() == "NCreature.UpdateIntent" ||
                                        ch["call"].str() == "NCreature.ToggleIsInteractable" ||
                                        ch["call"].str() == "NCombatRoom.SetCreatureIsInteractable" ||
                                        ch["call"].str().compare(0, 12, "NGame.Screen") == 0 ||
                                        ch["call"].str() == "NGame.DoHitStop" ||
                                        ch["call"].str().find("FormVfx.Create") != std::string::npos)) {
                /* The game's scripted effects (combat_vfx). */
                c.kind = CUE_SCRIPTED;
                c.data = ch;
                cues_.push_back(c);
            } else if (type == "ui" && ch["args"].size() > 0 && ch["args"][(size_t)0].is_string() &&
                       ch["args"][(size_t)0].str().find('#') != std::string::npos) {
                /* a call on a creature's special node */
                c.kind = CUE_SCRIPTED;
                c.data = ch;
                cues_.push_back(c);
            } else if (type == "ui" && ch["call"].str().compare(0, 26, "NKaiserCrabBossBackground.") == 0) {
                c.kind = CUE_SCRIPTED;
                c.data = ch;
                cues_.push_back(c);
                t += kaiser_crab_wait(ch["call"].str(), ch["args"]);
            } else if (type == "ui" && ch["call"].str() == "NDamageNumVfx.Create" && ch["args"].size() >= 2 &&
                       ch["args"][1].is_number() && ch["args"][1].i() > 0) {
                /* The number the game shows (unblocked damage + overkill). */
                c.kind = CUE_DAMAGE;
                c.who = ch["args"][(size_t)0].str();
                c.where = "center";
                c.amount = ch["args"][1].i();
                if (!creature_for(c.who)) { CueLedger::dropped(ch); continue; }
                /* requireInteractable (default true): an event's room has no
                 * interactable creatures, so only the local player's number,
                 * at viewport * (0.25, 0.5) */
                if (visual_only_ && !(ch["args"].size() > 2 && ch["args"][2].is_bool() && !ch["args"][2].b())) {
                    if (c.who != "player") { CueLedger::dropped(ch); continue; }
                    c.where = "viewport";
                }
                cues_.push_back(c);
                CueLedger::used(ch);
            } else if (type == "despawn" && escaped_.count(ch["who"].str())) {
                CueLedger::used(ch);
                continue;
            } else if (type == "death" || type == "despawn") {
                c.kind = CUE_ANIM;
                c.who = ch["who"].str();
                c.name = "Dead";
                c.data = ch; /* "play": the animator's ops for the Dead trigger */
                if (!creature_for(c.who)) { CueLedger::dropped(ch); continue; }
                CueLedger::used(ch);
                bool already = false;
                for (size_t q = 0; q < cues_.size(); ++q)
                    if (cues_[q].kind == CUE_ANIM && cues_[q].who == c.who && cues_[q].name == "Dead") already = true;
                if (already) continue;
                cues_.push_back(c);
            } else if (type == "wait") {
                CueLedger::used(ch);
                double w = ch["ms"].num(0) / 1000.0;
                const std::string& src = ch["src"].str();
                if (src == "power_fly" && ch["scale"].is_number()) {
                    /* NCardFlyPowerVfx.GetDuration as this room measures it */
                    float d = power_fly_seconds();
                    if (d > 0) w = d * ch["scale"].num(1);
                } else if (src == "shuffle") {
                    /* CardPileCmd.Shuffle: the next card's NCardFlyShuffleVfx,
                     * then the wait before the one after */
                    static Json fly;
                    if (fly.is_null()) fly.set("t", Json::make_string("shuffle_fly"));
                    c.kind = CUE_STATE;
                    c.data = fly;
                    cues_.push_back(c);
                }
                if (w > 0) t += w;
            } else {
                c.kind = CUE_STATE;
                c.data = ch;
                cues_.push_back(c);
                if (type == "round" && t > hand_locked_until_) hand_locked_until_ = t;
                if (type == "escape") escaped_.insert(ch["who"].str());
            }
        }
    }
    if (!sync_to.is_null()) {
        Cue sync;
        sync.kind = CUE_SYNC;
        sync.at = t;
        sync.amount = finishing ? 1 : 0;
        sync.data = sync_to;
        cues_.push_back(sync);
    }
    std::stable_sort(cues_.begin(), cues_.end(), cue_before);
    /* The pad waits for the actions (not for the banners, nor for a
     * monster's death animation: the game goes on; the player's holds the
     * game over; nor for what runs alongside). */
    busy_until_ = 0;
    for (size_t i = 0; i < cues_.size(); ++i) {
        if (cues_[i].delayed) continue;
        double end = cues_[i].at + (cues_[i].kind == CUE_ANIM && cues_[i].name == "Dead" && cues_[i].who == "player" ? 1.5 : 0.4);
        if (cues_[i].kind != CUE_BANNER && cues_[i].kind != CUE_AUDIO && end > busy_until_) busy_until_ = end;
    }
}

/* ---- the cards' flights (NCardFlyVfx, NCardFlyPowerVfx, NCardTrailVfx) ---- */

static float lerp_angle(float from, float to, float k) {
    float d = fmodf(to - from, 2 * PI_F);
    d = fmodf(2 * d, 2 * PI_F) - d;
    return from + d * k;
}

static float chaotic_range(float lo, float hi) { return lo + (hi - lo) * (float)(chaotic_rand() % 100001) / 100000.0f; }

/* NCardTrailVfx._Process: at the card; FadeOut: stays, fades in 0.5 s. */
static void follow(Node* trail, Control* card) {
    if (!trail || !trail->is_node2d() || !trail->parent) return;
    Node2D* t = (Node2D*)trail;
    Vec2 g = card->global_transform().origin;
    Xform2D pg = trail->parent->is_canvas_item() ? ((CanvasItem*)trail->parent)->global_transform() : Xform2D();
    t->position = inverse(pg).xform(g);
    t->rotation = card->rotation;
}

/* NCardFlyVfx.Create / NCardFlyPowerVfx.Create: where the card goes (in the
 * hand's coordinates), its trail (the character's card_trail scene) and
 * its sound. */
void CombatScreen::start_flight(MovingCard& m, int kind, const std::string& pile) {
    m.kind = kind;
    m.phase = 0;
    m.ftime = 0;
    m.scale_from = m.holder.scale;
    m.scale_t = 0;
    Xform2D to_local = inverse(hand_box_->global_transform());
    Vec2 start = hand_box_->global_transform().xform(m.holder.pos);
    if (kind == 1) {
        m.arrive = pile == "draw" ? 0 : pile == "discard" ? 1 : -1;
        Vec2 end = pile_center(pile);
        if (end.x < 0) end = start;
        m.to = to_local.xform(end);
        float off = chaotic_range(100, 400);
        m.speed = chaotic_range(1.1f, 1.25f);
        m.accel = chaotic_range(2, 2.5f);
        float arc = end.y < 540.0f ? -500.0f : 500.0f + off;
        m.fdur = chaotic_range(1, 1.75f);
        Vec2 c = start + (end - start) * 0.5f;
        c.y -= arc;
        m.ctrl = to_local.xform(c);
        if (lib_->audio())
            lib_->audio()->ui(pile == "draw" ? "event:/sfx/ui/cards/card_movement_B_into_draw"
                              : m.holder.id == played_instance_ ? "event:/sfx/ui/cards/card_movement_B_play_into_discard"
                                                                : "event:/sfx/ui/cards/card_movement_B_into_discard");
    } else {
        /* the swoosh path, ending at the player's VfxSpawnPosition */
        m.path_scene = instance(lib_, "scenes/vfx/vfx_card_power_fly.tscn");
        Node* p = m.path_scene ? m.path_scene->get_node("SwooshPath") : NULL;
        VfxCreature pc;
        if (p && p->type == "Path2D" && creature("player", &pc)) {
            Path2D* path = (Path2D*)p;
            path->set_point_position(path->point_count() - 1, pc.center - start);
            m.fdur = path->baked_length() / 3000.0f;
        } else {
            m.fdur = 0.3f;
        }
        m.from = start; /* global: the path is relative to it */
        if (lib_->audio()) lib_->audio()->ui("event:/sfx/ui/cards/card_movement_B_power");
    }
    /* NCardTrailVfx._Ready: the sprites fade in and shrink, the card to 0.75 */
    std::string ch = lower(player_.data["character"].str());
    m.trail = ch.empty() ? NULL : instance(lib_, "scenes/vfx/card_trail_" + ch + ".tscn");
    if (m.trail && hand_box_->parent) {
        hand_box_->parent->add_child_before(m.trail, hand_box_);
        follow(m.trail, m.holder.node); /* its trails start at the card */
        Node* sprites = m.trail->get_node("Sprites");
        if (sprites && sprites->is_canvas_item()) {
            CanvasItem* sp = (CanvasItem*)sprites;
            sp->modulate.a = 0;
            tweens_.tween(&sp->modulate.a, 1.0f, 1.0f, TRANS_CUBIC, EASE_OUT);
            if (sp->is_node2d()) tweens_.tween(&((Node2D*)sp)->scale, Vec2(0.5f, 0.5f), 0.5f, TRANS_CUBIC, EASE_IN, 0.25f);
        }
        tweens_.tween(&m.holder.node->modulate.a, 0.75f, 0.5f, TRANS_CUBIC, EASE_OUT);
    } else if (m.trail) {
        delete m.trail;
        m.trail = NULL;
    }
}

static void stop_particles(Node* n) {
    if (n->type == "GPUParticles2D" || n->type == "CPUParticles2D") ((Particles2D*)n)->emitting = false;
    for (size_t i = 0; i < n->children.size(); ++i) stop_particles(n->children[i]);
}

bool CombatScreen::fly(MovingCard& m, double dt) {
    Control* card = m.holder.node;
    Node* body = m.holder.card_node ? find_named(m.holder.card_node, "CardContainer") : NULL;
    float k = (float)dt;
    bool done = false;
    if (m.kind == 1) {
        /* NCardFlyVfx.PlayAnim */
        if (m.phase == 0) {
            m.ftime += m.speed * k;
            m.speed += m.accel * k;
            float u = m.ftime / m.fdur;
            if (u > 1) {
                place(card, m.to.x, m.to.y, 0, 0);
                m.phase = 1;
                m.ftime = 0;
                if (m.arrive >= 0) {
                    int pile = m.arrive;
                    m.arrive = -1;
                    land_on_pile(pile);
                }
            } else {
                float u2 = (m.ftime + 0.05f) / m.fdur;
                Vec2 p = m.from * ((1 - u) * (1 - u)) + m.ctrl * (2 * (1 - u) * u) + m.to * (u * u);
                Vec2 q = m.from * ((1 - u2) * (1 - u2)) + m.ctrl * (2 * (1 - u2) * u2) + m.to * (u2 * u2);
                place(card, p.x, p.y, 0, 0);
                float ang = atan2f(q.y - p.y, q.x - p.x) + PI_F / 2;
                card->rotation = lerp_angle(card->rotation, ang, minf(k * 12, 1));
                float d = clampf(m.ftime * 3 / m.fdur, 0, 1);
                if (body && body->is_control()) {
                    Control* b = (Control*)body;
                    b->modulate = Color(1 - d, 1 - d, 1 - d, b->modulate.a);
                    float s = 1 + (0.1f - 1) * d;
                    b->scale = Vec2(s, s);
                }
            }
        } else {
            m.ftime += m.speed * k;
            float u = m.ftime / m.fdur;
            if (u > 0.25f && m.trail) {
                stop_particles(m.trail);
                Trail t = { m.trail, 0 };
                fading_trails_.push_back(t);
                m.faded_trail = m.trail;
                m.trail = NULL;
            }
            if (body && body->is_control()) {
                float s = maxf(0.1f + (-0.15f - 0.1f) * u, 0);
                ((Control*)body)->scale = Vec2(s, s);
            }
            if (u > 1) done = true;
        }
    } else {
        /* NCardFlyPowerVfx.PlayAnim: along the path at 3000 px/s (Quad In),
         * turning towards it, to 0.1 in 0.3 s, then to nothing from 90% */
        m.ftime += k;
        float u = m.fdur > 0 ? m.ftime / m.fdur : 1.0f;
        Node* p = m.path_scene ? m.path_scene->get_node("SwooshPath") : NULL;
        if (p && p->type == "Path2D") {
            Path2D* path = (Path2D*)p;
            float rot = 0;
            Vec2 at = path->sample_baked(minf(u, 1) * minf(u, 1) * path->baked_length(), &rot);
            Vec2 g = m.from + at;
            Vec2 l = inverse(hand_box_->global_transform()).xform(g);
            place(card, l.x, l.y, 0, 0);
            float s = rot - card->rotation;
            float spin = PI_F + (PI_F * 50 - PI_F) * minf(u, 1);
            float step = minf(fabsf(s), spin * k);
            card->rotation += s < 0 ? -step : step;
        }
        if (u < 0.9f) {
            m.scale_t += k;
            float s = m.scale_from + (0.1f - m.scale_from) * minf(m.scale_t / 0.3f, 1);
            card->scale = Vec2(s, s);
            m.scale_from = m.scale_t >= 0.3f ? 0.1f : m.scale_from;
        } else {
            float rest = maxf(m.fdur * 0.1f, 0.001f);
            float s = 0.1f * maxf(1 - (u - 0.9f) * m.fdur / rest, 0);
            card->scale = Vec2(s, s);
        }
        if (u >= 1) {
            ScreenShake::get().shake("Medium", "Short");
            if (m.trail) {
                stop_particles(m.trail);
                Trail t = { m.trail, 0 };
                fading_trails_.push_back(t);
                m.trail = NULL;
            }
            done = true;
        }
    }
    if (m.trail) follow(m.trail, card);
    if (done) {
        /* NCardFlyVfx.OnCardExitedTree: its trail goes with the card */
        for (size_t i = 0; m.faded_trail && i < fading_trails_.size(); ++i)
            if (fading_trails_[i].node == m.faded_trail) {
                if (m.faded_trail->parent) m.faded_trail->parent->remove_child(m.faded_trail);
                delete m.faded_trail;
                fading_trails_.erase(fading_trails_.begin() + (long)i);
                break;
            }
        m.faded_trail = NULL;
        if (card->parent) card->parent->remove_child(card);
        delete card;
        delete m.path_scene;
        m.path_scene = NULL;
        return false;
    }
    return true;
}

/* ---- relic and power flashes ---- */

/* NRelicFlashVfx.StartVfx: from 64 px below the top of the hitbox up to it
 * (1 s, Expo Out); its three icons appear 0.2 s apart, each growing to 1.25
 * (1 s, Cubic Out) while it fades out over 1.5 s. */
class RelicFlashAnim : public ScriptBehavior {
public:
    explicit RelicFlashAnim(float y) : t_(0), y_(y) {}
    virtual void process(Node* n, double dt) {
        t_ += (float)dt;
        if (n->is_control()) {
            float k = minf(t_ / 1.0f, 1);
            float e = k >= 1 ? 1 : 1 - powf(2, -10 * k);
            Control* c = (Control*)n;
            float h = c->offset[SIDE_BOTTOM] - c->offset[SIDE_TOP];
            c->offset[SIDE_TOP] = y_ + 64 - 64 * e;
            c->offset[SIDE_BOTTOM] = c->offset[SIDE_TOP] + h;
        }
        for (int i = 0; i < 3; ++i) {
            Node* img = n->get_node(format("Image%d", i + 1));
            if (!img || !img->is_control()) continue;
            Control* c = (Control*)img;
            float lt = t_ - 0.2f * (float)i;
            if (lt < 0) {
                c->modulate.a = 0;
                continue;
            }
            float g = minf(lt / 1.0f, 1), u = 1 - g;
            float s = 1 + 0.25f * (1 - u * u * u);
            c->scale = Vec2(s, s);
            c->modulate.a = maxf(1 - lt / 1.5f, 0);
        }
        if (t_ >= 1.95f && !n->queued_for_deletion()) n->queue_free();
    }

private:
    float t_, y_;
};

/* NPowerFlashVfx.StartVfx: the big icon grows to 0.4 (0.4 s) and fades in
 * (0.25 s, Sine Out), then to 0.45 (0.4 s), then out (0.25 s, Sine In). */
class PowerFlashAnim : public ScriptBehavior {
public:
    PowerFlashAnim(Node2D* sprite) : t_(0), sprite_(sprite), from_(sprite ? sprite->scale.x : 0) {}
    virtual void process(Node* n, double dt) {
        t_ += (float)dt;
        if (sprite_) {
            float s, a;
            if (t_ < 0.4f) {
                s = from_ + (0.4f - from_) * (t_ / 0.4f);
                a = 1;
            } else if (t_ < 0.8f) {
                s = 0.4f + 0.05f * ((t_ - 0.4f) / 0.4f);
                a = 1;
            } else {
                s = 0.45f;
                float k = minf((t_ - 0.8f) / 0.25f, 1);
                a = cosf(k * 1.5707963f);
            }
            sprite_->scale = Vec2(s, s);
            sprite_->modulate.a = a;
        }
        if (t_ >= 1.05f && !n->queued_for_deletion()) n->queue_free();
    }

private:
    float t_;
    Node2D* sprite_;
    float from_;
};

void CombatScreen::flash_relic(const Json& cue) {
    if (!root_) return;
    const Json& relics = state_["player"]["relics"];
    int index = cue["index"].i(-1);
    if (index < 0 || index >= (int)relics.size()) return;
    std::string base = relic_icon_base(relics[(size_t)index]);
    Texture2DRes* icon = NULL;
    /* NRelicInventoryHolder.DoFlash: the flash particles of the relic's icon
     * over its holder in the top bar */
    Node* bar = root_->get_node("TopBarLayer");
    Node* row = bar ? find_named(bar, "RelicInventory") : NULL;
    Node* holder = row && index < (int)row->children.size() ? row->children[(size_t)index] : NULL;
    if (holder && holder->is_control()) {
        Node* hi = holder_relic_icon(holder);
        if (hi && hi->type == "TextureRect") icon = ((TextureRect*)hi)->texture;
        Node* fx = make_relic_holder_flash(lib_, (Control*)holder);
        if (fx) root_->add_child(fx);
    }
    /* Player.OnRelicFlashed: NRelicFlashVfx at each target's top of hitbox */
    const Json& targets = cue["targets"];
    for (size_t i = 0; i < targets.size(); ++i) {
        VfxCreature vc;
        if (!creature(targets[i].str(), &vc)) continue;
        Node* fx = instance(lib_, "scenes/vfx/relic_flash_vfx.tscn");
        if (!fx || !fx->is_control()) {
            delete fx;
            continue;
        }
        for (int k = 1; k <= 3; ++k) {
            Node* img = fx->get_node(format("Image%d", k));
            if (!img) continue;
            if (icon && img->type == "TextureRect") ((TextureRect*)img)->texture = icon;
            else set_relic_icon(lib_, img, base);
            if (img->is_canvas_item()) ((CanvasItem*)img)->modulate.a = 0;
        }
        Control* c = (Control*)fx;
        float w = c->offset[SIDE_RIGHT] - c->offset[SIDE_LEFT], h = c->offset[SIDE_BOTTOM] - c->offset[SIDE_TOP];
        for (int s2 = 0; s2 < 4; ++s2) c->anchor[s2] = 0;
        float y = vc.top.y + c->offset[SIDE_TOP];
        place(c, vc.top.x + c->offset[SIDE_LEFT], y + 64, w, h);
        fx->behavior = new RelicFlashAnim(y);
        root_->add_child(fx);
    }
}

void CombatScreen::flash_power(const Json& cue) {
    VfxCreature vc;
    if (!root_ || !creature(cue["who"].str(), &vc)) return;
    /* PowerModel.BigIcon: images/powers/<id>.png, else the beta one */
    std::string id = lower(cue["power"].str());
    Resource* r = lib_->ext("images/powers/" + id + ".png");
    if (!r || !r->is_texture()) r = lib_->ext("images/powers/beta/" + id + ".png");
    if (!r || !r->is_texture()) return;
    Node* fx = instance(lib_, "scenes/vfx/power_flash_vfx.tscn");
    Node* sp = fx ? fx->get_node("Sprite2D") : NULL;
    if (!fx || !fx->is_node2d() || !sp || sp->type != "Sprite2D") {
        delete fx;
        return;
    }
    ((Sprite2D*)sp)->texture = (Texture2DRes*)r;
    ((Node2D*)fx)->position = vc.center;
    fx->behavior = new PowerFlashAnim((Node2D*)sp);
    root_->add_child(fx);
}

/* ---- NCombatRoom's creature navigation ---- */

/* The creatures a pad can focus, left to right (UpdateCreatureNavigation:
 * the interactable ones by GlobalPosition.X). */
void CombatScreen::creature_order(std::vector<Creature*>& out) {
    out.clear();
    if (player_.node && player_.node->visible) out.push_back(&player_);
    if (osty_.node && osty_.node->visible && osty_.data["hp"].i(1) > 0) out.push_back(&osty_);
    for (size_t i = 0; i < enemies_.size(); ++i)
        if (enemies_[i].node && enemies_[i].node->visible && enemies_[i].data["hp"].i(1) > 0) out.push_back(&enemies_[i]);
    for (size_t i = 1; i < out.size(); ++i)
        for (size_t j = i; j > 0 && out[j]->node->global_transform().origin.x < out[j - 1]->node->global_transform().origin.x; --j) {
            Creature* t = out[j];
            out[j] = out[j - 1];
            out[j - 1] = t;
        }
}

CombatScreen::Creature* CombatScreen::creature_of(Control* node) {
    if (!node) return NULL;
    if (player_.node == node) return &player_;
    if (osty_.node == node) return &osty_;
    for (size_t i = 0; i < enemies_.size(); ++i)
        if (enemies_[i].node == node) return &enemies_[i];
    return NULL;
}

void CombatScreen::show_creature_focus(bool on) {
    Creature* c = creature_of(creature_focus_);
    if (creature_tips_) {
        if (creature_tips_->parent) creature_tips_->parent->remove_child(creature_tips_);
        delete creature_tips_;
        creature_tips_ = NULL;
    }
    if (!c) return;
    show_nameplate(*c, on);
    select_reticle(*c, on);
    if (on) show_creature_tips(*c);
}

/* NCreature.ShowHoverTips; again as the focused creature's powers change
 * (NCreature.OnPowerApplied / Removed refresh them): at each sync */
void CombatScreen::show_creature_tips(Creature& c) {
    if (creature_tips_) {
        if (creature_tips_->parent) creature_tips_->parent->remove_child(creature_tips_);
        delete creature_tips_;
        creature_tips_ = NULL;
    }
    if (!root_) return;
    const Json& tips = c.player ? state_["player_tips"] : c.data["tips"];
    Node* t = make_tip_set(lib_, tips, true);
    creature_tips_ = t && t->is_control() ? (Control*)t : NULL;
    if (t && !creature_tips_) delete t;
    if (!creature_tips_) return;
    creature_tips_->z_index = 10;
    creature_tips_->modulate.a = 0; /* placed on the next frame, after the layout */
    root_->add_child(creature_tips_);
}

void CombatScreen::focus_creatures() {
    std::vector<Creature*> order;
    creature_order(order);
    if (order.empty()) return;
    unfocus_creatures();
    creature_focus_ = order[0]->node;
    show_creature_focus(true);
    layout_hand();
    refresh_tips();
}

void CombatScreen::move_creature_focus(int step) {
    std::vector<Creature*> order;
    creature_order(order);
    if (order.empty()) {
        unfocus_creatures();
        return;
    }
    int at = 0;
    for (size_t i = 0; i < order.size(); ++i)
        if (order[i]->node == creature_focus_) at = (int)i;
    int n = (int)order.size();
    show_creature_focus(false);
    creature_focus_ = order[(size_t)((at + step + n) % n)]->node;
    show_creature_focus(true);
}

void CombatScreen::unfocus_creatures() {
    if (!creature_focus_) return;
    show_creature_focus(false);
    creature_focus_ = NULL;
    layout_hand();
    tips_for_ = -2;
    refresh_tips();
}

/* ---- NPlayerHand's selection mode ---- */

void CombatScreen::begin_hand_select(const Json& d) {
    end_hand_select();
    end_play();
    Node* ui = root_ ? find_named(root_, "CombatUi") : NULL;
    Node* hand = ui ? find_named(ui, "Hand") : NULL;
    if (!hand || !hand_box_) return;
    sel_ = d;
    sel_mode_ = d["kind"].str() == "hand_upgrade" ? 2 : 1;
    /* NCombatUi.OnHandSelectModeEntered: the hand (its backstop and the
     * picked cards) in front of the rest, the card being played included */
    if (hand->parent) {
        sel_hand_index_ = hand->index();
        hand->parent->add_child(hand);
    }
    sel_layers(true);
    sel_row_ = false;
    sel_focus_ = 0;
    sel_box_ = ctl(hand, "SelectedHandCardContainer");
    if (!sel_box_) {
        Node* n = hand->find_unique("SelectedHandCardContainer");
        sel_box_ = n && n->is_control() ? (Control*)n : NULL;
    }
    /* the backstop fades in (0.2 s) */
    Node* back = hand->find_unique("SelectModeBackstop");
    if (back && back->is_canvas_item()) {
        CanvasItem* b = (CanvasItem*)back;
        b->visible = true;
        b->self_modulate.a = 0;
        tweens_.tween(&b->self_modulate.a, 1.0f, 0.2f);
    }
    Node* header = hand->find_unique("SelectionHeader");
    set_label(header, "[center]" + d["prompt"].str() + "[/center]");
    set_visible(header, true);
    /* NConfirmButton: at the corner when enabled, out of the screen when not */
    Node* cb = hand->find_unique("SelectModeConfirmButton");
    sel_confirm_ = cb && cb->is_control() ? (Control*)cb : NULL;
    if (sel_confirm_) {
        Control* c = sel_confirm_;
        float w = c->offset[SIDE_RIGHT] - c->offset[SIDE_LEFT], h = c->offset[SIDE_BOTTOM] - c->offset[SIDE_TOP];
        /* from the scene's offsets, the first time only: later selections
         * find it moved */
        if (c != sel_confirm_placed_) {
            sel_confirm_placed_ = c;
            sel_confirm_x_ = 1920.0f - (c->offset[SIDE_RIGHT] + 120.0f);
            sel_confirm_y_ = 1080.0f - (-c->offset[SIDE_BOTTOM] + 110.0f);
        }
        for (int s2 = 0; s2 < 4; ++s2) c->anchor[s2] = 0;
        place(c, sel_confirm_x_ + 180.0f, sel_confirm_y_, w, h);
        c->visible = true;
        show_glyph(lib_, find_named(c, "HotkeyIcon"), "y");
        sel_confirm_shown_ = false;
    }
    /* PeekButton.Enable; its targets the selection's parts */
    peek_.setup(lib_, hand->find_unique("PeekButton"), true);
    static const char* const peek_targets[] = { "SelectModeBackstop", "UpgradePreviewContainer", "SelectModeConfirmButton",
                                                "SelectionHeader", "SelectedHandCardContainer" };
    for (size_t i = 0; i < sizeof(peek_targets) / sizeof(peek_targets[0]); ++i)
        peek_.add_target(hand->find_unique(peek_targets[i]));
    sel_filter();
    hand_focus_ = -1;
    sel_layout();
    layout_hand();
    refresh_tips();
}

/* The hand in front of NCombatUi.PlayContainer puts the cards being
 * played under the backstop. Here those cards are the hand container's
 * own (z 2 over the hand), so while selecting they keep z 2 by themselves
 * and the hand's selection parts rise over them. */
void CombatScreen::sel_layers(bool on) {
    Node* ui = root_ ? find_named(root_, "CombatUi") : NULL;
    Node* hand = ui ? find_named(ui, "Hand") : NULL;
    static const char* const parts[] = { "SelectModeBackstop",      "CardHolderContainer",     "SelectedHandCardContainer",
                                         "UpgradePreviewContainer", "SelectModeConfirmButton", "SelectionHeader",
                                         "PeekButton" };
    for (size_t i = 0; hand && i < sizeof(parts) / sizeof(parts[0]); ++i) {
        Node* n = hand->find_unique(parts[i]);
        if (!n) n = find_named(hand, parts[i]);
        if (n && n->is_canvas_item()) ((CanvasItem*)n)->z_index = on ? 3 : 0;
    }
    for (size_t i = 0; i < moving_cards_.size(); ++i)
        if (moving_cards_[i].kind == 3 || !on) moving_cards_[i].holder.node->z_as_relative = !on;
    for (size_t i = 0; !on && i < holders_.size(); ++i) holders_[i].node->z_as_relative = true;
}

/* UpdateSelectModeCardVisibility: only the cards the filter takes; all of
 * them while peeking */
void CombatScreen::sel_filter() {
    const Json& cs = sel_["cards"];
    for (size_t i = 0; i < holders_.size(); ++i) {
        bool ok = peek_.peeking();
        for (size_t k = 0; k < cs.size() && !ok; ++k)
            if (cs[k]["instance_id"].i(-1) == holders_[i].id) ok = true;
        holders_[i].node->visible = ok;
    }
}

void CombatScreen::end_hand_select() {
    if (!sel_mode_) return;
    /* PeekButton.Disable */
    peek_.set_peeking(false);
    /* OnSelectModeSourceFinished: the picked cards go back to the hand */
    while (!selected_.empty()) sel_unpick((int)selected_.size() - 1);
    for (size_t i = 0; i < holders_.size(); ++i) holders_[i].node->visible = true;
    Node* ui = root_ ? find_named(root_, "CombatUi") : NULL;
    Node* hand = ui ? find_named(ui, "Hand") : NULL;
    if (hand) {
        set_visible(hand->find_unique("SelectModeBackstop"), false);
        set_visible(hand->find_unique("SelectionHeader"), false);
        set_visible(hand->find_unique("UpgradePreviewContainer"), false);
        /* OnHandSelectModeExited: back to its place */
        Node* p = hand->parent;
        if (p && sel_hand_index_ >= 0 && sel_hand_index_ < (int)p->children.size() - 1)
            p->add_child_before(hand, p->children[sel_hand_index_]);
    }
    sel_hand_index_ = -1;
    sel_layers(false);
    peek_.setup(lib_, hand ? hand->find_unique("PeekButton") : NULL, false);
    if (sel_confirm_) sel_confirm_->visible = false;
    sel_mode_ = 0;
    sel_row_ = false;
    layout_hand();
    refresh_tips();
}

/* The upgrade preview (NUpgradePreview): the picked card before and after. */
static void fill_upgrade_preview(SceneLibrary* lib, Node* hand, const Json* card) {
    Node* box = hand ? hand->find_unique("UpgradePreviewContainer") : NULL;
    Node* before = box ? find_named(box, "Before") : NULL;
    Node* after = box ? find_named(box, "After") : NULL;
    Node* slots[2] = { before, after };
    for (int s = 0; s < 2; ++s)
        while (slots[s] && !slots[s]->children.empty()) {
            Node* c = slots[s]->children.back();
            slots[s]->remove_child(c);
            delete c;
        }
    set_visible(box, card != NULL);
    if (!card) return;
    Node* c1 = before ? make_card(lib, *card) : NULL;
    if (c1) before->add_child(c1);
    const Json& up = (*card)["upgrade_preview"];
    Node* c2 = after && up.is_object() ? make_card(lib, up) : NULL;
    if (c2) after->add_child(c2);
    set_visible(find_named(box, "Arrows"), c2 != NULL);
}

/* SelectCardInSimpleMode / SelectCardInUpgradeMode */
void CombatScreen::sel_pick(int i) {
    if (i < 0 || i >= (int)holders_.size() || !sel_box_) return;
    int max = sel_["max_select"].i(1);
    if (sel_mode_ == 2 || (int)selected_.size() >= max) {
        if (!selected_.empty()) sel_unpick((int)selected_.size() - 1);
        if (i >= (int)holders_.size()) i = (int)holders_.size() - 1;
    }
    Holder h = holders_[(size_t)i];
    Vec2 g = hand_box_->global_transform().xform(h.pos);
    holders_.erase(holders_.begin() + i);
    hand_box_->remove_child(h.node);
    sel_box_->add_child(h.node);
    h.pos = inverse(sel_box_->global_transform()).xform(g);
    h.to_angle = 0;
    selected_.push_back(h);
    if (sel_mode_ == 2) {
        /* the hand's card leaves; the preview shows it */
        h.node->visible = false;
        selected_.back().node->visible = false;
        const Json* card = NULL;
        const Json& cs = sel_["cards"];
        for (size_t k = 0; k < cs.size(); ++k)
            if (cs[k]["instance_id"].i(-1) == h.id) card = &cs[k];
        Node* ui = root_ ? find_named(root_, "CombatUi") : NULL;
        fill_upgrade_preview(lib_, ui ? find_named(ui, "Hand") : NULL, card);
    }
    sel_layout();
    layout_hand();
    tips_for_ = -2; /* the holders moved: the same index is another card */
    refresh_tips();
}

/* DeselectCard: back into the hand at its place */
void CombatScreen::sel_unpick(int k) {
    if (k < 0 || k >= (int)selected_.size()) return;
    Holder h = selected_[(size_t)k];
    selected_.erase(selected_.begin() + k);
    Vec2 g = sel_box_ ? sel_box_->global_transform().xform(h.pos) : h.pos;
    if (h.node->parent) h.node->parent->remove_child(h.node);
    hand_box_->add_child(h.node);
    h.pos = inverse(hand_box_->global_transform()).xform(g);
    h.node->visible = true;
    /* GetHandInsertIndex: the hand's order */
    int order = 0;
    for (size_t j = 0; j < hand_.size(); ++j)
        if (hand_[j]["instance_id"].i(-(int)j - 1) == h.id) order = (int)j;
    size_t at = holders_.size();
    for (size_t j = 0; j < holders_.size(); ++j) {
        int o = 0;
        for (size_t m = 0; m < hand_.size(); ++m)
            if (hand_[m]["instance_id"].i(-(int)m - 1) == holders_[j].id) o = (int)m;
        if (o > order) {
            at = j;
            break;
        }
    }
    holders_.insert(holders_.begin() + (long)at, h);
    if (sel_mode_ == 2) {
        Node* ui = root_ ? find_named(root_, "CombatUi") : NULL;
        fill_upgrade_preview(lib_, ui ? find_named(ui, "Hand") : NULL, NULL);
    }
    if (selected_.empty()) sel_row_ = false;
    if (sel_focus_ >= (int)selected_.size()) sel_focus_ = (int)selected_.size() - 1;
    if (sel_focus_ < 0) sel_focus_ = 0;
    sel_layout();
    layout_hand();
}

/* NSelectedHandCardContainer.RefreshHolderPositions (300 px apart at the
 * holders' small scale) and NPlayerHand.UpdateSelectedCardContainer (the
 * row rises and shrinks past 3 and 6 cards); the confirm button shown while
 * the count is within min..max (RefreshSelectModeConfirmButton). */
void CombatScreen::sel_layout() {
    int n = (int)selected_.size();
    for (int k = 0; k < n; ++k) {
        Holder& h = selected_[(size_t)k];
        h.to_pos = Vec2(-300.0f * (float)(n - 1) / 2.0f + 300.0f * (float)k, 0);
        h.to_angle = 0;
        h.to_scale = sel_row_ && k == sel_focus_ ? 0.9f : 0.8f;
        h.node->z_index = sel_row_ && k == sel_focus_ ? 1 : 0;
    }
    if (sel_box_) {
        float s = n > 6 ? 0.55f : (n > 3 ? 0.8f : 1.0f);
        float y = n > 6 ? 390.0f : (n > 3 ? 465.0f : 540.0f);
        tweens_.tween(&sel_box_->offset[SIDE_TOP], y - 540.0f, 0.5f, TRANS_QUAD, EASE_IN_OUT);
        tweens_.tween(&sel_box_->offset[SIDE_BOTTOM], y - 540.0f, 0.5f, TRANS_QUAD, EASE_IN_OUT);
        tweens_.tween(&sel_box_->scale, Vec2(s, s), 0.5f, TRANS_QUAD, EASE_IN_OUT);
    }
    bool ok = n >= sel_["min_select"].i(0) && n <= sel_["max_select"].i(1);
    if (sel_confirm_ && ok != sel_confirm_shown_) {
        sel_confirm_shown_ = ok;
        Control* c = sel_confirm_;
        float w = c->offset[SIDE_RIGHT] - c->offset[SIDE_LEFT];
        float x = ok ? sel_confirm_x_ : sel_confirm_x_ + 180.0f;
        TweenTrans t = ok ? TRANS_BACK : TRANS_EXPO;
        tweens_.tween(&c->offset[SIDE_LEFT], x, 0.35f, t, EASE_OUT);
        tweens_.tween(&c->offset[SIDE_RIGHT], x + w, 0.35f, t, EASE_OUT);
    }
}

std::string CombatScreen::hand_select_input(const PadState& pad) {
    if (!sel_mode_) return std::string();
    /* NPeekButton (the left stick): the fight without the selection, every
     * hand card shown (OnPeekButtonToggled) */
    if ((pad.pressed & PadState::LS) && peek_.enabled()) {
        peek_.toggle();
        unfocus_creatures();
        sel_row_ = false;
        hand_focus_ = -1;
        sel_filter();
        sel_layout();
        layout_hand();
        refresh_tips();
        return std::string();
    }
    if (peek_.peeking()) {
        /* the fight's navigation: the creatures (up) and the hand; a card
         * pressed wiggles the button (NPlayerHand.OnHolderPressed) */
        if (creatures_focused()) {
            if (pad.pressed & (PadState::LEFT | PadState::RIGHT)) move_creature_focus((pad.pressed & PadState::LEFT) ? -1 : 1);
            else if (pad.pressed & (PadState::DOWN | PadState::B)) unfocus_creatures();
            return std::string();
        }
        if (pad.pressed & PadState::UP) {
            focus_creatures();
            return std::string();
        }
        if (pad.pressed & PadState::A) {
            peek_.wiggle();
            return std::string();
        }
        if (!(pad.pressed & (PadState::LEFT | PadState::RIGHT))) return std::string();
    }
    int n = (int)selected_.size();
    if (pad.pressed & PadState::Y) {
        /* OnSelectModeConfirmButtonPressed */
        if (!sel_confirm_shown_) return std::string();
        std::string idx;
        const Json& cs = sel_["cards"];
        for (int k = 0; k < n; ++k)
            for (size_t j = 0; j < cs.size(); ++j)
                if (cs[j]["instance_id"].i(-1) == selected_[(size_t)k].id)
                    idx += (idx.empty() ? "" : ",") + format("%d", (int)j);
        return protocol_action("select_cards", "\"indices\":\"" + idx + "\"");
    }
    if ((pad.pressed & PadState::B) && sel_["cancelable"].b())
        return protocol_action("skip_select");
    if (sel_row_) {
        if (pad.pressed & (PadState::LEFT | PadState::RIGHT)) {
            sel_focus_ = (sel_focus_ + ((pad.pressed & PadState::LEFT) ? n - 1 : 1)) % (n > 0 ? n : 1);
            sel_layout();
        } else if (pad.pressed & PadState::DOWN) {
            sel_row_ = false;
            sel_layout();
            layout_hand();
            refresh_tips();
        } else if (pad.pressed & PadState::A) {
            sel_unpick(sel_focus_);
            tips_for_ = -2;
            refresh_tips();
        }
        return std::string();
    }
    int f = focused_holder();
    if (pad.pressed & (PadState::LEFT | PadState::RIGHT)) {
        /* the next visible card, wrapping */
        int m = (int)holders_.size();
        for (int s = 1; s <= m && f >= 0; ++s) {
            int j = (f + ((pad.pressed & PadState::LEFT) ? -s : s) + m * 2) % m;
            if (holders_[(size_t)j].node->visible) {
                hand_focus_ = j;
                break;
            }
        }
        layout_hand();
        refresh_tips();
    } else if ((pad.pressed & PadState::UP) && n > 0 && sel_mode_ == 1) {
        sel_row_ = true;
        sel_focus_ = n - 1;
        sel_layout();
        layout_hand();
        refresh_tips();
    } else if ((pad.pressed & PadState::A) && f >= 0) {
        sel_pick(f);
    }
    return std::string();
}

/* ---- the play pile (NCombatUi.PlayContainer) ---- */

/* PileType.Play.GetTargetPosition: the screen's middle, 100 px up (in the
 * hand's coordinates). */
Vec2 CombatScreen::play_pile_pos() const {
    Vec2 at(960, 440);
    return hand_box_ ? inverse(hand_box_->global_transform()).xform(at) : at;
}

CombatScreen::MovingCard* CombatScreen::in_play(int id) {
    for (size_t i = 0; i < moving_cards_.size(); ++i)
        if (moving_cards_[i].kind == 3 && moving_cards_[i].holder.id == id) return &moving_cards_[i];
    return NULL;
}

/* A card in the play pile: AppendPlayPileLerpTween (its place in the lerp
 * time, Cubic Out; upright; to 0.8 in 0.25 s), then NCard.AnimMultiCardPlay
 * when it plays again: to transparent black in 0.2 s, a pause (0.2 s, Fast
 * 0.1), back to white in 0.2 s while it grows from half its size and rises
 * from 250 px below (0.25 s, Back Out). */
void CombatScreen::update_in_play(MovingCard& m, double dt) {
    m.t += dt;
    float up = m.duration > 0 ? clampf((float)(m.t / m.duration), 0, 1) : 1;
    float us = clampf((float)(m.t / 0.25), 0, 1);
    Vec2 at = m.from + (m.to - m.from) * tween_interpolate(TRANS_CUBIC, EASE_OUT, up, 0, 1, 1);
    float sc = m.from_scale + (m.to_scale - m.from_scale) * tween_interpolate(TRANS_CUBIC, EASE_OUT, us, 0, 1, 1);
    Color mod(1, 1, 1, 1);
    float below = 0;
    if (m.mp_t >= 0) {
        /* PlayPileTween.FastForwardToCompletion */
        at = m.to;
        sc = m.to_scale;
        m.mp_t += dt;
        double pause = fast_mode() ? 0.1 : 0.2;
        if (m.mp_t < 0.2) {
            float k = 1 - (float)(m.mp_t / 0.2);
            mod = Color(k, k, k, k);
        } else if (m.mp_t < 0.2 + pause) {
            mod = Color(0, 0, 0, 0);
        } else {
            float b = (float)(m.mp_t - 0.2 - pause);
            float kc = tween_interpolate(TRANS_CUBIC, EASE_OUT, clampf(b / 0.2f, 0, 1), 0, 1, 1);
            mod = Color(kc, kc, kc, kc);
            sc = m.to_scale * (0.5f + 0.5f * tween_interpolate(TRANS_CUBIC, EASE_OUT, clampf(b / 0.25f, 0, 1), 0, 1, 1));
            below = 250.0f * (1 - tween_interpolate(TRANS_BACK, EASE_OUT, clampf(b / 0.25f, 0, 1), 0, 1, 1));
            if (b >= 0.25f) {
                m.mp_t = -1;
                mod = Color(1, 1, 1, 1);
            }
        }
    }
    m.holder.pos = at;
    m.holder.scale = sc;
    m.holder.node->z_as_relative = sel_mode_ == 0;
    place(m.holder.node, at.x, at.y + below, 0, 0);
    m.holder.node->rotation = 0;
    m.holder.node->scale = Vec2(sc, sc);
    m.holder.node->modulate = mod;
}

/* Cards left in the play pile when the enemies' turn starts (a card the
 * logic took out of combat): NCardExhaustVfx. */
void CombatScreen::burn_play_pile() {
    for (size_t i = moving_cards_.size(); i-- > 0;) {
        if (moving_cards_[i].kind != 3) continue;
        moving_cards_[i].holder.node->modulate = Color(1, 1, 1, 1);
        spawn_card_exhaust(lib_, exhaust_ui(), moving_cards_[i].holder.node, false);
        moving_cards_.erase(moving_cards_.begin() + (long)i);
    }
}

/* CardPileCmd's CreateCardNodeAndUpdateVisuals for a card leaving the
 * draw, discard or exhaust pile for the play pile (Havoc, Mayhem) or the
 * exhaust pile: made at full size at its old pile, AppendPlayPileLerpTween
 * to the play pile (the lerp time, to 0.8 in 0.25 s); exhausted, it waits
 * there (0.5 s, Fast 0.2) and NCardExhaustVfx burns it. */
/* NCard.Create(stolen) added to the %StolenCardPos marker (under the
 * hopper's CardBoneNode, behind its body), UpdateVisuals(Deck, Normal); it
 * stays there, going with the visuals when the hopper dies or escapes. */
void CombatScreen::hold_card(Creature& c, const Json& view) {
    Node* pos = c.visuals && view.is_object() ? c.visuals->find_unique("StolenCardPos") : NULL;
    if (!pos || !pos->children.empty()) return;
    Node* card = make_card(lib_, view);
    if (card) pos->add_child(card);
}

void CombatScreen::create_at_pile(const Json& ch) {
    int id = ch["card"].i(-1);
    if (!hand_box_ || in_play(id) || !ch["view"].is_object()) return;
    Vec2 at = pile_center(ch["from"].str());
    if (at.x < 0) return;
    Holder h = make_holder(ch["view"]);
    if (!h.node) return;
    hand_box_->add_child(h.node);
    play_cards_[id] = ch["view"];
    MovingCard m;
    m.holder = h;
    m.from = m.holder.pos = inverse(hand_box_->global_transform()).xform(at);
    m.to = play_pile_pos();
    m.from_scale = 1;
    m.to_scale = 0.8f;
    m.from_angle = 0;
    m.t = 0;
    m.duration = fast_mode() ? 0.1 : 0.25;
    m.mp_t = -1;
    m.exhaust = false;
    m.kind = 3;
    m.trail = m.path_scene = NULL;
    if (ch["to"].str() == "exhaust") m.burn_at = maxf((float)m.duration, 0.25f) + (fast_mode() ? 0.2 : 0.5);
    h.node->z_index = 2;
    moving_cards_.push_back(m);
}

int CombatScreen::arriving(int pile) const {
    int n = pile >= 0 && pile < 3 ? expect_[pile] : 0;
    for (size_t i = 0; i < moving_cards_.size(); ++i)
        if (moving_cards_[i].kind == 1 && moving_cards_[i].arrive == pile) ++n;
    for (size_t i = 0; i < shuffle_flies_.size(); ++i)
        if (shuffle_flies_[i].arrive == pile) ++n;
    return n;
}

/* NCombatCardPile.AddCard: the count up by one, the icon and the count
 * from 1.25x back to their size (Expo Out, 0.5 s). */
void CombatScreen::land_on_pile(int pile) {
    refresh_counters();
    Node* ui = root_ ? find_named(root_, "CombatUi") : NULL;
    Node* piles = ui ? find_named(ui, "CombatPileContainer") : NULL;
    Node* p = piles ? piles->get_node(pile == 0 ? "DrawPile" : "DiscardPile") : NULL;
    if (!p) return;
    Node* parts[2] = { p->get_node("Icon"), p->get_node("CountContainer/Count") };
    for (int k = 0; k < 2; ++k) {
        if (!parts[k] || !parts[k]->is_control()) continue;
        Control* c = (Control*)parts[k];
        c->pivot_offset = c->size * 0.5f;
        tweens_.kill(&c->scale);
        c->scale = Vec2(1.25f, 1.25f);
        tweens_.tween(&c->scale, Vec2(1, 1), 0.5f, TRANS_EXPO, EASE_OUT);
    }
}

/* PileType.GetTargetPosition: the pile button's centre on screen; (-1, -1)
 * without one. */
Vec2 CombatScreen::pile_center(const std::string& pile) {
    Node* ui = root_ ? find_named(root_, "CombatUi") : NULL;
    Node* piles = ui ? find_named(ui, "CombatPileContainer") : NULL;
    if (pile == "deck") return Vec2(1760, 40); /* the top bar's deck button */
    Node* b = piles ? piles->get_node(pile == "draw" ? "DrawPile" : pile == "exhaust" ? "ExhaustPile" : "DiscardPile") : NULL;
    if (!b || !b->is_control()) return Vec2(-1, -1);
    Control* c = (Control*)b;
    return c->global_transform().xform(c->size * 0.5f);
}

float CombatScreen::power_fly_seconds() {
    if (power_fly_seconds_ >= 0) return power_fly_seconds_;
    VfxCreature pc;
    if (!creature("player", &pc)) return -1;
    Node* scene = instance(lib_, "scenes/vfx/vfx_card_power_fly.tscn");
    Node* p = scene ? scene->get_node("SwooshPath") : NULL;
    if (p && p->type == "Path2D") {
        Path2D* path = (Path2D*)p;
        path->set_point_position(path->point_count() - 1, pc.center - Vec2(960, 440));
        power_fly_seconds_ = path->baked_length() / 3000.0f + 0.05f;
    }
    delete scene;
    return power_fly_seconds_;
}

bool CombatScreen::rewards_ready(bool boss) {
    if (busy()) return false;
    if (rewards_at_ < 0) {
        double left = 0;
        for (std::map<std::string, double>::const_iterator it = death_end_.begin(); it != death_end_.end(); ++it)
            if (it->second - life_t_ > left) left = it->second - life_t_;
        rewards_at_ = life_t_ + (fast_mode() ? (boss ? left * 0.5 : 0.5) : left + 1.0);
    }
    return life_t_ >= rewards_at_;
}

/* ---- NCardFlyShuffleVfx ---- */

static void place_global(Control* n, const Vec2& at) {
    Node* p = n->parent;
    Xform2D pg = p && p->is_canvas_item() ? ((CanvasItem*)p)->global_transform() : Xform2D();
    n->set_position(inverse(pg).xform(at));
}

/* NCardFlyShuffleVfx.Create / _Ready: a card's silhouette from one pile to
 * another on a random arc (CardPileCmd.Shuffle's next card, from the
 * discard pile to the draw pile; a card moved between piles without a
 * node), with the character's card trail and SfxCmd.PlayCardSwooshSfx. */
void CombatScreen::spawn_shuffle_fly(const std::string& from_pile, const std::string& to_pile) {
    int pf = from_pile == "draw" ? 0 : from_pile == "discard" ? 1 : from_pile == "exhaust" ? 2 : -1;
    int pt = to_pile == "draw" ? 0 : to_pile == "discard" ? 1 : -1;
    if (pf >= 0 && depart_[pf] > 0) --depart_[pf];
    if (pt >= 0 && expect_[pt] > 0) --expect_[pt];
    refresh_counters();
    if (!vfx_box_) return;
    Vec2 from = pile_center(from_pile), to = pile_center(to_pile);
    if (from.x < 0 || to.x < 0) return;
    if (lib_->audio())
        lib_->audio()->ui(to_pile == "draw" ? "event:/sfx/ui/cards/card_movement_B_into_draw"
                          : to_pile == "discard" ? "event:/sfx/ui/cards/card_movement_B_into_discard"
                                                 : "event:/sfx/ui/cards/card_movement_B_into_deck");
    Node* n = instance(lib_, "scenes/vfx/vfx_card_shuffle_fly.tscn");
    if (!n || !n->is_control()) {
        delete n;
        return;
    }
    ShuffleFly f;
    f.node = (Control*)n;
    f.from = from;
    f.to = to;
    f.arrive = to_pile == "draw" ? 0 : to_pile == "discard" ? 1 : -1;
    float off = chaotic_range(-300, 400);
    f.speed = chaotic_range(1.1f, 1.25f);
    f.accel = chaotic_range(2, 2.5f);
    float arc = to.y < 540.0f ? -500.0f : 500.0f + off;
    f.duration = chaotic_range(1, 1.75f);
    f.ctrl = from + (to - from) * 0.5f;
    f.ctrl.y -= arc;
    f.time = 0;
    f.phase = 0;
    std::string ch = lower(player_.data["character"].str());
    f.trail = ch.empty() ? NULL : instance(lib_, "scenes/vfx/card_trail_" + ch + ".tscn");
    if (f.trail) {
        vfx_box_->add_child(f.trail);
        Node* sprites = f.trail->get_node("Sprites");
        if (sprites && sprites->is_canvas_item()) {
            CanvasItem* sp = (CanvasItem*)sprites;
            sp->modulate.a = 0;
            tweens_.tween(&sp->modulate.a, 1.0f, 1.0f, TRANS_CUBIC, EASE_OUT);
            if (sp->is_node2d()) tweens_.tween(&((Node2D*)sp)->scale, Vec2(0.5f, 0.5f), 0.5f, TRANS_CUBIC, EASE_IN, 0.25f);
        }
        tweens_.tween(&f.node->modulate.a, 0.75f, 0.5f, TRANS_CUBIC, EASE_OUT);
    }
    vfx_box_->add_child(f.node);
    place_global(f.node, from);
    if (f.trail) follow(f.trail, f.node);
    shuffle_flies_.push_back(f);
}

/* NCardFlyShuffleVfx.PlayAnim: along the arc, speeding up and turning with
 * it; at the pile, from 0.1 of its size to nothing (the trail fades from a
 * quarter of that), then faded out in 0.8 s. */
void CombatScreen::update_shuffle_flies(double dt) {
    float k = (float)dt;
    for (size_t i = shuffle_flies_.size(); i-- > 0;) {
        ShuffleFly& f = shuffle_flies_[i];
        if (f.phase == 0) {
            f.time += f.speed * k;
            f.speed += f.accel * k;
            if (f.time / f.duration > 1) {
                place_global(f.node, f.to);
                f.phase = 1;
                f.time = 0;
                if (f.arrive >= 0) {
                    int pile = f.arrive;
                    f.arrive = -1;
                    land_on_pile(pile);
                }
            } else {
                Vec2 p = bezier(f.from, f.to, f.ctrl, f.time / f.duration);
                Vec2 q = bezier(f.from, f.to, f.ctrl, (f.time + 0.05f) / f.duration);
                place_global(f.node, p);
                f.node->rotation = atan2f(q.y - p.y, q.x - p.x) + PI_F / 2;
            }
        } else if (f.phase == 1) {
            f.time += f.speed * k;
            float u = f.time / f.duration;
            if (u > 0.25f && f.trail) {
                stop_particles(f.trail);
                Trail t = { f.trail, 0 };
                fading_trails_.push_back(t);
                f.trail = NULL;
            }
            float s = maxf(0.1f - 0.2f * u, 0);
            f.node->scale = Vec2(s, s);
            if (u > 1) {
                f.phase = 2;
                f.time = 0;
            }
        } else {
            f.time += k;
            f.node->modulate.a = maxf(1 - f.time / 0.8f, 0);
            if (f.time >= 0.8f) {
                if (f.node->parent) f.node->parent->remove_child(f.node);
                delete f.node;
                shuffle_flies_.erase(shuffle_flies_.begin() + (long)i);
                continue;
            }
        }
        if (f.trail) follow(f.trail, f.node);
    }
}

} /* namespace spire */
