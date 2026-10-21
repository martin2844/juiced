#pragma once
// The stock game's sound library (0x005B8000 .. 0x005C4400, docs/spec/26-sound.md): DirectSound 8 made through the
// game folder's eax.dll (EAXDirectSoundCreate8) when it is there, else by COM; banks of samples and streams
// (Audio/Bank.h), players of handles (Audio/Player.h), a manager of virtual voices for the 3D sounds
// (Audio/Voices.h) and streams played by DirectShow (Audio/Stream.h).

#include <cstdint>
#include <filesystem>

struct IDirectSound8;
struct IDirectSoundBuffer;
struct IDirectSoundBuffer8;
struct IDirectSound3DBuffer;
struct IKsPropertySet;
typedef struct _GUID GUID;
typedef struct HWND__* HWND;
typedef struct HINSTANCE__* HMODULE;

namespace oj::audio {

// [0x0071E718]: the configuration's audioflags as the library keeps them.
enum Flags : uint32_t {
    kNoSound = 0x01,     // All Audio off: no library at all
    kHardware = 0x02,    // Hardware Mixing: the 3D voices played in the hardware (DSBPLAY_LOCHARDWARE)
    kSoftware = 0x04,    // set when the hardware cannot: the voices played by the software (DSBPLAY_LOCSOFTWARE)
    kNoMusic = 0x08,
    kNoSpeech = 0x10,
    kNoEax = 0x20,
};

// The library's system ([0x0075F864], 0x10 bytes, vtable 0x006FE69C; made by 0x005B8500, refcounted by every
// player and bank). One a process.
class System {
public:
    // 0x005B8500: with the configuration's flag 1 the flags become 1 and nothing is made. Otherwise the flags are the
    // configuration's; 4 clears 0x20; without 0x20 eax.dll (the current folder: the game's) makes the device
    // (0x005B84B0), failing that (or with 0x20) CoCreateInstance(CLSID_DirectSound8) and Initialize(default). The
    // capabilities ([0x0075EC18], 0x60 bytes); no 3D streaming buffers in the hardware ([0x0075EC4C]) takes 2 off and
    // puts 4 on. The window's PRIORITY level, the primary buffer (0x100D1) set to 22050 Hz, 16 bits, stereo, then the
    // NORMAL level (as the stock: the format asked for is then not the device's) and the primary buffer played
    // looping. With the device of eax.dll (0x005B88A6 .. 0x005B8A80) the EAX probe (probeEax), then the reverb loaded
    // in FX slot 0 (EAXFXSLOT_ALLPARAMETERS: EAX_REVERB_EFFECT, volume 0); only when that fails, the slot's effect
    // read back (EAXFXSLOT_LOADEFFECT) and, when it is the reverb, its environment 0x15 ("Reverb type could not be
    // set" to the debugger when that fails) and the context's primary slot FX slot 0. Then [0x0075F860] = 1. Any
    // failure before leaves the sound off.
    bool start(HWND window, uint32_t configFlags);
    // 0x005B8320 (the system's destructor): the EAX objects (releaseEax), the primary buffer, the device, eax.dll.
    void stop();

    bool on() const { return m_on; }               // [0x0075F860]
    uint32_t flags() const { return m_flags; }     // [0x0071E718]
    void setFlags(uint32_t flags) { m_flags = flags; }
    IDirectSound8* device() const { return m_device; }   // [0x0075F858]
    IDirectSoundBuffer* primary() const { return m_primary; }  // [0x0075EC78]
    uint32_t minFrequency() const { return m_minFrequency; }   // [0x0075EC20]: DSCAPS dwMinSecondarySampleRate
    uint32_t hardware3dStreaming() const { return m_hw3d; }   // [0x0075EC4C]: DSCAPS dwMaxHw3DStreamingBuffers
    bool eax() const { return m_eaxDevice; }  // made by eax.dll (the property sets asked for at the start)
    // +0x0C: the flags eax.dll's device gives (probeEax); 0x10: each voice's own EAX properties (EAX40 Source).
    uint32_t eaxFlags() const { return m_eaxFlags; }
    // [0x0075F850]: the probe's property set (of its 3D buffer), the listener's EAX; null without one.
    IKsPropertySet* eaxProperties() const { return m_eaxProperties; }

