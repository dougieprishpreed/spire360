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

class SpineFollowNode;

class MaterialRes;

class SpineSprite : public Node2D {
public:
    SpineSprite();
    virtual ~SpineSprite();
    SpineDataRes* data_res;
    float time_scale;
    /* normal / additive / multiply / screen_material: what slots of each
     * blend mode draw with (NULL: spine-godot's default for the mode). */
    MaterialRes* slot_material[4];

    virtual bool set(const std::string& key, const Json& v, const PropCtx& ctx);
    virtual void process(double dt);
    virtual void draw(Canvas& c);

    /* NULL until the data loads (missing data stays NULL). */
    SpSkeleton* skeleton();
    SpAnimationState* animation_state();
    bool set_animation(const std::string& name, bool loop, int track);
    /* The track's animation from a point of it (0..1 of its duration). */
    void set_track_fraction(int track, float fraction);
    bool has_animation(const std::string& name);
    /* SetSkeletonDataRes from code (before anything is played). */
    void set_data(SceneLibrary* lib, SpineDataRes* res);
    /* Skeleton.SetSkin + SetSlotsToSetupPose; false without that skin. */
    bool set_skin(const std::string& name);
    /* The same with a skin made of these (NewSkin + AddSkin for each one the
     * skeleton has); false when it has none of them. */
    bool set_skins(const std::vector<std::string>& names);
    /* AnimationState.AddAnimation / AddEmptyAnimation / TimeScale */
    bool add_animation(const std::string& name, bool loop, float delay, int track);
    void add_empty_animation(int track);
    void set_state_time_scale(float scale);
    /* animation_started / animation_event since the last frame (its process
     * fills it; the scripts on its children read it in theirs). */
    const std::vector<SpFired>& fired() const { return fired_; }
    /* Skeleton.GetBounds: the region and mesh attachments' world vertices'
     * box, in the sprite's coordinates (empty without any). */
    Rect2 bounds();
    /* MegaSprite.GetGlobalBoneTransform's origin, in the sprite's
     * coordinates; false without that bone. */
    bool bone_position(const std::string& bone, Vec2* out);
    /* MegaBone.SetRotation: a bone's local rotation (degrees) from code,
     * kept over the animations each frame. */
    void set_bone_rotation(const std::string& bone, float degrees) { bone_rotation_[bone] = degrees; }

private:
    bool ensure();
    void drive_bones();
    void follow_bones();
    SpineFollowNode* follow_node(Node* n);
    SceneLibrary* lib_;
    SpSkeleton* skel_;
    SpSkin* custom_skin_; /* set_skins' (its attachments are the data's) */
    SpAnimationState state_;
    std::vector<SpFired> fired_;
    bool failed_;
    std::vector<float> world_;
    std::vector<Vec2> v_, uv_;
    std::vector<Color> col_;
    std::vector<int> idx_;
    SpClipper clipper_;
    std::vector<SpineFollowNode*> slot_nodes_; /* draw(): the slot nodes with materials, by slot */
    std::vector<float> cpos_, cuv_;
    std::vector<int> ctris_;
    std::map<std::string, float> bone_rotation_;
};

/* spine-godot's SpineBoneNode / SpineSlotNode, children of a SpineSprite:
 * follow a bone (a slot's bone) each frame, or (bone_mode 1, "drive") move
 * the bone to the node. A slot node's children draw with the sprite (after
 * it, or behind it with show_behind_parent), not at the slot's place in the
 * draw order. */
class SpineFollowNode : public Node2D {
public:
    explicit SpineFollowNode(bool slot) : is_slot(slot), drive(false), enabled(true), index(-2), slot_index(-2) {
        for (int i = 0; i < 4; ++i) slot_material[i] = NULL;
    }
    bool is_slot;
    std::string target; /* bone_name / slot_name */
    bool drive, enabled;
    int index;          /* resolved bone index (-1: none, -2: not yet) */
    int slot_index;     /* a slot node's slot (-1: none, -2: not yet) */
    /* A slot node's normal / additive / multiply / screen_material: its
     * slot draws with them before the sprite's (spine-godot's
     * SpineSprite::update_meshes). */
    MaterialRes* slot_material[4];
    virtual bool set(const std::string& key, const Json& v, const PropCtx& ctx);
};

} /* namespace spire */

#endif
