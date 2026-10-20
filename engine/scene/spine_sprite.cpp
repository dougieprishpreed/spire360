#include "spine_sprite.h"

#include "../core/pack.h"
#include "canvas.h"
#include "scene.h"
#include "shaders.h"

#include <algorithm>
#include <map>
#include <math.h>

namespace spire {

SpineDataRes::~SpineDataRes() { delete data_; }

/* The material properties a SpineSprite or SpineSlotNode has, per blend
 * mode: true when key is one of them (out[i] set, NULL unless a material). */
static bool slot_material_prop(const std::string& key, const Json& v, const PropCtx& ctx, MaterialRes* out[4]) {
    static const char* const MATERIALS[4] = { "normal_material", "additive_material", "multiply_material",
                                              "screen_material" };
    for (int i = 0; i < 4; ++i)
        if (key == MATERIALS[i]) {
            Resource* r = ctx.resource(v);
            out[i] = r && (r->kind == Resource::RES_CANVAS_ITEM_MATERIAL || r->kind == Resource::RES_SHADER_MATERIAL)
                         ? (MaterialRes*)r
                         : NULL;
            return true;
        }
    return false;
}

static bool read_json(const Pack* pack, const std::string& name, Json& out, std::string* err) {
    std::vector<u8> raw;
    if (!pack || !pack->read(name.c_str(), raw)) {
        *err = "not in the pack";
        return false;
    }
    out = Json::parse(raw.empty() ? "" : (const char*)&raw[0], raw.size(), err);
    return err->empty();
}

const SpSkeletonData* SpineDataRes::data(SceneLibrary* lib) {
    if (tried_) return data_;
    tried_ = true;
    if (!lib) return NULL;
    std::string err;
    Json atlas_doc;
    if (!read_json(lib->pack(), atlas_path, atlas_doc, &err) || !atlas_.load(atlas_doc, &err)) {
        log_warn("spine atlas %s: %s", atlas_path.c_str(), err.c_str());
        lib->note("spine atlas " + atlas_path);
        return NULL;
    }
    std::vector<u8> raw;
    if (!lib->pack() || !lib->pack()->read(skeleton_path.c_str(), raw) || raw.empty()) {
        log_warn("spine skeleton %s: not in the pack", skeleton_path.c_str());
        lib->note("spine skeleton " + skeleton_path);
        return NULL;
    }
    SpSkeletonData* d = new SpSkeletonData();
    if (!d->load(&raw[0], raw.size(), &atlas_, &err)) {
        log_warn("spine skeleton %s: %s", skeleton_path.c_str(), err.c_str());
        lib->note("spine skeleton " + skeleton_path);
        delete d;
        return NULL;
    }
    for (size_t i = 0; i < d->unsupported.size(); ++i) lib->note("spine " + d->unsupported[i]);
    for (size_t i = 0; i < atlas_.pages.size(); ++i) {
        Resource* r = lib->ext(atlas_.pages[i].texture);
        pages_.push_back(r && r->kind == RES_IMAGE_TEXTURE ? (ImageTextureRes*)r : NULL);
    }
    data_ = d;
    return data_;
}

SpineSprite::SpineSprite() : data_res(NULL), time_scale(1), lib_(NULL), skel_(NULL), custom_skin_(NULL), failed_(false) {
    for (int i = 0; i < 4; ++i) slot_material[i] = NULL;
}

SpineSprite::~SpineSprite() {
    delete skel_;
    delete custom_skin_;
}

bool SpineSprite::set(const std::string& key, const Json& v, const PropCtx& ctx) {
    if (key == "skeleton_data_res") {
        Resource* r = ctx.resource(v);
        data_res = r && r->kind == Resource::RES_SPINE_DATA ? (SpineDataRes*)r : NULL;
        lib_ = ctx.lib;
        return true;
    }
    if (key == "time_scale") {
        time_scale = (float)v.num(1);
        return true;
    }
    if (slot_material_prop(key, v, ctx, slot_material)) return true;
    /* Editor preview settings. */
    if (key == "preview_skin" || key == "preview_animation" || key == "preview_frame" || key == "preview_time")
        return true;
    return Node2D::set(key, v, ctx);
}

bool SpineSprite::ensure() {
    if (skel_) return true;
    if (failed_ || !data_res) return false;
    const SpSkeletonData* d = data_res->data(lib_);
    if (!d) {
        failed_ = true;
        return false;
    }
    skel_ = new SpSkeleton(d); /* y-down, like spine-godot */
    skel_->update_world_transform(SP_PHYSICS_RESET);
    return true;
}

SpSkeleton* SpineSprite::skeleton() { return ensure() ? skel_ : NULL; }

SpAnimationState* SpineSprite::animation_state() { return ensure() ? &state_ : NULL; }

bool SpineSprite::set_animation(const std::string& name, bool loop, int track) {
    if (!ensure()) return false;
    const SpAnimation* a = skel_->data()->find_animation(name);
    if (!a) return false;
    state_.set_animation(track, a, loop);
    return true;
}

void SpineSprite::set_track_fraction(int track, float fraction) {
    const SpAnimation* a = state_.current(track);
    if (a) state_.set_track_time(track, a->duration * fraction);
}

void SpineSprite::set_data(SceneLibrary* lib, SpineDataRes* res) {
    delete skel_;
    skel_ = NULL;
    failed_ = false;
    state_ = SpAnimationState();
    data_res = res;
    lib_ = lib;
}

bool SpineSprite::set_skin(const std::string& name) {
    if (!ensure()) return false;
    const SpSkin* skin = skel_->data()->find_skin(name);
    if (!skin) return false;
    skel_->set_skin(skin);
    skel_->set_slots_to_setup_pose();
    state_.apply(*skel_);
    skel_->update_world_transform(SP_PHYSICS_UPDATE);
    return true;
}

static void add_unique(std::vector<int>& to, const std::vector<int>& from) {
    for (size_t i = 0; i < from.size(); ++i)
        if (std::find(to.begin(), to.end(), from[i]) == to.end()) to.push_back(from[i]);
}

bool SpineSprite::set_skins(const std::vector<std::string>& names) {
    if (!ensure()) return false;
    SpSkin* skin = new SpSkin();
    skin->name = "custom-skin";
    bool any = false;
    for (size_t i = 0; i < names.size(); ++i) {
        const SpSkin* s = skel_->data()->find_skin(names[i]);
        if (!s) continue;
        any = true;
        /* Skin.AddSkin: its bones and constraints once, its attachments over
         * any with the same slot and name */
        add_unique(skin->bones, s->bones);
        add_unique(skin->ik, s->ik);
        add_unique(skin->transform, s->transform);
        add_unique(skin->path, s->path);
        add_unique(skin->physics, s->physics);
        for (std::map<std::string, SpAttachment*>::const_iterator it = s->lookup.begin(); it != s->lookup.end(); ++it)
            skin->lookup[it->first] = it->second;
    }
    if (!any) {
        delete skin;
        return false;
    }
    skel_->set_skin(skin);
    delete custom_skin_;
    custom_skin_ = skin;
    skel_->set_slots_to_setup_pose();
    state_.apply(*skel_);
    skel_->update_world_transform(SP_PHYSICS_UPDATE);
    return true;
}

bool SpineSprite::add_animation(const std::string& name, bool loop, float delay, int track) {
    if (!ensure()) return false;
    const SpAnimation* a = skel_->data()->find_animation(name);
    if (!a) return false;
    state_.add_animation(track, a, loop, delay);
    return true;
}

void SpineSprite::add_empty_animation(int track) {
    if (ensure()) state_.set_empty_animation(track);
}

void SpineSprite::set_state_time_scale(float scale) { state_.time_scale = scale; }

bool SpineSprite::has_animation(const std::string& name) {
    return ensure() && skel_->data()->find_animation(name) != NULL;
}

void SpineSprite::process(double dt) {
    if (!ensure()) return;
    float d = (float)dt * time_scale;
    state_.update(d);
    state_.apply(*skel_);
    drive_bones();
    skel_->update(d);
    skel_->update_world_transform(SP_PHYSICS_UPDATE);
    follow_bones();
}

bool SpineFollowNode::set(const std::string& key, const Json& v, const PropCtx& ctx) {
    if (key == "bone_name" || key == "slot_name") {
        target = v.str();
        index = -2;
        slot_index = -2;
        return true;
    }
    if (is_slot && slot_material_prop(key, v, ctx, slot_material)) return true;
    if (key == "bone_mode") {
        drive = v.i() == 1;
        return true;
    }
    if (key == "enabled") {
        enabled = v.b(true);
        return true;
    }
    if (key == "debug_thickness" || key == "debug_color") return true;
    return Node2D::set(key, v, ctx);
}

SpineFollowNode* SpineSprite::follow_node(Node* n) {
    if (n->type != "SpineBoneNode" && n->type != "SpineSlotNode") return NULL;
    SpineFollowNode* f = (SpineFollowNode*)n;
    if (f->index == -2) {
        const SpSkeletonData* d = skel_->data();
        if (f->is_slot) {
            int s = d->find_slot(f->target);
            f->index = s >= 0 ? d->slots[s].bone : -1;
        } else {
            f->index = d->find_bone(f->target);
        }
        if (f->index < 0) log_warn("%s: no %s %s", f->path().c_str(), f->is_slot ? "slot" : "bone", f->target.c_str());
    }
    return f->index >= 0 && f->enabled ? f : NULL;
}

/* Drive: the node's position and rotation (sprite space) become the bone's
 * world position and rotation, written as its local pose. */
void SpineSprite::drive_bones() {
    for (size_t i = 0; i < children.size(); ++i) {
        SpineFollowNode* f = follow_node(children[i]);
        if (!f || !f->drive || f->is_slot) continue;
        SpBone& b = skel_->bones[f->index];
        float pa, pb, pc, pd, pwx, pwy;
        if (b.parent >= 0) {
            const SpBone& p = skel_->bones[b.parent];
            pa = p.a;
            pb = p.b;
            pc = p.c;
            pd = p.d;
            pwx = p.world_x;
            pwy = p.world_y;
        } else {
            pa = skel_->sx();
            pb = 0;
            pc = 0;
            pd = skel_->sy();
            pwx = skel_->x;
            pwy = skel_->y;
        }
        float det = pa * pd - pb * pc;
        if (det == 0) continue;
        float ia = pd / det, ib = -pb / det, ic = -pc / det, id = pa / det;
        float dx = f->position.x - pwx, dy = f->position.y - pwy;
        b.x = ia * dx + ib * dy;
        b.y = ic * dx + id * dy;
        float wx = cosf(f->rotation), wy = sinf(f->rotation);
        float lx = ia * wx + ib * wy, ly = ic * wx + id * wy;
        b.rotation = atan2f(ly, lx) * 57.2957795f;
    }
}

/* Follow: the node takes the bone's world transform (sprite space). */
void SpineSprite::follow_bones() {
    for (size_t i = 0; i < children.size(); ++i) {
        SpineFollowNode* f = follow_node(children[i]);
        if (!f || f->drive) continue;
        const SpBone& b = skel_->bones[f->index];
        f->position = Vec2(b.world_x, b.world_y);
        f->rotation = atan2f(b.c, b.a);
        f->scale = Vec2(sqrtf(b.a * b.a + b.c * b.c), sqrtf(b.b * b.b + b.d * b.d));
        f->skew = atan2f(b.d, b.b) - f->rotation - 1.57079633f;
        while (f->skew > 3.14159265f) f->skew -= 6.28318531f;
        while (f->skew < -3.14159265f) f->skew += 6.28318531f;
    }
}

void SpineSprite::draw(Canvas& c) {
    if (!ensure()) return;
    BlendMode item_blend = c.blend();
    const SpSkeleton& s = *skel_;
    /* The slot nodes with materials of their own, by slot. */
    slot_nodes_.clear();
    for (size_t i = 0; i < children.size(); ++i) {
        Node* n = children[i];
        if (n->type != "SpineSlotNode") continue;
        SpineFollowNode* f = (SpineFollowNode*)n;
        if (!f->slot_material[0] && !f->slot_material[1] && !f->slot_material[2] && !f->slot_material[3]) continue;
        if (f->slot_index == -2) f->slot_index = s.data()->find_slot(f->target);
        if (f->slot_index < 0) continue;
        if ((int)slot_nodes_.size() <= f->slot_index) slot_nodes_.resize((size_t)f->slot_index + 1, NULL);
        if (!slot_nodes_[(size_t)f->slot_index]) slot_nodes_[(size_t)f->slot_index] = f;
    }
    static const int QUAD[6] = { 0, 1, 2, 2, 3, 0 };
    for (size_t n = 0; n < s.draw_order.size(); ++n) {
        int si = s.draw_order[n];
        const SpSlot& slot = s.slots[si];
        const SpAttachment* a = slot.attachment;
        if (!s.bones[slot.data->bone].active) {
            clipper_.end_slot(si);
            continue;
        }
        if (a && a->type == SP_ATT_CLIPPING) {
            clipper_.start(s, slot, *a);
            continue;
        }
        if (!a || (a->type != SP_ATT_REGION && a->type != SP_ATT_MESH)) {
            clipper_.end_slot(si);
            continue;
        }
        int frame = a->frame(slot.sequence_index);
        const SpAtlasRegion* region = frame < (int)a->regions.size() ? a->regions[frame] : NULL;
        Color col = s.color * slot.color * a->color;
        ImageTextureRes* img = region ? data_res->page(region->page) : NULL;
        if (!img || col.a <= 0) {
            clipper_.end_slot(si);
            continue;
        }
        s.world_vertices(slot, world_);
        const std::vector<float>& uvs = a->frame_uvs[frame];
        size_t nv = world_.size() / 2;
        if (uvs.size() < nv * 2) {
            clipper_.end_slot(si);
            continue;
        }
        if (a->type == SP_ATT_REGION) idx_.assign(QUAD, QUAD + 6);
        else idx_ = a->triangles;
        const std::vector<float>* pos = &world_;
        const std::vector<float>* uv = &uvs;
        if (clipper_.active()) {
            clipper_.clip(world_, uvs, idx_, cpos_, cuv_, ctris_);
            pos = &cpos_;
            uv = &cuv_;
            idx_ = ctris_;
            nv = cpos_.size() / 2;
        }
        v_.resize(nv);
        uv_.resize(nv);
        col_.assign(nv, col);
        for (size_t i = 0; i < nv; ++i) {
            v_[i] = Vec2((*pos)[i * 2], (*pos)[i * 2 + 1]);
            uv_[i] = Vec2((*uv)[i * 2], (*uv)[i * 2 + 1]);
        }
        /* SpineSprite.update_meshes: the sprite's material for the slot's
         * blend mode when it has one (the scene's choice: Neow's room draws
         * its multiply and screen slots as normal ones), else the mode's. */
        int bm = slot.data->blend >= 0 && slot.data->blend < 4 ? slot.data->blend : 0;
        const MaterialRes* m = si < (int)slot_nodes_.size() && slot_nodes_[(size_t)si]
                                   ? slot_nodes_[(size_t)si]->slot_material[bm]
                                   : NULL;
        if (!m) m = slot_material[bm];
        if (m && m->kind == Resource::RES_SHADER_MATERIAL) {
            /* A shader: drawn with it (its render_mode's blend), or not at
             * all when it is not ported (as the tree does). */
            if (!shader_begin(c, m, this)) {
                clipper_.end_slot(si);
                continue;
            }
            BlendMode sb = BLEND_MIX;
            shader_blend(m, &sb);
            c.set_blend(sb);
            if (!idx_.empty()) c.draw_triangles(img, v_, uv_, col_, idx_);
            shader_end(c, m);
            clipper_.end_slot(si);
            continue;
        }
        if (m && m->kind == Resource::RES_CANVAS_ITEM_MATERIAL) {
            static const BlendMode modes[] = { BLEND_MIX, BLEND_ADD, BLEND_SUB, BLEND_MUL, BLEND_PREMULT_ALPHA };
            c.set_blend(m->blend_mode >= 0 && m->blend_mode < 5 ? modes[m->blend_mode] : BLEND_MIX);
        } else {
            switch (slot.data->blend) {
            case SP_BLEND_ADDITIVE: c.set_blend(BLEND_ADD); break;
            case SP_BLEND_MULTIPLY: c.set_blend(BLEND_MUL); break;
            case SP_BLEND_SCREEN: c.set_blend(BLEND_ADD); break; /* not exact (no screen blend yet) */
            default: c.set_blend(item_blend); break;
            }
        }
        if (!idx_.empty()) c.draw_triangles(img, v_, uv_, col_, idx_);
        clipper_.end_slot(si);
    }
    clipper_.end();
    c.set_blend(item_blend);
}

} /* namespace spire */
