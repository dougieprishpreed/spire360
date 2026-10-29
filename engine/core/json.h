/* Minimal JSON DOM for scene files and the logic protocol. */
#ifndef SPIRE_JSON_H
#define SPIRE_JSON_H

#include <map>
#include <string>
#include <string.h>
#include <vector>

namespace spire {

/* A Json object's key: compared with a C string as it is, so a lookup
 * makes no std::string (C++03's map::find takes only its key type, and the
 * string made for each lookup was an allocation past 15 characters: the
 * audio's ids, the materials' property names, every frame). A key kept in
 * the map owns its text; one made from a char* for a lookup points at it. */
class JsonKey {
public:
    JsonKey(const char* text) : ref_(text) {}
    JsonKey(const std::string& text) : own_(text), ref_(NULL) {}
    JsonKey(const JsonKey& o) : own_(o.ref_ ? std::string(o.ref_) : o.own_), ref_(NULL) {}
    JsonKey& operator=(const JsonKey& o) {
        own_ = o.ref_ ? std::string(o.ref_) : o.own_;
        ref_ = NULL;
        return *this;
    }
    const char* c_str() const { return ref_ ? ref_ : own_.c_str(); }
    bool operator<(const JsonKey& o) const { return strcmp(c_str(), o.c_str()) < 0; }
    bool operator==(const JsonKey& o) const { return strcmp(c_str(), o.c_str()) == 0; }

private:
    std::string own_;
    const char* ref_;
};

class Json {
public:
    enum Type { NUL, BOOL, NUMBER, STRING, ARRAY, OBJECT };

    Json() : type_(NUL), num_(0), bool_(false) {}
    static Json parse(const char* text, size_t len, std::string* error);

    Type type() const { return type_; }
    bool is_null() const { return type_ == NUL; }
    bool is_number() const { return type_ == NUMBER; }
    bool is_string() const { return type_ == STRING; }
    bool is_array() const { return type_ == ARRAY; }
    bool is_object() const { return type_ == OBJECT; }
    bool is_bool() const { return type_ == BOOL; }

    double num(double fallback = 0) const { return type_ == NUMBER ? num_ : (type_ == BOOL ? (bool_ ? 1 : 0) : fallback); }
    float f(float fallback = 0) const { return (float)num(fallback); }
    int i(int fallback = 0) const { return (int)num(fallback); }
    bool b(bool fallback = false) const { return type_ == BOOL ? bool_ : (type_ == NUMBER ? num_ != 0 : fallback); }
    const std::string& str() const { return str_; }

    size_t size() const { return type_ == ARRAY ? arr_.size() : (type_ == OBJECT ? keys_.size() : 0); }
    const Json& operator[](size_t idx) const;
    const Json& operator[](const char* key) const;
    const Json& get(const std::string& key) const { return (*this)[key.c_str()]; }
    bool has(const char* key) const;
    /* Object keys in file order. */
    const std::vector<std::string>& keys() const { return keys_; }

    /* Building (for the protocol). */
    static Json make_string(const std::string& s) { Json j; j.type_ = STRING; j.str_ = s; return j; }
    static Json make_number(double v) { Json j; j.type_ = NUMBER; j.num_ = v; return j; }
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
    Type type_;
    double num_;
    bool bool_;
    std::string str_;
    std::vector<Json> arr_;
    std::vector<std::string> keys_;
    std::map<JsonKey, Json> obj_;
};

} /* namespace spire */

#endif
