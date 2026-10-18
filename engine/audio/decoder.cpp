#include "decoder.h"
#include "../core/pack.h"
#include <string.h>

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

AudioDecoder::AudioDecoder() : vorbis_(NULL), pcm_(NULL), data_offset_(0), frames_(0), position_(0), rate_(0), channels_(0) {}
AudioDecoder::~AudioDecoder() { close(); }
void AudioDecoder::close() {
    if (vorbis_) stb_vorbis_close((stb_vorbis*)vorbis_);
    if (pcm_) fclose(pcm_);
    vorbis_ = NULL; pcm_ = NULL;
    frames_ = position_ = rate_ = 0;
    channels_ = 0;
}

bool AudioDecoder::open(const Pack& pack, const std::string& path) {
    close();
    const PackEntry* entry = pack.find(path.c_str());
    std::vector<u8> head;
    if (!entry || entry->type != PACK_AUDIO || !pack.read_head(entry, 12, head) || head.size() < 12) return false;
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
    if (!out || count <= 0 || !pcm_ || position_ >= frames_) return 0;
    u32 remaining = frames_ - position_;
    if ((u32)count > remaining) count = (int)remaining;
    int got = 0;
    if (vorbis_) {
        got = stb_vorbis_get_samples_float_interleaved((stb_vorbis*)vorbis_, channels_, out, count * channels_);
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
    if (!pcm_ || frame > frames_) return false;
    bool ok = vorbis_ ? stb_vorbis_seek((stb_vorbis*)vorbis_, frame) != 0 :
        audio_seek(pcm_, data_offset_ + (u64)frame * channels_ * 2);
    if (ok) position_ = frame;
    return ok;
}
}
