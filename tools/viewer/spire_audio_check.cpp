/* Local pack validation: full streaming decode and sample-accurate seeks.
 * The pack and all decoded audio stay on the player's machine. */
#include "../../engine/audio/decoder.h"
#include "../../engine/audio/mixer.h"
#include "../../engine/audio/output.h"
#include "../../engine/audio/events.h"
#include "../../engine/audio/game_audio.h"
#include "../../engine/core/json.h"
#include "../../engine/core/pack.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

using namespace spire;

static int fixture_cases(const Pack& pack) {
    AudioEvents audio(&pack);
    if (!audio.available()) return 1;
    std::vector<float> data(2000);
    const char* names[] = { "layer", "nested", "choice", "delay", "timeline", "repeat", "condition", "bank", "bridge", "fade" };
    int failures = 0;
    for (int i = 0; i < 10; ++i) {
        audio.stop_all();
        Json params; params.set("intensity", Json::make_number(2));
        audio.play(std::string("event:/test/") + names[i], 1, params);
        audio.render(&data[0], 1000);
        for (int f = 0; f < 1000; ++f) {
            float want = f < 480 ? 0.25f : 0;
            if (i == 2) want = -want;
            if (i == 3) want = f >= 48 && f < 528 ? 0.25f : 0;
            if (i == 4) want = f >= 48 && f < 96 ? 0.25f : 0;
            if (i == 5) want = f < 480 ? 0.25f : f < 960 ? -0.25f : 0;
            if (i == 8) want = f < 240 ? 0.25f : f < 336 ? 0.125f : f < 816 ? -0.25f : 0;
            if (i == 9) want = f < 240 ? 0.25f : f < 336 ? 0.25f - (f - 240) * 0.5f / 96 : f < 720 ? -0.25f : 0;
            if (fabsf(data[(size_t)f * 2] - want) > 0.00001f || fabsf(data[(size_t)f * 2 + 1] - want) > 0.00001f) {
                fprintf(stderr, "FAIL event %s frame %d: %.6f expected %.6f\n", names[i], f, data[(size_t)f * 2], want); ++failures; break;
            }
        }
    }
    audio.stop_all(); audio.prefer_bank("other.bank");
    audio.play("event:/test/bank"); audio.render(&data[0], 64);
    if (data[0] != -0.25f) { fprintf(stderr, "FAIL bank mapping\n"); ++failures; }
    audio.stop_all(); audio.play("a.wav"); audio.play("b.wav"); audio.render(&data[0], 64);
    if (fabsf(data[0]) > 0.00001f) { fprintf(stderr, "FAIL imported streams\n"); ++failures; }
    audio.stop_all(); audio.prefer_bank("Master.bank");
    u32 music = audio.play("event:/test/music");
    audio.render(&data[0], 960);
    if (data[0] != 0.25f || data[1800] != 0.25f) { fprintf(stderr, "FAIL music loop\n"); ++failures; }
    audio.global_parameter("Progress", 1);
    audio.render(&data[0], 960);
    if (data[0] != -0.25f || data[900] != -0.25f || data[1800] != 0) { fprintf(stderr, "FAIL music parameter transition\n"); ++failures; }
    audio.stop(music, 0.001); audio.render(&data[0], 128);
    if (audio.active_voices()) { fprintf(stderr, "FAIL event release\n"); ++failures; }
    GameAudio game(&pack);
    const char* decision_text = "{\"context\":{\"total_floor\":0,\"audio\":{\"track\":\"event:/test/music\",\"progress\":6}}}";
    const char* cue_text = "{\"t\":\"ui\",\"call\":\"NRunMusicController.TriggerEliteSecondPhase\"}";
    std::string error;
    Json decision = Json::parse(decision_text, strlen(decision_text), &error);
    Json cue = Json::parse(cue_text, strlen(cue_text), &error);
    game.decision(decision); game.cue(cue); game.decision(decision);
    if (game.events().parameter_value(1, "Progress") != 8) { fprintf(stderr, "FAIL elite phase was reset by another decision\n"); ++failures; }
    audio.stop_all(); audio.global_parameter("Progress", 0);
    u32 ramp = audio.play("event:/test/music"); audio.parameter(ramp, "ramp", 1);
    for (int i = 0; i < 5; ++i) audio.render(&data[0], 960);
    if (fabsf(audio.parameter_value(ramp, "ramp") - 0.2f) > 0.00001f) { fprintf(stderr, "FAIL parameter seek up\n"); ++failures; }
    audio.parameter(ramp, "ramp", 0);
    for (int i = 0; i < 5; ++i) audio.render(&data[0], 960);
    if (fabsf(audio.parameter_value(ramp, "ramp") - 0.1f) > 0.00001f) { fprintf(stderr, "FAIL parameter seek down\n"); ++failures; }
    audio.stop_all();
    u64 scattered = audio.started_voices(); audio.play("event:/test/scatter"); audio.render(&data[0], 1000);
    if (audio.started_voices() - scattered != 42 || data[1968] != 0.25f) {
        fprintf(stderr, "FAIL infinite scatterer scheduling: %.0f spawns, sample %.6f\n", (double)(audio.started_voices() - scattered), data[1968]); ++failures;
    }
    /* the playhead jumps back inside an async box: its sound is not restarted */
    audio.stop_all();
    u64 before_jump = audio.started_voices(); audio.play("event:/test/asyncjump"); audio.render(&data[0], 1000);
    if (audio.started_voices() - before_jump != 1 || data[1998] != 0.25f) {
        fprintf(stderr, "FAIL async box across a jump: %.0f voices started, sample %.6f\n",
                (double)(audio.started_voices() - before_jump), data[1998]); ++failures;
    }
    printf("audio: synthetic event timing, layers, playlists, nested events, conditions, bank selection, loops and streams: %d failures\n", failures);
    return failures;
}

