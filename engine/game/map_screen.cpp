#include "map_screen.h"
#include "combat_vfx.h"
#include "loc.h"
#include "../audio/game_audio.h"

#include "../scene/controls.h"
#include "../scene/line_2d.h"
#include "../scene/node.h"
#include "../scene/scene.h"
#include "../scene/spine_sprite.h"

#include <math.h>

namespace spire {

namespace {
/* NMapCircleVfx._Ready with playAnim: its scale from twice down to its own
 * (1 s, Expo Out), its alpha 0 -> 0.95 (0.5 s, Expo Out), the five circle
 * frames at 24 a second. NMapNodeSelectVfx: gone after 1 s. */
class MapFx : public ScriptBehavior {
public:
    MapFx(SceneLibrary* lib, Control* c, Node* image, float scale, bool circle)
        : lib_(lib), c_(c), image_(image), scale_(scale), circle_(circle), done_(false), t_(0), frame_(-1) {}
    virtual void process(Node* n, double dt) {
        if (done_) return;
        t_ += dt;
        if (!circle_) {
            if (t_ >= 1.0 && !n->queued_for_deletion()) n->queue_free();
            return;
        }
        float u = clampf((float)t_, 0, 1);
        float s = scale_ * 2.0f + (scale_ - scale_ * 2.0f) * tween_interpolate(TRANS_EXPO, EASE_OUT, u, 0, 1, 1);
        c_->scale = Vec2(s, s);
        c_->modulate.a = 0.95f * tween_interpolate(TRANS_EXPO, EASE_OUT, clampf((float)(t_ / 0.5), 0, 1), 0, 1, 1);
        int f = (int)(t_ * 24.0);
        if (f > 4) f = 4;
        if (f != frame_) {
            frame_ = f;
            set_texture(lib_, image_, format("images/atlases/compressed.sprites/map/map_circle_%d.tres", f));
        }
        if (t_ >= 1.0) circle_ = done_ = true;
    }

private:
    SceneLibrary* lib_;
    Control* c_;
    Node* image_;
    float scale_;
    bool circle_, done_;
    double t_;
    int frame_;
};

/* Before the point's vote container (MoveChildSafely to its index), at the
 * point's pivot. */
void add_at_pivot(Control* point, Control* fx) {
    Vec2 pivot = point->pivot_offset;
    fx->offset[SIDE_LEFT] += pivot.x;
    fx->offset[SIDE_RIGHT] += pivot.x;
    fx->offset[SIDE_TOP] += pivot.y;
    fx->offset[SIDE_BOTTOM] += pivot.y;
    Node* vote = point->get_node("MapPointVoteContainer");
    if (vote) point->add_child_before(fx, vote);
    else point->add_child(fx);
}
} /* namespace */

void MapScreen::add_circle(Point& p, bool animate) {
    Node* n = instance(lib_, "scenes/vfx/map_circle_vfx.tscn");
    if (!n || !n->is_control()) {
        delete n;
        return;
    }
    Control* c = (Control*)n;
    c->rotation = p.circle_rot * 3.14159265f / 180.0f;
    Node* image = n->get_node("TextureRect");
    if (animate) {
        c->modulate.a = 0;
        set_texture(lib_, image, "images/atlases/compressed.sprites/map/map_circle_0.tres");
        c->behavior = new MapFx(lib_, c, image, p.circle_scale, true);
    } else {
        c->scale = Vec2(p.circle_scale, p.circle_scale);
        c->modulate.a = 0.95f;
        set_texture(lib_, image, "images/atlases/compressed.sprites/map/map_circle_4.tres");
    }
    add_at_pivot(p.node, c);
}

void MapScreen::add_select_vfx(Point& p) {
    Node* n = instance(lib_, "scenes/vfx/map_node_select_vfx.tscn");
    if (!n || !n->is_control()) {
        delete n;
        return;
    }
    Control* c = (Control*)n;
    float s = p.kind == 2 || p.kind == 3 ? 2.0f : p.normal ? 1.0f : 1.5f;
    c->scale = Vec2(s, s);
    vfx_restart(n);
    c->behavior = new MapFx(lib_, c, NULL, s, false);
    add_at_pivot(p.node, c);
}

static const float PI_F = 3.14159265f;

/* NMapScreen.SetMap / CreatePath constants. */
static const float TOTAL_HEIGHT = 2325.0f, TOTAL_WIDTH = 1050.0f;
static const float TICK_DIST = 22.0f;

/* A stable pseudo-random value in [0, 1) for a key (the game uses its
 * run-seeded jitter stream and Rng.Chaotic; the logic sends the seeded
 * jitter when it can, the rest only needs to look random). */
static float hash_unit(unsigned a, unsigned b, unsigned c) {
    unsigned h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA77u ^ (c + 0x165667B1u) * 0xC2B2AE3Du;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    h *= 0x297A2D39u;
    h ^= h >> 15;
    return (float)(h & 0xFFFFFF) / 16777216.0f;
}

static float hash_range(unsigned a, unsigned b, unsigned c, float lo, float hi) {
    return lo + (hi - lo) * hash_unit(a, b, c);
}

/* Rng.NextGaussianFloat(mean, sd), Box-Muller. */
static float hash_gauss(unsigned a, unsigned b, unsigned c, float sd) {
    float u1 = hash_unit(a, b, c * 2 + 1), u2 = hash_unit(a, b, c * 2 + 2);
    if (u1 < 1e-6f) u1 = 1e-6f;
    return sqrtf(-2.0f * logf(u1)) * cosf(2.0f * PI_F * u2) * sd;
}

/* The acts' map colours (ActModel.MapBgColor / MapTraveledColor /
 * MapUntraveledColor), for a logic that does not send them. */
struct ActLook {
    const char* id;
    unsigned bg, traveled, untraveled;
};
static const ActLook ACT_LOOKS[] = { { "overgrowth", 0xA78A67, 0x28231D, 0x877256 },
                                     { "hive", 0x9B9562, 0x27221C, 0x6E7750 },
                                     { "glory", 0x819A97, 0x1D1E2F, 0x60717C },
                                     { "underdocks", 0x9F95A5, 0x180F24, 0x534A62 } };

static const ActLook& act_look(const Json& d) {
    std::string name = lower(d["context"]["act_name"].str());
    size_t dot = name.find('.');
    if (dot != std::string::npos) name = name.substr(0, dot);
    for (size_t i = 0; i < sizeof(ACT_LOOKS) / sizeof(ACT_LOOKS[0]); ++i)
        if (name == ACT_LOOKS[i].id) return ACT_LOOKS[i];
    int act = d["context"]["act"].i(d["act"].i(1));
    return ACT_LOOKS[act >= 1 && act <= 3 ? act - 1 : 0];
}

static Color json_rgba(const Json& c, const Color& fallback) {
    if (c.size() < 3) return fallback;
    return Color((float)c[(size_t)0].num(), (float)c[1].num(), (float)c[2].num(), c.size() > 3 ? (float)c[3].num() : 1.0f);
}

/* Control.Position set before the map has its size (as SetMap runs): with
 * the scenes' centre anchors the point lands at the screen's centre plus
 * the position. */
static void place_from_centre(Control* c, const Vec2& p, const Vec2& size) {
    c->anchor[SIDE_LEFT] = c->anchor[SIDE_RIGHT] = 0.5f;
    c->anchor[SIDE_TOP] = c->anchor[SIDE_BOTTOM] = 0.5f;
    place(c, p.x, p.y, size.x, size.y);
}

/* NNormalMapPoint.IconName (and UnknownIconPath for a visited "?"). */
static std::string icon_name(const std::string& type, const std::string& room) {
    if (type == "Unknown" && !room.empty()) {
        if (room == "Treasure") return "map_unknown_chest";
        if (room == "Monster") return "map_unknown_monster";
        if (room == "Shop") return "map_unknown_shop";
        if (room == "Elite") return "map_unknown_elite";
        return "map_unknown";
    }
    if (type == "Monster") return "map_monster";
    if (type == "Elite") return "map_elite";
    if (type == "Treasure") return "map_chest";
    if (type == "Shop") return "map_shop";
    if (type == "RestSite") return "map_rest";
    return "map_unknown"; /* Unknown, Ancient, Unassigned */
}

static std::string outline_name(const std::string& type, const std::string& room) {
    if (type == "Unknown" && !room.empty()) {
        if (room == "Treasure") return "map_chest";
        if (room == "Monster") return "map_monster";
        if (room == "Shop") return "map_shop";
        if (room == "Elite") return "map_elite";
        return "map_unknown";
    }
    return icon_name(type, room);
}

static Control* as_ctl(Node* n) { return n && n->is_control() ? (Control*)n : NULL; }

/* NMapDrawings' lines for the act (the logic does not keep them): each in
 * the drawing viewport's coordinates (half the map's), drawn or erasing. */
namespace {
struct MapStroke {
    bool erase;
    std::vector<Vec2> points;
};
std::vector<MapStroke> g_strokes;
int g_strokes_act = -1;
}

void MapScreen::clear_drawings() {
    g_strokes.clear();
    g_strokes_act = -1;
}

MapScreen::MapScreen(SceneLibrary* lib) : tools_(NULL), tool_tip_(NULL), tool_focus_(-1), draw_mode_(0), drawings_(NULL),
    draw_vp_(NULL), session_(NULL), cursor_(NULL), cursor_icon_(NULL), pen_(false), pen_wait_(false), line_(NULL), lib_(lib),
    focus_(-1),
    next_tick_(0), current_col_(-1), current_row_(-1),
    opening_(0), travel_t_(-1), marker_(NULL), map_ctl_(NULL), scroll_(0), scroll_to_(0), scroll_lo_(0), scroll_hi_(0),
    scroll_step_(1), act_anim_(-1), act_delay_(1), act_duration_(3), map_ftue_seen_(false), travel_time_(0.8), banner_(NULL),
    banner_end_(0) {
    start_point_ = NULL;
    tool_icons_[0] = tool_icons_[1] = tool_icons_[2] = NULL;
}

/* map_line_draw.tscn / map_line_erase.tscn in the drawing viewport, in the
 * character's MapDrawingColor (CreateLineForPlayer). */
Line2D* MapScreen::add_line(bool erase, const std::vector<Vec2>& points) {
    if (!draw_vp_) return NULL;
    Node* n = instance(lib_, erase ? "scenes/screens/map/map_line_erase.tscn" : "scenes/screens/map/map_line_draw.tscn");
    if (!n || n->type != "Line2D") {
        delete n;
        return NULL;
    }
    Line2D* l = (Line2D*)n;
    l->color = draw_color_;
    l->points = points;
    l->position = Vec2(0, 0);
    draw_vp_->add_child(l);
    return l;
}

/* NMapScreen's drawing tools and NMapDrawings (TheMap/Drawings: a
 * map_drawing.tscn per player, its viewport at half the map's size), the
 * act's lines again. */
void MapScreen::build_drawings(Node* root, const Json& d) {
    int act = d["context"]["act"].i(d["act"].i());
    if (act != g_strokes_act) {
        g_strokes.clear();
        g_strokes_act = act;
    }
    draw_color_ = json_rgba(d["map"]["drawing_color"], hex(0xCB282B));
    tools_ = ctl(root, "DrawingTools");
    static const char* const names[3] = { "DrawButton", "EraseButton", "ClearButton" };
    for (int i = 0; i < 3; ++i) {
        Control* b = tools_ ? ctl(tools_, names[i]) : NULL;
        tool_icons_[i] = b ? as_ctl(b->get_node("Icon")) : NULL;
    }
    show_glyph(lib_, tools_ ? tools_->find_unique("DrawingToolsHotkey") : NULL, "rb");
    drawings_ = ctl(root, "Drawings");
    Node* layer = drawings_ ? instance(lib_, "scenes/screens/map/map_drawing.tscn") : NULL;
    if (layer) {
        drawings_->add_child(layer);
        draw_vp_ = layer->get_node("DrawViewport");
    }
    for (size_t i = 0; i < g_strokes.size(); ++i) add_line(g_strokes[i].erase, g_strokes[i].points);
}

/* NMapDrawButton / NMapEraseButton focused: the icon to 1.2 in its active
 * colour (0.05 s) with its hover tip 132 px above the tools; unfocused
 * back to 1.1, active or half white. */
void MapScreen::focus_tool(int i) {
    static const unsigned active[3] = { 0x57C4FF, 0xFF5757, 0xFFFFFF };
    static const char* const tips[3] = { "DRAWING_BUTTON", "ERASING_BUTTON", "CLEAR_DRAWING" };
    if (tool_tip_) {
        if (tool_tip_->parent) tool_tip_->parent->remove_child(tool_tip_);
        delete tool_tip_;
        tool_tip_ = NULL;
    }
    /* NMapDrawButton & co. (NButton): ui_hover as one takes the focus */
    if (i >= 0 && i != tool_focus_ && lib_->audio()) lib_->audio()->ui("event:/sfx/ui/clicks/ui_hover");
    tool_focus_ = i;
    for (int k = 0; k < 3; ++k) {
        Control* ic = tool_icons_[k];
        if (!ic) continue;
        bool on = k == i || k + 1 == draw_mode_;
        Color c = on ? hex(active[k]) : Color(1, 1, 1, 0.5f);
        float s = k == i ? 1.2f : 1.1f;
        tweens_.tween(&ic->scale, Vec2(s, s), 0.05f);
        tweens_.tween(&ic->self_modulate.r, c.r, 0.05f);
        tweens_.tween(&ic->self_modulate.g, c.g, 0.05f);
        tweens_.tween(&ic->self_modulate.b, c.b, 0.05f);
        tweens_.tween(&ic->self_modulate.a, c.a, 0.05f);
    }
    if (i < 0 || !tools_) return;
    Json list = Json::make_array();
    Json tip;
    std::string key = tips[i];
    tip.set("title", Json::make_string(Loc::get("map", (key + (i < 2 ? ".title_controller" : ".title")).c_str())));
    tip.set("description", Json::make_string(Loc::get("map", (key + ".description").c_str())));
    list.append(tip);
    Node* t = make_tip_set(lib_, list);
    tool_tip_ = t && t->is_control() ? (Control*)t : NULL;
    if (t && !tool_tip_) delete t;
    if (tool_tip_ && tools_->parent) {
        tools_->parent->add_child(tool_tip_);
        tool_tip_->set_position(tools_->global_transform().origin + Vec2(10, -132));
    }
}

/* NMapDrawingInput.Create (a ControllerMapDrawingSession): the cursor at the
 * screen's centre, the quill or eraser (SetDrawingModeLocal) */
void MapScreen::start_drawing(int mode) {
    stop_drawing();
    draw_mode_ = mode;
    session_ = instance(lib_, "scenes/screens/map/controller_map_drawing_input.tscn");
    Node* root = tools_ ? tools_->parent : NULL;
    if (!session_ || !root) {
        delete session_;
        session_ = NULL;
        draw_mode_ = 0;
        return;
    }
    root->add_child(session_);
    cursor_ = ctl(session_, "Cursor");
    cursor_icon_ = cursor_ ? as_ctl(cursor_->get_node("TextureRect")) : NULL;
    pen_ = false;
    /* the button acts on its release (NClickableControl.Released): the A
     * that pressed it draws nothing until it is let go */
    pen_wait_ = true;
    pen(false);
    if (cursor_icon_) {
        /* _drawingIconPos / _eraserIconPos */
        Vec2 at = mode == 1 ? Vec2(-10, -76) : Vec2(-34, -76);
        Vec2 size = offset_size(cursor_icon_);
        place(cursor_icon_, at.x, at.y, size.x, size.y);
    }
    focus_tool(-1);
}

/* the cursor's texture: tilted while the line is drawn */
void MapScreen::pen(bool down) {
    if (!cursor_icon_) return;
    const char* tex = draw_mode_ == 1 ? (down ? "images/packed/common_ui/cursor_quill_tilted.png" : "images/packed/common_ui/cursor_quill.png")
                                      : (down ? "images/packed/common_ui/cursor_eraser_tilted.png" : "images/packed/common_ui/cursor_eraser.png");
    /* the cursors are Image imports, which packs built before the
     * converter packed those lack: the tools' own quill and eraser there */
    Resource* r = lib_->ext(tex);
    if (!r || !r->is_texture()) tex = draw_mode_ == 1 ? "images/packed/map/drawing_quill.png" : "images/packed/map/drawing_eraser.png";
    set_texture(lib_, cursor_icon_, tex);
}

/* NMapDrawingInput.StopDrawing: the line ends, the mode with it */
void MapScreen::stop_drawing() {
    if (pen_ && line_) {
        MapStroke s = { draw_mode_ == 2, line_->points };
        g_strokes.push_back(s);
    }
    pen_ = false;
    line_ = NULL;
    draw_mode_ = 0;
    if (session_) {
        if (session_->parent) session_->parent->remove_child(session_);
        delete session_;
    }
    session_ = NULL;
    cursor_ = cursor_icon_ = NULL;
    cursor_dir_ = Vec2(0, 0);
    focus_tool(tool_focus_);
}

void MapScreen::tools_input(const PadState& pad) {
    if ((pad.pressed & PadState::LEFT) && tool_focus_ > 0) focus_tool(tool_focus_ - 1);
    else if ((pad.pressed & PadState::RIGHT) && tool_focus_ < 2) focus_tool(tool_focus_ + 1);
    else if (pad.pressed & (PadState::B | PadState::RB | PadState::UP)) focus_tool(-1);
    else if (pad.pressed & PadState::A) {
        if (lib_->audio()) lib_->audio()->ui("event:/sfx/ui/clicks/ui_click");
        if (tool_focus_ == 2) {
            /* OnClearMapDrawingButtonPressed: every line of the player's */
            stop_drawing();
            g_strokes.clear();
            while (draw_vp_ && !draw_vp_->children.empty()) {
                Node* c = draw_vp_->children.back();
                draw_vp_->remove_child(c);
                delete c;
            }
            if (lib_->audio()) lib_->audio()->ui("event:/sfx/ui/map/map_erase");
        } else if (draw_mode_ == tool_focus_ + 1) {
            stop_drawing();
        } else {
            start_drawing(tool_focus_ + 1);
        }
    }
}

/* NControllerMapDrawingInput._Process / _Input: A held draws (BeginLineLocal
 * at the cursor, StopLineLocal on release); B stops; RB back to the tools
 * (OnDrawingToolsHotkeyPressed: the focus moves, which stops it). */
void MapScreen::drawing_input(const PadState& pad) {
    if (pad.pressed & PadState::B) {
        stop_drawing();
        focus_tool(-1);
        return;
    }
    if (pad.pressed & PadState::RB) {
        int mode = draw_mode_;
        stop_drawing();
        focus_tool(mode - 1);
        return;
    }
    cursor_dir_ = Vec2((pad.held & PadState::RIGHT ? 1.0f : 0.0f) - (pad.held & PadState::LEFT ? 1.0f : 0.0f),
                       (pad.held & PadState::DOWN ? 1.0f : 0.0f) - (pad.held & PadState::UP ? 1.0f : 0.0f));
    if (cursor_dir_.x != 0 && cursor_dir_.y != 0) cursor_dir_ = cursor_dir_ * 0.70710678f; /* Input.GetVector */
    bool held = (pad.held & PadState::A) != 0;
    if (pen_wait_) {
        pen_wait_ = held;
        return;
    }
    if (held && !pen_ && cursor_ && drawings_) {
        Vec2 at = inverse(drawings_->global_transform()).xform(cursor_->global_transform().origin) * 0.5f;
        std::vector<Vec2> pts;
        pts.push_back(at);
        pts.push_back(at + Vec2(0, 0.5f));
        line_ = add_line(draw_mode_ == 2, pts);
        pen_ = line_ != NULL;
        pen(true);
    } else if (!held && pen_) {
        MapStroke s = { draw_mode_ == 2, line_->points };
        g_strokes.push_back(s);
        pen_ = false;
        line_ = NULL;
        pen(false);
    }
}

Node* MapScreen::build(const Json& d, std::string* error) {
    start_point_ = NULL;
    points_.clear();
    choices_.clear();
    paths_.clear();
    travel_ticks_.clear();
    travel_t_ = -1;
    const Json& m = d["map"];
    if (!m.is_object()) {
        if (error) *error = "map_select without a map";
        return NULL;
    }
    Node* root = instance(lib_, "scenes/screens/map/map_screen.tscn");
    if (!root) {
        if (error) *error = "no map_screen.tscn";
        return NULL;
    }
    const ActLook& fallback = act_look(d);
    const Json& look = m["look"];
    Color bg_color = json_rgba(look["bg_color"], hex(fallback.bg));
    Color traveled = json_rgba(look["traveled_color"], hex(fallback.traveled));
    Color untraveled = json_rgba(look["untraveled_color"], hex(fallback.untraveled));

    /* Open(): the backstop at 0.85, the map and the legend shown. */
    Node* backstop = root->find_unique("Backstop");
    if (backstop && backstop->is_canvas_item()) ((CanvasItem*)backstop)->modulate.a = 0.85f;
    hide(root->get_node("Back"));
    build_drawings(root, d);
    hide(root->find_unique("ShareButton"));
    hide(root->find_unique("ShareToast"));

    /* NMapBg: the act's three pictures, at y -1620 (16:9). */
    Node* bgbox = root->find_unique("MapBg");
    if (bgbox && bgbox->is_control()) {
        Control* b = (Control*)bgbox;
        b->offset[SIDE_TOP] = -1620;
        b->offset[SIDE_BOTTOM] = -1620 + 3240;
        static const char* const parts[3] = { "MapTop", "MapMid", "MapBot" };
        std::string id = fallback.id;
        const char* const names[3] = { "top", "middle", "bottom" };
        for (int i = 0; i < 3; ++i) {
            std::string path = look["bg"].size() == 3 ? look["bg"][(size_t)i].str()
                                                      : "images/packed/map/map_bgs/" + id + "/map_" + names[i] + "_" + id + ".png";
            set_texture(lib_, b->get_node(parts[i]), path);
        }
    }

    /* The legend: its texts (NMapLegendItem) at x = width * 0.8, Y to open it. */
    Node* legend = root->find_unique("MapLegend");
    if (legend && legend->is_control()) {
        Control* lc = (Control*)legend;
        float w = offset_size(lc).x;
        lc->offset[SIDE_LEFT] = 1920.0f * 0.8f - 960.0f;
        lc->offset[SIDE_RIGHT] = lc->offset[SIDE_LEFT] + w;
        set_label(legend->get_node("Header"), Loc::get("map", "LEGEND_HEADER"));
        static const struct {
            const char* node;
            const char* key;
        } items[] = { { "UnknownLegendItem", "LEGEND_UNKNOWN" }, { "MerchantLegendItem", "LEGEND_MERCHANT" },
                      { "TreasureLegendItem", "LEGEND_TREASURE" }, { "RestSiteLegendItem", "LEGEND_REST" },
                      { "EnemyLegendItem", "LEGEND_ENEMY" }, { "EliteLegendItem", "LEGEND_ELITE" } };
        for (size_t i = 0; i < sizeof(items) / sizeof(items[0]); ++i) {
            Node* it = legend->get_node(std::string("LegendItems/") + items[i].node);
            if (it) set_label(it->get_node("MegaLabel"), Loc::get("map", (std::string(items[i].key) + ".title").c_str()));
        }
        show_glyph(lib_, legend->find_unique("LegendHotkeyIcon"), "y");
    }

    Node* points_box = root->get_node("TheMap/Points");
    Node* paths_box = root->get_node("TheMap/Paths");
    Node* the_map = root->get_node("TheMap");
    if (!points_box || !paths_box || !the_map || !the_map->is_control()) {
        if (error) *error = "map_screen.tscn: no TheMap/Points or Paths";
        delete root;
        return NULL;
    }
    int cols = m["cols"].i(7), rows = m["rows"].i(16);
    bool second_boss = m["second_boss"].is_object();
    float f = second_boss ? 0.9f : 1.0f;
    float dist_y = TOTAL_HEIGHT / (float)(rows > 1 ? rows - 1 : 1) * f;
    float dist_x = TOTAL_WIDTH / (float)(cols > 0 ? cols : 1);

    /* The scroll: the current row's distance up from -600. */
    const Json& cur = m["current"];
    int cur_row = cur.size() == 2 ? cur[1].i() : 0;
    current_col_ = cur.size() == 2 ? cur[(size_t)0].i() : -1;
    current_row_ = cur_row;
    Control* map_ctl = (Control*)the_map;
    map_ctl->offset[SIDE_TOP] = map_ctl->offset[SIDE_BOTTOM] = -600.0f + (float)cur_row * dist_y;
    map_ctl_ = map_ctl;
    scroll_ = scroll_to_ = -600.0f + (float)cur_row * dist_y;
    scroll_lo_ = -600.0f;
    scroll_hi_ = -600.0f + (float)(rows > 1 ? rows - 1 : 0) * dist_y;
    scroll_step_ = dist_y * 2.0f;

    /* Visited coordinates (traveled), choices (travelable). */
    std::vector<std::pair<int, int> > visited;
    for (size_t i = 0; i < m["visited"].size(); ++i)
        visited.push_back(std::make_pair(m["visited"][i][(size_t)0].i(), m["visited"][i][1].i()));
    const Json& choices = d["choices"];

    /* Every point: normal ones, the start (ancient or normal), the boss(es). */
    std::vector<Placed> placed;
    std::string ancient = lower(d["context"]["ancient"].is_string() ? d["context"]["ancient"].str() : std::string("neow"));
    unsigned act = (unsigned)d["context"]["act"].i(1);
    const Json& pts = m["points"];
    for (size_t i = 0; i <= pts.size() + 2; ++i) {
        const Json* p = NULL;
        int kind = 0; /* 0 normal, 1 start, 2 boss, 3 second boss */
        if (i < pts.size()) p = &pts[i];
        else if (i == pts.size()) { p = &m["start"]; kind = 1; }
        else if (i == pts.size() + 1) { p = &m["boss"]; kind = 2; }
        else if (second_boss) { p = &m["second_boss"]; kind = 3; }
        if (!p || !p->is_object()) continue;
        int col = (*p)["col"].i(), row = (*p)["row"].i();
        const std::string& type = (*p)["type"].str();
        Node* n = NULL;
        Vec2 pos, size;
        if (kind == 0 || (kind == 1 && type != "Ancient")) {
            n = instance(lib_, "scenes/ui/normal_map_point.tscn");
            size = Vec2(56, 56);
            if (kind == 0) {
                const Json& j = (*p)["jitter"];
                float jx = j.size() == 2 ? (float)j[(size_t)0].num() : hash_range(act, col, row * 2, -21, 21);
                float jy = j.size() == 2 ? (float)j[1].num() : hash_range(act, col, row * 2 + 1, -25, 25);
                pos = Vec2((float)col * dist_x - 500.0f + jx, 740.0f - (float)row * dist_y + jy);
            } else {
                pos = Vec2(-80.0f, (float)row * -dist_y + 800.0f);
            }
        } else if (kind == 1) {
            n = instance(lib_, "scenes/ui/ancient_map_point.tscn");
            size = Vec2(208, 208);
            pos = Vec2(-80.0f, (float)row * -dist_y + 720.0f);
        } else {
            n = instance(lib_, "scenes/ui/boss_map_point.tscn");
            size = Vec2(374, 306);
            pos = Vec2(-200.0f, (kind == 2 ? -1980.0f : -2280.0f) * f);
        }
        if (!n || !n->is_control()) {
            delete n;
            continue;
        }
        Control* c = (Control*)n;
        place_from_centre(c, pos, size);
        if (kind == 1) start_point_ = c;
        Placed pl;
        pl.data = p;
        pl.pos = pos;
        pl.size = size;
        /* GetLineEndpoint: a normal point's position, others' centre. */
        pl.endpoint = n->type == "Control" && size.x == 56 ? pos : pos + size * 0.5f;
        pl.node = c;
        placed.push_back(pl);

        Point pt;
        pt.col = col;
        pt.row = row;
        pt.node = c;
        pt.icon_container = ctl(n, "IconContainer");
        pt.icon = ctl(n, "Icon");
        if (!pt.icon) {
            Node* ic = n->get_node("Icon");
            pt.icon = ic && ic->is_control() ? (Control*)ic : NULL;
        }
        pt.reticle = ctl(n, "SelectionReticle");
        if (pt.reticle) {
            pt.reticle->modulate.a = 0; /* NSelectionReticle._Ready */
            pt.reticle->pivot_offset = offset_size(pt.reticle) * 0.5f;
        }
        pt.choice = false;
        pt.kind = kind;
        pt.normal = kind == 0 || (kind == 1 && type != "Ancient");
        pt.circle_rot = (*p)["circle"].size() == 2 ? (*p)["circle"][(size_t)0].f() : 0;
        pt.circle_scale = (*p)["circle"].size() == 2 ? (*p)["circle"][1].f() : 0.875f;
        for (size_t k = 0; k < choices.size(); ++k)
            if (choices[k]["col"].i() == col && choices[k]["row"].i() == row) pt.choice = true;
        pt.elapsed = hash_range(act, col, row, 0, 3140);
        bool was_visited = false;
        for (size_t k = 0; k < visited.size(); ++k)
            if (visited[k].first == col && visited[k].second == row) was_visited = true;
        if (kind == 0 || (kind == 1 && type != "Ancient")) {
            /* NNormalMapPoint._Ready: icon, outline in the act's colour, the
             * state's colour (traveled / travelable: white, others: half),
             * a random tilt. */
            const std::string& room = (*p)["room"].is_string() ? (*p)["room"].str() : std::string();
            std::string name = icon_name(type, was_visited ? room : std::string());
            set_texture(lib_, pt.icon, "images/atlases/ui_atlas.sprites/map/icons/" + name + ".tres");
            Node* outline = n->find_unique("Outline");
            set_texture(lib_, outline, "images/atlases/compressed.sprites/map/" +
                                           outline_name(type, was_visited ? room : std::string()) + "_outline.tres");
            if (outline && outline->is_canvas_item()) ((CanvasItem*)outline)->modulate = bg_color;
            if (pt.icon) {
                pt.icon->self_modulate = was_visited || pt.choice ? Color(1, 1, 1, 1) : Color(1, 1, 1, 0.5f);
                /* map_color: the act's colour halfway to Colors.Gray. */
                pt.icon->shader_params["map_color.r"] = (bg_color.r + 0.745098f) * 0.5f;
                pt.icon->shader_params["map_color.g"] = (bg_color.g + 0.745098f) * 0.5f;
                pt.icon->shader_params["map_color.b"] = (bg_color.b + 0.745098f) * 0.5f;
            }
            if (pt.icon_container) pt.icon_container->rotation = hash_gauss(act, col, row, 8.0f) * PI_F / 180.0f;
            /* RefreshMarkedIconVisibility: a quest marks the point (Spoils
             * Map, Fur Coat) */
            set_visible(n->find_unique("QuestIcon"), (*p)["quest"].b());
            /* RefreshState: a traveled point shows its circle */
            if (was_visited) add_circle(pt, false);
        } else if (kind == 1) {
            /* NAncientMapPoint: the act's ancient's node picture. */
            set_texture(lib_, pt.icon, "images/packed/map/ancients/ancient_node_" + ancient + ".png");
            Node* outline = pt.icon ? pt.icon->get_node("Outline") : NULL;
            set_texture(lib_, outline, "images/packed/map/ancients/ancient_node_" + ancient + "_outline.png");
            if (outline && outline->is_canvas_item()) ((CanvasItem*)outline)->modulate = bg_color;
            if (pt.icon) pt.icon->self_modulate = was_visited || pt.choice ? hex(0x241F1A) : Color(0x7D / 255.0f, 0x6A / 255.0f, 0x55 / 255.0f, 0xD8 / 255.0f);
        } else {
            if (kind == 3 || second_boss) c->scale = Vec2(0.75f, 0.75f);
            /* NBossMapPoint._Ready: the boss's node, a skeleton playing
             * "animation" (boss_map_point: its black layer in the traveled
             * or untraveled colour, map_color the act's), or the
             * placeholder picture (that colour) and its outline (the
             * act's). */
            bool lit = was_visited || pt.choice;
            const std::string& path = look[kind == 2 ? "boss_node" : "second_boss_node"].str();
            Node* sn = n->find_unique("SpineSprite");
            Node* img = n->find_unique("PlaceholderImage");
            Node* outline = n->find_unique("PlaceholderOutline");
            Resource* r = path.empty() ? NULL : lib_->ext(path);
            Color layer = lit ? traveled : untraveled;
            if (r && r->kind == Resource::RES_SPINE_DATA && sn && sn->type == "SpineSprite") {
                SpineSprite* s = (SpineSprite*)sn;
                s->set_data(lib_, (SpineDataRes*)r);
                s->add_animation("animation", true, 0, 0);
                s->shader_params["black_layer_color.r"] = layer.r;
                s->shader_params["black_layer_color.g"] = layer.g;
                s->shader_params["black_layer_color.b"] = layer.b;
                s->shader_params["map_color.r"] = bg_color.r;
                s->shader_params["map_color.g"] = bg_color.g;
                s->shader_params["map_color.b"] = bg_color.b;
                set_visible(sn, true);
                set_visible(img, false);
            } else if (!path.empty()) {
                set_visible(sn, false);
                set_texture(lib_, img, path + ".png");
                set_texture(lib_, outline, path + "_outline.png");
                if (img && img->is_canvas_item()) ((CanvasItem*)img)->self_modulate = layer;
                if (outline && outline->is_canvas_item()) ((CanvasItem*)outline)->self_modulate = bg_color;
                set_visible(img, true);
            }
        }
        hide(n->find_unique("MapPointVoteContainer"));
        points_box->add_child(n);
        points_.push_back(pt);
    }

    /* The paths: a dot every 22 px from each point to its children (none
     * at either end), 3 px of jitter, the line's angle +- a little; the
     * act's untraveled colour, or traveled (and 1.2 scale) along the way
     * taken. */
    for (size_t i = 0; i < placed.size(); ++i) {
        const Json& kids = (*placed[i].data)["children"];
        int pc = (*placed[i].data)["col"].i(), pr = (*placed[i].data)["row"].i();
        for (size_t k = 0; k < kids.size(); ++k) {
            int cc = kids[k][(size_t)0].i(), cr = kids[k][1].i();
            const Placed* child = NULL;
            for (size_t j = 0; j < placed.size(); ++j)
                if ((*placed[j].data)["col"].i() == cc && (*placed[j].data)["row"].i() == cr) child = &placed[j];
            if (!child) continue;
            bool walked = false;
            for (size_t v = 0; v + 1 < visited.size(); ++v)
                if (visited[v].first == pc && visited[v].second == pr && visited[v + 1].first == cc && visited[v + 1].second == cr)
                    walked = true;
            Vec2 a = placed[i].endpoint, b = child->endpoint, dir = b - a;
            float len = sqrtf(dir.x * dir.x + dir.y * dir.y);
            if (len <= 0) continue;
            dir = dir * (1.0f / len);
            float angle = atan2f(dir.y, dir.x) + PI_F / 2;
            int n = (int)(len / TICK_DIST) + 1;
            Path path;
            path.from_col = pc; path.from_row = pr; path.to_col = cc; path.to_row = cr;
            for (int t = 1; t < n; ++t) {
                Node* dot = instance(lib_, "scenes/ui/map_dot.tscn");
                if (!dot || dot->type != "TextureRect") {
                    delete dot;
                    break;
                }
                TextureRect* r = (TextureRect*)dot;
                unsigned key = (unsigned)(pc * 131 + pr * 17 + cc * 7919 + cr * 104729);
                /* - (Size / 2 - 20): the screen's Size is still zero. */
                Vec2 at = a + dir * ((float)t * TICK_DIST) + Vec2(20, 20) +
                          Vec2(hash_range(act, key, t * 3, -3, 3), hash_range(act, key, t * 3 + 1, -3, 3));
                place_from_centre(r, at, Vec2(16, 16));
                r->flip_h = hash_unit(act, key, t * 3 + 2) < 0.5f;
                r->rotation = angle + hash_gauss(act, key, t + 1000, 0.1f);
                r->pivot_offset = Vec2(8, 8);
                r->modulate = walked ? traveled : untraveled;
                if (walked) r->scale = Vec2(1.2f, 1.2f);
                paths_box->add_child(dot);
                path.ticks.push_back(r);
            }
            paths_.push_back(path);
        }
    }

    /* NMapMarker on the last room visited (not on the start or boss rows):
     * the character's marker over the point's top centre. On the start
     * point too once RunManager.EnterAct's InitMarker put it there (the
     * first act, begun with Neow). */
    Node* marker = root->get_node("TheMap/MapMarker");
    marker_ = marker && marker->is_control() ? (Control*)marker : NULL;
    if (!visited.empty() && marker && marker->type == "TextureRect") {
        int vc = visited.back().first, vr = visited.back().second;
        bool at_start = vr == m["start"]["row"].i();
        for (size_t i = 0; i < placed.size(); ++i) {
            const Json& pd = *placed[i].data;
            if (pd["col"].i() != vc || pd["row"].i() != vr || pd["type"].str() == "Boss" || (at_start && !m["marker_at_start"].b()))
                continue;
            TextureRect* r = (TextureRect*)marker;
            std::string character =
                lower(d["player"]["character"].is_string() ? d["player"]["character"].str() : std::string("ironclad"));
            set_texture(lib_, r, "images/packed/map/icons/map_marker_" + character + ".png");
            place_from_centre(r, placed[i].pos + Vec2(placed[i].size.x * 0.5f - 20.0f, -35.0f), Vec2(40, 40));
            r->visible = r->texture != NULL;
            break;
        }
    }

    /* The choices left to right; the focus starts on the first. */
    for (size_t i = 0; i < points_.size(); ++i)
        if (points_[i].choice) choices_.push_back((int)i);
    for (size_t a = 0; a < choices_.size(); ++a)
        for (size_t b = a + 1; b < choices_.size(); ++b)
            if (points_[(size_t)choices_[b]].col < points_[(size_t)choices_[a]].col) std::swap(choices_[a], choices_[b]);
    root->add_child(frame_driver(this));
    focus_ = -1;
    if (!choices_.empty()) focus(0);
    tweens_.update(1.0); /* at rest */
    /* NMapScreen.Open: map/backstop in 0.25 s, points and legend delayed
     * 0.1 s; legend slides in from 120 px to the right. */
    opening_ = 0;
    map_ctl->modulate.a = 0;
    tweens_.tween(&map_ctl->modulate.a, 1, 0.25f);
    if (backstop && backstop->is_canvas_item()) {
        CanvasItem* b = (CanvasItem*)backstop;
        b->modulate.a = 0; tweens_.tween(&b->modulate.a, 0.85f, 0.25f);
    }
    if (points_box->is_canvas_item()) {
        CanvasItem* p = (CanvasItem*)points_box;
        p->modulate.a = 0; tweens_.tween(&p->modulate.a, 1, 0.25f, TRANS_LINEAR, EASE_IN_OUT, 0.1f);
    }
    if (legend && legend->is_control()) {
        Control* l = (Control*)legend;
        l->modulate.a = 0;
        tweens_.tween(&l->modulate.a, 1, 0.25f, TRANS_LINEAR, EASE_IN_OUT, 0.1f);
        tweens_.tween_from(&l->offset[SIDE_LEFT], l->offset[SIDE_LEFT] + 120, l->offset[SIDE_LEFT], 0.25f, TRANS_BACK, EASE_OUT, 0.1f);
        tweens_.tween_from(&l->offset[SIDE_RIGHT], l->offset[SIDE_RIGHT] + 120, l->offset[SIDE_RIGHT], 0.25f, TRANS_BACK, EASE_OUT, 0.1f);
    }

    /* The act's first map (not one opened from the top bar): the map
     * rises in (StartOfActAnim) under NActBanner; in Fast mode, once the
     * map's FTUE was seen, it is at the act's start at once under the
     * banner. The first act begins after Neow, at floor 1. */
    act_anim_ = -1;
    banner_ = NULL;
    bool view = d["type"].str() == "map_view";
    bool neow = d["act"].i(1) == 1 && d["context"]["ancient"].str() == "NEOW";
    if (!view && d["floor"].i(-1) == (neow ? 1 : 0)) {
        /* _mapAnimStartDelay / _mapAnimDuration; NActBanner's hold */
        bool fast = fast_mode();
        act_delay_ = fast ? 0.5 : 1.0;
        act_duration_ = fast ? 1.5 : 3.0;
        float hold = fast ? 0.5f : 2.0f;
        bool rise = !fast || !map_ftue_seen_;
        act_anim_ = rise ? 0 : -1;
        scroll_ = scroll_to_ = rise ? 1800 : -600;
        map_ctl->offset[SIDE_TOP] = map_ctl->offset[SIDE_BOTTOM] = scroll_;
        Node* banner = instance(lib_, "scenes/ui/act_banner.tscn");
        if (banner) {
            root->add_child(banner);
            banner_ = banner;
            banner_end_ = 1.75 + hold + 1.25; /* its fade ends at 1.75 + hold + 1 */
            /* NActBanner.AnimateVfx */
            std::string number = Loc::get("gameplay_ui", "ACT_NUMBER");
            size_t at = number.find("{actNumber}");
            if (at != std::string::npos) number.replace(at, 11, format("%d", d["act"].i(1)));
            Node* num = banner->get_node("ActNumber");
            Node* name = banner->get_node("ActName");
            Node* band = banner->find_unique("Banner");
            set_label(num, number);
            set_label(name, d["act_name"].str());
            if (band && band->is_canvas_item()) {
                ((CanvasItem*)band)->modulate = Color(0, 0, 0, 0);
                tweens_.tween(&((CanvasItem*)band)->modulate.a, 0.25f, 0.5f, TRANS_LINEAR, EASE_IN_OUT, 0.5f);
            }
            if (name && name->is_canvas_item()) {
                ((CanvasItem*)name)->modulate = Color(1, 1, 1, 0);
                tweens_.tween(&((CanvasItem*)name)->modulate.a, 1.0f, 1.0f, TRANS_LINEAR, EASE_IN_OUT, 0.25f);
            }
            if (num && num->is_control()) {
                Control* nc = (Control*)num;
                nc->modulate = Color(1, 1, 1, 0);
                tweens_.tween(&nc->modulate.a, 1.0f, 1.0f, TRANS_LINEAR, EASE_IN_OUT, 0.5f);
                /* position:y 450 -> 440 (its anchors at the middle) */
                float h = nc->offset[SIDE_BOTTOM] - nc->offset[SIDE_TOP];
                float top = 450.0f - 540.0f;
                nc->offset[SIDE_TOP] = top;
                nc->offset[SIDE_BOTTOM] = top + h;
                tweens_.tween(&nc->offset[SIDE_TOP], top - 10, 1.25f, TRANS_QUAD, EASE_OUT, 0.5f);
                tweens_.tween(&nc->offset[SIDE_BOTTOM], top - 10 + h, 1.25f, TRANS_QUAD, EASE_OUT, 0.5f);
            }
            if (banner->is_canvas_item())
                tweens_.tween(&((CanvasItem*)banner)->modulate.a, 0.0f, 1.0f, TRANS_QUAD, EASE_OUT, 1.75f + hold);
        }
    }
    return root;
}

/* NNormalMapPoint / NAncientMapPoint / NBossMapPoint.HoverScale */
static float hover_scale(int kind, bool normal) {
    if (kind == 2 || kind == 3) return 1.05f;
    return normal ? 1.45f : 1.1f;
}

/* The icon scales about its pivot: the scene's (the ancient's Icon: its
 * middle, 100, 100), else its middle (a filled icon has no offsets). */
static void centre_pivot(Control* icon) {
    if (icon->pivot_offset.x != 0 || icon->pivot_offset.y != 0) return;
    Vec2 s = icon->size.x > 0 && icon->size.y > 0 ? icon->size : offset_size(icon);
    icon->pivot_offset = s * 0.5f;
}

/* NMapPoint.OnFocus with a pad: the reticle (NSelectionReticle.OnSelect) and
 * the icon at HoverScale; the one left goes back. */
void MapScreen::focus(int i) {
    if (focus_ >= 0 && focus_ < (int)choices_.size() && focus_ != i) {
        Point& old = points_[(size_t)choices_[(size_t)focus_]];
        if (old.reticle) {
            tweens_.tween(&old.reticle->modulate.a, 0.0f, 0.2f, TRANS_SINE, EASE_OUT);
            tweens_.tween(&old.reticle->scale, Vec2(1.05f, 1.05f), 0.2f, TRANS_SINE, EASE_OUT);
        }
        if (old.icon) tweens_.tween(&old.icon->scale, Vec2(1, 1), 0.5f, old.normal ? TRANS_EXPO : TRANS_CUBIC, EASE_OUT);
        old.elapsed = 3.926991f; /* OnUnfocus */
    }
    focus_ = i;
    Point& p = points_[(size_t)choices_[(size_t)i]];
    if (p.reticle) {
        tweens_.kill(&p.reticle->modulate.a);
        p.reticle->modulate.a = 1;
        tweens_.tween_from(&p.reticle->scale, Vec2(0.9f, 0.9f), Vec2(1, 1), 0.5f, TRANS_EXPO, EASE_OUT);
    }
    if (p.icon) {
        centre_pivot(p.icon);
        float s = hover_scale(p.kind, p.normal);
        tweens_.tween(&p.icon->scale, Vec2(s, s), 0.05f);
    }
}

std::string MapScreen::input(const PadState& pad) {
    if (busy()) return std::string();
    /* past a quarter of it, input ends the rise (and still counts) */
    if (act_anim_ >= 0 && pad.pressed) act_anim_ = act_delay_ + act_duration_;
    if (draw_mode_ != 0) {
        drawing_input(pad);
        return std::string();
    }
    if (tool_focus_ >= 0) {
        tools_input(pad);
        return std::string();
    }
    if ((pad.pressed & PadState::RB) && tools_) {
        /* OnDrawingToolsHotkeyPressed: the draw button (the eraser's while
         * erasing) */
        focus_tool(0);
        return std::string();
    }
    if (pad.pressed & PadState::UP) scroll_to_ = minf(scroll_to_ + scroll_step_, scroll_hi_);
    if (pad.pressed & PadState::DOWN) scroll_to_ = maxf(scroll_to_ - scroll_step_, scroll_lo_);
    int n = (int)choices_.size();
    if (n == 0) return std::string();
    if ((pad.pressed & PadState::LEFT) && focus_ > 0) focus(focus_ - 1);
    if ((pad.pressed & PadState::RIGHT) && focus_ + 1 < n) focus(focus_ + 1);
    if (pad.pressed & PadState::A) {
        /* NMapPoint: NButton's ui_click on the press (its OnFocus plays
         * nothing) */
        if (lib_->audio()) lib_->audio()->ui("event:/sfx/ui/clicks/ui_click");
        const Point& p = points_[(size_t)choices_[(size_t)focus_]];
        return protocol_action("select_map_node", format("\"col\":%d,\"row\":%d", p.col, p.row));
    }
    return std::string();
}

void MapScreen::begin_travel(int col, int row) {
    if (travel_t_ >= 0) return;
    travel_t_ = 0;
    travel_time_ = fast_mode() ? 0.3 : 0.8;
    next_tick_ = 0;
    travel_ticks_.clear();
    for (size_t i = 0; i < paths_.size(); ++i)
        if (paths_[i].from_col == current_col_ && paths_[i].from_row == current_row_ &&
            paths_[i].to_col == col && paths_[i].to_row == row) travel_ticks_ = paths_[i].ticks;
    if (marker_) tweens_.tween(&marker_->scale, Vec2(0, 0), 0.2f);
    /* TravelToMapCoord: wipe_map with the fade out */
    if (lib_->audio()) lib_->audio()->ui("event:/sfx/ui/wipe_map");
    for (size_t i = 0; i < points_.size(); ++i) if (points_[i].col == col && points_[i].row == row) {
        Point& p = points_[i];
        /* OnSelected (ShowCircleVfx with its animation), NMapNodeSelectVfx */
        if (p.normal) add_circle(p, true);
        add_select_vfx(p);
        marker_to_ = offset_pos(p.node) + Vec2(offset_size(p.node).x * 0.5f - 20, -35);
        if (p.icon && p.normal) {
            p.icon->self_modulate = Color();
            tweens_.tween(&p.icon->scale, Vec2(1.5f, 1.5f), 0.2f, TRANS_EXPO, EASE_OUT);
        } else if (p.icon && p.kind == 1) {
            /* NAncientMapPoint.OnSelected: back to its size (0.3 s, Back Out) */
            centre_pivot(p.icon);
            tweens_.tween(&p.icon->scale, Vec2(1, 1), 0.3f, TRANS_BACK, EASE_OUT);
        }
    }
}

/* NNormalMapPoint._Process: a travelable point's icon pulses
 * (sin(t) * 0.25 + 1.2, t += 4 dt) unless it has the focus. */
void MapScreen::frame(double dt) {
    tweens_.update(dt);
    opening_ += dt;
    if (cursor_ && (cursor_dir_.x != 0 || cursor_dir_.y != 0)) {
        /* 700 px a second, kept on the screen; the line follows (points
         * 2 px apart at least: UpdateCurrentLinePosition) */
        Vec2 at = cursor_->global_transform().origin + cursor_dir_ * (700.0f * (float)dt);
        at = Vec2(clampf(at.x, 0, 1920), clampf(at.y, 0, 1080));
        Node* p = cursor_->parent;
        Vec2 local = p && p->is_canvas_item() ? inverse(((CanvasItem*)p)->global_transform()).xform(at) : at;
        cursor_->set_position(local);
        if (pen_ && line_ && drawings_) {
            Vec2 v = inverse(drawings_->global_transform()).xform(at) * 0.5f;
            Vec2 last = line_->points.empty() ? v : line_->points.back();
            Vec2 dd = v - last;
            if (dd.x * dd.x + dd.y * dd.y >= 4.0f) line_->points.push_back(v);
        }
    }
    if (act_anim_ >= 0) {
        act_anim_ += dt;
        float u = clampf((float)((act_anim_ - act_delay_) / act_duration_), 0, 1);
        scroll_ = scroll_to_ = 1800.0f + (-600.0f - 1800.0f) * tween_interpolate(TRANS_EXPO, EASE_IN_OUT, u, 0, 1, 1);
        if (map_ctl_) map_ctl_->offset[SIDE_TOP] = map_ctl_->offset[SIDE_BOTTOM] = scroll_;
        if (u >= 1) act_anim_ = -1;
    }
    if (banner_ && opening_ >= banner_end_) {
        if (banner_->parent) banner_->parent->remove_child(banner_);
        delete banner_;
        banner_ = NULL;
    }
    if (map_ctl_ && act_anim_ < 0 && fabsf(scroll_ - scroll_to_) > 0.1f) {
        float k = (float)dt * 10.0f;
        scroll_ += (scroll_to_ - scroll_) * (k > 1 ? 1 : k);
        if (fabsf(scroll_ - scroll_to_) < 0.5f) scroll_ = scroll_to_;
        map_ctl_->offset[SIDE_TOP] = map_ctl_->offset[SIDE_BOTTOM] = scroll_;
    }
    if (travel_t_ >= 0) {
        double before = travel_t_;
        travel_t_ += dt;
        while (next_tick_ < travel_ticks_.size() && travel_t_ >= travel_time_ * (double)(next_tick_ + 1) / (double)travel_ticks_.size()) {
            Control* tick = travel_ticks_[next_tick_++];
            tick->modulate = hex(0x241F1A); /* StsColors.pathDotTraveled */
            tweens_.tween_from(&tick->scale, Vec2(1.7f, 1.7f), Vec2(1.2f, 1.2f), 0.4f, TRANS_CUBIC, EASE_OUT);
        }
        if (marker_ && before < travel_time_ && travel_t_ >= travel_time_) {
            marker_->visible = true;
            place_from_centre(marker_, marker_to_, Vec2(40, 40));
            tweens_.tween_from(&marker_->scale, Vec2(0, 1), Vec2(1, 1), 0.2f);
            tweens_.tween(&marker_->offset[SIDE_TOP], marker_to_.y - 25, 0.2f, TRANS_SINE, EASE_IN);
            tweens_.tween(&marker_->offset[SIDE_BOTTOM], marker_to_.y + 15, 0.2f, TRANS_SINE, EASE_IN);
            tweens_.tween(&marker_->offset[SIDE_TOP], marker_to_.y, 0.75f, TRANS_ELASTIC, EASE_OUT, 0.2f);
            tweens_.tween(&marker_->offset[SIDE_BOTTOM], marker_to_.y + 40, 0.75f, TRANS_ELASTIC, EASE_OUT, 0.2f);
        }
    }
    for (size_t k = 0; k < choices_.size(); ++k) {
        Point& p = points_[(size_t)choices_[k]];
        Control* target = p.icon_container ? p.icon_container : p.node;
        float base = p.icon_container ? 1.2f : 1.0f, amount = p.icon_container ? 0.25f : 0.05f;
        if ((int)k != focus_) {
            p.elapsed += (float)dt * 4.0f;
            float s = sinf(p.elapsed) * amount + base;
            target->scale = Vec2(s, s);
        } else {
            target->scale = target->scale + (Vec2(1, 1) - target->scale) * 0.5f;
        }
        if (target->pivot_offset.x == 0 && target->pivot_offset.y == 0)
            target->pivot_offset = offset_size(target) * 0.5f;
    }
}

} /* namespace spire */
