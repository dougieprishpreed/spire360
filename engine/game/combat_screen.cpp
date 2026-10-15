#include "combat_screen.h"
#include "card_view.h"
#include "loc.h"

#include "../scene/controls.h"
#include "../scene/node.h"
#include "../scene/scene.h"
#include "../scene/spine_sprite.h"
#include "../scene/rich_text.h"
#include "../scene/script.h"
#include "../scene/text_nodes.h"

#include <ctype.h>
#include <math.h>

namespace spire {

CombatScreen::CombatScreen(SceneLibrary* lib) : lib_(lib), hand_focus_(-1) {}

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

static Node* find_named(Node* n, const char* name) {
    if (n->name == name) return n;
    for (size_t i = 0; i < n->children.size(); ++i) {
        Node* r = find_named(n->children[i], name);
        if (r) return r;
    }
    return NULL;
}

static Control* child_ctl(Node* n, const char* path) {
    Node* c = n ? n->get_node(path) : NULL;
    return c && c->is_control() ? (Control*)c : NULL;
}

static void set_label(Node* n, const std::string& text) {
    if (n && n->type == "Label") ((Label*)n)->set_text(text);
    else if (n && n->type == "RichTextLabel") ((RichTextLabel*)n)->text = text;
}

/* A control of the full-screen combat UI moved to a screen position, as the
 * scripts' Position = ... (its size kept). */
static void place_on_screen(Control* c, const Vec2& pos) {
    float w = (c->anchor[SIDE_RIGHT] - c->anchor[SIDE_LEFT]) * 1920.0f + c->offset[SIDE_RIGHT] - c->offset[SIDE_LEFT];
    float h = (c->anchor[SIDE_BOTTOM] - c->anchor[SIDE_TOP]) * 1080.0f + c->offset[SIDE_BOTTOM] - c->offset[SIDE_TOP];
    for (int i = 0; i < 4; ++i) c->anchor[i] = 0;
    place(c, pos.x, pos.y, w, h);
}

/* NHotkeyIcon.UpdateInput with a controller: the pad glyph shown, the
 * keyboard key hidden. Glyphs: controller_atlas xbox_360 (Xbox360Config). */
static void show_glyph(SceneLibrary* lib, Node* hotkey_icon, const char* glyph) {
    if (!hotkey_icon) return;
    Node* pad = hotkey_icon->find_unique("ControllerIcon");
    Node* key = hotkey_icon->find_unique("KeyboardIcon");
    if (key && key->is_canvas_item()) ((CanvasItem*)key)->visible = false;
    if (!pad || pad->type != "TextureRect") return;
    Resource* r = lib->ext(std::string("images/atlases/controller_atlas.sprites/xbox_360/") + glyph + ".tres");
    ((TextureRect*)pad)->texture = r && r->is_texture() ? (Texture2DRes*)r : NULL;
    ((CanvasItem*)pad)->visible = r != NULL;
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
        holders_.push_back(h);
    }
    layout_hand();
}

void CombatScreen::set_hand_focus(int index) {
    hand_focus_ = index;
    layout_hand();
}

void CombatScreen::layout_hand() {
    int n = (int)holders_.size();
    if (n == 0) return;
    int focus = hand_focus_ < 0 ? n / 2 : (hand_focus_ >= n ? n - 1 : hand_focus_);
    float scale = 0.8f * (n <= 7 ? 1.0f : n == 8 ? 0.95f : n == 9 ? 0.9f : 0.85f);
    for (int i = 0; i < n; ++i) {
        Control* h = holders_[i];
        float x = HAND_POS[n - 1][i][0], y = HAND_POS[n - 1][i][1];
        float angle = HAND_ANGLE[n - 1][i], sc = scale;
        h->z_index = 0;
        if (i == focus) {
            Control* hit = ctl(h, "Hitbox");
            y = -(hit ? offset_size(hit).y : 422.0f) * 0.5f + 2.0f;
            angle = 0;
            sc = 1;
            h->z_index = 1;
        } else {
            int dist = i < focus ? focus - i : i - focus;
            float push = 100.0f * (1.0f - minf(1.0f, (float)dist / 4.0f));
            x += i < focus ? -push : push;
        }
        place(h, x, y, 0, 0);
        h->rotation = angle * 3.14159265f / 180.0f;
        h->scale = Vec2(sc, sc);
    }
}

/* NSelectionReticle.OnSelect / OnDeselect, at rest. */
void CombatScreen::show_target(int enemy) {
    for (size_t i = 0; i < reticles_.size(); ++i)
        if (reticles_[i]) reticles_[i]->modulate.a = (int)i == enemy ? 1.0f : 0.0f;
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
    reticles_.clear();
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
        if (!make_creature(es[i]["id"].str(), false, c)) {
            reticles_.push_back(NULL); /* keep reticles_[i] = enemy i */
            continue;
        }
        c.hp_bar_reduction = es[i]["hp_bar_reduction"].f();
        enemies->add_child(c.node);
        reticles_.push_back(ctl(c.node, "SelectionReticle"));
        update_bounds(c);
        refresh_health_bar(c, es[i], es[i]["powers"]);
        enemy_nodes.push_back(c);
    }
    setup_combat_ui(root, d);
    build_hand(find_named(root, "CombatUi"), d);
    position_players(ally_nodes, scaling, view["fully_center_players"].b());
    position_enemies(enemy_nodes, scaling);
    play_idle(root);
    return root;
}

} /* namespace spire */
