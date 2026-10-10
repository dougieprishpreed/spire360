/* spire_view: renders a scene from pack.s3p to a PNG with the software
 * renderer, for comparing against Godot's own render.
 *
 *   spire_view <pack.s3p> <scene> <out.png> [--scale S] [--no-scripts] [--time T]
 *   spire_view <pack.s3p> <text> <out.png> --text <font> <size>
 *
 * --scale 1 renders at the design size (1920x1080) for diffs against a
 * reference; the console uses 2/3 (1280x720). */
#include "../../engine/core/pack.h"
#include "../../engine/game/game.h"
#include "../../engine/gfx/sw_renderer.h"
#include "../../engine/scene/canvas.h"
#include "../../engine/scene/controls.h"
#include "../../engine/scene/scene.h"
#include "../../engine/scene/script.h"
#include "../../engine/scene/text_nodes.h"
#include "../../engine/scene/tree.h"
#include "../../engine/text/font.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using namespace spire;

/* --text: one Label on black, like tools/godot/glyph.gd, for comparing
 * text rendering with Godot's. */
static Node* text_scene(SceneLibrary& lib, const char* font, int size, const char* text, Vec2* design) {
    Control* root = new Control();
    root->type = "Control";
    ColorRect* bg = new ColorRect();
    bg->type = "ColorRect";
    bg->color = Color(0, 0, 0, 1);
    bg->offset[2] = 400;
    bg->offset[3] = 300;
    root->add_child(bg);
    Label* l = new Label();
    l->type = "Label";
    l->text = text;
    l->offset[0] = l->offset[2] = 10;
    l->offset[1] = l->offset[3] = 10;
    Resource* f = lib.ext(font);
    if (f) l->override_resources["fonts/font"] = f;
    static Json size_json;
    size_json = Json::make_number(size);
    l->override_values["font_sizes/font_size"] = &size_json;
    root->add_child(l);
    *design = Vec2(400, 300);
    return root;
}

int main(int argc, char** argv) {
    if (argc < 4) {
        fprintf(stderr, "usage: spire_view <pack.s3p> <scene> <out.png> [--scale S] [--no-scripts] [--time T]\n");
        return 2;
    }
    float scale = 1.0f;
    int frames = 5; /* --time: seconds of process at 60 fps */
    for (int i = 4; i < argc; ++i) {
        if (strcmp(argv[i], "--scale") == 0 && i + 1 < argc) scale = (float)atof(argv[++i]);
        else if (strcmp(argv[i], "--time") == 0 && i + 1 < argc) frames = (int)(atof(argv[++i]) * 60.0 + 0.5);
        else if (strcmp(argv[i], "--no-scripts") == 0) set_scripts_enabled(false); /* as Godot's render, without the game's C# */
    }
    install_game_scripts();
    Pack pack;
    std::string err;
    if (!pack.open(argv[1], &err)) {
        fprintf(stderr, "%s\n", err.c_str());
        return 1;
    }
    SceneLibrary lib(&pack);
    FontLibrary fonts(&pack);
    FontLibrary::set_current(&fonts);
    Vec2 design(1920, 1080);
    Node* scene = NULL;
    for (int i = 4; i + 2 < argc; ++i)
        if (strcmp(argv[i], "--text") == 0) scene = text_scene(lib, argv[i + 1], atoi(argv[i + 2]), argv[2], &design);
    if (!scene) scene = lib.instantiate(argv[2], &err);
    if (!scene) {
        fprintf(stderr, "%s\n", err.c_str());
        return 1;
    }
    SceneTree tree;
    tree.set_viewport(design, Xform2D::scaling(scale, scale));
    tree.root()->add_child(scene);
    tree.layout();
    /* Godot's reference render is taken on its 5th frame (tools/godot/shot.gd). */
    for (int f = 0; f < frames; ++f) tree.process(1.0 / 60.0);
    SwRenderer sw;
    Canvas canvas(&sw, &pack);
    /* project.binary: rendering/environment/defaults/default_clear_color */
    sw.begin((int)(design.x * scale + 0.5f), (int)(design.y * scale + 0.5f),
             Color(0.0923724f, 0.122398f, 0.116929f, 1.0f));
    tree.render(canvas);
    sw.end();
    if (!write_png(argv[3], sw.framebuffer())) {
        fprintf(stderr, "cannot write %s\n", argv[3]);
        return 1;
    }
    printf("%s: %d items, %ld draw calls, %ld triangles\n", argv[3], tree.items_drawn, canvas.draw_calls(),
           sw.triangles_drawn());
    for (std::map<std::string, int>::iterator it = tree.skipped.begin(); it != tree.skipped.end(); ++it)
        printf("  not drawn: %s x%d\n", it->first.c_str(), it->second);
    for (std::map<std::string, int>::iterator it = lib.unsupported.begin(); it != lib.unsupported.end(); ++it)
        printf("  unsupported: %s x%d\n", it->first.c_str(), it->second);
    return 0;
}
