/* CPU images (premultiplied RGBA8), DXT decoding and PNG output. */
#ifndef SPIRE_IMAGE_H
#define SPIRE_IMAGE_H

#include "../core/base.h"

namespace spire {

struct Image {
    int w, h;
    std::vector<u8> px; /* RGBA, premultiplied alpha */
    Image() : w(0), h(0) {}
    void resize(int w_, int h_) { w = w_; h = h_; px.assign((size_t)w * h * 4, 0); }
    u8* at(int x, int y) { return &px[((size_t)y * w + x) * 4]; }
    const u8* at(int x, int y) const { return &px[((size_t)y * w + x) * 4]; }
};

/* Decodes one level (format TEX_DXT1/TEX_DXT5/TEX_ARGB8, PC block order)
 * into a premultiplied image of w x h. */
bool decode_texture_level(int format, int w, int h, const u8* data, size_t size, Image& out);

/* Writes an 8-bit RGBA PNG (un-premultiplies). */
bool write_png(const char* path, const Image& img);

} /* namespace spire */

#endif
