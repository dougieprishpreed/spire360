#include "scene.h"
#include "line_2d.h"
#include "animation.h"

#include "../core/pack.h"
#include "controls.h"
#include "particles.h"
#include "path_2d.h"
#include "rich_text.h"
#include "spine_sprite.h"
#include "script.h"
#include "text_nodes.h"
#include "value.h"
#include "viewport.h"

#include <string.h>

namespace spire {

/* An inline shader's name: "inline:" and a hash of its code without
 * comments and whitespace, so the copies of one shader in several files
 * (the character transitions, ...) are one shader to the ports
 * (shaders.cpp); "<file>::<sub id>" when it has no code. */
static std::string inline_shader_name(const SceneFile* ctx, const std::string& sub) {
    std::string fallback = (ctx ? ctx->name : std::string()) + "::" + sub;
    if (!ctx) return fallback;
    const Json& code = ctx->doc["sub"][sub.c_str()]["props"]["code"];
    if (!code.is_string() || code.str().empty()) return fallback;
    const std::string& src = code.str();
    std::string norm;
    norm.reserve(src.size());
    for (size_t i = 0; i < src.size(); ++i) {
        if (src[i] == '/' && i + 1 < src.size() && src[i + 1] == '/') {
            while (i < src.size() && src[i] != '\n') ++i;
            continue;
        }
        if (src[i] != ' ' && src[i] != '\t' && src[i] != '\n' && src[i] != '\r') norm += src[i];
    }
    return format("inline:%016llx", (unsigned long long)fnv1a64(norm.c_str()));
}

/* The scene being instantiated, for script factories (script_resource). */
static const PropCtx* g_script_ctx = NULL;

Resource* script_resource(const Json& v) { return g_script_ctx ? g_script_ctx->resource(v) : NULL; }


extern ThemeRes* g_default_theme;
extern FontRes* g_default_font;

static const char* const DEFAULT_THEME_JSON[] = {
#include "default_theme.inc"
    NULL
};

/* Godot's default font is not in the game's pack; the converter adds it
 * under this name when the Godot sources are available, otherwise text
 * using it falls back to the first font the scene names. */
static const char* const DEFAULT_FONT_NAME = "godot/OpenSans_SemiBold.ttf";

static SceneLibrary* g_library = NULL;

SceneLibrary* SceneLibrary::current() { return g_library; }

SceneLibrary::SceneLibrary(const Pack* pack) : pack_(pack), audio_(NULL), default_theme_(NULL), default_font_(NULL) {
    g_library = this;
    default_font_ = new FontRes("FontFile");
    default_font_->file = DEFAULT_FONT_NAME;
    default_font_->path = DEFAULT_FONT_NAME;
    owned_.push_back(default_font_);
    init_default_theme();
    g_default_theme = default_theme_;
    g_default_font = default_font_;
}

SceneLibrary::~SceneLibrary() {
    if (g_library == this) g_library = NULL;
    if (g_default_theme == default_theme_) {
        g_default_theme = NULL;
        g_default_font = NULL;
    }
    for (std::map<std::string, SceneFile*>::iterator it = files_.begin(); it != files_.end(); ++it) delete it->second;
    for (std::map<std::string, Resource*>::iterator it = ext_.begin(); it != ext_.end(); ++it) delete it->second;
    for (size_t i = 0; i < owned_.size(); ++i) delete owned_[i];
}

void SceneLibrary::init_default_theme() {
    std::string text;
    for (int i = 0; DEFAULT_THEME_JSON[i]; ++i) text += DEFAULT_THEME_JSON[i];
    std::string err;
    default_theme_doc_ = Json::parse(text.c_str(), text.size(), &err);
    if (!err.empty()) log_warn("default theme: %s", err.c_str());
    ThemeRes* t = new ThemeRes();
    owned_.push_back(t);
    t->path = "godot/default_theme";
    t->default_font = default_font_;
    t->default_font_size = default_theme_doc_["default_font_size"].i(16);
    t->default_base_scale = default_theme_doc_["default_base_scale"].f(1.0f);
    const Json& types = default_theme_doc_["types"];
    for (size_t i = 0; i < types.keys().size(); ++i) {
        const std::string& type = types.keys()[i];
        const Json& items = types[type.c_str()];
        for (size_t k = 0; k < items.keys().size(); ++k) {
            const std::string& key = items.keys()[k];
            const Json& v = items[key.c_str()];
            if (key == "base") t->variation_base[type] = v.str();
            else if (key.compare(0, 6, "fonts/") == 0) t->resources[type + "/" + key] = default_font_;
            else if (key.compare(0, 7, "styles/") == 0) t->resources[type + "/" + key] = object(v, NULL);
            else t->items[type + "/" + key] = &v;
        }
    }
    default_theme_ = t;
}

const SceneFile* SceneLibrary::file(const std::string& name) {
    std::map<std::string, SceneFile*>::iterator it = files_.find(name);
    if (it != files_.end()) return it->second;
    SceneFile* f = NULL;
    const PackEntry* e = pack_ ? pack_->find(name.c_str()) : NULL;
    std::vector<u8> data;
    if (e && e->type == PACK_SCENE && pack_->read(e, data)) {
        std::string err;
        f = new SceneFile();
        f->name = name;
        f->doc = Json::parse(data.empty() ? "" : (const char*)&data[0], data.size(), &err);
        if (!err.empty()) {
            log_warn("%s: %s", name.c_str(), err.c_str());
            delete f;
            f = NULL;
        }
    }
    files_[name] = f;
    return f;
}

/* ---- resources ---- */

/* AtlasResourceLoader: sprites absent from a sheet can use unpacked art,
 * including beta card portraits. These are resources, not missing assets. */
static void atlas_fallbacks(const std::string& name, std::vector<std::string>& out) {
    /* Sprites the game's code names "x.png.tres" (CardModel.AncientBorder)
     * are "x.tres" in the game's pck. */
    if (name.size() > 9 && name.compare(name.size() - 9, 9, ".png.tres") == 0)
        out.push_back(name.substr(0, name.size() - 9) + ".tres");
    const std::string prefix = "images/atlases/";
    size_t split = name.find(".sprites/");
    if (name.compare(0, prefix.size(), prefix) != 0 || split == std::string::npos ||
        name.size() < split + 14 || name.compare(name.size() - 5, 5, ".tres") != 0) return;
    std::string atlas = name.substr(prefix.size(), split - prefix.size());
    std::string sprite = name.substr(split + 9, name.size() - split - 14);
    if (atlas == "card_atlas") {
        out.push_back("images/packed/card_portraits/" + sprite + ".png");
        size_t slash = sprite.find_last_of('/');
        if (slash != std::string::npos && slash > 0)
            out.push_back("images/packed/card_portraits/" + sprite.substr(0, slash) + "/beta/" + sprite.substr(slash + 1) + ".png");
        out.push_back("images/packed/card_portraits/beta.png");
    } else if (atlas == "relic_atlas" || atlas == "relic_outline_atlas") {
        out.push_back("images/relics/" + sprite + ".png");
        out.push_back("images/relics/beta/" + sprite + ".png");
        out.push_back("images/powers/missing_power.png");
    } else if (atlas == "power_atlas") {
        out.push_back("images/powers/" + sprite + ".png");
        out.push_back("images/powers/beta/" + sprite + ".png");
        out.push_back("images/powers/missing_power.png");
    } else if (atlas == "potion_atlas" || atlas == "potion_outline_atlas") {
        out.push_back("images/potions/" + sprite + ".png");
        out.push_back("images/potions/missing_potion.png");
    }
}

Resource* SceneLibrary::ext(const std::string& name) {
    std::map<std::string, Resource*>::iterator it = ext_.find(name);
    if (it != ext_.end()) return it->second;
    Resource* r = NULL;
    const PackEntry* e = pack_ ? pack_->find(name.c_str()) : NULL;
    if (!e) {
        std::vector<std::string> candidates;
        atlas_fallbacks(name, candidates);
        for (size_t i = 0; pack_ && i < candidates.size(); ++i) {
            if (!pack_->find(candidates[i].c_str())) continue;
            Resource* image = ext(candidates[i]);
            if (!image || !image->is_texture()) continue;
            AtlasTextureRes* alias = new AtlasTextureRes();
            alias->atlas = (Texture2DRes*)image;
            r = alias;
            break;
        }
        if (!r) note("missing " + name);
    } else if (e->type == PACK_TEXTURE) {
        ImageTextureRes* t = new ImageTextureRes();
        std::vector<u8> head;
        int w, h, ow, oh;
        if (pack_->read_head(e, 12, head) && Pack::parse_texture_size(head, &w, &h, &ow, &oh))
            t->size_ = Vec2((float)ow, (float)oh);
        r = t;
    } else if (e->type == PACK_FONT) {
        FontRes* f = new FontRes("FontFile");
        f->file = name;
        r = f;
    } else if (e->type == PACK_SCENE) {
        /* Insert a placeholder first: resources can refer to each other. */
        ext_[name] = NULL;
        const SceneFile* f = file(name);
        if (f && f->doc["kind"].str() == "scene") {
            r = new Resource(Resource::RES_PACKED_SCENE, "PackedScene");
        } else if (f) {
            r = build(f->doc["type"].str(), f->doc["resource"], f, name);
            if (!r) note("resource " + f->doc["type"].str());
        }
    } else {
        r = new Resource(Resource::RES_GENERIC, "Resource");
    }
    if (r) r->path = name;
    ext_[name] = r;
    return r;
}

Resource* SceneLibrary::sub(const SceneFile* f, const std::string& id) {
    if (!f) return NULL;
    SceneFile* mf = (SceneFile*)f;
    std::map<std::string, Resource*>::iterator it = mf->subs.find(id);
    if (it != mf->subs.end()) return it->second;
    mf->subs[id] = NULL;
    const Json& s = f->doc["sub"][id.c_str()];
    Resource* r = s.is_object() ? build(s["type"].str(), s["props"], f, f->name + "::" + id) : NULL;
    if (r) owned_.push_back(r);
    mf->subs[id] = r;
    return r;
}

Resource* SceneLibrary::object(const Json& v, const SceneFile* ctx) {
    Resource* r = build(v["class"].str(), v["props"], ctx, "");
    if (r) owned_.push_back(r);
    return r;
}

Resource* SceneLibrary::resolve(const Json& v, const SceneFile* ctx) {
    if (jv_is_ext(v)) return v["ext"].is_string() ? ext(v["ext"].str()) : NULL;
    if (jv_is_sub(v)) return sub(ctx, v["sub"].str());
    if (jv_is_object(v)) return object(v, ctx);
    return NULL;
}

static void margins4(const Json& p, const char* prefix, float* out) {
    static const char* const sides[4] = { "left", "top", "right", "bottom" };
    for (int i = 0; i < 4; ++i) {
        std::string k = std::string(prefix) + sides[i];
        if (p.has(k.c_str())) out[i] = jv_float(p[k.c_str()]);
    }
}

Resource* SceneLibrary::build(const std::string& cls, const Json& p, const SceneFile* ctx, const std::string& path) {
    Resource* r = NULL;
    if (p["spire_baked_texture"].is_string()) {
        /* Procedural textures are generated by Godot during conversion.
         * Keep an alias resource here; Canvas owns the shared GPU upload. */
        Resource* image = ext(p["spire_baked_texture"].str());
        AtlasTextureRes* t = new AtlasTextureRes();
        t->atlas = image && image->is_texture() ? (Texture2DRes*)image : NULL;
        r = t;
    } else if (cls == "SpineSkeletonDataResource") {
        SpineDataRes* d = new SpineDataRes();
        const Json& a = p["atlas_res"];
        const Json& k = p["skeleton_file_res"];
        if (jv_is_ext(a) && a["ext"].is_string()) d->atlas_path = a["ext"].str();
        if (jv_is_ext(k) && k["ext"].is_string()) d->skeleton_path = k["ext"].str();
        r = d;
    } else if (cls == "AtlasTexture") {
        AtlasTextureRes* t = new AtlasTextureRes();
        Resource* a = resolve(p["atlas"], ctx);
        t->atlas = a && a->is_texture() ? (Texture2DRes*)a : NULL;
        jv_rect2(p["region"], t->region);
        jv_rect2(p["margin"], t->margin);
        t->filter_clip = p["filter_clip"].b();
        r = t;
    } else if (cls == "StyleBoxFlat") {
        StyleBoxFlatRes* s = new StyleBoxFlatRes();
        jv_color(p["bg_color"], s->bg_color);
        jv_color(p["border_color"], s->border_color);
        jv_color(p["shadow_color"], s->shadow_color);
        float bw[4] = { 0, 0, 0, 0 };
        margins4(p, "border_width_", bw);
        for (int i = 0; i < 4; ++i) s->border_width[i] = (int)bw[i];
        static const char* const corners[4] = { "corner_radius_top_left", "corner_radius_top_right",
                                                 "corner_radius_bottom_right", "corner_radius_bottom_left" };
        for (int i = 0; i < 4; ++i)
            if (p.has(corners[i])) s->corner_radius[i] = jv_float(p[corners[i]]);
        margins4(p, "expand_margin_", s->expand_margin);
        margins4(p, "content_margin_", s->content_margin);
        if (p.has("corner_detail")) s->corner_detail = p["corner_detail"].i();
        if (p.has("shadow_size")) s->shadow_size = p["shadow_size"].i();
        jv_vec2(p["shadow_offset"], s->shadow_offset);
        jv_vec2(p["skew"], s->skew);
        if (p.has("draw_center")) s->draw_center = p["draw_center"].b(true);
        if (p.has("border_blend")) s->border_blend = p["border_blend"].b();
        if (p.has("anti_aliasing")) s->anti_aliased = p["anti_aliasing"].b(true);
        if (p.has("anti_aliasing_size")) s->aa_size = jv_float(p["anti_aliasing_size"], 1.0f);
        r = s;
    } else if (cls == "StyleBoxTexture") {
        StyleBoxTextureRes* s = new StyleBoxTextureRes();
        Resource* t = resolve(p["texture"], ctx);
        s->texture = t && t->is_texture() ? (Texture2DRes*)t : NULL;
        margins4(p, "texture_margin_", s->texture_margin);
        margins4(p, "margin_", s->texture_margin); /* Godot 3 names */
        margins4(p, "expand_margin_", s->expand_margin);
        margins4(p, "content_margin_", s->content_margin);
        jv_rect2(p["region_rect"], s->region_rect);
        s->axis_h = p["axis_stretch_horizontal"].i();
        s->axis_v = p["axis_stretch_vertical"].i();
        if (p.has("draw_center")) s->draw_center = p["draw_center"].b(true);
        jv_color(p["modulate_color"], s->modulate);
        r = s;
    } else if (cls == "StyleBoxLine") {
        StyleBoxLineRes* s = new StyleBoxLineRes();
        margins4(p, "content_margin_", s->content_margin);
        jv_color(p["color"], s->color);
        if (p.has("thickness")) s->thickness = p["thickness"].i(1);
        if (p.has("grow_begin")) s->grow_begin = jv_float(p["grow_begin"], 1);
        if (p.has("grow_end")) s->grow_end = jv_float(p["grow_end"], 1);
        s->vertical = p["vertical"].b();
        r = s;
    } else if (cls == "StyleBoxEmpty" || cls == "StyleBox") {
        StyleBoxRes* s = new StyleBoxRes(Resource::RES_STYLEBOX_EMPTY, cls);
        margins4(p, "content_margin_", s->content_margin);
        r = s;
    } else if (cls == "Theme") {
        r = build_theme(p, ctx);
    } else if (cls == "FontVariation" || cls == "FontFile" || cls == "SystemFont") {
        FontRes* f = new FontRes(cls);
        if (cls == "FontVariation") {
            Resource* b = resolve(p["base_font"], ctx);
            f->base = b && b->kind == Resource::RES_FONT ? (FontRes*)b : NULL;
            f->spacing_glyph = p["spacing_glyph"].i();
            f->spacing_space = p["spacing_space"].i();
            f->spacing_top = p["spacing_top"].i();
            f->spacing_bottom = p["spacing_bottom"].i();
            f->embolden = jv_float(p["variation_embolden"]);
            Vec2 x, y, o;
            const Json& vt = p["variation_transform"];
            if (vt.is_object() && vt["v"].size() >= 6) {
                const Json& a = vt["v"];
                f->variation_transform = Xform2D(Vec2(jv_float(a[(size_t)0]), jv_float(a[1])), Vec2(jv_float(a[2]), jv_float(a[3])),
                                                 Vec2(jv_float(a[4]), jv_float(a[5])));
            }
        } else if (cls == "SystemFont") {
            note("resource SystemFont");
        }
        const Json& fb = p["fallbacks"];
        for (size_t i = 0; i < fb.size(); ++i) {
            Resource* x = resolve(fb[i], ctx);
            if (x && x->kind == Resource::RES_FONT) f->fallbacks.push_back((FontRes*)x);
        }
        r = f;
    } else if (cls == "ViewportTexture") {
        ViewportTextureRes* t = new ViewportTextureRes();
        const Json& np = p["viewport_path"];
        t->viewport_path = np.is_string() ? np.str() : np["np"].str();
        r = t;
    } else if (cls == "CanvasItemMaterial") {
        MaterialRes* m = new MaterialRes(Resource::RES_CANVAS_ITEM_MATERIAL, cls);
        m->blend_mode = p["blend_mode"].i();
        m->light_mode = p["light_mode"].i();
        r = m;
    } else if (cls == "ShaderMaterial") {
        MaterialRes* m = new MaterialRes(Resource::RES_SHADER_MATERIAL, cls);
        const Json& sh = p["shader"];
        if (jv_is_ext(sh) && sh["ext"].is_string()) m->shader = sh["ext"].str();
        else if (jv_is_sub(sh)) m->shader = inline_shader_name(ctx, sh["sub"].str());
        r = m;
    } else {
        r = new Resource(Resource::RES_GENERIC, cls);
    }
    r->path = path;
    r->props = &p;
    r->file = ctx;
    return r;
}

ThemeRes* SceneLibrary::build_theme(const Json& p, const SceneFile* ctx) {
    ThemeRes* t = new ThemeRes();
    for (size_t i = 0; i < p.keys().size(); ++i) {
        const std::string& key = p.keys()[i];
        const Json& v = p[key.c_str()];
        if (key == "default_font") {
            Resource* f = resolve(v, ctx);
            t->default_font = f && f->kind == Resource::RES_FONT ? (FontRes*)f : NULL;
        } else if (key == "default_font_size") {
            t->default_font_size = v.i(-1);
        } else if (key == "default_base_scale") {
            t->default_base_scale = jv_float(v);
        } else {
            size_t slash = key.find('/');
            if (slash == std::string::npos) continue;
            std::string type = key.substr(0, slash), rest = key.substr(slash + 1);
            if (rest == "base_type") t->variation_base[type] = v.str();
            else if (rest.compare(0, 6, "fonts/") == 0 || rest.compare(0, 7, "styles/") == 0 ||
                     rest.compare(0, 6, "icons/") == 0)
                t->resources[key] = resolve(v, ctx);
            else t->items[key] = &v;
        }
    }
    return t;
}

/* ---- nodes ---- */

static Node* new_node(const std::string& type) {
    if (type == "Control") return new Control();
    if (type == "TextureRect") return new TextureRect();
    if (type == "NinePatchRect") return new NinePatchRect();
    if (type == "ColorRect") return new ColorRect();
    if (type == "Panel") return new Panel();
    if (type == "PanelContainer") return new PanelContainer();
    if (type == "VBoxContainer") return new BoxContainer(true);
    if (type == "HBoxContainer") return new BoxContainer(false);
    if (type == "BoxContainer") return new BoxContainer(false);
    if (type == "MarginContainer") return new MarginContainer();
    if (type == "CenterContainer") return new CenterContainer();
    if (type == "GridContainer") return new GridContainer();
    if (type == "FlowContainer" || type == "HFlowContainer") return new FlowContainer(false);
    if (type == "VFlowContainer") return new FlowContainer(true);
    if (type == "Label") return new Label();
    if (type == "RichTextLabel") return new RichTextLabel();
    if (type == "Sprite2D") return new Sprite2D();
    if (type == "MeshInstance2D") return new MeshInstance2D();
    if (type == "SubViewport") return new SubViewport();
    if (type == "SubViewportContainer") return new SubViewportContainer();
    if (type == "CanvasGroup") return new CanvasGroup();
    if (type == "AnimatedSprite2D") return new AnimatedSprite2D();
    if (type == "Line2D") return new Line2D();
    if (type == "CPUParticles2D") return new Particles2D(false);
    if (type == "GPUParticles2D") return new Particles2D(true);
    if (type == "LightOccluder2D") return new LightOccluder2D();
    if (type == "Path2D") return new Path2D();
    if (type == "PathFollow2D") return new PathFollow2D();
    if (type == "AnimationPlayer") return new AnimationPlayer();
    if (type == "SpineSprite") return new SpineSprite();
    if (type == "SpineBoneNode") return new SpineFollowNode(false);
    if (type == "SpineSlotNode") return new SpineFollowNode(true);
    if (godot_is_a(type, "Container")) return new Container();
    if (godot_is_a(type, "Control")) return new Control();
    if (godot_is_a(type, "Node2D")) return new Node2D();
    return new Node();
}

Node* SceneLibrary::create_node(const std::string& type) {
    Node* n = new_node(type);
    /* The Godot class: theme lookups and scripts go by it. */
    if (n && n->type.empty()) n->type = type;
    return n;
}

static void ready_tree(Node* n) {
    for (size_t i = 0; i < n->children.size(); ++i) ready_tree(n->children[i]);
    if (n->behavior) n->behavior->ready(n);
}

Node* SceneLibrary::instantiate(const std::string& scene, std::string* error) {
    const SceneFile* f = file(scene);
    if (!f || f->doc["kind"].str() != "scene") {
        if (error) *error = "no scene " + scene;
        return NULL;
    }
    Node* root = instantiate_file(f, 0, error);
    if (root) ready_tree(root);
    return root;
}

static bool is_native(const std::string& type) {
    return type == "Control" || type == "TextureRect" || type == "NinePatchRect" || type == "ColorRect" ||
           type == "Panel" || type == "PanelContainer" || type == "VBoxContainer" || type == "HBoxContainer" ||
           type == "MarginContainer" || type == "CenterContainer" || type == "GridContainer" ||
           type == "FlowContainer" || type == "HFlowContainer" || type == "VFlowContainer" || type == "Label" ||
           type == "RichTextLabel" || type == "Sprite2D" || type == "MeshInstance2D" || type == "SubViewport" || type == "SubViewportContainer" || type == "AnimatedSprite2D" || type == "CanvasGroup" || type == "Line2D" ||
           type == "Node" || type == "Node2D" ||
           type == "Container" || type == "BoxContainer" || type == "Range" || type == "Marker2D" || type == "BackBufferCopy" || type == "CPUParticles2D" ||
           type == "GPUParticles2D" || type == "LightOccluder2D" || type == "SpineMesh2D" || type == "Path2D" || type == "PathFollow2D" || type == "AnimationPlayer" || type == "SpineSprite" || type == "SpineBoneNode" || type == "SpineSlotNode" ||
           type == "Control";
}

Node* SceneLibrary::instantiate_file(const SceneFile* f, int depth, std::string* error) {
    if (depth > 32) {
        if (error) *error = "instancing too deep at " + f->name;
        return NULL;
    }
    const Json& nodes = f->doc["nodes"];
    if (!nodes.size()) {
        if (error) *error = f->name + ": no nodes";
        return NULL;
    }
    PropCtx ctx(this, f);
    Node* root = NULL;
    for (size_t i = 0; i < nodes.size(); ++i) {
        const Json& nd = nodes[i];
        std::string name = nd["name"].str();
        Node* parent = NULL;
        if (i > 0) {
            parent = root->get_node(nd.has("parent") ? nd["parent"].str() : std::string("."));
            if (!parent) {
                log_warn("%s: no parent %s for %s", f->name.c_str(), nd["parent"].str().c_str(), name.c_str());
                continue;
            }
        }
        Node* node = NULL;
        bool existing = false;
        if (nd.has("instance")) {
            const Json& inst = nd["instance"];
            std::string sub = inst["ext"].is_string() ? inst["ext"].str() : std::string();
            const SceneFile* sf = sub.empty() ? NULL : file(sub);
            if (sf) node = instantiate_file(sf, depth + 1, error);
            if (!node) {
                note("missing scene " + sub);
                node = new Control();
                node->type = "Control";
            }
        } else if (nd.has("type")) {
            std::string type = nd["type"].str();
            node = create_node(type);
            node->type = type;
            if (!is_native(type)) note("node " + type);
        } else if (parent) {
            /* A node of an instanced or inherited scene: properties override it. */
            node = parent->get_node(name);
            existing = node != NULL;
            if (!node) {
                log_warn("%s: no node %s/%s to override", f->name.c_str(), nd["parent"].str().c_str(), name.c_str());
                continue;
            }
        } else {
            if (error) *error = f->name + ": root without type";
            return NULL;
        }
        if (!existing) node->name = name;
        const Json& props = nd["props"];
        for (size_t k = 0; k < props.keys().size(); ++k) {
            const std::string& key = props.keys()[k];
            const Json& v = props[key.c_str()];
            if (!node->set(key, v, ctx)) node->extra[key] = &v;
        }
        if (!existing && !node->script.empty() && !node->behavior) {
            g_script_ctx = &ctx;
            node->behavior = create_script(node->script, node);
            g_script_ctx = NULL;
        }
        if (i == 0) {
            root = node;
        } else if (!existing) {
            parent->add_child(node);
            node->owner = root;
            if (nd.has("index")) {
                int want = nd["index"].i(-1);
                std::vector<Node*>& ch = parent->children;
                if (want >= 0 && want < (int)ch.size() - 1) {
                    ch.pop_back();
                    ch.insert(ch.begin() + want, node);
                }
            }
        }
    }
    return root;
}

} /* namespace spire */
