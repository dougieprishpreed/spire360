#include "json.h"
#include "base.h"
#include "thread.h"

#include <algorithm>
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

static const std::string& empty_string() {
    static std::string none;
    return none;
}

Json::Json(const Json& o) : type_(o.type_), bool_(o.bool_) {
    u_ = o.u_;
    if (type_ == STRING) atomic_add(&u_.text->refs, 1);
    else if ((type_ == ARRAY || type_ == OBJECT) && u_.body) atomic_add(&u_.body->refs, 1);
}

Json& Json::operator=(const Json& o) {
    /* o read (and counted) before this value's contents go: it may be one
     * of them (x = x["child"]) */
    unsigned char type = o.type_;
    bool flag = o.bool_;
    Json copy_of = o;
    drop();
    type_ = type;
    bool_ = flag;
    u_ = copy_of.u_;
    copy_of.type_ = NUL; /* its count is this value's now */
    return *this;
}

Json::~Json() { drop(); }

/* what this value holds let go (its type and contents are then stale) */
void Json::drop() {
    if (type_ == STRING) release(u_.text);
    else if ((type_ == ARRAY || type_ == OBJECT) && u_.body) release(u_.body);
}

void Json::release(Body* b) {
    if (atomic_add(&b->refs, -1) == 0) delete b;
}

void Json::release(Text* t) {
    if (atomic_add(&t->refs, -1) == 0) delete t;
}

Json Json::make_string(const std::string& s) {
    Json j;
    j.type_ = STRING;
    j.u_.text = new Text(s);
    return j;
}

const std::string& Json::str() const { return type_ == STRING ? u_.text->s : empty_string(); }

Json::Body* Json::own() {
    if (!u_.body) {
        u_.body = new Body();
    } else if (u_.body->refs > 1) {
        Body* mine = new Body(*u_.body);
        release(u_.body);
        u_.body = mine;
    }
    return u_.body;
}

static const std::vector<std::string>& no_keys() {
    static std::vector<std::string> none;
    return none;
}

size_t Json::size() const {
    if ((type_ != ARRAY && type_ != OBJECT) || !u_.body) return 0;
    return u_.body->arr.size();
}

const std::vector<std::string>& Json::keys() const {
    return type_ == OBJECT && u_.body ? u_.body->keys : no_keys();
}

const Json& Json::operator[](size_t idx) const {
    if (type_ != ARRAY || !u_.body || idx >= u_.body->arr.size()) return null_json();
    return u_.body->arr[idx];
}

/* An object's index past this many members (fewer: its keys looked
 * through, no index made: most objects are a few members). */
static const size_t INDEXED = 8;

/* the keys looked through, or a binary search of the sorted places */
int Json::find(const Body* b, const char* key) {
    if (b->order.empty()) {
        for (size_t i = 0; i < b->keys.size(); ++i)
            if (strcmp(b->keys[i].c_str(), key) == 0) return (int)i;
        return -1;
    }
    size_t lo = 0, hi = b->order.size();
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        int c = strcmp(b->keys[b->order[mid]].c_str(), key);
        if (c == 0) return (int)b->order[mid];
        if (c < 0) lo = mid + 1;
        else hi = mid;
    }
    return -1;
}

const Json& Json::operator[](const char* key) const {
    if (type_ != OBJECT || !u_.body) return null_json();
    int at = find(u_.body, key);
    return at < 0 ? null_json() : u_.body->arr[(size_t)at];
}

bool Json::has(const char* key) const { return type_ == OBJECT && u_.body && find(u_.body, key) >= 0; }

struct KeyLess {
    const std::vector<std::string>* keys;
    bool operator()(unsigned a, unsigned b) const {
        int c = strcmp((*keys)[a].c_str(), (*keys)[b].c_str());
        return c < 0 || (c == 0 && a < b);
    }
};

/* an object's places sorted by key (past INDEXED members) */
static void sort_places(const std::vector<std::string>& keys, std::vector<unsigned>& order) {
    order.resize(keys.size());
    for (size_t i = 0; i < keys.size(); ++i) order[i] = (unsigned)i;
    KeyLess less;
    less.keys = &keys;
    std::sort(order.begin(), order.end(), less);
}

