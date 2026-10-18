#include "mixer.h"
#include "decoder.h"
#include "../core/pack.h"
#include <string.h>

namespace spire {
struct AudioMixer::Voice {
    AudioDecoder decoder;
    std::vector<float> buffer;
    u32 id, cursor, loop_start, loop_end;
    int loops, used, count;
    float left, right, next_left, next_right, gain, gain_step, pitch;
    u32 gain_frames;
    double fraction;
    bool next_valid, alive, stop_after_ramp;
    Voice() : buffer(4096), id(0), cursor(0), loop_start(0), loop_end(0), loops(0), used(0), count(0),
        left(0), right(0), next_left(0), next_right(0), gain(1), gain_step(0), pitch(1), gain_frames(0),
        fraction(0), next_valid(false), alive(true), stop_after_ramp(false) {}
    bool fetch(float& l, float& r) {
        if (cursor == loop_end && loops != 0) {
            if (!decoder.seek(loop_start)) return false;
            cursor = loop_start; used = count = 0;
            if (loops > 0) --loops;
        }
        if (cursor >= decoder.frames()) return false;
        if (used >= count) {
            u32 end = loops != 0 && cursor < loop_end ? loop_end : decoder.frames();
            int frames = (int)(end - cursor);
            if (frames > 2048) frames = 2048;
            count = decoder.read(&buffer[0], frames);
            used = 0;
            if (!count) return false;
        }
        int channels = decoder.channels();
        l = buffer[(size_t)used * channels];
        r = channels == 1 ? l : buffer[(size_t)used * channels + 1];
        ++used; ++cursor;
        return true;
    }
};

AudioMixer::AudioMixer(const Pack* pack, u32 rate) : pack_(pack), rate_(rate ? rate : 48000), next_id_(1), paused_(false) {}
AudioMixer::~AudioMixer() { stop_all(); }

u32 AudioMixer::play(const std::string& path, float gain, float ratio, u32 start, u32 end, int loops, u32 offset) {
    if (!pack_ || voices_.size() >= 64) return 0;
    Voice* v = new Voice;
    if (!v->decoder.open(*pack_, path) || offset >= v->decoder.frames() || !v->decoder.seek(offset)) { delete v; return 0; }
    if (loops != 0) {
        if (!end) end = v->decoder.frames();
        if (start >= end || end > v->decoder.frames() || offset >= end) { delete v; return 0; }
        v->loop_start = start; v->loop_end = end; v->loops = loops;
    }
    v->cursor = offset;
    v->gain = maxf(gain, 0);
    v->pitch = clampf(ratio, 0.125f, 8);
    if (!v->fetch(v->left, v->right)) { delete v; return 0; }
    v->next_valid = v->fetch(v->next_left, v->next_right);
    if (!v->next_valid) { v->next_left = v->left; v->next_right = v->right; }
    v->id = next_id_++;
    if (!next_id_) ++next_id_;
    voices_.push_back(v);
    return v->id;
}

AudioMixer::Voice* AudioMixer::find(u32 id) {
    for (size_t i = 0; i < voices_.size(); ++i) if (voices_[i]->id == id) return voices_[i];
    return NULL;
}
bool AudioMixer::playing(u32 id) const {
    for (size_t i = 0; i < voices_.size(); ++i) if (voices_[i]->id == id) return true;
    return false;
}
double AudioMixer::remaining(u32 id) const {
    for (size_t i = 0; i < voices_.size(); ++i) if (voices_[i]->id == id) {
        const Voice& v = *voices_[i];
        if (v.loops < 0) return -1;
        double frames = (double)v.decoder.frames() - v.cursor + (v.next_valid ? 2 : 1) - v.fraction;
        frames += (double)v.loops * (v.loop_end - v.loop_start);
        return frames / (v.decoder.rate() * v.pitch);
    }
    return 0;
}
void AudioMixer::volume(u32 id, float gain, double seconds) {
    Voice* v = find(id);
    if (!v) return;
    gain = maxf(gain, 0);
    v->gain_frames = seconds > 0 ? (u32)(seconds * rate_) : 0;
    if (seconds > 0 && !v->gain_frames) v->gain_frames = 1;
    if (v->gain_frames) v->gain_step = (gain - v->gain) / v->gain_frames;
    else v->gain = gain;
}
void AudioMixer::pitch(u32 id, float ratio) { Voice* v = find(id); if (v) v->pitch = clampf(ratio, 0.125f, 8); }
void AudioMixer::stop(u32 id, double seconds) {
    Voice* v = find(id);
    if (!v) return;
    if (seconds > 0) { volume(id, 0, seconds); v->stop_after_ramp = true; }
    else {
        for (size_t i = 0; i < voices_.size(); ++i) if (voices_[i] == v) {
            voices_.erase(voices_.begin() + i); delete v; break;
        }
    }
}
void AudioMixer::stop_all(double seconds) {
    if (seconds > 0) {
        for (size_t i = 0; i < voices_.size(); ++i) { volume(voices_[i]->id, 0, seconds); voices_[i]->stop_after_ramp = true; }
    } else {
        for (size_t i = 0; i < voices_.size(); ++i) delete voices_[i];
        voices_.clear();
    }
}
size_t AudioMixer::buffer_bytes() const {
    size_t bytes = 0;
    for (size_t i = 0; i < voices_.size(); ++i) bytes += voices_[i]->buffer.size() * sizeof(float);
    return bytes;
}
void AudioMixer::render(float* out, int frames) {
    if (!out || frames <= 0) return;
    memset(out, 0, (size_t)frames * 2 * sizeof(float));
    if (paused_) return;
    for (size_t i = 0; i < voices_.size(); ++i) {
        Voice& v = *voices_[i];
        double step = (double)v.decoder.rate() * v.pitch / rate_;
        for (int f = 0; f < frames && v.alive; ++f) {
            float fraction = (float)v.fraction;
            out[f * 2] += (v.left + (v.next_left - v.left) * fraction) * v.gain;
            out[f * 2 + 1] += (v.right + (v.next_right - v.right) * fraction) * v.gain;
            if (v.gain_frames) {
                v.gain += v.gain_step;
                if (!--v.gain_frames && v.stop_after_ramp) v.alive = false;
            }
            v.fraction += step;
            while (v.fraction >= 1 && v.alive) {
                v.fraction -= 1;
                if (!v.next_valid) { v.alive = false; break; }
                v.left = v.next_left; v.right = v.next_right;
                v.next_valid = v.fetch(v.next_left, v.next_right);
                if (!v.next_valid) { v.next_left = v.left; v.next_right = v.right; }
            }
        }
    }
    for (size_t i = 0; i < voices_.size();) {
        if (!voices_[i]->alive) { delete voices_[i]; voices_.erase(voices_.begin() + i); }
        else ++i;
    }
    for (int f = 0; f < frames * 2; ++f) out[f] = clampf(out[f], -1, 1);
}
}
