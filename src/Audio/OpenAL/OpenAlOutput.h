#pragma once
// The fixed build's sound output (Audio/Output.h, docs/fixes.md Added): OpenAL Soft (third_party/openal-soft-1.25.2,
// static) in place of DirectSound and DirectShow, answering the library's calls as DirectSound does. The voices'
// buffers are OpenAL sources over buffers their duplicates share; 3D ones placed as DirectSound 3D places them (the
// inverse distance clamped model, the listener's rolloff, left-handed places turned right-handed); the game's EAX 4
// as EFX (an EAX reverb in an auxiliary slot, Generic, each voice's room its send's gain); the movies' looping buffer
// a ring the mixer reads through a callback; the stream banks' WMA files decoded by JuicedConfigExt's decoder
// (third_party/wmadec) into a streaming source. No eax.dll, no DirectSound, no DirectShow.

#include "Audio/Output.h"

#include <memory>

namespace oj::audio {

class OpenAlOutput final : public Output {
public:
    OpenAlOutput();
    ~OpenAlOutput() override;

    // The default device and a context of two auxiliary sends; the distance model DirectSound's (inverse, clamped),
    // no doppler (the game works it out). The flags as DirectSound's start leaves them on a device without hardware
    // 3D buffers (4 asked for clears 0x20, then 2 off and 4 on); EFX's EAX reverb in FX slot 0 (the game's EAX
    // device with the voices' own EAX, flags 0x10), heard without 0x20 (enableEax switches it while the game runs).
    bool start(HWND window, uint32_t flags, OutputStart& result) override;
    void stop() override;
    bool hardwarePlays3d(uint32_t buffers) override;
    bool setEax(const GUID& set, uint32_t id, const void* data, uint32_t bytes) override;
    bool getEax(const GUID& set, uint32_t id, void* data, uint32_t bytes) override;
    std::unique_ptr<OutputBuffer> createBuffer(const SampleFormat& format, uint32_t bytes, uint32_t caps) override;
    std::unique_ptr<OutputBuffer> duplicate(OutputBuffer& buffer) override;
    std::unique_ptr<OutputListener> openListener() override;
    std::unique_ptr<OutputStream> openStream(const std::filesystem::path& file) override;
    uintptr_t soundSystem() const override;
    void enableEax(bool on) override;

    // The device, context and reverb, shared with the buffers and the listener (any let go after the output, as at
    // the process's end).
    struct State;

private:
    std::shared_ptr<State> m;
};

}  // namespace oj::audio