void Json::index_members(Body* b) { sort_places(b->keys, b->order); }

/* the place a new key takes in the sorted order */
static size_t insert_at(const std::vector<std::string>& keys, const std::vector<unsigned>& order, const char* key) {
    size_t lo = 0, hi = order.size();
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        if (strcmp(keys[order[mid]].c_str(), key) < 0) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}

void Json::set(const std::string& key, const Json& value) {
    /* value may be one of this value's own members (or below them): held
     * before the write copies or replaces them */
    Json keep(value);
    if (type_ != OBJECT) {
        *this = Json();
        type_ = OBJECT;
    }
    Body* b = own();
    int at = find(b, key.c_str());
    if (at >= 0) {
        b->arr[(size_t)at] = keep;
        return;
    }
    b->keys.push_back(key);
    b->arr.push_back(keep);
    if (b->keys.size() <= INDEXED) return;
    if (b->order.empty()) {
        index_members(b);
        return;
    }
    size_t place = insert_at(b->keys, b->order, key.c_str());
    b->order.insert(b->order.begin() + (long)place, (unsigned)(b->keys.size() - 1));
}

void Json::append(const Json& value) {
    Json keep(value);
    if (type_ != ARRAY) {
        *this = Json();
        type_ = ARRAY;
    }
    own()->arr.push_back(keep);
}

void Json::swap(Json& other) {
    unsigned char t = type_;
    type_ = other.type_;
    other.type_ = t;
    bool b = bool_;
    bool_ = other.bool_;
    other.bool_ = b;
    std::swap(u_, other.u_);
}

