// Exercise the real native mix callback and SDL's PCM queue, with only the
// hardware endpoint replaced. This test neither requires nor plays to speakers.
#include "Audio/SoundSystem.h"
#include <SDL3/SDL.h>
#include <dsound.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

namespace {
SDL_AudioStreamCallback callback = nullptr;
void* context = nullptr;
SDL_AudioStream* endpoint = nullptr;
int failures = 0;
int checks = 0;

void check(bool ok, const char* what) {
    ++checks;
    failures += !ok;
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", what);
}

std::vector<float> render(int frames) {
    SDL_ClearAudioStream(endpoint);
    callback(context, endpoint, frames * 8, frames * 8);
    std::vector<float> pcm(frames * 2, -99.f);
    const int bytes = SDL_GetAudioStreamData(endpoint, pcm.data(), frames * 8);
    check(bytes == frames * 8, "native callback queued exactly the requested float stereo frames");
    return pcm;
}

bool equal(const std::vector<float>& actual, const std::vector<float>& expected) {
    return actual.size() == expected.size() &&
           std::equal(actual.begin(), actual.end(), expected.begin(),
                      [](float a, float b) { return std::isfinite(a) && std::abs(a - b) < 0.000002f; });
}

IDirectSoundBuffer* buffer(std::vector<float> pcm) {
    WAVEFORMATEX format{1, 2, 48000, 192000, 4, 16, 0};
    DSBUFFERDESC desc{};
    desc.lpwfxFormat = &format;
    IDirectSoundBuffer* out = nullptr;
    oj::audio::system().device()->CreateSoundBuffer(&desc, &out, nullptr);
    out->samples = std::make_shared<std::vector<float>>(std::move(pcm));
    return out;
}

void balance(IDirectSoundBuffer* voice, LONG pan, LONG volume, float left, float right, const char* what) {
    voice->Stop();
    voice->SetCurrentPosition(0);
    voice->SetPan(pan);
    voice->SetVolume(volume);
    voice->Play(0, 0, DSBPLAY_LOOPING);
    const auto pcm = render(2);
    std::printf("PAN mb=%d volume=%d left=%.9g right=%.9g expected_left=%.9g expected_right=%.9g\n",
                static_cast<int>(pan), static_cast<int>(volume), pcm[0], pcm[1], left, right);
    check(equal(pcm, {left, right, left, right}), what);
}
} // namespace

extern "C" SDL_AudioStream* SDLCALL __wrap_SDL_OpenAudioDeviceStream(
    SDL_AudioDeviceID, const SDL_AudioSpec* spec, SDL_AudioStreamCallback mix, void* userdata) {
    if (spec->format != SDL_AUDIO_F32 || spec->channels != 2 || spec->freq != 48000) return nullptr;
    callback = mix;
    context = userdata;
    endpoint = SDL_CreateAudioStream(spec, spec);
    return endpoint;
}

extern "C" bool SDLCALL __wrap_SDL_ResumeAudioStreamDevice(SDL_AudioStream* stream) {
    return stream == endpoint;
}

