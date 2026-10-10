/* Fonts as the converter exports them (converter/spire_convert/fonts.py):
 * MSDF glyph pages rendered by Godot, metrics, and the kerning and
 * ligatures Godot's shaper applies. All metrics are at base_size; Godot
 * scales MSDF fonts linearly (text_server_adv.cpp). */
#ifndef SPIRE_FONT_H
#define SPIRE_FONT_H

#include "../core/base.h"

#include <map>

namespace spire {

class Pack;
class ImageTextureRes;
class FontRes;

struct GlyphInfo {
    u32 gid;
    int page;          /* -1: nothing to draw (space) */
    Rect2 uv;          /* in page pixels */
    Vec2 offset, size; /* at base size, from the pen position */
    float advance;     /* at base size */
};

class FontFace {
public:
    FontFace();
    ~FontFace();
    bool load(const Pack& pack, const std::string& name, std::string* error);

    std::string name;
    bool msdf;
    float px_range, base_size, ascent, descent, underline_position, underline_thickness;
    std::vector<ImageTextureRes*> pages;

    u32 glyph_for(u32 cp) const;           /* 0 if the font has no glyph */
    const GlyphInfo* glyph(u32 gid) const;
    float char_advance(u32 cp) const;      /* shaped advance at base size */
    float kerning(u32 c1, u32 c2) const;   /* at base size */
    u32 ligature(u32 c1, u32 c2) const;    /* glyph, 0 if none */

private:
    struct Pair {
        u64 key;
        float value;
        bool operator<(const Pair& o) const { return key < o.key; }
    };
    std::vector<std::pair<u32, u32> > cmap_; /* sorted by codepoint */
    std::vector<GlyphInfo> glyphs_;          /* sorted by gid */
    std::vector<Pair> advance_, kern_, lig_; /* sorted by key */
};

/* Faces by pack name, loaded on first use; the font a FontRes chain draws
 * with (FontVariation -> FontFile). */
class FontLibrary {
public:
    explicit FontLibrary(const Pack* pack);
    ~FontLibrary();
    FontFace* face(const std::string& name);
    FontFace* face_for(const FontRes* font); /* NULL if none */
    static FontLibrary* current();          /* the one the scene system uses */
    static void set_current(FontLibrary* lib);

private:
    const Pack* pack_;
    std::map<std::string, FontFace*> faces_;
};

} /* namespace spire */

#endif
