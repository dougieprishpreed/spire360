#include "json.h"
#include "base.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_MSC_VER) && _MSC_VER < 1900
#define vsnprintf _vsnprintf /* VS2010: may not terminate; format() terminates */
#endif

namespace spire {

static const Json& null_json() {
    static Json n;
    return n;
}

const Json& Json::operator[](size_t idx) const {
    if (type_ != ARRAY || idx >= arr_.size()) return null_json();
    return arr_[idx];
}

const Json& Json::operator[](const char* key) const {
    if (type_ != OBJECT) return null_json();
    std::map<std::string, Json>::const_iterator it = obj_.find(key);
    return it == obj_.end() ? null_json() : it->second;
}

bool Json::has(const char* key) const {
    return type_ == OBJECT && obj_.find(key) != obj_.end();
}

void Json::set(const std::string& key, const Json& value) {
    if (type_ != OBJECT) { *this = Json(); type_ = OBJECT; }
    if (obj_.find(key) == obj_.end()) keys_.push_back(key);
    obj_[key] = value;
}

void Json::append(const Json& value) {
    if (type_ != ARRAY) { *this = Json(); type_ = ARRAY; }
    arr_.push_back(value);
}

bool Json::operator==(const Json& other) const {
    if (type_ != other.type_) return false;
    switch (type_) {
    case BOOL: return bool_ == other.bool_;
    case NUMBER: return num_ == other.num_;
    case STRING: return str_ == other.str_;
    case ARRAY: return arr_ == other.arr_;
    case OBJECT: return obj_ == other.obj_;
    default: return true;
    }
}

class JsonParser {
public:
    JsonParser(const char* s, size_t n) : s_(s), end_(s + n), p_(s), err_() {}

    bool parse(Json& out) {
        ws();
        if (!value(out, 0)) return false;
        ws();
        if (p_ != end_) return fail("trailing characters");
        return true;
    }
    const std::string& error() const { return err_; }

private:
    const char* s_;
    const char* end_;
    const char* p_;
    std::string err_;

