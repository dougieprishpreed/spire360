#include "loc.h"

#include "../core/base.h"

#include <stdio.h>

namespace spire {

static std::string g_dir, g_lang = "eng";
static std::map<std::string, Json>* g_tables = NULL;
static std::map<std::string, Json>* g_eng_tables = NULL; /* when another language is on */

static std::string g_data, g_sep = "/";

std::string Loc::data_path(const char* rel) {
    std::string p = g_data + g_sep;
    for (const char* c = rel; *c; ++c) p += *c == '/' ? g_sep : std::string(1, *c);
    return p;
}

Json Loc::settings_save() {
    std::string path = data_path("user/settings.save");
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return Json();
    std::string text;
    char buf[8192];
    size_t got;
    while ((got = fread(buf, 1, sizeof(buf), f)) > 0) text.append(buf, got);
    fclose(f);
    std::string err;
    return Json::parse(text.c_str(), text.size(), &err);
}

void Loc::init(const std::string& data_dir, const std::string& lang) {
    /* The console wants backslashes: follow the folder's own. */
    std::string sep = data_dir.find('\\') != std::string::npos ? "\\" : "/";
    g_data = data_dir;
    g_sep = sep;
    set_language(lang);
}

void Loc::set_language(const std::string& lang) {
    g_lang = lang.empty() ? std::string("eng") : lang;
    if (!g_tables) g_tables = new std::map<std::string, Json>();
    if (!g_eng_tables) g_eng_tables = new std::map<std::string, Json>();
    g_tables->clear();
    g_dir = g_data + g_sep + "localization" + g_sep + g_lang + g_sep;
}

const std::string& Loc::language() { return g_lang; }

static const Json& table_of(std::map<std::string, Json>*& tables, const std::string& dir, const char* name) {
    if (!tables) tables = new std::map<std::string, Json>();
    std::map<std::string, Json>::iterator it = tables->find(name);
    if (it != tables->end()) return it->second;
    Json& t = (*tables)[name];
    std::string path = dir + name + ".json";
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

static const Json& table(const char* name) { return table_of(g_tables, g_dir, name); }

std::string Loc::get(const char* tbl, const char* key) {
    const Json& v = table(tbl)[key];
    if (v.is_string()) return v.str();
    if (g_lang != "eng") {
        std::string dir = g_data + g_sep + "localization" + g_sep + "eng" + g_sep;
        const Json& t = table_of(g_eng_tables, dir, tbl);
        if (t[key].is_string()) return t[key].str();
    }
    return std::string(tbl) + "." + key;
}

std::string Loc::format(const char* tbl, const char* key, const char* name, const std::string& value) {
    std::string s = get(tbl, key);
    std::string ph = std::string("{") + name + "}";
    for (size_t at = s.find(ph); at != std::string::npos; at = s.find(ph, at + value.size()))
        s.replace(at, ph.size(), value);
    return s;
}

} /* namespace spire */
