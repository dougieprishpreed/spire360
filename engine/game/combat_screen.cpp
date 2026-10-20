#include "combat_screen.h"
#include "game.h"
#include "card_view.h"
#include "loc.h"
#include "../audio/game_audio.h"
#include "ui_util.h"

#include "../scene/controls.h"
#include "../scene/tree.h"
#include "../scene/node.h"
#include "../scene/particles.h"
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
 * root holds it; the screen outlives its tree). */
class CombatDriver : public ScriptBehavior {
public:
    explicit CombatDriver(CombatScreen* s) : screen_(s) {}
    virtual void process(Node*, double dt) { screen_->process(dt); }

private:
    CombatScreen* screen_;
};

CombatScreen::CombatScreen(SceneLibrary* lib)
    : lib_(lib), hand_focus_(-1), aftermath_(false), hand_box_(NULL), play_card_(-1), aiming_(false), play_single_(false), play_target_(-1), arrow_(NULL),
      arrow_head_(NULL) {
    player_.node = NULL;
    osty_.node = NULL;
    next_cue_ = 0;
    cue_t_ = 0;
    life_t_ = 0;
    busy_until_ = 0;
    root_ = NULL;
    vfx_box_ = NULL;
    places_ = NULL;
    final_ = false;
    visual_only_ = false;
    tips_ = NULL;
    tips_for_ = -2;
    pending_finish_ = false;
    played_instance_ = -1;
    orb_box_ = NULL;
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
    Node* n = instance(lib_, "scenes/combat/creature.tscn");
    if (!n || !n->is_control()) {
        delete n;
        return false;
    }
    Node* v = instance(lib_, "scenes/creature_visuals/" + lower(visuals_id) + ".tscn");
    if (!v) v = instance(lib_, "scenes/creature_visuals/fallback.tscn");
    if (v) {
        n->add_child(v);
        /* MoveChild(Visuals, 0) */
        n->children.pop_back();
        n->children.insert(n->children.begin(), v);
        if (v->is_node2d()) ((Node2D*)v)->position = Vec2(0, 0);
        Node* bounds = v->find_unique("Bounds");
        if (bounds && bounds->is_control()) {
            Control* b = (Control*)bounds;
            out.width = b->offset[SIDE_RIGHT] - b->offset[SIDE_LEFT]; /* anchors are 0 */
        }
    }
    out.node = (Control*)n;
    out.visuals = v;
    return true;
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
    Control* intents = ctl(c.node, "Intents");
    Node* ipos = vis->get_node("IntentPos");
    if (intents && ipos && ipos->is_node2d()) {
        Vec2 isz = offset_size(intents);
        Vec2 p = ((Node2D*)ipos)->position - isz * 0.5f;
        place(intents, p.x, p.y * vs.x, isz.x, isz.y);
    }

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
    if (blk <= 0) {
        if (block) block->visible = false;
        if (outline) outline->visible = false;
        fg->self_modulate = hex(0xF1373E);
    } else {
        if (outline) outline->visible = true;
        fg->self_modulate = hex(0x3B6FA3);
        if (block) block->visible = true;
        if (block_label && block_label->type == "Label") ((Label*)block_label)->set_text(format("%d", blk));
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

static void set_counter_label(Node* label, int count) {
    set_label(label, format("%d", count));
}

void CombatScreen::setup_combat_ui(Node* root, const Json& d) {
    Node* ui = find_named(root, "CombatUi");
    if (!ui) return;
    const Json& player = d["player"];
    std::string character = lower(player["character"].is_string() ? player["character"].str() : std::string("ironclad"));
    int energy = d["energy"].i(), max_energy = d["max_energy"].i(), stars = d["stars"].i();

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
        Color outline(0, 0, 0, 0.05f); /* CharacterModel default #0000000D */
        for (size_t i = 0; i < sizeof(outlines) / sizeof(outlines[0]); ++i)
            if (character == outlines[i].id) outline = hex(outlines[i].outline);
        Node* label = orb->get_node("Label");
        if (label && label->is_control()) {
            Control* l = (Control*)label;
            l->add_theme_color_override("font_color", energy > 0 ? Color(1, 0.964706f, 0.886275f) : hex(0xFF5555));
            l->add_theme_color_override("font_outline_color", energy > 0 ? outline : hex(0x501717));
            set_label(label, format("%d/%d", energy, max_energy));
        }
        /* RefreshLabel: the layers' energy_orb_dark material (the scene's
         * own state) only at 0 energy; DarkGray then. */
        Node* layers = orb->find_unique("Layers");
        Node* spin = orb->find_unique("RotationLayers");
        Node* groups[2] = { layers, spin };
        for (int g = 0; g < 2; ++g)
            for (size_t i = 0; groups[g] && i < groups[g]->children.size(); ++i) {
                Node* c = groups[g]->children[i];
                if (c->is_control() && energy > 0) ((Control*)c)->material = NULL;
            }
        if (layers && layers->is_control() && energy <= 0)
            ((Control*)layers)->modulate = Color(0.663f, 0.663f, 0.663f); /* Colors.DarkGray */
        if (spin && !orb->behavior) orb->behavior = new SpinLayers(spin, energy > 0 ? 30.0f : 5.0f);
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
        Node* spin = star->get_node("Icon/RotationLayers");
        if (spin && !star->behavior) star->behavior = new SpinLayers(spin, stars > 0 ? 30.0f : 5.0f);
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
            moving_cards_.push_back(m);
        }
    }
    holders_ = next;
    hand_ = cards;
    tips_for_ = -2;
    layout_hand(!animate);
    refresh_tips();
}

