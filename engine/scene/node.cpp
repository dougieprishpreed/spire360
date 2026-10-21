#include "node.h"

#include "scene.h"
#include "script.h"
#include "value.h"

#include <math.h>
#include <string.h>

namespace spire {

/* Set by SceneLibrary: Godot's default theme (the last place theme lookups go). */
ThemeRes* g_default_theme = NULL;
FontRes* g_default_font = NULL;

Resource* PropCtx::resource(const Json& v) const { return lib ? lib->resolve(v, file) : NULL; }

/* ---- class hierarchy ---- */

static const char* const CLASS_PARENTS[][2] = {
    { "Node", "Object" },
    { "CanvasItem", "Node" },
    { "CanvasLayer", "Node" },
    { "Node2D", "CanvasItem" },
    { "Control", "CanvasItem" },
    { "Container", "Control" },
    { "BoxContainer", "Container" },
    { "HBoxContainer", "BoxContainer" },
    { "VBoxContainer", "BoxContainer" },
    { "MarginContainer", "Container" },
    { "PanelContainer", "Container" },
    { "CenterContainer", "Container" },
    { "GridContainer", "Container" },
    { "FlowContainer", "Container" },
    { "HFlowContainer", "FlowContainer" },
    { "VFlowContainer", "FlowContainer" },
    { "ScrollContainer", "Container" },
    { "AspectRatioContainer", "Container" },
    { "SplitContainer", "Container" },
    { "HSplitContainer", "SplitContainer" },
    { "VSplitContainer", "SplitContainer" },
    { "SubViewportContainer", "Container" },
    { "TabContainer", "Container" },
    { "GraphElement", "Container" },
    { "Label", "Control" },
    { "RichTextLabel", "Control" },
    { "TextureRect", "Control" },
    { "NinePatchRect", "Control" },
    { "ColorRect", "Control" },
    { "Panel", "Control" },
    { "ReferenceRect", "Control" },
    { "VideoStreamPlayer", "Control" },
    { "Range", "Control" },
    { "ProgressBar", "Range" },
    { "TextureProgressBar", "Range" },
    { "ScrollBar", "Range" },
    { "HScrollBar", "ScrollBar" },
    { "VScrollBar", "ScrollBar" },
    { "Slider", "Range" },
    { "HSlider", "Slider" },
    { "VSlider", "Slider" },
    { "SpinBox", "Range" },
    { "BaseButton", "Control" },
    { "Button", "BaseButton" },
    { "TextureButton", "BaseButton" },
    { "LinkButton", "BaseButton" },
    { "CheckBox", "Button" },
    { "CheckButton", "Button" },
    { "MenuButton", "Button" },
    { "OptionButton", "Button" },
    { "ColorPickerButton", "Button" },
    { "LineEdit", "Control" },
    { "TextEdit", "Control" },
    { "CodeEdit", "TextEdit" },
    { "ItemList", "Control" },
    { "Tree", "Control" },
    { "TabBar", "Control" },
    { "Separator", "Control" },
    { "HSeparator", "Separator" },
    { "VSeparator", "Separator" },
    { "Sprite2D", "Node2D" },
    { "AnimatedSprite2D", "Node2D" },
    { "Marker2D", "Node2D" },
    { "CanvasGroup", "Node2D" },
    { "Line2D", "Node2D" },
    { "Polygon2D", "Node2D" },
    { "MeshInstance2D", "Node2D" },
    { "MultiMeshInstance2D", "Node2D" },
    { "Path2D", "Node2D" },
    { "PathFollow2D", "Node2D" },
    { "RemoteTransform2D", "Node2D" },
    { "GPUParticles2D", "Node2D" },
    { "CPUParticles2D", "Node2D" },
    { "Camera2D", "Node2D" },
    { "Light2D", "Node2D" },
    { "PointLight2D", "Light2D" },
    { "DirectionalLight2D", "Light2D" },
    { "LightOccluder2D", "Node2D" },
    /* spine-godot's slot meshes, saved empty in some scenes (the runtime
     * makes its own): nothing to draw */
    { "SpineMesh2D", "Node2D" },
    { "BackBufferCopy", "Node2D" },
    { "VisibleOnScreenNotifier2D", "Node2D" },
    { "Parallax2D", "Node2D" },
    { "ParallaxBackground", "CanvasLayer" },
    { "ParallaxLayer", "Node2D" },
    { "CollisionObject2D", "Node2D" },
    { "Area2D", "CollisionObject2D" },
    { "CollisionShape2D", "Node2D" },
    { "CollisionPolygon2D", "Node2D" },
    { "SpineSprite", "Node2D" },
    { "SpineBoneNode", "Node2D" },
    { "SpineSlotNode", "Node2D" },
    { "AudioStreamPlayer2D", "Node2D" },
    { "Timer", "Node" },
    { "AnimationPlayer", "Node" },
    { "AnimationTree", "Node" },
    { "AudioStreamPlayer", "Node" },
    { "HTTPRequest", "Node" },
    { "Viewport", "Node" },
    { "SubViewport", "Viewport" },
    { "Window", "Viewport" },
    { NULL, NULL }
};

const char* godot_parent_class(const std::string& cls) {
    for (int i = 0; CLASS_PARENTS[i][0]; ++i)
        if (cls == CLASS_PARENTS[i][0]) return CLASS_PARENTS[i][1];
    return "";
}

bool godot_is_a(const std::string& cls, const char* base) {
    std::string c = cls;
    for (int guard = 0; guard < 16 && !c.empty(); ++guard) {
        if (c == base) return true;
        c = godot_parent_class(c);
    }
    return false;
}

/* ---- Node ---- */

Node::Node() : parent(NULL), owner(NULL), unique_name(false), behavior(NULL), flags(0), queued_free_(false) {}

Node::~Node() {
    delete behavior;
    for (size_t i = 0; i < owned_resources.size(); ++i) delete owned_resources[i];
    for (size_t i = 0; i < children.size(); ++i) delete children[i];
}

void Node::add_child(Node* child) {
    if (child->parent) child->parent->remove_child(child);
    child->parent = this;
    children.push_back(child);
}

void Node::remove_child(Node* child) {
    for (size_t i = 0; i < children.size(); ++i) {
        if (children[i] == child) {
            children.erase(children.begin() + i);
            child->parent = NULL;
            return;
        }
    }
}

int Node::index() const {
    if (!parent) return 0;
    for (size_t i = 0; i < parent->children.size(); ++i)
        if (parent->children[i] == this) return (int)i;
    return -1;
}

static Node* find_unique_in(Node* n, const Node* owner, const std::string& name) {
    for (size_t i = 0; i < n->children.size(); ++i) {
        Node* c = n->children[i];
        if (c->unique_name && c->owner == owner && c->name == name) return c;
        Node* r = find_unique_in(c, owner, name);
        if (r) return r;
    }
    return NULL;
}

Node* Node::find_unique(const std::string& n) {
    /* Godot's %Name: the nodes this node owns (it is a scene's root), then
     * its owner's. */
    Node* r = find_unique_in(this, this, n);
    if (!r && owner) r = find_unique_in(owner, owner, n);
    return r;
}

Node* Node::get_node(const std::string& path) {
    Node* cur = this;
    size_t i = 0;
    if (path.empty() || path == ".") return this;
    while (i <= path.size()) {
        size_t j = path.find('/', i);
        if (j == std::string::npos) j = path.size();
        std::string part = path.substr(i, j - i);
        i = j + 1;
        if (part.empty() || part == ".") {
            if (j >= path.size()) break;
            continue;
        }
        if (part == "..") {
            cur = cur->parent;
        } else if (part[0] == '%') {
            cur = cur->find_unique(part.substr(1));
        } else {
            Node* next = NULL;
            for (size_t k = 0; k < cur->children.size(); ++k)
                if (cur->children[k]->name == part) {
                    next = cur->children[k];
                    break;
                }
            cur = next;
        }
        if (!cur) return NULL;
        if (j >= path.size()) break;
    }
    return cur;
}

std::string Node::path() const {
    if (!parent) return "/" + name;
    return parent->path() + "/" + name;
}

bool Node::set(const std::string& key, const Json& v, const PropCtx& ctx) {
    (void)ctx;
    if (key == "unique_name_in_owner") {
        unique_name = v.b();
        return true;
    }
    if (key == "script") {
        /* {"ext": null, "path": "res://..."} when the script is not in the
         * pack; packs from a .pck that holds the scripts name the entry
         * instead ({"ext": "src/....cs"}): the same res:// path. */
        script.clear();
        if (v.is_object() && v["path"].is_string()) script = v["path"].str();
        else if (v.is_object() && v["ext"].is_string()) script = "res://" + v["ext"].str();
        return true;
    }
    return false;
}

const Json* Node::get_extra(const char* key) const {
    std::map<std::string, const Json*>::const_iterator it = extra.find(key);
    return it == extra.end() ? NULL : it->second;
}

/* ---- CanvasItem ---- */

CanvasItem::CanvasItem()
    : visible(true), show_behind_parent(false), top_level(false), clip_children(0), z_index(0), z_as_relative(true),
      material(NULL), use_parent_material(false), texture_filter(0), texture_repeat(0) {
    flags |= NF_CANVAS_ITEM;
}

bool CanvasItem::set(const std::string& key, const Json& v, const PropCtx& ctx) {
    if (key == "visible") visible = v.b(true);
    else if (key == "modulate") jv_color(v, modulate);
    else if (key == "self_modulate") jv_color(v, self_modulate);
    else if (key == "show_behind_parent") show_behind_parent = v.b();
    else if (key == "top_level") top_level = v.b();
    else if (key == "clip_children") clip_children = v.i();
    else if (key == "z_index") z_index = v.i();
    else if (key == "z_as_relative") z_as_relative = v.b(true);
    else if (key == "use_parent_material") use_parent_material = v.b();
    else if (key == "texture_filter") texture_filter = v.i();
    else if (key == "texture_repeat") texture_repeat = v.i();
    else if (key.compare(0, 27, "instance_shader_parameters/") == 0) {
        /* a number: the item's shader parameter (vectors stay extras) */
        if (v.is_number()) shader_params[key.substr(27)] = (float)v.num();
        return Node::set(key, v, ctx);
    }
    else if (key == "material") {
        Resource* r = ctx.resource(v);
        material = r && (r->kind == Resource::RES_CANVAS_ITEM_MATERIAL || r->kind == Resource::RES_SHADER_MATERIAL)
                       ? (MaterialRes*)r
                       : NULL;
    } else return Node::set(key, v, ctx);
    return true;
}

CanvasItem* CanvasItem::parent_item() const {
    if (top_level || !parent || !parent->is_canvas_item()) return NULL;
    return (CanvasItem*)parent;
}

Xform2D CanvasItem::global_transform() const {
    CanvasItem* p = parent_item();
    return p ? p->global_transform() * get_transform() : get_transform();
}

bool CanvasItem::is_visible_in_tree() const {
    for (const Node* n = this; n; n = n->parent) {
        if (n->is_canvas_item() && !((const CanvasItem*)n)->visible) return false;
        if (!n->is_canvas_item()) break;
    }
    return true;
}

int CanvasItem::effective_texture_flags() const {
    int filter = 0, repeat = 0;
    for (const Node* n = this; n && n->is_canvas_item() && (!filter || !repeat); n = n->parent) {
        const CanvasItem* ci = (const CanvasItem*)n;
        if (!filter && ci->texture_filter) filter = ci->texture_filter;
        if (!repeat && ci->texture_repeat) repeat = ci->texture_repeat;
    }
    int f = 0;
    if (filter == 1 || filter == 3) f |= 2;  /* NEAREST, NEAREST_WITH_MIPMAPS */
    if (repeat == 2 || repeat == 3) f |= 1;  /* ENABLED, MIRROR (as repeat) */
    return f;
}

/* ---- Node2D ---- */

Node2D::Node2D() : scale(1, 1), rotation(0), skew(0) { flags |= NF_NODE2D; }

bool Node2D::set(const std::string& key, const Json& v, const PropCtx& ctx) {
    if (key == "position") jv_vec2(v, position);
    else if (key == "rotation") rotation = jv_float(v);
    else if (key == "rotation_degrees") rotation = jv_float(v) * 3.14159265f / 180.0f;
    else if (key == "scale") jv_vec2(v, scale);
    else if (key == "skew") skew = jv_float(v);
    else return CanvasItem::set(key, v, ctx);
    return true;
}

/* ---- Control ---- */

static u32 g_layout_pass = 1;

void Control::begin_layout_pass() { ++g_layout_pass; }

Control::Control()
    : grow_h(GROW_END), grow_v(GROW_END), size_flags_h(SIZE_FILL), size_flags_v(SIZE_FILL), stretch_ratio(1),
      rotation(0), scale(1, 1), clip_contents(false), mouse_filter(0), focus_mode(0), theme(NULL), min_pass_(0) {
    flags |= NF_CONTROL;
    for (int i = 0; i < 4; ++i) {
        anchor[i] = 0;
        offset[i] = 0;
    }
}

static int side_of(const std::string& s) {
    if (s == "left") return SIDE_LEFT;
    if (s == "top") return SIDE_TOP;
    if (s == "right") return SIDE_RIGHT;
    if (s == "bottom") return SIDE_BOTTOM;
    return -1;
}

bool Control::set(const std::string& key, const Json& v, const PropCtx& ctx) {
    if (key.compare(0, 7, "anchor_") == 0 && side_of(key.substr(7)) >= 0) {
        /* Out of the tree, as while loading, Godot keeps the offsets. */
        set_anchor(side_of(key.substr(7)), jv_float(v), parent == NULL, true);
    } else if (key.compare(0, 7, "offset_") == 0 && side_of(key.substr(7)) >= 0) {
        offset[side_of(key.substr(7))] = jv_float(v);
    } else if (key == "grow_horizontal") grow_h = v.i();
    else if (key == "grow_vertical") grow_v = v.i();
    else if (key == "custom_minimum_size") jv_vec2(v, custom_minimum_size);
    else if (key == "size_flags_horizontal") size_flags_h = v.i();
    else if (key == "size_flags_vertical") size_flags_v = v.i();
    else if (key == "size_flags_stretch_ratio") stretch_ratio = jv_float(v, 1);
    else if (key == "rotation") rotation = jv_float(v);
    else if (key == "rotation_degrees") rotation = jv_float(v) * 3.14159265f / 180.0f;
    else if (key == "scale") jv_vec2(v, scale);
    else if (key == "pivot_offset") jv_vec2(v, pivot_offset);
    else if (key == "clip_contents") clip_contents = v.b();
    else if (key == "mouse_filter") mouse_filter = v.i();
    else if (key == "focus_mode") focus_mode = v.i();
    else if (key == "layout_mode" || key == "anchors_preset" || key == "layout_direction") {
        /* Editor bookkeeping: the anchors and offsets are stored explicitly. */
    } else if (key == "theme") {
        Resource* r = ctx.resource(v);
        theme = r && r->kind == Resource::RES_THEME ? (ThemeRes*)r : NULL;
    } else if (key == "theme_type_variation") theme_type_variation = v.str();
    else if (key.compare(0, 15, "theme_override_") == 0) {
        std::string k = key.substr(15); /* "colors/font_color" */
        if (k.compare(0, 6, "fonts/") == 0 || k.compare(0, 7, "styles/") == 0 || k.compare(0, 6, "icons/") == 0) {
            Resource* r = ctx.resource(v);
            if (r) override_resources[k] = r;
            else override_resources.erase(k);
        } else {
            override_values[k] = &v;
        }
    } else return CanvasItem::set(key, v, ctx);
    return true;
}

void Control::set_anchor(int side, float value, bool keep_offset, bool push_opposite) {
    Rect2 pr = parent_anchorable_rect();
    float range = (side == SIDE_LEFT || side == SIDE_RIGHT) ? pr.size.x : pr.size.y;
    float prev_pos = offset[side] + anchor[side] * range;
    float prev_opp = offset[(side + 2) % 4] + anchor[(side + 2) % 4] * range;
    anchor[side] = value;
    int opp = (side + 2) % 4;
    if (((side == SIDE_LEFT || side == SIDE_TOP) && anchor[side] > anchor[opp]) ||
        ((side == SIDE_RIGHT || side == SIDE_BOTTOM) && anchor[side] < anchor[opp])) {
        if (push_opposite) anchor[opp] = anchor[side];
        else anchor[side] = anchor[opp];
    }
    if (!keep_offset) {
        offset[side] = prev_pos - anchor[side] * range;
        if (push_opposite) offset[opp] = prev_opp - anchor[opp] * range;
    }
}

Xform2D Control::get_transform() const {
    /* T(position) * T(pivot) * R * S * T(-pivot) */
    Xform2D xf = Xform2D::make(pivot_offset, rotation, scale, 0.0f);
    xf.origin = xf.origin - xf.basis_xform(pivot_offset);
    xf.origin = xf.origin + position;
    return xf;
}

Vec2 Control::combined_minimum_size() {
    if (min_pass_ != g_layout_pass) {
        Vec2 m = get_minimum_size();
        min_cache_ = Vec2(maxf(m.x, custom_minimum_size.x), maxf(m.y, custom_minimum_size.y));
        min_pass_ = g_layout_pass;
    }
    return min_cache_;
}

/* The viewport's visible rect, for controls without a CanvasItem parent. */
Rect2 g_viewport_rect(0, 0, 1920, 1080);

Rect2 Control::parent_anchorable_rect() const {
    CanvasItem* p = parent_item();
    if (p) return p->anchorable_rect();
    return g_viewport_rect;
}

void Control::update_rect() {
    Rect2 pr = parent_anchorable_rect();
    float edge[4];
    for (int i = 0; i < 4; ++i) edge[i] = offset[i] + anchor[i] * ((i & 1) ? pr.size.y : pr.size.x);
    Vec2 pos(edge[0], edge[1]);
    Vec2 sz(edge[2] - edge[0], edge[3] - edge[1]);
    Vec2 m = combined_minimum_size();
    if (m.x > sz.x) {
        if (grow_h == GROW_BEGIN) pos.x += sz.x - m.x;
        else if (grow_h == GROW_BOTH) pos.x += 0.5f * (sz.x - m.x);
        sz.x = m.x;
    }
    if (m.y > sz.y) {
        if (grow_v == GROW_BEGIN) pos.y += sz.y - m.y;
        else if (grow_v == GROW_BOTH) pos.y += 0.5f * (sz.y - m.y);
        sz.y = m.y;
    }
    position = pos;
    size = sz;
}

void Control::set_rect(const Rect2& r) {
    Vec2 ps = parent_anchorable_rect().size;
    offset[0] = r.pos.x - anchor[0] * ps.x;
    offset[1] = r.pos.y - anchor[1] * ps.y;
    offset[2] = r.pos.x + r.size.x - anchor[2] * ps.x;
    offset[3] = r.pos.y + r.size.y - anchor[3] * ps.y;
    update_rect();
}

void Control::set_position(const Vec2& p) { set_rect(Rect2(p, size)); }

void Control::set_size(const Vec2& s) {
    Vec2 m = combined_minimum_size();
    set_rect(Rect2(position, Vec2(maxf(s.x, m.x), maxf(s.y, m.y))));
}

/* ---- theme lookup ---- */

static Control* parent_control(Control* c) {
    return c->parent && c->parent->is_control() ? (Control*)c->parent : NULL;
}

static std::string variation_base(ThemeRes* t, const std::string& type) {
    std::map<std::string, std::string>::const_iterator it = t->variation_base.find(type);
    return it == t->variation_base.end() ? std::string() : it->second;
}

void Control::type_chain(std::vector<std::string>& out, ThemeRes** variation_theme) {
    out.clear();
    if (!theme_type_variation.empty()) {
        ThemeRes* vt = NULL;
        for (Control* c = this; c && !vt; c = parent_control(c))
            if (c->theme && !variation_base(c->theme, theme_type_variation).empty()) vt = c->theme;
        if (!vt && g_default_theme && !variation_base(g_default_theme, theme_type_variation).empty())
            vt = g_default_theme;
        if (vt) {
            std::string v = theme_type_variation;
            for (int guard = 0; guard < 16 && !v.empty(); ++guard) {
                out.push_back(v);
                v = variation_base(vt, v);
                if (v == type) break;
            }
        }
        if (variation_theme) *variation_theme = vt;
    }
    std::string c = type.empty() ? std::string("Control") : type;
    for (int guard = 0; guard < 16 && !c.empty() && c != "Object"; ++guard) {
        out.push_back(c);
        c = godot_parent_class(c);
    }
}

const Json* Control::theme_value(const char* kind, const char* name) {
    std::string key = std::string(kind) + "/" + name;
    std::map<std::string, const Json*>::const_iterator ov = override_values.find(key);
    if (ov != override_values.end()) return ov->second;
    std::vector<std::string> chain;
    type_chain(chain, NULL);
    for (int pass = 0; pass < 2; ++pass) {
        for (Control* c = this; pass == 1 || c; c = pass == 0 ? parent_control(c) : NULL) {
            ThemeRes* t = pass == 0 ? c->theme : g_default_theme;
            if (t) {
                for (size_t i = 0; i < chain.size(); ++i) {
                    std::map<std::string, const Json*>::const_iterator it = t->items.find(chain[i] + "/" + key);
                    if (it != t->items.end()) return it->second;
                }
            }
            if (pass == 1) break;
        }
    }
    return NULL;
}

Resource* Control::theme_resource(const char* kind, const char* name) {
    std::string key = std::string(kind) + "/" + name;
    std::map<std::string, Resource*>::const_iterator ov = override_resources.find(key);
    if (ov != override_resources.end()) return ov->second;
    std::vector<std::string> chain;
    type_chain(chain, NULL);
    bool is_font = strcmp(kind, "fonts") == 0;
    for (int pass = 0; pass < 2; ++pass) {
        for (Control* c = this; pass == 1 || c; c = pass == 0 ? parent_control(c) : NULL) {
            ThemeRes* t = pass == 0 ? c->theme : g_default_theme;
            if (t) {
                for (size_t i = 0; i < chain.size(); ++i) {
                    std::map<std::string, Resource*>::const_iterator it = t->resources.find(chain[i] + "/" + key);
                    if (it != t->resources.end() && it->second) return it->second;
                    if (is_font && t->default_font) return t->default_font; /* has_font: the theme's default */
                }
            }
            if (pass == 1) break;
        }
    }
    return is_font ? (Resource*)g_default_font : NULL;
}

Color Control::theme_color(const char* name) {
    const Json* v = theme_value("colors", name);
    Color c(0, 0, 0, 0);
    if (v) jv_color(*v, c);
    return c;
}

int Control::theme_constant(const char* name) {
    const Json* v = theme_value("constants", name);
    return v ? v->i() : 0;
}

/* The font size when the lookup ends at a theme's default_font_size. */
static int default_font_size_for(Control* self) {
    for (Control* c = self; c; c = parent_control(c))
        if (c->theme && c->theme->default_font_size > 0) return c->theme->default_font_size;
    return g_default_theme && g_default_theme->default_font_size > 0 ? g_default_theme->default_font_size : 16;
}

int Control::theme_font_size(const char* name) {
    std::string key = std::string("font_sizes/") + name;
    std::map<std::string, const Json*>::const_iterator ov = override_values.find(key);
    if (ov != override_values.end() && ov->second->i() > 0) return ov->second->i();
    std::vector<std::string> chain;
    type_chain(chain, NULL);
    for (int pass = 0; pass < 2; ++pass) {
        for (Control* c = this; pass == 1 || c; c = pass == 0 ? parent_control(c) : NULL) {
            ThemeRes* t = pass == 0 ? c->theme : g_default_theme;
            if (t) {
                for (size_t i = 0; i < chain.size(); ++i) {
                    std::map<std::string, const Json*>::const_iterator it = t->items.find(chain[i] + "/" + key);
                    if (it != t->items.end() && it->second->i() > 0) return it->second->i();
                    if (t->default_font_size > 0) return t->default_font_size;
                }
            }
            if (pass == 1) break;
        }
    }
    return default_font_size_for(this);
}

FontRes* Control::theme_font(const char* name) {
    Resource* r = theme_resource("fonts", name);
    return r && r->kind == Resource::RES_FONT ? (FontRes*)r : g_default_font;
}

StyleBoxRes* Control::theme_stylebox(const char* name) {
    Resource* r = theme_resource("styles", name);
    return r && r->is_stylebox() ? (StyleBoxRes*)r : NULL;
}

void Control::add_theme_color_override(const std::string& name, const Color& c) {
    std::string k = "colors/" + name;
    std::string text = format("{\"t\":\"Color\",\"v\":[%.9g,%.9g,%.9g,%.9g]}", c.r, c.g, c.b, c.a);
    Json& j = code_overrides[k];
    j = Json::parse(text.c_str(), text.size(), NULL);
    override_values[k] = &j;
}

void Control::add_theme_font_size_override(const std::string& name, int size) {
    std::string k = "font_sizes/" + name;
    Json& j = code_overrides[k];
    j = Json::make_number(size);
    override_values[k] = &j;
}

void Control::add_theme_constant_override(const std::string& name, int value) {
    std::string k = "constants/" + name;
    Json& j = code_overrides[k];
    j = Json::make_number(value);
    override_values[k] = &j;
}

bool Control::has_theme_color_override(const char* name) const {
    return override_values.find(std::string("colors/") + name) != override_values.end();
}

} /* namespace spire */
