/* NAudioManager / NRunMusicController presentation side. Sound cues never
 * extend combat input waits; device and headless rendering share one clock. */
#ifndef SPIRE_GAME_AUDIO_H
#define SPIRE_GAME_AUDIO_H
#include "events.h"
#include "output.h"
#include "../core/thread.h"
#include <map>
#include <stdio.h>
namespace spire {
class GameAudio {
public:
    explicit GameAudio(const Pack* pack);
    ~GameAudio();
    bool open_output(bool muted = false);
    /* The device fed from its own thread (the console's hardware thread
     * hw_thread); update() then does nothing. After open_output. The
     * sounds read on a thread of their own (loader_hw_thread), none in the
     * mix or under its lock. */
    bool start_thread(int hw_thread, int loader_hw_thread = -1);
    /* The mix's time for each 256 frames (5.3 ms of sound) since the last
     * call: the worst, the average (ms), and how many. */
    void mix_times(double* worst_ms, double* average_ms, int* count);
    /* The output's own count (AudioOutput::stats). */
    bool output_stats(AudioOutput::Stats* out) const { return output_.stats(out); }
    bool record(const char* path);
    void update(double seconds);
    void cue(const Json& cue);
    void steps(const Json& steps);
    void decision(const Json& decision);
    void menu();
    /* NRunMusicController.ToggleMerchantTrack (NMerchantRoom, the map
     * screen opened or closed over it): Progress MerchantEnd or Merchant */
    void merchant_map(bool open);
    void ui(const std::string& path, float volume = 1);
    /* A sound read ahead of its first play (a debug_audio name or a pack
     * path; not an event's). */
    void prefetch(const std::string& path);
    /* Cues run on the clock as ever but nothing is decoded or mixed (a tool
     * sweeping traces: spire_gaps --fast). */
    void set_dry(bool dry) { dry_ = dry; }
    void stop_room_loops(const std::string& owner = std::string());
    AudioEvents& events() { return events_; }
private:
    struct Loop { u32 id; std::string parameter, owner; float stop_value; };
    struct Scheduled { double at; Json cue; };
    void music(const std::string& path);
    void ambience(const std::string& path);
    void render(float* out, int count);
    std::string stream(const std::string& path);
    const Pack* pack_;
    AudioEvents events_;
    AudioOutput output_;
    std::map<std::string, Loop> loops_;
    /* NDebugAudioManager.Play's handles (the logic's numbers) -> voices,
     * for its Stop(handle, fadeTime) */
    std::map<int, u32> debug_voices_;
    std::vector<Scheduled> pending_;
    std::string track_, ambience_, act_track_;
    u32 music_id_, ambience_id_, rng_;
    int floor_, context_progress_;
    double clock_, remainder_;
    FILE* recording_;
    u32 recorded_frames_;
    /* cues from the game's thread, the mix on the audio thread */
    Mutex lock_;
    /* the game's thread waiting for lock_ (GameLock): the mix gives it up
     * between chunks */
    volatile int waiting_;
    friend class GameLock;
    Thread thread_;
    volatile bool stopping_;
    /* the audio thread's renders (mix_times), under lock_ */
    double mix_worst_, mix_total_, slow_logged_;
    int mix_count_;
    bool dry_;
    static void thread_main(void* self);
};
}
#endif