static int event_cases(const Pack& pack) {
    AudioEvents audio(&pack);
    std::vector<u8> raw;
    if (!audio.available() || !pack.read("audio/runtime.json", raw)) return 1;
    std::string error;
    Json index = Json::parse((const char*)&raw[0], raw.size(), &error);
    float buffer[512];
    int events = 0, sounded = 0, streams = 0;
    for (size_t i = 0; i < index["events"].keys().size(); ++i) {
        const std::string& name = index["events"].keys()[i];
        if (name.compare(0, 6, "event:") != 0) continue;
        audio.stop_all();
        audio.play(name);
        u64 before = audio.started_voices();
        for (int k = 0; k < 375; ++k) audio.render(buffer, 256);
        if (audio.started_voices() > before) ++sounded;
        else printf("audio: %s starts no sample within 2 s\n", name.c_str());
        ++events;
    }
    for (size_t i = 0; i < pack.count(); ++i) {
        const PackEntry& entry = pack.entry(i);
        const char* name = pack.name_of(&entry);
        if (entry.type != PACK_AUDIO || strncmp(name, "debug_audio/", 12)) continue;
        audio.stop_all(); audio.play(name); audio.render(buffer, 256); ++streams;
    }
    audio.stop_all(); audio.render(buffer, 256);
    printf("audio: %d named events (%d start samples within 2 s), %d imported streams, %u failures\n", events, sounded, streams, audio.failures());
    return audio.failures() || !events || audio.active_voices() ? 1 : 0;
}

static int music_cases(const Pack& pack) {
    AudioEvents audio(&pack);
    std::vector<u8> raw; std::string error;
    if (!pack.read("audio/runtime.json", raw)) return 1;
    Json index = Json::parse((const char*)&raw[0], raw.size(), &error);
    float samples[512]; int checked = 0, failed = 0;
    const char* boss_parameters[] = { "ceremonial_beast_progress", "vantom_progress", "the_kin_progress",
        "waterfall_giant_progress", "soulfysh_progress", "kaiser_crab_progress", "knowledge_demon_progress",
        "the_insatiable_progress", "queen_progress", "test_subject_progress" };
    for (size_t i = 0; i < index["events"].keys().size(); ++i) {
        const std::string& path = index["events"].keys()[i];
        if (path.find("event:/music/") != 0) continue;
        audio.stop_all(); audio.global_parameter("Progress", 0);
        u32 id = audio.play(path); u64 before = audio.started_voices();
        for (int progress = 0; progress < 10; ++progress) {
            audio.global_parameter("Progress", (float)progress);
            for (size_t p = 0; p < sizeof(boss_parameters) / sizeof(boss_parameters[0]); ++p)
                audio.parameter(id, boss_parameters[p], (float)(progress % 3));
            for (int k = 0; k < 750; ++k) audio.render(samples, 256);
        }
        if (before == audio.started_voices()) { fprintf(stderr, "FAIL music remained silent: %s\n", path.c_str()); ++failed; }
        audio.stop_all(0.2);
        for (int k = 0; k < 60; ++k) audio.render(samples, 256);
        if (audio.active_voices()) { fprintf(stderr, "FAIL music voices survived stop\n"); ++failed; }
        ++checked;
    }
    /* The campfire's crackle (Rest and the fire lit) goes with the rest
     * site: Studio untriggers an instrument whose conditions stop holding
     * (its envelope's release), not at the ambience's next loop */
    audio.stop_all(); audio.render(samples, 256);
    audio.global_parameter("Progress", 3);
    u32 ambience = audio.play("event:/sfx/ambience/act1_ambience");
    audio.parameter(ambience, "Campfire", 0);
    /* the lit fire's loop and, once it is out, the embers' */
    const char* lit_loop = "357aa40c1ab0", *embers = "a954c2e74693";
    for (int k = 0; k < 190; ++k) audio.render(samples, 256);
    std::string lit = audio.describe_voices();
    audio.parameter(ambience, "Campfire", 1);
    for (int k = 0; k < 940; ++k) audio.render(samples, 256);
    std::string out = audio.describe_voices();
    audio.parameter(ambience, "Campfire", 0);
    for (int k = 0; k < 190; ++k) audio.render(samples, 256);
    audio.global_parameter("Progress", 2);
    for (int k = 0; k < 940; ++k) audio.render(samples, 256);
    std::string shop = audio.describe_voices();
    printf("audio: act 1 ambience: fire lit [%s], fire out [%s], lit then the shop [%s]\n", lit.c_str(), out.c_str(),
           shop.c_str());
    if (lit.find(lit_loop) == std::string::npos || out.find(lit_loop) != std::string::npos ||
        out.find(embers) == std::string::npos || shop.find(lit_loop) != std::string::npos) {
        fprintf(stderr, "FAIL the campfire's crackle does not follow the fire and the room\n");
        ++failed;
    }
    audio.stop_all(); audio.global_parameter("Progress", 0);
    printf("audio: %d music events, Progress 0..9 and boss parameters, %u decoder failures, %d failures\n", checked, audio.failures(), failed);
    return failed || audio.failures() ? 1 : 0;
}