void CombatScreen::set_hand_focus(int index) {
    hand_focus_ = index;
    layout_hand();
    refresh_tips();
}

/* The tips of the focused card (none while a card is being played). */
void CombatScreen::refresh_tips() {
    int n = (int)holders_.size();
    int focus = n == 0 || play_card_ >= 0 || aiming_ ? -1 : (hand_focus_ < 0 ? n / 2 : (hand_focus_ >= n ? n - 1 : hand_focus_));
    if (focus == tips_for_) return;
    tips_for_ = focus;
    if (tips_) {
        if (tips_->parent) tips_->parent->remove_child(tips_);
        delete tips_;
        tips_ = NULL;
    }
    if (focus < 0 || !root_) return;
    tips_ = make_tip_set(lib_, hand_[(size_t)focus]["tips"]);
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
void CombatScreen::layout_hand(bool snap) {
    int n = (int)holders_.size();
    if (n == 0) return;
    int focus = hand_focus_ < 0 ? n / 2 : (hand_focus_ >= n ? n - 1 : hand_focus_);
    float scale = 0.8f * (n <= 7 ? 1.0f : n == 8 ? 0.95f : n == 9 ? 0.9f : 0.85f);
    for (int i = 0; i < n; ++i) {
        Holder& h = holders_[i];
        if (i == play_card_) continue; /* centred by begin_play */
        float x = HAND_POS[n - 1][i][0], y = HAND_POS[n - 1][i][1];
        float angle = HAND_ANGLE[n - 1][i], sc = scale;
        h.node->z_index = 0;
        if (i == focus) {
            y = -hitbox_height(h.node) * 0.5f + 2.0f;
            angle = 0;
            sc = 1;
            h.node->z_index = 1;
        } else {
            int dist = i < focus ? focus - i : i - focus;
            float push = 100.0f * (1.0f - minf(1.0f, (float)dist / 4.0f));
            x += i < focus ? -push : push;
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

/* Godot's Transform2D.affine_inverse. */
static Xform2D inverse(const Xform2D& t) {
    float det = t.x.x * t.y.y - t.y.x * t.x.y;
    if (det == 0) return Xform2D();
    float id = 1.0f / det;
    Xform2D r(Vec2(t.y.y * id, -t.x.y * id), Vec2(-t.y.x * id, t.x.x * id), Vec2(0, 0));
    r.origin = r.basis_xform(t.origin) * -1.0f;
    return r;
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

void CombatScreen::begin_play(int card, const std::string& target_type, int target) {
    if (card < 0 || card >= (int)holders_.size()) return;
    end_play();
    play_card_ = card;
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
void CombatScreen::process(double dt) {
    float t = (float)dt;
    tweens_.update(dt);
    for (size_t i = moving_cards_.size(); i-- > 0;) {
        MovingCard& m = moving_cards_[i];
        m.t += dt;
        float u = clampf((float)(m.t / m.duration), 0, 1);
        float k = tween_interpolate(TRANS_CUBIC, EASE_OUT, u, 0, 1, 1);
        Vec2 at = m.from + (m.to - m.from) * k;
        place(m.holder.node, at.x, at.y, 0, 0);
        float sc = m.from_scale + (m.to_scale - m.from_scale) * k;
        m.holder.node->scale = Vec2(sc, sc);
        m.holder.node->modulate.a = 1 - u;
        if (u >= 1) {
            if (m.holder.node->parent) m.holder.node->parent->remove_child(m.holder.node);
            delete m.holder.node;
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
    /* The tips follow their card: right of its hitbox (left of it past
     * three quarters of the screen), kept above the screen's bottom. */
    if (tips_ && tips_for_ >= 0 && tips_for_ < (int)holders_.size()) {
        Control* hit = ctl(holders_[(size_t)tips_for_].node, "Hitbox");
        if (hit) {
            Xform2D xf = hit->global_transform();
            float sx = sqrtf(xf.x.x * xf.x.x + xf.x.y * xf.x.y);
            Vec2 at = xf.origin;
            float w = tips_->size.x, hgt = tips_->size.y;
            float x = at.x > 1920.0f * 0.75f ? at.x - w - 10.0f : at.x + (offset_size(hit).x + 10.0f) * sx;
            if (x + w > 1920.0f) x = at.x - w - 10.0f; /* CorrectHorizontalOverflow */
            float y = at.y;
            if (y + hgt > 1080.0f) y = 1080.0f - hgt;
            tips_->set_position(Vec2(x, y));
            tips_->modulate.a = 1;
        }
    }
    /* The steps' cues as their time comes; the damage numbers fly
     * (NDamageNumVfx._Process: velocity, gravity 2000 px/s2). */
    cue_t_ += dt;
    life_t_ += dt;
    for (size_t i = 0; i < orbs_.size(); ++i) {
        OrbSlot& o = orbs_[i];
        o.t += dt;
        float k = tween_interpolate(TRANS_SINE, EASE_IN_OUT, (float)(o.t < 0.45 ? o.t : 0.45), 0, 1, 0.45f);
        o.node->set_position(o.from + (o.to - o.from) * k);
    }
    for (size_t i = fading_orbs_.size(); i-- > 0;) {
        FadingOrb& o = fading_orbs_[i];
        if (life_t_ >= o.until) {
            o.node->queue_free();
            fading_orbs_.erase(fading_orbs_.begin() + (long)i);
        } else o.node->modulate.a = (float)(o.until - life_t_) / 0.25f;
    }
    while (next_cue_ < cues_.size() && cues_[next_cue_].at <= cue_t_) fire(cues_[next_cue_++]);
    for (size_t i = banners_.size(); i-- > 0;)
        if (life_t_ >= banners_[i].until) {
            banners_[i].node->queue_free();
            banners_.erase(banners_.begin() + (long)i);
        }
    for (size_t i = numbers_.size(); i-- > 0;) {
        FlyingNumber& n = numbers_[i];
        if (life_t_ >= n.until) {
            n.node->queue_free();
            numbers_.erase(numbers_.begin() + (long)i);
            continue;
        }
        n.node->position = n.node->position + n.velocity * t;
        n.velocity = n.velocity + Vec2(0, 2000) * t;
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
    numbers_.clear();
    root_ = root;
    state_ = d;
    vfx_box_ = ctl(root, "CombatVfxContainer");
    hand_ = d["hand"];
    tips_ = NULL;
    tips_for_ = -2;
    std::vector<int> enemy_node_ids;
    ghosts_.clear();
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
    position_players(ally_nodes, scaling, view["fully_center_players"].b(), &osty_, osty_max_hp, &pets_x_);
    position_pets(pets_x_);
    /* Where the enemies stand: as before in this fight; laid out again
     * (all of them) when one has no place yet. */
    bool known = places_ != NULL && !enemy_nodes.empty();
    for (size_t i = 0; known && i < enemy_node_ids.size(); ++i)
        if (places_->find(enemy_node_ids[i]) == places_->end()) known = false;
    if (known) {
        for (size_t i = 0; i < enemy_nodes.size(); ++i) enemy_nodes[i].node->set_position((*places_)[enemy_node_ids[i]]);
    } else {
        std::vector<Vec2> at;
        position_enemies(enemy_nodes, scaling, &at);
        for (size_t i = 0; places_ && i < at.size() && i < enemy_node_ids.size(); ++i)
            (*places_)[enemy_node_ids[i]] = at[i];
    }
    play_idle(root);
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
    driver->behavior = new CombatDriver(this);
    root->add_child(driver);
    if (!aftermath_ && !final_) refresh_tips();
    return root;
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
    if (animate && mid && c.shown_hp >= 0 && c.data["hp"].i() > 0) {
        float target = mid->offset[SIDE_RIGHT];
        mid->offset[SIDE_RIGHT] = old;
        if (changed) tweens_.tween(&mid->offset[SIDE_RIGHT], target, 1, TRANS_EXPO, EASE_OUT, target < old ? 1.0f : 0.0f);
    }
    c.shown_hp = c.data["hp"].i();
    c.shown_max_hp = c.data["max_hp"].i();
}

void CombatScreen::refresh_counters() {
    Node* ui = find_named(root_, "CombatUi");
    Node* box = ui ? ui->get_node("EnergyCounterContainer") : NULL;
    if (box && !box->children.empty())
        set_label(box->children[0]->get_node("Label"), format("%d/%d", state_["energy"].i(), state_["max_energy"].i()));
    Node* star = ui ? ui->get_node("StarCounter") : NULL;
    if (star) {
        set_visible(star, state_["always_show_stars"].b() || state_["stars"].i() > 0);
        set_label(star->find_unique("CountLabel"), format("[center]%d[/center]", state_["stars"].i()));
    }
    Node* piles = find_named(ui, "CombatPileContainer");
    static const char* names[] = { "DrawPile", "DiscardPile", "ExhaustPile" };
    static const char* fields[] = { "draw_pile_count", "discard_pile_count", "exhaust_pile_count" };
    for (int i = 0; piles && i < 3; ++i) {
        Node* pile = piles->get_node(names[i]);
        if (!pile) continue;
        set_counter_label(pile->get_node("CountContainer/Count"), state_[fields[i]].i());
        if (i == 2) set_visible(pile, state_[fields[i]].i() > 0);
    }
    Node* end = ui ? ui->get_node("EndTurnButton") : NULL;
    if (end) set_label(end->get_node("Visuals/Label"), Loc::format("gameplay_ui", "END_TURN_BUTTON", "turnNumber",
                        format("%d", state_["turn_number"].i(state_["round"].i(1)))));
    Node* top = root_ ? root_->get_node("TopBarLayer") : NULL;
    if (top) {
        set_label(find_named(top, "HpLabel"), format("%d/%d", player_.data["hp"].i(), player_.data["max_hp"].i()));
        set_label(find_named(top, "GoldLabel"), format("%d", state_["player"]["gold"].i()));
    }
}

/* Keep the background, Spine tracks, particles and surviving holders alive.
 * Only new combatants are instanced; absent enemies remain through death. */
void CombatScreen::update_decision(const Json& d, bool finishing) {
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
    pending_finish_ = finishing;
    previous_ = state_;
    played_instance_ = play_card_ >= 0 && play_card_ < (int)holders_.size() ? holders_[(size_t)play_card_].id : -1;
    played_destination_ = Vec2(0, -500);
    if (play_target_ >= 0 && play_target_ < (int)enemies_.size() && enemies_[(size_t)play_target_].node && hand_box_)
        played_destination_ = inverse(hand_box_->global_transform()).xform(enemies_[(size_t)play_target_].node->global_transform().xform(enemies_[(size_t)play_target_].hitbox.pos + enemies_[(size_t)play_target_].hitbox.size * 0.5f));
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
                if (places_ && places_->find(es[i]["combat_id"].i()) != places_->end()) c.node->set_position((*places_)[es[i]["combat_id"].i()]);
                else c.node->set_position(Vec2(250.0f + (float)i * 220.0f, 200));
                play_idle(c.node);
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
    play_steps(d["steps"]);
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
    for (size_t i = 0; i < enemies_.size(); ++i) hide(enemies_[i].node);
    for (size_t i = 0; i < ghosts_.size(); ++i) hide(ghosts_[i].c.node);
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
    if (from == "hand" && to != "hand") {
        for (size_t i = 0; i < holders_.size(); ++i) {
            if (holders_[i].id != id) continue;
            MovingCard m;
            m.holder = holders_[i];
            m.from = m.holder.pos;
            m.to = id == played_instance_ ? played_destination_ : Vec2(850, to == "exhaust" ? -250.0f : -70.0f);
            m.from_scale = m.holder.scale;
            m.to_scale = id == played_instance_ ? 0.5f : 0.15f;
            m.t = 0;
            m.duration = 0.4;
            moving_cards_.push_back(m);
            holders_.erase(holders_.begin() + (long)i);
            break;
        }
        Json hand = Json::make_array();
        for (size_t i = 0; i < hand_.size(); ++i)
            if (hand_[i]["instance_id"].i(-1) != id) hand.append(hand_[i]);
        hand_ = hand;
        layout_hand();
    }
    if (to == "hand" && from != "hand") {
        const Json& cards = pending_["hand"];
        for (size_t i = 0; hand_box_ && i < cards.size(); ++i) {
            if (cards[i]["instance_id"].i(-2) != id) continue;
            bool exists = false;
            for (size_t j = 0; j < holders_.size(); ++j) if (holders_[j].id == id) exists = true;
            if (exists) break;
            Holder h = make_holder(cards[i]);
            if (!h.node) break;
            h.pos = Vec2(from == "draw" ? -850.0f : 0.0f, from == "draw" ? -70.0f : -500.0f);
            h.scale = 0.15f;
            h.angle = -25;
            hand_box_->add_child(h.node);
            holders_.push_back(h);
            hand_.append(cards[i]);
            layout_hand();
            break;
        }
    }
    refresh_counters();
}

void CombatScreen::apply_state(const Json& ch) {
    const std::string& type = ch["t"].str();
    Creature* c = creature_for(ch["who"].str());
    if (type == "card") { move_card(ch); return; }
    if (type == "orb_activate") { activate_orb(ch); return; }
    if (type == "orbs" && ch["orbs"].is_array()) {
        sync_orbs(ch["orbs"], ch["slots"].i(), true);
        state_.set("orbs", ch["orbs"]);
        state_.set("orb_slots", ch["slots"]);
        return;
    }
    if (type == "energy" || type == "max_energy" || type == "stars" || type == "round") {
        state_.set(type, ch["to"]);
        if (type == "round") state_.set("turn_number", ch["to"]);
        refresh_counters();
        return;
    }
    if (!c) return;
    if (type == "hp" || type == "max_hp" || type == "block") {
        c->data.set(type, ch["to"]);
        refresh_state(*c, true);
        refresh_counters();
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
            const Json* metadata = NULL;
            const Json& es = pending_["enemies"];
            const Json& powers = ch["who"].str() == "player" ? pending_["player_powers"] : c->powers;
            metadata = find_power(powers, ch["power"].str().c_str());
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
    } else if (type == "spawn" || type == "revive") {
        c->node->visible = true;
        tweens_.kill(&c->node->modulate.a);
        c->node->modulate.a = 1;
        play_idle(c->node);
    }
}

void CombatScreen::finish_state() {
    if (pending_.is_null()) return;
    if (pending_["player"].is_object()) {
        player_.data = pending_["player"];
        player_.powers = pending_finish_ ? Json() : pending_["player_powers"];
        refresh_state(player_, true);
        make_powers(player_, player_.powers);
    }
    if (!pending_finish_) {
        const Json& es = pending_["enemies"];
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
            c.node->visible = true;
            tweens_.kill(&c.node->modulate.a);
            c.node->modulate.a = 1;
            refresh_state(c, true);
            make_powers(c, c.powers);
        }
        if (osty_.node && pending_["osty"].is_object()) {
            osty_.data = pending_["osty"];
            refresh_state(osty_, true);
        }
        sync_hand(pending_["hand"], true);
        if (state_["orbs"] != pending_["orbs"] || state_["orb_slots"] != pending_["orb_slots"])
            sync_orbs(pending_["orbs"], pending_["orb_slots"].i(), true);
        state_ = pending_;
    } else {
        sync_hand(Json(), true);
        hide(find_named(root_, "CombatUi"));
        state_.set("player", pending_["player"]);
    }
    refresh_counters();
    pending_ = Json();
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

/* Where VfxCmd puts an effect: a creature's VfxSpawnPosition (%CenterPos)
 * or its position (its feet), the middle of a side's creatures, the
 * screen's centre. */
bool CombatScreen::vfx_position(const Cue& c, Vec2* out) {
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
    if (c.kind == CUE_STATE) { apply_state(c.data); return; }
    if (c.kind == CUE_SYNC) { finish_state(); return; }
    if (c.kind == CUE_BANNER) {
        show_banner(c.name, c.amount);
        return;
    }
    if (c.kind == CUE_ANIM) {
        if (c.name == "Dead" && lib_->audio()) lib_->audio()->stop_room_loops(c.who);
        Creature* cr = creature_for(c.who);
        SpineSprite* s = cr ? first_spine(cr->visuals) : NULL;
        const char* name = trigger_animation(c.name);
        std::string anim = name ? name : lower(c.name);
        if (c.name == "Dead" && cr && cr->node) {
            cr->data.set("hp", Json::make_number(0));
            refresh_state(*cr, true);
            hide(cr->node->find_unique("Intents"));
            /* NCreature's death: the creature fades once the animation has
             * played (the game frees it). */
            tweens_.tween(&cr->node->modulate.a, 0.0f, 0.6f, TRANS_LINEAR, EASE_IN_OUT, 0.9f);
        }
        if (!s) return;
        if (!s->set_animation(anim, false, 0)) return;
        SpSkeleton* sk = s->skeleton();
        const SpAnimation* idle = sk && anim != "die" ? sk->data()->find_animation("idle_loop") : NULL;
        if (idle && s->animation_state()) s->animation_state()->add_animation(0, idle, true, 0);
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
    /* CUE_DAMAGE: NDamageNumVfx (scenes/vfx/vfx_damage_num.tscn) 100 px
     * above the centre, thrown up, red to cream, fading over 2 s. */
    Node* num = instance(lib_, "scenes/vfx/vfx_damage_num.tscn");
    if (!num || !num->is_node2d()) {
        delete num;
        return;
    }
    Node2D* n = (Node2D*)num;
    float rx = (float)(chaotic_rand() % 2001) / 1000.0f - 1.0f, ry = (float)(chaotic_rand() % 2001) / 1000.0f - 1.0f;
    n->position = at + Vec2(rx * 10.0f, -100.0f + ry * 5.0f);
    n->rotation = ((float)(chaotic_rand() % 1001) / 100.0f - 5.0f) * PI_F / 180.0f;
    Node* label = num->get_node("Label");
    set_label(label, format("%d", c.amount));
    if (label && label->is_control()) {
        float k = 1.2f + (float)(chaotic_rand() % 101) / 1000.0f;
        ((Control*)label)->scale = Vec2(k, k);
    }
    Color cream = hex(0xFFF6E2);
    tweens_.tween(&n->modulate.r, cream.r, 0.5f, TRANS_CUBIC, EASE_OUT);
    tweens_.tween(&n->modulate.g, cream.g, 0.5f, TRANS_CUBIC, EASE_OUT);
    tweens_.tween(&n->modulate.b, cream.b, 0.5f, TRANS_CUBIC, EASE_OUT);
    tweens_.tween(&n->modulate.a, 0.0f, 2.0f, TRANS_QUAD, EASE_IN);
    tweens_.tween_from(&n->scale, Vec2(2.5f, 2.5f), Vec2(1, 1), 1.2f, TRANS_QUAD, EASE_OUT);
    vfx_box_->add_child(num);
    FlyingNumber f = { n, Vec2(rx * 100.0f, -700.0f - (float)(chaotic_rand() % 101)), life_t_ + 2.05 };
    numbers_.push_back(f);
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
    if (lib_->audio()) lib_->audio()->ui(kind == "start" ? "battle_start_1.mp3" : kind == "enemy" ? "enemy_turn.mp3" : "player_turn.mp3");
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

void CombatScreen::show_start_banner() {
    busy_until_ = 2.6; /* NCombatStartBanner: player turn starts here. */
    Cue c;
    c.kind = CUE_BANNER;
    c.amount = 1;
    c.at = 0;
    c.name = "start";
    add_cue(c);
    c.at = 2.6;
    c.name = "player";
    add_cue(c);
}

/* The steps as a timeline. CreatureCmd.TriggerAnim's pause precedes its
 * impact; VFX, damage numbers and state changes at that impact are parallel.
 * Cmd.Wait supplies the remaining pacing without an extra per-step delay. */
void CombatScreen::play_steps(const Json& steps) {
    cues_.clear();
    next_cue_ = 0;
    cue_t_ = 0;
    double t = 0;
    /* A new round in the steps: the enemies' turn came first (its banner,
     * then their actions), the player's turn banner at the new round. */
    bool new_round = false;
    for (size_t i = 0; i < steps.size() && !new_round; ++i)
        for (size_t k = 0; k < steps[i].size(); ++k)
            if (steps[i][k]["t"].str() == "round") new_round = true;
    if (new_round) {
        Cue b;
        b.kind = CUE_BANNER;
        b.name = "enemy";
        b.amount = 0;
        b.at = 0;
        cues_.push_back(b);
        t = 0.9;
    }
    for (size_t i = 0; i < steps.size(); ++i) {
        const Json& step = steps[i];
        for (size_t k = 0; k < step.size(); ++k)
            if (step[k]["t"].str() == "round") {
                Cue b;
                b.kind = CUE_BANNER;
                b.name = "player";
                b.amount = step[k]["to"].i();
                b.at = t + 0.3;
                cues_.push_back(b);
                t += 0.3;
            }
        for (size_t k = 0; k < step.size(); ++k) {
            const Json& ch = step[k];
            const std::string& type = ch["t"].str();
            Cue c;
            c.at = t;
            c.amount = 0;
            if (type == "sfx" || (type == "ui" && (ch["call"].str().find("Audio") != std::string::npos ||
                                                    ch["call"].str().find("Music") != std::string::npos))) {
                c.kind = CUE_AUDIO; c.data = ch; cues_.push_back(c);
            } else if (type == "anim") {
                c.kind = CUE_ANIM;
                c.who = ch["who"].str();
                c.name = ch["name"].str();
                if (!creature_for(c.who)) continue;
                cues_.push_back(c);
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
            } else if (type == "ui" && ch["call"].str() == "NDamageNumVfx.Create" && ch["args"].size() >= 2 &&
                       ch["args"][1].is_number() && ch["args"][1].i() > 0) {
                /* The number the game shows (unblocked damage + overkill). */
                c.kind = CUE_DAMAGE;
                c.who = ch["args"][(size_t)0].str();
                c.where = "center";
                c.amount = ch["args"][1].i();
                if (!creature_for(c.who)) continue;
                cues_.push_back(c);
            } else if (type == "death" || type == "despawn") {
                c.kind = CUE_ANIM;
                c.who = ch["who"].str();
                c.name = "Dead";
                if (!creature_for(c.who)) continue;
                bool already = false;
                for (size_t q = 0; q < cues_.size(); ++q)
                    if (cues_[q].kind == CUE_ANIM && cues_[q].who == c.who && cues_[q].name == "Dead") already = true;
                if (already) continue;
                cues_.push_back(c);
            } else if (type == "wait") {
                double w = ch["ms"].num(0) / 1000.0;
                if (w > 0) t += w;
            } else {
                c.kind = CUE_STATE;
                c.data = ch;
                cues_.push_back(c);
            }
        }
    }
    if (!pending_.is_null()) {
        Cue sync;
        sync.kind = CUE_SYNC;
        sync.at = t;
        sync.amount = 0;
        cues_.push_back(sync);
    }
    /* The pad waits for the actions (not for the banners). */
    busy_until_ = 0;
    for (size_t i = 0; i < cues_.size(); ++i) {
        double end = cues_[i].at + (cues_[i].kind == CUE_ANIM && cues_[i].name == "Dead" ? 1.5 : 0.4);
        if (cues_[i].kind != CUE_BANNER && cues_[i].kind != CUE_AUDIO && end > busy_until_) busy_until_ = end;
    }
}

} /* namespace spire */
