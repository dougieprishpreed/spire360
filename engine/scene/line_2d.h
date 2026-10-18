/* Line2D geometry used by the game's projectile and card trails. */
#ifndef SPIRE_LINE_2D_H
#define SPIRE_LINE_2D_H

#include "particles.h"

namespace spire {

class Line2D : public Node2D {
public:
    enum LineJointMode { LINE_JOINT_SHARP, LINE_JOINT_BEVEL, LINE_JOINT_ROUND };
    enum LineCapMode { LINE_CAP_NONE, LINE_CAP_BOX, LINE_CAP_ROUND };
    enum LineTextureMode { LINE_TEXTURE_NONE, LINE_TEXTURE_TILE, LINE_TEXTURE_STRETCH };
    Line2D() : width(10), texture(NULL), texture_mode(0), closed(false), begin_cap(0), end_cap(0),
        joint_mode(0), sharp_limit(2), round_precision(8) {}
    std::vector<Vec2> points;
    float width;
    Color color;
    Texture2DRes* texture;
    int texture_mode;
    bool closed;
    int begin_cap, end_cap;
    int joint_mode;
    float sharp_limit;
    int round_precision;
    CurveData width_curve;
    GradientData gradient;
    virtual bool set(const std::string& key, const Json& value, const PropCtx& ctx);
    virtual void draw(Canvas& canvas);
};

} /* namespace spire */
#endif
