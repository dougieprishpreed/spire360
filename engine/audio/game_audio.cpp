#include "game_audio.h"
#include "../core/cue_ledger.h"
#include "../core/pack.h"
#include <algorithm>

namespace spire {
namespace {
bool is_audio_call(const std::string& call) {
    return call.compare(0, 14, "NAudioManager.") == 0 || call.find("NRunMusicController.") == 0 ||
        call.compare(0, 19, "NDebugAudioManager.") == 0 || call == "NCreature.StartSfxLoop" || call == "NCreature.StopSfxLoop";
}
}
GameAudio::GameAudio(const Pack* pack) : pack_(pack), events_(pack), music_id_(0), ambience_id_(0), rng_(0x39ba1u),
    floor_(-1), context_progress_(-1), clock_(0), remainder_(0), recording_(NULL), recorded_frames_(0), stopping_(false), dry_(false) {
    /* The banks every screen uses, parsed before the first sound (sfx.bank
     * alone is a long parse, a hitch at the first attack otherwise). */
    if (events_.available()) {
        events_.preload("Master.bank");
        events_.preload("sfx.bank");
        events_.preload("ambience.bank");
    }
}
namespace {
void le_word(FILE* file, u32 value, int count) { for (int i = 0; i < count; ++i) fputc((int)((value >> (i * 8)) & 255), file); }
void wave_header(FILE* file, u32 frames) {
    rewind(file); fwrite("RIFF", 1, 4, file); le_word(file, frames * 4 + 36, 4); fwrite("WAVEfmt ", 1, 8, file);
    le_word(file, 16, 4); le_word(file, 1, 2); le_word(file, 2, 2); le_word(file, 48000, 4); le_word(file, 192000, 4);
    le_word(file, 4, 2); le_word(file, 16, 2); fwrite("data", 1, 4, file); le_word(file, frames * 4, 4);
}
}
GameAudio::~GameAudio() {
    stopping_ = true;
    thread_.join();
    if (recording_) { wave_header(recording_, recorded_frames_); fclose(recording_); }
}
bool GameAudio::start_thread(int hw_thread) {
    if (!output_.active() || thread_.started()) return false;
    output_.set_queue(8);
    return thread_.start(thread_main, this, hw_thread);
}
void GameAudio::thread_main(void* self) {
    GameAudio* a = (GameAudio*)self;
    float samples[512];
    while (!a->stopping_) {
        g_trace_audio = "locking";
        {
            ScopedLock lock(a->lock_);
            g_trace_audio = "mixing";
            while (a->output_.demand()) { a->render(samples, 256); if (!a->output_.submit(samples, 256)) break; }
        }
        g_trace_audio = "sleeping";
        sleep_ms(2);
    }
}
bool GameAudio::record(const char* path) {
    ScopedLock lock(lock_);
    if (recording_) { wave_header(recording_, recorded_frames_); fclose(recording_); }
    recording_ = fopen(path, "wb"); recorded_frames_ = 0;
    if (recording_) wave_header(recording_, 0);
    return recording_ != NULL;
}
bool GameAudio::open_output(bool muted) { return output_.open(48000, muted); }

std::string GameAudio::stream(const std::string& path) {
    if (path.compare(0, 6, "event:") == 0 || path.compare(0, 6, "res://") == 0 || path.find('/') != std::string::npos) return path;
    return "debug_audio/" + path;
}
void GameAudio::ui(const std::string& path, float volume) { ScopedLock lock(lock_); events_.play(stream(path), volume); }
void GameAudio::music(const std::string& path) {
    if (track_ == path && events_.playing(music_id_)) return;
    events_.stop(music_id_, 0.4); track_ = path; music_id_ = path.empty() ? 0 : events_.play(path);
}
void GameAudio::ambience(const std::string& path) {
    if (ambience_ == path && events_.playing(ambience_id_)) return;
    events_.stop(ambience_id_, 0.5); ambience_ = path; ambience_id_ = path.empty() ? 0 : events_.play(path);
}
void GameAudio::menu() {
    ScopedLock lock(lock_);
    stop_room_loops(); pending_.clear(); floor_ = -1; context_progress_ = -1; act_track_.clear();
    ambience(""); events_.global_parameter("Progress", 0);
    music("event:/music/menu_update");
}
void GameAudio::stop_room_loops(const std::string& owner) {
    ScopedLock lock(lock_);
    for (std::map<std::string, Loop>::iterator i = loops_.begin(); i != loops_.end();) {
        if (owner.empty() || i->second.owner == owner) { events_.stop(i->second.id, 0.1); loops_.erase(i++); }
        else ++i;
    }
}
void GameAudio::cue(const Json& cue) {
    ScopedLock lock(lock_);
    const std::string& kind = cue["t"].str();
    if (kind == "death" || kind == "despawn") { stop_room_loops(cue["who"].str()); return; }
    if (kind == "ui" || kind == "sfx") CueLedger::used(cue);
    std::string path = cue["path"].str(), operation = cue["op"].str(), owner = cue["who"].str();
    float volume = cue["volume"].f(1);
    float variance = 0;
    std::string loop_parameter = cue.has("loop_param") ? cue["loop_param"].str() : "loop";
    float loop_stop = cue["stop_value"].f(1);
    Json params = cue["params"];
    if (kind == "ui") {
        const std::string& call = cue["call"].str();
        if (!is_audio_call(call)) { CueLedger::retract(cue); return; }
        const Json& a = cue["args"];
        std::string method = call.substr(call.find('.') + 1);
        path = a[(size_t)0].str();
        if (method == "PlayOneShot" || method == "Play") {
            operation = "play";
            if (a[1].is_object()) { params = a[1]; volume = a[2].f(1); }
            else volume = a[1].f(1);
            if (call == "NDebugAudioManager.Play") {
                const std::string& v = a[2].str();
                variance = v == "Small" ? 0.02f : v == "Medium" ? 0.05f : v == "Large" ? 0.1f : v == "TooMuch" ? 0.2f : 0;
            }
        } else if (method == "PlayLoop" || method == "StartSfxLoop") {
            operation = "loop";
            if (a[1].is_bool() && !a[1].b()) loop_parameter.clear();
            if (a[1].is_string()) { loop_parameter = a[1].str(); loop_stop = a[2].f(1); }
        }
        else if (method == "StopLoop" || method == "StopSfxLoop") operation = "stop";
        else if (method == "SetParam") { operation = "param"; params.set(a[1].str(), a[2]); }
        else if (method == "StopAllLoops" || method == "StopAll") { stop_room_loops(); return; }
        else if (call == "NDebugAudioManager.Stop") {
            /* Stop(handle, fadeTime = 0.5): the voice that Play gave it,
             * faded out */
            std::map<int, u32>::iterator v = debug_voices_.find(a[(size_t)0].i(-1));
            if (v == debug_voices_.end()) { CueLedger::retract(cue); return; }
            events_.stop(v->second, a.size() > 1 ? a[1].f(0.5f) : 0.5f);
            debug_voices_.erase(v);
            return;
        }
        else if (method == "PlayMusic" || method == "PlayCustomMusic") { music(path); return; }
        else if (method == "StopCustomMusic") { music(act_track_); events_.global_parameter("Progress", 7); return; }
        else if (method == "StopMusic") { music(""); return; }
        else if (method == "UpdateMusicParameter" || method == "UpdateCustomTrack") {
            if (a[1].is_string()) events_.parameter_label(music_id_, path, a[1].str());
            else events_.parameter(music_id_, path, a[1].f());
            return;
        } else if (method == "TriggerEliteSecondPhase") { events_.global_parameter("Progress", 8); return; }
        else if (method == "TriggerCampfireGoingOut") { events_.parameter(ambience_id_, "Campfire", 1); return; }
        else { CueLedger::retract(cue); return; }
    } else if (kind != "sfx") return;
    if (path.empty()) return;
    std::string loop_key = owner + ":" + path;
    if (operation == "param") {
        for (std::map<std::string, Loop>::iterator i = loops_.begin(); i != loops_.end(); ++i)
            if (i->first == loop_key || (owner.empty() && i->first.substr(i->first.find(':') + 1) == path))
                for (size_t k = 0; k < params.keys().size(); ++k) events_.parameter(i->second.id, params.keys()[k], params.get(params.keys()[k]).f());
    } else if (operation == "stop") {
        if (!loops_.count(loop_key)) return;
        Loop loop = loops_[loop_key]; loops_.erase(loop_key);
        if (loop.parameter.empty()) events_.stop(loop.id, 0.1);
        else events_.parameter(loop.id, loop.parameter, loop.stop_value);
    } else if (operation == "loop") {
        if (loops_.count(loop_key) && events_.playing(loops_[loop_key].id)) return;
        Loop loop; loop.id = events_.play(stream(path), volume, params); loop.owner = owner;
        loop.parameter = loop_parameter;
        loop.stop_value = loop_stop; loops_[loop_key] = loop;
    } else {
        std::string name = stream(path);
        if (name.size() >= 5 && name.substr(name.size() - 5) == ".tres") {
            std::vector<u8> bytes;
            if (pack_->read(name.c_str(), bytes)) {
                std::string error;
                Json doc = Json::parse((const char*)&bytes[0], bytes.size(), &error);
                if (doc["type"].str() == "AudioStreamRandomizer") {
                    int count = doc["resource"]["streams_count"].i();
                    rng_ = rng_ * 1664525u + 1013904223u;
                    if (count > 0) name = doc["resource"].get(format("stream_%u/stream", (rng_ >> 8) % (unsigned)count))["ext"].str();
                }
            }
        }
        u32 id = events_.play(name, volume, params);
        if (variance > 0) { rng_ = rng_ * 1664525u + 1013904223u; events_.pitch(id, 1 + (((rng_ >> 8) / 16777216.0f) * 2 - 1) * variance); }
        if (cue["handle"].is_number()) {
            if (debug_voices_.size() > 64) debug_voices_.clear();
            debug_voices_[cue["handle"].i()] = id;
        }
    }
}
void GameAudio::steps(const Json& steps) {
    ScopedLock lock(lock_);
    double delay = 0;
    for (size_t i = 0; i < steps.size(); ++i) for (size_t k = 0; k < steps[i].size(); ++k) {
        const Json& c = steps[i][k];
        if (c["t"].str() == "wait") delay += c["ms"].num() / 1000;
        else if (c["t"].str() == "sfx" || is_audio_call(c["call"].str())) {
            /* "delay": a sound of a task the game runs alongside */
            Scheduled scheduled; scheduled.at = clock_ + delay + c["delay"].num(0) / 1000; scheduled.cue = c; pending_.push_back(scheduled);
        }
    }
}
void GameAudio::decision(const Json& d) {
    const Json& a = d["context"]["audio"];
    if (a.is_null()) return;
    std::string bank = a["bank"].str();
    size_t slash = bank.find_last_of('/'); if (slash != std::string::npos) bank = bank.substr(slash + 1);
    /* The act's bank and the banks of the tracks about to play, parsed
     * before the lock: the audio thread keeps mixing (a parse under it
     * was a gap in the music). */
    events_.prefer_bank(bank);
    events_.preload(bank);
    events_.preload_for(a["custom"].str().empty() ? a["track"].str() : a["custom"].str());
    events_.preload_for(a["ambience"].str());
    ScopedLock lock(lock_);
    int floor = d["context"]["total_floor"].i();
    if (floor_ != floor) { stop_room_loops(); floor_ = floor; }
    act_track_ = a["track"].str();
    music(a["custom"].str().empty() ? act_track_ : a["custom"].str());
    ambience(a["ambience"].str());
    /* UpdateTrack runs at room/combat changes, not at every card decision.
     * Keep TriggerEliteSecondPhase's Progress=8 until the combat ends. */
    int progress = a["progress"].i();
    if (progress != context_progress_) { events_.global_parameter("Progress", (float)progress); context_progress_ = progress; }
    if (d["context"]["room_type"].str() == "RestSite") events_.parameter(ambience_id_, "Campfire", d["done"].b() ? 1.0f : 0.0f);
    if (d["decision"].str() == "game_over") { stop_room_loops(); music(""); ambience(""); ui("event:/temp/sfx/game_over"); }
}
void GameAudio::render(float* out, int count) {
    int cursor = 0;
    while (cursor < count) {
        int part = count - cursor;
        for (size_t i = 0; i < pending_.size();) {
            if (pending_[i].at <= clock_ + 0.0000001) { cue(pending_[i].cue); pending_.erase(pending_.begin() + i); }
            else { int until = (int)ceil((pending_[i].at - clock_) * 48000 - 0.0001); if (until > 0 && until < part) part = until; ++i; }
        }
        if (dry_) {
            events_.stop_all(0);
            for (int k = 0; k < part * 2; ++k) out[cursor * 2 + k] = 0;
        } else {
            events_.render(out + cursor * 2, part);
        }
        if (recording_ && recorded_frames_ < 0x3fffffffU - (u32)part) {
            for (int k = 0; k < part * 2; ++k) {
                int sample = (int)(clampf(out[cursor * 2 + k], -1, 1) * 32767);
                le_word(recording_, (u32)sample, 2);
            }
            recorded_frames_ += part;
        }
        clock_ += part / 48000.0; cursor += part;
    }
}
void GameAudio::update(double dt) {
    if (thread_.started()) return;
    ScopedLock lock(lock_);
    float samples[512];
    if (output_.active()) {
        while (output_.demand()) { render(samples, 256); if (!output_.submit(samples, 256)) break; }
    } else {
        remainder_ += dt * 48000;
        int frames = (int)remainder_; remainder_ -= frames;
        while (frames > 0) { int part = frames < 256 ? frames : 256; render(samples, part); frames -= part; }
    }
}
}
