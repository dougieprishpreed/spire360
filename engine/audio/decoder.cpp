#include "decoder.h"
#include "../core/pack.h"
#include "../core/thread.h"
#include <string.h>

#include <deque>
#include <map>

#define STB_VORBIS_NO_PUSHDATA_API
#define STB_VORBIS_NO_INTEGER_CONVERSION
#include "../../third_party/stb/stb_vorbis.c"

namespace spire {

static u16 le16(const u8* p) { return (u16)(p[0] | (p[1] << 8)); }
static u32 le32(const u8* p) { return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24); }
static bool audio_seek(FILE* file, u64 offset) {
#if defined(_MSC_VER)
    return _fseeki64(file, (i64)offset, SEEK_SET) == 0;
#else
    return fseeko(file, (off_t)offset, SEEK_SET) == 0;
#endif
}

/* The kept sounds (decoder.h): a voice starting reads nothing from the pack,
 * where each opened the pack again and read the sound's headers (on the
 * console a few ms from its USB drive, in the frame loop under the audio
 * lock, for each sound an effect plays), and the mix reads none while
 * textures load. Past the budget the least recently played that no voice
 * reads go: kept till exit, the budget filled in a long session and every
 * new sound after it streamed, a file each, until the console's 32 handles
 * ran out and voices did not start ("cannot open"). */
enum { KEPT_MAX = 512 * 1024, KEPT_TOTAL = 12 * 1024 * 1024 };
/* With the loader every sound is read whole, the music's stems too (2-3.5
 * MB each); past the budget only sounds no voice reads go. A Vorbis
 * decoder's setup (its codebooks) is the loader's too: 11-53 ms a voice
 * on a PC, several times that on the console, in the mix or under its lock
 * as each sound started (the music's dropouts as a card's effects began).
 * A sound of up to PCM_MAX bytes of samples is kept decoded (16-bit, little
 * endian, interleaved: a voice starts at once); a longer one as it is, each
 * voice's decoder opened on the loader's thread (OpenJob). */
enum { PCM_MAX = 2 * 1024 * 1024 };
enum { KEPT_LOADING = 0, KEPT_READY = 1, KEPT_FAILED = -1 };
struct KeptSound {
    std::vector<u8> data; /* the file, or (pcm) its samples */
    int users;
    u64 used;
    volatile int state;
    const PackEntry* entry;
    double asked;  /* clock_ms() as it was asked for */
    size_t bytes;  /* counted in g_kept_bytes */
    bool pcm;
    int channels;
    u32 rate, frames;
    void* spare;   /* the decoder the loader opened to measure it: the first voice's */
    KeptSound()
        : users(0), used(0), state(KEPT_READY), entry(NULL), asked(0), bytes(0), pcm(false), channels(0), rate(0),
          frames(0), spare(NULL) {}
    ~KeptSound() {
        if (spare) stb_vorbis_close((stb_vorbis*)spare);
    }
};
/* A voice's decoder opened on the loader's thread. */
struct OpenJob {
    KeptSound* kept; /* one of its users while the job lasts */
    void* vorbis;
    volatile int state; /* KEPT_LOADING until done */
    bool abandoned;     /* the decoder closed first: the loader frees the job */
    OpenJob(KeptSound* k) : kept(k), vorbis(NULL), state(KEPT_LOADING), abandoned(false) {}
};
static Mutex g_kept_lock;
static std::map<std::string, KeptSound*> g_kept;
static size_t g_kept_bytes = 0;
static u64 g_kept_clock = 0;
/* the loader (start_loader); its queues and counts under g_kept_lock */
static const Pack* g_load_pack = NULL;
static size_t g_kept_budget = KEPT_TOTAL;
static std::deque<KeptSound*> g_load_queue;
static std::deque<OpenJob*> g_open_queue;
static Thread g_loader;
static volatile bool g_loader_stop = false;
static AudioDecoder::LoadStats g_load_stats;

/* The least recently played that no voice reads go while the budget has
 * no room for `more` bytes; false if those in use leave none. */
