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
 * into an image of w x h, premultiplied unless asked not to (data textures). */
bool decode_texture_level(int format, int w, int h, const u8* data, size_t size, Image& out, bool premultiply = true);

/* Bilinear read at uv (0..1), edges clamped: premultiplied 0..1 floats. */
void image_bilinear(const Image& img, float u, float v, float out[4]);

/* Godot's mipmap step for a screen copy (render_target_gen_back_buffer_mipmaps):
 * half the size (at least 1), each texel the bilinear read of src at its
 * centre, then the 7-tap Gaussian of copy.glsl (MODE_GAUSSIAN_BLUR) across
 * and down, edges clamped. */
void blur_mip_level(const Image& src, Image& dst);

/* textureLod with filter_linear_mipmap over levels[0..count): bilinear in
 * the two nearest levels, blended. */
void image_lod(const Image* levels, int count, float u, float v, float lod, float out[4]);

/* Writes an 8-bit RGBA PNG (un-premultiplies). */
bool write_png(const char* path, const Image& img);

} /* namespace spire */

#endif
