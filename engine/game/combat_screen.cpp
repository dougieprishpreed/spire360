#include "combat_screen.h"
#include "card_view.h"
#include "loc.h"
#include "ui_util.h"

#include "../scene/controls.h"
#include "../scene/tree.h"
#include "../scene/node.h"
#include "../scene/scene.h"
#include "../scene/spine_sprite.h"
#include "../scene/rich_text.h"
#include "../scene/script.h"
#include "../scene/shaders.h"
#include "../scene/text_nodes.h"

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
    std::vector<Control*> nodes;
    for (size_t i = 0; i < powers.size(); ++i) {
        const Json& pw = powers[i];
        if (!pw["visible"].b(true)) continue;
        Node* n = instance(lib_, "scenes/combat/power.tscn");
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
        box->add_child(n);
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
void CombatScreen::position_enemies(std::vector<Creature>& cs, float scaling) {
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
        cs[i].node->set_position(Vec2(x + cs[i].width * 0.5f, 200.0f - ((i % 2) != 0 ? stagger : 0.0f)));
        x += cs[i].width + gap;
    }
}

/* NCombatRoom.PositionPlayersAndPets, for the local player (pets come
 * later: Necrobinder's Osty). */
void CombatScreen::position_players(std::vector<Creature>& cs, float scaling, bool fully_center) {
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
void CombatScreen::build_hand(Node* ui, const Json& d) {
    holders_.clear();
    Node* hand = ui ? find_named(ui, "Hand") : NULL;
    Node* box = hand ? hand->find_unique("CardHolderContainer") : NULL;
    hand_box_ = box && box->is_control() ? (Control*)box : NULL;
    if (!box) return;
    const Json& cards = d["hand"];
    for (size_t i = 0; i < cards.size() && i < 10; ++i) {
        Node* holder = instance(lib_, "scenes/cards/holders/hand_card_holder.tscn");
        if (!holder || !holder->is_control()) {
            delete holder;
            continue;
        }
        Control* h = (Control*)holder;
        Node* card = make_card(lib_, cards[i]);
        if (card) h->add_child(card);
        /* Flash (upgrade / cost change pulse) and the index label are off at rest. */
        Node* flash = h->get_node("Flash");
        if (flash && flash->is_canvas_item()) ((CanvasItem*)flash)->modulate.a = 0;
        Node* index = h->find_unique("HandIndex");
        if (index && index->is_canvas_item()) ((CanvasItem*)index)->visible = false;
        box->add_child(h);
        Holder hd;
        hd.node = h;
        hd.angle = hd.to_angle = 0;
        hd.scale = hd.to_scale = 1;
        hd.text = cards[i]["description_bbcode"].str();
        const Json& by = cards[i]["description_by_target"];
        for (size_t t = 0; t < by.size(); ++t) hd.text_by_target.push_back(by[t].str());
        hd.text_multi = cards[i]["description_multi"].is_string() ? cards[i]["description_multi"].str() : hd.text;
        holders_.push_back(hd);
    }
    layout_hand(true);
}

void CombatScreen::set_hand_focus(int index) {
    hand_focus_ = index;
    layout_hand();
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
    if (arrow_ && arrow_->visible && (play_card_ >= 0 || aiming_)) {
        arrow_pos_ = arrow_pos_ + (arrow_to_ - arrow_pos_) * minf(1.0f, t * 14.0f);
        update_arrow(arrow_pos_);
    }
}

static void play_idle(Node* n) {
    if (n->type == "SpineSprite") ((SpineSprite*)n)->set_animation("idle_loop", true, 0);
    for (size_t i = 0; i < n->children.size(); ++i) play_idle(n->children[i]);
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
        refresh_health_bar(c, player, player_powers);
        set_nameplate_text(c.nameplate, player["name"].str());
        make_powers(c, player_powers);
        ally_nodes.push_back(c);
        player_ = c;
    }
    static const Json no_enemies;
    const Json& es = aftermath_ ? no_enemies : d["enemies"];
    for (size_t i = 0; i < es.size(); ++i) {
        if (!make_creature(es[i]["id"].str(), false, c)) {
            enemies_.push_back(c); /* node NULL: keeps enemies_[i] = enemy i */
            continue;
        }
        c.hp_bar_reduction = es[i]["hp_bar_reduction"].f();
        enemies->add_child(c.node);
        update_bounds(c);
        refresh_health_bar(c, es[i], es[i]["powers"]);
        set_nameplate_text(c.nameplate, es[i]["name"].str());
        make_intents(c, es[i]["intents"], (int)i);
        make_powers(c, es[i]["powers"]);
        enemy_nodes.push_back(c);
        enemies_.push_back(c);
    }
    if (aftermath_) {
        Node* ui = find_named(root, "CombatUi");
        if (ui && ui->is_canvas_item()) ((CanvasItem*)ui)->visible = false;
    } else {
        setup_combat_ui(root, d);
        build_hand(find_named(root, "CombatUi"), d);
    }
    position_players(ally_nodes, scaling, view["fully_center_players"].b());
    position_enemies(enemy_nodes, scaling);
    play_idle(root);
    arrow_ = make_arrow();
    root->add_child(arrow_);
    Node* driver = SceneLibrary::create_node("Node");
    driver->name = "CombatDriver";
    driver->behavior = new CombatDriver(this);
    root->add_child(driver);
    return root;
}

} /* namespace spire */
