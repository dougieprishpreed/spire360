#if defined(_XBOX)
#include <xtl.h>
#include <xaudio2.h>
#define SPIRE_XAUDIO_OUTPUT 1
#elif defined(_WIN32) || defined(__CYGWIN__)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <xaudio2.h>
#define SPIRE_XAUDIO_OUTPUT 1
#endif

#include "output.h"
#include <string.h>

namespace spire {
#if defined(SPIRE_XAUDIO_OUTPUT)
struct AudioOutput::Impl {
    IXAudio2* engine;
    IXAudio2MasteringVoice* master;
    IXAudio2SourceVoice* source;
    float buffers[16][512];
    unsigned cursor;
    unsigned queue;
#if !defined(_XBOX)
    HMODULE library;
    bool com_initialized;
#endif
    Impl() : engine(NULL), master(NULL), source(NULL), cursor(0), queue(4) {
#if !defined(_XBOX)
        library = NULL;
        com_initialized = false;
#endif
    }
    ~Impl() {
        if (source) source->DestroyVoice();
        if (master) master->DestroyVoice();
        if (engine) engine->Release();
#if !defined(_XBOX)
        if (library) FreeLibrary(library);
        if (com_initialized) CoUninitialize();
#endif
    }
};
#else
struct AudioOutput::Impl {};
#endif
AudioOutput::AudioOutput() : impl_(NULL) {}
AudioOutput::~AudioOutput() { close(); }
void AudioOutput::close() { delete impl_; impl_ = NULL; }
bool AudioOutput::open(u32 rate, bool silent) {
    close();
#if defined(SPIRE_XAUDIO_OUTPUT)
    Impl* p = new Impl;
#if defined(_XBOX)
    HRESULT result = XAudio2Create(&p->engine, 0, XAUDIO2_DEFAULT_PROCESSOR);
#else
    HRESULT com = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    p->com_initialized = SUCCEEDED(com);
    if (FAILED(com) && com != RPC_E_CHANGED_MODE) { delete p; return false; }
    p->library = LoadLibraryW(L"xaudio2_9.dll");
    if (!p->library) p->library = LoadLibraryW(L"xaudio2_8.dll");
    if (!p->library) { delete p; return false; }
    typedef HRESULT (WINAPI *Create)(IXAudio2**, UINT32, XAUDIO2_PROCESSOR);
    FARPROC address = GetProcAddress(p->library, "XAudio2Create");
    Create create = NULL;
    memcpy(&create, &address, sizeof(create));
    HRESULT result = create ? create(&p->engine, 0, XAUDIO2_DEFAULT_PROCESSOR) : E_FAIL;
#endif
    if (FAILED(result)) { delete p; return false; }
#if defined(_XBOX)
    result = p->engine->CreateMasteringVoice(&p->master, 2, rate, 0, 0, NULL);
#else
    result = p->engine->CreateMasteringVoice(&p->master, 2, rate, 0, NULL, NULL, AudioCategory_GameEffects);
#endif
    if (FAILED(result)) { delete p; return false; }
    p->master->SetVolume(silent ? 0.0f : 1.0f);
    WAVEFORMATEX format;
    memset(&format, 0, sizeof(format));
    format.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
    format.nChannels = 2;
    format.nSamplesPerSec = rate;
    format.wBitsPerSample = 32;
    format.nBlockAlign = 8;
    format.nAvgBytesPerSec = rate * 8;
    result = p->engine->CreateSourceVoice(&p->source, &format, 0, 1, NULL, NULL, NULL);
    if (FAILED(result) || FAILED(p->source->Start(0, 0))) { delete p; return false; }
    impl_ = p;
    return true;
#else
    (void)rate; (void)silent;
    return false;
#endif
}
int AudioOutput::demand() const {
#if defined(SPIRE_XAUDIO_OUTPUT)
    if (!impl_) return 0;
    XAUDIO2_VOICE_STATE state;
    impl_->source->GetState(&state, 0);
    return state.BuffersQueued < impl_->queue ? 256 : 0;
#else
    return 0;
#endif
}
bool AudioOutput::submit(const float* data, int frames) {
#if defined(SPIRE_XAUDIO_OUTPUT)
    if (!data || frames != 256 || !demand()) return false;
    float* buffer = impl_->buffers[impl_->cursor % 16];
    memcpy(buffer, data, 512 * sizeof(float));
    XAUDIO2_BUFFER packet;
    memset(&packet, 0, sizeof(packet));
    packet.AudioBytes = 512 * sizeof(float);
    packet.pAudioData = (const BYTE*)buffer;
    if (FAILED(impl_->source->SubmitSourceBuffer(&packet, NULL))) return false;
    ++impl_->cursor;
    return true;
#else
    (void)data; (void)frames;
    return false;
#endif
}
void AudioOutput::set_queue(int buffers) {
#if defined(SPIRE_XAUDIO_OUTPUT)
    if (impl_) impl_->queue = buffers < 1 ? 1 : buffers > 15 ? 15 : (unsigned)buffers;
#else
    (void)buffers;
#endif
}
u64 AudioOutput::played_frames() const {
#if defined(SPIRE_XAUDIO_OUTPUT)
    if (!impl_) return 0;
    XAUDIO2_VOICE_STATE state;
    impl_->source->GetState(&state, 0);
    return state.SamplesPlayed;
#else
    return 0;
#endif
}
}
