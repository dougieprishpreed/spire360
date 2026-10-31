#include "image.h"
#include "../core/pack.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

namespace spire {

static void rgb565(u16 c, int out[3]) {
    int r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
    out[0] = (r << 3) | (r >> 2);
    out[1] = (g << 2) | (g >> 4);
    out[2] = (b << 3) | (b >> 2);
}

/* One BC1 colour block into 16 RGBA pixels (straight alpha). */
static void bc1_colors(const u8* b, bool four_color, u8 out[16][4]) {
    u16 c0 = (u16)(b[0] | (b[1] << 8)), c1 = (u16)(b[2] | (b[3] << 8));
    int p[4][4];
    rgb565(c0, p[0]);
    rgb565(c1, p[1]);
    p[0][3] = p[1][3] = 255;
    if (four_color || c0 > c1) {
        for (int k = 0; k < 3; ++k) {
            p[2][k] = (2 * p[0][k] + p[1][k] + 1) / 3;
            p[3][k] = (p[0][k] + 2 * p[1][k] + 1) / 3;
        }
        p[2][3] = p[3][3] = 255;
    } else {
        for (int k = 0; k < 3; ++k) {
            p[2][k] = (p[0][k] + p[1][k]) / 2;
            p[3][k] = 0;
        }
        p[2][3] = 255;
        p[3][3] = 0;
    }
    u32 bits = (u32)b[4] | ((u32)b[5] << 8) | ((u32)b[6] << 16) | ((u32)b[7] << 24);
    for (int i = 0; i < 16; ++i) {
        int idx = (bits >> (2 * i)) & 3;
        for (int k = 0; k < 4; ++k) out[i][k] = (u8)p[idx][k];
    }
}

static void bc3_alpha(const u8* b, u8 out[16][4]) {
    int a[8];
    a[0] = b[0];
    a[1] = b[1];
    if (a[0] > a[1]) {
        for (int k = 1; k < 7; ++k) a[k + 1] = ((7 - k) * a[0] + k * a[1] + 3) / 7;
    } else {
        for (int k = 1; k < 5; ++k) a[k + 1] = ((5 - k) * a[0] + k * a[1] + 2) / 5;
        a[6] = 0;
        a[7] = 255;
    }
    u64 bits = 0;
    for (int k = 0; k < 6; ++k) bits |= (u64)b[2 + k] << (8 * k);
    for (int i = 0; i < 16; ++i) out[i][3] = (u8)a[(bits >> (3 * i)) & 7];
}

static void put_premul(Image& img, int x, int y, const u8* c, bool premul) {
    if (x >= img.w || y >= img.h) return;
    u8* d = img.at(x, y);
    int a = c[3];
    if (!premul) {
        d[0] = c[0];
        d[1] = c[1];
        d[2] = c[2];
        d[3] = c[3];
        return;
    }
    d[0] = (u8)((c[0] * a + 127) / 255);
    d[1] = (u8)((c[1] * a + 127) / 255);
    d[2] = (u8)((c[2] * a + 127) / 255);
    d[3] = (u8)a;
}

bool decode_texture_level(int format, int w, int h, const u8* data, size_t size, Image& out, bool premul) {
    out.resize(w, h);
    if (format == TEX_ARGB8) {
        if (size < (size_t)w * h * 4) return false;
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                const u8* s = data + ((size_t)y * w + x) * 4; /* B G R A */
                u8 c[4] = { s[2], s[1], s[0], s[3] };
                put_premul(out, x, y, c, premul);
            }
        return true;
    }
    int bw = (w + 3) / 4, bh = (h + 3) / 4;
    size_t bsize = format == TEX_DXT1 ? 8 : 16;
    if (size < (size_t)bw * bh * bsize) return false;
    u8 px[16][4];
    for (int by = 0; by < bh; ++by)
        for (int bx = 0; bx < bw; ++bx) {
            const u8* b = data + ((size_t)by * bw + bx) * bsize;
            if (format == TEX_DXT1) {
                bc1_colors(b, false, px);
            } else {
                bc1_colors(b + 8, true, px);
                bc3_alpha(b, px);
            }
            for (int i = 0; i < 16; ++i) put_premul(out, bx * 4 + (i & 3), by * 4 + (i >> 2), px[i], premul);
        }
    return true;
}