static bool make_room(size_t more, size_t budget) {
    while (g_kept_bytes + more > budget) {
        std::map<std::string, KeptSound*>::iterator oldest = g_kept.end();
        for (std::map<std::string, KeptSound*>::iterator k = g_kept.begin(); k != g_kept.end(); ++k)
            if (!k->second->users && k->second->state != KEPT_LOADING &&
                (oldest == g_kept.end() || k->second->used < oldest->second->used))
                oldest = k;
        if (oldest == g_kept.end()) return false;
        g_kept_bytes -= oldest->second->bytes;
        delete oldest->second;
        g_kept.erase(oldest);
    }
    return true;
}

static KeptSound* kept_sound(const Pack& pack, const PackEntry* entry, const std::string& path) {
    bool loader = g_loader.started();
    if (!loader && entry->size > KEPT_MAX) return NULL;
    ScopedLock lock(g_kept_lock);
    std::map<std::string, KeptSound*>::iterator it = g_kept.find(path);
    if (it != g_kept.end()) {
        ++it->second->users;
        it->second->used = ++g_kept_clock;
        return it->second;
    }
    /* with the loader, one past the budget is read all the same while twice
     * the budget holds it (an act's track crossing over to the next) */
    if (!make_room(entry->size, g_kept_budget) && (!loader || g_kept_bytes + entry->size > 2 * g_kept_budget)) return NULL;
    KeptSound* k = new KeptSound();
    k->entry = entry;
    k->users = 1;
    k->used = ++g_kept_clock;
    if (loader) {
        k->state = KEPT_LOADING;
        k->asked = clock_ms();
        g_load_queue.push_back(k);
    } else if (!pack.read(entry, k->data) || k->data.size() < 12) {
        delete k;
        return NULL;
    }
    k->bytes = entry->size;
    g_kept[path] = k;
    g_kept_bytes += k->bytes;
    return k;
}

static void unkeep(KeptSound* k) {
    ScopedLock lock(g_kept_lock);
    --k->users;
}

/* The loader's read of a sound (ahead of the textures: Pack::read_urgent)
 * and, for a Vorbis one, its setup and length; one short enough decoded
 * whole. Outside the lock but for the result. */
static void load(KeptSound* k) {
    std::vector<u8> data;
    bool ok = g_load_pack->read_urgent(k->entry, data) && data.size() >= 12;
    bool pcm = false;
    int channels = 0;
    u32 rate = 0, frames = 0;
    stb_vorbis* v = NULL;
    if (ok && !memcmp(&data[0], "OggS", 4)) {
        int error = 0;
        v = stb_vorbis_open_memory(&data[0], (int)data.size(), &error, NULL);
        if (v) {
            stb_vorbis_info info = stb_vorbis_get_info(v);
            channels = info.channels == 1 ? 1 : 2;
            rate = info.sample_rate;
            frames = stb_vorbis_stream_length_in_samples(v);
        }
        ok = v && frames && rate;
        if (ok && (size_t)frames * channels * 2 <= PCM_MAX) {
            std::vector<u8> samples((size_t)frames * channels * 2);
            float part[2048];
            u32 got = 0;
            while (got < frames) {
                int want = frames - got > 1024 ? 1024 : (int)(frames - got);
                int n = stb_vorbis_get_samples_float_interleaved(v, channels, part, want * channels);
                if (n <= 0) break;
                u8* out = &samples[(size_t)got * channels * 2];
                for (int i = 0; i < n * channels; ++i) {
                    int s = (int)(clampf(part[i], -1, 1) * 32767.0f);
                    out[i * 2] = (u8)(s & 255);
                    out[i * 2 + 1] = (u8)((s >> 8) & 255);
                }
                got += (u32)n;
            }
            samples.resize((size_t)got * channels * 2);
            data.swap(samples);
            frames = got;
            pcm = true;
            ok = got > 0;
        }
        if (pcm || !ok) {
            if (v) stb_vorbis_close(v);
            v = NULL;
        }
    }
    ScopedLock lock(g_kept_lock);
    k->data.swap(data);
    k->pcm = pcm;
    k->channels = channels;
    k->rate = rate;
    k->frames = frames;
    k->spare = v;
    if (ok) {
        g_kept_bytes = g_kept_bytes - k->bytes + k->data.size();
        k->bytes = k->data.size();
    }
    k->state = ok ? KEPT_READY : KEPT_FAILED;
    ++g_load_stats.loads;
    g_load_stats.bytes += (double)k->entry->size;
    double waited = clock_ms() - k->asked;
    if (waited > g_load_stats.longest_ms) g_load_stats.longest_ms = waited;
}