bool Json::operator==(const Json& other) const {
    if (type_ != other.type_) return false;
    switch (type_) {
    case BOOL: return bool_ == other.bool_;
    case NUMBER: return u_.num == other.u_.num;
    case STRING: return u_.text == other.u_.text || u_.text->s == other.u_.text->s;
    case ARRAY:
    case OBJECT: {
        if (u_.body == other.u_.body) return true;
        if (size() != other.size()) return false;
        if (!u_.body || !other.u_.body) return true; /* both empty */
        if (type_ == ARRAY) return u_.body->arr == other.u_.body->arr;
        /* the same keys with the same values, in whichever order: up to
         * INDEXED members (no sorted index) each looked up in the other */
        const Body* a = u_.body;
        const Body* o = other.u_.body;
        if (a->order.empty() || o->order.empty()) {
            for (size_t i = 0; i < a->keys.size(); ++i) {
                int at = find(o, a->keys[i].c_str());
                if (at < 0 || a->arr[i] != o->arr[(size_t)at]) return false;
            }
            return true;
        }
        for (size_t i = 0; i < a->order.size(); ++i) {
            const std::string& ka = a->keys[a->order[i]];
            if (ka != o->keys[o->order[i]]) return false;
            if (a->arr[a->order[i]] != o->arr[o->order[i]]) return false;
        }
        return true;
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
    /* grown by swapping the parsed elements over (a vector's growth copied
     * every element and counted its contents up and down) */
    static void grow(std::vector<Json>& v, size_t capacity) {
        std::vector<Json> bigger(capacity);
        for (size_t i = 0; i < v.size(); ++i) bigger[i].swap(v[i]);
        v.swap(bigger);
    }
    static void grow(std::vector<std::string>& v, size_t capacity) {
        std::vector<std::string> bigger(capacity);
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
        /* the run up to the first escape or the closing quote at once */
        const char* run = p_;
        while (p_ < end_ && *p_ != '"' && *p_ != '\\') ++p_;
        o.assign(run, (size_t)(p_ - run));
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
    /* an object's members parsed, in file order: its index sorted (past
     * INDEXED members), and a key given twice kept at its first place with
     * its last value (as a map took it) */
    static void index(Json::Body* b) {
        size_t n = b->keys.size();
        std::vector<unsigned> order;
        sort_places(b->keys, order);
        bool twice = false;
        for (size_t i = 1; i < n && !twice; ++i)
            twice = b->keys[order[i]] == b->keys[order[i - 1]];
        if (!twice) {
            if (n > INDEXED) b->order.swap(order);
            return;
        }
        std::vector<std::string> keys;
        std::vector<Json> vals;
        for (size_t i = 0; i < n; ++i) {
            size_t at = keys.size();
            for (size_t k = 0; k < keys.size(); ++k)
                if (keys[k] == b->keys[i]) at = k;
            if (at == keys.size()) {
                keys.push_back(b->keys[i]);
                vals.push_back(b->arr[i]);
            } else {
                vals[at] = b->arr[i];
            }
        }
        b->keys.swap(keys);
        b->arr.swap(vals);
        index(b);
    }
    bool value(Json& o, int depth) {
        if (depth > 200) return fail("nesting too deep");
        if (p_ >= end_) return fail("unexpected end");
        char c = *p_;
        if (c == '"') {
            o.drop();
            o.type_ = Json::STRING;
            o.u_.text = new Json::Text(std::string());
            return string(o.u_.text->s);
        }
        if (c == '{') {
            o.drop();
            o.type_ = Json::OBJECT;
            o.u_.body = NULL;
            ++p_;
            ws();
            if (p_ < end_ && *p_ == '}') { ++p_; return true; }
            Json::Body* b = o.own();
            size_t n = 0;
            for (;;) {
                ws();
                if (p_ >= end_ || *p_ != '"') return fail("expected key");
                /* parsed into its place: assigning a parsed value copied
                 * its whole subtree, at every level of the tree */
                if (n == b->keys.size()) {
                    size_t cap = n ? n * 2 : 4;
                    grow(b->keys, cap);
                    grow(b->arr, cap);
                }
                if (!string(b->keys[n])) return false;
                ws();
                if (p_ >= end_ || *p_ != ':') return fail("expected ':'");
                ++p_;
                ws();
                if (!value(b->arr[n], depth + 1)) return false;
                ++n;
                ws();
                if (p_ < end_ && *p_ == ',') { ++p_; continue; }
                if (p_ < end_ && *p_ == '}') {
                    ++p_;
                    b->keys.resize(n);
                    b->arr.resize(n);
                    /* the room the doubling left over goes */
                    trim(b->keys);
                    trim(b->arr);
                    index(b);
                    return true;
                }
                return fail("expected ',' or '}'");
            }
        }
        if (c == '[') {
            o.drop();
            o.type_ = Json::ARRAY;
            o.u_.body = NULL;
            ++p_;
            ws();
            if (p_ < end_ && *p_ == ']') { ++p_; return true; }
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
                    trim(arr);
                    return true;
                }
                return fail("expected ',' or ']'");
            }
        }
        if (c == 't') { o.drop(); o.type_ = Json::BOOL; o.bool_ = true; return lit("true"); }
        if (c == 'f') { o.drop(); o.type_ = Json::BOOL; o.bool_ = false; return lit("false"); }
        if (c == 'n') { o.drop(); o.type_ = Json::NUL; o.u_.num = 0; return lit("null"); }
        if (c == '-' || (c >= '0' && c <= '9')) {
            char buf[64];
            size_t n = 0;
            while (p_ < end_ && n < sizeof(buf) - 1 &&
                   (*p_ == '-' || *p_ == '+' || *p_ == '.' || *p_ == 'e' || *p_ == 'E' || (*p_ >= '0' && *p_ <= '9')))
                buf[n++] = *p_++;
            buf[n] = 0;
            o.drop();
            o.type_ = Json::NUMBER;
            o.u_.num = strtod(buf, NULL);
            return true;
        }
        return fail("unexpected character");
    }
    /* a parsed array's or object's room left over by the doubling given
     * back (the elements swapped over, not copied) */
    template <class T> static void trim(std::vector<T>& v) {
        if (v.capacity() == v.size()) return;
        std::vector<T> exact(v.size());
        for (size_t i = 0; i < v.size(); ++i) exact[i].swap(v[i]);
        v.swap(exact);
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
