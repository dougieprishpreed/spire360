/* Bounded streaming voices, stereo mixing and linear sample-rate conversion.
 * Device backends consume the same float output used by the PC verifier. */
#ifndef SPIRE_AUDIO_MIXER_H
#define SPIRE_AUDIO_MIXER_H
#include "../core/base.h"

namespace spire {
class Pack;
class AudioMixer {
public:
    explicit AudioMixer(const Pack* pack, u32 rate = 48000);
    ~AudioMixer();
    /* loop_count: -1 forever, 0 once, >0 additional loop-region repeats.
     * After the last repeat, playback continues through the sample's tail. */
    u32 play(const std::string& path, float volume = 1, float pitch = 1,
             u32 loop_start = 0, u32 loop_end = 0, int loop_count = 0, u32 offset = 0);
    void stop(u32 voice, double fade = 0);
    void stop_all(double fade = 0);
    void volume(u32 voice, float value, double ramp = 0);
    void pitch(u32 voice, float ratio);
    void pause(bool paused) { paused_ = paused; }
    void render(float* stereo, int frames);
    size_t active_voices() const { return voices_.size(); }
    bool playing(u32 voice) const;
    double remaining(u32 voice) const;
    u32 rate() const { return rate_; }
    size_t buffer_bytes() const;
    /* Why the last play() returned 0. */
    const char* last_failure() const { return failure_; }
private:
    AudioMixer(const AudioMixer&);
    AudioMixer& operator=(const AudioMixer&);
    struct Voice;
    Voice* find(u32 id);
    const Pack* pack_;
    u32 rate_, next_id_;
    bool paused_;
    const char* failure_;
    std::vector<Voice*> voices_;
};
}
#endif
