/* Spine 4.2 runtime: our own, written from the format's public description
 * (no Esoteric Software code). The converter
 * (converter/spire_convert/spine.py) reads the binary skeleton and the atlas
 * and stores them in the pack (skeleton: our binary engine form, "SPB1";
 * atlas: JSON); this side holds the data, poses the skeleton from
 * animations and constraints, and produces the triangles a SpineSprite draws.
 *
 * Coordinates: Spine's y axis points up; the skeleton runs y-down (as
 * spine-godot does), so world positions come out in Godot's local space.
 *
 * Files: spine_data.cpp (loading), spine_anim.cpp (timelines, animation
 * state), spine_skeleton.cpp (world transforms, update order, IK /
 * transform / path / physics constraints, vertices), spine_clip.cpp
 * (clipping attachments). Not done: dark-colour tinting (drawn with the
 * light colour; the game's dark colours are black or next to it), the
 * screen blend mode without a screen_material (drawn additive; the game's
 * two screen-blend skeletons, Neow and the Defect's select, sit in scenes
 * that give them a Mix material). */
#ifndef SPIRE_SPINE_H
#define SPIRE_SPINE_H

#include "../core/base.h"
#include "../core/json.h"

#include <map>

namespace spire {

enum SpInherit { SP_INHERIT_NORMAL, SP_INHERIT_ONLY_TRANSLATION, SP_INHERIT_NO_ROTATION_OR_REFLECTION,
                 SP_INHERIT_NO_SCALE, SP_INHERIT_NO_SCALE_OR_REFLECTION };
enum SpBlend { SP_BLEND_NORMAL, SP_BLEND_ADDITIVE, SP_BLEND_MULTIPLY, SP_BLEND_SCREEN };
enum SpAttachmentType { SP_ATT_REGION, SP_ATT_MESH, SP_ATT_BOUNDING_BOX, SP_ATT_PATH, SP_ATT_POINT,
                        SP_ATT_CLIPPING, SP_ATT_OTHER };
enum SpPhysics { SP_PHYSICS_NONE, SP_PHYSICS_RESET, SP_PHYSICS_UPDATE, SP_PHYSICS_POSE };

/* ---- atlas ---- */

struct SpAtlasPage {
    std::string texture; /* pack path of the page image */
    int width, height;
};

struct SpAtlasRegion {
    std::string name;
    int page;
    int x, y, w, h;       /* packed rect in the page (w, h before rotation) */
    int ox, oy, ow, oh;   /* offset of the packed rect in the original image (oy from the bottom), original size */
    int degrees;          /* 0 or 90: stored rotated 90 degrees counter-clockwise */
    int index;
};

class SpAtlas {
public:
    bool load(const Json& doc, std::string* error);
    const SpAtlasRegion* find(const std::string& name) const;
    std::vector<SpAtlasPage> pages;
    std::vector<SpAtlasRegion> regions;
};

/* ---- skeleton data ---- */

struct SpBoneData {
    std::string name;
    int parent;
    float x, y, rotation, scale_x, scale_y, shear_x, shear_y, length;
    int inherit;
    bool skin_required;
};

struct SpSlotData {
    std::string name;
    int bone;
    Color color;
    bool has_dark;
    std::string attachment; /* setup attachment key, "" for none */
    int blend;
};

struct SpIkData {
    std::string name;
    int order;
    std::vector<int> bones;
    int target, bend;
    bool compress, stretch, uniform, skin_required;
    float mix, softness;
};

struct SpTransformData {
    std::string name;
    int order;
    std::vector<int> bones;
    int target;
    bool local, relative, skin_required;
    float offset_rotation, offset_x, offset_y, offset_scale_x, offset_scale_y, offset_shear_y;
    float mix_rotate, mix_x, mix_y, mix_scale_x, mix_scale_y, mix_shear_y;
};

enum SpPositionMode { SP_POSITION_FIXED, SP_POSITION_PERCENT };
enum SpSpacingMode { SP_SPACING_LENGTH, SP_SPACING_FIXED, SP_SPACING_PERCENT, SP_SPACING_PROPORTIONAL };
enum SpRotateMode { SP_ROTATE_TANGENT, SP_ROTATE_CHAIN, SP_ROTATE_CHAIN_SCALE };

struct SpPathData {
    std::string name;
    int order;
    std::vector<int> bones;
    int target; /* slot */
    int position_mode, spacing_mode, rotate_mode;
    bool skin_required;
    float offset_rotation, position, spacing, mix_rotate, mix_x, mix_y;
};

enum { SP_PHYS_GLOBAL_INERTIA = 1, SP_PHYS_GLOBAL_STRENGTH = 2, SP_PHYS_GLOBAL_DAMPING = 4, SP_PHYS_GLOBAL_MASS = 8,
       SP_PHYS_GLOBAL_WIND = 16, SP_PHYS_GLOBAL_GRAVITY = 32, SP_PHYS_GLOBAL_MIX = 64 };

struct SpPhysicsData {
    std::string name;
    int order, bone;
    float x, y, rotate, scale_x, shear_x, limit, step;
    float inertia, strength, damping, mass_inverse, wind, gravity, mix;
    int global; /* SP_PHYS_GLOBAL_* */
    bool skin_required;
};

struct SpAttachment {
    SpAttachment();
    int type;
    std::string key;          /* skin placeholder name */
    std::string name, path;
    std::string timeline_key; /* deform timelines keyed on this (linked meshes: the parent) */
    int slot;
    Color color;
    /* sequence (frames of an animation as separate atlas regions) */
    int seq_count, seq_start, seq_digits, seq_setup;
    /* region */
    float x, y, rotation, scale_x, scale_y, width, height;
    /* vertex attachments (mesh, bounding box, path, clipping) */
    int vertex_count;
    std::vector<float> vertices;  /* unweighted: x, y per vertex; weighted: x, y, weight per bone entry */
    std::vector<int> bones;       /* weighted: per vertex, count then bone indices; empty if unweighted */
    std::vector<float> region_uvs;
    std::vector<int> triangles;
    int hull;
    bool closed, constant_speed;  /* path */
    std::vector<float> lengths;   /* path: length at the end of each curve */
    int end_slot;                 /* clipping */
    /* Resolved from the atlas, per sequence frame (one frame without a
     * sequence): region, page uvs (region: 8 values BL UL UR BR; mesh: per
     * vertex) and, for regions, the quad in bone space. */
    std::vector<const SpAtlasRegion*> regions;
    std::vector<std::vector<float> > frame_uvs;
    std::vector<std::vector<float> > frame_offsets;
    bool weighted() const { return !bones.empty(); }
    int deform_length() const { return (int)(weighted() ? vertices.size() / 3 * 2 : vertices.size()); }
    int frame(int sequence_index) const; /* index into regions / frame_uvs */
};

struct SpSkin {
    std::string name;
    std::vector<int> bones, ik, transform, path, physics;
    std::vector<SpAttachment*> attachments;
    std::map<std::string, SpAttachment*> lookup; /* "slot/key" */
    SpAttachment* find(int slot, const std::string& key) const;
};

/* A timeline's curve between keys. Bezier curves are sampled to 9 points per
 * value (Spine's runtimes evaluate them as that polyline). */
struct SpCurves {
    std::vector<int> kind;       /* per key but the last: CURVE_* or the offset into points */
    std::vector<float> points;   /* 18 floats (x, y) per value of a bezier key */
    enum { CURVE_LINEAR = -1, CURVE_STEPPED = -2 };
};

/* Values match the converter's TIMELINE_IDS (spine.py). */
enum SpTimelineType {
    SPT_ROTATE, SPT_TRANSLATE, SPT_TRANSLATE_X, SPT_TRANSLATE_Y, SPT_SCALE, SPT_SCALE_X, SPT_SCALE_Y, SPT_SHEAR,
    SPT_SHEAR_X, SPT_SHEAR_Y, SPT_INHERIT,
    SPT_ATTACHMENT, SPT_RGBA, SPT_RGB, SPT_ALPHA, SPT_RGBA2, SPT_RGB2,
    SPT_DEFORM, SPT_SEQUENCE, SPT_DRAW_ORDER, SPT_EVENT,
    SPT_IK, SPT_TRANSFORM, SPT_PATH_POSITION, SPT_PATH_SPACING, SPT_PATH_MIX,
    SPT_PHYSICS_INERTIA, SPT_PHYSICS_STRENGTH, SPT_PHYSICS_DAMPING, SPT_PHYSICS_MASS, SPT_PHYSICS_WIND,
    SPT_PHYSICS_GRAVITY, SPT_PHYSICS_MIX, SPT_PHYSICS_RESET,
    SPT_COUNT
};

struct SpTimeline {
    SpTimeline() : type(0), target(0), values(0), skin(0) {}
    int type;
    int target;               /* bone, slot or constraint index (physics: -1 = all) */
    int values;               /* values per key (curve timelines) */
    std::vector<float> keys;  /* curve timelines: time, values... per key; others: times */
    SpCurves curves;
    std::vector<std::string> names;              /* attachment; event: the event per key */
    std::vector<int> ints;                        /* inherit modes; IK: bend per key; event: int */
    std::vector<float> fvals;                     /* event: float per key */
    std::vector<std::string> strs;                /* event: string per key */
    std::vector<int> flags;                       /* IK: 1 compress, 2 stretch per key */
    std::vector<std::vector<int> > orders;        /* draw order (empty = setup) */
    std::vector<std::vector<float> > deforms;     /* deform */
    int skin;
    std::string attachment;                       /* deform / sequence: the timeline key */
    int key_count() const { return values ? (int)keys.size() / (values + 1) : (int)keys.size(); }
    float time(int k) const { return values ? keys[k * (values + 1)] : keys[k]; }
};

struct SpAnimation {
    std::string name;
    float duration;
    std::vector<SpTimeline> timelines;
};

class SpSkeletonData {
public:
    SpSkeletonData();
    ~SpSkeletonData();
    /* skeleton: the pack's binary form; atlas: may be NULL (no textures). */
    bool load(const u8* data, size_t size, const SpAtlas* atlas, std::string* error);
    int find_bone(const std::string& name) const;
    int find_slot(const std::string& name) const;
    const SpAnimation* find_animation(const std::string& name) const;
    const SpSkin* find_skin(const std::string& name) const;
    const SpSkin* default_skin() const;

