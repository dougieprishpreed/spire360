/* Script behaviours: C++ ports of the game's node scripts (C#), attached by
 * script path like Godot attaches the script. The game's C# UI code is not
 * in the logic build (patches.json ui.strip); each port lives in
 * engine/game and registers itself by the script's res:// path.
 *
 * Scripts can be switched off (spire_view --no-scripts) to compare a scene
 * with Godot's own render of it, which runs without the game's C#. */
#ifndef SPIRE_SCRIPT_H
#define SPIRE_SCRIPT_H

#include "../core/base.h"

namespace spire {

class Node;
class Control;

class ScriptBehavior {
public:
    virtual ~ScriptBehavior() {}
    /* After the scene that holds the node is instantiated (Godot: _ready,
     * children first). */
    virtual void ready(Node*) {}
    /* Each layout pass, after the control's rect is set (Godot:
     * NOTIFICATION_RESIZED; check for a change yourself). */
    virtual void resized(Control*) {}
    /* Each frame, before the node's own process (Godot: _Process). */
    virtual void process(Node*, double dt) { (void)dt; }
};

/* A factory runs while its node's scene is being instantiated, after the
 * node's properties are set (script exports are in node->extra). */
typedef ScriptBehavior* (*ScriptFactory)(Node* node);
/* In a factory: a resource reference from the node's scene ({"ext": ...},
 * {"sub": ...}) resolved; NULL outside a factory or if missing. */
class Resource;
class Json;
Resource* script_resource(const Json& v);
void register_script(const char* res_path, ScriptFactory factory);
ScriptBehavior* create_script(const std::string& res_path, Node* node); /* NULL if none */
void set_scripts_enabled(bool on);
bool scripts_enabled();

} /* namespace spire */

#endif