/* A voice's Vorbis decoder (the one opened to measure the sound, else a new
 * one), outside the lock: the job's sound stays while it is one of its
 * users. */
static void open_job(OpenJob* j) {
    KeptSound* k = j->kept;
    stb_vorbis* v = NULL;
    bool ok = !j->abandoned && k->state == KEPT_READY;
    if (ok && !k->pcm && !memcmp(&k->data[0], "OggS", 4)) {
        {
            ScopedLock lock(g_kept_lock);
            v = (stb_vorbis*)k->spare;
            k->spare = NULL;
        }
        int error = 0;
        if (!v) v = stb_vorbis_open_memory(&k->data[0], (int)k->data.size(), &error, NULL);
        ok = v != NULL;
    }
    ScopedLock lock(g_kept_lock);
    if (j->abandoned) {
        if (v) stb_vorbis_close(v);
        --k->users;
        delete j;
        return;
    }
    j->vorbis = v;
    j->state = ok ? KEPT_READY : KEPT_FAILED;
}

/* The loader's thread: the reads first, then the decoders of the sounds
 * read. */
static void loader_main(void*) {
    while (!g_loader_stop) {
        KeptSound* k = NULL;
        OpenJob* j = NULL;
        {
            ScopedLock lock(g_kept_lock);
            if (!g_load_queue.empty()) {
                k = g_load_queue.front();
                g_load_queue.pop_front();
            } else {
                for (std::deque<OpenJob*>::iterator it = g_open_queue.begin(); it != g_open_queue.end(); ++it)
                    if ((*it)->abandoned || (*it)->kept->state != KEPT_LOADING) {
                        j = *it;
                        g_open_queue.erase(it);
                        break;
                    }
            }
        }
        if (k) load(k);
        else if (j) open_job(j);
        else sleep_ms(1);
    }
}

bool AudioDecoder::start_loader(const Pack* pack, int hw_thread, size_t budget) {
    if (g_loader.started() || !pack) return false;
    g_load_pack = pack;
    g_kept_budget = budget;
    g_loader_stop = false;
    return g_loader.start(loader_main, NULL, hw_thread);
}

void AudioDecoder::stop_loader() {
    g_loader_stop = true;
    g_loader.join();
}

AudioDecoder::LoadStats AudioDecoder::load_stats() {
    ScopedLock lock(g_kept_lock);
    LoadStats s = g_load_stats;
    s.kept = g_kept_bytes;
    g_load_stats.loads = 0;
    g_load_stats.bytes = 0;
    g_load_stats.longest_ms = 0;
    return s;
}

AudioDecoder::AudioDecoder()
    : vorbis_(NULL), pcm_(NULL), kept_(NULL), kept_size_(0), kept_pos_(0), data_offset_(0), frames_(0), position_(0),
      rate_(0), channels_(0), kept_entry_(NULL), job_(NULL), pending_(false), big_(false) {}
AudioDecoder::~AudioDecoder() { close(); }
void AudioDecoder::close() {
    if (job_) {
        ScopedLock lock(g_kept_lock);
        if (job_->state == KEPT_LOADING) {
            job_->abandoned = true;
        } else {
            if (job_->vorbis) stb_vorbis_close((stb_vorbis*)job_->vorbis);
            --job_->kept->users;
            delete job_;
        }
        job_ = NULL;
    }
    if (vorbis_) stb_vorbis_close((stb_vorbis*)vorbis_);
    if (pcm_) fclose(pcm_);
    if (kept_entry_) unkeep(kept_entry_);
    vorbis_ = NULL; pcm_ = NULL;
    kept_entry_ = NULL;
    kept_ = NULL;
    kept_size_ = kept_pos_ = 0;
    frames_ = position_ = rate_ = 0;
    channels_ = 0;
    pending_ = false;
}

/* A kept sound's samples (decoded by the loader). */
void AudioDecoder::open_samples(const KeptSound* k) {
    kept_ = k->data.empty() ? NULL : &k->data[0];
    kept_size_ = (u32)k->data.size();
    data_offset_ = 0;
    kept_pos_ = 0;
    channels_ = k->channels;
    rate_ = k->rate;
    frames_ = k->frames;
}

