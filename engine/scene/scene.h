/* SceneLibrary: scene and resource files from the pack (converter JSON),
 * the resources they reference, and instancing (Godot's
 * SceneState::instantiate: instanced scenes, inherited scenes, overrides of
 * nodes inside instances). */
#ifndef SPIRE_SCENE_H
#define SPIRE_SCENE_H

#include "node.h"

#include <deque>
#include <set>

namespace spire {

class Pack;
class GameAudio;
class Canvas;
class SpineDataRes;

struct SceneFile {
    std::string name; /* pack name */
    Json doc;
    std::map<std::string, Resource*> subs;
};

class SceneLibrary {
public:
    explicit SceneLibrary(const Pack* pack);
    ~SceneLibrary();
    /* The library the scene system uses (BBCode [img] / [font] paths). */
    static SceneLibrary* current();

    const Pack* pack() const { return pack_; }
    GameAudio* audio() const { return audio_; }
    void set_audio(GameAudio* audio) { audio_ = audio; }

    /* Loads (and caches) a scene/resource file. NULL if missing. */
    const SceneFile* file(const std::string& name);

    /* A new node tree for a scene; the caller owns it. */
    Node* instantiate(const std::string& scene, std::string* error);

    /* Resources: by pack name, sub-resource of a file, or an inline object. */
    Resource* ext(const std::string& name);
    Resource* sub(const SceneFile* f, const std::string& id);
    Resource* object(const Json& v, const SceneFile* ctx);
    /* Any reference value ({"ext"}, {"sub"}, inline object); NULL otherwise. */
    Resource* resolve(const Json& v, const SceneFile* ctx);

    ThemeRes* default_theme() { return default_theme_; }
    FontRes* default_font() { return default_font_; }

    /* Factory for Godot classes; unknown classes fall back to their nearest
     * known base (Control, Node2D, Node). */
    static Node* create_node(const std::string& type);

    /* Unsupported things met while loading, for the viewer's report:
     * "kind name" -> count. */
    std::map<std::string, int> unsupported;
    void note(const std::string& what) { ++unsupported[what]; }
    /* The scene files loaded so far (instanced, or instanced inside one),
     * for spire_gaps' scenes the client never shows. */
    void loaded_scenes(std::vector<std::string>& out) const {
        for (std::map<std::string, SceneFile*>::const_iterator it = files_.begin(); it != files_.end(); ++it)
            if (it->second) out.push_back(it->first);
    }

    /* PreloadManager's sets (the logic's "preload": the run's assets, the
     * act's): their scene and resource files read and their textures
     * loaded ahead of their first use, a few each frame (warm_step, from
     * SceneTree::render), so a first card play does not stop to read
     * them. Off: nothing is warmed (spire_gaps counts the scenes shown). */
    void warm(const Json& paths);
    /* Ahead of the queue (and what they reference with them): the next
     * screen's own files, read while the logic works on it (a run's first
     * screen, Neow). */
    void warm_first(const std::vector<std::string>& paths);
    void warm_step(Canvas& canvas);
    bool warming() const { return !warm_queue_.empty() || !warm_first_.empty() || !warm_reading_.empty(); }
    bool warming_first() const { return !warm_first_.empty(); }
    void set_warm_enabled(bool on) { warm_enabled_ = on; }
    /* A clock (ms) and each frame's share of it: warm_step reads until the
     * share is spent (one file at least) instead of a fixed count, and logs
     * each batch's total as it ends. The console's reads are far slower. */
    void set_warm_clock(LoadClock clock, double ms_per_frame) {
        warm_clock_ = clock;
        warm_share_ = ms_per_frame;
    }
    /* A larger share while nothing is shown (the embark's black) */
    double warm_share() const { return warm_share_; }
    void set_warm_share(double ms_per_frame) { warm_share_ = ms_per_frame; }

    /* What the library holds on the heap (the console's memory): the
     * parsed scene files (kept: their resources point into them) and the
     * Spine skeletons' data, which the unused ones give back past a budget
     * (trim_skeletons, each frame from warm_step: the oldest unused first;
     * a skeleton used again is read again). */
    struct MemoryStats {
        size_t scene_bytes, skeleton_bytes;
        unsigned scene_files, skeletons, skeletons_used, skeletons_evicted;
    };
    MemoryStats memory_stats() const;
    void skeleton_loaded(SpineDataRes* r);
    void set_skeleton_budget(size_t bytes) { skeleton_budget_ = bytes; }
    void trim_skeletons();

private:
    Resource* build(const std::string& cls, const Json& props, const SceneFile* ctx, const std::string& path);
    ThemeRes* build_theme(const Json& props, const SceneFile* ctx);
    Node* instantiate_file(const SceneFile* f, int depth, std::string* error);
    void init_default_theme();

    const Pack* pack_;
    GameAudio* audio_; /* service owned by the application */
    std::map<std::string, SceneFile*> files_;
    std::map<std::string, Resource*> ext_;
    std::vector<Resource*> owned_; /* subs and inline objects */
    std::deque<std::string> warm_queue_;
    std::deque<std::string> warm_first_;
    std::set<std::string> warmed_;
    /* textures the canvas's reader is reading (Canvas::prefetch_async):
     * made as each is in, a frame's share at a time */
    std::vector<std::string> warm_reading_;
    bool warm_enabled_;
    LoadClock warm_clock_;
    double warm_share_, warm_spent_;
    int warm_files_, warm_textures_, warm_frames_;
    size_t scene_bytes_, skeleton_bytes_, skeleton_budget_;
    std::vector<SpineDataRes*> skeletons_; /* the parsed ones */
    unsigned skeletons_evicted_;
    Json default_theme_doc_;
    ThemeRes* default_theme_;
    FontRes* default_font_;
};

} /* namespace spire */

#endif
