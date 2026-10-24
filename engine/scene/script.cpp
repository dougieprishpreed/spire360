#include "script.h"

#include "node.h"

#include <map>

namespace spire {

static std::map<std::string, ScriptFactory>& registry() {
    static std::map<std::string, ScriptFactory> r;
    return r;
}
static bool g_enabled = true;
static bool g_count_missing = false;
static std::map<std::string, int> g_missing;

void register_script(const char* res_path, ScriptFactory factory) { registry()[res_path] = factory; }

ScriptBehavior* create_script(const std::string& res_path, Node* node) {
    if (!g_enabled) return NULL;
    std::map<std::string, ScriptFactory>::iterator it = registry().find(res_path);
    if (it == registry().end()) {
        /* counted at its first process unless the code that made it gave
         * it a behavior by then (the effects their spawners drive) */
        if (g_count_missing && !res_path.empty() && node) node->flags |= NF_SCRIPT_UNPORTED;
        return NULL;
    }
    return it->second(node);
}

void count_missing_scripts(bool on) { g_count_missing = on; }
void note_unported_script(const std::string& res_path) {
    if (g_count_missing && !res_path.empty()) ++g_missing[res_path];
}
const std::map<std::string, int>& missing_scripts() { return g_missing; }

void set_scripts_enabled(bool on) { g_enabled = on; }
bool scripts_enabled() { return g_enabled; }

} /* namespace spire */