static int mixer_cases(const Pack& pack, const char* name) {
    AudioDecoder reference;
    if (!reference.open(pack, name) || reference.frames() < 64) return 0;
    int channels = reference.channels();
    std::vector<float> raw((size_t)64 * channels), mixed(128);
    if (reference.read(&raw[0], 64) != 64) return 1;
    AudioMixer mixer(&pack, reference.rate());
    mixer.play(name, 0.2f); mixer.play(name, 0.3f);
    mixer.pause(true); mixer.render(&mixed[0], 16);
    for (size_t i = 0; i < 32; ++i) if (mixed[i] != 0) return 1;
    mixer.pause(false); mixer.render(&mixed[0], 64);
    for (int f = 0; f < 64; ++f)
        for (int c = 0; c < 2; ++c)
            if (fabsf(mixed[(size_t)f * 2 + c] - clampf(raw[(size_t)f * channels + (channels == 1 ? 0 : c)] * 0.5f, -1, 1)) > 0.00001f)
                return 1;
    mixer.stop_all();
    mixer.play(name, 0.5f, 1, 4, 11, 2);
    mixer.render(&mixed[0], 64);
    for (int f = 0; f < 64; ++f) {
        int source = f < 11 ? f : (f < 25 ? 4 + (f - 11) % 7 : f - 14);
        for (int c = 0; c < 2; ++c)
            if (fabsf(mixed[(size_t)f * 2 + c] - clampf(raw[(size_t)source * channels + (channels == 1 ? 0 : c)] * 0.5f, -1, 1)) > 0.00001f)
                return 1;
    }
    mixer.stop_all();
    u32 voice = mixer.play(name, 1, 0.5f);
    mixer.render(&mixed[0], 64);
    for (int f = 0; f < 64; ++f)
        for (int c = 0; c < 2; ++c) {
            int ch = channels == 1 ? 0 : c;
            float a = raw[(size_t)(f / 2) * channels + ch], b = raw[(size_t)(f / 2 + 1) * channels + ch];
            float value = f & 1 ? (a + b) * 0.5f : a;
            if (fabsf(mixed[(size_t)f * 2 + c] - clampf(value, -1, 1)) > 0.00001f) return 1;
        }
    mixer.stop(voice, 3.0 / reference.rate());
    mixer.render(&mixed[0], 16);
    if (mixer.active_voices() || mixer.buffer_bytes()) return 1;
    for (int i = 6; i < 32; ++i) if (mixed[(size_t)i] != 0) return 1;
    voice = mixer.play(name, 1, 1, 4, 11, -1);
    mixer.stop(voice, 0.25 / reference.rate());
    mixer.render(&mixed[0], 16);
    if (mixer.active_voices()) return 1;
    printf("audio: mixing, loops, resampling, pause and fade checks passed\n");
    return 0;
}

