#include "pack.h"

#include <string.h>

namespace spire {

static u32 be32(const u8* p) { return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3]; }
static u16 be16(const u8* p) { return (u16)((p[0] << 8) | p[1]); }
static u64 be64(const u8* p) { return ((u64)be32(p) << 32) | be32(p + 4); }

u64 fnv1a64(const char* s) {
    u64 h = 0xCBF29CE484222325ULL;
    for (; *s; ++s) {
        h ^= (u8)*s;
        h *= 0x100000001B3ULL;
    }
    return h;
}

Pack::Pack() : f_(NULL), flags_(0) {}
Pack::~Pack() { close(); }

void Pack::close() {
    if (f_) fclose(f_);
    f_ = NULL;
    entries_.clear();
    names_.clear();
}

static bool seek64(FILE* f, u64 off) {
#if defined(_MSC_VER)
    return _fseeki64(f, (i64)off, SEEK_SET) == 0;
#else
    return fseeko(f, (off_t)off, SEEK_SET) == 0;
#endif
}

bool Pack::open(const char* path, std::string* error) {
    close();
    f_ = fopen(path, "rb");
    if (!f_) {
        if (error) *error = format("cannot open %s", path);
        return false;
    }
    u8 h[64];
    if (fread(h, 1, 64, f_) != 64 || memcmp(h, "S3P1", 4) != 0 || be32(h + 4) != 1) {
        if (error) *error = "not a Spire360 pack (S3P1 v1)";
        close();
        return false;
    }
    u32 count = be32(h + 8);
    flags_ = be32(h + 12);
    u64 dir_off = be64(h + 16);
    u64 names_off = be64(h + 24);
    u32 names_size = be32(h + 32);
    size_t gv = 0;
    while (gv < 24 && h[40 + gv]) ++gv;
    game_version_.assign((const char*)h + 40, gv);
    std::vector<u8> dir((size_t)count * 32);
    names_.resize(names_size + 1);
    if (!seek64(f_, dir_off) || (count && fread(&dir[0], 1, dir.size(), f_) != dir.size()) ||
        !seek64(f_, names_off) || (names_size && fread(&names_[0], 1, names_size, f_) != names_size)) {
        if (error) *error = "truncated pack directory";
        close();
        return false;
    }
    names_[names_size] = 0;
    entries_.resize(count);
    for (u32 i = 0; i < count; ++i) {
        const u8* p = &dir[i * 32];
        PackEntry& e = entries_[i];
        e.hash = be64(p);
        e.offset = be64(p + 8);
        e.size = be32(p + 16);
        e.type = be16(p + 20);
        e.flags = be16(p + 22);
        e.name_offset = be32(p + 24);
        e.crc = be32(p + 28);
    }
    return true;
}

const PackEntry* Pack::find(const char* name) const {
    u64 h = fnv1a64(name);
    size_t lo = 0, hi = entries_.size();
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        if (entries_[mid].hash < h) lo = mid + 1;
        else hi = mid;
    }
    if (lo < entries_.size() && entries_[lo].hash == h) return &entries_[lo];
    return NULL;
}

const char* Pack::name_of(const PackEntry* e) const {
    return e->name_offset < names_.size() ? &names_[e->name_offset] : "";
}

bool Pack::read(const PackEntry* e, std::vector<u8>& out) const {
    if (!e || !f_) return false;
    out.resize(e->size);
    if (!seek64(f_, e->offset)) return false;
    return e->size == 0 || fread(&out[0], 1, e->size, f_) == e->size;
}

bool Pack::read_head(const PackEntry* e, u32 len, std::vector<u8>& out) const {
    if (!e || !f_) return false;
    if (len > e->size) len = e->size;
    out.resize(len);
    if (!seek64(f_, e->offset)) return false;
    return len == 0 || fread(&out[0], 1, len, f_) == len;
}

bool Pack::parse_texture_size(const std::vector<u8>& d, int* w, int* h, int* ow, int* oh) {
    if (d.size() < 12) return false;
    *w = be16(&d[4]);
    *h = be16(&d[6]);
    *ow = be16(&d[8]);
    *oh = be16(&d[10]);
    return true;
}

bool Pack::read(const char* name, std::vector<u8>& out) const {
    return read(find(name), out);
}

bool Pack::parse_texture(const std::vector<u8>& d, TextureData& t) {
    if (d.size() < 12) return false;
    t.format = d[0];
    int n = d[1];
    t.flags = be16(&d[2]);
    t.width = be16(&d[4]);
    t.height = be16(&d[6]);
    t.orig_width = be16(&d[8]);
    t.orig_height = be16(&d[10]);
    if (d.size() < (size_t)(12 + 4 * n)) return false;
    t.levels.resize(n);
    for (int i = 0; i < n; ++i) {
        u32 a = be32(&d[12 + 4 * i]);
        u32 b = i + 1 < n ? be32(&d[12 + 4 * (i + 1)]) : (u32)d.size();
        if (a > b || b > d.size()) return false;
        t.levels[i].assign(d.begin() + a, d.begin() + b);
    }
    return true;
}

} /* namespace spire */
