/* pack.s3p reader (format: converter/spire_convert/s3p.py, docs/PACK_FORMAT.md). */
#ifndef SPIRE_PACK_H
#define SPIRE_PACK_H

#include "thread.h"
#include "base.h"

#include <stdio.h>

namespace spire {

enum PackEntryType { PACK_TEXTURE = 1, PACK_BLOB = 2, PACK_SCENE = 3, PACK_FONT = 4, PACK_AUDIO = 5, PACK_SPINE = 6 };
enum TexFormat { TEX_DXT1 = 1, TEX_DXT5 = 2, TEX_ARGB8 = 3 };

struct PackEntry {
    u64 hash;
    u64 offset;
    u32 size;
    u16 type;
    u16 flags;
    u32 name_offset;
    u32 crc;
};

enum { TEXF_DATA = 1 }; /* texture flags: not colour (MSDF): never premultiply */

struct TextureData {
    int format;            /* TexFormat */
    int flags;             /* TEXF_* */
    int width, height;     /* stored size */
    int orig_width, orig_height; /* size in layout units */
    std::vector<std::vector<u8> > levels;
};

u64 fnv1a64(const char* s);

class Pack {
public:
    Pack();
    ~Pack();
    bool open(const char* path, std::string* error);
    void close();

    const PackEntry* find(const char* name) const;
    bool read(const PackEntry* e, std::vector<u8>& out) const;
    bool read(const char* name, std::vector<u8>& out) const;
    /* The first bytes of an entry (headers). */
    bool read_head(const PackEntry* e, u32 len, std::vector<u8>& out) const;
    /* An independent file handle positioned at this entry, for streaming
     * audio without moving the renderer's shared pack cursor. Caller closes. */
    FILE* open_stream(const PackEntry* e) const;
    const char* name_of(const PackEntry* e) const;
    size_t count() const { return entries_.size(); }
    const PackEntry& entry(size_t i) const { return entries_[i]; }
    const std::string& game_version() const { return game_version_; }
    bool texture_little_endian() const { return (flags_ & 1) != 0; }

    static bool parse_texture(const std::vector<u8>& payload, TextureData& out);
    /* Stored and layout sizes from the first 12 bytes of a texture entry. */
    static bool parse_texture_size(const std::vector<u8>& head, int* w, int* h, int* orig_w, int* orig_h);

private:
    FILE* f_;
    /* read / read_head seek and read f_: the audio thread reads too */
    mutable Mutex lock_;
    std::string path_;
    u32 flags_;
    std::string game_version_;
    std::vector<PackEntry> entries_; /* sorted by hash */
    std::vector<char> names_;
};

} /* namespace spire */

#endif
