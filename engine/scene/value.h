/* Reading Godot values in the converter's JSON encoding
 * (converter/spire_convert/scenes.py): {"t": "Vector2", "v": [x, y]}, ... */
#ifndef SPIRE_VALUE_H
#define SPIRE_VALUE_H

#include "../core/base.h"
#include "../core/json.h"

namespace spire {

float jv_float(const Json& v, float fallback = 0.0f); /* numbers, {"f": "inf"} */
bool jv_vec2(const Json& v, Vec2& out);               /* Vector2, Vector2i, Size2 */
bool jv_color(const Json& v, Color& out);
bool jv_rect2(const Json& v, Rect2& out);              /* Rect2, Rect2i */
const char* jv_ctor(const Json& v);                    /* "Vector2", ... or "" */
bool jv_is_ext(const Json& v);                         /* {"ext": name|null, "type"} */
bool jv_is_sub(const Json& v);                         /* {"sub": id} */
bool jv_is_object(const Json& v);                      /* {"t": "Object", "class", "props"} */
std::string jv_nodepath(const Json& v);                /* {"np": "a/b"} or a plain string */

} /* namespace spire */

#endif
