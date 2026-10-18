/* Offline Studio event programs: per-bank samples, nested events, timelines,
 * playlists and parameter-driven transitions. All clocks advance with the
 * mixer, independent of the logic thread and of frame rendering speed. */
#ifndef SPIRE_AUDIO_EVENTS_H
#define SPIRE_AUDIO_EVENTS_H
#include "../core/base.h"
#include "../core/json.h"

namespace spire {
class Pack;
class AudioEvents {
public:
    explicit AudioEvents(const Pack* pack, u32 rate = 48000);
    ~AudioEvents();
    bool available() const;
    void prefer_bank(const std::string& bank);
    u32 play(const std::string& path, float volume = 1, const Json& parameters = Json());
    void parameter(u32 instance, const std::string& name, float value);
    void parameter_label(u32 instance, const std::string& name, const std::string& label);
    void global_parameter(const std::string& name, float value);
    void pitch(u32 instance, float ratio);
    void stop(u32 instance, double fade = 0);
    void stop_all(double fade = 0);
    bool playing(u32 instance) const;
    void bus_volume(const std::string& path, float linear);
    void render(float* stereo, int frames);
    size_t active_voices() const;
    size_t active_events() const;
    unsigned failures() const;
    unsigned missing_events() const;
    u64 started_voices() const;
    /* Verifier diagnostics, in seconds and parameter values. */
    double timeline(u32 instance) const;
    float parameter_value(u32 instance, const std::string& name) const;
private:
    AudioEvents(const AudioEvents&);
    AudioEvents& operator=(const AudioEvents&);
    struct Impl;
    Impl* impl_;
};
}
#endif
