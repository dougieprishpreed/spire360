/* NAudioManager / NRunMusicController presentation side. Sound cues never
 * extend combat input waits; device and headless rendering share one clock. */
#ifndef SPIRE_GAME_AUDIO_H
#define SPIRE_GAME_AUDIO_H
#include "events.h"
#include "output.h"
#include <map>
#include <stdio.h>
namespace spire {
class GameAudio {
public:
    explicit GameAudio(const Pack* pack);
    ~GameAudio();
    bool open_output(bool muted = false);
    bool record(const char* path);
    void update(double seconds);
    void cue(const Json& cue);
    void steps(const Json& steps);
    void decision(const Json& decision);
    void menu();
    void ui(const std::string& path, float volume = 1);
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
    std::vector<Scheduled> pending_;
    std::string track_, ambience_, act_track_;
    u32 music_id_, ambience_id_, rng_;
    int floor_, context_progress_;
    double clock_, remainder_;
    FILE* recording_;
    u32 recorded_frames_;
};
}
#endif
