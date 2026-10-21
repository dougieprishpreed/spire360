#include "node3d.h"
#include "canvas.h"
#include "resource.h"
#include "scene.h"
#include "shaders.h"
#include "value.h"

#include <algorithm>
#include <map>
#include <math.h>

namespace spire {

void packed_bytes(const Json& v, std::vector<u8>& out); /* controls.cpp */

/* ---- Xform3D ---- */

Xform3D::Xform3D() {
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) m[i][j] = i == j ? 1.0f : 0.0f;
        o[i] = 0;
    }
}

void Xform3D::apply(const float in[3], float out[3]) const {
    float r[3];
    for (int i = 0; i < 3; ++i) r[i] = m[i][0] * in[0] + m[i][1] * in[1] + m[i][2] * in[2] + o[i];
    out[0] = r[0];
    out[1] = r[1];
    out[2] = r[2];
}

Xform3D Xform3D::operator*(const Xform3D& b) const {
    Xform3D r;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) r.m[i][j] = m[i][0] * b.m[0][j] + m[i][1] * b.m[1][j] + m[i][2] * b.m[2][j];
        r.o[i] = m[i][0] * b.o[0] + m[i][1] * b.o[1] + m[i][2] * b.o[2] + o[i];
    }
    return r;
}

Xform3D Xform3D::inverse() const {
    Xform3D r;
    float det = m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
                m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
    if (fabsf(det) < 1e-12f) return r;
    float id = 1.0f / det;
    r.m[0][0] = (m[1][1] * m[2][2] - m[1][2] * m[2][1]) * id;
    r.m[0][1] = (m[0][2] * m[2][1] - m[0][1] * m[2][2]) * id;
    r.m[0][2] = (m[0][1] * m[1][2] - m[0][2] * m[1][1]) * id;
    r.m[1][0] = (m[1][2] * m[2][0] - m[1][0] * m[2][2]) * id;
    r.m[1][1] = (m[0][0] * m[2][2] - m[0][2] * m[2][0]) * id;
    r.m[1][2] = (m[0][2] * m[1][0] - m[0][0] * m[1][2]) * id;
    r.m[2][0] = (m[1][0] * m[2][1] - m[1][1] * m[2][0]) * id;
    r.m[2][1] = (m[0][1] * m[2][0] - m[0][0] * m[2][1]) * id;
    r.m[2][2] = (m[0][0] * m[1][1] - m[0][1] * m[1][0]) * id;
    for (int i = 0; i < 3; ++i) r.o[i] = -(r.m[i][0] * o[0] + r.m[i][1] * o[1] + r.m[i][2] * o[2]);
    return r;
}

/* ---- nodes ---- */

Node3D::Node3D() : visible(true), top_level(false) {}

bool Node3D::set(const std::string& key, const Json& v, const PropCtx& ctx) {
    if (key == "transform") {
        /* Transform3D(basis rows, origin) */
        const Json& a = v["v"];
        if (a.size() >= 12) {
            for (int i = 0; i < 3; ++i)
                for (int j = 0; j < 3; ++j) transform.m[i][j] = jv_float(a[(size_t)(i * 3 + j)]);
            for (int i = 0; i < 3; ++i) transform.o[i] = jv_float(a[(size_t)(9 + i)]);
        }
    } else if (key == "visible") {
        visible = v.b(true);
    } else if (key == "top_level") {
        top_level = v.b();
    } else {
        return Node::set(key, v, ctx);
    }
    return true;
}

static Node3D* as_3d(Node* n) {
    return n && (n->type == "Node3D" || n->type == "Camera3D" || n->type == "MeshInstance3D" ||
                 n->type == "GPUParticles3D")
               ? (Node3D*)n
               : NULL;
}

Xform3D Node3D::global_transform() const {
    if (top_level) return transform;
    Node3D* p = as_3d(parent);
    return p ? p->global_transform() * transform : transform;
}

bool Node3D::visible_in_tree() const {
    for (const Node* n = this; n; n = n->parent) {
        Node3D* t = as_3d((Node*)n);
        if (!t) break;
        if (!t->visible) return false;
    }
    return true;
}

Camera3D::Camera3D() : fov(75), near_plane(0.05f), keep_aspect(1) {}

bool Camera3D::set(const std::string& key, const Json& v, const PropCtx& ctx) {
    if (key == "fov") fov = jv_float(v, 75);
    else if (key == "near") near_plane = jv_float(v, 0.05f);
    else if (key == "keep_aspect") keep_aspect = v.i(1);
    else return Node3D::set(key, v, ctx);
    return true;
}

MeshInstance3D::MeshInstance3D() : mesh(NULL), material(NULL) {}

