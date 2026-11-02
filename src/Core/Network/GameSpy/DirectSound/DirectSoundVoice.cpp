#include "Core/Network/GameSpy/DirectSound/DirectSoundVoice.h"

#include <windows.h>
#include <mmreg.h>
#include <dsound.h>

#include <cstring>

#pragma comment(lib, "dsound.lib")
#pragma comment(lib, "dxguid.lib")

namespace oj::net::gv {

namespace {

GUID guidOf(const DeviceId& id) {
    GUID guid{};
    std::memcpy(&guid, id.data(), 16);
    return guid;
}

// The DirectSound object and its secondary buffer: the buffer released, then the object (0x0057EC70).
class DirectSoundPlayback final : public PlaybackBuffer {
public:
    DirectSoundPlayback(IDirectSound* sound, IDirectSoundBuffer* buffer) : m_sound(sound), m_buffer(buffer) {}
    ~DirectSoundPlayback() override {
        m_buffer->Release();
        m_sound->Release();
    }

    bool playCursor(uint32_t& play) override {
        DWORD p = 0;
        DWORD write = 0;
        if (FAILED(m_buffer->GetCurrentPosition(&p, &write))) return false;
        play = p;
        return true;
    }
    bool lock(uint32_t at, uint32_t bytes, Locked& locked) override {
        DWORD b1 = 0;
        DWORD b2 = 0;
        const DWORD flags = bytes == 0 ? DSBLOCK_ENTIREBUFFER : 0;
        if (FAILED(m_buffer->Lock(at, bytes, &locked.first, &b1, &locked.second, &b2, flags))) return false;
        locked.firstBytes = b1;
        locked.secondBytes = b2;
        return true;
    }
    bool unlock(const Locked& locked) override {
        return SUCCEEDED(m_buffer->Unlock(locked.first, locked.firstBytes, locked.second, locked.secondBytes));
    }
    void stop() override { m_buffer->Stop(); }
    bool rewind() override { return SUCCEEDED(m_buffer->SetCurrentPosition(0)); }
    bool playLooping() override { return SUCCEEDED(m_buffer->Play(0, 0, DSBPLAY_LOOPING)); }
    bool playing() override {
        DWORD status = 0;
        if (FAILED(m_buffer->GetStatus(&status))) return false;
        return (status & DSBSTATUS_PLAYING) != 0;
    }
    void setVolume(int32_t mb) override { m_buffer->SetVolume(mb); }
    bool volume(int32_t& mb) override {
        LONG v = 0;
        if (FAILED(m_buffer->GetVolume(&v))) return false;
        mb = v;
        return true;
    }

private:
    IDirectSound* m_sound;
    IDirectSoundBuffer* m_buffer;
};

// The DirectSoundCapture object and its capture buffer: the buffer stopped and released, then the object (0x0057EC70).
class DirectSoundCapture final : public CaptureBuffer {
public:
    DirectSoundCapture(IDirectSoundCapture* capture, IDirectSoundCaptureBuffer* buffer)
        : m_capture(capture), m_buffer(buffer) {}
    ~DirectSoundCapture() override {
        m_buffer->Stop();
        m_buffer->Release();
        m_capture->Release();
    }

    bool readCursor(uint32_t& read) override {
        DWORD r = 0;
        if (FAILED(m_buffer->GetCurrentPosition(nullptr, &r))) return false;
        read = r;
        return true;
    }
    bool lock(uint32_t at, uint32_t bytes, Locked& locked) override {
        DWORD b1 = 0;
        DWORD b2 = 0;
        if (FAILED(m_buffer->Lock(at, bytes, &locked.first, &b1, &locked.second, &b2, 0))) return false;
        locked.firstBytes = b1;
        locked.secondBytes = b2;
        return true;
    }
    bool unlock(const Locked& locked) override {
        return SUCCEEDED(m_buffer->Unlock(locked.first, locked.firstBytes, locked.second, locked.secondBytes));
    }

private:
    IDirectSoundCapture* m_capture;
    IDirectSoundCaptureBuffer* m_buffer;
};

// LPDSENUMCALLBACKA: the device to the caller's function.
BOOL CALLBACK onDevice(LPGUID guid, LPCSTR description, LPCSTR, LPVOID context) {
    const auto& each = *static_cast<const VoiceSound::Listed*>(context);
    DeviceId id{};
    if (guid != nullptr) std::memcpy(id.data(), guid, 16);
    return each(guid != nullptr ? &id : nullptr, description) ? TRUE : FALSE;
}

}  // namespace

void DirectSoundVoice::list(bool capture, const Listed& each) {
    if (capture) {
        DirectSoundCaptureEnumerateA(onDevice, const_cast<Listed*>(&each));
    } else {
        DirectSoundEnumerateA(onDevice, const_cast<Listed*>(&each));
    }
}

DeviceId DirectSoundVoice::defaultDevice(bool capture) {
    GUID guid{};
    GetDeviceID(capture ? &DSDEVID_DefaultVoiceCapture : &DSDEVID_DefaultVoicePlayback, &guid);
    DeviceId id{};
    std::memcpy(id.data(), &guid, 16);
    return id;
}

std::unique_ptr<PlaybackBuffer> DirectSoundVoice::openPlayback(const DeviceId& id, HWND window,
                                                               const tWAVEFORMATEX& format, uint32_t bytes) {
    const GUID guid = guidOf(id);
    IDirectSound* sound = nullptr;
    if (FAILED(DirectSoundCreate(&guid, &sound, nullptr))) return nullptr;
    IDirectSoundBuffer* buffer = nullptr;
    if (SUCCEEDED(sound->SetCooperativeLevel(window, DSSCL_NORMAL))) {
        DSBUFFERDESC desc{};
        desc.dwSize = 0x24;
        desc.dwFlags = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_GLOBALFOCUS | DSBCAPS_CTRLVOLUME;
        desc.dwBufferBytes = bytes;
        desc.lpwfxFormat = const_cast<WAVEFORMATEX*>(&format);
        if (FAILED(sound->CreateSoundBuffer(&desc, &buffer, nullptr))) buffer = nullptr;
    }
    if (buffer == nullptr) {
        sound->Release();
        return nullptr;
    }
    return std::make_unique<DirectSoundPlayback>(sound, buffer);
}

std::unique_ptr<CaptureBuffer> DirectSoundVoice::openCapture(const DeviceId& id, const tWAVEFORMATEX& format,
                                                             uint32_t bytes) {
    const GUID guid = guidOf(id);
    IDirectSoundCapture* capture = nullptr;
    if (FAILED(DirectSoundCaptureCreate(&guid, &capture, nullptr))) return nullptr;
    DSCBUFFERDESC desc{};
    desc.dwBufferBytes = bytes;
    desc.dwSize = 0x1C;
    desc.lpwfxFormat = const_cast<WAVEFORMATEX*>(&format);
    IDirectSoundCaptureBuffer* buffer = nullptr;
    if (SUCCEEDED(capture->CreateCaptureBuffer(&desc, &buffer, nullptr))) {
        if (SUCCEEDED(buffer->Start(DSCBSTART_LOOPING))) return std::make_unique<DirectSoundCapture>(capture, buffer);
        buffer->Release();
    }
    capture->Release();
    return nullptr;
}

}  // namespace oj::net::gv
