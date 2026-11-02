#pragma once
// The fixed build's voice sound (Network/GameSpy/VoiceSound.h, docs/fixes.md Added): OpenAL Soft (third_party/
// openal-soft-1.25.2, static) in place of DirectSound and DirectSoundCapture. The devices are OpenAL Soft's (their
// names without its "OpenAL Soft on " prefix, as DirectSound describes the same endpoints; a device's GUID made of its
// name); a playback device a device and context of its own whose source plays the looping buffer through a callback
// (the play cursor where the mixer has read to); a capture device OpenAL's capture, its samples taken into the looping
// buffer as the read cursor is asked for.

#include "Core/Network/GameSpy/VoiceSound.h"

namespace oj::net::gv {

class OpenAlVoice final : public VoiceSound {
public:
    void list(bool capture, const Listed& each) override;
    DeviceId defaultDevice(bool capture) override;
    std::unique_ptr<PlaybackBuffer> openPlayback(const DeviceId& id, HWND window, const tWAVEFORMATEX& format,
                                                 uint32_t bytes) override;
    std::unique_ptr<CaptureBuffer> openCapture(const DeviceId& id, const tWAVEFORMATEX& format,
                                               uint32_t bytes) override;
};

}  // namespace oj::net::gv