bool MeshInstance3D::set(const std::string& key, const Json& v, const PropCtx& ctx) {
    if (key == "mesh") {
        mesh = ctx.resource(v);
    } else if (key == "material_override") {
        Resource* r = ctx.resource(v);
        material = r && r->kind == Resource::RES_SHADER_MATERIAL ? (MaterialRes*)r : NULL;
    } else {
        return Node3D::set(key, v, ctx);
    }
    return true;
}

/* ---- ArrayMesh surfaces (RenderingServer's format version 2) ---- */

namespace {

struct MeshData {
    std::vector<float> pos; /* x, y, z per vertex */
    std::vector<Vec2> uv;
    std::vector<int> idx;
};

const u64 F_VERTEX = 1, F_NORMAL = 2, F_TANGENT = 4, F_COLOR = 8, F_UV = 16, F_UV2 = 32, F_INDEX = 1u << 12;
const u64 F_2D = 1u << 25, F_COMPRESS = 1u << 29, F_V2 = (u64)1 << 35;

u16 read_u16(const u8* p) { return (u16)(p[0] | (p[1] << 8)); }

float read_f32(const u8* p) {
    union {
        u32 u;
        float f;
    } x;
    x.u = (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
    return x.f;
}

/* The first surface's triangles. Compressed (ARRAY_FLAG_COMPRESS_ATTRIBUTES):
 * positions unorm16 in the AABB (x, y, z, then the tangent's angle), the
 * octahedral normal 2 x unorm16, uvs unorm16 over [-uv_scale, uv_scale]
 * (or [0, 1] when the scale is 0). */
bool decode(const Resource* mesh, MeshData& out) {
    if (!mesh || !mesh->props || mesh->type != "ArrayMesh") return false;
    const Json& surfaces = (*mesh->props)["_surfaces"];
    if (surfaces.size() == 0) return false;
    const Json& s = surfaces[(size_t)0];
    u64 format = (u64)s["format"].num();
    int count = s["vertex_count"].i();
    if (!(format & F_VERTEX) || !(format & F_V2) || count <= 0 || s["primitive"].i(3) != 3) {
        log_warn("MeshInstance3D: surface format %llx not ported", (unsigned long long)format);
        return false;
    }
    bool packed = (format & F_COMPRESS) != 0;
    std::vector<u8> vd, ad, id;
    packed_bytes(s["vertex_data"], vd);
    packed_bytes(s["attribute_data"], ad);
    int pos_size = packed ? 8 : ((format & F_2D) ? 8 : 12);
    int vstride = pos_size + ((format & F_NORMAL) ? 4 : 0) + ((format & F_TANGENT) && !packed ? 4 : 0);
    int uv_size = packed ? 4 : 8;
    int astride = ((format & F_COLOR) ? 4 : 0) + ((format & F_UV) ? uv_size : 0) + ((format & F_UV2) ? uv_size : 0);
    if ((int)vd.size() < vstride * count || (int)ad.size() < astride * count) {
        log_warn("MeshInstance3D: short surface data");
        return false;
    }
    float box[6] = { 0, 0, 0, 1, 1, 1 }, scale[2] = { 0, 0 };
    const Json& aabb = s["aabb"]["v"];
    for (size_t i = 0; i < 6 && i < aabb.size(); ++i) box[i] = jv_float(aabb[i]);
    const Json& us = s["uv_scale"]["v"];
    for (size_t i = 0; i < 2 && i < us.size(); ++i) scale[i] = jv_float(us[i]);
    for (int i = 0; i < count; ++i) {
        const u8* p = &vd[(size_t)i * vstride];
        for (int k = 0; k < 3; ++k) {
            float x;
            if (packed) x = box[k] + read_u16(p + k * 2) / 65535.0f * box[3 + k];
            else x = (format & F_2D) && k == 2 ? 0.0f : read_f32(p + k * 4);
            out.pos.push_back(x);
        }
        Vec2 uv(0, 0);
        if ((format & F_UV) && astride) {
            const u8* a = &ad[(size_t)i * astride] + ((format & F_COLOR) ? 4 : 0);
            if (packed) {
                float u = read_u16(a) / 65535.0f, w = read_u16(a + 2) / 65535.0f;
                uv = Vec2(scale[0] != 0 ? (u * 2 - 1) * scale[0] : u, scale[1] != 0 ? (w * 2 - 1) * scale[1] : w);
            } else {
                uv = Vec2(read_f32(a), read_f32(a + 4));
            }
        }
        out.uv.push_back(uv);
    }
    if (format & F_INDEX) {
        packed_bytes(s["index_data"], id);
        bool wide = count > 65535;
        size_t n = wide ? id.size() / 4 : id.size() / 2;
        for (size_t i = 0; i < n; ++i) {
            int k = wide ? (int)(id[i * 4] | (id[i * 4 + 1] << 8) | (id[i * 4 + 2] << 16) | ((u32)id[i * 4 + 3] << 24))
                         : (int)read_u16(&id[i * 2]);
            out.idx.push_back(k < count ? k : 0);
        }
    } else {
        for (int i = 0; i < count; ++i) out.idx.push_back(i);
    }
    return true;
}

const MeshData* mesh_data(const Resource* mesh) {
    static std::map<const Resource*, MeshData> cache;
    std::map<const Resource*, MeshData>::iterator it = cache.find(mesh);
    if (it != cache.end()) return it->second.idx.empty() ? NULL : &it->second;
    MeshData& d = cache[mesh];
    if (!decode(mesh, d)) d = MeshData();
    return d.idx.empty() ? NULL : &d;
}

void find_3d(Node* n, Camera3D** cam, std::vector<MeshInstance3D*>& meshes) {
    if (n->type == "Camera3D" && !*cam && ((Camera3D*)n)->visible_in_tree()) *cam = (Camera3D*)n;
    if (n->type == "MeshInstance3D" && ((MeshInstance3D*)n)->visible_in_tree()) meshes.push_back((MeshInstance3D*)n);
    for (size_t i = 0; i < n->children.size(); ++i) find_3d(n->children[i], cam, meshes);
}

struct ByDepth {
    const std::vector<float>* depth;
    bool operator()(int a, int b) const { return (*depth)[(size_t)a] < (*depth)[(size_t)b]; }
};

} /* namespace */

bool draw_3d(Canvas& c, Node* viewport, const Vec2& size, const Xform2D& to_pixels) {
    Camera3D* cam = NULL;
    std::vector<MeshInstance3D*> meshes;
    for (size_t i = 0; i < viewport->children.size(); ++i) find_3d(viewport->children[i], &cam, meshes);
    if (!cam || size.x <= 0 || size.y <= 0) return false;
    Xform3D view = cam->global_transform().inverse();
    /* Camera3D's perspective: fov is the kept axis' (height by default) */
    float f = 1.0f / tanf(cam->fov * 0.5f * 0.017453293f), aspect = size.x / size.y;
    float fx = cam->keep_aspect == 1 ? f / aspect : f, fy = cam->keep_aspect == 1 ? f : f * aspect;
    /* the transparent pass: far to near by origin */
    std::vector<float> depth;
    std::vector<int> order;
    for (size_t i = 0; i < meshes.size(); ++i) {
        float at[3];
        Xform3D g = view * meshes[i]->global_transform();
        at[0] = g.o[0];
        at[1] = g.o[1];
        at[2] = g.o[2];
        depth.push_back(at[2]);
        order.push_back((int)i);
    }
    ByDepth by;
    by.depth = &depth;
    std::stable_sort(order.begin(), order.end(), by);
    c.set_clip(NULL);
    for (size_t k = 0; k < order.size(); ++k) {
        MeshInstance3D* mi = meshes[(size_t)order[k]];
        const MeshData* d = mesh_data(mi->mesh);
        if (!d) continue;
        Xform3D g = view * mi->global_transform();
        size_t n = d->uv.size();
        std::vector<Vec2> v(n), uv(d->uv);
        std::vector<bool> front(n);
        for (size_t i = 0; i < n; ++i) {
            float p[3];
            g.apply(&d->pos[i * 3], p);
            front[i] = p[2] < -cam->near_plane;
            float w = front[i] ? -p[2] : 1.0f;
            v[i] = Vec2((p[0] * fx / w * 0.5f + 0.5f) * size.x, (0.5f - p[1] * fy / w * 0.5f) * size.y);
        }
        std::vector<int> idx;
        for (size_t t = 0; t + 2 < d->idx.size(); t += 3)
            if (front[(size_t)d->idx[t]] && front[(size_t)d->idx[t + 1]] && front[(size_t)d->idx[t + 2]]) {
                idx.push_back(d->idx[t]);
                idx.push_back(d->idx[t + 1]);
                idx.push_back(d->idx[t + 2]);
            }
        if (idx.empty()) continue;
        BlendMode blend = BLEND_MIX;
        if (mi->material) shader_blend(mi->material, &blend);
        c.set_item(to_pixels, Color(1, 1, 1, 1), blend, 0);
        std::vector<Color> colors(n, Color(1, 1, 1, 1));
        if (mi->material) {
            if (shader_begin(c, mi->material, NULL)) {
                c.draw_triangles(NULL, v, uv, colors, idx);
                shader_end(c, mi->material);
            }
        } else {
            c.draw_triangles(NULL, v, uv, colors, idx);
        }
    }
    return true;
}

} /* namespace spire */
