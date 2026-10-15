/* Spine clipping attachments: the triangles of the slots between a clipping
 * attachment and its end slot are cut to the clip polygon. The polygon is
 * split into triangles (ear clipping) and every drawn triangle is clipped
 * against each of them (Sutherland-Hodgman); the pieces cover exactly the
 * inside of the polygon. */
#include "spine.h"

#include <math.h>

namespace spire {

static float cross(float ax, float ay, float bx, float by, float cx, float cy) {
    return (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
}

static bool point_in_tri(float px, float py, const float* t) {
    float d1 = cross(t[0], t[1], t[2], t[3], px, py), d2 = cross(t[2], t[3], t[4], t[5], px, py),
          d3 = cross(t[4], t[5], t[0], t[1], px, py);
    return d1 >= 0 && d2 >= 0 && d3 >= 0;
}

/* Ear clipping of a simple polygon given counter-clockwise (positive area
 * in the cross() sense). */
static void triangulate(const std::vector<float>& poly, std::vector<float>& out) {
    out.clear();
    int n = (int)poly.size() / 2;
    std::vector<int> idx(n);
    for (int i = 0; i < n; ++i) idx[i] = i;
    int guard = 0;
    while (idx.size() > 3 && guard++ < 10000) {
        bool cut = false;
        int m = (int)idx.size();
        for (int i = 0; i < m; ++i) {
            int i0 = idx[(i + m - 1) % m], i1 = idx[i], i2 = idx[(i + 1) % m];
            float t[6] = { poly[i0 * 2], poly[i0 * 2 + 1], poly[i1 * 2], poly[i1 * 2 + 1], poly[i2 * 2], poly[i2 * 2 + 1] };
            if (cross(t[0], t[1], t[2], t[3], t[4], t[5]) <= 0) continue; /* reflex */
            bool ear = true;
            for (int k = 0; k < m && ear; ++k) {
                int j = idx[k];
                if (j == i0 || j == i1 || j == i2) continue;
                if (point_in_tri(poly[j * 2], poly[j * 2 + 1], t)) ear = false;
            }
            if (!ear) continue;
            out.insert(out.end(), t, t + 6);
            idx.erase(idx.begin() + i);
            cut = true;
            break;
        }
        if (!cut) break; /* degenerate: keep what we have */
    }
    if (idx.size() == 3) {
        for (int k = 0; k < 3; ++k) {
            out.push_back(poly[idx[k] * 2]);
            out.push_back(poly[idx[k] * 2 + 1]);
        }
    }
}

void SpClipper::start(const SpSkeleton& skel, const SpSlot& slot, const SpAttachment& clip) {
    if (active_) return;
    int n = clip.vertex_count * 2;
    if (n < 6) return;
    std::vector<float> poly((size_t)n);
    skel.world_vertices(slot, clip, 0, n, &poly[0]);
    float area = 0;
    for (int i = 0; i < n; i += 2) {
        int j = (i + 2) % n;
        area += poly[i] * poly[j + 1] - poly[j] * poly[i + 1];
    }
    if (area < 0) { /* make counter-clockwise */
        for (int i = 0, j = n - 2; i < j; i += 2, j -= 2) {
            float tx = poly[i], ty = poly[i + 1];
            poly[i] = poly[j];
            poly[i + 1] = poly[j + 1];
            poly[j] = tx;
            poly[j + 1] = ty;
        }
    }
    triangulate(poly, triangles_);
    end_slot_ = clip.end_slot;
    active_ = !triangles_.empty();
}

void SpClipper::end_slot(int slot_index) {
    if (active_ && slot_index == end_slot_) end();
}

void SpClipper::end() {
    active_ = false;
    end_slot_ = -1;
    triangles_.clear();
}

struct ClipVert {
    float x, y, u, v;
};

void SpClipper::clip(const std::vector<float>& pos, const std::vector<float>& uv, const std::vector<int>& tris,
                     std::vector<float>& out_pos, std::vector<float>& out_uv, std::vector<int>& out_tris) {
    out_pos.clear();
    out_uv.clear();
    out_tris.clear();
    std::vector<ClipVert> a, b;
    for (size_t t = 0; t + 2 < tris.size(); t += 3) {
        for (size_t c = 0; c + 5 < triangles_.size(); c += 6) {
            const float* ct = &triangles_[c];
            a.clear();
            for (int k = 0; k < 3; ++k) {
                int i = tris[t + k];
                ClipVert v = { pos[i * 2], pos[i * 2 + 1], uv[i * 2], uv[i * 2 + 1] };
                a.push_back(v);
            }
            /* Against each edge of the (counter-clockwise) clip triangle. */
            for (int e = 0; e < 3 && !a.empty(); ++e) {
                float ex0 = ct[e * 2], ey0 = ct[e * 2 + 1], ex1 = ct[((e + 1) % 3) * 2], ey1 = ct[((e + 1) % 3) * 2 + 1];
                b.clear();
                for (size_t k = 0; k < a.size(); ++k) {
                    const ClipVert& p = a[k];
                    const ClipVert& q = a[(k + 1) % a.size()];
                    float dp = cross(ex0, ey0, ex1, ey1, p.x, p.y), dq = cross(ex0, ey0, ex1, ey1, q.x, q.y);
                    bool pin = dp >= 0, qin = dq >= 0;
                    if (pin) b.push_back(p);
                    if (pin != qin) {
                        float s = dp / (dp - dq);
                        ClipVert m = { p.x + (q.x - p.x) * s, p.y + (q.y - p.y) * s, p.u + (q.u - p.u) * s,
                                       p.v + (q.v - p.v) * s };
                        b.push_back(m);
                    }
                }
                a.swap(b);
            }
            if (a.size() < 3) continue;
            int base = (int)out_pos.size() / 2;
            for (size_t k = 0; k < a.size(); ++k) {
                out_pos.push_back(a[k].x);
                out_pos.push_back(a[k].y);
                out_uv.push_back(a[k].u);
                out_uv.push_back(a[k].v);
            }
            for (size_t k = 1; k + 1 < a.size(); ++k) {
                out_tris.push_back(base);
                out_tris.push_back(base + (int)k);
                out_tris.push_back(base + (int)k + 1);
            }
        }
    }
}

} /* namespace spire */
