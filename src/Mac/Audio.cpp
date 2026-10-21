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
    pan = std::clamp(mb / 10000.f, -1.f, 1.f);
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
namespace {
bool focused = true;
}
System &system() {
    static System s;
    return s;
}
void setActive(bool v) {
    focused = v;
}
bool active() {
    return focused;
}
bool System::start(HWND, uint32_t flags) {
    m_flags = (flags & ~kHardware) | kSoftware | kNoEax;
    if (flags & kNoSound)
        return false;
    SDL_AudioSpec spec{SDL_AUDIO_F32, 2, 48000};
    output = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, mix, nullptr);
    if (!output) {
        log::warning("audio output: {}", SDL_GetError());
        return false;
    }
    SDL_ResumeAudioStreamDevice(output);
    m_device = new IDirectSound8;
    DSBUFFERDESC desc{};
    m_device->CreateSoundBuffer(&desc, &m_primary, nullptr);
    m_on = true;
    return true;
}
void System::stop() {
    if (output) {
        SDL_DestroyAudioStream(output);
        output = nullptr;
    }
    if (m_primary) {
        m_primary->Release();
        m_primary = nullptr;
    }
    if (m_device) {
        m_device->Release();
        m_device = nullptr;
    }
    m_on = false;
}
void System::update() {
    Stream::focus(focused);
    Player::updateAll();
}
bool System::hardwarePlays3d() {
    return false;
}
bool System::setEax(const GUID &, uint32_t, const void *, uint32_t) {
    return false;
}
bool System::getEax(const GUID &, uint32_t, void *, uint32_t) {
    return false;
}
int32_t volumeMb(float v) {
    return v > 0 ? std::max(-10000, (int)std::nearbyint(2000 * std::log10(std::min(v, 1.f)))) : -10000;
}
uint32_t clockSeconds() {
    return (uint32_t)(SDL_GetTicks() / 1000);
}
} // namespace oj::audio
