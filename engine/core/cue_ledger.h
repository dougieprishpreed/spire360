/* What the client does with the logic's cues, for the presentation audit
 * (spire_gaps' "cues"; tools/presentation_audit.py): each kind of cue
 * ("ui:<Type.Method>" for the calls into the game's UI layer, the cue type
 * for the others) counted as the responses bring it (seen), as a screen
 * plays it (used) and as one leaves it (dropped: no handler took it there).
 * A kind nothing ever uses is presentation the client leaves out. Off unless
 * a tool enables it; the game never pays for it. */
#ifndef SPIRE_CORE_CUE_LEDGER_H
#define SPIRE_CORE_CUE_LEDGER_H

#include "json.h"

#include <map>
#include <string>

namespace spire {

class CueLedger {
public:
    struct Counts {
        int seen, used, dropped;
        std::string first; /* where it was first seen (the tool's label) */
        Counts() : seen(0), used(0), dropped(0) {}
    };
    static bool enabled;
    static std::string where;

    /* "ui:NFoo.Bar" or the cue's type */
    static std::string key(const Json& cue);
    /* a response's steps, as it arrives */
    static void seen(const Json& steps);
    static void used(const Json& cue);
    static void used_key(const std::string& key);
    static void dropped(const Json& cue);
    /* a cue first counted as used that its handler then left after all */
    static void retract(const Json& cue);
    static const std::map<std::string, Counts>& counts() { return counts_; }

private:
    static std::map<std::string, Counts> counts_;
};

} /* namespace spire */

#endif
