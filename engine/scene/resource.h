/* Resources the scene system draws with: textures (pack images and
 * AtlasTexture), style boxes, themes, fonts, materials. Each mirrors the
 * Godot class of the same name, as far as the game uses it. */
#ifndef SPIRE_RESOURCE_H
#define SPIRE_RESOURCE_H

#include "../core/base.h"
#include "../core/json.h"

#include <map>

namespace spire {

class Canvas;
class Texture;
class ResourceLoader;

enum Side { SIDE_LEFT, SIDE_TOP, SIDE_RIGHT, SIDE_BOTTOM };

class Resource {
public:
    enum Kind {
        RES_GENERIC,
        RES_IMAGE_TEXTURE, /* an imported image in the pack (CompressedTexture2D) */
        RES_ATLAS_TEXTURE,
        RES_STYLEBOX_EMPTY,
        RES_STYLEBOX_FLAT,
        RES_STYLEBOX_TEXTURE,
        RES_THEME,
        RES_FONT,
        RES_CANVAS_ITEM_MATERIAL,
        RES_SHADER_MATERIAL,
        RES_PACKED_SCENE
    };
    Resource(Kind k, const std::string& cls) : kind(k), type(cls), props(NULL) {}
    virtual ~Resource() {}

    bool is_texture() const { return kind == RES_IMAGE_TEXTURE || kind == RES_ATLAS_TEXTURE; }
    bool is_stylebox() const { return kind == RES_STYLEBOX_EMPTY || kind == RES_STYLEBOX_FLAT || kind == RES_STYLEBOX_TEXTURE; }

    Kind kind;
    std::string type; /* Godot class */
    std::string path; /* pack name, or "scene::sub_id" */
    const Json* props; /* as stored (generic access) */
};

/* ---- textures ---- */

class Texture2DRes : public Resource {
public:
    Texture2DRes(Kind k, const std::string& cls) : Resource(k, cls) {}
    virtual Vec2 size() const = 0;
    /* Maps drawing src (in this texture's pixels; empty = whole) to dst onto
     * the image that holds the pixels (Godot's get_rect_region). */
    virtual bool get_rect_region(const Rect2& dst, const Rect2& src, Rect2& out_dst, Rect2& out_src,
                                 class ImageTextureRes** image) = 0;
};

class ImageTextureRes : public Texture2DRes {
public:
    ImageTextureRes() : Texture2DRes(RES_IMAGE_TEXTURE, "CompressedTexture2D"), gpu(NULL), loaded(false) {}
    virtual Vec2 size() const { return size_; }
    virtual bool get_rect_region(const Rect2& dst, const Rect2& src, Rect2& out_dst, Rect2& out_src,
                                 ImageTextureRes** image);
    Vec2 size_;     /* layout units (the game's size) */
    Texture* gpu;   /* loaded by the canvas on first draw */
    bool loaded;
};

class AtlasTextureRes : public Texture2DRes {
public:
    AtlasTextureRes() : Texture2DRes(RES_ATLAS_TEXTURE, "AtlasTexture"), atlas(NULL), filter_clip(false) {}
    virtual Vec2 size() const;
    virtual bool get_rect_region(const Rect2& dst, const Rect2& src, Rect2& out_dst, Rect2& out_src,
                                 ImageTextureRes** image);
    Texture2DRes* atlas;
    Rect2 region, margin;
    bool filter_clip;
private:
    Rect2 region_rect() const;
};

/* ---- style boxes ---- */

class StyleBoxRes : public Resource {
public:
    StyleBoxRes(Kind k, const std::string& cls) : Resource(k, cls) {
        for (int i = 0; i < 4; ++i) content_margin[i] = -1.0f;
    }
    float margin(int side) const { return content_margin[side] < 0 ? style_margin(side) : content_margin[side]; }
    Vec2 minimum_size() const { return Vec2(margin(SIDE_LEFT) + margin(SIDE_RIGHT), margin(SIDE_TOP) + margin(SIDE_BOTTOM)); }
    Vec2 offset() const { return Vec2(margin(SIDE_LEFT), margin(SIDE_TOP)); }
    virtual float style_margin(int) const { return 0.0f; }
    virtual void draw(Canvas&, const Rect2&) const {}
    float content_margin[4];
};

class StyleBoxFlatRes : public StyleBoxRes {
public:
    StyleBoxFlatRes();
    virtual float style_margin(int side) const { return (float)border_width[side]; }
    virtual void draw(Canvas& c, const Rect2& rect) const;
    Color bg_color, border_color, shadow_color;
    int border_width[4];
    float corner_radius[4];
    float expand_margin[4];
    int corner_detail, shadow_size;
    Vec2 shadow_offset, skew;
    bool draw_center, border_blend, anti_aliased;
    float aa_size;
};

class StyleBoxLineRes : public StyleBoxRes {
public:
    StyleBoxLineRes() : StyleBoxRes(RES_STYLEBOX_EMPTY, "StyleBoxLine"), color(0, 0, 0, 1), thickness(1), grow_begin(1),
                        grow_end(1), vertical(false) {}
    virtual float style_margin(int side) const {
        if (vertical) return (side == SIDE_LEFT || side == SIDE_RIGHT) ? thickness / 2.0f : 0.0f;
        return (side == SIDE_TOP || side == SIDE_BOTTOM) ? thickness / 2.0f : 0.0f;
    }
    virtual void draw(Canvas& c, const Rect2& rect) const;
    Color color;
    int thickness;
    float grow_begin, grow_end;
    bool vertical;
};

class StyleBoxTextureRes : public StyleBoxRes {
public:
    StyleBoxTextureRes();
    virtual float style_margin(int side) const { return texture_margin[side]; }
    virtual void draw(Canvas& c, const Rect2& rect) const;
    Texture2DRes* texture;
    float texture_margin[4], expand_margin[4];
    Rect2 region_rect;
    int axis_h, axis_v;
    bool draw_center;
    Color modulate;
};

/* ---- fonts (glyphs: engine/text) ---- */

class FontRes : public Resource {
public:
    FontRes(const std::string& cls) : Resource(RES_FONT, cls), base(NULL), embolden(0), spacing_glyph(0),
        spacing_space(0), spacing_top(0), spacing_bottom(0), face(NULL) {}
    std::string file;  /* FontFile: pack name of the font data */
    FontRes* base;     /* FontVariation: base_font */
    std::vector<FontRes*> fallbacks;
    float embolden;
    int spacing_glyph, spacing_space, spacing_top, spacing_bottom;
    Xform2D variation_transform;
    void* face;        /* engine/text's face for this file, set on first use */
    const FontRes* root() const { return base ? base->root() : this; }
};

/* ---- theme ---- */

class ThemeRes : public Resource {
public:
    ThemeRes() : Resource(RES_THEME, "Theme"), default_font(NULL), default_font_size(-1), default_base_scale(0) {}
    /* key: "Type/kind/name", kind = colors|constants|fonts|font_sizes|styles|icons.
     * Plain values in items, fonts / styles / icons resolved in resources. */
    std::map<std::string, const Json*> items;
    std::map<std::string, Resource*> resources;
    std::map<std::string, std::string> variation_base; /* type -> base type */
    FontRes* default_font;
    int default_font_size;
    float default_base_scale;
};

/* ---- materials ---- */

class MaterialRes : public Resource {
public:
    MaterialRes(Kind k, const std::string& cls) : Resource(k, cls), blend_mode(0), light_mode(0) {}
    int blend_mode; /* CanvasItemMaterial::BlendMode */
    int light_mode;
    std::string shader; /* ShaderMaterial: pack name of the shader */
};

} /* namespace spire */

#endif
