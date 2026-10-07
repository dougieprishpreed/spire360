/* spire_view: renders a scene from pack.s3p to a PNG with the software
 * renderer, for comparing against Godot's own render.
 *
 *   spire_view <pack.s3p> <scene> <out.png> [--scale S]
 *
 * --scale 1 renders at the design size (1920x1080) for diffs against a
 * reference; the console uses 2/3 (1280x720). */
#include "../../engine/core/pack.h"
#include "../../engine/gfx/sw_renderer.h"
#include "../../engine/scene/canvas.h"
#include "../../engine/scene/scene.h"
#include "../../engine/scene/tree.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using namespace spire;

int main(int argc, char** argv) {
    if (argc < 4) {
        fprintf(stderr, "usage: spire_view <pack.s3p> <scene> <out.png> [--scale S]\n");
        return 2;
    }
    float scale = 1.0f;
    for (int i = 4; i + 1 < argc; ++i)
        if (strcmp(argv[i], "--scale") == 0) scale = (float)atof(argv[++i]);
    Pack pack;
    std::string err;
    if (!pack.open(argv[1], &err)) {
        fprintf(stderr, "%s\n", err.c_str());
        return 1;
    }
    SceneLibrary lib(&pack);
    Node* scene = lib.instantiate(argv[2], &err);
    if (!scene) {
        fprintf(stderr, "%s\n", err.c_str());
        return 1;
    }
    SceneTree tree;
    Vec2 design(1920, 1080);
    tree.set_viewport(design, Xform2D::scaling(scale, scale));
    tree.root()->add_child(scene);
    tree.layout();
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
