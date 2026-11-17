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
    /* A bank parsed now (its first event would parse it mid-game), read
     * and parsed without the lock, so the mixer keeps playing meanwhile. */
    void preload(const std::string& bank);
    /* The event whose changes are noted (the music: a jump a parameter
     * made, its timeline's commands, a parameter the game changed), for
     * take_notes on the game's thread (the mix's thread writes no log). */
    void set_logged(u32 id);
    void take_notes(std::vector<std::string>& out);
    /* The act's banks no longer preferred: the parsed ones nothing plays
     * go (a run's end). */
    void release_banks();
    /* The bank an event would play from, parsed the same way. */
    void preload_for(const std::string& path);
    /* An event's samples read ahead (AudioDecoder::prefetch), for sounds
     * met soon; warm_monsters: the events under enemy_attacks/<folder>/ of
     * each monster id (lower case) that starts with the folder. */
    /* The samples an event reaches (its instruments', its nested events'),
     * added to out (its bank parsed first: preload_for) */
    void samples_of(const std::string& path, std::vector<std::string>& out);
    void warm(const std::string& path);
    /* sfx: the monsters' own sounds as the logic names them (death, hurt,
     * impact): their folders' events too (a Calcified Cultist's are under
     * cultists/); all of them held (AudioDecoder::set_held) */
    void warm_monsters(const std::vector<std::string>& ids, const std::vector<std::string>& sfx = std::vector<std::string>());
    u32 play(const std::string& path, float volume = 1, const Json& parameters = Json());
    /* A test aid (spire_play --audio-trace): every play logged with its
     * path. */
    static void set_trace(bool on);
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
    /* the voices playing, by sample (AudioMixer::describe; checks) */
    std::string describe_voices() const;
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
