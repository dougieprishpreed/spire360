/* Godot's 3D nodes as far as the game uses them: the two power-up effects
 * (vfx_power_up_3d_back, vfx_ghostly_power_up_3d_back) render a Node3D
 * tree through a Camera3D into a SubViewport, which a Sprite2D shows
 * behind the creature. Node3D transforms (Transform3D, top_level), the
 * camera's perspective (fov, near, keep_aspect), MeshInstance3D's ArrayMesh
 * (plain or compressed surfaces) drawn with its material_override into the
 * viewport's canvas: triangles with affine textures, no depth buffer
 * (meshes far to near, each in index order, as Godot's transparent pass
 * draws them). GPUParticles3D are only Node3Ds here: neither scene's draws
 * (both auras hidden, the beam never emitted). */
#ifndef SPIRE_NODE3D_H
#define SPIRE_NODE3D_H

#include "node.h"

namespace spire {

class Canvas;
class MaterialRes;

/* Basis (rows, as Godot's text gives them) and origin. */
struct Xform3D {
    float m[3][3];
    float o[3];
    Xform3D();
    void apply(const float in[3], float out[3]) const;
    Xform3D operator*(const Xform3D& b) const; /* this after b */
    Xform3D inverse() const;
};

class Node3D : public Node {
public:
    Node3D();
    Xform3D transform;
    bool visible, top_level;
    virtual bool set(const std::string& key, const Json& v, const PropCtx& ctx);
    Xform3D global_transform() const;
    bool visible_in_tree() const;
};

class Camera3D : public Node3D {
public:
    Camera3D();
    float fov, near_plane;
    int keep_aspect; /* 0 keep width, 1 keep height (Godot's default) */
    virtual bool set(const std::string& key, const Json& v, const PropCtx& ctx);
};

class MeshInstance3D : public Node3D {
public:
    MeshInstance3D();
    const Resource* mesh;
    MaterialRes* material;
    virtual bool set(const std::string& key, const Json& v, const PropCtx& ctx);
};

/* The 3D nodes under a SubViewport through its Camera3D, into the canvas
 * (set to the viewport's units: size). False when it has no camera. */
bool draw_3d(Canvas& c, Node* viewport, const Vec2& size, const Xform2D& to_pixels);

} /* namespace spire */

#endif
