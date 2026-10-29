#include "pack.h"

#include <string.h>
#if defined(__XSDK__)
#include <unistd.h>
#endif

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
    path_.clear();
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

/* n bytes at off. On xsdk the C library's stdio reads through a 1 KB
 * buffer (newlib's BUFSIZ, as xsdk's fstat gives no block size), each
 * refill a request of the USB drive: the console read the pack at 3.9 MB/s
 * whatever the entry, the Neow room's 12 MB background in 3.2 s. read()
 * asks the kernel for all of it at once. */
static bool read_at(FILE* f, u64 off, void* buf, size_t n) {
#if defined(__XSDK__)
    int fd = fileno(f);
    if (lseek(fd, (off_t)off, SEEK_SET) != (off_t)off) return false;
    u8* p = (u8*)buf;
    while (n) {
        ssize_t got = read(fd, p, n);
        if (got <= 0) return false;
        p += got;
        n -= (size_t)got;
    }
    return true;
#else
    return seek64(f, off) && (n == 0 || fread(buf, 1, n, f) == n);
#endif
}

/* An audio stream's stdio buffer: its reads are small (stb_vorbis takes a
 * byte at a time), each refill of xsdk's 1 KB one a request of the drive. */
enum { STREAM_BUFFER = 32 * 1024 };

bool Pack::open(const char* path, std::string* error) {
    close();
    f_ = fopen(path, "rb");
    if (!f_) {
        if (error) *error = format("cannot open %s", path);
        return false;
    }
    path_ = path;
    u8 h[64];
    if (!read_at(f_, 0, h, 64) || memcmp(h, "S3P1", 4) != 0 || be32(h + 4) != 1) {
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
    if ((count && !read_at(f_, dir_off, &dir[0], dir.size())) ||
        (names_size && !read_at(f_, names_off, &names_[0], names_size))) {
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
    ScopedLock lock(lock_);
    out.resize(e->size);
    return e->size == 0 || read_at(f_, e->offset, &out[0], e->size);
}

bool Pack::read_head(const PackEntry* e, u32 len, std::vector<u8>& out) const {
    if (!e || !f_) return false;
    ScopedLock lock(lock_);
    if (len > e->size) len = e->size;
    out.resize(len);
    return len == 0 || read_at(f_, e->offset, &out[0], len);
}

FILE* Pack::open_stream(const PackEntry* e) const {
    if (!f_ || !e) return NULL;
    FILE* stream = fopen(path_.c_str(), "rb");
    if (stream) setvbuf(stream, NULL, _IOFBF, STREAM_BUFFER);
    if (stream && !seek64(stream, e->offset)) { fclose(stream); stream = NULL; }
    return stream;
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