int main(int argc, char** argv) {
    if (argc == 2 && !strcmp(argv[1], "--device-check")) {
        AudioOutput output;
        if (!output.open(48000, true)) { fprintf(stderr, "audio: XAudio2 device unavailable\n"); return 1; }
        float silence[512] = { 0 };
        int submitted = 0;
        for (int tick = 0; tick < 200 && submitted < 16; ++tick) {
            while (submitted < 16 && output.demand()) {
                if (!output.submit(silence, 256)) { fprintf(stderr, "FAIL XAudio2 submit\n"); return 1; }
                ++submitted;
            }
            usleep(1000);
        }
        printf("audio: XAudio2 %d buffers submitted, %.0f frames consumed (muted)\n", submitted, (double)output.played_frames());
        return submitted == 16 && output.played_frames() > 0 ? 0 : 1;
    }
    if (argc == 4 && !strcmp(argv[2], "--event-levels")) {
        /* An event's loudness over its first 3 s, 50 ms at a time (where
         * its sounds fall: a hit timed inside an attack's event). */
        Pack pack;
        std::string error;
        if (!pack.open(argv[1], &error)) { fprintf(stderr, "%s\n", error.c_str()); return 2; }
        AudioEvents audio(&pack);
        if (!audio.available()) return 1;
        audio.play(argv[3]);
        float buffer[2 * 2400];
        for (int w = 0; w < 60; ++w) {
            audio.render(buffer, 2400);
            double sum = 0;
            for (int i = 0; i < 2 * 2400; ++i) sum += (double)buffer[i] * buffer[i];
            printf("%4d ms %6.3f %s\n", w * 50, sqrt(sum / (2 * 2400)),
                   std::string((size_t)(sqrt(sum / (2 * 2400)) * 200), '#').c_str());
        }
        return 0;
    }
    if (argc < 2 || argc > 3) { fprintf(stderr, "usage: spire_audio_check <pack.s3p> [--mix-only] | --device-check\n"); return 2; }
    bool mix_only = argc == 3 && !strcmp(argv[2], "--mix-only");
    Pack pack;
    std::string error;
    if (!pack.open(argv[1], &error)) { fprintf(stderr, "%s\n", error.c_str()); return 2; }
    if (argc == 3 && !strcmp(argv[2], "--fixtures")) return fixture_cases(pack) ? 1 : 0;
    if (argc == 3 && !strcmp(argv[2], "--events")) return event_cases(pack);
    if (argc == 3 && !strcmp(argv[2], "--music")) return music_cases(pack);
    int checked = 0, failures = 0;
    u64 total = 0;
    for (size_t e = 0; e < pack.count(); ++e) {
        const PackEntry& entry = pack.entry(e);
        if (entry.type != PACK_AUDIO) continue;
        const char* name = pack.name_of(&entry);
        if (!checked && mixer_cases(pack, name)) { fprintf(stderr, "FAIL mixer: %s\n", name); ++failures; }
        if (mix_only) return failures ? 1 : 0;
        AudioDecoder decoder;
        if (!decoder.open(pack, name)) { fprintf(stderr, "FAIL open: %s\n", name); ++failures; continue; }
        u32 frames = 0, middle = decoder.frames() / 2;
        std::vector<float> block((size_t)1024 * decoder.channels()), reference;
        bool valid = true;
        while (frames < decoder.frames()) {
            int want = 1024;
            if (frames < middle && middle - frames < (u32)want) want = (int)(middle - frames);
            int count = decoder.read(&block[0], want);
            if (!count) break;
            if (frames == middle) reference.assign(block.begin(), block.begin() + count * decoder.channels());
            for (int i = 0; i < count * decoder.channels(); ++i)
                if (!(block[(size_t)i] >= -64 && block[(size_t)i] <= 64)) valid = false;
            frames += (u32)count;
        }
        if (frames != decoder.frames() || decoder.read(&block[0], 1) != 0 || !valid) {
            fprintf(stderr, "FAIL decode: %s (%u/%u frames)\n", name, frames, decoder.frames()); ++failures;
        }
        if (!reference.empty()) {
            bool same = decoder.seek(middle);
            int got = same ? decoder.read(&block[0], (int)reference.size() / decoder.channels()) : 0;
            same = same && got * decoder.channels() == (int)reference.size();
            for (size_t i = 0; same && i < reference.size(); ++i)
                if (fabsf(block[i] - reference[i]) > 0.00001f) same = false;
            if (!same) { fprintf(stderr, "FAIL seek: %s at %u\n", name, middle); ++failures; }
        }
        total += frames;
        if (++checked % 200 == 0) { printf("audio: %d samples decoded\n", checked); fflush(stdout); }
    }
    if (!checked) { fprintf(stderr, "FAIL no audio entries\n"); ++failures; }
    printf("audio: %d samples, %.0f decoded frames, %d failures\n", checked, (double)total, failures);
    return failures ? 1 : 0;
}
