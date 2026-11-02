#pragma once
// The stock sound library's output (Audio/Output.h): DirectSound 8 made through the game folder's eax.dll
// (EAXDirectSoundCreate8) when it is there, else by COM; its buffers, the primary buffer's listener, eax.dll's EAX
// probe; the stream banks' files played by DirectShow.

#include "Audio/Output.h"

struct IDirectSound8;
struct IDirectSoundBuffer;
struct IDirectSoundBuffer8;
struct IDirectSound3DBuffer;
struct IKsPropertySet;
typedef struct HINSTANCE__* HMODULE;

namespace oj::audio {

class DirectSoundOutput final : public Output {
public:
    ~DirectSoundOutput() override;

    // 0x005B8500 after its first checks: CoInitialize; 4 clears 0x20; without 0x20 eax.dll (the current folder: the
    // game's) makes the device (0x005B84B0), failing that (or with 0x20) CoCreateInstance(CLSID_DirectSound8) and
    // Initialize(default). The capabilities ([0x0075EC18], 0x60 bytes); no 3D streaming buffers in the hardware
    // ([0x0075EC4C]) takes 2 off and puts 4 on. The window's PRIORITY level, the primary buffer (0x100D1) set to 22050
    // Hz, 16 bits, stereo, then the NORMAL level (as the stock: the format asked for is then not the device's) and the
    // primary buffer played looping. With the device of eax.dll (0x005B88A6 .. 0x005B8A80) the EAX probe (probeEax),
    // then the reverb loaded in FX slot 0 (EAXFXSLOT_ALLPARAMETERS: EAX_REVERB_EFFECT, volume 0); only when that
    // fails, the slot's effect read back (EAXFXSLOT_LOADEFFECT) and, when it is the reverb, its environment 0x15
    // ("Reverb type could not be set" to the debugger when that fails) and the context's primary slot FX slot 0. Any
    // failure before leaves the sound off.
    bool start(HWND window, uint32_t flags, OutputStart& result) override;
    // 0x005B8320 (the system's destructor): the EAX objects (releaseEax), the primary buffer, the device, eax.dll.
    void stop() override;
    // 0x005C3150: as many 3D buffers as the device makes (0x005C3010: 0x400B0, 0x80 bytes of 22050 Hz mono 16 bits)
    // made and played in the hardware (DSBPLAY_LOCHARDWARE) at the least volume; one failing answers no.
    bool hardwarePlays3d(uint32_t buffers) override;
    // 0x005B91A0: Set of a property through the probe's property set (no instance data); false without it.
    bool setEax(const GUID& set, uint32_t id, const void* data, uint32_t bytes) override;
    // 0x005B91E0: Get of a property; false while [0x0075F854] is 0.
    bool getEax(const GUID& set, uint32_t id, void* data, uint32_t bytes) override;
    // CreateSoundBuffer, then its IDirectSoundBuffer8 (the first interface released).
    std::unique_ptr<OutputBuffer> createBuffer(const SampleFormat& format, uint32_t bytes, uint32_t caps) override;
    // DuplicateSoundBuffer, then the copy's IDirectSoundBuffer8.
    std::unique_ptr<OutputBuffer> duplicate(OutputBuffer& buffer) override;
    // The primary buffer's IDirectSound3DListener.
    std::unique_ptr<OutputListener> openListener() override;
    // A DirectShow filter graph of the file (DirectShowStream).
    std::unique_ptr<OutputStream> openStream(const std::filesystem::path& file) override;
    uintptr_t soundSystem() const override { return reinterpret_cast<uintptr_t>(m_device); }

private:
    // 0x005B8C30: a temporary 3D buffer in the hardware (0x40 bytes, 22050 Hz mono 16 bits), its IDirectSoundBuffer8,
    // IDirectSound3DBuffer and IKsPropertySet ("No eax property set" to the debugger without one); [0x0075F854] 1, the
    // device and the primary buffer AddRef'd, then each property set asked for (QuerySupport, get and set both):
    // the context (1), FX slots 0 .. 3 (0x10001), the source (1), [0x0075F854] left 1 only when all answer. The
    // device's flags (kDeviceFlags +0x10) into +0x0C: without 0x10 they are cleared, [0x0075F854] 0 and releaseEax;
    // releaseEax too whenever [0x0075F854] ends 0.
    void probeEax(uint32_t& eaxFlags);
    // 0x005B8F40: FX slot 0 back to the reverb at volume -10000 (EAXFXSLOT_ALLPARAMETERS); when that fails (or
    // without the property set) the slot's effect read (only while [0x0075F854]) and, when it is the reverb, its
    // environment 0 ("Reverb type could not be set" when that fails) and the context's primary slot FX slot 0; then
    // the property set, the 3D buffer, the buffer 8 and the buffer released and [0x0075F854] 0.
    void releaseEax();

    IDirectSound8* m_device = nullptr;              // [0x0075F858]
    IDirectSoundBuffer* m_primary = nullptr;        // [0x0075EC78]
    HMODULE m_eax = nullptr;
    IDirectSoundBuffer* m_eaxBuffer = nullptr;      // [0x0075F844]
    IDirectSoundBuffer8* m_eaxBuffer8 = nullptr;    // [0x0075F848]
    IDirectSound3DBuffer* m_eaxBuffer3d = nullptr;  // [0x0075F84C]
    IKsPropertySet* m_eaxProperties = nullptr;      // [0x0075F850]
    bool m_eaxOn = false;                           // [0x0075F854]
};

}  // namespace oj::audio
