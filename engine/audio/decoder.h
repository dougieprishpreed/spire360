/* Streaming decoders for the converter's Ogg samples and PCM16 WAVs.
 * Sounds up to 512 KB are read once and kept in memory (12 MB of them, the
 * least recently played that no voice reads going first), each decoder
 * reading its own way through them; the rest, music among them, stream
 * from the pack through a handle of each decoder's own.
 *
 * With the loader started (the console's audio thread), every sound is
 * kept, read whole on the loader's thread ahead of the textures' reads, a
 * short one decoded there to its samples, and each Vorbis decoder of a
 * long one (the music) set up there too: nothing in the mix or under its
 * lock reads the drive or sets a decoder up. A decoder opened before that
 * is done is pending until it is. */
#ifndef SPIRE_AUDIO_DECODER_H
#define SPIRE_AUDIO_DECODER_H

#include "../core/base.h"
#include <stdio.h>

namespace spire {
class Pack;
struct KeptSound;
struct OpenJob;
class AudioDecoder {
public:
    AudioDecoder();
    ~AudioDecoder();
    /* false: no such sound, or it cannot be read; true and pending(): read
     * on the loader's thread, poll() until it is */
    bool open(const Pack& pack, const std::string& path);
    bool pending() const { return pending_; }
    /* A pending sound read: open (true), still being read (false, pending),
     * or not to be read (false, no longer pending). */
    bool poll();
    /* The sound was too big to keep in the first place (a stream: music,
     * ambience), whose voices keep to their timeline while it loads. */
    bool big() const { return big_; }
    void close();
    int read(float* output, int frames); /* interleaved, channels() values per frame */
    bool seek(u32 frame);
    int channels() const { return channels_; }
    u32 rate() const { return rate_; }
    u32 frames() const { return frames_; }
    u32 position() const { return position_; }

    /* The loader's thread (hw_thread: the console's hardware thread, -1 any)
     * and the budget of the sounds kept (bytes). */
    static bool start_loader(const Pack* pack, int hw_thread, size_t budget);
    static void stop_loader();
    struct LoadStats {
        int loads;            /* read since the last call */
        double bytes;         /* their bytes */
        double longest_ms;    /* the longest from asked to read */
        size_t kept;          /* bytes kept now */
    };
    static LoadStats load_stats();
private:
    AudioDecoder(const AudioDecoder&);
    AudioDecoder& operator=(const AudioDecoder&);
    bool open_kept(const std::vector<u8>& data);
    void open_samples(const KeptSound* k);
    void* vorbis_;
    FILE* pcm_;
    /* a kept sound: its bytes, and where a WAV's next frame is */
    const u8* kept_;
    u32 kept_size_, kept_pos_;
    u64 data_offset_;
    u32 frames_, position_, rate_;
    int channels_;
    KeptSound* kept_entry_; /* the kept sound this one reads (one of its users) */
    OpenJob* job_;          /* its decoder being opened on the loader's thread */
    bool pending_, big_;
};
}
#endif
