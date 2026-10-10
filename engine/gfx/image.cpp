#include "image.h"
#include "../core/pack.h"

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

/* ---- PNG writer (stored deflate blocks: no compression library needed) ---- */

static u32 crc_table[256];
static bool crc_ready = false;

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
