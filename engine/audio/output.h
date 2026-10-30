/* XAudio2 output on Xbox 360 (XDK) and Windows, xsdk's audio feeder on the
 * xSDK console build. Eight bounded buffers are reused
 * only after consumption; the producer never waits on the audio thread. */
#ifndef SPIRE_AUDIO_OUTPUT_H
#define SPIRE_AUDIO_OUTPUT_H
#include "../core/base.h"

namespace spire {
class AudioMixer;
class AudioOutput {
public:
    AudioOutput();
    ~AudioOutput();
    bool open(u32 rate = 48000, bool silent = false);
    void close();
    bool active() const { return impl_ != NULL; }
    /* One 256-frame buffer may be submitted for each positive demand(). */
    int demand() const;
    /* Buffers kept queued (4 by default, at most 15): more absorb a slow
     * render (the audio thread). On the xSDK console, before open: its
     * feeder's frames ahead (8 by default, at most 16). */
    void set_queue(int buffers);
    bool submit(const float* stereo, int frames);
    u64 played_frames() const;
    /* The device's own count (the xSDK console's feeder; false elsewhere):
     * frames it played silence for as none was queued (the mix behind),
     * the fewest queued as it came for one. */
    struct Stats {
        u32 played, silent, fewest_queued, queue;
    };
    bool stats(Stats* out) const;
private:
    AudioOutput(const AudioOutput&);
    AudioOutput& operator=(const AudioOutput&);
    struct Impl;
    Impl* impl_;
    int ahead_; /* the xSDK feeder's frames ahead */
};
}
#endif
