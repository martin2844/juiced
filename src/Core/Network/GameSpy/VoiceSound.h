#pragma once
// The voice devices' sound (GameSpy Voice 2's gvDirectSound.c on the PC; GameSpyVoice.h): the stock's DirectSound 8 and
// DirectSoundCapture (DirectSound/DirectSoundVoice.h) behind the calls the voice SDK makes, in the stock's order and
// with their answers, so that another sound can stand in for it. Its parts: the devices listed (and the default voice
// devices), a playback device's looping buffer the mix writes behind its play cursor, a capture device's looping
// buffer read behind its read cursor.

#include <array>
#include <cstdint>
#include <functional>
#include <memory>

typedef struct HWND__* HWND;
struct tWAVEFORMATEX;

namespace oj::net::gv {

using DeviceId = std::array<uint8_t, 16>;  // a device's GUID

// The part of a looping buffer locked (IDirectSoundBuffer::Lock's): the bytes from the place asked for, the rest from
// the buffer's start when they go past its end.
struct Locked {
    void* first = nullptr;
    uint32_t firstBytes = 0;
    void* second = nullptr;
    uint32_t secondBytes = 0;
};

// A playback device's looping buffer (DirectSound's object with its secondary buffer); let go, the buffer released,
// then the object.
class PlaybackBuffer {
public:
    virtual ~PlaybackBuffer() = default;
    virtual bool playCursor(uint32_t& play) = 0;  // GetCurrentPosition's play cursor
    // Lock of the bytes from `at` (0 bytes: the whole buffer, DSBLOCK_ENTIREBUFFER).
    virtual bool lock(uint32_t at, uint32_t bytes, Locked& locked) = 0;
    virtual bool unlock(const Locked& locked) = 0;
    virtual void stop() = 0;
    virtual bool rewind() = 0;       // SetCurrentPosition(0)
    virtual bool playLooping() = 0;  // Play(0, 0, DSBPLAY_LOOPING)
    // GetStatus's DSBSTATUS_PLAYING; false when it fails.
    virtual bool playing() = 0;
    virtual void setVolume(int32_t mb) = 0;  // DirectSound's hundredths of a decibel
    virtual bool volume(int32_t& mb) = 0;
};

// A capture device's looping buffer (DirectSoundCapture's object with its capture buffer, started); let go, stopped
// and released, then the object.
class CaptureBuffer {
public:
    virtual ~CaptureBuffer() = default;
    virtual bool readCursor(uint32_t& read) = 0;  // GetCurrentPosition's read cursor
    virtual bool lock(uint32_t at, uint32_t bytes, Locked& locked) = 0;
    virtual bool unlock(const Locked& locked) = 0;
};

class VoiceSound {
public:
    // A device enumerated (DirectSoundEnumerateA's callback): its GUID (none for the primary driver) and description;
    // false stops the enumeration.
    using Listed = std::function<bool(const DeviceId* id, const char* description)>;

    virtual ~VoiceSound() = default;
    // DirectSoundCaptureEnumerateA (capture) or DirectSoundEnumerateA.
    virtual void list(bool capture, const Listed& each) = 0;
    // GetDeviceID of DSDEVID_DefaultVoiceCapture or DSDEVID_DefaultVoicePlayback (zeros when it cannot be had).
    virtual DeviceId defaultDevice(bool capture) = 0;
    // DirectSoundCreate of the GUID, the cooperative level DSSCL_NORMAL with the window, the buffer of the bytes and
    // format (GETCURRENTPOSITION2 | GLOBALFOCUS | CTRLVOLUME). Null when any of it fails.
    virtual std::unique_ptr<PlaybackBuffer> openPlayback(const DeviceId& id, HWND window, const tWAVEFORMATEX& format,
                                                         uint32_t bytes) = 0;
    // DirectSoundCaptureCreate of the GUID, the capture buffer of the bytes and format, started looping (released when
    // it does not start). Null when any of it fails.
    virtual std::unique_ptr<CaptureBuffer> openCapture(const DeviceId& id, const tWAVEFORMATEX& format,
                                                       uint32_t bytes) = 0;
};

// The build's sound (VoiceSound.cpp): the stock's DirectSound (the fixed build's: OpenAL Soft, OpenAL/OpenAlVoice.h).
std::unique_ptr<VoiceSound> makeVoiceSound();
// The SDK's sound: the process's own (made at the first call), or the one setVoiceSound put in its place (the tests').
VoiceSound& voiceSound();
void setVoiceSound(VoiceSound* sound);

}  // namespace oj::net::gv
