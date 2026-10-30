#include "decoder.h"
#include "../core/pack.h"
#include "../core/thread.h"
#include <string.h>

#include <map>

#define STB_VORBIS_NO_PUSHDATA_API
#define STB_VORBIS_NO_INTEGER_CONVERSION
#include "../../third_party/stb/stb_vorbis.c"

namespace spire {

static u16 le16(const u8* p) { return (u16)(p[0] | (p[1] << 8)); }
static u32 le32(const u8* p) { return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24); }
static bool audio_seek(FILE* file, u64 offset) {
#if defined(_MSC_VER)
    return _fseeki64(file, (i64)offset, SEEK_SET) == 0;
#else
    return fseeko(file, (off_t)offset, SEEK_SET) == 0;
#endif
}

/* The kept sounds (decoder.h): a voice starting reads nothing from the pack,
 * where each opened the pack again and read the sound's headers (on the
 * console a few ms from its USB drive, in the frame loop under the audio
 * lock, for each sound an effect plays), and the mix reads none while
 * textures load. Past the budget the least recently played that no voice
 * reads go: kept till exit, the budget filled in a long session and every
 * new sound after it streamed, a file each, until the console's 32 handles
 * ran out and voices did not start ("cannot open"). */
enum { KEPT_MAX = 512 * 1024, KEPT_TOTAL = 12 * 1024 * 1024 };
struct KeptSound {
    std::vector<u8> data;
    int users;
    u64 used;
};
static Mutex g_kept_lock;
static std::map<std::string, KeptSound*> g_kept;
static size_t g_kept_bytes = 0;
static u64 g_kept_clock = 0;

static KeptSound* kept_sound(const Pack& pack, const PackEntry* entry, const std::string& path) {
    if (entry->size > KEPT_MAX) return NULL;
    ScopedLock lock(g_kept_lock);
    std::map<std::string, KeptSound*>::iterator it = g_kept.find(path);
    if (it != g_kept.end()) {
        ++it->second->users;
        it->second->used = ++g_kept_clock;
        return it->second;
    }
    while (g_kept_bytes + entry->size > KEPT_TOTAL) {
        std::map<std::string, KeptSound*>::iterator oldest = g_kept.end();
        for (std::map<std::string, KeptSound*>::iterator k = g_kept.begin(); k != g_kept.end(); ++k)
            if (!k->second->users && (oldest == g_kept.end() || k->second->used < oldest->second->used)) oldest = k;
        if (oldest == g_kept.end()) return NULL;
        g_kept_bytes -= oldest->second->data.size();
        delete oldest->second;
        g_kept.erase(oldest);
    }
    KeptSound* k = new KeptSound();
    if (!pack.read(entry, k->data) || k->data.size() < 12) {
        delete k;
        return NULL;
    }
    k->users = 1;
    k->used = ++g_kept_clock;
    g_kept[path] = k;
    g_kept_bytes += k->data.size();
    return k;
}

static void unkeep(KeptSound* k) {
    ScopedLock lock(g_kept_lock);
    --k->users;
}

AudioDecoder::AudioDecoder()
    : vorbis_(NULL), pcm_(NULL), kept_(NULL), kept_size_(0), kept_pos_(0), data_offset_(0), frames_(0), position_(0),
      rate_(0), channels_(0), kept_entry_(NULL) {}
AudioDecoder::~AudioDecoder() { close(); }
void AudioDecoder::close() {
    if (vorbis_) stb_vorbis_close((stb_vorbis*)vorbis_);
    if (pcm_) fclose(pcm_);
    if (kept_entry_) unkeep(kept_entry_);
    vorbis_ = NULL; pcm_ = NULL;
    kept_entry_ = NULL;
    kept_ = NULL;
    kept_size_ = kept_pos_ = 0;
    frames_ = position_ = rate_ = 0;
    channels_ = 0;
}

bool AudioDecoder::open_kept(const std::vector<u8>& data) {
    const u8* d = &data[0];
    u32 n = (u32)data.size();
    if (!memcmp(d, "OggS", 4)) {
        int error = 0;
        stb_vorbis* v = stb_vorbis_open_memory(d, (int)n, &error, NULL);
        if (!v) return false;
        vorbis_ = v;
        kept_ = d;
        kept_size_ = n;
        stb_vorbis_info info = stb_vorbis_get_info(v);
        channels_ = info.channels == 1 ? 1 : 2;
        rate_ = info.sample_rate;
        frames_ = stb_vorbis_stream_length_in_samples(v);
        if (!frames_ || !rate_) { close(); return false; }
        return true;
    }
    if (memcmp(d, "RIFF", 4) || memcmp(d + 8, "WAVE", 4)) return false;
    u32 pos = 12;
    bool format_ok = false;
    while (pos <= n && n - pos >= 8) {
        const u8* chunk = d + pos;
        u32 size = le32(chunk + 4);
        pos += 8;
        if (size > n - pos) break;
        if (!memcmp(chunk, "fmt ", 4)) {
            if (size < 16) break;
            const u8* f = d + pos;
            channels_ = le16(f + 2);
            rate_ = le32(f + 4);
            format_ok = le16(f) == 1 && le16(f + 14) == 16 && (channels_ == 1 || channels_ == 2) &&
                        le16(f + 12) == channels_ * 2 && rate_ > 0;
        } else if (!memcmp(chunk, "data", 4) && format_ok) {
            kept_ = d;
            kept_size_ = pos + size;
            data_offset_ = pos;
            kept_pos_ = pos;
            frames_ = size / (channels_ * 2);
            return true;
        }
        if (size == n - pos) break;
        pos += size + (size & 1);
    }
    channels_ = 0; rate_ = 0;
    return false;
}

