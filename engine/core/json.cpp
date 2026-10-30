#include "json.h"
#include "base.h"
#include "thread.h"

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

Json::Json(const Json& o) : type_(o.type_), num_(o.num_), bool_(o.bool_), str_(o.str_), body_(o.body_) {
    if (body_) atomic_add(&body_->refs, 1);
}

Json& Json::operator=(const Json& o) {
    if (o.body_) atomic_add(&o.body_->refs, 1);
    Body* old = body_;
    type_ = o.type_;
    num_ = o.num_;
    bool_ = o.bool_;
    str_ = o.str_;
    body_ = o.body_;
    if (old) release(old);
    return *this;
}

Json::~Json() {
    if (body_) release(body_);
}

void Json::release(Body* b) {
    if (atomic_add(&b->refs, -1) == 0) delete b;
}

Json::Body* Json::own() {
    if (!body_) {
        body_ = new Body();
    } else if (body_->refs > 1) {
        Body* mine = new Body(*body_);
        release(body_);
        body_ = mine;
    }
    return body_;
}

static const std::vector<std::string>& no_keys() {
    static std::vector<std::string> none;
    return none;
}

size_t Json::size() const {
    if (!body_) return 0;
    return type_ == ARRAY ? body_->arr.size() : (type_ == OBJECT ? body_->keys.size() : 0);
}

const std::vector<std::string>& Json::keys() const {
    return type_ == OBJECT && body_ ? body_->keys : no_keys();
}

const Json& Json::operator[](size_t idx) const {
    if (type_ != ARRAY || !body_ || idx >= body_->arr.size()) return null_json();
    return body_->arr[idx];
}

const Json& Json::operator[](const char* key) const {
    if (type_ != OBJECT || !body_) return null_json();
    std::map<JsonKey, Json>::const_iterator it = body_->obj.find(key);
    return it == body_->obj.end() ? null_json() : it->second;
}

bool Json::has(const char* key) const {
    return type_ == OBJECT && body_ && body_->obj.find(key) != body_->obj.end();
}

void Json::set(const std::string& key, const Json& value) {
    /* value may be one of this value's own members (or below them): held
     * before the write copies or replaces them */
    Json keep(value);
    if (type_ != OBJECT) { *this = Json(); type_ = OBJECT; }
    Body* b = own();
    std::map<JsonKey, Json>::iterator it = b->obj.find(key.c_str());
    if (it == b->obj.end()) {
        b->keys.push_back(key);
        b->obj.insert(std::make_pair(JsonKey(key), keep));
    } else {
        it->second = keep;
    }
}

void Json::append(const Json& value) {
    Json keep(value);
    if (type_ != ARRAY) { *this = Json(); type_ = ARRAY; }
    own()->arr.push_back(keep);
}

void Json::swap(Json& other) {
    Type t = type_;
    type_ = other.type_;
    other.type_ = t;
    double n = num_;
    num_ = other.num_;
    other.num_ = n;
    bool b = bool_;
    bool_ = other.bool_;
    other.bool_ = b;
    str_.swap(other.str_);
    Body* body = body_;
    body_ = other.body_;
    other.body_ = body;
}

bool Json::operator==(const Json& other) const {
    if (type_ != other.type_) return false;
    switch (type_) {
    case BOOL: return bool_ == other.bool_;
    case NUMBER: return num_ == other.num_;
    case STRING: return str_ == other.str_;
    case ARRAY:
    case OBJECT: {
        if (body_ == other.body_) return true;
        if (size() != other.size()) return false;
        if (!body_ || !other.body_) return true; /* both empty */
        return type_ == ARRAY ? body_->arr == other.body_->arr : body_->obj == other.body_->obj;
    }
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
    static void grow(std::vector<Json>& v, size_t capacity) {
        std::vector<Json> bigger(capacity);
        for (size_t i = 0; i < v.size(); ++i) bigger[i].swap(v[i]);
        v.swap(bigger);
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
                /* parsed into its place: assigning a parsed value copied
                 * its whole subtree, at every level of the tree */
                Json::Body* b = o.own();
                std::map<JsonKey, Json>::iterator slot = b->obj.find(key.c_str());
                if (slot == b->obj.end()) {
                    b->keys.push_back(key);
                    slot = b->obj.insert(std::make_pair(JsonKey(key), Json())).first;
                } else {
                    Json().swap(slot->second);
                }
                if (!value(slot->second, depth + 1)) return false;
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
            /* grown by swapping the parsed elements over (a vector's
             * growth copied every element and its subtree) */
            size_t n = 0;
            std::vector<Json>& arr = o.own()->arr;
            for (;;) {
                ws();
                if (n == arr.size()) grow(arr, n ? n * 2 : 4);
                if (!value(arr[n], depth + 1)) return false;
                ++n;
                ws();
                if (p_ < end_ && *p_ == ',') { ++p_; continue; }
                if (p_ < end_ && *p_ == ']') {
                    ++p_;
                    arr.resize(n);
                    return true;
                }
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
static bool g_hold = false;
/* held loads (set_load_trace's hold): made with the first call, before
 * any other thread loads */
static Mutex* g_held_lock = NULL;
static std::vector<HeldLoad>* g_held = NULL;

void set_load_trace(LoadClock clock, bool hold) {
    if (hold && !g_held_lock) {
        g_held_lock = new Mutex();
        g_held = new std::vector<HeldLoad>();
    }
    g_load_clock = NULL;
    if (g_held_lock) {
        g_held_lock->lock();
        g_held->clear();
        g_held_lock->unlock();
    }
    g_hold = hold && g_held_lock;
    g_load_clock = clock;
}

double load_trace_start() { return g_load_clock ? g_load_clock() : -1.0; }

static PhaseMark g_phase_mark = NULL;
void set_phase_mark(PhaseMark mark) { g_phase_mark = mark; }
void phase_mark(const char* what) {
    if (g_phase_mark) g_phase_mark(what);
}

void load_traced(const char* kind, const std::string& name, double started) {
    if (!g_load_clock || started < 0) return;
    double ms = g_load_clock() - started;
    if (!g_hold) {
        log_info("load %s %.2f ms %s", kind, ms, name.c_str());
        return;
    }
    HeldLoad h;
    h.kind = kind;
    h.name = name;
    h.ms = ms;
    g_held_lock->lock();
    /* a frame's worth: a long load screen's thousands need not all stay */
    if (g_held->size() < 4096) g_held->push_back(h);
    g_held_lock->unlock();
}

void take_held_loads(std::vector<HeldLoad>& out) {
    out.clear();
    if (!g_held_lock) return;
    g_held_lock->lock();
    out.swap(*g_held);
    g_held_lock->unlock();
}

} /* namespace spire */
