/* Streaming decoders for the converter's Ogg samples and PCM16 WAVs.
 * Sounds up to 512 KB are read once and kept in memory (12 MB of them, the
 * least recently played that no voice reads going first), each decoder
 * reading its own way through them; the rest, music among them, stream
 * from the pack through a handle of each decoder's own.
 *
 * With the loader started (the console's audio thread), every sound is
 * kept, read on the loader's thread ahead of the textures' reads, a short
 * one decoded there to its samples (its voices start on the first part),
 * and each Vorbis decoder of a long one (the music) set up there too, at
 * the frame its voice starts from: nothing in the mix or under its lock
 * reads the drive, sets a decoder up or seeks. A decoder opened before that
 * is done is pending until it is. */
#ifndef SPIRE_AUDIO_DECODER_H
#define SPIRE_AUDIO_DECODER_H

#include "../core/base.h"
#include <stdio.h>
#include <string>
#include <vector>

namespace spire {
class Pack;
struct KeptSound;
struct OpenJob;
class AudioDecoder {
public:
    AudioDecoder();
    ~AudioDecoder();
    /* false: no such sound, or it cannot be read; true and pending(): read
     * on the loader's thread, poll() until it is. start: the frame its voice
     * starts from (where the loader opens a stream's decoder); follow: the
     * voice keeps to a timeline, from where it is by the time it opens. */
    bool open(const Pack& pack, const std::string& path, u32 start = 0, bool follow = false);
    bool pending() const { return pending_; }
    /* A pending sound read: open (true), still being read (false, pending),
     * or not to be read (false, no longer pending). */
    bool poll();
    /* The sound was too big to keep in the first place (a stream: music,
     * ambience), whose voices keep to their timeline while it loads. */
    bool big() const { return big_; }
    void close();
    int read(float* output, int frames); /* interleaved, channels() values per frame */
    /* The last read gave nothing as its samples are still being decoded
     * (not the end): the voice waits for them. */
    bool waiting() const { return waiting_; }
    bool seek(u32 frame);
    int channels() const { return channels_; }
    u32 rate() const { return rate_; }
    u32 frames() const { return frames_; }
    u32 position() const { return position_; }

    /* The loader's thread (hw_thread: the console's hardware thread, -1 any)
     * and the budget of the sounds kept (bytes). */
    static bool start_loader(const Pack* pack, int hw_thread, size_t budget);
    /* A new budget, the least recently played that no voice reads going at
     * once past it (the console's memory running out). */
    static void set_budget(size_t budget);
    /* A sound read ahead of its first play (kept, as recently played as
     * any); nothing without the loader. */
    static void prefetch(const Pack& pack, const std::string& path);
    /* The sounds kept whatever the budget (an act's music and ambience while
     * they are the run's: each read once, in the background behind any
     * sound a voice waits on, not read again at each room's change); the
     * set replaces the last one, whose sounds go back to the budget's
     * least recently played order. The budget counts the others only. */
    static void set_pinned(const Pack& pack, const std::vector<std::string>& paths);
    /* Bytes the pinned sounds hold now. */
    static size_t pinned_bytes();
    /* The pinned sounds not read yet (a run's or act's first screen waits
     * for its music behind the black). */
    static int pinned_pending();
    /* The least recently played that no voice reads go until the unpinned
     * ones hold `keep` bytes at most, the budget as it was (a run's end). */
    static void let_go(size_t keep);
    /* A Vorbis decoder set up on the loader's thread at the frame a voice
     * of the sound will start from (a music timeline's jump, known ahead):
     * that voice takes it at once, neither waiting for the loader nor
     * skipping what went by meanwhile. Unused, it goes after 30 s. */
    static void prepare(const Pack& pack, const std::string& path, u32 frame);
    static void stop_loader();
    struct LoadStats {
        int loads;            /* read since the last call */
        double bytes;         /* their bytes */
        double longest_ms;    /* the longest from asked to its voices' start */
        double longest_queue_ms, longest_read_ms; /* of it: waiting for the loader, reading */
        const char* longest;  /* its pack name ("" none) */
        /* while it waited for the loader, the loader's time (ms): decoding
         * short sounds' parts, reading short sounds, opening voices'
         * decoders, reading long sounds (the music) */
        double longest_busy[4];
        size_t kept;          /* bytes kept now */
        int prepared, prepared_taken; /* decoders set up ahead (prepare), and taken by their voice */
    };
    static LoadStats load_stats();
private:
    AudioDecoder(const AudioDecoder&);
    AudioDecoder& operator=(const AudioDecoder&);
    bool open_kept(const std::vector<u8>& data);
    void open_samples(const KeptSound* k);
    void refresh_samples();
    int take_kept(KeptSound* k);
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
    /* the loader's samples (samples_): decoded up to avail_, all of them
     * once complete_ */
    bool samples_, complete_, waiting_;
    u32 avail_;
    u32 start_;
    bool follow_;
    double asked_; /* clock_ms() as it was opened */
};
}
#endif
