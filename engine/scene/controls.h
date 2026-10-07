/* Concrete nodes: the Controls and Node2Ds the game's scenes use. Ported
 * from Godot 4.5.1 scene/gui and scene/2d (MIT, see THIRD_PARTY.md). */
#ifndef SPIRE_CONTROLS_H
#define SPIRE_CONTROLS_H

#include "node.h"

namespace spire {

class Texture2DRes;

class TextureRect : public Control {
public:
    enum { EXPAND_KEEP_SIZE, EXPAND_IGNORE_SIZE, EXPAND_FIT_WIDTH, EXPAND_FIT_WIDTH_PROPORTIONAL, EXPAND_FIT_HEIGHT,
           EXPAND_FIT_HEIGHT_PROPORTIONAL };
    enum { STRETCH_SCALE, STRETCH_TILE, STRETCH_KEEP, STRETCH_KEEP_CENTERED, STRETCH_KEEP_ASPECT,
           STRETCH_KEEP_ASPECT_CENTERED, STRETCH_KEEP_ASPECT_COVERED };
    TextureRect() : texture(NULL), expand_mode(0), stretch_mode(0), flip_h(false), flip_v(false) {}
    Texture2DRes* texture;
    int expand_mode, stretch_mode;
    bool flip_h, flip_v;
    virtual bool set(const std::string& key, const Json& v, const PropCtx& ctx);
    virtual Vec2 get_minimum_size();
    virtual void draw(Canvas& c);
};

class NinePatchRect : public Control {
public:
    NinePatchRect() : texture(NULL), draw_center(true), axis_h(0), axis_v(0) {
        for (int i = 0; i < 4; ++i) margin[i] = 0;
    }
    Texture2DRes* texture;
    bool draw_center;
    Rect2 region_rect;
    float margin[4];
    int axis_h, axis_v;
    virtual bool set(const std::string& key, const Json& v, const PropCtx& ctx);
    virtual Vec2 get_minimum_size() {
        return Vec2(margin[SIDE_LEFT] + margin[SIDE_RIGHT], margin[SIDE_TOP] + margin[SIDE_BOTTOM]);
    }
    virtual void draw(Canvas& c);
};

class ColorRect : public Control {
public:
    Color color;
    virtual bool set(const std::string& key, const Json& v, const PropCtx& ctx);
    virtual void draw(Canvas& c);
};

class Panel : public Control {
public:
    virtual void draw(Canvas& c);
};

class Container : public Control {
public:
    Container() { flags |= NF_CONTAINER; }
    /* Godot's Container::fit_child_in_rect. */
    void fit_child_in_rect(Control* child, const Rect2& rect);
    /* Children a container lays out: visible Controls that are not top level. */
    static Control* sortable(Node* n);
};

class BoxContainer : public Container {
public:
    enum { ALIGN_BEGIN, ALIGN_CENTER, ALIGN_END };
    explicit BoxContainer(bool vert) : vertical(vert), alignment(ALIGN_BEGIN) {}
    bool vertical;
    int alignment;
    virtual bool set(const std::string& key, const Json& v, const PropCtx& ctx);
    virtual Vec2 get_minimum_size();
    virtual void sort_children();
};

class MarginContainer : public Container {
public:
    virtual Vec2 get_minimum_size();
    virtual void sort_children();
};

class PanelContainer : public Container {
public:
    virtual Vec2 get_minimum_size();
    virtual void sort_children();
    virtual void draw(Canvas& c);
};

class CenterContainer : public Container {
public:
    CenterContainer() : use_top_left(false) {}
    bool use_top_left;
    virtual bool set(const std::string& key, const Json& v, const PropCtx& ctx);
    virtual Vec2 get_minimum_size();
    virtual void sort_children();
};

class GridContainer : public Container {
public:
    GridContainer() : columns(1) {}
    int columns;
    virtual bool set(const std::string& key, const Json& v, const PropCtx& ctx);
    virtual Vec2 get_minimum_size();
    virtual void sort_children();
};

class FlowContainer : public Container {
public:
    enum { ALIGN_BEGIN, ALIGN_CENTER, ALIGN_END };
    explicit FlowContainer(bool vert) : vertical(vert), alignment(ALIGN_BEGIN), reverse_fill(false), cached_size(0) {}
    bool vertical;
    int alignment;
    bool reverse_fill;
    virtual bool set(const std::string& key, const Json& v, const PropCtx& ctx);
    virtual Vec2 get_minimum_size();
    virtual void sort_children();
private:
    float cached_size;
};

class Sprite2D : public Node2D {
public:
    Sprite2D() : texture(NULL), centered(true), flip_h(false), flip_v(false), region_enabled(false), hframes(1),
                 vframes(1), frame(0) {}
    Texture2DRes* texture;
    bool centered, flip_h, flip_v, region_enabled;
    Vec2 offset;
    Rect2 region_rect;
    int hframes, vframes, frame;
    virtual bool set(const std::string& key, const Json& v, const PropCtx& ctx);
    virtual void draw(Canvas& c);
};

} /* namespace spire */

#endif
