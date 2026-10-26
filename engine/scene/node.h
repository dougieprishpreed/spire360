/* The scene tree: Node, CanvasItem, Node2D and Control, with Godot 4.5's
 * layout rules (anchors, offsets, grow direction, minimum sizes) and theme
 * lookup. Concrete controls are in controls.h. Ported from Godot 4.5.1
 * (scene/main/node.cpp, scene/main/canvas_item.cpp, scene/gui/control.cpp,
 * scene/theme/theme_owner.cpp; MIT, see THIRD_PARTY.md). */
#ifndef SPIRE_NODE_H
#define SPIRE_NODE_H

#include "../core/base.h"
#include "../core/json.h"
#include "resource.h"

#include <map>

namespace spire {

class Canvas;
class SceneLibrary;
class ScriptBehavior;
struct SceneFile;

/* What set() needs to resolve references in a scene file. */
struct PropCtx {
    SceneLibrary* lib;
    const SceneFile* file;
    PropCtx(SceneLibrary* l, const SceneFile* f) : lib(l), file(f) {}
    Resource* resource(const Json& v) const;
};

/* NF_PROCESS: the class has a process() of its own (the tree calls no
 * other node's). */
/* NF_SCRIPT_UNPORTED: its script has no registered port; the tree checks
 * at its first process whether code gave it a behavior since
 * (count_missing_scripts) */
enum NodeFlags { NF_CANVAS_ITEM = 1, NF_CONTROL = 2, NF_NODE2D = 4, NF_CONTAINER = 8, NF_CANVAS_LAYER = 16, NF_PROCESS = 32,
                 NF_SCRIPT_UNPORTED = 64 };

class Node {
public:
    Node();
    virtual ~Node();
    /* A freed node is no tween's target any more (TweenSet::forget): a
     * screen's tweens can outlive a node they move. The size is the
     * deleted node's own (its class's). */
    static void* operator new(size_t size);
    static void operator delete(void* p, size_t size);

    std::string name;
    std::string type; /* Godot class the scene asked for */
    Node* parent;
    std::vector<Node*> children;
    Node* owner;         /* root of the scene file that created this node */
    bool unique_name;    /* %Name */
    std::string script;  /* res:// path of the attached script ("" if none) */
    ScriptBehavior* behavior; /* its C++ port, if there is one (owned) */
    std::map<std::string, const Json*> extra; /* properties without a native field (script exports, metadata) */
    std::vector<class Resource*> owned_resources; /* resources bound to this node (deleted with it) */
    u32 flags;

    bool is_canvas_item() const { return (flags & NF_CANVAS_ITEM) != 0; }
    bool is_control() const { return (flags & NF_CONTROL) != 0; }
    bool is_node2d() const { return (flags & NF_NODE2D) != 0; }
    bool is_container() const { return (flags & NF_CONTAINER) != 0; }

    void add_child(Node* child);
    void remove_child(Node* child); /* does not delete */
    /* add_child placed just before sibling (drawn under it); at the end when
     * sibling is not a child */
    void add_child_before(Node* child, Node* sibling);
    /* Godot's QueueFree: removed after this frame's process traversal. */
    void queue_free() {
        queued_free_ = true;
        ++frees_pending;
    }
    /* Nodes queued since the tree last removed them (none: no sweep). */
    static int frees_pending;
    bool queued_for_deletion() const { return queued_free_; }
    int index() const;
    /* "a/b", "..", "%Unique", "%Unique/child" */
    Node* get_node(const std::string& path);
    Node* find_unique(const std::string& name);
    std::string path() const; /* from the root, for logs */

    /* Applies a scene property. Returns false if it is not a native property
     * of this class (it then goes to extra). */
    virtual bool set(const std::string& key, const Json& v, const PropCtx& ctx);
    const Json* get_extra(const char* key) const;
    /* Per frame (Godot's _process / internal process), dt in seconds. */
    virtual void process(double dt) { (void)dt; }

private:
    bool queued_free_;
    Node(const Node&);
    Node& operator=(const Node&);
};

class MaterialRes;

class CanvasItem : public Node {
public:
    CanvasItem();
    virtual ~CanvasItem();
    bool visible;
    Color modulate, self_modulate;
    bool show_behind_parent, top_level;
    int clip_children; /* 0 disabled, 1 clip only, 2 clip and draw */
    int z_index;
    bool z_as_relative;
    MaterialRes* material;
    bool use_parent_material;
    int texture_filter, texture_repeat; /* 0 = parent */
    /* Shader parameters set from code for this item (Godot: a material
     * local to the scene, changed by the node's script). */
    std::map<std::string, float> shader_params;

