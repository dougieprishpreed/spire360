/* SpineSprite (the spine-godot node the game's scenes use) on our own Spine
 * runtime (engine/spine). The node holds a skeleton and an animation state
 * built from a SpineSkeletonDataResource (atlas + skeleton, converted to
 * JSON in the pack), advances them each frame and draws the slots in draw
 * order in its own local space, Spine's y axis flipped to Godot's. */
#ifndef SPIRE_SPINE_SPRITE_H
#define SPIRE_SPINE_SPRITE_H

#include "../spine/spine.h"
#include "node.h"

namespace spire {

class ImageTextureRes;
class SceneLibrary;

/* SpineSkeletonDataResource: loads the atlas and skeleton on first use;
 * one per resource path, shared by every sprite that uses it. */
class SpineDataRes : public Resource {
public:
    SpineDataRes() : Resource(RES_SPINE_DATA, "SpineSkeletonDataResource"), tried_(false), data_(NULL) {}
    virtual ~SpineDataRes();
    std::string atlas_path, skeleton_path;
    const SpSkeletonData* data(SceneLibrary* lib);
    ImageTextureRes* page(int i) const { return i >= 0 && (size_t)i < pages_.size() ? pages_[i] : NULL; }

private:
    bool tried_;
    SpAtlas atlas_;
    SpSkeletonData* data_;
    std::vector<ImageTextureRes*> pages_;
};

class SpineSprite : public Node2D {
public:
    SpineSprite();
    virtual ~SpineSprite();
    SpineDataRes* data_res;
    float time_scale;

    virtual bool set(const std::string& key, const Json& v, const PropCtx& ctx);
    virtual void process(double dt);
    virtual void draw(Canvas& c);

    /* NULL until the data loads (missing data stays NULL). */
    SpSkeleton* skeleton();
    SpAnimationState* animation_state();
    bool set_animation(const std::string& name, bool loop, int track);

private:
    bool ensure();
    SceneLibrary* lib_;
    SpSkeleton* skel_;
    SpAnimationState state_;
    bool failed_;
    std::vector<float> world_;
    std::vector<Vec2> v_, uv_;
    std::vector<Color> col_;
    std::vector<int> idx_;
    SpClipper clipper_;
    std::vector<float> cpos_, cuv_;
    std::vector<int> ctris_;
};

} /* namespace spire */

#endif
