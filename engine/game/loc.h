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
    /* The text, or table.key when missing. */
    static std::string get(const char* table, const char* key);
    /* get() with one {name} placeholder filled. */
    static std::string format(const char* table, const char* key, const char* name, const std::string& value);
};

} /* namespace spire */

#endif
