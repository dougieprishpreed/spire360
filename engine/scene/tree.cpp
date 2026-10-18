#include "tree.h"

#include "canvas.h"
#include "controls.h"
#include "script.h"
#include "shaders.h"

#include <algorithm>
#include <math.h>

namespace spire {

extern Rect2 g_viewport_rect;

struct SceneTree::DrawItem {
    CanvasItem* ci;
    Xform2D xf;
    Color modulate;
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
        return a->z != b->z ? a->z < b->z : a->seq < b->seq;
    }
};

SceneTree::SceneTree() : items_drawn(0), design_size_(1920, 1080), canvas_(NULL) {
    root_.name = "root";
    root_.type = "Window";
}

SceneTree::~SceneTree() {}

void SceneTree::set_viewport(const Vec2& design_size, const Xform2D& to_screen) {
    design_size_ = design_size;
    to_screen_ = to_screen;
}

void SceneTree::layout_node(Node* n) {
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

void SceneTree::process_node(Node* n, double dt) {
    if (n->queued_for_deletion()) return;
    if (n->is_canvas_item() && !((CanvasItem*)n)->visible) return;
    if (n->behavior) n->behavior->process(n, dt);
    n->process(dt);
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

void SceneTree::process(double dt) {
    g_shader_time += dt;
    process_node(&root_, dt);
    flush_deleted(&root_);
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
    canvas.set_count_only(true);
    canvas.set_item(d.xf, d.modulate, BLEND_MIX, d.ci->effective_texture_flags());
    d.ci->draw(canvas);
    canvas.set_count_only(false);
    return canvas.draw_calls() != before;
}

void SceneTree::collect(Node* n, const Xform2D& parent_xf, const Color& parent_mod, int z, const Rect2* clip,
                        MaterialRes* parent_mat, bool in_group, std::vector<DrawItem>& out) {
    if (!n->is_canvas_item()) {
        /* CanvasItems under a plain Node start again from the canvas. */
        for (size_t i = 0; i < n->children.size(); ++i)
            collect(n->children[i], to_screen_, Color(), 0, NULL, NULL, in_group, out);
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

void SceneTree::render(Canvas& canvas) {
    skipped.clear();
    items_drawn = 0;
    std::vector<DrawItem> items;
    canvas_ = &canvas;
    collect(&root_, to_screen_, Color(), 0, NULL, NULL, false, items);
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
        d.ci->draw(canvas);
        shader_end(canvas, d.material);
        ++items_drawn;
    }
    if (group_mask) canvas.renderer()->end_group();
    canvas.set_clip(NULL);
}

} /* namespace spire */