bool bright_boxes(int format, int w, int h, const u8* data, size_t size, std::vector<u16>& out) {
    int cell = format == TEX_ARGB8 ? 1 : 4;
    int cw = (w + cell - 1) / cell, ch = (h + cell - 1) / cell;
    size_t bsize = format == TEX_ARGB8 ? 4 : format == TEX_DXT1 ? 8 : format == TEX_DXT5 ? 16 : 0;
    if (!bsize || w <= 0 || h <= 0 || size < (size_t)cw * ch * bsize) return false;
    int box[BRIGHT_LEVELS][4];
    for (int k = 0; k < BRIGHT_LEVELS; ++k) {
        box[k][0] = cw;
        box[k][1] = ch;
        box[k][2] = box[k][3] = 0;
    }
    for (int y = 0; y < ch; ++y)
        for (int x = 0; x < cw; ++x) {
            const u8* b = data + ((size_t)y * cw + x) * bsize;
            int blue;
            if (format == TEX_ARGB8) {
                blue = b[0]; /* B G R A */
            } else {
                if (format == TEX_DXT5) b += 8;
                int b0 = b[0] & 31, b1 = b[2] & 31;
                int top = b0 > b1 ? b0 : b1;
                blue = (top << 3) | (top >> 2);
            }
            /* the levels it passes: blue > 8k */
            for (int k = 0; k < BRIGHT_LEVELS && blue > k * 8; ++k) {
                if (x < box[k][0]) box[k][0] = x;
                if (y < box[k][1]) box[k][1] = y;
                if (x + 1 > box[k][2]) box[k][2] = x + 1;
                if (y + 1 > box[k][3]) box[k][3] = y + 1;
            }
        }
    out.resize(BRIGHT_LEVELS * 4);
    for (int k = 0; k < BRIGHT_LEVELS; ++k) {
        bool none = box[k][2] == 0;
        out[k * 4] = (u16)(none ? 0 : box[k][0] * cell);
        out[k * 4 + 1] = (u16)(none ? 0 : box[k][1] * cell);
        out[k * 4 + 2] = (u16)(none ? 0 : (box[k][2] * cell < w ? box[k][2] * cell : w));
        out[k * 4 + 3] = (u16)(none ? 0 : (box[k][3] * cell < h ? box[k][3] * cell : h));
    }
    return true;
}

bool alpha_cover(int format, int w, int h, const u8* data, size_t size, std::vector<u8>& out, int* cols, int* rows) {
    if ((format != TEX_DXT5 && format != TEX_ARGB8) || w <= 0 || h <= 0) return false;
    int bw = (w + 3) / 4, bh = (h + 3) / 4;
    if (size < (format == TEX_DXT5 ? (size_t)bw * bh * 16 : (size_t)w * h * 4)) return false;
    int per = COVER_CELL / 4; /* blocks a cell */
    int cw = (bw + per - 1) / per, ch = (bh + per - 1) / per;
    out.assign((size_t)cw * ch, 0);
    for (int by = 0; by < bh; ++by)
        for (int bx = 0; bx < bw; ++bx) {
            bool any = false;
            if (format == TEX_DXT5) {
                const u8* b = data + ((size_t)by * bw + bx) * 16;
                u64 bits = 0;
                for (int k = 0; k < 6; ++k) bits |= (u64)b[2 + k] << (8 * k);
                /* an index of 7 in a 3-bit field: 255 in six-alpha mode */
                any = b[0] || b[1] || (bits & (bits >> 1) & (bits >> 2) & 0x249249249249ULL);
            } else {
                for (int y = by * 4; y < by * 4 + 4 && y < h && !any; ++y)
                    for (int x = bx * 4; x < bx * 4 + 4 && x < w && !any; ++x)
                        any = data[((size_t)y * w + x) * 4 + 3] != 0;
            }
            if (!any) continue;
            /* the cells of the block and of the blocks beside it */
            for (int y = by - 1; y <= by + 1; ++y)
                for (int x = bx - 1; x <= bx + 1; ++x)
                    if (x >= 0 && y >= 0 && x < bw && y < bh) out[(size_t)(y / per) * cw + x / per] = 1;
        }
    size_t drawn = 0;
    for (size_t i = 0; i < out.size(); ++i) drawn += out[i];
    if (drawn * 6 > out.size() * 5) {
        out.clear();
        return false;
    }
    *cols = cw;
    *rows = ch;
    return true;
}