bool AudioDecoder::poll() {
    if (!pending_ || !job_) return false;
    ScopedLock lock(g_kept_lock);
    if (job_->state == KEPT_LOADING) return false;
    OpenJob* j = job_;
    job_ = NULL;
    pending_ = false;
    bool ok = j->state == KEPT_READY;
    stb_vorbis* v = (stb_vorbis*)j->vorbis;
    --j->kept->users; /* the job's: the decoder is one of its own */
    delete j;
    const KeptSound* k = kept_entry_;
    if (ok && k->pcm) {
        open_samples(k);
        big_ = false;
        return true;
    }
    if (ok && v) {
        vorbis_ = v;
        kept_ = &k->data[0];
        kept_size_ = (u32)k->data.size();
        channels_ = k->channels;
        rate_ = k->rate;
        frames_ = k->frames;
        big_ = true;
        return true;
    }
    /* a WAV */
    if (ok && !v && open_kept(k->data)) return true;
    if (v) stb_vorbis_close(v);
    close();
    return false;
}

bool AudioDecoder::open_kept(const std::vector<u8>& data) {
    const u8* d = &data[0];
    u32 n = (u32)data.size();
    if (!memcmp(d, "OggS", 4)) {
        int error = 0;
        stb_vorbis* v = stb_vorbis_open_memory(d, (int)n, &error, NULL);
        if (!v) return false;
        vorbis_ = v;
        kept_ = d;
        kept_size_ = n;
        stb_vorbis_info info = stb_vorbis_get_info(v);
        channels_ = info.channels == 1 ? 1 : 2;
        rate_ = info.sample_rate;
        frames_ = stb_vorbis_stream_length_in_samples(v);
        if (!frames_ || !rate_) { close(); return false; }
        return true;
    }
    if (memcmp(d, "RIFF", 4) || memcmp(d + 8, "WAVE", 4)) return false;
    u32 pos = 12;
    bool format_ok = false;
    while (pos <= n && n - pos >= 8) {
        const u8* chunk = d + pos;
        u32 size = le32(chunk + 4);
        pos += 8;
        if (size > n - pos) break;
        if (!memcmp(chunk, "fmt ", 4)) {
            if (size < 16) break;
            const u8* f = d + pos;
            channels_ = le16(f + 2);
            rate_ = le32(f + 4);
            format_ok = le16(f) == 1 && le16(f + 14) == 16 && (channels_ == 1 || channels_ == 2) &&
                        le16(f + 12) == channels_ * 2 && rate_ > 0;
        } else if (!memcmp(chunk, "data", 4) && format_ok) {
            kept_ = d;
            kept_size_ = pos + size;
            data_offset_ = pos;
            kept_pos_ = pos;
            frames_ = size / (channels_ * 2);
            return true;
        }
        if (size == n - pos) break;
        pos += size + (size & 1);
    }
    channels_ = 0; rate_ = 0;
    return false;
}

