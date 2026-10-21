// Integration tests: DirectSound.
// What the system's DirectSound does with the values the stock's 3D voices ask for (Audio/Bank.cpp, Audio/Voices.cpp):
// Windows refuses a min distance of 0 and a rolloff above DS3D_MAXROLLOFFFACTOR and keeps the values it had; Wine
// takes both. A voice's min distance of 0 (0x005C211E) is therefore not asked for: the buffers keep 1 everywhere.

#include "Framework/Test.h"

#include <windows.h>
#include <mmreg.h>
#include <dsound.h>

namespace {

using oj::test::check;

template <class T>
void release(T*& p) {
    if (p != nullptr) p->Release();
    p = nullptr;
}

}  // namespace

// A 3D buffer as the 3D sets make them (0x005C33C0: 0x400A0 with CTRL3D, mono, 22050 Hz, 16 bits) and the primary
// buffer's listener (0x100D1).
OJ_INTEGRATION_TEST(DirectSound, Refused) {
    IDirectSound8* device = nullptr;
    if (FAILED(DirectSoundCreate8(nullptr, &device, nullptr))) {
        check(false, "a DirectSound 8 device (the tests need one)");
        return;
    }
    device->SetCooperativeLevel(GetDesktopWindow(), DSSCL_PRIORITY);
    DSBUFFERDESC primaryDesc{sizeof(primaryDesc)};
    primaryDesc.dwFlags = 0x100D1;
    IDirectSoundBuffer* primary = nullptr;
    IDirectSound3DListener8* listener = nullptr;
    if (SUCCEEDED(device->CreateSoundBuffer(&primaryDesc, &primary, nullptr))) {
        primary->QueryInterface(IID_IDirectSound3DListener8, reinterpret_cast<void**>(&listener));
    }
    WAVEFORMATEX format{};
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = 1;
    format.nSamplesPerSec = 22050;
    format.wBitsPerSample = 16;
    format.nBlockAlign = 2;
    format.nAvgBytesPerSec = 44100;
    DSBUFFERDESC desc{sizeof(desc)};
    desc.dwFlags = 0x400A0 | DSBCAPS_CTRL3D;
    desc.dwBufferBytes = 44100;
    desc.lpwfxFormat = &format;
    IDirectSoundBuffer* buffer = nullptr;
    IDirectSound3DBuffer8* buffer3d = nullptr;
    if (SUCCEEDED(device->CreateSoundBuffer(&desc, &buffer, nullptr))) {
        buffer->QueryInterface(IID_IDirectSound3DBuffer8, reinterpret_cast<void**>(&buffer3d));
    }
    check(listener != nullptr && buffer3d != nullptr, "the listener and a 3D buffer");
    if (listener != nullptr && buffer3d != nullptr) {
        D3DVALUE made = 0.0f;
        buffer3d->GetMinDistance(&made);
        const HRESULT deferred = buffer3d->SetMinDistance(0.0f, DS3D_DEFERRED);
        listener->CommitDeferredSettings();
        D3DVALUE kept = 0.0f;
        buffer3d->GetMinDistance(&kept);
        check(made == DS3D_DEFAULTMINDISTANCE && deferred == DSERR_INVALIDPARAM && kept == DS3D_DEFAULTMINDISTANCE,
              "a min distance of 0 refused (DSERR_INVALIDPARAM), the buffer's 1 kept");

        D3DVALUE rolloff = 0.0f;
        const HRESULT largest = listener->SetRolloffFactor(3.4028235e38f, DS3D_DEFERRED);
        listener->CommitDeferredSettings();
        listener->GetRolloffFactor(&rolloff);
        check(largest == DSERR_INVALIDPARAM && rolloff == DS3D_DEFAULTROLLOFFFACTOR,
              "the rolloff FLT_MAX refused, the listener's 1 kept");
    }
    release(buffer3d);
    release(buffer);
    release(listener);
    release(primary);
    release(device);
}
