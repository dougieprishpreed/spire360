/* Spine 4.2 runtime (DECISIONS D12: our own, written from the format's
 * public description; no Esoteric Software code). The converter
 * (converter/spire_convert/spine.py) reads the binary skeleton and the atlas
 * and stores them as JSON; this side holds the data, poses the skeleton from
 * animations and produces the triangles a SpineSprite draws.
 *
 * Coordinates: Spine's y axis points up; the skeleton runs with scale_y = -1
 * so world positions come out in Godot's y-down local space, as the
 * SpineSprite node sees them.
 *
 * Supported: bones (all inherit modes), slots (colour, attachment), region
 * and mesh attachments (weighted or not, deform), skins, animations with
 * linear / stepped / bezier curves (bone, slot colour and attachment, deform,
 * draw order, inherit timelines), looping tracks with crossfades. Not yet:
 * IK / transform / path / physics constraints, clipping, sequences, dark
 * colour tinting (logged once per skeleton). */
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
    float x, y, rotation, scale_x, scale_y, shear_x, shear_y;
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

struct SpAttachment {
    SpAttachment();
    int type;
    std::string key;          /* skin placeholder name */
    std::string name, path;
    std::string timeline_key; /* deform timelines keyed on this (linked meshes: the parent) */
    int slot;
    Color color;
    bool has_sequence;
    /* region */
    float x, y, rotation, scale_x, scale_y, width, height;
    float offsets[8]; /* the quad in bone space: BL, UL, UR, BR */
    /* vertex attachments (mesh, bounding box, path, clipping) */
    int vertex_count;
    std::vector<float> vertices;  /* unweighted: x, y per vertex; weighted: x, y, weight per bone entry */
    std::vector<int> bones;       /* weighted: per vertex, count then bone indices; empty if unweighted */
    std::vector<float> region_uvs;
    std::vector<int> triangles;
    int hull;
    /* resolved from the atlas */
    const SpAtlasRegion* region;
    std::vector<float> uvs; /* in page 0..1 (region: 8 values, BL UL UR BR) */
    int end_slot;
    bool weighted() const { return !bones.empty(); }
    int deform_length() const { return (int)(weighted() ? vertices.size() / 3 * 2 : vertices.size()); }
};

struct SpSkin {
    std::string name;
    std::vector<int> bones;
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

enum SpTimelineType {
    SPT_ROTATE, SPT_TRANSLATE, SPT_TRANSLATE_X, SPT_TRANSLATE_Y, SPT_SCALE, SPT_SCALE_X, SPT_SCALE_Y, SPT_SHEAR,
    SPT_SHEAR_X, SPT_SHEAR_Y, SPT_INHERIT,
    SPT_ATTACHMENT, SPT_RGBA, SPT_RGB, SPT_ALPHA, SPT_RGBA2, SPT_RGB2,
    SPT_DEFORM, SPT_SEQUENCE, SPT_DRAW_ORDER, SPT_EVENT, SPT_OTHER
};

struct SpTimeline {
    int type;
    int target;               /* bone or slot index */
    int values;               /* values per key (curve timelines) */
    std::vector<float> keys;  /* curve timelines: time, values... per key; others: times */
    SpCurves curves;
    std::vector<std::string> names;              /* attachment */
    std::vector<int> ints;                        /* inherit modes */
    std::vector<std::vector<int> > orders;        /* draw order (empty = setup) */
    std::vector<std::vector<float> > deforms;     /* deform */
    int skin;
    std::string attachment;                       /* deform: the timeline key */
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
    /* skeleton: the converter's JSON; atlas: may be NULL (no textures). */
    bool load(const Json& doc, const SpAtlas* atlas, std::string* error);
    int find_bone(const std::string& name) const;
    int find_slot(const std::string& name) const;
    const SpAnimation* find_animation(const std::string& name) const;
    const SpSkin* find_skin(const std::string& name) const;
    const SpSkin* default_skin() const;

    std::string version;
    float x, y, width, height;
    std::vector<SpBoneData> bones;
    std::vector<SpSlotData> slots;
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
    float x, y, rotation, scale_x, scale_y, shear_x, shear_y;
    int inherit;
    float a, b, c, d, world_x, world_y;
    bool active;
};

struct SpSlot {
    const SpSlotData* data;
    Color color;
    SpAttachment* attachment;
    std::vector<float> deform;
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
    void update_world_transform();
    /* World positions of an attachment's vertices (x, y pairs). */
    void world_vertices(const SpSlot& slot, std::vector<float>& out) const;

    std::vector<SpBone> bones;
    std::vector<SpSlot> slots;
    std::vector<int> draw_order;
    Color color;
    float x, y, scale_x, scale_y;

private:
    const SpSkeletonData* data_;
    const SpSkin* skin_;
};

/* AnimationState, for what the game does with it: tracks that play an
 * animation (looping or not), queued entries, and crossfades between
 * animations on a track (mix duration). */
class SpAnimationState {
public:
    SpAnimationState();
    void set_animation(int track, const SpAnimation* anim, bool loop);
    void add_animation(int track, const SpAnimation* anim, bool loop, float delay);
    void update(float dt);
    void apply(SpSkeleton& skel);
    const SpAnimation* current(int track) const;
    float default_mix;
    float time_scale;

private:
    struct Entry {
        const SpAnimation* anim;
        bool loop;
        float time, last_time, delay, mix_time, mix_duration, time_scale;
        Entry() : anim(NULL), loop(false), time(0), last_time(-1), delay(0), mix_time(0), mix_duration(0),
                  time_scale(1) {}
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
 * than 1 mixes from the current pose. */
void sp_apply_animation(const SpAnimation& anim, SpSkeleton& skel, float time, bool loop, float alpha);

} /* namespace spire */

#endif
