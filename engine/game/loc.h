/* The game's localization tables (data/localization/<lang>/<table>.json,
 * written by the converter): flat key -> text maps. Texts keep the game's
 * markup (BBCode); {name} placeholders are filled by format(). SmartFormat
 * features beyond plain placeholders are not handled: texts that use them
 * come formatted from the logic instead. */
#ifndef SPIRE_LOC_H
#define SPIRE_LOC_H

#include "../core/json.h"

#include <map>

namespace spire {

class Loc {
public:
    /* dir: the data folder (holding localization/). */
    static void init(const std::string& data_dir, const std::string& lang = "eng");
    /* LocManager.SetLanguage: the tables of another language (eng, deu,
     * jpn, ...); keys it lacks come from English. */
    static void set_language(const std::string& lang);
    static const std::string& language();
    /* The text, or table.key when missing. */
    static std::string get(const char* table, const char* key);
    /* A file of the data folder (rel with '/'), with the folder's own
     * separators (user/settings.save, ...). */
    static std::string data_path(const char* rel);
    /* data/user/settings.save (the game's SettingsSave, which the logic
     * writes): null when there is none. */
    static Json settings_save();
    /* get() with one {name} placeholder filled. */
    static std::string format(const char* table, const char* key, const char* name, const std::string& value);
};

} /* namespace spire */

#endif
