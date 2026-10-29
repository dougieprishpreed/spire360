/* Streaming decoders for the converter's Ogg samples and PCM16 WAVs.
 * Sounds up to 512 KB are read once and kept in memory (12 MB of them),
 * each decoder reading its own way through them; the rest, music among
 * them, stream from the pack through a handle of each decoder's own. */
#ifndef SPIRE_AUDIO_DECODER_H
#define SPIRE_AUDIO_DECODER_H

#include "../core/base.h"
#include <stdio.h>

namespace spire {
class Pack;
class AudioDecoder {
public:
    AudioDecoder();
    ~AudioDecoder();
    bool open(const Pack& pack, const std::string& path);
    void close();
    int read(float* output, int frames); /* interleaved, channels() values per frame */
    bool seek(u32 frame);
    int channels() const { return channels_; }
    u32 rate() const { return rate_; }
    u32 frames() const { return frames_; }
    u32 position() const { return position_; }
private:
    AudioDecoder(const AudioDecoder&);
    AudioDecoder& operator=(const AudioDecoder&);
    bool open_kept(const std::vector<u8>& data);
    void* vorbis_;
    FILE* pcm_;
    /* a kept sound: its bytes, and where a WAV's next frame is */
    const u8* kept_;
    u32 kept_size_, kept_pos_;
    u64 data_offset_;
    u32 frames_, position_, rate_;
    int channels_;
};
}
#endif
