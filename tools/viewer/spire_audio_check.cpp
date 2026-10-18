/* Local pack validation: full streaming decode and sample-accurate seeks.
 * The pack and all decoded audio stay on the player's machine. */
#include "../../engine/audio/decoder.h"
#include "../../engine/audio/mixer.h"
#include "../../engine/audio/output.h"
#include "../../engine/core/json.h"
#include "../../engine/core/pack.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

using namespace spire;

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
    if (argc < 2 || argc > 3) { fprintf(stderr, "usage: spire_audio_check <pack.s3p> [--mix-only] | --device-check\n"); return 2; }
    bool mix_only = argc == 3 && !strcmp(argv[2], "--mix-only");
    Pack pack;
    std::string error;
    if (!pack.open(argv[1], &error)) { fprintf(stderr, "%s\n", error.c_str()); return 2; }
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
