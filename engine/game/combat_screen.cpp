#include "combat_screen.h"

#include "../scene/controls.h"
#include "../scene/node.h"
#include "../scene/scene.h"
#include "../scene/spine_sprite.h"
#include "../scene/text_nodes.h"

#include <ctype.h>
#include <math.h>

namespace spire {

CombatScreen::CombatScreen(SceneLibrary* lib) : lib_(lib) {}

static std::string lower(std::string s) {
    for (size_t i = 0; i < s.size(); ++i) s[i] = (char)tolower((unsigned char)s[i]);
    return s;
}

static Node* instance(SceneLibrary* lib, const std::string& path) {
    std::string err;
    Node* n = lib->instantiate(path, &err);
    if (!n) log_warn("combat: %s", err.c_str());
    return n;
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

static Control* ctl(Node* scope, const char* unique) {
    Node* n = scope ? scope->find_unique(unique) : NULL;
    return n && n->is_control() ? (Control*)n : NULL;
}

/* Position / Size of a control with its anchors together (all the creature
 * scene's controls): the offsets are the rect. */
static void place(Control* c, float x, float y, float w, float h) {
    c->offset[SIDE_LEFT] = x;
    c->offset[SIDE_TOP] = y;
    c->offset[SIDE_RIGHT] = x + w;
    c->offset[SIDE_BOTTOM] = y + h;
}

static Vec2 offset_pos(const Control* c) { return Vec2(c->offset[SIDE_LEFT], c->offset[SIDE_TOP]); }
static Vec2 offset_size(const Control* c) {
    return Vec2(c->offset[SIDE_RIGHT] - c->offset[SIDE_LEFT], c->offset[SIDE_BOTTOM] - c->offset[SIDE_TOP]);
}

static Color hex(unsigned v) {
    return Color(((v >> 16) & 255) / 255.0f, ((v >> 8) & 255) / 255.0f, (v & 255) / 255.0f, 1.0f);
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
        /* HEALTH_BAR.DEAD (gameplay_ui); the engine has no localization
         * tables yet. */
        l->add_theme_color_override("font_color", cream);
        l->add_theme_color_override("font_outline_color", hex(0x900000));
        l->set_text("Dead");
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

static void play_idle(Node* n) {
    if (n->type == "SpineSprite") ((SpineSprite*)n)->set_animation("idle_loop", true, 0);
    for (size_t i = 0; i < n->children.size(); ++i) play_idle(n->children[i]);
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
    const Json& player = d["player"];
    Creature c;
    if (make_creature(player["character"].is_string() ? player["character"].str() : std::string("ironclad"), true, c)) {
        allies->add_child(c.node);
        update_bounds(c);
        refresh_health_bar(c, player, d["player_powers"]);
        ally_nodes.push_back(c);
    }
    const Json& es = d["enemies"];
    for (size_t i = 0; i < es.size(); ++i) {
        if (!make_creature(es[i]["id"].str(), false, c)) continue;
        c.hp_bar_reduction = es[i]["hp_bar_reduction"].f();
        enemies->add_child(c.node);
        update_bounds(c);
        refresh_health_bar(c, es[i], es[i]["powers"]);
        enemy_nodes.push_back(c);
    }
    position_players(ally_nodes, scaling, view["fully_center_players"].b());
    position_enemies(enemy_nodes, scaling);
    play_idle(root);
    return root;
}

} /* namespace spire */
