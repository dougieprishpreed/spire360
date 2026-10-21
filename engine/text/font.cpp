#include "font.h"

#include "../core/json.h"
#include "../core/pack.h"
#include "../scene/resource.h"

#include <algorithm>

namespace spire {

static u64 pair_key(u32 a, u32 b) { return ((u64)a << 32) | b; }

FontFace::FontFace()
    : msdf(true), px_range(16), base_size(48), ascent(0), descent(0), underline_position(0), underline_thickness(0) {}

FontFace::~FontFace() {
    for (size_t i = 0; i < pages.size(); ++i) delete pages[i];
}

struct GlyphLess {
    bool operator()(const GlyphInfo& a, const GlyphInfo& b) const { return a.gid < b.gid; }
};

bool FontFace::load(const Pack& pack, const std::string& n, std::string* error) {
    name = n;
    const PackEntry* e = pack.find(n.c_str());
    std::vector<u8> data;
    if (!e || e->type != PACK_FONT || !pack.read(e, data) || data.empty() || data[0] != '{') {
        if (error) *error = "no exported font " + n + " (convert with Godot, see converter/spire_convert/fonts.py)";
        return false;
    }
    std::string err;
    Json j = Json::parse((const char*)&data[0], data.size(), &err);
    if (!err.empty()) {
        if (error) *error = n + ": " + err;
        return false;
    }
    msdf = j["msdf"].b(true);
    px_range = j["px_range"].f(16);
    base_size = j["base_size"].f(48);
    ascent = j["ascent"].f();
    descent = j["descent"].f();
    underline_position = j["underline_position"].f();
    underline_thickness = j["underline_thickness"].f();
    const Json& pg = j["pages"];
    for (size_t i = 0; i < pg.size(); ++i) {
        ImageTextureRes* t = new ImageTextureRes();
        t->path = pg[i].str();
        std::vector<u8> head;
        int w = 0, h = 0, ow = 0, oh = 0;
        if (pack.read_head(pack.find(t->path.c_str()), 12, head)) Pack::parse_texture_size(head, &w, &h, &ow, &oh);
        t->size_ = Vec2((float)ow, (float)oh);
        pages.push_back(t);
    }
    const Json& gl = j["glyphs"];
    glyphs_.resize(gl.size());
    for (size_t i = 0; i < gl.size(); ++i) {
        const Json& g = gl[i];
        GlyphInfo& o = glyphs_[i];
        o.gid = (u32)g[(size_t)0].num();
        o.page = g[1].i(-1);
        o.uv = Rect2(g[2].f(), g[3].f(), g[4].f(), g[5].f());
        o.offset = Vec2(g[6].f(), g[7].f());
        o.size = Vec2(g[8].f(), g[9].f());
        o.advance = g[10].f();
    }
    std::sort(glyphs_.begin(), glyphs_.end(), GlyphLess());
    const Json& cm = j["cmap"];
    cmap_.resize(cm.size());
    for (size_t i = 0; i < cm.size(); ++i) cmap_[i] = std::make_pair((u32)cm[i][(size_t)0].num(), (u32)cm[i][1].num());
    std::sort(cmap_.begin(), cmap_.end());
    const Json& ad = j["advance"];
    for (size_t i = 0; i < ad.size(); ++i) {
        Pair p;
        p.key = (u32)ad[i][(size_t)0].num();
        p.value = ad[i][1].f();
        advance_.push_back(p);
    }
    const Json& kn = j["kern"];
    for (size_t i = 0; i < kn.size(); ++i) {
        Pair p;
        p.key = pair_key((u32)kn[i][(size_t)0].num(), (u32)kn[i][1].num());
        p.value = kn[i][2].f();
        kern_.push_back(p);
    }
    const Json& lg = j["lig"];
    for (size_t i = 0; i < lg.size(); ++i) {
        Pair p;
        p.key = pair_key((u32)lg[i][(size_t)0].num(), (u32)lg[i][1].num());
        p.value = (float)lg[i][2].num();
        lig_.push_back(p);
    }
    std::sort(advance_.begin(), advance_.end());
    std::sort(kern_.begin(), kern_.end());
    std::sort(lig_.begin(), lig_.end());
    return true;
}

u32 FontFace::glyph_for(u32 cp) const {
    std::vector<std::pair<u32, u32> >::const_iterator it =
        std::lower_bound(cmap_.begin(), cmap_.end(), std::make_pair(cp, (u32)0));
    return it != cmap_.end() && it->first == cp ? it->second : 0;
}

const GlyphInfo* FontFace::glyph(u32 gid) const {
    GlyphInfo k;
    k.gid = gid;
    std::vector<GlyphInfo>::const_iterator it = std::lower_bound(glyphs_.begin(), glyphs_.end(), k, GlyphLess());
    return it != glyphs_.end() && it->gid == gid ? &*it : NULL;
}

template <class V> static const typename V::value_type* find_pair(const V& v, u64 key) {
    typename V::value_type k;
    k.key = key;
    typename V::const_iterator it = std::lower_bound(v.begin(), v.end(), k);
    return it != v.end() && it->key == key ? &*it : NULL;
}

float FontFace::char_advance(u32 cp) const {
    const Pair* p = find_pair(advance_, cp);
    if (p) return p->value;
    const GlyphInfo* g = glyph(glyph_for(cp));
    return g ? g->advance : 0.0f;
}

float FontFace::kerning(u32 c1, u32 c2) const {
    const Pair* p = find_pair(kern_, pair_key(c1, c2));
    return p ? p->value : 0.0f;
}

u32 FontFace::ligature(u32 c1, u32 c2) const {
    const Pair* p = find_pair(lig_, pair_key(c1, c2));
    return p ? (u32)p->value : 0;
}

/* ---- FontLibrary ---- */

static FontLibrary* g_fonts = NULL;

FontLibrary::FontLibrary(const Pack* pack) : pack_(pack) {}

FontLibrary::~FontLibrary() {
    if (g_fonts == this) g_fonts = NULL;
    for (std::map<std::string, FontFace*>::iterator it = faces_.begin(); it != faces_.end(); ++it) delete it->second;
}

FontLibrary* FontLibrary::current() { return g_fonts; }
void FontLibrary::set_current(FontLibrary* lib) { g_fonts = lib; }

FontFace* FontLibrary::face(const std::string& name) {
    std::map<std::string, FontFace*>::iterator it = faces_.find(name);
    if (it != faces_.end()) return it->second;
    FontFace* f = new FontFace();
    std::string err;
    if (!pack_ || !f->load(*pack_, name, &err)) {
        log_warn("font: %s", err.c_str());
        delete f;
        f = NULL;
    }
    faces_[name] = f;
    return f;
}

FontFace* FontLibrary::face_for(const FontRes* font) {
    if (!font) return NULL;
    const FontRes* root = font->root();
    return root->file.empty() ? NULL : face(root->file);
}

static std::vector<const FontRes*> g_global_fallbacks;

void FontLibrary::set_global_fallbacks(const std::vector<const FontRes*>& fonts) { g_global_fallbacks = fonts; }

static const FontRes* chain_with(FontLibrary* lib, const FontRes* font, u32 cp, int depth) {
    if (!font || depth > 8) return NULL;
    FontFace* f = lib->face_for(font);
    if (f && f->glyph_for(cp)) return font;
    for (const FontRes* r = font; r; r = r->base)
        for (size_t i = 0; i < r->fallbacks.size(); ++i) {
            const FontRes* x = chain_with(lib, r->fallbacks[i], cp, depth + 1);
            if (x) return x;
        }
    return NULL;
}

const FontRes* FontLibrary::font_with(const FontRes* font, u32 cp) {
    const FontRes* x = chain_with(this, font, cp, 0);
    if (x) return x;
    for (size_t i = 0; i < g_global_fallbacks.size(); ++i) {
        FontFace* f = face_for(g_global_fallbacks[i]);
        if (f && f->glyph_for(cp)) return g_global_fallbacks[i];
    }
    return font;
}

} /* namespace spire */
