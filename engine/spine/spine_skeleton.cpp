/* Spine skeleton: setup pose, update order, world transforms and the
 * constraints (IK, transform, path, physics), as Spine 4.2 defines them. */
#include "spine.h"

#include <math.h>

namespace spire {

static const float PI = 3.14159265358979323846f;
static const float PI2 = PI * 2.0f;
static const float DEG_RAD = PI / 180.0f;
static const float RAD_DEG = 180.0f / PI;
static const float EPSILON = 0.00001f;

static float atan2d(float y, float x) { return atan2f(y, x) * RAD_DEG; }
static float wrap_pi(float r) {
    if (r > PI) r -= PI2;
    else if (r < -PI) r += PI2;
    return r;
}
static float signf(float v) { return v > 0 ? 1.0f : (v < 0 ? -1.0f : 0.0f); }

/* ---- setup ---- */

SpSkeleton::SpSkeleton(const SpSkeletonData* data)
    : x(0), y(0), scale_x(1), scale_y(1), y_down(true), time(0), data_(data), skin_(NULL) {
    bones.resize(data->bones.size());
    for (size_t i = 0; i < bones.size(); ++i) {
        bones[i].data = &data->bones[i];
        bones[i].parent = data->bones[i].parent;
        if (bones[i].parent >= 0) bones[bones[i].parent].children.push_back((int)i);
    }
    slots.resize(data->slots.size());
    for (size_t i = 0; i < slots.size(); ++i) slots[i].data = &data->slots[i];
    ik.resize(data->ik.size());
    for (size_t i = 0; i < ik.size(); ++i) ik[i].data = &data->ik[i];
    transform.resize(data->transform.size());
    for (size_t i = 0; i < transform.size(); ++i) transform[i].data = &data->transform[i];
    path.resize(data->path.size());
    for (size_t i = 0; i < path.size(); ++i) path[i].data = &data->path[i];
    physics.resize(data->physics.size());
    for (size_t i = 0; i < physics.size(); ++i) {
        SpPhysicsC& c = physics[i];
        c.data = &data->physics[i];
        c.ux = c.uy = c.cx = c.cy = c.tx = c.ty = 0;
    }
    reset_physics(-1);
    set_skin(NULL);
    set_to_setup_pose();
}

bool SpSkeleton::skin_has(const std::vector<int> SpSkin::*list, int index) const {
    if (!skin_) return false;
    const std::vector<int>& l = skin_->*list;
    for (size_t i = 0; i < l.size(); ++i)
        if (l[i] == index) return true;
    return false;
}

void SpSkeleton::set_skin(const SpSkin* skin) {
    skin_ = skin;
    update_cache();
}

void SpSkeleton::set_to_setup_pose() {
    set_bones_to_setup_pose();
    set_slots_to_setup_pose();
}

void SpSkeleton::set_bones_to_setup_pose() {
    for (size_t i = 0; i < bones.size(); ++i) {
        SpBone& b = bones[i];
        const SpBoneData& d = *b.data;
        b.x = d.x;
        b.y = d.y;
        b.rotation = d.rotation;
        b.scale_x = d.scale_x;
        b.scale_y = d.scale_y;
        b.shear_x = d.shear_x;
        b.shear_y = d.shear_y;
        b.inherit = d.inherit;
    }
    for (size_t i = 0; i < ik.size(); ++i) {
        SpIk& c = ik[i];
        c.bend = c.data->bend;
        c.compress = c.data->compress;
        c.stretch = c.data->stretch;
        c.mix = c.data->mix;
        c.softness = c.data->softness;
    }
    for (size_t i = 0; i < transform.size(); ++i) {
        SpTransform& c = transform[i];
        const SpTransformData& d = *c.data;
        c.mix_rotate = d.mix_rotate;
        c.mix_x = d.mix_x;
        c.mix_y = d.mix_y;
        c.mix_scale_x = d.mix_scale_x;
        c.mix_scale_y = d.mix_scale_y;
        c.mix_shear_y = d.mix_shear_y;
    }
    for (size_t i = 0; i < path.size(); ++i) {
        SpPath& c = path[i];
        const SpPathData& d = *c.data;
        c.position = d.position;
        c.spacing = d.spacing;
        c.mix_rotate = d.mix_rotate;
        c.mix_x = d.mix_x;
        c.mix_y = d.mix_y;
    }
    for (size_t i = 0; i < physics.size(); ++i) {
        SpPhysicsC& c = physics[i];
        const SpPhysicsData& d = *c.data;
        c.inertia = d.inertia;
        c.strength = d.strength;
        c.damping = d.damping;
        c.mass_inverse = d.mass_inverse;
        c.wind = d.wind;
        c.gravity = d.gravity;
        c.mix = d.mix;
    }
}

void SpSkeleton::set_slots_to_setup_pose() {
    draw_order.resize(slots.size());
    for (size_t i = 0; i < slots.size(); ++i) {
        SpSlot& s = slots[i];
        draw_order[i] = (int)i;
        s.color = s.data->color;
        s.attachment = s.data->attachment.empty() ? NULL : attachment((int)i, s.data->attachment);
        s.deform.clear();
        s.sequence_index = -1;
        s.attachment_state = 0;
    }
}

SpAttachment* SpSkeleton::attachment(int slot, const std::string& key) const {
    SpAttachment* a = skin_ ? skin_->find(slot, key) : NULL;
    if (!a) {
        const SpSkin* d = data_->default_skin();
        if (d) a = d->find(slot, key);
    }
    return a;
}

void SpSkeleton::reset_physics(int constraint) {
    for (size_t i = 0; i < physics.size(); ++i) {
        if (constraint >= 0 && (int)i != constraint) continue;
        SpPhysicsC& c = physics[i];
        c.remaining = 0;
        c.last_time = time;
        c.reset = true;
        c.x_offset = c.x_velocity = c.y_offset = c.y_velocity = 0;
        c.rotate_offset = c.rotate_velocity = c.scale_offset = c.scale_velocity = 0;
    }
}

/* ---- update order ----
 * Bones are posed parents first; each constraint runs once what it reads is
 * posed, and the bones it moves (with their children) are posed again
 * after it. */

void SpSkeleton::sort_bone(int i) {
    if (i < 0) return;
    SpBone& b = bones[i];
    if (b.sorted) return;
    if (b.parent >= 0) sort_bone(b.parent);
    b.sorted = true;
    CacheItem it = { 0, i };
    cache_.push_back(it);
}

void SpSkeleton::sort_reset(const std::vector<int>& list) {
    for (size_t i = 0; i < list.size(); ++i) {
        SpBone& b = bones[list[i]];
        if (!b.active) continue;
        if (b.sorted) sort_reset(b.children);
        b.sorted = false;
    }
}

void SpSkeleton::sort_path_attachment(const SpAttachment* a, int slot_bone) {
    if (!a || a->type != SP_ATT_PATH) return;
    if (!a->weighted()) {
        sort_bone(slot_bone);
        return;
    }
    for (size_t i = 0; i < a->bones.size();) {
        int n = a->bones[i++];
        for (int k = 0; k < n && i < a->bones.size(); ++k) sort_bone(a->bones[i++]);
    }
}

void SpSkeleton::sort_path_attachment(const SpSkin* skin, int slot, int slot_bone) {
    if (!skin) return;
    for (size_t i = 0; i < skin->attachments.size(); ++i)
        if (skin->attachments[i]->slot == slot) sort_path_attachment(skin->attachments[i], slot_bone);
}

struct ConstraintOrder {
    int order, kind, index;
};

void SpSkeleton::update_cache() {
    cache_.clear();
    for (size_t i = 0; i < bones.size(); ++i) {
        bones[i].sorted = bones[i].data->skin_required;
        bones[i].active = !bones[i].sorted;
    }
    if (skin_) {
        for (size_t i = 0; i < skin_->bones.size(); ++i) {
            int b = skin_->bones[i];
            while (b >= 0 && b < (int)bones.size()) {
                bones[b].sorted = false;
                bones[b].active = true;
                b = bones[b].parent;
            }
        }
    }
    /* Constraints in their order. */
    std::vector<ConstraintOrder> order;
    for (size_t i = 0; i < ik.size(); ++i) {
        ConstraintOrder it = { ik[i].data->order, 1, (int)i };
        order.push_back(it);
    }
    for (size_t i = 0; i < transform.size(); ++i) {
        ConstraintOrder it = { transform[i].data->order, 2, (int)i };
        order.push_back(it);
    }
    for (size_t i = 0; i < path.size(); ++i) {
        ConstraintOrder it = { path[i].data->order, 3, (int)i };
        order.push_back(it);
    }
    for (size_t i = 0; i < physics.size(); ++i) {
        ConstraintOrder it = { physics[i].data->order, 4, (int)i };
        order.push_back(it);
    }
    for (size_t i = 1; i < order.size(); ++i) /* insertion sort: stable, few items */
        for (size_t k = i; k > 0 && order[k].order < order[k - 1].order; --k) {
            ConstraintOrder t = order[k];
            order[k] = order[k - 1];
            order[k - 1] = t;
        }
    for (size_t n = 0; n < order.size(); ++n) {
        const ConstraintOrder& it = order[n];
        CacheItem ci = { it.kind, it.index };
        if (it.kind == 1) {
            SpIk& c = ik[it.index];
            const SpIkData& d = *c.data;
            c.active = d.target >= 0 && bones[d.target].active && !d.bones.empty() &&
                       (!d.skin_required || skin_has(&SpSkin::ik, it.index));
            if (!c.active) continue;
            sort_bone(d.target);
            int parent = d.bones[0];
            sort_bone(parent);
            if (d.bones.size() == 1) {
                cache_.push_back(ci);
                sort_reset(bones[parent].children);
            } else {
                int child = d.bones.back();
                sort_bone(child);
                cache_.push_back(ci);
                sort_reset(bones[parent].children);
                bones[child].sorted = true;
            }
        } else if (it.kind == 2) {
            SpTransform& c = transform[it.index];
            const SpTransformData& d = *c.data;
            c.active = d.target >= 0 && bones[d.target].active &&
                       (!d.skin_required || skin_has(&SpSkin::transform, it.index));
            if (!c.active) continue;
            sort_bone(d.target);
            for (size_t i = 0; i < d.bones.size(); ++i) {
                if (d.local) sort_bone(bones[d.bones[i]].parent);
                sort_bone(d.bones[i]);
            }
            cache_.push_back(ci);
            for (size_t i = 0; i < d.bones.size(); ++i) sort_reset(bones[d.bones[i]].children);
            for (size_t i = 0; i < d.bones.size(); ++i) bones[d.bones[i]].sorted = true;
        } else if (it.kind == 3) {
            SpPath& c = path[it.index];
            const SpPathData& d = *c.data;
            int slot_bone = d.target >= 0 && d.target < (int)slots.size() ? slots[d.target].data->bone : -1;
            c.active = slot_bone >= 0 && bones[slot_bone].active &&
                       (!d.skin_required || skin_has(&SpSkin::path, it.index));
            if (!c.active) continue;
            sort_path_attachment(skin_, d.target, slot_bone);
            const SpSkin* def = data_->default_skin();
            if (def && def != skin_) sort_path_attachment(def, d.target, slot_bone);
            sort_path_attachment(slots[d.target].attachment, slot_bone);
            for (size_t i = 0; i < d.bones.size(); ++i) sort_bone(d.bones[i]);
            cache_.push_back(ci);
            for (size_t i = 0; i < d.bones.size(); ++i) sort_reset(bones[d.bones[i]].children);
            for (size_t i = 0; i < d.bones.size(); ++i) bones[d.bones[i]].sorted = true;
        } else {
            SpPhysicsC& c = physics[it.index];
            const SpPhysicsData& d = *c.data;
            c.active = d.bone >= 0 && bones[d.bone].active &&
                       (!d.skin_required || skin_has(&SpSkin::physics, it.index));
            if (!c.active) continue;
            sort_bone(d.bone);
            cache_.push_back(ci);
            sort_reset(bones[d.bone].children);
            bones[d.bone].sorted = true;
        }
    }
    for (size_t i = 0; i < bones.size(); ++i) sort_bone((int)i);
}

/* ---- bones ---- */

void SpSkeleton::bone_update(SpBone& b, float bx, float by, float rotation, float scx, float scy, float shx,
                             float shy) {
    b.ax = bx;
    b.ay = by;
    b.arotation = rotation;
    b.ascale_x = scx;
    b.ascale_y = scy;
    b.ashear_x = shx;
    b.ashear_y = shy;
    float sx = this->sx(), sy = this->sy();
    float rx = (rotation + shx) * DEG_RAD, ry = (rotation + 90.0f + shy) * DEG_RAD;
    if (b.parent < 0) {
        b.a = cosf(rx) * scx * sx;
        b.b = cosf(ry) * scy * sx;
        b.c = sinf(rx) * scx * sy;
        b.d = sinf(ry) * scy * sy;
        b.world_x = bx * sx + x;
        b.world_y = by * sy + y;
        return;
    }
    const SpBone& p = bones[b.parent];
    float pa = p.a, pb = p.b, pc = p.c, pd = p.d;
    b.world_x = pa * bx + pb * by + p.world_x;
    b.world_y = pc * bx + pd * by + p.world_y;
    switch (b.inherit) {
    case SP_INHERIT_NORMAL: {
        float la = cosf(rx) * scx, lb = cosf(ry) * scy, lc = sinf(rx) * scx, ld = sinf(ry) * scy;
        b.a = pa * la + pb * lc;
        b.b = pa * lb + pb * ld;
        b.c = pc * la + pd * lc;
        b.d = pc * lb + pd * ld;
        return;
    }
    case SP_INHERIT_ONLY_TRANSLATION:
        b.a = cosf(rx) * scx;
        b.b = cosf(ry) * scy;
        b.c = sinf(rx) * scx;
        b.d = sinf(ry) * scy;
        break;
    case SP_INHERIT_NO_ROTATION_OR_REFLECTION: {
        /* The parent's scale and shear without its rotation (or a flip). */
        float isx = 1.0f / sx, isy = 1.0f / sy;
        pa *= isx;
        pc *= isy;
        float s = pa * pa + pc * pc, prx;
        if (s > 0.0001f) {
            s = fabsf(pa * pd * isy - pb * isx * pc) / s;
            pb = pc * s;
            pd = pa * s;
            prx = atan2d(pc, pa);
        } else {
            pa = 0;
            pc = 0;
            prx = 90.0f - atan2d(pd, pb);
        }
        float rx2 = (rotation + shx - prx) * DEG_RAD, ry2 = (rotation + shy - prx + 90.0f) * DEG_RAD;
        float la = cosf(rx2) * scx, lb = cosf(ry2) * scy, lc = sinf(rx2) * scx, ld = sinf(ry2) * scy;
        b.a = pa * la - pb * lc;
        b.b = pa * lb - pb * ld;
        b.c = pc * la + pd * lc;
        b.d = pc * lb + pd * ld;
        break;
    }
    default: { /* no scale (or reflection): the parent's rotation only */
        float r = rotation * DEG_RAD, cs = cosf(r), sn = sinf(r);
        float za = (pa * cs + pb * sn) / sx, zc = (pc * cs + pd * sn) / sy;
        float s = sqrtf(za * za + zc * zc);
        if (s > 0.00001f) s = 1.0f / s;
        za *= s;
        zc *= s;
        s = sqrtf(za * za + zc * zc);
        if (b.inherit == SP_INHERIT_NO_SCALE && ((pa * pd - pb * pc < 0) != ((sx < 0) != (sy < 0)))) s = -s;
        r = PI / 2.0f + atan2f(zc, za);
        float zb = cosf(r) * s, zd = sinf(r) * s;
        float shxr = shx * DEG_RAD, shyr = (90.0f + shy) * DEG_RAD;
        float la = cosf(shxr) * scx, lb = cosf(shyr) * scy, lc = sinf(shxr) * scx, ld = sinf(shyr) * scy;
        b.a = za * la + zb * lc;
        b.b = za * lb + zb * ld;
        b.c = zc * la + zd * lc;
        b.d = zc * lb + zd * ld;
        break;
    }
    }
    b.a *= sx;
    b.b *= sx;
    b.c *= sy;
    b.d *= sy;
}

/* The local (applied) transform that gives the bone's current world
 * transform under its parent: after a constraint moved it in world space. */
void SpSkeleton::bone_update_applied(SpBone& b) {
    if (b.parent < 0) {
        b.ax = b.world_x - x;
        b.ay = b.world_y - y;
        b.arotation = atan2d(b.c, b.a);
        b.ascale_x = sqrtf(b.a * b.a + b.c * b.c);
        b.ascale_y = sqrtf(b.b * b.b + b.d * b.d);
        b.ashear_x = 0;
        b.ashear_y = atan2d(b.a * b.b + b.c * b.d, b.a * b.d - b.b * b.c);
        return;
    }
    const SpBone& p = bones[b.parent];
    float pa = p.a, pb = p.b, pc = p.c, pd = p.d;
    float pid = 1.0f / (pa * pd - pb * pc);
    float ia = pd * pid, ib = pb * pid, ic = pc * pid, id = pa * pid;
    float dx = b.world_x - p.world_x, dy = b.world_y - p.world_y;
    b.ax = dx * ia - dy * ib;
    b.ay = dy * id - dx * ic;
    float ra, rb, rc, rd;
    if (b.inherit == SP_INHERIT_ONLY_TRANSLATION) {
        ra = b.a;
        rb = b.b;
        rc = b.c;
        rd = b.d;
    } else {
        float sx = this->sx(), sy = this->sy();
        if (b.inherit == SP_INHERIT_NO_ROTATION_OR_REFLECTION) {
            float s = fabsf(pa * pd - pb * pc) / (pa * pa + pc * pc);
            float sa = pa / sx, sc = pc / sy;
            pb = -sc * s * sx;
            pd = sa * s * sy;
            pid = 1.0f / (pa * pd - pb * pc);
            ia = pd * pid;
            ib = pb * pid;
        } else if (b.inherit == SP_INHERIT_NO_SCALE || b.inherit == SP_INHERIT_NO_SCALE_OR_REFLECTION) {
            float r = b.rotation * DEG_RAD, cs = cosf(r), sn = sinf(r);
            pa = (pa * cs + pb * sn) / sx;
            pc = (pc * cs + pd * sn) / sy;
            float s = sqrtf(pa * pa + pc * pc);
            if (s > 0.00001f) s = 1.0f / s;
            pa *= s;
            pc *= s;
            s = sqrtf(pa * pa + pc * pc);
            if (b.inherit == SP_INHERIT_NO_SCALE && (pid < 0) != ((sx < 0) != (sy < 0))) s = -s;
            r = PI / 2.0f + atan2f(pc, pa);
            pb = cosf(r) * s;
            pd = sinf(r) * s;
            pid = 1.0f / (pa * pd - pb * pc);
            ia = pd * pid;
            ib = pb * pid;
            ic = pc * pid;
            id = pa * pid;
        }
        ra = ia * b.a - ib * b.c;
        rb = ia * b.b - ib * b.d;
        rc = id * b.c - ic * b.a;
        rd = id * b.d - ic * b.b;
    }
    b.ashear_x = 0;
    b.ascale_x = sqrtf(ra * ra + rc * rc);
    if (b.ascale_x > 0.0001f) {
        float det = ra * rd - rb * rc;
        b.ascale_y = det / b.ascale_x;
        b.ashear_y = -atan2d(ra * rb + rc * rd, det);
        b.arotation = atan2d(rc, ra);
    } else {
        b.ascale_x = 0;
        b.ascale_y = sqrtf(rb * rb + rd * rd);
        b.ashear_y = 0;
        b.arotation = 90.0f - atan2d(rd, rb);
    }
}

void SpSkeleton::update_world_transform(int phys) {
    for (size_t i = 0; i < bones.size(); ++i) {
        SpBone& b = bones[i];
        b.ax = b.x;
        b.ay = b.y;
        b.arotation = b.rotation;
        b.ascale_x = b.scale_x;
        b.ascale_y = b.scale_y;
        b.ashear_x = b.shear_x;
        b.ashear_y = b.shear_y;
    }
    for (size_t i = 0; i < cache_.size(); ++i) {
        const CacheItem& it = cache_[i];
        switch (it.kind) {
        case 0: {
            SpBone& b = bones[it.index];
            bone_update(b, b.ax, b.ay, b.arotation, b.ascale_x, b.ascale_y, b.ashear_x, b.ashear_y);
            break;
        }
        case 1: apply_ik(ik[it.index]); break;
        case 2: apply_transform(transform[it.index]); break;
        case 3: apply_path(path[it.index]); break;
        default: apply_physics(physics[it.index], phys); break;
        }
    }
}

/* ---- IK ---- */

static void ik_one(SpSkeleton& s, SpBone& bone, float target_x, float target_y, bool compress, bool stretch,
                   bool uniform, float alpha) {
    if (bone.parent < 0) return;
    const SpBone& p = s.bones[bone.parent];
    float pa = p.a, pb = p.b, pc = p.c, pd = p.d;
    float rotation_ik = -bone.ashear_x - bone.arotation, tx, ty;
    switch (bone.inherit) {
    case SP_INHERIT_ONLY_TRANSLATION:
        tx = (target_x - bone.world_x) * signf(s.sx());
        ty = (target_y - bone.world_y) * signf(s.sy());
        break;
    case SP_INHERIT_NO_ROTATION_OR_REFLECTION: {
        float sc = fabsf(pa * pd - pb * pc) / maxf(0.0001f, pa * pa + pc * pc);
        float sa = pa / s.sx(), scc = pc / s.sy();
        pb = -scc * sc * s.sx();
        pd = sa * sc * s.sy();
        rotation_ik += atan2d(scc, sa);
    } /* fall through */
    default: {
        float x = target_x - p.world_x, y = target_y - p.world_y;
        float d = pa * pd - pb * pc;
        if (fabsf(d) <= 0.0001f) {
            tx = 0;
            ty = 0;
        } else {
            tx = (x * pd - y * pb) / d - bone.ax;
            ty = (y * pa - x * pc) / d - bone.ay;
        }
    }
    }
    rotation_ik += atan2d(ty, tx);
    if (bone.ascale_x < 0) rotation_ik += 180.0f;
    if (rotation_ik > 180.0f) rotation_ik -= 360.0f;
    else if (rotation_ik < -180.0f) rotation_ik += 360.0f;
    float sx = bone.ascale_x, sy = bone.ascale_y;
    if (compress || stretch) {
        if (bone.inherit == SP_INHERIT_NO_SCALE || bone.inherit == SP_INHERIT_NO_SCALE_OR_REFLECTION) {
            tx = target_x - bone.world_x;
            ty = target_y - bone.world_y;
        }
        float b = bone.data->length * sx;
        if (b > 0.0001f) {
            float dd = tx * tx + ty * ty;
            if ((compress && dd < b * b) || (stretch && dd > b * b)) {
                float sc = (sqrtf(dd) / b - 1.0f) * alpha + 1.0f;
                sx *= sc;
                if (uniform) sy *= sc;
            }
        }
    }
    s.bone_update(bone, bone.ax, bone.ay, bone.arotation + rotation_ik * alpha, sx, sy, bone.ashear_x,
                  bone.ashear_y);
}

static void ik_two(SpSkeleton& s, SpBone& parent, SpBone& child, float target_x, float target_y, int bend_dir,
                   bool stretch, bool uniform, float softness, float alpha) {
    if (parent.inherit != SP_INHERIT_NORMAL || child.inherit != SP_INHERIT_NORMAL || parent.parent < 0) return;
    float px = parent.ax, py = parent.ay, psx = parent.ascale_x, psy = parent.ascale_y, sx = psx, sy = psy;
    float csx = child.ascale_x;
    int os1, os2, s2;
    if (psx < 0) {
        psx = -psx;
        os1 = 180;
        s2 = -1;
    } else {
        os1 = 0;
        s2 = 1;
    }
    if (psy < 0) {
        psy = -psy;
        s2 = -s2;
    }
    if (csx < 0) {
        csx = -csx;
        os2 = 180;
    } else {
        os2 = 0;
    }
    float cx = child.ax, cy, cwx, cwy, a = parent.a, b = parent.b, c = parent.c, d = parent.d;
    bool u = fabsf(psx - psy) <= 0.0001f;
    if (!u || stretch) {
        cy = 0;
        cwx = a * cx + parent.world_x;
        cwy = c * cx + parent.world_y;
    } else {
        cy = child.ay;
        cwx = a * cx + b * cy + parent.world_x;
        cwy = c * cx + d * cy + parent.world_y;
    }
    const SpBone& pp = s.bones[parent.parent];
    a = pp.a;
    b = pp.b;
    c = pp.c;
    d = pp.d;
    float id = a * d - b * c, x = cwx - pp.world_x, y = cwy - pp.world_y;
    id = fabsf(id) <= 0.0001f ? 0 : 1.0f / id;
    float dx = (x * d - y * b) * id - px, dy = (y * a - x * c) * id - py;
    float l1 = sqrtf(dx * dx + dy * dy), l2 = child.data->length * csx, a1, a2;
    if (l1 < 0.0001f) {
        ik_one(s, parent, target_x, target_y, false, stretch, false, alpha);
        s.bone_update(child, cx, cy, 0, child.ascale_x, child.ascale_y, child.ashear_x, child.ashear_y);
        return;
    }
    x = target_x - pp.world_x;
    y = target_y - pp.world_y;
    float tx = (x * d - y * b) * id - px, ty = (y * a - x * c) * id - py;
    float dd = tx * tx + ty * ty;
    if (softness != 0) {
        softness *= psx * (csx + 1) * 0.5f;
        float td = sqrtf(dd), sd = td - l1 - l2 * psx + softness;
        if (sd > 0) {
            float p = minf(1.0f, sd / (softness * 2)) - 1;
            p = (sd - softness * (1 - p * p)) / td;
            tx -= p * tx;
            ty -= p * ty;
            dd = tx * tx + ty * ty;
        }
    }
    if (u) {
        l2 *= psx;
        float cs = (dd - l1 * l1 - l2 * l2) / (2 * l1 * l2);
        if (cs < -1) {
            cs = -1;
            a2 = PI * bend_dir;
        } else if (cs > 1) {
            cs = 1;
            a2 = 0;
            if (stretch) {
                a = (sqrtf(dd) / (l1 + l2) - 1) * alpha + 1;
                sx *= a;
                if (uniform) sy *= a;
            }
        } else {
            a2 = acosf(cs) * bend_dir;
        }
        a = l1 + l2 * cs;
        b = l2 * sinf(a2);
        a1 = atan2f(ty * a - tx * b, tx * a + ty * b);
    } else {
        a = psx * l2;
        b = psy * l2;
        float aa = a * a, bb = b * b, ta = atan2f(ty, tx);
        c = bb * l1 * l1 + aa * dd - aa * bb;
        float c1 = -2 * bb * l1, c2 = bb - aa;
        d = c1 * c1 - 4 * c2 * c;
        bool done = false;
        if (d >= 0) {
            float q = sqrtf(d);
            if (c1 < 0) q = -q;
            q = -(c1 + q) * 0.5f;
            float r0 = q / c2, r1 = c / q;
            float r = fabsf(r0) < fabsf(r1) ? r0 : r1;
            r0 = dd - r * r;
            if (r0 >= 0) {
                y = sqrtf(r0) * bend_dir;
                a1 = ta - atan2f(y, r);
                a2 = atan2f(y / psy, (r - l1) / psx);
                done = true;
            }
        }
        if (!done) {
            float min_angle = PI, min_x = l1 - a, min_dist = min_x * min_x, min_y = 0;
            float max_angle = 0, max_x = l1 + a, max_dist = max_x * max_x, max_y = 0;
            c = -a * l1 / (aa - bb);
            if (c >= -1 && c <= 1) {
                c = acosf(c);
                x = a * cosf(c) + l1;
                y = b * sinf(c);
                d = x * x + y * y;
                if (d < min_dist) {
                    min_angle = c;
                    min_dist = d;
                    min_x = x;
                    min_y = y;
                }
                if (d > max_dist) {
                    max_angle = c;
                    max_dist = d;
                    max_x = x;
                    max_y = y;
                }
            }
            if (dd <= (min_dist + max_dist) * 0.5f) {
                a1 = ta - atan2f(min_y * bend_dir, min_x);
                a2 = min_angle * bend_dir;
            } else {
                a1 = ta - atan2f(max_y * bend_dir, max_x);
                a2 = max_angle * bend_dir;
            }
        }
    }
    float os = atan2f(cy, cx) * s2;
    float rotation = parent.arotation;
    a1 = (a1 - os) * RAD_DEG + os1 - rotation;
    if (a1 > 180) a1 -= 360;
    else if (a1 < -180) a1 += 360;
    s.bone_update(parent, px, py, rotation + a1 * alpha, sx, sy, 0, 0);
    rotation = child.arotation;
    a2 = ((a2 + os) * RAD_DEG - child.ashear_x) * s2 + os2 - rotation;
    if (a2 > 180) a2 -= 360;
    else if (a2 < -180) a2 += 360;
    s.bone_update(child, cx, cy, rotation + a2 * alpha, child.ascale_x, child.ascale_y, child.ashear_x,
                  child.ashear_y);
}

void SpSkeleton::apply_ik(SpIk& c) {
    if (c.mix == 0) return;
    const SpIkData& d = *c.data;
    const SpBone& target = bones[d.target];
    if (d.bones.size() == 1)
        ik_one(*this, bones[d.bones[0]], target.world_x, target.world_y, c.compress, c.stretch, d.uniform, c.mix);
    else if (d.bones.size() >= 2)
        ik_two(*this, bones[d.bones[0]], bones[d.bones[1]], target.world_x, target.world_y, c.bend, c.stretch,
               d.uniform, c.softness, c.mix);
}

/* ---- transform constraints ---- */

void SpSkeleton::apply_transform(SpTransform& c) {
    if (c.mix_rotate == 0 && c.mix_x == 0 && c.mix_y == 0 && c.mix_scale_x == 0 && c.mix_scale_y == 0 &&
        c.mix_shear_y == 0)
        return;
    const SpTransformData& d = *c.data;
    const SpBone& t = bones[d.target];
    if (d.local) {
        for (size_t i = 0; i < d.bones.size(); ++i) {
            SpBone& b = bones[d.bones[i]];
            float rotation, bx, by, scx, scy, shy;
            if (d.relative) {
                rotation = b.arotation + (t.arotation + d.offset_rotation) * c.mix_rotate;
                bx = b.ax + (t.ax + d.offset_x) * c.mix_x;
                by = b.ay + (t.ay + d.offset_y) * c.mix_y;
                scx = b.ascale_x * (((t.ascale_x - 1 + d.offset_scale_x) * c.mix_scale_x) + 1);
                scy = b.ascale_y * (((t.ascale_y - 1 + d.offset_scale_y) * c.mix_scale_y) + 1);
                shy = b.ashear_y + (t.ashear_y + d.offset_shear_y) * c.mix_shear_y;
            } else {
                rotation = b.arotation;
                if (c.mix_rotate != 0) {
                    float r = t.arotation - rotation + d.offset_rotation;
                    r -= floorf(r / 360.0f + 0.5f) * 360.0f;
                    rotation += r * c.mix_rotate;
                }
                bx = b.ax + (t.ax - b.ax + d.offset_x) * c.mix_x;
                by = b.ay + (t.ay - b.ay + d.offset_y) * c.mix_y;
                scx = b.ascale_x;
                scy = b.ascale_y;
                if (c.mix_scale_x != 0 && scx != 0)
                    scx = (scx + (t.ascale_x - scx + d.offset_scale_x) * c.mix_scale_x) / scx;
                if (c.mix_scale_y != 0 && scy != 0)
                    scy = (scy + (t.ascale_y - scy + d.offset_scale_y) * c.mix_scale_y) / scy;
                shy = b.ashear_y;
                if (c.mix_shear_y != 0) {
                    float r = t.ashear_y - shy + d.offset_shear_y;
                    r -= floorf(r / 360.0f + 0.5f) * 360.0f;
                    shy += r * c.mix_shear_y;
                }
            }
            bone_update(b, bx, by, rotation, scx, scy, b.ashear_x, shy);
        }
        return;
    }
    float ta = t.a, tb = t.b, tc = t.c, td = t.d;
    float reflect = ta * td - tb * tc > 0 ? DEG_RAD : -DEG_RAD;
    float offset_rotation = d.offset_rotation * reflect, offset_shear_y = d.offset_shear_y * reflect;
    bool translate = c.mix_x != 0 || c.mix_y != 0;
    for (size_t i = 0; i < d.bones.size(); ++i) {
        SpBone& b = bones[d.bones[i]];
        if (c.mix_rotate != 0) {
            float a = b.a, bb = b.b, cc = b.c, dd = b.d;
            float r = d.relative ? atan2f(tc, ta) + offset_rotation
                                 : atan2f(tc, ta) - atan2f(cc, a) + offset_rotation;
            r = wrap_pi(r) * c.mix_rotate;
            float cs = cosf(r), sn = sinf(r);
            b.a = cs * a - sn * cc;
            b.b = cs * bb - sn * dd;
            b.c = sn * a + cs * cc;
            b.d = sn * bb + cs * dd;
        }
        if (translate) {
            float wx = ta * d.offset_x + tb * d.offset_y + t.world_x;
            float wy = tc * d.offset_x + td * d.offset_y + t.world_y;
            if (d.relative) {
                b.world_x += wx * c.mix_x;
                b.world_y += wy * c.mix_y;
            } else {
                b.world_x += (wx - b.world_x) * c.mix_x;
                b.world_y += (wy - b.world_y) * c.mix_y;
            }
        }
        if (c.mix_scale_x != 0) {
            float s;
            if (d.relative) {
                s = (sqrtf(ta * ta + tc * tc) - 1 + d.offset_scale_x) * c.mix_scale_x + 1;
            } else {
                s = sqrtf(b.a * b.a + b.c * b.c);
                if (s != 0) s = (s + (sqrtf(ta * ta + tc * tc) - s + d.offset_scale_x) * c.mix_scale_x) / s;
            }
            b.a *= s;
            b.c *= s;
        }
        if (c.mix_scale_y != 0) {
            float s;
            if (d.relative) {
                s = (sqrtf(tb * tb + td * td) - 1 + d.offset_scale_y) * c.mix_scale_y + 1;
            } else {
                s = sqrtf(b.b * b.b + b.d * b.d);
                if (s != 0) s = (s + (sqrtf(tb * tb + td * td) - s + d.offset_scale_y) * c.mix_scale_y) / s;
            }
            b.b *= s;
            b.d *= s;
        }
        if (c.mix_shear_y > 0) {
            float bb = b.b, dd = b.d, r;
            if (d.relative) {
                r = wrap_pi(atan2f(td, tb) - atan2f(tc, ta));
                r = atan2f(dd, bb) + (r - PI / 2 + offset_shear_y) * c.mix_shear_y;
            } else {
                float by = atan2f(dd, bb);
                r = wrap_pi(atan2f(td, tb) - atan2f(tc, ta) - (by - atan2f(b.c, b.a)));
                r = by + (r + offset_shear_y) * c.mix_shear_y;
            }
            float s = sqrtf(bb * bb + dd * dd);
            b.b = cosf(r) * s;
            b.d = sinf(r) * s;
        }
        bone_update_applied(b);
    }
}

/* ---- vertices ---- */

void SpSkeleton::world_vertices(const SpSlot& slot, const SpAttachment& a, int start, int count, float* out) const {
    const SpBone& bone = bones[slot.data->bone];
    if (a.type == SP_ATT_REGION) {
        const std::vector<float>& o = a.frame_offsets[a.frame(slot.sequence_index)];
        for (int i = 0; i < 8 && i < (int)o.size(); i += 2) {
            out[i] = bone.a * o[i] + bone.b * o[i + 1] + bone.world_x;
            out[i + 1] = bone.c * o[i] + bone.d * o[i + 1] + bone.world_y;
        }
        return;
    }
    const std::vector<float>& def = slot.deform;
    if (!a.weighted()) {
        const std::vector<float>& v = def.size() == a.vertices.size() ? def : a.vertices;
        for (int i = start, o = 0; i + 1 < start + count && i + 1 < (int)v.size(); i += 2, o += 2) {
            out[o] = bone.a * v[i] + bone.b * v[i + 1] + bone.world_x;
            out[o + 1] = bone.c * v[i] + bone.d * v[i + 1] + bone.world_y;
        }
        return;
    }
    bool has_def = (int)def.size() == a.deform_length();
    /* Skip the vertices before start. */
    size_t bi = 0, vi = 0, di = 0;
    for (int i = 0; i < start && bi < a.bones.size(); i += 2) {
        int n = a.bones[bi];
        bi += (size_t)n + 1;
        vi += (size_t)n * 3;
        di += (size_t)n * 2;
    }
    for (int o = 0; o < count && bi < a.bones.size(); o += 2) {
        int n = a.bones[bi++];
        float wx = 0, wy = 0;
        for (int k = 0; k < n; ++k, vi += 3, di += 2) {
            const SpBone& b = bones[a.bones[bi++]];
            float vx = a.vertices[vi], vy = a.vertices[vi + 1], w = a.vertices[vi + 2];
            if (has_def) {
                vx += def[di];
                vy += def[di + 1];
            }
            wx += (b.a * vx + b.b * vy + b.world_x) * w;
            wy += (b.c * vx + b.d * vy + b.world_y) * w;
        }
        out[o] = wx;
        out[o + 1] = wy;
    }
}

void SpSkeleton::world_vertices(const SpSlot& slot, std::vector<float>& out) const {
    const SpAttachment* a = slot.attachment;
    out.clear();
    if (!a) return;
    int n = a->type == SP_ATT_REGION ? 8 : a->vertex_count * 2;
    out.resize(n);
    if (n) world_vertices(slot, *a, 0, n, &out[0]);
}

/* ---- path constraints ---- */

static void add_before(float p, const float* temp, int i, float* out, int o) {
    float x1 = temp[i], y1 = temp[i + 1], dx = temp[i + 2] - x1, dy = temp[i + 3] - y1, r = atan2f(dy, dx);
    out[o] = x1 + p * cosf(r);
    out[o + 1] = y1 + p * sinf(r);
    out[o + 2] = r;
}

static void add_after(float p, const float* temp, int i, float* out, int o) {
    float x1 = temp[i + 2], y1 = temp[i + 3], dx = x1 - temp[i], dy = y1 - temp[i + 1], r = atan2f(dy, dx);
    out[o] = x1 + p * cosf(r);
    out[o + 1] = y1 + p * sinf(r);
    out[o + 2] = r;
}

static void add_curve(float p, float x1, float y1, float cx1, float cy1, float cx2, float cy2, float x2, float y2,
                      float* out, int o, bool tangents) {
    if (p < EPSILON || p != p) {
        out[o] = x1;
        out[o + 1] = y1;
        out[o + 2] = atan2f(cy1 - y1, cx1 - x1);
        return;
    }
    float tt = p * p, ttt = tt * p, u = 1 - p, uu = u * u, uuu = uu * u, ut = u * p, ut3 = ut * 3, uut3 = u * ut3,
          utt3 = ut3 * p;
    float x = x1 * uuu + cx1 * uut3 + cx2 * utt3 + x2 * ttt, y = y1 * uuu + cy1 * uut3 + cy2 * utt3 + y2 * ttt;
    out[o] = x;
    out[o + 1] = y;
    if (tangents) {
        if (p < 0.001f) out[o + 2] = atan2f(cy1 - y1, cx1 - x1);
        else out[o + 2] = atan2f(y - (y1 * uu + cy1 * ut * 2 + cy2 * tt), x - (x1 * uu + cx1 * ut * 2 + cx2 * tt));
    }
}

/* Positions (x, y, angle) along the path for each space. */
static void path_positions(const SpSkeleton& s, const SpSlot& target, const SpAttachment& att, const SpPath& c,
                           const std::vector<float>& spaces, bool tangents, std::vector<float>& out,
                           std::vector<float>& world, std::vector<float>& curves) {
    const SpPathData& d = *c.data;
    int spaces_count = (int)spaces.size();
    float position = c.position;
    out.assign((size_t)spaces_count * 3 + 2, 0.0f);
    bool closed = att.closed;
    int vlen = att.vertex_count * 2, curve_count = vlen / 6, prev_curve = -1;
    const int BEFORE = -2, AFTER = -3;
    float* o = &out[0];
    if (!att.constant_speed) {
        const std::vector<float>& lengths = att.lengths;
        curve_count -= closed ? 1 : 2;
        if (curve_count < 0 || curve_count >= (int)lengths.size()) return;
        float path_length = lengths[curve_count];
        if (d.position_mode == SP_POSITION_PERCENT) position *= path_length;
        float multiplier = d.spacing_mode == SP_SPACING_PERCENT ? path_length
                         : d.spacing_mode == SP_SPACING_PROPORTIONAL ? path_length / spaces_count : 1.0f;
        float w[8];
        for (int i = 0, oi = 0, curve = 0; i < spaces_count; ++i, oi += 3) {
            float space = spaces[i] * multiplier;
            position += space;
            float p = position;
            if (closed) {
                p = fmodf(p, path_length);
                if (p < 0) p += path_length;
                curve = 0;
            } else if (p < 0) {
                if (prev_curve != BEFORE) {
                    prev_curve = BEFORE;
                    s.world_vertices(target, att, 2, 4, w);
                }
                add_before(p, w, 0, o, oi);
                continue;
            } else if (p > path_length) {
                if (prev_curve != AFTER) {
                    prev_curve = AFTER;
                    s.world_vertices(target, att, vlen - 6, 4, w);
                }
                add_after(p - path_length, w, 0, o, oi);
                continue;
            }
            for (;; ++curve) {
                if (curve >= (int)lengths.size()) {
                    curve = (int)lengths.size() - 1;
                    break;
                }
                float length = lengths[curve];
                if (p > length) continue;
                if (curve == 0) p /= length;
                else {
                    float prev = lengths[curve - 1];
                    p = (p - prev) / (length - prev);
                }
                break;
            }
            if (curve != prev_curve) {
                prev_curve = curve;
                if (closed && curve == curve_count) {
                    s.world_vertices(target, att, vlen - 4, 4, w);
                    s.world_vertices(target, att, 0, 4, w + 4);
                } else {
                    s.world_vertices(target, att, curve * 6 + 2, 8, w);
                }
            }
            add_curve(p, w[0], w[1], w[2], w[3], w[4], w[5], w[6], w[7], o, oi,
                      tangents || (i > 0 && space < EPSILON));
        }
        return;
    }
    /* Constant speed: measure the curves in world space. */
    if (closed) {
        vlen += 2;
        world.assign((size_t)vlen, 0.0f);
        s.world_vertices(target, att, 2, vlen - 4, &world[0]);
        s.world_vertices(target, att, 0, 2, &world[vlen - 4]);
        world[vlen - 2] = world[0];
        world[vlen - 1] = world[1];
    } else {
        curve_count--;
        vlen -= 4;
        if (vlen <= 0) return;
        world.assign((size_t)vlen, 0.0f);
        s.world_vertices(target, att, 2, vlen, &world[0]);
    }
    if (curve_count <= 0) return;
    curves.assign((size_t)curve_count, 0.0f);
    float path_length = 0;
    float x1 = world[0], y1 = world[1], cx1 = 0, cy1 = 0, cx2 = 0, cy2 = 0, x2 = 0, y2 = 0;
    float tmpx, tmpy, dddfx, dddfy, ddfx, ddfy, dfx, dfy;
    for (int i = 0, wi = 2; i < curve_count; ++i, wi += 6) {
        cx1 = world[wi];
        cy1 = world[wi + 1];
        cx2 = world[wi + 2];
        cy2 = world[wi + 3];
        x2 = world[wi + 4];
        y2 = world[wi + 5];
        tmpx = (x1 - cx1 * 2 + cx2) * 0.1875f;
        tmpy = (y1 - cy1 * 2 + cy2) * 0.1875f;
        dddfx = ((cx1 - cx2) * 3 - x1 + x2) * 0.09375f;
        dddfy = ((cy1 - cy2) * 3 - y1 + y2) * 0.09375f;
        ddfx = tmpx * 2 + dddfx;
        ddfy = tmpy * 2 + dddfy;
        dfx = (cx1 - x1) * 0.75f + tmpx + dddfx * 0.16666667f;
        dfy = (cy1 - y1) * 0.75f + tmpy + dddfy * 0.16666667f;
        path_length += sqrtf(dfx * dfx + dfy * dfy);
        dfx += ddfx;
        dfy += ddfy;
        ddfx += dddfx;
        ddfy += dddfy;
        path_length += sqrtf(dfx * dfx + dfy * dfy);
        dfx += ddfx;
        dfy += ddfy;
        path_length += sqrtf(dfx * dfx + dfy * dfy);
        dfx += ddfx + dddfx;
        dfy += ddfy + dddfy;
        path_length += sqrtf(dfx * dfx + dfy * dfy);
        curves[i] = path_length;
        x1 = x2;
        y1 = y2;
    }
    if (d.position_mode == SP_POSITION_PERCENT) position *= path_length;
    float multiplier = d.spacing_mode == SP_SPACING_PERCENT ? path_length
                     : d.spacing_mode == SP_SPACING_PROPORTIONAL ? path_length / spaces_count : 1.0f;
    float segments[10];
    float curve_length = 0;
    for (int i = 0, oi = 0, curve = 0, segment = 0; i < spaces_count; ++i, oi += 3) {
        float space = spaces[i] * multiplier;
        position += space;
        float p = position;
        if (closed) {
            p = fmodf(p, path_length);
            if (p < 0) p += path_length;
            curve = 0;
            segment = 0;
        } else if (p < 0) {
            add_before(p, &world[0], 0, o, oi);
            continue;
        } else if (p > path_length) {
            add_after(p - path_length, &world[0], vlen - 4, o, oi);
            continue;
        }
        for (;; ++curve) {
            if (curve >= curve_count) {
                curve = curve_count - 1;
                break;
            }
            float length = curves[curve];
            if (p > length) continue;
            if (curve == 0) p /= length;
            else {
                float prev = curves[curve - 1];
                p = (p - prev) / (length - prev);
            }
            break;
        }
        if (curve != prev_curve) {
            prev_curve = curve;
            int ii = curve * 6;
            x1 = world[ii];
            y1 = world[ii + 1];
            cx1 = world[ii + 2];
            cy1 = world[ii + 3];
            cx2 = world[ii + 4];
            cy2 = world[ii + 5];
            x2 = world[ii + 6];
            y2 = world[ii + 7];
            tmpx = (x1 - cx1 * 2 + cx2) * 0.03f;
            tmpy = (y1 - cy1 * 2 + cy2) * 0.03f;
            dddfx = ((cx1 - cx2) * 3 - x1 + x2) * 0.006f;
            dddfy = ((cy1 - cy2) * 3 - y1 + y2) * 0.006f;
            ddfx = tmpx * 2 + dddfx;
            ddfy = tmpy * 2 + dddfy;
            dfx = (cx1 - x1) * 0.3f + tmpx + dddfx * 0.16666667f;
            dfy = (cy1 - y1) * 0.3f + tmpy + dddfy * 0.16666667f;
            curve_length = sqrtf(dfx * dfx + dfy * dfy);
            segments[0] = curve_length;
            for (ii = 1; ii < 8; ++ii) {
                dfx += ddfx;
                dfy += ddfy;
                ddfx += dddfx;
                ddfy += dddfy;
                curve_length += sqrtf(dfx * dfx + dfy * dfy);
                segments[ii] = curve_length;
            }
            dfx += ddfx;
            dfy += ddfy;
            curve_length += sqrtf(dfx * dfx + dfy * dfy);
            segments[8] = curve_length;
            dfx += ddfx + dddfx;
            dfy += ddfy + dddfy;
            curve_length += sqrtf(dfx * dfx + dfy * dfy);
            segments[9] = curve_length;
            segment = 0;
        }
        p *= curve_length;
        for (;; ++segment) {
            if (segment >= 10) {
                segment = 9;
                break;
            }
            float length = segments[segment];
            if (p > length) continue;
            if (segment == 0) p /= length;
            else {
                float prev = segments[segment - 1];
                p = segment + (p - prev) / (length - prev);
            }
            break;
        }
        add_curve(p * 0.1f, x1, y1, cx1, cy1, cx2, cy2, x2, y2, o, oi, tangents || (i > 0 && space < EPSILON));
    }
}

void SpSkeleton::apply_path(SpPath& c) {
    const SpPathData& d = *c.data;
    const SpSlot& target = slots[d.target];
    const SpAttachment* att = target.attachment;
    if (!att || att->type != SP_ATT_PATH) return;
    if (c.mix_rotate == 0 && c.mix_x == 0 && c.mix_y == 0) return;
    bool tangents = d.rotate_mode == SP_ROTATE_TANGENT, scale = d.rotate_mode == SP_ROTATE_CHAIN_SCALE;
    int bone_count = (int)d.bones.size(), spaces_count = tangents ? bone_count : bone_count + 1;
    std::vector<float>& spaces = scratch_[0];
    std::vector<float> lengths;
    spaces.assign((size_t)spaces_count, 0.0f);
    if (scale) lengths.assign((size_t)bone_count, 0.0f);
    float spacing = c.spacing;
    if (d.spacing_mode == SP_SPACING_PERCENT) {
        if (scale) {
            for (int i = 0; i < bone_count; ++i) {
                const SpBone& b = bones[d.bones[i]];
                float l = b.data->length, x = l * b.a, y = l * b.c;
                lengths[i] = sqrtf(x * x + y * y);
            }
        }
        for (int i = 1; i < spaces_count; ++i) spaces[i] = spacing;
    } else if (d.spacing_mode == SP_SPACING_PROPORTIONAL) {
        float sum = 0;
        for (int i = 0, n = spaces_count - 1; i < n;) {
            const SpBone& b = bones[d.bones[i]];
            float l = b.data->length;
            if (l < EPSILON) {
                if (scale) lengths[i] = 0;
                spaces[++i] = spacing;
            } else {
                float x = l * b.a, y = l * b.c, length = sqrtf(x * x + y * y);
                if (scale) lengths[i] = length;
                spaces[++i] = length;
                sum += length;
            }
        }
        if (sum > 0) {
            sum = spaces_count / sum * spacing;
            for (int i = 1; i < spaces_count; ++i) spaces[i] *= sum;
        }
    } else {
        bool length_spacing = d.spacing_mode == SP_SPACING_LENGTH;
        for (int i = 0, n = spaces_count - 1; i < n;) {
            const SpBone& b = bones[d.bones[i]];
            float l = b.data->length;
            if (l < EPSILON) {
                if (scale) lengths[i] = 0;
                spaces[++i] = spacing;
            } else {
                float x = l * b.a, y = l * b.c, length = sqrtf(x * x + y * y);
                if (scale) lengths[i] = length;
                spaces[++i] = (length_spacing ? l + spacing : spacing) * length / l;
            }
        }
    }
    std::vector<float>& pos = scratch_[1];
    std::vector<float> world, curves;
    path_positions(*this, target, *att, c, spaces, tangents, pos, world, curves);
    if (pos.size() < 3) return;
    float bone_x = pos[0], bone_y = pos[1], offset_rotation = d.offset_rotation;
    bool tip;
    if (offset_rotation == 0) {
        tip = d.rotate_mode == SP_ROTATE_CHAIN;
    } else {
        tip = false;
        const SpBone& p = bones[target.data->bone];
        offset_rotation *= p.a * p.d - p.b * p.c > 0 ? DEG_RAD : -DEG_RAD;
    }
    for (int i = 0, p = 3; i < bone_count; ++i, p += 3) {
        SpBone& b = bones[d.bones[i]];
        b.world_x += (bone_x - b.world_x) * c.mix_x;
        b.world_y += (bone_y - b.world_y) * c.mix_y;
        float x = pos[p], y = pos[p + 1], dx = x - bone_x, dy = y - bone_y;
        if (scale) {
            float length = lengths[i];
            if (length >= EPSILON) {
                float s = (sqrtf(dx * dx + dy * dy) / length - 1) * c.mix_rotate + 1;
                b.a *= s;
                b.c *= s;
            }
        }
        bone_x = x;
        bone_y = y;
        if (c.mix_rotate > 0) {
            float a = b.a, bb = b.b, cc = b.c, dd = b.d, r;
            if (tangents) r = pos[p - 1];
            else if (spaces[i + 1] < EPSILON) r = pos[p + 2];
            else r = atan2f(dy, dx);
            r -= atan2f(cc, a);
            if (tip) {
                float cs = cosf(r), sn = sinf(r), length = b.data->length;
                bone_x += (length * (cs * a - sn * cc) - dx) * c.mix_rotate;
                bone_y += (length * (sn * a + cs * cc) - dy) * c.mix_rotate;
            } else {
                r += offset_rotation;
            }
            r = wrap_pi(r) * c.mix_rotate;
            float cs = cosf(r), sn = sinf(r);
            b.a = cs * a - sn * cc;
            b.b = cs * bb - sn * dd;
            b.c = sn * a + cs * cc;
            b.d = sn * bb + cs * dd;
        }
        bone_update_applied(b);
    }
}

/* ---- physics constraints ---- */

void SpSkeleton::apply_physics(SpPhysicsC& c, int phys) {
    float mix = c.mix;
    if (mix == 0) return;
    const SpPhysicsData& d = *c.data;
    bool px = d.x > 0, py = d.y > 0, rotate_or_shear_x = d.rotate > 0 || d.shear_x > 0, scale_x = d.scale_x > 0;
    SpBone& bone = bones[d.bone];
    float l = bone.data->length;
    switch (phys) {
    case SP_PHYSICS_NONE: return;
    case SP_PHYSICS_RESET:
        reset_physics((int)(&c - &physics[0])); /* fall through */
    case SP_PHYSICS_UPDATE: {
        float delta = maxf(time - c.last_time, 0.0f);
        c.remaining += delta;
        c.last_time = time;
        float bx = bone.world_x, by = bone.world_y;
        if (c.reset) {
            c.reset = false;
            c.ux = bx;
            c.uy = by;
        } else {
            float a = c.remaining, i = c.inertia, t = d.step, f = data_->reference_scale, dmp = -1;
            float qx = d.limit * delta, qy = qx * fabsf(sy());
            qx *= fabsf(sx());
            if (px || py) {
                if (px) {
                    float u = (c.ux - bx) * i;
                    c.x_offset += u > qx ? qx : (u < -qx ? -qx : u);
                    c.ux = bx;
                }
                if (py) {
                    float u = (c.uy - by) * i;
                    c.y_offset += u > qy ? qy : (u < -qy ? -qy : u);
                    c.uy = by;
                }
                if (a >= t && t > 0) {
                    dmp = powf(c.damping, 60 * t);
                    float m = c.mass_inverse * t, e = c.strength, w = c.wind * f * sx(), g = c.gravity * f * sy();
                    do {
                        if (px) {
                            c.x_velocity += (w - c.x_offset * e) * m;
                            c.x_offset += c.x_velocity * t;
                            c.x_velocity *= dmp;
                        }
                        if (py) {
                            c.y_velocity -= (g + c.y_offset * e) * m;
                            c.y_offset += c.y_velocity * t;
                            c.y_velocity *= dmp;
                        }
                        a -= t;
                    } while (a >= t);
                }
                if (px) bone.world_x += c.x_offset * mix * d.x;
                if (py) bone.world_y += c.y_offset * mix * d.y;
            }
            if (rotate_or_shear_x || scale_x) {
                float ca = atan2f(bone.c, bone.a), cs, sn, mr = 0;
                float dx = c.cx - bone.world_x, dy = c.cy - bone.world_y;
                if (dx > qx) dx = qx;
                else if (dx < -qx) dx = -qx;
                if (dy > qy) dy = qy;
                else if (dy < -qy) dy = -qy;
                if (rotate_or_shear_x) {
                    mr = (d.rotate + d.shear_x) * mix;
                    float r = atan2f(dy + c.ty, dx + c.tx) - ca - c.rotate_offset * mr;
                    c.rotate_offset += (r - ceilf(r / PI2 - 0.5f) * PI2) * i;
                    r = c.rotate_offset * mr + ca;
                    cs = cosf(r);
                    sn = sinf(r);
                    if (scale_x) {
                        r = l * sqrtf(bone.a * bone.a + bone.c * bone.c);
                        if (r > 0) c.scale_offset += (dx * cs + dy * sn) * i / r;
                    }
                } else {
                    cs = cosf(ca);
                    sn = sinf(ca);
                    float r = l * sqrtf(bone.a * bone.a + bone.c * bone.c);
                    if (r > 0) c.scale_offset += (dx * cs + dy * sn) * i / r;
                }
                a = c.remaining;
                if (a >= t && t > 0) {
                    if (dmp == -1) dmp = powf(c.damping, 60 * t);
                    float m = c.mass_inverse * t, e = c.strength, w = c.wind, g = y_down ? -c.gravity : c.gravity,
                          h = l / f;
                    for (;;) {
                        a -= t;
                        if (scale_x) {
                            c.scale_velocity += (w * cs - g * sn - c.scale_offset * e) * m;
                            c.scale_offset += c.scale_velocity * t;
                            c.scale_velocity *= dmp;
                        }
                        if (rotate_or_shear_x) {
                            c.rotate_velocity -= ((w * sn + g * cs) * h + c.rotate_offset * e) * m;
                            c.rotate_offset += c.rotate_velocity * t;
                            c.rotate_velocity *= dmp;
                            if (a < t) break;
                            float r = c.rotate_offset * mr + ca;
                            cs = cosf(r);
                            sn = sinf(r);
                        } else if (a < t) {
                            break;
                        }
                    }
                }
            }
            c.remaining = a;
        }
        c.cx = bone.world_x;
        c.cy = bone.world_y;
        break;
    }
    case SP_PHYSICS_POSE:
        if (px) bone.world_x += c.x_offset * mix * d.x;
        if (py) bone.world_y += c.y_offset * mix * d.y;
        break;
    }
    if (rotate_or_shear_x) {
        float o = c.rotate_offset * mix, s, cs, a;
        if (d.shear_x > 0) {
            float r = 0;
            if (d.rotate > 0) {
                r = o * d.rotate;
                s = sinf(r);
                cs = cosf(r);
                a = bone.b;
                bone.b = cs * a - s * bone.d;
                bone.d = s * a + cs * bone.d;
            }
            r += o * d.shear_x;
            s = sinf(r);
            cs = cosf(r);
            a = bone.a;
            bone.a = cs * a - s * bone.c;
            bone.c = s * a + cs * bone.c;
        } else {
            o *= d.rotate;
            s = sinf(o);
            cs = cosf(o);
            a = bone.a;
            bone.a = cs * a - s * bone.c;
            bone.c = s * a + cs * bone.c;
            a = bone.b;
            bone.b = cs * a - s * bone.d;
            bone.d = s * a + cs * bone.d;
        }
    }
    if (scale_x) {
        float s = 1 + c.scale_offset * mix * d.scale_x;
        bone.a *= s;
        bone.c *= s;
    }
    if (phys != SP_PHYSICS_POSE) {
        c.tx = l * bone.a;
        c.ty = l * bone.c;
    }
    bone_update_applied(bone);
}

} /* namespace spire */
