/* Minimal JSON DOM for scene files and the logic protocol. */
#ifndef SPIRE_JSON_H
#define SPIRE_JSON_H

#include <map>
#include <string>
#include <string.h>
#include <vector>

namespace spire {

/* A value. A string's text and an array's or object's contents are shared
 * by its copies, the contents copied (one level: the children shared in
 * turn) at the first write to one of them, so a copy costs one count: the
 * client copied each fight decision's whole tree five times a card played,
 * thousands of allocations each, the console's longest part of a decision.
 *
 * 16 bytes a value, an object's members as its keys and values in file
 * order with an index sorted by key: a parsed scene file took 13 times its
 * text (a value 48 bytes, a member a map node of its own with its key held
 * twice), and the files a run had used held 40 MB of the console's memory
 * by act 3 (rp108). */
class Json {
public:
    enum Type { NUL, BOOL, NUMBER, STRING, ARRAY, OBJECT };

    Json() : type_(NUL), bool_(false) { u_.num = 0; }
    Json(const Json& o);
    Json& operator=(const Json& o);
    ~Json();
    static Json parse(const char* text, size_t len, std::string* error);

    Type type() const { return (Type)type_; }
    bool is_null() const { return type_ == NUL; }
    bool is_number() const { return type_ == NUMBER; }
    bool is_string() const { return type_ == STRING; }
    bool is_array() const { return type_ == ARRAY; }
    bool is_object() const { return type_ == OBJECT; }
    bool is_bool() const { return type_ == BOOL; }

    double num(double fallback = 0) const {
        return type_ == NUMBER ? u_.num : (type_ == BOOL ? (bool_ ? 1 : 0) : fallback);
    }
    float f(float fallback = 0) const { return (float)num(fallback); }
    int i(int fallback = 0) const { return (int)num(fallback); }
    bool b(bool fallback = false) const { return type_ == BOOL ? bool_ : (type_ == NUMBER ? u_.num != 0 : fallback); }
    /* the text ("" for a value that is no string) */
    const std::string& str() const;

    size_t size() const;
    const Json& operator[](size_t idx) const;
    const Json& operator[](const char* key) const;
    const Json& get(const std::string& key) const { return (*this)[key.c_str()]; }
    bool has(const char* key) const;
    /* Object keys in file order. */
    const std::vector<std::string>& keys() const;

    /* Building (for the protocol). */
    static Json make_string(const std::string& s);
    static Json make_number(double v) { Json j; j.type_ = NUMBER; j.u_.num = v; return j; }
    static Json make_bool(bool v) { Json j; j.type_ = BOOL; j.bool_ = v; return j; }
    static Json make_array() { Json j; j.type_ = ARRAY; return j; }
    void set(const std::string& key, const Json& value);
    void append(const Json& value);
    /* Contents exchanged in place (no copy: C++03 has no moves, and a
     * copy of a tree copies every node in it). */
    void swap(Json& other);
    bool operator==(const Json& other) const;
    bool operator!=(const Json& other) const { return !(*this == other); }

private:
    friend class JsonParser;
    struct Body;
    struct Text;
    /* body for a write: made, or copied from the copies sharing it */
    Body* own();
    /* the member slot of key in an object's body, or -1 */
    static int find(const Body* b, const char* key);
    static void index_members(Body* b);
    static void release(Body* b);
    static void release(Text* t);
    void drop();
    unsigned char type_;
    bool bool_;
    union {
        double num;
        Text* text; /* STRING (never NULL) */
        Body* body; /* ARRAY, OBJECT: their elements (NULL: none yet) */
    } u_;
};

/* A string's text, shared by the copies of its value. */
struct Json::Text {
    volatile long refs;
    std::string s;
    explicit Text(const std::string& v) : refs(1), s(v) {}
};

/* An array's elements, or an object's values with its keys, in file order;
 * order: the members' places sorted by key (none for a few members). */
struct Json::Body {
    volatile long refs;
    std::vector<Json> arr;
    std::vector<std::string> keys;
    std::vector<unsigned> order;
    Body() : refs(1) {}
    Body(const Body& o) : refs(1), arr(o.arr), keys(o.keys), order(o.order) {}

private:
    Body& operator=(const Body&);
};

} /* namespace spire */

#endif
