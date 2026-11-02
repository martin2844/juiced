#pragma once
// The sound library's output (docs/code/audio.md): the stock's DirectSound 8 (Audio/DirectSound/DirectSoundOutput.h)
// behind the calls the library's logic makes, in the stock's order and with DirectSound's answers, so that another
// output can stand in for it. Its parts: the device as the library starts it (the primary buffer, eax.dll's EAX and
// its probe), the voices' buffers (their 3D and EAX properties), the primary buffer's 3D listener, the streams of the
// stream banks and the buffers of the movies' sound.

#include <cstdint>
#include <filesystem>
#include <memory>

typedef struct _GUID GUID;
typedef struct HWND__* HWND;

namespace oj::audio {

// An answer of the output: DS_OK, another success, DSERR_BUFFERLOST, another failure.
enum class Answer { Ok, Done, Lost, Failed };
inline bool failed(Answer a) {
    return a == Answer::Lost || a == Answer::Failed;
}

// A buffer's samples (WAVEFORMATEX's fields).
struct SampleFormat {
    uint16_t tag = 1;  // WAVE_FORMAT_PCM
    uint16_t channels = 1;
    uint32_t rate = 22050;
    uint32_t bytesPerSecond = 44100;
    uint16_t blockAlign = 2;
    uint16_t bits = 16;
};

// DirectSound's flags the library gives and reads (DSBCAPS_*, DSBPLAY_*, DSBSTATUS_*).
enum : uint32_t {
    kCapsLocHardware = 0x4,
    kCapsCtrl3d = 0x10,
    kCapsCtrlFrequency = 0x20,
    kCapsCtrlPan = 0x40,
    kCapsCtrlVolume = 0x80,
    kCapsGlobalFocus = 0x8000,
    kCapsGetCurrentPosition2 = 0x10000,
    kCapsLocDefer = 0x40000,
    kPlayLooping = 0x1,
    kPlayLocHardware = 0x2,
    kPlayLocSoftware = 0x4,
    kStatusPlaying = 0x1,
    kStatusLooping = 0x4,
};

// A buffer of samples a voice plays (IDirectSoundBuffer8, with its IDirectSound3DBuffer and IKsPropertySet); let go,
// stopped and released.
class OutputBuffer {
public:
    virtual ~OutputBuffer() = default;
    // Locked for the bytes from `at` (across the end), the data written (zeros without it), unlocked.
    virtual Answer write(uint32_t at, const void* data, uint32_t bytes) = 0;
    virtual Answer restore() = 0;
    // AcquireResources(DSBPLAY_LOCHARDWARE): the hardware's resources asked for.
    virtual Answer acquireHardware() = 0;
    virtual Answer play(uint32_t flags) = 0;
    virtual Answer stop() = 0;
    virtual Answer setPosition(uint32_t bytes) = 0;  // SetCurrentPosition
    virtual Answer cursors(uint32_t& play, uint32_t& write) = 0;  // GetCurrentPosition
    virtual Answer setVolume(int32_t mb) = 0;  // hundredths of a decibel
    virtual Answer setPan(int32_t mb) = 0;
    virtual Answer setFrequency(uint32_t hz) = 0;
    virtual Answer status(uint32_t& flags) = 0;
    // The 3D buffer (asked for once; true when there is one), the parameters of a new voice given to it (deferred: at
    // the listener, no cone, distances 1 .. 1e9, normal) and its place (deferred).
    virtual bool open3d() = 0;
    virtual void reset3d() = 0;
    virtual void place3d(float x, float y, float z) = 0;
    // The EAX property set (asked for once; true when there is one) and a property given through it.
    virtual bool openEax() = 0;
    virtual bool hasEax() const = 0;
    virtual Answer setEax(const GUID& set, uint32_t id, const void* data, uint32_t bytes) = 0;
};

// The primary buffer's 3D listener (IDirectSound3DListener): its settings deferred, then committed.
class OutputListener {
public:
    virtual ~OutputListener() = default;
    virtual void setDistanceFactor(float factor) = 0;
    virtual void setRolloffFactor(float factor) = 0;
    virtual void setPosition(float x, float y, float z) = 0;
    virtual void setOrientation(float frontX, float frontY, float frontZ, float upX, float upY, float upZ) = 0;
    virtual void commit() = 0;
};

// A stream bank's file played in pieces (Audio/Stream.h; the stock's DirectShow filter graph): times in 100 ns.
class OutputStream {
public:
    virtual ~OutputStream() = default;
    virtual void stop() = 0;
    virtual void run() = 0;
    // From current to stop, absolute (the current time returned); true when taken.
    virtual bool setPositions(int64_t current, int64_t stop) = 0;
    // Whether the positions can be read (the graph's seeking), and they.
    virtual bool seekable() const = 0;
    virtual void positions(int64_t& current, int64_t& end) = 0;
    virtual void setVolume(int32_t mb) = 0;
};

// What starting the output leaves the library.
struct OutputStart {
    uint32_t flags = 0;                // the configuration's flags as the start leaves them
    bool eaxDevice = false;            // made by eax.dll
    uint32_t eaxFlags = 0;             // the flags eax.dll's device gives (0x10: the voices' own EAX)
    uint32_t minFrequency = 100;       // DSCAPS dwMinSecondarySampleRate
    uint32_t hardware3dStreaming = 0;  // DSCAPS dwMaxHw3DStreamingBuffers
};

class Output {
public:
    virtual ~Output() = default;
    // The device made and started as the library does (System::start); false leaves the sound off.
    virtual bool start(HWND window, uint32_t flags, OutputStart& result) = 0;
    virtual void stop() = 0;
    // Whether the hardware plays as many 3D voices as it says it has (0x005C3150's buffers played there).
    virtual bool hardwarePlays3d(uint32_t buffers) = 0;
    // The listener's EAX (the probe's property set): Set, Get; false without it or when it fails.
    virtual bool setEax(const GUID& set, uint32_t id, const void* data, uint32_t bytes) = 0;
    virtual bool getEax(const GUID& set, uint32_t id, void* data, uint32_t bytes) = 0;
    // A buffer of the samples' format and bytes (DSBCAPS flags); a duplicate of one (the samples shared); null when
    // the device does not make it.
    virtual std::unique_ptr<OutputBuffer> createBuffer(const SampleFormat& format, uint32_t bytes, uint32_t caps) = 0;
    virtual std::unique_ptr<OutputBuffer> duplicate(OutputBuffer& buffer) = 0;
    // The primary buffer's listener; null without one.
    virtual std::unique_ptr<OutputListener> openListener() = 0;
    // A stream bank's file opened (never null; one that cannot play does nothing).
    virtual std::unique_ptr<OutputStream> openStream(const std::filesystem::path& file) = 0;
    // What binkw32's BinkSetSoundSystem is given (DirectSound's device); 0 without a device (the library's banks make
    // no buffers then).
    virtual uintptr_t soundSystem() const = 0;
    // The fixed build's EAX switched while it runs (the game's audio options): its reverb heard or not; the voices of
    // the banks made from then on take it or not (System::flags' 0x20). Nothing where the output has none.
    virtual void enableEax(bool on) { (void)on; }
};

// The build's output: DirectSound 8 (the fixed build's: OpenAL Soft, Audio/OpenAL/OpenAlOutput.h).
std::unique_ptr<Output> makeOutput();

}  // namespace oj::audio
