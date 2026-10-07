/* SceneLibrary: scene and resource files from the pack (converter JSON),
 * the resources they reference, and instancing (Godot's
 * SceneState::instantiate: instanced scenes, inherited scenes, overrides of
 * nodes inside instances). */
#ifndef SPIRE_SCENE_H
#define SPIRE_SCENE_H

#include "node.h"

namespace spire {

class Pack;

struct SceneFile {
    std::string name; /* pack name */
    Json doc;
    std::map<std::string, Resource*> subs;
};

class SceneLibrary {
public:
    explicit SceneLibrary(const Pack* pack);
    ~SceneLibrary();

    const Pack* pack() const { return pack_; }

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

private:
    Resource* build(const std::string& cls, const Json& props, const SceneFile* ctx, const std::string& path);
    ThemeRes* build_theme(const Json& props, const SceneFile* ctx);
    Node* instantiate_file(const SceneFile* f, int depth, std::string* error);
    void init_default_theme();

    const Pack* pack_;
    std::map<std::string, SceneFile*> files_;
    std::map<std::string, Resource*> ext_;
    std::vector<Resource*> owned_; /* subs and inline objects */
    Json default_theme_doc_;
    ThemeRes* default_theme_;
    FontRes* default_font_;
};

} /* namespace spire */

#endif
