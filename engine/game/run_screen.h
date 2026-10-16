/* A ported screen of a run: built from one decision of the logic, it takes
 * the pad and answers with protocol commands. The game client shows one at
 * a time (combat has its own richer interface, CombatScreen). */
#ifndef SPIRE_RUN_SCREEN_H
#define SPIRE_RUN_SCREEN_H

#include "../core/json.h"

namespace spire {

class Node;

/* Controller state for one frame (Xbox 360 pad). */
struct PadState {
    enum {
        A = 1 << 0, B = 1 << 1, X = 1 << 2, Y = 1 << 3, LB = 1 << 4, RB = 1 << 5, LT = 1 << 6, RT = 1 << 7,
        UP = 1 << 8, DOWN = 1 << 9, LEFT = 1 << 10, RIGHT = 1 << 11, START = 1 << 12, BACK = 1 << 13
    };
    unsigned held;
    unsigned pressed; /* went down this frame */
    PadState() : held(0), pressed(0) {}
};

/* {"cmd":"action","action":name,"args":{args}}; args: "k":v pairs. */
std::string protocol_action(const char* name, const std::string& args = std::string());

class RunScreen {
public:
    virtual ~RunScreen() {}
    /* A new node tree for the decision (the caller owns it); NULL on error.
     * The screen object outlives the tree (it may drive its animations). */
    virtual Node* build(const Json& decision, std::string* error) = 0;
    /* One frame of pad input: a command for the logic, or "" for none. */
    virtual std::string input(const PadState& pad) = 0;
};

} /* namespace spire */

#endif