bool AudioDecoder::open(const Pack& pack, const std::string& path) {
    close();
    const PackEntry* entry = pack.find(path.c_str());
    if (!entry || entry->type != PACK_AUDIO) return false;
    KeptSound* kept = kept_sound(pack, entry, path);
    if (kept) {
        kept_entry_ = kept;
        if (open_kept(kept->data)) return true;
        close();
        return false;
    }
    std::vector<u8> head;
    if (!pack.read_head(entry, 12, head) || head.size() < 12) return false;
    FILE* input = pack.open_stream(entry);
    if (!input) return false;
    if (!memcmp(&head[0], "OggS", 4)) {
        int error = 0;
        /* Own the file ourselves until open succeeds: the decoder does not
         * close a supplied handle on failure. */
        stb_vorbis* v = stb_vorbis_open_file_section(input, 0, &error, NULL, entry->size);
        if (!v) { fclose(input); return false; }
        vorbis_ = v;
        pcm_ = input;
        stb_vorbis_info info = stb_vorbis_get_info(v);
        channels_ = info.channels == 1 ? 1 : 2;
        rate_ = info.sample_rate;
        frames_ = stb_vorbis_stream_length_in_samples(v);
        if (!frames_ || !rate_) { close(); return false; }
        return true;
    }
    if (memcmp(&head[0], "RIFF", 4) || memcmp(&head[8], "WAVE", 4)) { fclose(input); return false; }
    u32 pos = 12;
    bool format_ok = false;
    while (pos <= entry->size && entry->size - pos >= 8) {
        u8 chunk[24];
        if (!audio_seek(input, entry->offset + pos) || fread(chunk, 1, 8, input) != 8) break;
        u32 size = le32(chunk + 4);
        pos += 8;
        if (size > entry->size - pos) break;
        if (!memcmp(chunk, "fmt ", 4)) {
            if (size < 16 || fread(chunk + 8, 1, 16, input) != 16) break;
            channels_ = le16(chunk + 10);
            rate_ = le32(chunk + 12);
            format_ok = le16(chunk + 8) == 1 && le16(chunk + 22) == 16 &&
                (channels_ == 1 || channels_ == 2) && le16(chunk + 20) == channels_ * 2 && rate_ > 0;
        } else if (!memcmp(chunk, "data", 4) && format_ok) {
            data_offset_ = entry->offset + pos;
            frames_ = size / (channels_ * 2);
            pcm_ = input;
            return true;
        }
        if (size == entry->size - pos) break;
        pos += size + (size & 1);
    }
    fclose(input);
    channels_ = 0; rate_ = 0;
    return false;
}

int AudioDecoder::read(float* out, int count) {
    if (!out || count <= 0 || (!pcm_ && !kept_) || position_ >= frames_) return 0;
    u32 remaining = frames_ - position_;
    if ((u32)count > remaining) count = (int)remaining;
    int got = 0;
    if (vorbis_) {
        got = stb_vorbis_get_samples_float_interleaved((stb_vorbis*)vorbis_, channels_, out, count * channels_);
    } else if (kept_) {
        u32 have = (kept_size_ - kept_pos_) / (u32)(channels_ * 2);
        if ((u32)count > have) count = (int)have;
        const u8* p = kept_ + kept_pos_;
        for (int i = 0; i < count * channels_; ++i) {
            int value = le16(p + i * 2);
            if (value >= 32768) value -= 65536;
            out[i] = value / 32768.0f;
        }
        kept_pos_ += (u32)(count * channels_ * 2);
        got = count;
    } else {
        u8 data[4096];
        while (got < count) {
            int part = count - got;
            if (part > 2048 / channels_) part = 2048 / channels_;
            int samples = (int)fread(data, 2, (size_t)part * channels_, pcm_);
            for (int i = 0; i < samples; ++i) {
                int value = le16(data + i * 2);
                if (value >= 32768) value -= 65536;
                out[got * channels_ + i] = value / 32768.0f;
            }
            got += samples / channels_;
            if (samples != part * channels_) break;
        }
    }
    position_ += (u32)got;
    return got;
}

bool AudioDecoder::seek(u32 frame) {
    if ((!pcm_ && !kept_) || frame > frames_) return false;
    bool ok = true;
    if (vorbis_) ok = stb_vorbis_seek((stb_vorbis*)vorbis_, frame) != 0;
    else if (kept_) kept_pos_ = (u32)(data_offset_ + (u64)frame * channels_ * 2);
    else ok = audio_seek(pcm_, data_offset_ + (u64)frame * channels_ * 2);
    if (ok) position_ = frame;
    return ok;
}
}