    // 0x005B91A0: Set of a property through the probe's property set (no instance data); true when it succeeded.
    // False without the property set.
    bool setEax(const GUID& set, uint32_t id, const void* data, uint32_t bytes);
    // 0x005B91E0: Get of a property into data; true when it succeeded. False while [0x0075F854] is 0.
    bool getEax(const GUID& set, uint32_t id, void* data, uint32_t bytes);

    // 0x005C3150: whether the hardware plays its 3D voices: as many 3D buffers as the device makes (0x005C3010) made
    // and played in the hardware (DSBPLAY_LOCHARDWARE); one failing answers no. Asked once ([0x0079E9D0]).
    bool hardwarePlays3d();

    // +0x04 (0x005B8AB0): the streams follow the game's focus (0x005BB140), then every player is stepped (0x005BA110).
    // The game's main loop asks for it every frame (0x0040CF0A, unless a job runs), the music as it starts and stops
    // a song.
    void update();

private:
    // 0x005B8C30: a temporary 3D buffer in the hardware (0x40 bytes, 22050 Hz mono 16 bits), its IDirectSoundBuffer8,
    // IDirectSound3DBuffer and IKsPropertySet ("No eax property set" to the debugger without one); [0x0075F854] 1, the
    // device and the primary buffer AddRef'd, then each property set asked for (QuerySupport, get and set both):
    // the context (1), FX slots 0 .. 3 (0x10001), the source (1), [0x0075F854] left 1 only when all answer. The
    // device's flags (kDeviceFlags +0x10) into +0x0C: without 0x10 they are cleared, [0x0075F854] 0 and releaseEax;
    // releaseEax too whenever [0x0075F854] ends 0.
    void probeEax();
    // 0x005B8F40: FX slot 0 back to the reverb at volume -10000 (EAXFXSLOT_ALLPARAMETERS); when that fails (or
    // without the property set) the slot's effect read (only while [0x0075F854]) and, when it is the reverb, its
    // environment 0 ("Reverb type could not be set" when that fails) and the context's primary slot FX slot 0; then
    // the property set, the 3D buffer, the buffer 8 and the buffer released and [0x0075F854] 0.
    void releaseEax();

    bool m_on = false;
    bool m_eaxDevice = false;
    uint32_t m_flags = 0;
    uint32_t m_eaxFlags = 0;  // +0x0C
    uint32_t m_minFrequency = 100;
    uint32_t m_hw3d = 0;
    IDirectSound8* m_device = nullptr;
    IDirectSoundBuffer* m_primary = nullptr;
    HMODULE m_eax = nullptr;
    int m_hardwareAsked = -1;
    IDirectSoundBuffer* m_eaxBuffer = nullptr;      // [0x0075F844]
    IDirectSoundBuffer8* m_eaxBuffer8 = nullptr;    // [0x0075F848]
    IDirectSound3DBuffer* m_eaxBuffer3d = nullptr;  // [0x0075F84C]
    IKsPropertySet* m_eaxProperties = nullptr;      // [0x0075F850]
    bool m_eaxOn = false;                           // [0x0075F854]
};

System& system();

// [0x0075F780]: the game is the active application (WM_ACTIVATEAPP), for the streams.
void setActive(bool active);
bool active();

// 0x005B8B50: a volume in [0, 1] as DirectSound's hundredths of a decibel, round(2000 log10 v); 0 and below, or
// below -10000, are -10000; above 1 is 1.
int32_t volumeMb(float volume);

// The library's clock ([0x0075F8A8], 0x005A2250, method +0x0C 0x005A2310): the performance counter over its
// frequency, single precision, cut to whole seconds.
uint32_t clockSeconds();

}  // namespace oj::audio