    float x, y, width, height;
    float reference_scale; /* physics: units per meter */
    std::vector<SpBoneData> bones;
    std::vector<SpSlotData> slots;
    std::vector<SpIkData> ik;
    std::vector<SpTransformData> transform;
    std::vector<SpPathData> path;
    std::vector<SpPhysicsData> physics;
    std::vector<SpSkin*> skins;
    std::vector<SpAnimation> animations;
    const SpAtlas* atlas;
    std::vector<std::string> unsupported; /* features this runtime ignores */

private:
    SpSkeletonData(const SpSkeletonData&);
    SpSkeletonData& operator=(const SpSkeletonData&);
};

/* ---- skeleton instance ---- */

struct SpBone {
    const SpBoneData* data;
    int parent;
    std::vector<int> children;
    /* pose (what animations set) and applied (what constraints adjust) */
    float x, y, rotation, scale_x, scale_y, shear_x, shear_y;
    float ax, ay, arotation, ascale_x, ascale_y, ashear_x, ashear_y;
    int inherit;
    float a, b, c, d, world_x, world_y;
    bool active, sorted;
};

struct SpSlot {
    const SpSlotData* data;
    Color color;
    SpAttachment* attachment;
    std::vector<float> deform;
    int sequence_index; /* -1: the sequence's setup frame */
};

struct SpIk {
    const SpIkData* data;
    int bend;
    bool compress, stretch, active;
    float mix, softness;
};

struct SpTransform {
    const SpTransformData* data;
    bool active;
    float mix_rotate, mix_x, mix_y, mix_scale_x, mix_scale_y, mix_shear_y;
};

struct SpPath {
    const SpPathData* data;
    bool active;
    float position, spacing, mix_rotate, mix_x, mix_y;
};

struct SpPhysicsC {
    const SpPhysicsData* data;
    bool active, reset;
    float inertia, strength, damping, mass_inverse, wind, gravity, mix;
    float ux, uy, cx, cy, tx, ty;
    float x_offset, x_velocity, y_offset, y_velocity;
    float rotate_offset, rotate_velocity, scale_offset, scale_velocity;
    float remaining, last_time;
};

class SpSkeleton {
public:
    explicit SpSkeleton(const SpSkeletonData* data);
    const SpSkeletonData* data() const { return data_; }
    void set_skin(const SpSkin* skin); /* NULL: the default skin only */
    void set_to_setup_pose();
    void set_bones_to_setup_pose();
    void set_slots_to_setup_pose();
    SpAttachment* attachment(int slot, const std::string& key) const;
    /* Physics time (Godot's frame delta). */
    void update(float dt) { time += dt; }
    void update_world_transform(int physics = SP_PHYSICS_UPDATE);
    /* World positions of vertex values [start, start + count) of an
     * attachment (x, y pairs; region: its 4 corners). */
    void world_vertices(const SpSlot& slot, const SpAttachment& a, int start, int count, float* out) const;
    void world_vertices(const SpSlot& slot, std::vector<float>& out) const;
    void reset_physics(int constraint); /* -1: all */