/* ---- PNG writer (stored deflate blocks: no compression library needed) ---- */

static u32 crc_table[256];
static bool crc_ready = false;

void image_bilinear(const Image& img, float u, float v, float out[4]) {
    if (img.w <= 0 || img.h <= 0) {
        out[0] = out[1] = out[2] = out[3] = 0;
        return;
    }
    float x = u * img.w - 0.5f, y = v * img.h - 0.5f;
    int x0 = (int)floorf(x), y0 = (int)floorf(y);
    float fx = x - x0, fy = y - y0;
    int x1 = x0 + 1, y1 = y0 + 1;
    x0 = x0 < 0 ? 0 : (x0 >= img.w ? img.w - 1 : x0);
    x1 = x1 < 0 ? 0 : (x1 >= img.w ? img.w - 1 : x1);
    y0 = y0 < 0 ? 0 : (y0 >= img.h ? img.h - 1 : y0);
    y1 = y1 < 0 ? 0 : (y1 >= img.h ? img.h - 1 : y1);
    const u8* a = img.at(x0, y0);
    const u8* b = img.at(x1, y0);
    const u8* c = img.at(x0, y1);
    const u8* d = img.at(x1, y1);
    for (int k = 0; k < 4; ++k) {
        float top = a[k] + (b[k] - a[k]) * fx;
        float bot = c[k] + (d[k] - c[k]) * fx;
        out[k] = (top + (bot - top) * fy) * (1.0f / 255.0f);
    }
}

void blur_mip_level(const Image& src, Image& dst) {
    static const float k[4] = { 0.214607f, 0.189879f, 0.131514f, 0.071303f };
    int w = src.w > 1 ? src.w >> 1 : 1, h = src.h > 1 ? src.h >> 1 : 1;
    std::vector<float> a((size_t)w * h * 4), b((size_t)w * h * 4);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) image_bilinear(src, (x + 0.5f) / w, (y + 0.5f) / h, &a[((size_t)y * w + x) * 4]);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            for (int c = 0; c < 4; ++c) {
                float sum = a[((size_t)y * w + x) * 4 + c] * k[0];
                for (int t = 1; t < 4; ++t) {
                    int l = x - t < 0 ? 0 : x - t, r = x + t >= w ? w - 1 : x + t;
                    sum += (a[((size_t)y * w + l) * 4 + c] + a[((size_t)y * w + r) * 4 + c]) * k[t];
                }
                b[((size_t)y * w + x) * 4 + c] = sum;
            }
    dst.resize(w, h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            for (int c = 0; c < 4; ++c) {
                float sum = b[((size_t)y * w + x) * 4 + c] * k[0];
                for (int t = 1; t < 4; ++t) {
                    int up = y - t < 0 ? 0 : y - t, dn = y + t >= h ? h - 1 : y + t;
                    sum += (b[((size_t)up * w + x) * 4 + c] + b[((size_t)dn * w + x) * 4 + c]) * k[t];
                }
                sum = sum < 0 ? 0 : (sum > 1 ? 1 : sum);
                dst.at(x, y)[c] = (u8)(sum * 255 + 0.5f);
            }
}