    bool fail(const char* msg) {
        if (err_.empty()) err_ = format("%s at offset %d", msg, (int)(p_ - s_));
        return false;
    }
    void ws() {
        while (p_ < end_ && (*p_ == ' ' || *p_ == '\t' || *p_ == '\n' || *p_ == '\r')) ++p_;
    }
    bool lit(const char* w) {
        size_t n = strlen(w);
        if ((size_t)(end_ - p_) < n || memcmp(p_, w, n) != 0) return fail("bad literal");
        p_ += n;
        return true;
    }
    static void utf8(std::string& o, unsigned cp) {
        if (cp < 0x80) o += (char)cp;
        else if (cp < 0x800) { o += (char)(0xC0 | (cp >> 6)); o += (char)(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) { o += (char)(0xE0 | (cp >> 12)); o += (char)(0x80 | ((cp >> 6) & 0x3F)); o += (char)(0x80 | (cp & 0x3F)); }
        else { o += (char)(0xF0 | (cp >> 18)); o += (char)(0x80 | ((cp >> 12) & 0x3F)); o += (char)(0x80 | ((cp >> 6) & 0x3F)); o += (char)(0x80 | (cp & 0x3F)); }
    }
    bool hex4(unsigned& v) {
        if (end_ - p_ < 4) return fail("short \\u escape");
        v = 0;
        for (int k = 0; k < 4; ++k) {
            char c = *p_++;
            v <<= 4;
            if (c >= '0' && c <= '9') v |= (unsigned)(c - '0');
            else if (c >= 'a' && c <= 'f') v |= (unsigned)(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') v |= (unsigned)(c - 'A' + 10);
            else return fail("bad \\u escape");
        }
        return true;
    }
    bool string(std::string& o) {
        ++p_; /* opening quote */
        while (p_ < end_ && *p_ != '"') {
            char c = *p_++;
            if (c != '\\') { o += c; continue; }
            if (p_ >= end_) return fail("bad escape");
            char e = *p_++;
            switch (e) {
            case 'n': o += '\n'; break;
            case 't': o += '\t'; break;
            case 'r': o += '\r'; break;
            case 'b': o += '\b'; break;
            case 'f': o += '\f'; break;
            case 'u': {
                unsigned cp;
                if (!hex4(cp)) return false;
                if (cp >= 0xD800 && cp < 0xDC00 && end_ - p_ >= 6 && p_[0] == '\\' && p_[1] == 'u') {
                    unsigned lo;
                    p_ += 2;
                    if (!hex4(lo)) return false;
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                }
                utf8(o, cp);
                break;
            }
            default: o += e; break;
            }
        }
        if (p_ >= end_) return fail("unterminated string");
        ++p_;
        return true;
    }
    bool value(Json& o, int depth) {
        if (depth > 200) return fail("nesting too deep");
        if (p_ >= end_) return fail("unexpected end");
        char c = *p_;
        if (c == '"') { o.type_ = Json::STRING; return string(o.str_); }
        if (c == '{') {
            o.type_ = Json::OBJECT;
            ++p_;
            ws();
            if (p_ < end_ && *p_ == '}') { ++p_; return true; }
            for (;;) {
                ws();
                if (p_ >= end_ || *p_ != '"') return fail("expected key");
                std::string key;
                if (!string(key)) return false;
                ws();
                if (p_ >= end_ || *p_ != ':') return fail("expected ':'");
                ++p_;
                ws();
                Json v;
                if (!value(v, depth + 1)) return false;
                if (o.obj_.find(key) == o.obj_.end()) o.keys_.push_back(key);
                o.obj_[key] = v;
                ws();
                if (p_ < end_ && *p_ == ',') { ++p_; continue; }
                if (p_ < end_ && *p_ == '}') { ++p_; return true; }
                return fail("expected ',' or '}'");
            }
        }
        if (c == '[') {
            o.type_ = Json::ARRAY;
            ++p_;
            ws();
            if (p_ < end_ && *p_ == ']') { ++p_; return true; }
            for (;;) {
                ws();
                o.arr_.push_back(Json());
                if (!value(o.arr_.back(), depth + 1)) return false;
                ws();
                if (p_ < end_ && *p_ == ',') { ++p_; continue; }
                if (p_ < end_ && *p_ == ']') { ++p_; return true; }
                return fail("expected ',' or ']'");
            }
        }
        if (c == 't') { o.type_ = Json::BOOL; o.bool_ = true; return lit("true"); }
        if (c == 'f') { o.type_ = Json::BOOL; o.bool_ = false; return lit("false"); }
        if (c == 'n') { o.type_ = Json::NUL; return lit("null"); }
        if (c == '-' || (c >= '0' && c <= '9')) {
            char buf[64];
            size_t n = 0;
            while (p_ < end_ && n < sizeof(buf) - 1 &&
                   (*p_ == '-' || *p_ == '+' || *p_ == '.' || *p_ == 'e' || *p_ == 'E' || (*p_ >= '0' && *p_ <= '9')))
                buf[n++] = *p_++;
            buf[n] = 0;
            o.type_ = Json::NUMBER;
            o.num_ = strtod(buf, NULL);
            return true;
        }
        return fail("unexpected character");
    }
};

Json Json::parse(const char* text, size_t len, std::string* error) {
    Json out;
    JsonParser p(text, len);
    if (!p.parse(out)) {
        if (error) *error = p.error();
        return Json();
    }
    return out;
}

std::string format(const char* fmt, ...) {
    char buf[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    buf[sizeof(buf) - 1] = 0;
    return buf;
}

static LogSink g_log_sink = NULL;

void set_log_sink(LogSink sink) { g_log_sink = sink; }

static void log_line(const char* prefix, const char* fmt, va_list ap) {
    char buf[1024];
    size_t n = strlen(prefix);
    memcpy(buf, prefix, n);
    vsnprintf(buf + n, sizeof(buf) - n, fmt, ap);
    buf[sizeof(buf) - 1] = 0;
    if (g_log_sink) {
        g_log_sink(buf);
    } else {
        fputs(buf, stderr);
        fputc('\n', stderr);
    }
}

void log_info(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    log_line("", fmt, ap);
    va_end(ap);
}

void log_warn(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    log_line("[warn] ", fmt, ap);
    va_end(ap);
}

static LoadClock g_load_clock = NULL;

void set_load_trace(LoadClock clock) { g_load_clock = clock; }

double load_trace_start() { return g_load_clock ? g_load_clock() : -1.0; }

void load_traced(const char* kind, const std::string& name, double started) {
    if (g_load_clock && started >= 0) log_info("load %s %.2f ms %s", kind, g_load_clock() - started, name.c_str());
}

} /* namespace spire */
