#include "tree.h"
#include "node3d.h"
#include "particles.h"

#include "canvas.h"
#include "controls.h"
#include "scene.h"
#include "script.h"
#include "shaders.h"
#include "tween.h"
#include "viewport.h"
#include "../core/thread.h"

#include <algorithm>
#include <math.h>

namespace spire {

extern Rect2 g_viewport_rect;

struct SceneTree::DrawItem {
    CanvasItem* ci;
    Xform2D xf;
    Color modulate;
    int layer; /* the CanvasLayer's (0: the default canvas) */
    int z;
    int seq;
    bool has_clip;
    Rect2 clip;
    MaterialRes* material;
    int group; /* GROUP_* */
};

/* Canvas groups (clip_children), as Godot's canvas culler orders them: a
 * clipping item outside any group emits a begin marker, then all of its
 * children (show_behind_parent makes no difference), then itself as the
 * mask; "clip and draw" also draws itself normally first. The group forms
 * when the item draws something and children of the same z fall between
 * the marker and the mask; one level only, clipping items inside a group
 * are drawn as ordinary items after their children. */
enum { GROUP_NONE = 0, GROUP_BEGIN = 1, GROUP_MASK = 2 };

struct DrawOrder {
    bool operator()(const SceneTree::DrawItem* a, const SceneTree::DrawItem* b) const {
        if (a->layer != b->layer) return a->layer < b->layer;
        return a->z != b->z ? a->z < b->z : a->seq < b->seq;
    }
};

SceneTree::SceneTree() : items_drawn(0), design_size_(1920, 1080), canvas_(NULL), layer_(0) {
    root_.name = "root";
    root_.type = "Window";
}

SceneTree::~SceneTree() {}

void SceneTree::set_viewport(const Vec2& design_size, const Xform2D& to_screen) {
    design_size_ = design_size;
    to_screen_ = to_screen;
}

void SceneTree::layout_node(Node* n) {
    if (n->type == "SubViewport") {
        /* Its Controls anchor to its canvas. */
        Rect2 saved = g_viewport_rect;
        g_viewport_rect = Rect2(Vec2(0, 0), ((SubViewport*)n)->canvas_size());
        for (size_t i = 0; i < n->children.size(); ++i) layout_node(n->children[i]);
        g_viewport_rect = saved;
        return;
    }
    if (n->is_control()) {
        Control* c = (Control*)n;
        CanvasItem* p = c->parent_item();
        bool placed = p && p->is_container() && Container::sortable(c);
        if (!placed) c->update_rect();
        c->resized();
        if (c->behavior) c->behavior->resized(c);
        if (c->is_container() && c->visible) c->sort_children();
    }
    for (size_t i = 0; i < n->children.size(); ++i) layout_node(n->children[i]);
}

void SceneTree::layout(int passes) {
    g_viewport_rect = Rect2(Vec2(0, 0), design_size_);
    for (int i = 0; i < passes; ++i) {
        Control::begin_layout_pass();
        layout_node(&root_);
    }
}

/* The profile: ms and counts of process and draw by node type or script. */
struct ProfileCost {
    double process_ms, draw_ms;
    unsigned processed, drawn;
    ProfileCost() : process_ms(0), draw_ms(0), processed(0), drawn(0) {}
};
static LoadClock g_profile_clock = NULL;
static std::map<std::string, ProfileCost> g_profile;

static ProfileCost& profile_of(const Node* n, bool behavior) {
    if (behavior) {
        /* a behaviour from code (a screen's FrameDriver): by where it is;
         * the names made once (profile=1 runs on the console: no
         * allocation in each node's timing) */
        static std::map<std::string, std::string> code_keys, script_keys;
        if (n->script.empty()) {
            const std::string& where = n->parent ? n->parent->name : n->name;
            std::map<std::string, std::string>::iterator it = code_keys.find(where);
            if (it == code_keys.end()) it = code_keys.insert(std::make_pair(where, "code in " + where)).first;
            return g_profile[it->second];
        }
        std::map<std::string, std::string>::iterator it = script_keys.find(n->script);
        if (it == script_keys.end()) {
            size_t s = n->script.find_last_of('/');
            it = script_keys.insert(std::make_pair(n->script, "script " + (s == std::string::npos ? n->script : n->script.substr(s + 1)))).first;
        }
        return g_profile[it->second];
    }
    return g_profile[n->type];
}

void SceneTree::set_profile(LoadClock clock) {
    g_profile_clock = clock;
    g_profile.clear();
}

static int g_draw_skip = -1;

void SceneTree::set_draw_skip(int mask) { g_draw_skip = mask; }

static bool skip_draw(const SceneTree::DrawItem& d, int mask);

void SceneTree::log_profile(int frames) {
    if (!g_profile_clock || frames <= 0) return;
    std::vector<std::pair<double, std::string> > order;
    for (std::map<std::string, ProfileCost>::const_iterator it = g_profile.begin(); it != g_profile.end(); ++it)
        order.push_back(std::make_pair(-(it->second.process_ms + it->second.draw_ms), it->first));
    std::sort(order.begin(), order.end());
    for (size_t i = 0; i < order.size() && i < 12; ++i) {
        const ProfileCost& c = g_profile[order[i].second];
        log_info("profile: %-28s process %.2f ms (%u), draw %.2f ms (%u) per frame", order[i].second.c_str(),
                 c.process_ms / frames, c.processed / frames, c.draw_ms / frames, c.drawn / frames);
    }
    g_profile.clear();
}

void SceneTree::process_node(Node* n, double dt) {
    g_trace_node = n;
    if (n->queued_for_deletion()) return;
    if (n->flags & NF_SCRIPT_UNPORTED) {
        n->flags &= ~(u32)NF_SCRIPT_UNPORTED;
        if (!n->behavior) note_unported_script(n->script);
    }
    if (n->is_canvas_item() && !((CanvasItem*)n)->visible) return;
    if (g_profile_clock && (n->behavior || (n->flags & NF_PROCESS))) {
        double t0 = g_profile_clock();
        if (n->behavior) {
            n->behavior->process(n, dt);
            double t1 = g_profile_clock();
            ProfileCost& c = profile_of(n, true);
            c.process_ms += t1 - t0;
            ++c.processed;
            t0 = t1;
        }
        if (n->flags & NF_PROCESS) {
            n->process(dt);
            ProfileCost& c = profile_of(n, false);
            c.process_ms += g_profile_clock() - t0;
            ++c.processed;
        }
        for (size_t i = 0; i < n->children.size(); ++i) process_node(n->children[i], dt);
        return;
    }
    if (n->behavior) n->behavior->process(n, dt);
    if (n->flags & NF_PROCESS) n->process(dt);
    for (size_t i = 0; i < n->children.size(); ++i) process_node(n->children[i], dt);
}

static void flush_deleted(Node* n) {
    for (size_t i = n->children.size(); i-- > 0;) {
        Node* child = n->children[i];
        if (child->queued_for_deletion()) {
            n->remove_child(child);
            delete child;
        } else {
            flush_deleted(child);
        }
    }
}

float SceneTree::time_scale_ = 1, SceneTree::hit_stop_t_ = 0, SceneTree::hit_stop_len_ = 0;
int SceneTree::hit_stop_trans_ = 0;

void SceneTree::hit_stop(float seconds, int trans) {
    time_scale_ = 0.1f;
    hit_stop_t_ = 0;
    hit_stop_len_ = seconds > 0 ? seconds : 0.01f;
    hit_stop_trans_ = trans;
}

void SceneTree::process(double dt) {
    double t = dt * time_scale_;
    g_shader_time += t;
    /* the particles' steps queued by their process() and run together
     * (on the worker threads when there are some), before frees */
    Particles2D::begin_frame();
    process_node(&root_, t);
    double p0 = g_profile_clock ? g_profile_clock() : 0;
    Particles2D::end_frame();
    if (g_profile_clock) {
        ProfileCost& c = g_profile["particles' steps (workers)"];
        c.process_ms += g_profile_clock() - p0;
        ++c.processed;
    }
    if (Node::frees_pending > 0) {
        Node::frees_pending = 0;
        flush_deleted(&root_);
    }
    /* NHitStop.HitStopTask: the next frame's scale from the real time gone */
    if (hit_stop_len_ > 0) {
        hit_stop_t_ += (float)dt;
        if (hit_stop_t_ > hit_stop_len_) {
            time_scale_ = 1;
            hit_stop_len_ = 0;
        } else {
            float k = tween_interpolate((TweenTrans)hit_stop_trans_, EASE_IN, hit_stop_t_ / hit_stop_len_, 0, 1, 1);
            time_scale_ = 0.1f + k * 0.9f < 1 ? 0.1f + k * 0.9f : 1.0f;
        }
    }
}

static Rect2 xform_rect(const Xform2D& xf, const Rect2& r) {
    Vec2 p[4] = { xf.xform(r.pos), xf.xform(Vec2(r.pos.x + r.size.x, r.pos.y)), xf.xform(r.end()),
                  xf.xform(Vec2(r.pos.x, r.pos.y + r.size.y)) };
    float x0 = p[0].x, y0 = p[0].y, x1 = p[0].x, y1 = p[0].y;
    for (int i = 1; i < 4; ++i) {
        x0 = minf(x0, p[i].x);
        y0 = minf(y0, p[i].y);
        x1 = maxf(x1, p[i].x);
        y1 = maxf(y1, p[i].y);
    }
    return Rect2(x0, y0, x1 - x0, y1 - y0);
}

/* Does the item draw anything (Godot: has it any commands)? */
static bool draws_something(Canvas& canvas, SceneTree::DrawItem& d) {
    long before = canvas.draw_calls();
    bool was = canvas.count_only();
    canvas.set_count_only(true);
    canvas.set_item(d.xf, d.modulate, BLEND_MIX, d.ci->effective_texture_flags());
    d.ci->draw(canvas);
    canvas.set_count_only(was);
    return canvas.draw_calls() != before;
}

void SceneTree::collect(Node* n, const Xform2D& parent_xf, const Color& parent_mod, int z, const Rect2* clip,
                        MaterialRes* parent_mat, bool in_group, std::vector<DrawItem>& out) {
    if (n->type == "SubViewport") {
        /* drawn into its texture (render_viewport), not here */
        viewports_.push_back((SubViewport*)n);
        return;
    }
    if (!n->is_canvas_item()) {
        /* CanvasItems under a plain Node start again from the canvas; a
         * CanvasLayer's are a canvas of their own, drawn in layer order. */
        int was = layer_;
        if (n->type == "CanvasLayer") layer_ = ((CanvasLayer*)n)->layer;
        for (size_t i = 0; i < n->children.size(); ++i)
            collect(n->children[i], to_screen_, Color(), 0, NULL, NULL, in_group, out);
        layer_ = was;
        return;
    }
    CanvasItem* ci = (CanvasItem*)n;
    if (!ci->visible) return;
    Color mod = parent_mod * ci->modulate;
    if (mod.a < 0.007f) return;
    Xform2D xf = (ci->top_level ? to_screen_ : parent_xf) * ci->get_transform();
    Rect2 my_clip;
    const Rect2* child_clip = clip;
    if (ci->clips()) {
        Rect2 g = xform_rect(xf, ci->item_rect());
        my_clip = clip ? clip->intersection(g) : g;
        if (my_clip.size.x < 0.5f || my_clip.size.y < 0.5f) return;
        my_clip.pos = Vec2(floorf(my_clip.pos.x + 0.5f), floorf(my_clip.pos.y + 0.5f));
        my_clip.size = Vec2(floorf(my_clip.size.x + 0.5f), floorf(my_clip.size.y + 0.5f));
        child_clip = &my_clip;
    }
    int my_z = ci->z_as_relative ? z + ci->z_index : ci->z_index;
    if (my_z < -4096) my_z = -4096;
    if (my_z > 4096) my_z = 4096;
    MaterialRes* mat = ci->use_parent_material && parent_mat ? parent_mat : ci->material;

    DrawItem d;
    d.ci = ci;
    d.xf = xf;
    d.modulate = mod * ci->self_modulate;
    d.layer = layer_;
    d.z = my_z;
    d.seq = (int)out.size();
    d.has_clip = child_clip != NULL;
    if (child_clip) d.clip = *child_clip;
    d.material = mat;
    d.group = GROUP_NONE;
    /* Godot forms the group only for an item that draws something; an
     * empty clipping item is an ordinary one (and does not count as an
     * enclosing group). */
    if (ci->clip_children != 0 && (in_group || draws_something(*canvas_, d))) {
        if (!in_group) {
            if (ci->clip_children == 2) out.push_back(d);
            d.group = GROUP_BEGIN;
            out.push_back(d);
        }
        for (size_t i = 0; i < n->children.size(); ++i) collect(n->children[i], xf, mod, my_z, child_clip, mat, true, out);
        d.group = in_group ? GROUP_NONE : GROUP_MASK;
        out.push_back(d);
        return;
    }
    for (size_t i = 0; i < n->children.size(); ++i) {
        Node* c = n->children[i];
        if (c->is_canvas_item() && ((CanvasItem*)c)->show_behind_parent)
            collect(c, xf, mod, my_z, child_clip, mat, in_group, out);
    }
    out.push_back(d);
    for (size_t i = 0; i < n->children.size(); ++i) {
        Node* c = n->children[i];
        if (!(c->is_canvas_item() && ((CanvasItem*)c)->show_behind_parent))
            collect(c, xf, mod, my_z, child_clip, mat, in_group, out);
    }
}

/* The screen area a group's mask covers. */
static Rect2 group_bounds(const SceneTree::DrawItem& d) {
    Rect2 r = d.ci->item_rect();
    if (r.size.x <= 0 || r.size.y <= 0) r = Rect2(-1e6f, -1e6f, 2e6f, 2e6f);
    else r = xform_rect(d.xf, r);
    if (d.has_clip) r = r.intersection(d.clip);
    return Rect2(r.pos.x - 2, r.pos.y - 2, r.size.x + 4, r.size.y + 4);
}

/* A SubViewport's children into its texture: its canvas scaled to the
 * texture's pixels (its size at the screen's scale, within the screen,
 * in multiples of 32) at the screen's top left. */
void SceneTree::render_viewport(Canvas& canvas, SubViewport* vp) {
    if (!vp->wants_render()) return;
    Vec2 units = vp->canvas_size();
    if (units.x <= 0 || units.y <= 0) return;
    float k = sqrtf(to_screen_.x.x * to_screen_.x.x + to_screen_.x.y * to_screen_.x.y);
    Vec2 screen = design_size_ * k;
    float px = vp->width * k, py = vp->height * k;
    float fit = minf(1.0f, minf(screen.x / px, screen.y / py));
    int w = ((int)ceilf(px * fit) + 31) & ~31, h = ((int)ceilf(py * fit) + 31) & ~31;
    if (w > (int)screen.x) w = (int)screen.x & ~31;
    if (h > (int)screen.y) h = (int)screen.y & ~31;
    if (w <= 0 || h <= 0) return;
    Xform2D xf(Vec2(w / units.x, 0), Vec2(0, h / units.y), Vec2(0, 0));
    std::vector<DrawItem> items;
    int was = layer_;
    layer_ = 0; /* a viewport's canvas has its own layers */
    for (size_t i = 0; i < vp->children.size(); ++i) collect(vp->children[i], xf, Color(), 0, NULL, NULL, false, items);
    layer_ = was;
    if (!canvas.renderer()->begin_capture(w, h)) return;
    if (!vp->transparent_bg) {
        /* Godot's default clear colour */
        canvas.set_item(Xform2D(), Color(), BLEND_MIX, 0);
        canvas.set_clip(NULL);
        canvas.draw_rect(Rect2(Vec2(0, 0), Vec2((float)w, (float)h)), Color(0.3f, 0.3f, 0.3f, 1));
    }
    /* the 3D under it first (Godot's 3D pass, then its canvas) */
    draw_3d(canvas, vp, units, xf);
    draw_items(canvas, items);
    vp->rendered(canvas.renderer()->end_capture(vp->texture));
}

void SceneTree::render(Canvas& canvas) {
    canvas.begin_frame();
    if (SceneLibrary* lib = SceneLibrary::current())
        if (g_draw_skip < 0) lib->warm_step(canvas);
    skipped.clear();
    items_drawn = 0;
    std::vector<DrawItem> items;
    canvas_ = &canvas;
    viewports_.clear();
    layer_ = 0;
    collect(&root_, to_screen_, Color(), 0, NULL, NULL, false, items);
    /* the viewports first (one inside another: found as its turn comes) */
    for (size_t i = 0; i < viewports_.size(); ++i) render_viewport(canvas, viewports_[i]);
    draw_items(canvas, items);
    canvas.set_clip(NULL);
    canvas.end_frame();
}

static bool skip_draw(const SceneTree::DrawItem& d, int mask) {
    const std::string& t = d.ci->type;
    if ((mask & SceneTree::SKIP_TEXT) && (t == "Label" || t == "RichTextLabel")) return true;
    if ((mask & SceneTree::SKIP_PARTICLES) && (t == "GPUParticles2D" || t == "CPUParticles2D")) return true;
    if ((mask & SceneTree::SKIP_SPINE) && t == "SpineSprite") return true;
    if ((mask & SceneTree::SKIP_SHADERS) && d.material && d.material->kind == Resource::RES_SHADER_MATERIAL) return true;
    if ((mask & SceneTree::SKIP_IMAGES) && (t == "TextureRect" || t == "Sprite2D" || t == "NinePatchRect")) return true;
    if (mask & SceneTree::SKIP_BACKGROUND)
        for (const Node* n = d.ci; n; n = n->parent)
            if (n->name == "BgContainer") return true;
    return false;
}

void SceneTree::draw_items(Canvas& canvas, std::vector<DrawItem>& items) {
    std::vector<DrawItem*> order(items.size());
    for (size_t i = 0; i < items.size(); ++i) {
        items[i].seq = (int)i;
        order[i] = &items[i];
    }
    std::stable_sort(order.begin(), order.end(), DrawOrder());
    DrawItem* group_mask = NULL; /* the open group's mask item */
    for (size_t i = 0; i < order.size(); ++i) {
        DrawItem& d = *order[i];
        int extra_flags = 0;
        if (d.group == GROUP_BEGIN) {
            size_t j = i + 1;
            while (j < order.size() && !(order[j]->ci == d.ci && order[j]->group == GROUP_MASK)) ++j;
            if (j >= order.size()) continue;
            DrawItem& m = *order[j];
            /* No children of this z, or nothing to mask with: no group. The
             * mask is not drawn (clip and draw drew the item already) and
             * the children draw as usual. */
            if (group_mask || j == i + 1 || !draws_something(canvas, m) ||
                !canvas.renderer()->begin_group(group_bounds(m))) {
                m.ci = NULL;
                continue;
            }
            group_mask = &m;
            continue;
        }
        if (!d.ci) continue;
        if (g_draw_skip > 0 && d.group == GROUP_NONE && skip_draw(d, g_draw_skip)) continue;
        if (d.group == GROUP_MASK) {
            if (&d != group_mask) continue;
            canvas.renderer()->end_group();
            group_mask = NULL;
            extra_flags = DRAW_GROUP;
        }
        BlendMode blend = BLEND_MIX;
        if (d.material && d.material->kind == Resource::RES_SHADER_MATERIAL && !shader_begin(canvas, d.material, d.ci)) {
            /* Godot shaders are ported one by one (engine/scene/shaders); an
             * unknown one is not drawn rather than drawn wrong. */
            ++skipped["shader " + d.material->shader];
            continue;
        }
        if (d.material && d.material->kind == Resource::RES_CANVAS_ITEM_MATERIAL) {
            static const BlendMode modes[] = { BLEND_MIX, BLEND_ADD, BLEND_SUB, BLEND_MUL, BLEND_PREMULT_ALPHA };
            int m = d.material->blend_mode;
            blend = m >= 0 && m < 5 ? modes[m] : BLEND_MIX;
        }
        shader_blend(d.material, &blend);
        canvas.set_item(d.xf, d.modulate, blend, d.ci->effective_texture_flags() | extra_flags);
        canvas.set_clip(d.has_clip ? &d.clip : NULL);
        g_trace_node = d.ci;
        if (g_profile_clock) {
            double t0 = g_profile_clock();
            d.ci->draw(canvas);
            ProfileCost& c = profile_of(d.ci, false);
            c.draw_ms += g_profile_clock() - t0;
            ++c.drawn;
        } else {
            d.ci->draw(canvas);
        }
        shader_end(canvas, d.material);
        ++items_drawn;
    }
    if (group_mask) canvas.renderer()->end_group();
}

} /* namespace spire */
