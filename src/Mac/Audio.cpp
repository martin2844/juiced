#include "Audio/Player.h"
#include "Audio/SoundSystem.h"
#include "Audio/Stream.h"
#include "Core/Base/Log.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <dsound.h>
#include <mutex>
namespace {
auto &audioMutex = *new std::recursive_mutex;
auto &voices = *new std::vector<IDirectSoundBuffer *>;
SDL_AudioStream *output = nullptr;
void SDLCALL mix(void *, SDL_AudioStream *stream, int additional, int) {
    std::lock_guard lock(audioMutex);
    int frames = additional / 8;
    if (frames <= 0)
        return;
    std::vector<float> out(frames * 2);
    for (auto *v : voices) {
        if (!v->playing || !v->samples || v->samples->empty())
            continue;
        auto &data = *v->samples;
        size_t length = data.size() / 2;
        double rate = v->frequency / 48000.;
        float left = v->gain * std::min(1.f, 1 - v->pan), right = v->gain * std::min(1.f, 1 + v->pan);
        for (int i = 0; i < frames; i++) {
            if (v->cursor >= length) {
                if (v->loop)
                    v->cursor = fmod(v->cursor, (double)length);
                else {
                    v->playing = false;
                    break;
                }
            }
            size_t a = (size_t)v->cursor, b = std::min(a + 1, length - 1);
            float t = v->cursor - a;
            out[i * 2] += (data[a * 2] * (1 - t) + data[b * 2] * t) * left;
            out[i * 2 + 1] += (data[a * 2 + 1] * (1 - t) + data[b * 2 + 1] * t) * right;
            v->cursor += rate;
        }
    }
    for (float &s : out)
        s = std::clamp(s, -1.f, 1.f);
    SDL_PutAudioStreamData(stream, out.data(), out.size() * 4);
}
void decode(IDirectSoundBuffer &b) {
    if (b.raw.empty())
        return;
    std::vector<uint8_t> wav(44 + b.raw.size());
    auto put = [&](size_t off, uint32_t v) { memcpy(wav.data() + off, &v, 4); };
    memcpy(wav.data(), "RIFF", 4);
    put(4, wav.size() - 8);
    memcpy(wav.data() + 8, "WAVEfmt ", 8);
    put(16, 16);
    memcpy(wav.data() + 20, &b.format, 16);
    memcpy(wav.data() + 36, "data", 4);
    put(40, b.raw.size());
    memcpy(wav.data() + 44, b.raw.data(), b.raw.size());
    SDL_AudioSpec src{};
    Uint8 *samples = nullptr;
    Uint32 bytes = 0;
    if (!SDL_LoadWAV_IO(SDL_IOFromConstMem(wav.data(), wav.size()), true, &src, &samples, &bytes)) {
        oj::log::warning("audio format {}: {}", b.format.wFormatTag, SDL_GetError());
        return;
    }
    SDL_AudioSpec dst{SDL_AUDIO_F32, 2, (int)b.format.nSamplesPerSec};
    Uint8 *converted = nullptr;
    int len = 0;
    if (SDL_ConvertAudioSamples(&src, samples, bytes, &dst, &converted, &len)) {
        b.samples = std::make_shared<std::vector<float>>((float *)converted, (float *)(converted + len));
        SDL_free(converted);
    }
    SDL_free(samples);
}
} // namespace
IDirectSoundBuffer::~IDirectSoundBuffer() {
    std::lock_guard l(audioMutex);
    std::erase(voices, this);
}
int IDirectSoundBuffer::AddRef() {
    return ++refs;
}
int IDirectSoundBuffer::Release() {
    if (--refs == 0) {
        delete this;
        return 0;
    }
    return refs;
}
HRESULT IDirectSoundBuffer::QueryInterface(int id, void **out) {
    *out = nullptr;
    if (id == IID_IDirectSoundBuffer8) {
        *out = this;
        AddRef();
    } else if (id == IID_IDirectSound3DBuffer)
        *out = new IDirectSound3DBuffer(this);
    else if (id == IID_IDirectSound3DListener)
        *out = new IDirectSound3DListener;
    return *out ? S_OK : E_FAIL;
}
HRESULT IDirectSoundBuffer::Lock(DWORD at, DWORD n, void **a, DWORD *na, void **b, DWORD *nb, DWORD) {
    audioMutex.lock();
    if (at >= raw.size() || n > raw.size()) {
        audioMutex.unlock();
        return E_FAIL;
    }
    *na = std::min<uint32_t>(n, raw.size() - at);
    *a = raw.data() + at;
    *nb = n - *na;
    *b = *nb ? raw.data() : nullptr;
    return S_OK;
}
HRESULT IDirectSoundBuffer::Unlock(void *, DWORD, void *, DWORD) {
    decode(*this);
    audioMutex.unlock();
    return S_OK;
}
HRESULT IDirectSoundBuffer::Play(DWORD, DWORD, DWORD flags) {
    std::lock_guard l(audioMutex);
    if (!samples)
        decode(*this);
    loop = flags & 1;
    playing = true;
    return S_OK;
}
HRESULT IDirectSoundBuffer::Stop() {
    std::lock_guard l(audioMutex);
    playing = false;
    return S_OK;
}
HRESULT IDirectSoundBuffer::SetCurrentPosition(DWORD n) {
    std::lock_guard l(audioMutex);
    cursor = format.nBlockAlign ? n / format.nBlockAlign : 0;
    return S_OK;
}
HRESULT IDirectSoundBuffer::GetCurrentPosition(DWORD *play, DWORD *write) {
    std::lock_guard l(audioMutex);
    DWORD at = (uint64_t)cursor * format.nBlockAlign;
    if (play)
        *play = at;
    if (write)
        *write = at;
    return S_OK;
}
HRESULT IDirectSoundBuffer::GetStatus(DWORD *s) {
    std::lock_guard l(audioMutex);
    *s = playing ? (loop ? DSBSTATUS_PLAYING | DSBSTATUS_LOOPING : DSBSTATUS_PLAYING) : 0;
    return S_OK;
}
HRESULT IDirectSoundBuffer::SetVolume(LONG mb) {
    std::lock_guard l(audioMutex);
    gain = mb <= -10000 ? 0 : powf(10.f, std::min(mb, 0) / 2000.f);
    return S_OK;
}
HRESULT IDirectSoundBuffer::SetFrequency(DWORD f) {
    std::lock_guard l(audioMutex);
    frequency = f ? f : format.nSamplesPerSec;
    return S_OK;
}
HRESULT IDirectSoundBuffer::SetPan(LONG mb) {
    std::lock_guard l(audioMutex);
    // DirectSound's pan is attenuation in hundredths of a decibel, whereas
    // the mixer and 3D positioning use a linear channel balance.
    const float value = std::clamp(static_cast<float>(mb), -10000.f, 10000.f);
    const float quiet = std::abs(value) >= 10000.f ? 0.f : std::pow(10.f, -std::abs(value) / 2000.f);
    pan = std::copysign(1.f - quiet, value);
    return S_OK;
}
HRESULT IDirectSound3DBuffer::SetPosition(float x, float, float z, DWORD) {
    std::lock_guard l(audioMutex);
    parent->pan = std::clamp(x / std::max(1.f, std::sqrt(x * x + z * z)), -1.f, 1.f);
    return S_OK;
}
HRESULT IDirectSound8::CreateSoundBuffer(DSBUFFERDESC *d, IDirectSoundBuffer **out, void *) {
    auto b = new IDirectSoundBuffer8;
    if (d->lpwfxFormat)
        b->format = *d->lpwfxFormat;
    else
        b->format = {1, 2, 48000, 192000, 4, 16, 0};
    b->raw.resize(d->dwBufferBytes);
    b->frequency = b->format.nSamplesPerSec;
    {
        std::lock_guard l(audioMutex);
        voices.push_back(b);
    }
    *out = b;
    return S_OK;
}
HRESULT IDirectSound8::DuplicateSoundBuffer(IDirectSoundBuffer *src, IDirectSoundBuffer **out) {
    std::lock_guard l(audioMutex);
    auto b = new IDirectSoundBuffer8;
    b->format = src->format;
    b->raw = src->raw;
    b->samples = src->samples;
    b->frequency = src->frequency;
    voices.push_back(b);
    *out = b;
    return S_OK;
}
namespace oj::audio {
std::unique_ptr<OutputStream> openNativeStream(const std::filesystem::path& file);
namespace {
Answer answer(HRESULT r) { return r == S_OK ? Answer::Ok : SUCCEEDED(r) ? Answer::Done : Answer::Failed; }
class SdlBuffer final : public OutputBuffer {
public:
    IDirectSoundBuffer* buffer;
    IDirectSound3DBuffer* spatial = nullptr;
    explicit SdlBuffer(IDirectSoundBuffer* b) : buffer(b) {}
    ~SdlBuffer() override {
        buffer->Stop();
        if (spatial) spatial->Release();
        buffer->Release();
    }
    Answer write(uint32_t at, const void* data, uint32_t bytes) override {
        if (!bytes) return Answer::Ok;
        void *a=nullptr,*b=nullptr; DWORD na=0,nb=0;
        const auto r=buffer->Lock(at,bytes,&a,&na,&b,&nb,0);
        if (FAILED(r)) return answer(r);
        if (data) {
            memcpy(a,data,na);
            if (nb) memcpy(b,static_cast<const uint8_t*>(data)+na,nb);
        } else { memset(a,0,na); if(nb) memset(b,0,nb); }
        return answer(buffer->Unlock(a,na,b,nb));
    }
    Answer restore() override { return answer(buffer->Restore()); }
    Answer acquireHardware() override { return Answer::Failed; }
    Answer play(uint32_t f) override { return answer(buffer->Play(0,0,f)); }
    Answer stop() override { return answer(buffer->Stop()); }
    Answer setPosition(uint32_t n) override { return answer(buffer->SetCurrentPosition(n)); }
    Answer cursors(uint32_t& p,uint32_t& w) override { return answer(buffer->GetCurrentPosition(&p,&w)); }
    Answer setVolume(int32_t v) override { return answer(buffer->SetVolume(v)); }
    Answer setPan(int32_t v) override { return answer(buffer->SetPan(v)); }
    Answer setFrequency(uint32_t v) override { return answer(buffer->SetFrequency(v)); }
    Answer status(uint32_t& f) override { return answer(buffer->GetStatus(&f)); }
    bool open3d() override {
        if (!spatial) buffer->QueryInterface(IID_IDirectSound3DBuffer,reinterpret_cast<void**>(&spatial));
        return spatial!=nullptr;
    }
    void reset3d() override { if(spatial) { DS3DBUFFER p{};spatial->SetAllParameters(&p,DS3D_DEFERRED); } }
    void place3d(float x,float y,float z) override { if(spatial) spatial->SetPosition(x,y,z,DS3D_DEFERRED); }
    bool openEax() override { return false; }
    bool hasEax() const override { return false; }
    Answer setEax(const GUID&,uint32_t,const void*,uint32_t) override { return Answer::Failed; }
};
class SdlListener final : public OutputListener {
    IDirectSound3DListener listener;
public:
    void setDistanceFactor(float f) override { listener.SetDistanceFactor(f,DS3D_DEFERRED); }
    void setRolloffFactor(float f) override { listener.SetRolloffFactor(f,DS3D_DEFERRED); }
    void setPosition(float x,float y,float z) override { listener.SetPosition(x,y,z,DS3D_DEFERRED); }
    void setOrientation(float x,float y,float z,float ux,float uy,float uz) override { listener.SetOrientation(x,y,z,ux,uy,uz,DS3D_DEFERRED); }
    void commit() override { listener.CommitDeferredSettings(); }
};
class SdlOutput final : public Output {
    IDirectSound8 device;
public:
    ~SdlOutput() override { stop(); }
    bool start(HWND,uint32_t flags,OutputStart& result) override {
        result.flags=(flags & ~kHardware)|kSoftware|kNoEax;
        result.minFrequency=100;result.hardware3dStreaming=0;
        result.eaxDevice=false;result.eaxFlags=0;
        if(flags & kNoSound) return false;
        if(::output) return true;
        SDL_AudioSpec spec{SDL_AUDIO_F32,2,48000};
        ::output=SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,&spec,mix,nullptr);
        if(!::output) { log::warning("audio output: {}",SDL_GetError());return false; }
        SDL_ResumeAudioStreamDevice(::output);return true;
    }
    void stop() override { if(::output) { SDL_DestroyAudioStream(::output);::output=nullptr; } }
    bool hardwarePlays3d(uint32_t) override { return false; }
    bool setEax(const GUID&,uint32_t,const void*,uint32_t) override { return false; }
    bool getEax(const GUID&,uint32_t,void*,uint32_t) override { return false; }
    std::unique_ptr<OutputBuffer> createBuffer(const SampleFormat& f,uint32_t bytes,uint32_t caps) override {
        WAVEFORMATEX format{f.tag,f.channels,f.rate,f.bytesPerSecond,f.blockAlign,f.bits,0};
        DSBUFFERDESC desc{};desc.lpwfxFormat=&format;desc.dwBufferBytes=bytes;desc.dwFlags=caps;
        IDirectSoundBuffer* b=nullptr;
        if(FAILED(device.CreateSoundBuffer(&desc,&b,nullptr))) return {};
        return std::make_unique<SdlBuffer>(b);
    }
    std::unique_ptr<OutputBuffer> duplicate(OutputBuffer& original) override {
        auto* src=dynamic_cast<SdlBuffer*>(&original);if(!src) return {};
        IDirectSoundBuffer* b=nullptr;
        if(FAILED(device.DuplicateSoundBuffer(src->buffer,&b))) return {};
        return std::make_unique<SdlBuffer>(b);
    }
    std::unique_ptr<OutputListener> openListener() override { return std::make_unique<SdlListener>(); }
    std::unique_ptr<OutputStream> openStream(const std::filesystem::path& f) override { return openNativeStream(f); }
    uintptr_t soundSystem() const override { return reinterpret_cast<uintptr_t>(&device); }
};
}
std::unique_ptr<Output> makeOutput() { return std::make_unique<SdlOutput>(); }
} // namespace oj::audio