    std::vector<SpBone> bones;
    std::vector<SpSlot> slots;
    std::vector<int> draw_order;
    std::vector<SpIk> ik;
    std::vector<SpTransform> transform;
    std::vector<SpPath> path;
    std::vector<SpPhysicsC> physics;
    Color color;
    float x, y, scale_x, scale_y; /* scale_y as authored; y_down flips it */
    bool y_down;
    float time;

    /* Effective scale (y_down applied). */
    float sx() const { return scale_x; }
    float sy() const { return y_down ? -scale_y : scale_y; }

    /* Used by the constraints (spine_skeleton.cpp). */
    void bone_update(SpBone& b, float x, float y, float rotation, float scale_x, float scale_y, float shear_x,
                     float shear_y);
    void bone_update_applied(SpBone& b);

private:
    void update_cache();
    void sort_bone(int bone);
    void sort_reset(const std::vector<int>& bones);
    void sort_path_attachment(const SpSkin* skin, int slot, int slot_bone);
    void sort_path_attachment(const SpAttachment* a, int slot_bone);
    bool skin_has(const std::vector<int> SpSkin::*list, int index) const;
    void apply_ik(SpIk& c);
    void apply_transform(SpTransform& c);
    void apply_path(SpPath& c);
    void apply_physics(SpPhysicsC& c, int physics);
    struct CacheItem {
        int kind; /* 0 bone, 1 ik, 2 transform, 3 path, 4 physics */
        int index;
    };
    std::vector<CacheItem> cache_;
    const SpSkeletonData* data_;
    const SpSkin* skin_;
    std::vector<float> scratch_[3];
};

/* AnimationState, for what the game does with it: tracks that play an
 * animation (looping or not), queued entries, and crossfades between
 * animations on a track (mix duration). */
/* What an AnimationState did that its listeners hear (spine-godot's
 * animation_started / animation_event signals). */
struct SpFired {
    bool start;             /* an entry started (else an event key passed) */
    int track;
    const SpAnimation* anim;
    std::string name;       /* the event's */
    int ival;
    float fval;
    std::string sval;
};

class SpAnimationState {
public:
    SpAnimationState();
    /* Entries started and event keys passed since the owner last cleared it. */
    std::vector<SpFired> fired;
    void set_animation(int track, const SpAnimation* anim, bool loop);
    void add_animation(int track, const SpAnimation* anim, bool loop, float delay);
    void set_empty_animation(int track);
    /* AddEmptyAnimation: the track empties after what is queued on it. */
    void add_empty_animation(int track);
    void update(float dt);
    void apply(SpSkeleton& skel);
    const SpAnimation* current(int track) const;
    /* Whether the current entry of a track loops. */
    bool current_loop(int track) const;
    /* Seconds into the current animation of a track (-1: none). */
    float track_time(int track) const;
    /* MegaTrackEntry.SetTrackTime: the current entry moved to t seconds. */
    void set_track_time(int track, float t);
    bool track_complete(int track) const;
    float default_mix;
    float time_scale;

private:
    struct Entry {
        const SpAnimation* anim;
        bool loop;
        float time, last_time, delay, mix_time, mix_duration, time_scale;
        bool events_from_start; /* its event keys from the start fire on the next update */
        Entry() : anim(NULL), loop(false), time(0), last_time(-1), delay(0), mix_time(0), mix_duration(0),
                  time_scale(1), events_from_start(true) {}
    };
    struct Track {
        Entry cur, from;
        bool has_cur, has_from;
        std::vector<Entry> queue;
        Track() : has_cur(false), has_from(false) {}
    };
    std::vector<Track> tracks_;
    Track& track(int i);
    void start(Track& t, const Entry& e);
};

/* Poses the skeleton from an animation at a time: alpha 1 replaces the
 * animated properties (keys before the first one give the setup pose), less
 * than 1 mixes from the current pose. last_time (-1: none) is the previous
 * time, for physics resets keyed in between. */
void sp_apply_animation(const SpAnimation& anim, SpSkeleton& skel, float last_time, float time, bool loop,
                        float alpha);

/* Clipping (spine_clip.cpp): triangles of the slots after a clipping
 * attachment are cut to its polygon until its end slot. */
class SpClipper {
public:
    SpClipper() : active_(false), end_slot_(-1) {}
    bool active() const { return active_; }
    void start(const SpSkeleton& skel, const SpSlot& slot, const SpAttachment& clip);
    /* Ends the clip after the given slot when it is the end slot. */
    void end_slot(int slot_index);
    void end();
    /* Clips indexed triangles (positions x, y; uvs u, v) to the polygon. */
    void clip(const std::vector<float>& pos, const std::vector<float>& uv, const std::vector<int>& tris,
              std::vector<float>& out_pos, std::vector<float>& out_uv, std::vector<int>& out_tris);

private:
    bool active_;
    int end_slot_;
    std::vector<float> triangles_; /* the clip polygon as triangles, 6 floats each, counter-clockwise */
};

} /* namespace spire */

#endif
