#include "spine_sprite.h"

#include "../core/pack.h"
#include "canvas.h"
#include "scene.h"

namespace spire {

SpineDataRes::~SpineDataRes() { delete data_; }

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
    Json atlas_doc, skel_doc;
    if (!read_json(lib->pack(), atlas_path, atlas_doc, &err) || !atlas_.load(atlas_doc, &err)) {
        log_warn("spine atlas %s: %s", atlas_path.c_str(), err.c_str());
        lib->note("spine atlas " + atlas_path);
        return NULL;
    }
    if (!read_json(lib->pack(), skeleton_path, skel_doc, &err)) {
        log_warn("spine skeleton %s: %s", skeleton_path.c_str(), err.c_str());
        lib->note("spine skeleton " + skeleton_path);
        return NULL;
    }
    SpSkeletonData* d = new SpSkeletonData();
    if (!d->load(skel_doc, &atlas_, &err)) {
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

SpineSprite::SpineSprite() : data_res(NULL), time_scale(1), lib_(NULL), skel_(NULL), failed_(false) {}

SpineSprite::~SpineSprite() { delete skel_; }

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
    skel_ = new SpSkeleton(d);
    skel_->scale_y = -1.0f; /* Spine is y-up */
    skel_->update_world_transform();
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

void SpineSprite::process(double dt) {
    if (!ensure()) return;
    state_.update((float)dt * time_scale);
    state_.apply(*skel_);
    skel_->update_world_transform();
}

void SpineSprite::draw(Canvas& c) {
    if (!ensure()) return;
    BlendMode item_blend = c.blend();
    const SpSkeleton& s = *skel_;
    static const int QUAD[6] = { 0, 1, 2, 2, 3, 0 };
    for (size_t n = 0; n < s.draw_order.size(); ++n) {
        const SpSlot& slot = s.slots[s.draw_order[n]];
        const SpAttachment* a = slot.attachment;
        if (!a || (a->type != SP_ATT_REGION && a->type != SP_ATT_MESH) || !a->region) continue;
        if (!s.bones[slot.data->bone].active) continue;
        Color col = s.color * slot.color * a->color;
        if (col.a <= 0) continue;
        ImageTextureRes* img = data_res->page(a->region->page);
        if (!img) continue;
        s.world_vertices(slot, world_);
        size_t nv = world_.size() / 2;
        if (a->uvs.size() < nv * 2) continue;
        v_.resize(nv);
        uv_.resize(nv);
        col_.assign(nv, col);
        for (size_t i = 0; i < nv; ++i) {
            v_[i] = Vec2(world_[i * 2], world_[i * 2 + 1]);
            uv_[i] = Vec2(a->uvs[i * 2], a->uvs[i * 2 + 1]);
        }
        if (a->type == SP_ATT_REGION) idx_.assign(QUAD, QUAD + 6);
        else idx_ = a->triangles;
        switch (slot.data->blend) {
        case SP_BLEND_ADDITIVE: c.set_blend(BLEND_ADD); break;
        case SP_BLEND_MULTIPLY: c.set_blend(BLEND_MUL); break;
        case SP_BLEND_SCREEN: c.set_blend(BLEND_ADD); break; /* not exact (no screen blend yet) */
        default: c.set_blend(item_blend); break;
        }
        c.draw_triangles(img, v_, uv_, col_, idx_);
    }
    c.set_blend(item_blend);
}

} /* namespace spire */
