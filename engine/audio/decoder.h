/* Streaming decoders for the converter's Ogg samples and PCM16 WAVs.
 * Each decoder has its own pack handle; compressed music stays on disk. */
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
    void* vorbis_;
    FILE* pcm_;
    u64 data_offset_;
    u32 frames_, position_, rate_;
    int channels_;
};
}
#endif