void image_lod(const Image* levels, int count, float u, float v, float lod, float out[4]) {
    float top = (float)(count - 1);
    if (lod > top) lod = top;
    if (lod < 0) lod = 0;
    int l0 = (int)floorf(lod);
    float f = lod - l0;
    image_bilinear(levels[l0], u, v, out);
    if (f > 0 && l0 + 1 < count) {
        float o[4];
        image_bilinear(levels[l0 + 1], u, v, o);
        for (int c = 0; c < 4; ++c) out[c] += (o[c] - out[c]) * f;
    }
}

static u32 crc32_update(u32 c, const u8* p, size_t n) {
    if (!crc_ready) {
        for (u32 i = 0; i < 256; ++i) {
            u32 v = i;
            for (int k = 0; k < 8; ++k) v = (v & 1) ? 0xEDB88320u ^ (v >> 1) : v >> 1;
            crc_table[i] = v;
        }
        crc_ready = true;
    }
    c ^= 0xFFFFFFFFu;
    for (size_t i = 0; i < n; ++i) c = crc_table[(c ^ p[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

static void put32(std::vector<u8>& v, u32 x) {
    v.push_back((u8)(x >> 24));
    v.push_back((u8)(x >> 16));
    v.push_back((u8)(x >> 8));
    v.push_back((u8)x);
}

static void chunk(FILE* f, const char* type, const std::vector<u8>& data) {
    std::vector<u8> head;
    put32(head, (u32)data.size());
    fwrite(&head[0], 1, 4, f);
    std::vector<u8> body(type, type + 4);
    body.insert(body.end(), data.begin(), data.end());
    fwrite(&body[0], 1, body.size(), f);
    std::vector<u8> crc;
    put32(crc, crc32_update(0, &body[0], body.size()));
    fwrite(&crc[0], 1, 4, f);
}

bool write_png(const char* path, const Image& img) {
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    static const u8 sig[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
    fwrite(sig, 1, 8, f);
    std::vector<u8> ihdr;
    put32(ihdr, (u32)img.w);
    put32(ihdr, (u32)img.h);
    ihdr.push_back(8);  /* bit depth */
    ihdr.push_back(6);  /* RGBA */
    ihdr.push_back(0);
    ihdr.push_back(0);
    ihdr.push_back(0);
    chunk(f, "IHDR", ihdr);
    /* raw scanlines, filter 0, straight alpha */
    std::vector<u8> raw;
    raw.reserve((size_t)img.h * (img.w * 4 + 1));
    for (int y = 0; y < img.h; ++y) {
        raw.push_back(0);
        for (int x = 0; x < img.w; ++x) {
            const u8* p = img.at(x, y);
            int a = p[3];
            for (int k = 0; k < 3; ++k) raw.push_back((u8)(a ? (p[k] * 255 + a / 2) / a : 0));
            raw.push_back((u8)a);
        }
    }
    std::vector<u8> z;
    z.push_back(0x78);
    z.push_back(0x01);
    u32 s1 = 1, s2 = 0;
    for (size_t i = 0; i < raw.size(); ++i) {
        s1 = (s1 + raw[i]) % 65521;
        s2 = (s2 + s1) % 65521;
    }
    size_t pos = 0;
    while (pos < raw.size() || raw.empty()) {
        size_t n = raw.size() - pos;
        if (n > 65535) n = 65535;
        bool last = pos + n >= raw.size();
        z.push_back(last ? 1 : 0);
        z.push_back((u8)(n & 0xFF));
        z.push_back((u8)(n >> 8));
        z.push_back((u8)(~n & 0xFF));
        z.push_back((u8)((~n >> 8) & 0xFF));
        z.insert(z.end(), raw.begin() + pos, raw.begin() + pos + n);
        pos += n;
        if (raw.empty()) break;
    }
    put32(z, (s2 << 16) | s1);
    chunk(f, "IDAT", z);
    chunk(f, "IEND", std::vector<u8>());
    fclose(f);
    return true;
}

} /* namespace spire */
