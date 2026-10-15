#include "combat_screen.h"

#include "../scene/node.h"
#include "../scene/scene.h"
#include "../scene/spine_sprite.h"

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
        ally_nodes.push_back(c);
    }
    const Json& es = d["enemies"];
    for (size_t i = 0; i < es.size(); ++i) {
        if (!make_creature(es[i]["id"].str(), false, c)) continue;
        enemies->add_child(c.node);
        enemy_nodes.push_back(c);
    }
    position_players(ally_nodes, scaling, view["fully_center_players"].b());
    position_enemies(enemy_nodes, scaling);
    play_idle(root);
    return root;
}

} /* namespace spire */
