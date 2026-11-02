#pragma once
// The stock game's sound library (0x005B8000 .. 0x005C4400, docs/spec/26-sound.md): its output (Audio/Output.h:
// DirectSound 8 made through the game folder's eax.dll when it is there, else by COM); banks of samples and streams
// (Audio/Bank.h), players of handles (Audio/Player.h), a manager of virtual voices for the 3D sounds
// (Audio/Voices.h) and streams (Audio/Stream.h).

#include "Audio/Output.h"

#include <cstdint>
#include <filesystem>
#include <memory>

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
// player and bank). One a process. Its device is the output's (Audio/Output.h: DirectSound 8).
class System {
public:
    System();  // the build's output (makeOutput)
    explicit System(std::unique_ptr<Output> output);
    ~System();

    // 0x005B8500: with the configuration's flag 1 the flags become 1 and nothing is made; started already, nothing.
    // Otherwise the output started with the configuration's flags (Output::start), which leaves them as the stock's
    // start does; [0x0075F860] = 1 when it started. Any failure leaves the sound off.
    bool start(HWND window, uint32_t configFlags);
    // 0x005B8320 (the system's destructor): the output let go (Output::stop).
    void stop();

    bool on() const { return m_on; }               // [0x0075F860]
    uint32_t flags() const { return m_flags; }     // [0x0071E718]
    void setFlags(uint32_t flags) { m_flags = flags; }
    // The fixed build's EAX on or off while the game runs (its audio options): the flags' 0x20 (the banks made from
    // then on, a race's) and the output's reverb heard or not at once.
    void enableEax(bool on);
    uint32_t minFrequency() const { return m_minFrequency; }   // [0x0075EC20]: DSCAPS dwMinSecondarySampleRate
    uint32_t hardware3dStreaming() const { return m_hw3d; }   // [0x0075EC4C]: DSCAPS dwMaxHw3DStreamingBuffers
    bool eax() const { return m_eaxDevice; }  // made by eax.dll (the property sets asked for at the start)
    // +0x0C: the flags eax.dll's device gives (the probe); 0x10: each voice's own EAX properties (EAX40 Source).
    uint32_t eaxFlags() const { return m_eaxFlags; }
    Output& output() { return *m_output; }

    // 0x005B91A0: Set of a property through the probe's property set (no instance data); true when it succeeded.
    // False without the property set.
    bool setEax(const GUID& set, uint32_t id, const void* data, uint32_t bytes);
    // 0x005B91E0: Get of a property into data; true when it succeeded. False while [0x0075F854] is 0.
    bool getEax(const GUID& set, uint32_t id, void* data, uint32_t bytes);

    // 0x005C3150: whether the hardware plays its 3D voices (Output::hardwarePlays3d with the hardware's 3D streaming
    // buffers). Asked once ([0x0079E9D0]).
    bool hardwarePlays3d();

    // +0x04 (0x005B8AB0): the streams follow the game's focus (0x005BB140), then every player is stepped (0x005BA110).
    // The game's main loop asks for it every frame (0x0040CF0A, unless a job runs), the music as it starts and stops
    // a song.
    void update();

private:
    std::unique_ptr<Output> m_output;
    bool m_on = false;
    bool m_eaxDevice = false;
    uint32_t m_flags = 0;
    uint32_t m_eaxFlags = 0;  // +0x0C
    uint32_t m_minFrequency = 100;
    uint32_t m_hw3d = 0;
    int m_hardwareAsked = -1;
};

// The library's system: the process's own, or the one setSystem put in its place (the tests' own, of an output of
// theirs; null gives the process's back).
System& system();
void setSystem(System* system);

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
