#pragma once
// The voice devices' sound of the stock (GameSpy Voice 2's gvDirectSound.c; Network/GameSpy/VoiceSound.h): the devices
// of DirectSoundEnumerateA and DirectSoundCaptureEnumerateA, a DirectSound object with a secondary buffer for each
// playback device, a DirectSoundCapture object with a capture buffer for each capture device.

#include "Core/Network/GameSpy/VoiceSound.h"

namespace oj::net::gv {

class DirectSoundVoice final : public VoiceSound {
public:
    void list(bool capture, const Listed& each) override;
    DeviceId defaultDevice(bool capture) override;
    // 0x0057FA20's DirectSoundCreate, then 0x0057F8B0.
    std::unique_ptr<PlaybackBuffer> openPlayback(const DeviceId& id, HWND window, const tWAVEFORMATEX& format,
                                                 uint32_t bytes) override;
    // 0x0057FA20's DirectSoundCaptureCreate, then 0x0057F980.
    std::unique_ptr<CaptureBuffer> openCapture(const DeviceId& id, const tWAVEFORMATEX& format,
                                               uint32_t bytes) override;
};

}  // namespace oj::net::gv
