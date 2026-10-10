#include "script.h"

#include <map>

namespace spire {

static std::map<std::string, ScriptFactory>& registry() {
    static std::map<std::string, ScriptFactory> r;
    return r;
}
static bool g_enabled = true;

void register_script(const char* res_path, ScriptFactory factory) { registry()[res_path] = factory; }

ScriptBehavior* create_script(const std::string& res_path, Node* node) {
    if (!g_enabled) return NULL;
    std::map<std::string, ScriptFactory>::iterator it = registry().find(res_path);
    return it == registry().end() ? NULL : it->second(node);
}

void set_scripts_enabled(bool on) { g_enabled = on; }
bool scripts_enabled() { return g_enabled; }

} /* namespace spire */
