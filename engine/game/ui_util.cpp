#include "ui_util.h"

#include "../scene/controls.h"
#include "../scene/node.h"
#include "../scene/rich_text.h"
#include "../scene/scene.h"
#include "../scene/text_nodes.h"

#include <ctype.h>

namespace spire {

std::string lower(std::string s) {
    for (size_t i = 0; i < s.size(); ++i) s[i] = (char)tolower((unsigned char)s[i]);
    return s;
}

Node* instance(SceneLibrary* lib, const std::string& path) {
    std::string err;
    Node* n = lib->instantiate(path, &err);
    if (!n) log_warn("screen: %s", err.c_str());
    return n;
}

Node* find_named(Node* n, const char* name) {
    if (!n) return NULL;
    if (n->name == name) return n;
    for (size_t i = 0; i < n->children.size(); ++i) {
        Node* r = find_named(n->children[i], name);
        if (r) return r;
    }
    return NULL;
}

Control* ctl(Node* scope, const char* unique) {
    Node* n = scope ? scope->find_unique(unique) : NULL;
    return n && n->is_control() ? (Control*)n : NULL;
}

void set_label(Node* n, const std::string& text) {
    if (n && n->type == "Label") ((Label*)n)->set_text(text);
    else if (n && n->type == "RichTextLabel") ((RichTextLabel*)n)->text = text;
}

void show_glyph(SceneLibrary* lib, Node* hotkey_icon, const char* glyph) {
    if (!hotkey_icon) return;
    Node* pad = hotkey_icon->find_unique("ControllerIcon");
    Node* key = hotkey_icon->find_unique("KeyboardIcon");
    if (key && key->is_canvas_item()) ((CanvasItem*)key)->visible = false;
    if (!pad || pad->type != "TextureRect") return;
    Resource* r = lib->ext(std::string("images/atlases/controller_atlas.sprites/xbox_360/") + glyph + ".tres");
    ((TextureRect*)pad)->texture = r && r->is_texture() ? (Texture2DRes*)r : NULL;
    ((CanvasItem*)pad)->visible = r != NULL;
}

void place(Control* c, float x, float y, float w, float h) {
    c->offset[SIDE_LEFT] = x;
    c->offset[SIDE_TOP] = y;
    c->offset[SIDE_RIGHT] = x + w;
    c->offset[SIDE_BOTTOM] = y + h;
}

Vec2 offset_pos(const Control* c) { return Vec2(c->offset[SIDE_LEFT], c->offset[SIDE_TOP]); }

Vec2 offset_size(const Control* c) {
    return Vec2(c->offset[SIDE_RIGHT] - c->offset[SIDE_LEFT], c->offset[SIDE_BOTTOM] - c->offset[SIDE_TOP]);
}

Color hex(unsigned v) {
    return Color(((v >> 16) & 255) / 255.0f, ((v >> 8) & 255) / 255.0f, (v & 255) / 255.0f, 1.0f);
}

Control* full_rect_control(const char* name) {
    Control* c = (Control*)SceneLibrary::create_node("Control");
    c->name = name;
    c->anchor[SIDE_LEFT] = c->anchor[SIDE_TOP] = 0;
    c->anchor[SIDE_RIGHT] = c->anchor[SIDE_BOTTOM] = 1;
    for (int i = 0; i < 4; ++i) c->offset[i] = 0;
    c->mouse_filter = 2;
    return c;
}

namespace {
class FrameDriver : public ScriptBehavior {
public:
    explicit FrameDriver(FrameListener* l) : l_(l) {}
    virtual void process(Node*, double dt) { l_->frame(dt); }

private:
    FrameListener* l_;
};
} /* namespace */

Node* frame_driver(FrameListener* listener) {
    Node* n = SceneLibrary::create_node("Node");
    n->name = "FrameDriver";
    n->behavior = new FrameDriver(listener);
    return n;
}

} /* namespace spire */
