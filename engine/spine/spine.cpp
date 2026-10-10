#include "spine.h"

#include <math.h>
#include <string.h>

namespace spire {

static const float DEG_RAD = 3.14159265358979323846f / 180.0f;

static float jf(const Json& v, float fallback = 0) { return v.is_number() ? (float)v.num() : fallback; }

static void jfloats(const Json& v, std::vector<float>& out) {
    out.resize(v.size());
    for (size_t i = 0; i < v.size(); ++i) out[i] = (float)v[i].num();
}

static Color jcolor(const Json& v) {
    if (v.size() < 4) return Color();
    return Color(jf(v[(size_t)0]), jf(v[1]), jf(v[2]), jf(v[3]));
}

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
    : type(SP_ATT_OTHER), slot(0), has_sequence(false), x(0), y(0), rotation(0), scale_x(1), scale_y(1), width(0),
      height(0), vertex_count(0), hull(0), region(NULL), end_slot(-1) {
    for (int i = 0; i < 8; ++i) offsets[i] = 0;
}

static std::string skin_key(int slot, const std::string& key) { return format("%d/", slot) + key; }

SpAttachment* SpSkin::find(int slot, const std::string& key) const {
    std::map<std::string, SpAttachment*>::const_iterator it = lookup.find(skin_key(slot, key));
    return it == lookup.end() ? NULL : it->second;
}

/* The quad of a region attachment in bone space and its page uvs. The
 * attachment's width x height covers the original image; the atlas may have
 * stripped its blank border, so the quad covers only the packed part. */
static void region_setup(SpAttachment& a, const SpAtlas* atlas) {
    float lx = -a.width * 0.5f * a.scale_x, ly = -a.height * 0.5f * a.scale_y;
    float lx2 = -lx, ly2 = -ly;
    const SpAtlasRegion* r = a.region;
    if (r && r->ow > 0 && r->oh > 0) {
        float rsx = a.width / (float)r->ow * a.scale_x, rsy = a.height / (float)r->oh * a.scale_y;
        lx += (float)r->ox * rsx;
        ly += (float)r->oy * rsy;
        lx2 = lx + (float)r->w * rsx;
        ly2 = ly + (float)r->h * rsy;
    }
    float cs = cosf(a.rotation * DEG_RAD), sn = sinf(a.rotation * DEG_RAD);
    const float corners[8] = { lx, ly, lx, ly2, lx2, ly2, lx2, ly };
    for (int i = 0; i < 8; i += 2) {
        a.offsets[i] = corners[i] * cs - corners[i + 1] * sn + a.x;
        a.offsets[i + 1] = corners[i] * sn + corners[i + 1] * cs + a.y;
    }
    a.uvs.assign(8, 0.0f);
    if (!r || !atlas || r->page < 0 || r->page >= (int)atlas->pages.size()) return;
    const SpAtlasPage& p = atlas->pages[r->page];
    float W = (float)p.width, H = (float)p.height;
    float u = (float)r->x / W, v = (float)r->y / H;
    if (r->degrees == 90) {
        float u2 = (float)(r->x + r->h) / W, v2 = (float)(r->y + r->w) / H;
        const float uv[8] = { u2, v2, u, v2, u, v, u2, v };
        a.uvs.assign(uv, uv + 8);
    } else {
        float u2 = (float)(r->x + r->w) / W, v2 = (float)(r->y + r->h) / H;
        const float uv[8] = { u, v2, u, v, u2, v, u2, v2 };
        a.uvs.assign(uv, uv + 8);
    }
}

/* Mesh uvs: region_uvs are 0..1 over the original image (v down); map them
 * into the page around the packed rect. */
static void mesh_setup(SpAttachment& a, const SpAtlas* atlas) {
    a.uvs.assign(a.region_uvs.size(), 0.0f);
    const SpAtlasRegion* r = a.region;
    if (!r || !atlas || r->page < 0 || r->page >= (int)atlas->pages.size()) {
        a.uvs = a.region_uvs;
        return;
    }
    const SpAtlasPage& p = atlas->pages[r->page];
    float W = (float)p.width, H = (float)p.height;
    if (r->degrees == 90) {
        float u0 = (float)(r->x - (r->oh - r->oy - r->h)) / W, v0 = (float)(r->y - (r->ow - r->ox - r->w)) / H;
        float uw = (float)r->oh / W, vh = (float)r->ow / H;
        for (size_t i = 0; i + 1 < a.uvs.size(); i += 2) {
            a.uvs[i] = u0 + a.region_uvs[i + 1] * uw;
            a.uvs[i + 1] = v0 + (1.0f - a.region_uvs[i]) * vh;
        }
    } else {
        float u0 = (float)(r->x - r->ox) / W, v0 = (float)(r->y - (r->oh - r->oy - r->h)) / H;
        float uw = (float)r->ow / W, vh = (float)r->oh / H;
        for (size_t i = 0; i + 1 < a.uvs.size(); i += 2) {
            a.uvs[i] = u0 + a.region_uvs[i] * uw;
            a.uvs[i + 1] = v0 + a.region_uvs[i + 1] * vh;
        }
    }
}

static void read_vertices(const Json& j, SpAttachment& a) {
    a.vertex_count = j["vertex_count"].i();
    if (j.has("bones")) {
        const Json& b = j["bones"];
        size_t i = 0;
        while (i < b.size()) {
            int k = b[i++].i();
            a.bones.push_back(k);
            for (int n = 0; n < k && i + 3 < b.size(); ++n) {
                a.bones.push_back(b[i].i());
                a.vertices.push_back(jf(b[i + 1]));
                a.vertices.push_back(jf(b[i + 2]));
                a.vertices.push_back(jf(b[i + 3]));
                i += 4;
            }
        }
    } else {
        jfloats(j["vertices"], a.vertices);
    }
}

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

static float polyline(const float* p, float t0, float v0, float t1, float v1, float time) {
    if (p[0] > time) return v0 + (time - t0) / (p[0] - t0) * (p[1] - v0);
    for (int i = 2; i < 18; i += 2) {
        if (p[i] >= time) {
            float x0 = p[i - 2], y0 = p[i - 1];
            return y0 + (time - x0) / (p[i] - x0) * (p[i + 1] - y0);
        }
    }
    return p[17] + (time - p[16]) / (t1 - p[16]) * (v1 - p[17]);
}

/* curves: per key but the last, 0 linear, 1 stepped or 4 floats per value
 * (cx1, cy1, cx2, cy2). Value endpoints come from the keys, or 0..1 for a
 * percentage curve (values == 0, deform). */
static void read_curves(const Json& c, SpTimeline& tl, int values) {
    int stride = tl.values + 1;
    for (size_t k = 0; k < c.size(); ++k) {
        const Json& v = c[k];
        if (v.is_array()) {
            tl.curves.kind.push_back((int)tl.curves.points.size());
            int n = values ? values : 1;
            for (int i = 0; i < n && (size_t)(i * 4 + 3) < v.size(); ++i) {
                float t0 = tl.time((int)k), t1 = tl.time((int)k + 1);
                float v0 = 0, v1 = 1;
                if (values) {
                    v0 = tl.keys[k * stride + 1 + i];
                    v1 = tl.keys[(k + 1) * stride + 1 + i];
                }
                sample_bezier(tl.curves.points, t0, v0, jf(v[i * 4]), jf(v[i * 4 + 1]), jf(v[i * 4 + 2]),
                              jf(v[i * 4 + 3]), t1, v1);
            }
        } else {
            tl.curves.kind.push_back(v.i() == 1 ? (int)SpCurves::CURVE_STEPPED : (int)SpCurves::CURVE_LINEAR);
        }
    }
}

/* Value i of key k at time (key k <= time < key k + 1). */
static float curve_value(const SpTimeline& tl, int k, int i, float time) {
    int stride = tl.values + 1;
    const float* key = &tl.keys[k * stride];
    float v0 = key[1 + i];
    int kind = k < (int)tl.curves.kind.size() ? tl.curves.kind[k] : SpCurves::CURVE_STEPPED;
    if (kind == SpCurves::CURVE_STEPPED || (k + 1) * stride >= (int)tl.keys.size()) return v0;
    float t0 = key[0], t1 = key[stride], v1 = key[stride + 1 + i];
    if (kind == SpCurves::CURVE_LINEAR) return t1 > t0 ? v0 + (time - t0) / (t1 - t0) * (v1 - v0) : v0;
    return polyline(&tl.curves.points[kind + i * 18], t0, v0, t1, v1, time);
}

static float curve_percent(const SpTimeline& tl, int k, float time) {
    int kind = k < (int)tl.curves.kind.size() ? tl.curves.kind[k] : SpCurves::CURVE_STEPPED;
    if (kind == SpCurves::CURVE_STEPPED || k + 1 >= tl.key_count()) return 0;
    float t0 = tl.time(k), t1 = tl.time(k + 1);
    if (kind == SpCurves::CURVE_LINEAR) return t1 > t0 ? (time - t0) / (t1 - t0) : 0;
    return polyline(&tl.curves.points[kind], t0, 0, t1, 1, time);
}

/* Index of the last key at or before time; -1 before the first. */
static int key_index(const SpTimeline& tl, float time) {
    int n = tl.key_count();
    if (n == 0 || time < tl.time(0)) return -1;
    int lo = 0, hi = n - 1;
    while (lo < hi) {
        int mid = (lo + hi + 1) / 2;
        if (tl.time(mid) <= time) lo = mid;
        else hi = mid - 1;
    }
    return lo;
}

/* ---- skeleton data ---- */

SpSkeletonData::SpSkeletonData() : x(0), y(0), width(0), height(0), atlas(NULL) {}

SpSkeletonData::~SpSkeletonData() {
    for (size_t i = 0; i < skins.size(); ++i) {
        for (size_t k = 0; k < skins[i]->attachments.size(); ++k) delete skins[i]->attachments[k];
        delete skins[i];
    }
}

static int timeline_type(const std::string& t, int* values) {
    static const struct { const char* name; int type, values; } T[] = {
        { "rotate", SPT_ROTATE, 1 }, { "translate", SPT_TRANSLATE, 2 }, { "translatex", SPT_TRANSLATE_X, 1 },
        { "translatey", SPT_TRANSLATE_Y, 1 }, { "scale", SPT_SCALE, 2 }, { "scalex", SPT_SCALE_X, 1 },
        { "scaley", SPT_SCALE_Y, 1 }, { "shear", SPT_SHEAR, 2 }, { "shearx", SPT_SHEAR_X, 1 },
        { "sheary", SPT_SHEAR_Y, 1 }, { "inherit", SPT_INHERIT, 0 }, { "attachment", SPT_ATTACHMENT, 0 },
        { "rgba", SPT_RGBA, 4 }, { "rgb", SPT_RGB, 3 }, { "alpha", SPT_ALPHA, 1 }, { "rgba2", SPT_RGBA2, 7 },
        { "rgb2", SPT_RGB2, 6 }, { "deform", SPT_DEFORM, 0 }, { "sequence", SPT_SEQUENCE, 0 },
        { "draw_order", SPT_DRAW_ORDER, 0 }, { "event", SPT_EVENT, 0 }
    };
    for (size_t i = 0; i < sizeof(T) / sizeof(T[0]); ++i) {
        if (t == T[i].name) {
            *values = T[i].values;
            return T[i].type;
        }
    }
    *values = 0;
    return SPT_OTHER;
}

static void note(std::vector<std::string>& list, const std::string& what) {
    for (size_t i = 0; i < list.size(); ++i)
        if (list[i] == what) return;
    list.push_back(what);
}

bool SpSkeletonData::load(const Json& doc, const SpAtlas* atl, std::string* error) {
    if (doc["kind"].str() != "spine_skeleton") {
        if (error) *error = "not a spine skeleton";
        return false;
    }
    atlas = atl;
    version = doc["version"].str();
    x = jf(doc["x"]);
    y = jf(doc["y"]);
    width = jf(doc["width"]);
    height = jf(doc["height"]);
    const Json& jb = doc["bones"];
    bones.resize(jb.size());
    for (size_t i = 0; i < jb.size(); ++i) {
        const Json& b = jb[i];
        SpBoneData& o = bones[i];
        o.name = b["name"].str();
        o.parent = b["parent"].is_number() ? b["parent"].i() : -1;
        o.x = jf(b["x"]);
        o.y = jf(b["y"]);
        o.rotation = jf(b["rotation"]);
        o.scale_x = jf(b["scale_x"], 1);
        o.scale_y = jf(b["scale_y"], 1);
        o.shear_x = jf(b["shear_x"]);
        o.shear_y = jf(b["shear_y"]);
        o.inherit = b["inherit"].i();
        o.skin_required = b["skin_required"].b();
        if (o.parent >= (int)i) {
            if (error) *error = "bone parent after child";
            return false;
        }
    }
    const Json& js = doc["slots"];
    slots.resize(js.size());
    for (size_t i = 0; i < js.size(); ++i) {
        const Json& s = js[i];
        SpSlotData& o = slots[i];
        o.name = s["name"].str();
        o.bone = s["bone"].i();
        o.color = jcolor(s["color"]);
        o.has_dark = s.has("dark");
        o.attachment = s["attachment"].is_string() ? s["attachment"].str() : std::string();
        o.blend = s["blend"].i();
        if (o.has_dark) note(unsupported, "dark colour");
        if (o.blend == SP_BLEND_SCREEN) note(unsupported, "screen blend");
    }
    static const char* const constraints[4] = { "ik", "transform", "path", "physics" };
    for (int i = 0; i < 4; ++i)
        if (doc[constraints[i]].size()) note(unsupported, std::string(constraints[i]) + " constraints");

    const Json& jk = doc["skins"];
    for (size_t i = 0; i < jk.size(); ++i) {
        const Json& s = jk[i];
        SpSkin* skin = new SpSkin();
        skins.push_back(skin);
        skin->name = s["name"].str();
        for (size_t k = 0; k < s["bones"].size(); ++k) skin->bones.push_back(s["bones"][k].i());
        const Json& atts = s["attachments"];
        for (size_t k = 0; k < atts.size(); ++k) {
            const Json& j = atts[k];
            SpAttachment* a = new SpAttachment();
            const std::string& t = j["type"].str();
            a->type = t == "region" ? SP_ATT_REGION : t == "mesh" ? SP_ATT_MESH : t == "boundingbox" ? SP_ATT_BOUNDING_BOX
                    : t == "path" ? SP_ATT_PATH : t == "point" ? SP_ATT_POINT : t == "clipping" ? SP_ATT_CLIPPING
                    : SP_ATT_OTHER;
            a->key = j["key"].str();
            a->name = j["name"].str();
            a->path = j["path"].is_string() ? j["path"].str() : a->name;
            a->timeline_key = j["timeline_key"].is_string() ? j["timeline_key"].str() : a->key;
            a->slot = j["slot"].i();
            if (j.has("color")) a->color = jcolor(j["color"]);
            a->has_sequence = j.has("sequence");
            if (a->has_sequence) note(unsupported, "sequences");
            if (a->type == SP_ATT_REGION) {
                a->x = jf(j["x"]);
                a->y = jf(j["y"]);
                a->rotation = jf(j["rotation"]);
                a->scale_x = jf(j["scale_x"], 1);
                a->scale_y = jf(j["scale_y"], 1);
                a->width = jf(j["width"]);
                a->height = jf(j["height"]);
            } else if (a->type != SP_ATT_POINT && a->type != SP_ATT_OTHER) {
                read_vertices(j, *a);
                if (a->type == SP_ATT_MESH) {
                    jfloats(j["uvs"], a->region_uvs);
                    const Json& tr = j["triangles"];
                    a->triangles.resize(tr.size());
                    for (size_t n = 0; n < tr.size(); ++n) a->triangles[n] = tr[n].i();
                    a->hull = j["hull"].i();
                }
                if (a->type == SP_ATT_CLIPPING) {
                    a->end_slot = j["end_slot"].i();
                    note(unsupported, "clipping");
                }
            }
            if (a->type == SP_ATT_REGION || a->type == SP_ATT_MESH) {
                a->region = atlas ? atlas->find(a->path) : NULL;
                if (atlas && !a->region) note(unsupported, "missing region " + a->path);
                if (a->type == SP_ATT_REGION) region_setup(*a, atlas);
                else mesh_setup(*a, atlas);
            }
            skin->attachments.push_back(a);
            skin->lookup[skin_key(a->slot, a->key)] = a;
        }
    }

    const Json& ja = doc["animations"];
    animations.resize(ja.size());
    for (size_t i = 0; i < ja.size(); ++i) {
        const Json& an = ja[i];
        SpAnimation& o = animations[i];
        o.name = an["name"].str();
        o.duration = jf(an["duration"]);
        const Json& tls = an["timelines"];
        for (size_t k = 0; k < tls.size(); ++k) {
            const Json& t = tls[k];
            SpTimeline tl;
            tl.type = timeline_type(t["type"].str(), &tl.values);
            tl.target = t.has("bone") ? t["bone"].i() : t["slot"].i();
            tl.skin = t["skin"].i();
            switch (tl.type) {
            case SPT_INHERIT:
                jfloats(t["times"], tl.keys);
                for (size_t n = 0; n < t["modes"].size(); ++n) tl.ints.push_back(t["modes"][n].i());
                break;
            case SPT_ATTACHMENT:
                jfloats(t["times"], tl.keys);
                for (size_t n = 0; n < t["names"].size(); ++n)
                    tl.names.push_back(t["names"][n].is_string() ? t["names"][n].str() : std::string());
                break;
            case SPT_DEFORM:
                jfloats(t["times"], tl.keys);
                tl.attachment = t["attachment"].str();
                tl.deforms.resize(t["vertices"].size());
                for (size_t n = 0; n < t["vertices"].size(); ++n) jfloats(t["vertices"][n], tl.deforms[n]);
                read_curves(t["curves"], tl, 0);
                break;
            case SPT_DRAW_ORDER:
                jfloats(t["times"], tl.keys);
                tl.orders.resize(t["orders"].size());
                for (size_t n = 0; n < t["orders"].size(); ++n)
                    for (size_t m = 0; m < t["orders"][n].size(); ++m) tl.orders[n].push_back(t["orders"][n][m].i());
                break;
            case SPT_EVENT:
                continue; /* events reach the game through the logic, not the presentation */
            case SPT_SEQUENCE:
            case SPT_OTHER:
                note(unsupported, t["type"].str() + " timelines");
                continue;
            default:
                jfloats(t["frames"], tl.keys);
                read_curves(t["curves"], tl, tl.values);
                if (tl.type == SPT_RGBA2 || tl.type == SPT_RGB2) note(unsupported, "dark colour timelines");
                break;
            }
            o.timelines.push_back(tl);
        }
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

/* ---- skeleton ---- */

SpSkeleton::SpSkeleton(const SpSkeletonData* data) : x(0), y(0), scale_x(1), scale_y(1), data_(data), skin_(NULL) {
    bones.resize(data->bones.size());
    for (size_t i = 0; i < bones.size(); ++i) {
        bones[i].data = &data->bones[i];
        bones[i].parent = data->bones[i].parent;
    }
    slots.resize(data->slots.size());
    for (size_t i = 0; i < slots.size(); ++i) slots[i].data = &data->slots[i];
    set_skin(NULL);
    set_to_setup_pose();
}

void SpSkeleton::set_skin(const SpSkin* skin) {
    skin_ = skin;
    for (size_t i = 0; i < bones.size(); ++i) {
        bool active = !bones[i].data->skin_required;
        if (!active && skin)
            for (size_t k = 0; k < skin->bones.size(); ++k)
                if (skin->bones[k] == (int)i) active = true;
        if (active && bones[i].parent >= 0 && !bones[bones[i].parent].active) active = false;
        bones[i].active = active;
    }
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
}

void SpSkeleton::set_slots_to_setup_pose() {
    draw_order.resize(slots.size());
    for (size_t i = 0; i < slots.size(); ++i) {
        SpSlot& s = slots[i];
        draw_order[i] = (int)i;
        s.color = s.data->color;
        s.attachment = s.data->attachment.empty() ? NULL : attachment((int)i, s.data->attachment);
        s.deform.clear();
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

/* World transforms, parents first (bones are stored that way). */
void SpSkeleton::update_world_transform() {
    for (size_t i = 0; i < bones.size(); ++i) {
        SpBone& b = bones[i];
        if (!b.active) continue;
        float sx = scale_x, sy = scale_y;
        float rx = (b.rotation + b.shear_x) * DEG_RAD, ry = (b.rotation + 90.0f + b.shear_y) * DEG_RAD;
        if (b.parent < 0) {
            b.a = cosf(rx) * b.scale_x * sx;
            b.b = cosf(ry) * b.scale_y * sx;
            b.c = sinf(rx) * b.scale_x * sy;
            b.d = sinf(ry) * b.scale_y * sy;
            b.world_x = b.x * sx + x;
            b.world_y = b.y * sy + y;
            continue;
        }
        const SpBone& p = bones[b.parent];
        float pa = p.a, pb = p.b, pc = p.c, pd = p.d;
        b.world_x = pa * b.x + pb * b.y + p.world_x;
        b.world_y = pc * b.x + pd * b.y + p.world_y;
        switch (b.inherit) {
        case SP_INHERIT_NORMAL: {
            float la = cosf(rx) * b.scale_x, lb = cosf(ry) * b.scale_y;
            float lc = sinf(rx) * b.scale_x, ld = sinf(ry) * b.scale_y;
            b.a = pa * la + pb * lc;
            b.b = pa * lb + pb * ld;
            b.c = pc * la + pd * lc;
            b.d = pc * lb + pd * ld;
            continue;
        }
        case SP_INHERIT_ONLY_TRANSLATION:
            b.a = cosf(rx) * b.scale_x;
            b.b = cosf(ry) * b.scale_y;
            b.c = sinf(rx) * b.scale_x;
            b.d = sinf(ry) * b.scale_y;
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
                prx = atan2f(pc, pa) / DEG_RAD;
            } else {
                pa = 0;
                pc = 0;
                prx = 90.0f - atan2f(pd, pb) / DEG_RAD;
            }
            float rx2 = (b.rotation + b.shear_x - prx) * DEG_RAD, ry2 = (b.rotation + b.shear_y - prx + 90.0f) * DEG_RAD;
            float la = cosf(rx2) * b.scale_x, lb = cosf(ry2) * b.scale_y;
            float lc = sinf(rx2) * b.scale_x, ld = sinf(ry2) * b.scale_y;
            b.a = pa * la - pb * lc;
            b.b = pa * lb - pb * ld;
            b.c = pc * la + pd * lc;
            b.d = pc * lb + pd * ld;
            break;
        }
        default: { /* no scale (or reflection): the parent's rotation only */
            float r = b.rotation * DEG_RAD, cs = cosf(r), sn = sinf(r);
            float za = (pa * cs + pb * sn) / sx, zc = (pc * cs + pd * sn) / sy;
            float s = sqrtf(za * za + zc * zc);
            if (s > 0.00001f) s = 1.0f / s;
            za *= s;
            zc *= s;
            s = sqrtf(za * za + zc * zc);
            if (b.inherit == SP_INHERIT_NO_SCALE && ((pa * pd - pb * pc < 0) != ((sx < 0) != (sy < 0)))) s = -s;
            r = 3.14159265358979323846f / 2.0f + atan2f(zc, za);
            float zb = cosf(r) * s, zd = sinf(r) * s;
            float shx = b.shear_x * DEG_RAD, shy = (90.0f + b.shear_y) * DEG_RAD;
            float la = cosf(shx) * b.scale_x, lb = cosf(shy) * b.scale_y;
            float lc = sinf(shx) * b.scale_x, ld = sinf(shy) * b.scale_y;
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
}

void SpSkeleton::world_vertices(const SpSlot& slot, std::vector<float>& out) const {
    const SpAttachment* a = slot.attachment;
    out.clear();
    if (!a) return;
    const SpBone& bone = bones[slot.data->bone];
    if (a->type == SP_ATT_REGION) {
        out.resize(8);
        for (int i = 0; i < 8; i += 2) {
            float ox = a->offsets[i], oy = a->offsets[i + 1];
            out[i] = bone.a * ox + bone.b * oy + bone.world_x;
            out[i + 1] = bone.c * ox + bone.d * oy + bone.world_y;
        }
        return;
    }
    const std::vector<float>& def = slot.deform;
    if (!a->weighted()) {
        const std::vector<float>& v = def.size() == a->vertices.size() ? def : a->vertices;
        out.resize(v.size());
        for (size_t i = 0; i + 1 < v.size(); i += 2) {
            out[i] = bone.a * v[i] + bone.b * v[i + 1] + bone.world_x;
            out[i + 1] = bone.c * v[i] + bone.d * v[i + 1] + bone.world_y;
        }
        return;
    }
    bool has_def = (int)def.size() == a->deform_length();
    out.resize(a->vertex_count * 2);
    size_t bi = 0, vi = 0, di = 0;
    for (int n = 0; n < a->vertex_count && bi < a->bones.size(); ++n) {
        int count = a->bones[bi++];
        float wx = 0, wy = 0;
        for (int k = 0; k < count; ++k, vi += 3, di += 2) {
            const SpBone& b = bones[a->bones[bi++]];
            float vx = a->vertices[vi], vy = a->vertices[vi + 1], w = a->vertices[vi + 2];
            if (has_def) {
                vx += def[di];
                vy += def[di + 1];
            }
            wx += (b.a * vx + b.b * vy + b.world_x) * w;
            wy += (b.c * vx + b.d * vy + b.world_y) * w;
        }
        out[n * 2] = wx;
        out[n * 2 + 1] = wy;
    }
}

/* ---- animation ---- */

static float mixf(float from, float to, float alpha) { return alpha >= 1.0f ? to : from + (to - from) * alpha; }

static void set_attachment(SpSkeleton& skel, int slot, const std::string& key) {
    SpSlot& s = skel.slots[slot];
    SpAttachment* a = key.empty() ? NULL : skel.attachment(slot, key);
    if (a == s.attachment) return;
    if (!a || !s.attachment || a->timeline_key != s.attachment->timeline_key) s.deform.clear();
    s.attachment = a;
}

static void apply_timeline(const SpTimeline& tl, SpSkeleton& skel, float time, float alpha) {
    int k = key_index(tl, time);
    if (tl.type <= SPT_INHERIT) {
        if (tl.target < 0 || tl.target >= (int)skel.bones.size()) return;
        SpBone& b = skel.bones[tl.target];
        if (!b.active) return;
        const SpBoneData& d = *b.data;
        if (tl.type == SPT_INHERIT) {
            b.inherit = k < 0 ? d.inherit : tl.ints[k];
            return;
        }
        float v0 = k < 0 ? 0 : curve_value(tl, k, 0, time);
        float v1 = k < 0 || tl.values < 2 ? 0 : curve_value(tl, k, 1, time);
        switch (tl.type) {
        case SPT_ROTATE: b.rotation = mixf(b.rotation, d.rotation + v0, alpha); break;
        case SPT_TRANSLATE:
            b.x = mixf(b.x, d.x + v0, alpha);
            b.y = mixf(b.y, d.y + v1, alpha);
            break;
        case SPT_TRANSLATE_X: b.x = mixf(b.x, d.x + v0, alpha); break;
        case SPT_TRANSLATE_Y: b.y = mixf(b.y, d.y + v0, alpha); break;
        case SPT_SCALE:
            b.scale_x = mixf(b.scale_x, k < 0 ? d.scale_x : d.scale_x * v0, alpha);
            b.scale_y = mixf(b.scale_y, k < 0 ? d.scale_y : d.scale_y * v1, alpha);
            break;
        case SPT_SCALE_X: b.scale_x = mixf(b.scale_x, k < 0 ? d.scale_x : d.scale_x * v0, alpha); break;
        case SPT_SCALE_Y: b.scale_y = mixf(b.scale_y, k < 0 ? d.scale_y : d.scale_y * v0, alpha); break;
        case SPT_SHEAR:
            b.shear_x = mixf(b.shear_x, d.shear_x + v0, alpha);
            b.shear_y = mixf(b.shear_y, d.shear_y + v1, alpha);
            break;
        case SPT_SHEAR_X: b.shear_x = mixf(b.shear_x, d.shear_x + v0, alpha); break;
        case SPT_SHEAR_Y: b.shear_y = mixf(b.shear_y, d.shear_y + v0, alpha); break;
        default: break;
        }
        return;
    }
    if (tl.type == SPT_DRAW_ORDER) {
        if (alpha < 0.5f) return;
        if (k < 0 || tl.orders[k].size() != skel.draw_order.size()) {
            for (size_t i = 0; i < skel.draw_order.size(); ++i) skel.draw_order[i] = (int)i;
        } else {
            skel.draw_order = tl.orders[k];
        }
        return;
    }
    if (tl.target < 0 || tl.target >= (int)skel.slots.size()) return;
    SpSlot& s = skel.slots[tl.target];
    if (!skel.bones[s.data->bone].active) return;
    switch (tl.type) {
    case SPT_ATTACHMENT:
        if (alpha < 0.5f) return; /* attachments switch over at the middle of a crossfade */
        set_attachment(skel, tl.target, k < 0 ? s.data->attachment : tl.names[k]);
        return;
    case SPT_RGBA:
    case SPT_RGBA2: {
        const Color& c0 = s.data->color;
        Color c = k < 0 ? c0 : Color(curve_value(tl, k, 0, time), curve_value(tl, k, 1, time),
                                     curve_value(tl, k, 2, time), curve_value(tl, k, 3, time));
        s.color = Color(mixf(s.color.r, c.r, alpha), mixf(s.color.g, c.g, alpha), mixf(s.color.b, c.b, alpha),
                        mixf(s.color.a, c.a, alpha));
        return;
    }
    case SPT_RGB:
    case SPT_RGB2: {
        const Color& c0 = s.data->color;
        float r = k < 0 ? c0.r : curve_value(tl, k, 0, time), g = k < 0 ? c0.g : curve_value(tl, k, 1, time);
        float bl = k < 0 ? c0.b : curve_value(tl, k, 2, time);
        s.color.r = mixf(s.color.r, r, alpha);
        s.color.g = mixf(s.color.g, g, alpha);
        s.color.b = mixf(s.color.b, bl, alpha);
        return;
    }
    case SPT_ALPHA:
        s.color.a = mixf(s.color.a, k < 0 ? s.data->color.a : curve_value(tl, k, 0, time), alpha);
        return;
    case SPT_DEFORM: {
        SpAttachment* a = s.attachment;
        if (!a || a->timeline_key != tl.attachment || (a->type != SP_ATT_MESH && a->type != SP_ATT_BOUNDING_BOX &&
                                                       a->type != SP_ATT_PATH && a->type != SP_ATT_CLIPPING))
            return;
        size_t n = (size_t)a->deform_length();
        if (k < 0) {
            if (alpha >= 1.0f) s.deform.clear();
            return;
        }
        const std::vector<float>& f0 = tl.deforms[k];
        if (f0.size() != n) return;
        if (s.deform.size() != n) {
            if (a->weighted()) s.deform.assign(n, 0.0f);
            else s.deform = a->vertices;
        }
        if (k + 1 >= (int)tl.deforms.size()) {
            for (size_t i = 0; i < n; ++i) s.deform[i] = mixf(s.deform[i], f0[i], alpha);
            return;
        }
        const std::vector<float>& f1 = tl.deforms[k + 1];
        float pct = curve_percent(tl, k, time);
        for (size_t i = 0; i < n; ++i) s.deform[i] = mixf(s.deform[i], f0[i] + (f1[i] - f0[i]) * pct, alpha);
        return;
    }
    default: return;
    }
}

void sp_apply_animation(const SpAnimation& anim, SpSkeleton& skel, float time, bool loop, float alpha) {
    if (loop && anim.duration > 0) {
        time = fmodf(time, anim.duration);
        if (time < 0) time += anim.duration;
    } else if (time > anim.duration) {
        time = anim.duration;
    }
    for (size_t i = 0; i < anim.timelines.size(); ++i) apply_timeline(anim.timelines[i], skel, time, alpha);
}

/* ---- animation state ---- */

SpAnimationState::SpAnimationState() : default_mix(0), time_scale(1) {}

SpAnimationState::Track& SpAnimationState::track(int i) {
    if (i < 0) i = 0;
    if ((size_t)i >= tracks_.size()) tracks_.resize(i + 1);
    return tracks_[i];
}

void SpAnimationState::start(Track& t, const Entry& e) {
    if (t.has_cur && t.cur.anim && default_mix > 0) {
        t.from = t.cur;
        t.has_from = true;
    } else {
        t.has_from = false;
    }
    t.cur = e;
    t.cur.mix_time = 0;
    t.cur.mix_duration = t.has_from ? default_mix : 0;
    t.has_cur = true;
}

void SpAnimationState::set_animation(int ti, const SpAnimation* anim, bool loop) {
    Track& t = track(ti);
    t.queue.clear();
    Entry e;
    e.anim = anim;
    e.loop = loop;
    start(t, e);
}

void SpAnimationState::add_animation(int ti, const SpAnimation* anim, bool loop, float delay) {
    Track& t = track(ti);
    Entry e;
    e.anim = anim;
    e.loop = loop;
    if (!t.has_cur) {
        e.delay = delay;
        start(t, e);
        return;
    }
    /* Delay <= 0: from the end of the entry before it (less the crossfade). */
    const Entry& prev = t.queue.empty() ? t.cur : t.queue.back();
    if (delay <= 0) {
        float d = prev.anim ? prev.anim->duration : 0;
        delay = maxf(0.0f, d - default_mix + delay);
    }
    e.delay = delay;
    t.queue.push_back(e);
}

void SpAnimationState::update(float dt) {
    dt *= time_scale;
    for (size_t i = 0; i < tracks_.size(); ++i) {
        Track& t = tracks_[i];
        if (!t.has_cur) continue;
        if (t.cur.delay > 0) {
            t.cur.delay -= dt;
            if (t.cur.delay > 0) continue;
            dt = -t.cur.delay;
            t.cur.delay = 0;
        }
        t.cur.time += dt * t.cur.time_scale;
        if (t.has_from) {
            t.from.time += dt * t.from.time_scale;
            t.cur.mix_time += dt;
            if (t.cur.mix_time >= t.cur.mix_duration) t.has_from = false;
        }
        if (!t.queue.empty() && t.cur.time >= t.queue[0].delay) {
            Entry next = t.queue[0];
            t.queue.erase(t.queue.begin());
            next.time = t.cur.time - next.delay;
            next.delay = 0;
            start(t, next);
        }
    }
}

void SpAnimationState::apply(SpSkeleton& skel) {
    for (size_t i = 0; i < tracks_.size(); ++i) {
        Track& t = tracks_[i];
        if (!t.has_cur || t.cur.delay > 0 || !t.cur.anim) continue;
        if (t.has_from && t.from.anim && t.cur.mix_duration > 0) {
            sp_apply_animation(*t.from.anim, skel, t.from.time, t.from.loop, 1.0f);
            sp_apply_animation(*t.cur.anim, skel, t.cur.time, t.cur.loop, t.cur.mix_time / t.cur.mix_duration);
        } else {
            sp_apply_animation(*t.cur.anim, skel, t.cur.time, t.cur.loop, 1.0f);
        }
    }
}

const SpAnimation* SpAnimationState::current(int ti) const {
    if (ti < 0 || (size_t)ti >= tracks_.size() || !tracks_[ti].has_cur) return NULL;
    return tracks_[ti].cur.anim;
}

} /* namespace spire */
