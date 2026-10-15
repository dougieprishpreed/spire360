#include "loc.h"

#include "../core/base.h"

#include <stdio.h>

namespace spire {

static std::string g_dir;
static std::map<std::string, Json>* g_tables = NULL;

void Loc::init(const std::string& data_dir, const std::string& lang) {
    /* The console wants backslashes: follow the folder's own. */
    std::string sep = data_dir.find('\\') != std::string::npos ? "\\" : "/";
    g_dir = data_dir + sep + "localization" + sep + lang + sep;
    if (!g_tables) g_tables = new std::map<std::string, Json>();
    g_tables->clear();
}

static const Json& table(const char* name) {
    if (!g_tables) g_tables = new std::map<std::string, Json>();
    std::map<std::string, Json>::iterator it = g_tables->find(name);
    if (it != g_tables->end()) return it->second;
    Json& t = (*g_tables)[name];
    std::string path = g_dir + name + ".json";
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) {
        log_warn("localization: no %s", path.c_str());
        return t;
    }
    std::string text;
    char buf[65536];
    size_t got;
    while ((got = fread(buf, 1, sizeof(buf), f)) > 0) text.append(buf, got);
    fclose(f);
    std::string err;
    t = Json::parse(text.c_str(), text.size(), &err);
    if (!err.empty()) log_warn("localization: %s: %s", path.c_str(), err.c_str());
    return t;
}

std::string Loc::get(const char* tbl, const char* key) {
    const Json& v = table(tbl)[key];
    return v.is_string() ? v.str() : std::string(tbl) + "." + key;
}

std::string Loc::format(const char* tbl, const char* key, const char* name, const std::string& value) {
    std::string s = get(tbl, key);
    std::string ph = std::string("{") + name + "}";
    for (size_t at = s.find(ph); at != std::string::npos; at = s.find(ph, at + value.size()))
        s.replace(at, ph.size(), value);
    return s;
}

} /* namespace spire */
