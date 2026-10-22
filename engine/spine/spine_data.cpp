/* Spine data: the atlas (JSON from the converter) and the skeleton's binary
 * engine form (converter/spire_convert/spine.py, engine_binary). */
#include "spine.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

namespace spire {

static const float DEG_RAD = 3.14159265358979323846f / 180.0f;

/* ---- atlas ---- */

bool SpAtlas::load(const Json& doc, std::string* error) {
    const Json& p = doc["pages"];
    const Json& r = doc["regions"];
    if (!p.is_array() || !r.is_array()) {
        if (error) *error = "not a spine atlas";
        return false;
    }
    pages.resize(p.size());
    for (size_t i = 0; i < p.size(); ++i) {
        pages[i].texture = p[i]["texture"].str();
        pages[i].width = p[i]["size"][(size_t)0].i();
        pages[i].height = p[i]["size"][1].i();
    }
    regions.resize(r.size());
    for (size_t i = 0; i < r.size(); ++i) {
        const Json& g = r[i];
        SpAtlasRegion& o = regions[i];
        o.name = g["name"].str();
        o.page = g["page"].i();
        o.x = g["x"].i();
        o.y = g["y"].i();
        o.w = g["w"].i();
        o.h = g["h"].i();
        o.ox = g["ox"].i();
        o.oy = g["oy"].i();
        o.ow = g["ow"].i(o.w);
        o.oh = g["oh"].i(o.h);
        o.degrees = g["rotate"].i();
        o.index = g["index"].i(-1);
    }
    return true;
}

const SpAtlasRegion* SpAtlas::find(const std::string& name) const {
    for (size_t i = 0; i < regions.size(); ++i)
        if (regions[i].name == name) return &regions[i];
    return NULL;
}

/* ---- attachments ---- */

SpAttachment::SpAttachment()
    : type(SP_ATT_OTHER), slot(0), seq_count(0), seq_start(0), seq_digits(0), seq_setup(0), x(0), y(0), rotation(0),
      scale_x(1), scale_y(1), width(0), height(0), vertex_count(0), hull(0), closed(false), constant_speed(false),
      end_slot(-1) {}

int SpAttachment::frame(int sequence_index) const {
    if (seq_count <= 0) return 0;
    int i = sequence_index < 0 ? seq_setup : sequence_index;
    if (i < 0) i = 0;
    if (i >= (int)regions.size()) i = (int)regions.size() - 1;
    return i < 0 ? 0 : i;
}

static std::string skin_key(int slot, const std::string& key) { return format("%d/", slot) + key; }

SpAttachment* SpSkin::find(int slot, const std::string& key) const {
    std::map<std::string, SpAttachment*>::const_iterator it = lookup.find(skin_key(slot, key));
    return it == lookup.end() ? NULL : it->second;
}

/* The quad of a region attachment in bone space and its page uvs. The
 * attachment's width x height covers the original image; the atlas may have
 * stripped its blank border, so the quad covers only the packed part. */
static void region_setup(const SpAttachment& a, const SpAtlasRegion* r, const SpAtlas* atlas,
                         std::vector<float>& offsets, std::vector<float>& uvs) {
    float lx = -a.width * 0.5f * a.scale_x, ly = -a.height * 0.5f * a.scale_y;
    float lx2 = -lx, ly2 = -ly;
    if (r && r->ow > 0 && r->oh > 0) {
        float rsx = a.width / (float)r->ow * a.scale_x, rsy = a.height / (float)r->oh * a.scale_y;
        lx += (float)r->ox * rsx;
        ly += (float)r->oy * rsy;
        lx2 = lx + (float)r->w * rsx;
        ly2 = ly + (float)r->h * rsy;
    }
    float cs = cosf(a.rotation * DEG_RAD), sn = sinf(a.rotation * DEG_RAD);
    const float corners[8] = { lx, ly, lx, ly2, lx2, ly2, lx2, ly };
    offsets.resize(8);
    for (int i = 0; i < 8; i += 2) {
        offsets[i] = corners[i] * cs - corners[i + 1] * sn + a.x;
        offsets[i + 1] = corners[i] * sn + corners[i + 1] * cs + a.y;
    }
    uvs.assign(8, 0.0f);
    if (!r || !atlas || r->page < 0 || r->page >= (int)atlas->pages.size()) return;
    const SpAtlasPage& p = atlas->pages[r->page];
    float W = (float)p.width, H = (float)p.height;
    float u = (float)r->x / W, v = (float)r->y / H;
    if (r->degrees == 90) {
        float u2 = (float)(r->x + r->h) / W, v2 = (float)(r->y + r->w) / H;
        const float uv[8] = { u2, v2, u, v2, u, v, u2, v };
        uvs.assign(uv, uv + 8);
    } else {
        float u2 = (float)(r->x + r->w) / W, v2 = (float)(r->y + r->h) / H;
        const float uv[8] = { u, v2, u, v, u2, v, u2, v2 };
        uvs.assign(uv, uv + 8);
    }
}

/* Mesh uvs: region_uvs are 0..1 over the original image (v down); map them
 * into the page around the packed rect. */
static void mesh_setup(const SpAttachment& a, const SpAtlasRegion* r, const SpAtlas* atlas, std::vector<float>& uvs) {
    uvs.assign(a.region_uvs.size(), 0.0f);
    if (!r || !atlas || r->page < 0 || r->page >= (int)atlas->pages.size()) {
        uvs = a.region_uvs;
        return;
    }
    const SpAtlasPage& p = atlas->pages[r->page];
    float W = (float)p.width, H = (float)p.height;
    if (r->degrees == 90) {
        float u0 = (float)(r->x - (r->oh - r->oy - r->h)) / W, v0 = (float)(r->y - (r->ow - r->ox - r->w)) / H;
        float uw = (float)r->oh / W, vh = (float)r->ow / H;
        for (size_t i = 0; i + 1 < uvs.size(); i += 2) {
            uvs[i] = u0 + a.region_uvs[i + 1] * uw;
            uvs[i + 1] = v0 + (1.0f - a.region_uvs[i]) * vh;
        }
    } else {
        float u0 = (float)(r->x - r->ox) / W, v0 = (float)(r->y - (r->oh - r->oy - r->h)) / H;
        float uw = (float)r->ow / W, vh = (float)r->oh / H;
        for (size_t i = 0; i + 1 < uvs.size(); i += 2) {
            uvs[i] = u0 + a.region_uvs[i] * uw;
            uvs[i + 1] = v0 + a.region_uvs[i + 1] * vh;
        }
    }
}

static void note(std::vector<std::string>& list, const std::string& what) {
    for (size_t i = 0; i < list.size(); ++i)
        if (list[i] == what) return;
    list.push_back(what);
}

/* Regions per sequence frame (path + zero-padded start + index). */
static void resolve_regions(SpAttachment& a, const SpAtlas* atlas, std::vector<std::string>& unsupported) {
    int frames = a.seq_count > 0 ? a.seq_count : 1;
    a.regions.assign(frames, (const SpAtlasRegion*)NULL);
    a.frame_uvs.resize(frames);
    a.frame_offsets.resize(frames);
    for (int f = 0; f < frames; ++f) {
        std::string path = a.path;
        if (a.seq_count > 0) {
            char num[32];
            sprintf(num, "%0*d", a.seq_digits > 0 ? a.seq_digits : 1, a.seq_start + f);
            path += num;
        }
        const SpAtlasRegion* r = atlas ? atlas->find(path) : NULL;
        if (atlas && !r) note(unsupported, "missing region " + path);
        a.regions[f] = r;
        if (a.type == SP_ATT_REGION) region_setup(a, r, atlas, a.frame_offsets[f], a.frame_uvs[f]);
        else mesh_setup(a, r, atlas, a.frame_uvs[f]);
    }
}

/* ---- binary reader ---- */

class SpReader {
public:
    SpReader(const u8* d, size_t n) : d_(d), n_(n), p_(0), bad_(false) {}
    bool bad() const { return bad_; }
    size_t pos() const { return p_; }
    bool take(size_t k) {
        if (bad_ || n_ - p_ < k) {
            bad_ = true;
            return false;
        }
        return true;
    }
    u32 u32v() {
        if (!take(4)) return 0;
        u32 v = ((u32)d_[p_] << 24) | ((u32)d_[p_ + 1] << 16) | ((u32)d_[p_ + 2] << 8) | (u32)d_[p_ + 3];
        p_ += 4;
        return v;
    }
    int i32() { return (int)u32v(); }
    int u8v() {
        if (!take(1)) return 0;
        return d_[p_++];
    }
    bool flag() { return u8v() != 0; }
    float f32() {
        u32 v = u32v();
        float f;
        memcpy(&f, &v, 4);
        return f;
    }
    /* A count that must fit in what is left (guards against corrupt data). */
    u32 count(size_t min_item) {
        u32 n = u32v();
        if (min_item && (size_t)n > (n_ - p_) / min_item) {
            bad_ = true;
            return 0;
        }
        return n;
    }
    std::string str(bool* is_null = NULL) {
        int n = i32();
        if (is_null) *is_null = n < 0;
        if (n <= 0 || !take((size_t)n)) return std::string();
        std::string s((const char*)d_ + p_, (size_t)n);
        p_ += (size_t)n;
        return s;
    }
    void ints(std::vector<int>& out) {
        u32 n = count(4);
        out.resize(n);
        for (u32 i = 0; i < n; ++i) out[i] = i32();
    }
    void floats(std::vector<float>& out) {
        u32 n = count(4);
        out.resize(n);
        for (u32 i = 0; i < n; ++i) out[i] = f32();
    }
    Color color() {
        float r = f32(), g = f32(), b = f32(), a = f32();
        return Color(r, g, b, a);
    }

private:
    const u8* d_;
    size_t n_, p_;
    bool bad_;
};

/* ---- curves ---- */

static float bezier1(float p0, float p1, float p2, float p3, float t) {
    float u = 1.0f - t;
    return u * u * u * p0 + 3.0f * u * u * t * p1 + 3.0f * u * t * t * p2 + t * t * t * p3;
}

/* 9 points of the cubic from (t0, v0) to (t1, v1) at t = 0.1 .. 0.9. */
static void sample_bezier(std::vector<float>& out, float t0, float v0, float cx1, float cy1, float cx2, float cy2,
                          float t1, float v1) {
    for (int i = 1; i <= 9; ++i) {
        float t = (float)i / 10.0f;
        out.push_back(bezier1(t0, cx1, cx2, t1, t));
        out.push_back(bezier1(v0, cy1, cy2, v1, t));
    }
}

/* Per key but the last: u8 0 linear, 1 stepped, 2 bezier + 4 floats per
 * value. Value endpoints come from the keys (value i of key k is
 * keys[k * stride + 1 + i]), or 0..1 for a percentage curve (values 0). */
static void read_curves(SpReader& r, SpTimeline& tl, int values) {
    u32 n = r.count(1);
    int stride = tl.values + 1;
    int nv = values ? values : 1;
    for (u32 k = 0; k < n && !r.bad(); ++k) {
        int kind = r.u8v();
        if (kind == 2) {
            tl.curves.kind.push_back((int)tl.curves.points.size());
            for (int i = 0; i < nv; ++i) {
                float cx1 = r.f32(), cy1 = r.f32(), cx2 = r.f32(), cy2 = r.f32();
                if ((int)k + 1 >= tl.key_count()) continue;
                float t0 = tl.time((int)k), t1 = tl.time((int)k + 1);
                float v0 = 0, v1 = 1;
                if (values) {
                    v0 = tl.keys[k * stride + 1 + i];
                    v1 = tl.keys[(k + 1) * stride + 1 + i];
                }
                sample_bezier(tl.curves.points, t0, v0, cx1, cy1, cx2, cy2, t1, v1);
            }
        } else {
            tl.curves.kind.push_back(kind == 1 ? (int)SpCurves::CURVE_STEPPED : (int)SpCurves::CURVE_LINEAR);
        }
    }
}

static int curve_values(int type) {
    switch (type) {
    case SPT_TRANSLATE: case SPT_SCALE: case SPT_SHEAR: return 2;
    case SPT_RGBA: return 4;
    case SPT_RGB: return 3;
    case SPT_RGBA2: return 7;
    case SPT_RGB2: return 6;
    case SPT_TRANSFORM: return 6;
    case SPT_PATH_MIX: return 3;
    case SPT_ROTATE: case SPT_TRANSLATE_X: case SPT_TRANSLATE_Y: case SPT_SCALE_X: case SPT_SCALE_Y:
    case SPT_SHEAR_X: case SPT_SHEAR_Y: case SPT_ALPHA: case SPT_PATH_POSITION: case SPT_PATH_SPACING:
    case SPT_PHYSICS_INERTIA: case SPT_PHYSICS_STRENGTH: case SPT_PHYSICS_DAMPING: case SPT_PHYSICS_MASS:
    case SPT_PHYSICS_WIND: case SPT_PHYSICS_GRAVITY: case SPT_PHYSICS_MIX: return 1;
    default: return 0;
    }
}

static bool read_timeline(SpReader& r, SpTimeline& tl) {
    tl.type = r.u8v();
    int cv = curve_values(tl.type);
    if (cv) {
        tl.values = cv;
        tl.target = r.i32();
        r.floats(tl.keys);
        read_curves(r, tl, cv);
        return !r.bad();
    }
    switch (tl.type) {
    case SPT_ATTACHMENT: {
        tl.target = r.i32();
        r.floats(tl.keys);
        u32 n = r.count(4);
        for (u32 i = 0; i < n && !r.bad(); ++i) tl.names.push_back(r.str());
        break;
    }
    case SPT_INHERIT:
        tl.target = r.i32();
        r.floats(tl.keys);
        r.ints(tl.ints);
        break;
    case SPT_DEFORM: {
        tl.target = r.i32();
        tl.skin = r.i32();
        tl.attachment = r.str();
        r.floats(tl.keys);
        u32 n = r.count(4);
        tl.deforms.resize(n);
        for (u32 i = 0; i < n && !r.bad(); ++i) r.floats(tl.deforms[i]);
        read_curves(r, tl, 0);
        break;
    }
    case SPT_SEQUENCE:
        tl.target = r.i32();
        tl.skin = r.i32();
        tl.attachment = r.str();
        r.floats(tl.keys); /* time, mode, index, delay per key */
        break;
    case SPT_DRAW_ORDER: {
        r.i32();
        r.floats(tl.keys);
        u32 n = r.count(4);
        tl.orders.resize(n);
        for (u32 i = 0; i < n && !r.bad(); ++i) r.ints(tl.orders[i]);
        break;
    }
    case SPT_IK:
        tl.values = 2; /* mix, softness */
        tl.target = r.i32();
        r.floats(tl.keys);
        r.ints(tl.ints);
        r.ints(tl.flags);
        read_curves(r, tl, 2);
        break;
    case SPT_PHYSICS_RESET:
        tl.target = r.i32();
        r.floats(tl.keys);
        break;
    case SPT_EVENT: {
        r.i32();
        r.floats(tl.keys);
        u32 n = r.count(13);
        for (u32 i = 0; i < n && !r.bad(); ++i) {
            tl.names.push_back(r.str());
            tl.ints.push_back(r.i32());
            tl.fvals.push_back(r.f32());
            tl.strs.push_back(r.str());
        }
        break;
    }
    default:
        return false;
    }
    return !r.bad();
}

/* ---- skeleton data ---- */

SpSkeletonData::SpSkeletonData() : x(0), y(0), width(0), height(0), reference_scale(100), atlas(NULL) {}

SpSkeletonData::~SpSkeletonData() {
    for (size_t i = 0; i < skins.size(); ++i) {
        for (size_t k = 0; k < skins[i]->attachments.size(); ++k) delete skins[i]->attachments[k];
        delete skins[i];
    }
}

bool SpSkeletonData::load(const u8* data, size_t size, const SpAtlas* atl, std::string* error) {
    if (size < 8 || memcmp(data, "SPB1", 4) != 0) {
        if (error) *error = "not a spine skeleton (SPB1)";
        return false;
    }
    SpReader r(data + 4, size - 4);
    u32 version = r.u32v(); /* 2: the animations' event timelines kept */
    if (version != 1 && version != 2) {
        if (error) *error = "spine skeleton: unknown version";
        return false;
    }
    atlas = atl;
    x = r.f32();
    y = r.f32();
    width = r.f32();
    height = r.f32();
    reference_scale = r.f32();

    bones.resize(r.count(30));
    for (size_t i = 0; i < bones.size() && !r.bad(); ++i) {
        SpBoneData& o = bones[i];
        o.name = r.str();
        o.parent = r.i32();
        o.x = r.f32();
        o.y = r.f32();
        o.rotation = r.f32();
        o.scale_x = r.f32();
        o.scale_y = r.f32();
        o.shear_x = r.f32();
        o.shear_y = r.f32();
        o.length = r.f32();
        o.inherit = r.u8v();
        o.skin_required = r.flag();
        if (o.parent >= (int)i) {
            if (error) *error = "bone parent after child";
            return false;
        }
    }
    slots.resize(r.count(30));
    for (size_t i = 0; i < slots.size() && !r.bad(); ++i) {
        SpSlotData& o = slots[i];
        o.name = r.str();
        o.bone = r.i32();
        o.color = r.color();
        o.has_dark = r.flag();
        if (o.has_dark) {
            /* not drawn, as spine-godot (see spine.h) */
            r.f32();
            r.f32();
            r.f32();
        }
        o.attachment = r.str();
        o.blend = r.u8v();
        if (o.bone < 0 || o.bone >= (int)bones.size()) {
            if (error) *error = "slot bone out of range";
            return false;
        }
    }
    ik.resize(r.count(20));
    for (size_t i = 0; i < ik.size() && !r.bad(); ++i) {
        SpIkData& o = ik[i];
        o.name = r.str();
        o.order = r.i32();
        r.ints(o.bones);
        o.target = r.i32();
        o.bend = r.i32();
        o.compress = r.flag();
        o.stretch = r.flag();
        o.uniform = r.flag();
        o.mix = r.f32();
        o.softness = r.f32();
        o.skin_required = r.flag();
    }
    transform.resize(r.count(20));
    for (size_t i = 0; i < transform.size() && !r.bad(); ++i) {
        SpTransformData& o = transform[i];
        o.name = r.str();
        o.order = r.i32();
        r.ints(o.bones);
        o.target = r.i32();
        o.local = r.flag();
        o.relative = r.flag();
        o.offset_rotation = r.f32();
        o.offset_x = r.f32();
        o.offset_y = r.f32();
        o.offset_scale_x = r.f32();
        o.offset_scale_y = r.f32();
        o.offset_shear_y = r.f32();
        o.mix_rotate = r.f32();
        o.mix_x = r.f32();
        o.mix_y = r.f32();
        o.mix_scale_x = r.f32();
        o.mix_scale_y = r.f32();
        o.mix_shear_y = r.f32();
        o.skin_required = r.flag();
    }
    path.resize(r.count(20));
    for (size_t i = 0; i < path.size() && !r.bad(); ++i) {
        SpPathData& o = path[i];
        o.name = r.str();
        o.order = r.i32();
        r.ints(o.bones);
        o.target = r.i32();
        o.position_mode = r.u8v();
        o.spacing_mode = r.u8v();
        o.rotate_mode = r.u8v();
        o.offset_rotation = r.f32();
        o.position = r.f32();
        o.spacing = r.f32();
        o.mix_rotate = r.f32();
        o.mix_x = r.f32();
        o.mix_y = r.f32();
        o.skin_required = r.flag();
    }
    physics.resize(r.count(20));
    for (size_t i = 0; i < physics.size() && !r.bad(); ++i) {
        SpPhysicsData& o = physics[i];
        o.name = r.str();
        o.order = r.i32();
        o.bone = r.i32();
        o.x = r.f32();
        o.y = r.f32();
        o.rotate = r.f32();
        o.scale_x = r.f32();
        o.shear_x = r.f32();
        o.limit = r.f32();
        o.step = r.f32();
        o.inertia = r.f32();
        o.strength = r.f32();
        o.damping = r.f32();
        o.mass_inverse = r.f32();
        o.wind = r.f32();
        o.gravity = r.f32();
        o.mix = r.f32();
        o.global = r.u8v();
        o.skin_required = r.flag();
    }

    u32 nskins = r.count(20);
    for (u32 i = 0; i < nskins && !r.bad(); ++i) {
        SpSkin* skin = new SpSkin();
        skins.push_back(skin);
        skin->name = r.str();
        r.ints(skin->bones);
        r.ints(skin->ik);
        r.ints(skin->transform);
        r.ints(skin->path);
        r.ints(skin->physics);
        u32 natt = r.count(20);
        for (u32 k = 0; k < natt && !r.bad(); ++k) {
            SpAttachment* a = new SpAttachment();
            skin->attachments.push_back(a);
            int t = r.u8v();
            a->type = t <= SP_ATT_CLIPPING ? t : SP_ATT_OTHER;
            a->slot = r.i32();
            a->key = r.str();
            a->name = r.str();
            a->path = r.str();
            a->timeline_key = r.str();
            a->color = r.color();
            if (r.flag()) {
                a->seq_count = r.i32();
                a->seq_start = r.i32();
                a->seq_digits = r.i32();
                a->seq_setup = r.i32();
            }
            if (a->type == SP_ATT_REGION) {
                a->x = r.f32();
                a->y = r.f32();
                a->rotation = r.f32();
                a->scale_x = r.f32();
                a->scale_y = r.f32();
                a->width = r.f32();
                a->height = r.f32();
            } else if (a->type == SP_ATT_POINT) {
                a->rotation = r.f32();
                a->x = r.f32();
                a->y = r.f32();
            } else if (a->type != SP_ATT_OTHER) {
                a->vertex_count = r.i32();
                r.ints(a->bones);
                r.floats(a->vertices);
                if (a->type == SP_ATT_MESH) {
                    r.floats(a->region_uvs);
                    r.ints(a->triangles);
                    a->hull = r.i32();
                } else if (a->type == SP_ATT_PATH) {
                    a->closed = r.flag();
                    a->constant_speed = r.flag();
                    r.floats(a->lengths);
                } else if (a->type == SP_ATT_CLIPPING) {
                    a->end_slot = r.i32();
                }
            } else {
                if (error) *error = format("unknown attachment type %d", t);
                return false;
            }
            if (a->type == SP_ATT_REGION || a->type == SP_ATT_MESH) resolve_regions(*a, atlas, unsupported);
            skin->lookup[skin_key(a->slot, a->key)] = a;
        }
    }

    animations.resize(r.count(8));
    for (size_t i = 0; i < animations.size() && !r.bad(); ++i) {
        SpAnimation& o = animations[i];
        o.name = r.str();
        o.duration = r.f32();
        u32 n = r.count(5);
        o.timelines.reserve(n);
        for (u32 k = 0; k < n && !r.bad(); ++k) {
            SpTimeline tl;
            if (!read_timeline(r, tl)) {
                if (error) *error = format("animation %s: bad timeline (type %d)", o.name.c_str(), tl.type);
                return false;
            }
            o.timelines.push_back(tl);
        }
    }
    if (r.bad()) {
        if (error) *error = "spine skeleton: truncated";
        return false;
    }
    size_t end = r.pos() + 4;
    if (size - 4 < end || memcmp(data + 4 + r.pos(), "END1", 4) != 0) {
        if (error) *error = "spine skeleton: bad end marker";
        return false;
    }
    return true;
}

int SpSkeletonData::find_bone(const std::string& name) const {
    for (size_t i = 0; i < bones.size(); ++i)
        if (bones[i].name == name) return (int)i;
    return -1;
}

int SpSkeletonData::find_slot(const std::string& name) const {
    for (size_t i = 0; i < slots.size(); ++i)
        if (slots[i].name == name) return (int)i;
    return -1;
}

const SpAnimation* SpSkeletonData::find_animation(const std::string& name) const {
    for (size_t i = 0; i < animations.size(); ++i)
        if (animations[i].name == name) return &animations[i];
    return NULL;
}

const SpSkin* SpSkeletonData::find_skin(const std::string& name) const {
    for (size_t i = 0; i < skins.size(); ++i)
        if (skins[i]->name == name) return skins[i];
    return NULL;
}

const SpSkin* SpSkeletonData::default_skin() const { return find_skin("default"); }

} /* namespace spire */