bool AudioDecoder::open(const Pack& pack, const std::string& path) {
    close();
    const PackEntry* entry = pack.find(path.c_str());
    if (!entry || entry->type != PACK_AUDIO) return false;
    big_ = entry->size > KEPT_MAX;
    KeptSound* kept = kept_sound(pack, entry, path);
    if (kept) {
        kept_entry_ = kept;
        if (g_loader.started()) {
            /* an effect's samples or a WAV at once; a Vorbis decoder (or a
             * sound still being read) the loader's */
            ScopedLock lock(g_kept_lock);
            if (kept->state == KEPT_FAILED) {
                close();
                return false;
            }
            bool vorbis = kept->state == KEPT_LOADING || (!kept->pcm && !memcmp(&kept->data[0], "OggS", 4));
            if (!vorbis) {
                if (kept->pcm) open_samples(kept);
                else if (!open_kept(kept->data)) {
                    close();
                    return false;
                }
                return true;
            }
            job_ = new OpenJob(kept);
            ++kept->users;
            g_open_queue.push_back(job_);
            pending_ = true;
            return true;
        }
        if (kept->state == KEPT_READY && open_kept(kept->data)) return true;
        close();
        return false;
    }
    /* the loader keeps every sound: one it cannot is not played */
    if (g_loader.started()) return false;
    std::vector<u8> head;
    if (!pack.read_head(entry, 12, head) || head.size() < 12) return false;
    FILE* input = pack.open_stream(entry);
    if (!input) return false;
    if (!memcmp(&head[0], "OggS", 4)) {
        int error = 0;
        /* Own the file ourselves until open succeeds: the decoder does not
         * close a supplied handle on failure. */
        stb_vorbis* v = stb_vorbis_open_file_section(input, 0, &error, NULL, entry->size);
        if (!v) { fclose(input); return false; }
        vorbis_ = v;
        pcm_ = input;
        stb_vorbis_info info = stb_vorbis_get_info(v);
        channels_ = info.channels == 1 ? 1 : 2;
        rate_ = info.sample_rate;
        frames_ = stb_vorbis_stream_length_in_samples(v);
        if (!frames_ || !rate_) { close(); return false; }
        return true;
    }
    if (memcmp(&head[0], "RIFF", 4) || memcmp(&head[8], "WAVE", 4)) { fclose(input); return false; }
    u32 pos = 12;
    bool format_ok = false;
    while (pos <= entry->size && entry->size - pos >= 8) {
        u8 chunk[24];
        if (!audio_seek(input, entry->offset + pos) || fread(chunk, 1, 8, input) != 8) break;
        u32 size = le32(chunk + 4);
        pos += 8;
        if (size > entry->size - pos) break;
        if (!memcmp(chunk, "fmt ", 4)) {
            if (size < 16 || fread(chunk + 8, 1, 16, input) != 16) break;
            channels_ = le16(chunk + 10);
            rate_ = le32(chunk + 12);
            format_ok = le16(chunk + 8) == 1 && le16(chunk + 22) == 16 &&
                (channels_ == 1 || channels_ == 2) && le16(chunk + 20) == channels_ * 2 && rate_ > 0;
        } else if (!memcmp(chunk, "data", 4) && format_ok) {
            data_offset_ = entry->offset + pos;
            frames_ = size / (channels_ * 2);
            pcm_ = input;
            return true;
        }
        if (size == entry->size - pos) break;
        pos += size + (size & 1);
    }
    fclose(input);
    channels_ = 0; rate_ = 0;
    return false;
}

int AudioDecoder::read(float* out, int count) {
    if (!out || count <= 0 || (!pcm_ && !kept_) || position_ >= frames_) return 0;
    u32 remaining = frames_ - position_;
    if ((u32)count > remaining) count = (int)remaining;
    int got = 0;
    if (vorbis_) {
        got = stb_vorbis_get_samples_float_interleaved((stb_vorbis*)vorbis_, channels_, out, count * channels_);
    } else if (kept_) {
        u32 have = (kept_size_ - kept_pos_) / (u32)(channels_ * 2);
        if ((u32)count > have) count = (int)have;
        const u8* p = kept_ + kept_pos_;
        for (int i = 0; i < count * channels_; ++i) {
            int value = le16(p + i * 2);
            if (value >= 32768) value -= 65536;
            out[i] = value / 32768.0f;
        }
        kept_pos_ += (u32)(count * channels_ * 2);
        got = count;
    } else {
        u8 data[4096];
        while (got < count) {
            int part = count - got;
            if (part > 2048 / channels_) part = 2048 / channels_;
            int samples = (int)fread(data, 2, (size_t)part * channels_, pcm_);
            for (int i = 0; i < samples; ++i) {
                int value = le16(data + i * 2);
                if (value >= 32768) value -= 65536;
                out[got * channels_ + i] = value / 32768.0f;
            }
            got += samples / channels_;
            if (samples != part * channels_) break;
        }
    }
    position_ += (u32)got;
    return got;
}

bool AudioDecoder::seek(u32 frame) {
    if ((!pcm_ && !kept_) || frame > frames_) return false;
    bool ok = true;
    if (vorbis_) ok = stb_vorbis_seek((stb_vorbis*)vorbis_, frame) != 0;
    else if (kept_) kept_pos_ = (u32)(data_offset_ + (u64)frame * channels_ * 2);
    else ok = audio_seek(pcm_, data_offset_ + (u64)frame * channels_ * 2);
    if (ok) position_ = frame;
    return ok;
}
}
