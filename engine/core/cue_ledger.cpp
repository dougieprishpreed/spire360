#include "cue_ledger.h"

#include <stdio.h>
#include <stdlib.h>

namespace spire {

/* SPIRE_CUE_TRACE in the environment (a diagnostic, off by default): each
 * cue played, dropped or taken back, on stderr as it happens. The console
 * has no environment (the XDK's Release CRT has no getenv). */
static bool trace_on() {
#if defined(_XBOX) || defined(__XSDK__)
    return false;
#else
    static int on = -1;
    if (on < 0) on = getenv("SPIRE_CUE_TRACE") ? 1 : 0;
    return on != 0;
#endif
}

static void trace(const char* what, const std::string& key) {
    if (trace_on()) fprintf(stderr, "cue %s %s\n", what, key.c_str());
}

bool CueLedger::enabled = false;
std::string CueLedger::where;
std::map<std::string, CueLedger::Counts> CueLedger::counts_;

std::string CueLedger::key(const Json& cue) {
    const std::string& t = cue["t"].str();
    if (t == "ui") return "ui:" + cue["call"].str();
    return t;
}

void CueLedger::seen(const Json& steps) {
    if (!enabled) return;
    for (size_t i = 0; i < steps.size(); ++i)
        for (size_t k = 0; k < steps[i].size(); ++k) {
            Counts& c = counts_[key(steps[i][k])];
            if (c.seen == 0) c.first = where;
            ++c.seen;
        }
}

void CueLedger::used(const Json& cue) {
    if (trace_on()) trace("used", key(cue));
    if (enabled) ++counts_[key(cue)].used;
}

void CueLedger::used_key(const std::string& k) {
    if (enabled) ++counts_[k].used;
}

void CueLedger::dropped(const Json& cue) {
    if (trace_on()) trace("dropped", key(cue));
    if (enabled) ++counts_[key(cue)].dropped;
}

void CueLedger::retract(const Json& cue) {
    if (trace_on()) trace("retracted", key(cue));
    if (!enabled) return;
    Counts& c = counts_[key(cue)];
    if (c.used > 0) --c.used;
    ++c.dropped;
}

} /* namespace spire */
