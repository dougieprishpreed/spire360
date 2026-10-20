/* AnimationPlayer with its scene's Animation resources: value tracks
 * (continuous numbers, vectors and colours; anything else discrete),
 * bezier tracks and method tracks (queue_free), on paths relative to the
 * player's parent (root_node ".."), with autoplay, looping and the
 * animation_finished signal. Ported from Godot 4.5.1
 * (scene/animation/animation_player.cpp, animation_mixer.cpp,
 * scene/resources/animation.cpp; MIT, see THIRD_PARTY.md). */
#ifndef SPIRE_ANIMATION_H
#define SPIRE_ANIMATION_H

#include "node.h"

namespace spire {

class AnimationPlayer : public Node {
public:
    AnimationPlayer();
    virtual bool set(const std::string& key, const Json& v, const PropCtx& ctx);
    virtual void process(double dt);
    /* Plays an animation of its libraries from the start ("" the
     * autoplay one); false if it has none by that name. */
    bool play(const std::string& name);
    void stop() { playing_ = false; }
    bool is_playing() const { return playing_; }
    /* The animation that last ran to its end (animation_finished), until
     * the next play; "" if none. */
    const std::string& finished() const { return finished_; }

private:
    void apply(double from, double to, bool seeked);
    const Json* animation(const std::string& name) const;
    SceneLibrary* lib_;
    const SceneFile* file_;   /* where the libraries' resources are */
    const Json* libraries_;
    std::string autoplay_, current_, finished_;
    const Json* anim_; /* the current animation's props */
    double time_, length_;
    bool loop_, playing_, started_;
    std::map<int, int> discrete_; /* track -> key last set */
};

} /* namespace spire */

#endif