    virtual bool set(const std::string& key, const Json& v, const PropCtx& ctx);
    virtual Xform2D get_transform() const { return Xform2D(); }
    /* Local rect (Control: its size), for clipping. */
    virtual Rect2 item_rect() const { return Rect2(); }
    /* What child Controls anchor to (Godot's get_anchorable_rect). */
    virtual Rect2 anchorable_rect() const { return Rect2(); }
    virtual bool clips() const { return false; }
    virtual void draw(Canvas&) {}

    CanvasItem* parent_item() const; /* NULL if top level or the parent is not a CanvasItem */
    Xform2D global_transform() const;
    bool is_visible_in_tree() const;
    int effective_texture_flags() const;
};

class Node2D : public CanvasItem {
public:
    Node2D();
    Vec2 position, scale;
    float rotation, skew;
    virtual bool set(const std::string& key, const Json& v, const PropCtx& ctx);
    virtual Xform2D get_transform() const { return Xform2D::make(position, rotation, scale, skew); }
};

class ThemeRes;
class FontRes;
class StyleBoxRes;

enum { GROW_BEGIN = 0, GROW_END = 1, GROW_BOTH = 2 };
enum { SIZE_SHRINK_BEGIN = 0, SIZE_FILL = 1, SIZE_EXPAND = 2, SIZE_SHRINK_CENTER = 4, SIZE_SHRINK_END = 8 };

class Control : public CanvasItem {
public:
    Control();
    float anchor[4], offset[4];
    int grow_h, grow_v;
    Vec2 custom_minimum_size;
    int size_flags_h, size_flags_v;
    float stretch_ratio;
    float rotation;
    Vec2 scale, pivot_offset;
    bool clip_contents;
    int mouse_filter, focus_mode;
    ThemeRes* theme;
    std::string theme_type_variation;
    std::map<std::string, const Json*> override_values;  /* "colors/font_color" -> value */
    std::map<std::string, Resource*> override_resources; /* fonts, styles, icons */
    /* AddThemeColorOverride from code ("font_color"). */
    void add_theme_color_override(const std::string& name, const Color& c);
    void add_theme_font_size_override(const std::string& name, int size);
    void add_theme_constant_override(const std::string& name, int value);
    std::map<std::string, Json> code_overrides; /* storage for the above */
    /* theme_constant's answers (layout asks for the same few each frame),
     * kept until anything they depend on may have changed: theme_generation
     * moves on with the tree's shape, a theme or an override set. */
    static unsigned theme_generation;
    unsigned constants_gen_;
    std::vector<std::pair<std::string, int> > constants_;

    /* Layout state (Godot's pos_cache / size_cache). */
    Vec2 position, size;

    virtual bool set(const std::string& key, const Json& v, const PropCtx& ctx);
    virtual Xform2D get_transform() const;
    virtual Rect2 item_rect() const { return Rect2(Vec2(0, 0), size); }
    virtual Rect2 anchorable_rect() const { return Rect2(Vec2(0, 0), size); }
    virtual bool clips() const { return clip_contents; }

    /* Godot's get_minimum_size: what the content needs. */
    virtual Vec2 get_minimum_size() { return Vec2(); }
    Vec2 combined_minimum_size(); /* max(content, custom), cached per layout pass */
    /* Containers place their children here (NOTIFICATION_SORT_CHILDREN). */
    virtual void sort_children() {}
    /* Called after this control's rect is known, before its children are
     * laid out (text reflow and the like). */
    virtual void resized() {}

    Rect2 parent_anchorable_rect() const;
    void update_rect();                 /* _size_changed */
    void set_rect(const Rect2& r);      /* moves the offsets, keeps the anchors */
    void set_position(const Vec2& p);
    void set_size(const Vec2& s);
    void set_anchor(int side, float value, bool keep_offset, bool push_opposite);
    Rect2 rect() const { return Rect2(position, size); }

    /* Theme lookup: overrides, then the themes of this control and its Control
     * ancestors, then Godot's default theme. */
    Color theme_color(const char* name);
    int theme_constant(const char* name);
    int theme_font_size(const char* name);
    FontRes* theme_font(const char* name);
    StyleBoxRes* theme_stylebox(const char* name);
    bool has_theme_color_override(const char* name) const;

    static void begin_layout_pass(); /* invalidates every minimum size cache */

private:
    const Json* theme_value(const char* kind, const char* name);
    Resource* theme_resource(const char* kind, const char* name);
    void type_chain(std::vector<std::string>& out, ThemeRes** variation_theme);
    u32 min_pass_;
    Vec2 min_cache_;
};

/* Godot's native class hierarchy for theme types and fallbacks: the parent
 * class of a Godot class, "" at Object. */
const char* godot_parent_class(const std::string& cls);
bool godot_is_a(const std::string& cls, const char* base);

} /* namespace spire */

#endif
