/* Path2D and PathFollow2D: a Curve2D (cubic Bezier segments: each point's
 * position with its in / out handles) baked into points about bake_interval
 * apart (Godot's Curve2D::_bake), and the node that rides it (its progress
 * along the baked length, rotating with the curve). Used by the power card's
 * swoosh (NCardFlyPowerVfx), the Regent's sovereign blade and the event
 * VFX that move along paths. */
#ifndef SPIRE_PATH_2D_H
#define SPIRE_PATH_2D_H

#include "node.h"

namespace spire {

class Path2D : public Node2D {
public:
    Path2D();
    virtual bool set(const std::string& key, const Json& v, const PropCtx& ctx);
    int point_count() const { return (int)pos_.size(); }
    Vec2 point_position(int i) const { return pos_[(size_t)i]; }
    void set_point_position(int i, const Vec2& p);
    /* Curve2D.GetBakedLength / SampleBaked(WithRotation): the point at
     * offset along the curve (clamped) and, if rotation, the curve's angle
     * there (radians). */
    float baked_length();
    Vec2 sample_baked(float offset, float* rotation);

private:
    void bake();
    std::vector<Vec2> pos_, in_, out_;
    std::vector<Vec2> baked_;
    std::vector<float> dist_; /* the length up to each baked point */
    bool dirty_;
    float bake_interval_;
};

class PathFollow2D : public Node2D {
public:
    PathFollow2D();
    virtual bool set(const std::string& key, const Json& v, const PropCtx& ctx);
    virtual void process(double dt);
    float progress, progress_ratio, h_offset, v_offset;
    bool rotates, loop, use_ratio;
    /* places the node from progress (or progress_ratio) on its parent path */
    void update_transform();
};

} /* namespace spire */

#endif
