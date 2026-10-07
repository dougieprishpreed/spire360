/* Minimal JSON DOM for scene files and the logic protocol. */
#ifndef SPIRE_JSON_H
#define SPIRE_JSON_H

#include <map>
#include <string>
#include <vector>

namespace spire {

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

private:
    friend class JsonParser;
    Type type_;
    double num_;
    bool bool_;
    std::string str_;
    std::vector<Json> arr_;
    std::vector<std::string> keys_;
    std::map<std::string, Json> obj_;
};

} /* namespace spire */

#endif
