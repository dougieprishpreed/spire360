/* Text shaping and line breaking, matching Godot 4.5's TextServerAdvanced
 * for the fonts the game uses (MSDF, Latin/Cyrillic/Greek scripts): advances
 * and kerning come from Godot's own shaper (exported per font), positions
 * follow text_server_adv.cpp (_shape_run), line breaks follow
 * TextServer::shaped_text_get_line_breaks. Break opportunities are a subset
 * of ICU's line breaking rules (spaces, hyphens, CJK, newlines).
 * Ported from Godot 4.5.1 (MIT, see THIRD_PARTY.md). */
#ifndef SPIRE_SHAPER_H
#define SPIRE_SHAPER_H

#include "font.h"

namespace spire {

class Canvas;

enum GlyphFlags {
    GF_SPACE = 1,
    GF_BREAK_SOFT = 2,
    GF_BREAK_HARD = 4,
    GF_VIRTUAL = 8 /* inserted break point, zero width */
};

struct ShapedGlyph {
    FontFace* face; /* NULL: nothing to draw */
    u32 gid, cp;
    int font_size;
    float advance, x_off, y_off;
    int start, end, count;
    u32 flags;
};

struct ShapedText {
    std::vector<ShapedGlyph> glyphs;
    float width, ascent, descent;
    int start, end;
    ShapedText() : width(0), ascent(0), descent(0), start(0), end(0) {}
    void clear() {
        glyphs.clear();
        width = ascent = descent = 0;
        start = end = 0;
    }
};

/* UTF-8 -> code points. */
void utf8_decode(const std::string& s, std::vector<u32>& out);
bool is_whitespace_cp(u32 c);
bool is_linebreak_cp(u32 c);

/* Font metrics with the variation's spacing (Font::get_ascent etc.). */
float font_ascent(const FontRes* font, int size);
float font_descent(const FontRes* font, int size);
float font_height(const FontRes* font, int size);

/* Appends a run [start, end) of text in one font. last_run: the run ends
 * the text (its last glyph gets no extra spacing). */
void shape_run(const std::vector<u32>& text, int start, int end, const FontRes* font, int size, bool last_run,
               ShapedText& out);
/* Break flags (call once after all runs). */
void shape_finish(const std::vector<u32>& text, ShapedText& out);

enum { BREAK_MANDATORY = 1, BREAK_WORD_BOUND = 2, BREAK_GRAPHEME_BOUND = 4, BREAK_ADAPTIVE = 8,
       BREAK_TRIM_START_EDGE_SPACES = 16, BREAK_TRIM_END_EDGE_SPACES = 32 };
/* Line ranges as [start0, end0, start1, end1, ...] (character positions). */
void line_breaks(const ShapedText& t, float width, int flags, std::vector<int>& out);
/* The glyphs of [start, end), with metrics. */
void shaped_substr(const ShapedText& t, int start, int end, ShapedText& out);

/* Draws one glyph at the pen position (baseline). outline_size > 0 draws
 * the outline (Godot's font_draw_glyph_outline). */
void draw_glyph(Canvas& c, const ShapedGlyph& g, const Vec2& pos, const Color& color, int outline_size);

} /* namespace spire */

#endif