int main() {
    auto& sound = oj::audio::system();
    if (!sound.start(nullptr, oj::audio::kNoEax) || !callback || !endpoint) {
        std::fprintf(stderr, "native sound system/endpoint failed: %s\n", SDL_GetError());
        return 2;
    }
    auto* voice = buffer({0.5f, 0.5f, 0.5f, 0.5f});
    balance(voice, 0, 0, 0.5f, 0.5f, "center leaves both channels unchanged");
    balance(voice, 2000, 0, 0.05f, 0.5f, "+20dB pan attenuates only the left channel tenfold");
    balance(voice, -2000, 0, 0.5f, 0.05f, "-20dB pan attenuates only the right channel tenfold");
    balance(voice, 602, 0, 0.2500173f, 0.5f, "+6.02dB pan nearly halves only the left channel");
    balance(voice, -602, 0, 0.5f, 0.2500173f, "-6.02dB pan nearly halves only the right channel");
    balance(voice, 2000, -2000, 0.005f, 0.05f, "pan attenuation multiplies volume attenuation");
    balance(voice, 10000, 0, 0.f, 0.5f, "far right silences only the left channel");
    balance(voice, -10000, 0, 0.5f, 0.f, "far left silences only the right channel");
    balance(voice, 0, -10000, 0.f, 0.f, "mute silences both channels");
    voice->Stop();
    check(equal(render(2), {0, 0, 0, 0}), "stopped voices emit silence");
    voice->SetPan(0);
    voice->SetVolume(0);
    voice->SetCurrentPosition(0);
    voice->Play(0, 0, 0);
    check(equal(render(4), {0.5f, 0.5f, 0.5f, 0.5f, 0, 0, 0, 0}), "one-shot ends with silence at sample boundary");
    DWORD status = ~0u;
    voice->GetStatus(&status);
    check(status == 0, "completed one-shot reports stopped");
    voice->Release();

    auto* rate = buffer({0.f, 0.f, 0.5f, -0.5f, 0.f, 0.f});
    rate->SetFrequency(24000);
    rate->Play(0, 0, 0);
    check(equal(render(4), {0, 0, 0.25f, -0.25f, 0.5f, -0.5f, 0.25f, -0.25f}),
          "half-rate playback interpolates the actual stereo samples");
    rate->Stop();
    rate->SetFrequency(48000);
    rate->SetCurrentPosition(4);
    rate->Play(0, 0, 0);
    check(equal(render(1), {0.5f, -0.5f}), "byte seek resumes at the requested PCM frame");
    rate->Release();

    auto* first = buffer({0.75f, -0.75f});
    auto* second = buffer({0.75f, -0.75f});
    first->Play(0, 0, DSBPLAY_LOOPING);
    second->Play(0, 0, DSBPLAY_LOOPING);
    check(equal(render(2), {1, -1, 1, -1}), "simultaneous voices mix and saturate without wrapping");
    first->Release();
    second->Release();

    auto* spatial = buffer({0.5f, 0.5f});
    IDirectSound3DBuffer* position = nullptr;
    spatial->QueryInterface(IID_IDirectSound3DBuffer, reinterpret_cast<void**>(&position));
    position->SetPosition(10, 0, 0, DS3D_DEFERRED);
    spatial->Play(0, 0, DSBPLAY_LOOPING);
    check(equal(render(1), {0, 0.5f}), "3D right emitter keeps its directional channel");
    position->SetPosition(-10, 0, 0, DS3D_DEFERRED);
    check(equal(render(1), {0.5f, 0}), "3D left emitter reverses the directional channel");
    position->SetPosition(0, 0, 10, DS3D_DEFERRED);
    check(equal(render(1), {0.5f, 0.5f}), "3D forward emitter stays centered");
    position->Release();
    spatial->Release();

    WAVEFORMATEX format{1, 2, 48000, 192000, 4, 16, 0};
    DSBUFFERDESC desc{};
    desc.lpwfxFormat = &format;
    desc.dwBufferBytes = 8;
    IDirectSoundBuffer* decoded = nullptr;
    sound.device()->CreateSoundBuffer(&desc, &decoded, nullptr);
    const int16_t source[4]{16384, -8192, -16384, 8192};
    void* a = nullptr;
    void* b = nullptr;
    DWORD na = 0, nb = 0;
    const bool locked = decoded->Lock(0, sizeof source, &a, &na, &b, &nb, 0) == S_OK;
    check(locked && na == sizeof source && nb == 0, "raw PCM lock exposes the requested sample storage");
    if (locked) {
        std::memcpy(a, source, sizeof source);
        decoded->Unlock(a, na, b, nb);
        decoded->Play(0, 0, 0);
        check(equal(render(2), {0.5f, -0.25f, -0.5f, 0.25f}),
              "raw signed16 stereo decodes through SDL and reaches the native mix unchanged");
    }
    decoded->Release();
    sound.stop();
    std::printf("%d passed, %d failed\n", checks - failures, failures);
    return failures ? 1 : 0;
}
