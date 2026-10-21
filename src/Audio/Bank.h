#pragma once
// A bank of the stock game's sound library (docs/spec/26-sound.md, "Banks"): audio\...\<name>\<name>.cfg names the
// samples or streams and the sound sets (SS) that play them; <name>.dat holds the samples (entries of 0xA4 bytes,
// the data after them), or each sample is <sample>.wav. A sample bank (banktype 0 or 3, 0xBC bytes, vtable
// 0x006FEE88, made by 0x005BDD60) makes the DirectSound buffers of every set as it ends ([EndBank], 0x005BE000); a
// stream bank (1 or 2, 0xC0 bytes, 0x006FEEF8, 0x005BE6E0) plays <name>.dsb (Audio/Stream.h).

#include "Core/Base/GameFiles.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

struct IDirectSoundBuffer8;
struct IDirectSound3DBuffer;
struct IKsPropertySet;
typedef struct tWAVEFORMATEX WAVEFORMATEX;

namespace oj::audio {

class Bank;
class SoundSet;
class Stream;

// An entry of <bank>.dat (0xA4 bytes) or of a <sample>.wav read by 0x005C1CE0.
struct SampleEntry {
    uint32_t size = 0;                 // +0x00: the data's bytes
    uint32_t formatSize = 0;           // +0x04
    std::byte format[0x80]{};          // +0x08: WAVEFORMATEX (a stream's is not read)
    int32_t offset = -1;               // +0x88: of the data in the .dat; -1 for a .wav
    const std::byte* data = nullptr;   // +0x8C
    uint32_t start = 0;                // +0x90: a stream's start (type 1 a float, type 2 whole seconds)
    std::string name;                  // +0x94
    const WAVEFORMATEX& wave() const { return *reinterpret_cast<const WAVEFORMATEX*>(format); }
    uint32_t rate() const;             // +0x0C: nSamplesPerSec
    uint32_t bytesPerSecond() const;   // +0x10: nAvgBytesPerSec
};

// A DirectSound buffer of a set (0x2C bytes, vtable 0x006FF3E8, made by 0x005C42D0): the values last given to it.
struct HardwareVoice {
    uint32_t frequency = 48000;            // +0x04
    uint32_t given = 0;                    // +0x08: a 3D voice's frequency last given (with its doppler)
    int32_t pan = 0;                       // +0x0C
    uint32_t position = 0;                 // +0x10
    int32_t volume = 0;                    // +0x14: hundredths of a decibel
    uint32_t playFlags = 0;                // +0x18
    float occlusion = 0.0f;                // +0x1C (EAX)
    IDirectSoundBuffer8* buffer = nullptr;     // +0x20
    IDirectSound3DBuffer* buffer3d = nullptr;  // +0x24
    IKsPropertySet* properties = nullptr;      // +0x28
};

// The buffers of a set (vtables 0x006FF090 2D, 0x006FF120 3D, 0x006FF2D8 stream; base 0x005C41D0 / 0x005C33C0):
// the set's first buffer holds the sample; the others are its duplicates (instances).
class VoiceSet {
public:
    VoiceSet(SoundSet& set, const SampleEntry& entry);
    ~VoiceSet();
    VoiceSet(const VoiceSet&) = delete;
    VoiceSet& operator=(const VoiceSet&) = delete;

    SoundSet& set() { return m_set; }
    uint32_t count() const { return static_cast<uint32_t>(m_voices.size()); }
    HardwareVoice& voice(uint32_t i) { return m_voices[i]; }
    Stream* stream() const { return m_stream; }
    void setStream(Stream* stream) { m_stream = stream; }

    // +0x18 (0x005C3850): played: the flags given (+0x18) with LOCHARDWARE for a 3D set when the hardware plays
    // (flag 2 on, 4 off), else LOCSOFTWARE (a 2D set: its pan first); the position, volume and (2D) pan, then the
    // frequency set; a lost buffer is filled again (0x005C3310); a hardware play failing is tried in the software.
    // Nonzero when it cannot play.
    int play(uint32_t i);
    // +0x1C (0x005C3AD0): played again as last set (position, volume, pan, frequency, flags).
    int replay(uint32_t i);
    int stop(uint32_t i);    // +0x20 (0x005C3BF0)
    int pause(uint32_t i);   // +0x24: a sample's stopped; a stream paused
    int resume(uint32_t i);  // +0x28 (0x005C3C30): Play with the flags kept
    // +0x3C (0x005C3C90): the volume (0x005B8B50) when it changed.
    int setVolume(uint32_t i, float volume);
    // +0x44 (0x005C3D00, 2D; the 3D set's 0x005C2220 does not clamp): round(request x the sample's rate / 48000),
    // clamped to [minimum, 48000], given when it changed.
    int setFrequency(uint32_t i, uint32_t request);
    uint32_t frequency(uint32_t i) const { return m_voices[i].frequency; }  // +0x48 (0x005C4150)
    void setPlayFlags(uint32_t i, uint32_t flags) { m_voices[i].playFlags = flags; }  // +0x4C (0x005C4180)
    // +0x54 (0x005C3DD0): 4 looping, 3 playing, else 0.
    int status(uint32_t i);
    // +0x84 (0x005C1E70, 2D): the pan from -1 .. 1: 0x005B8B50(1 - |p|), negated to the right.
    int setPan(uint32_t i, float pan);
    // +0x84 (0x005C20B0, 3D): placed (DS3D, deferred) at the emitter's place against the listener, min distance 0
    // ([0x0079E9C8], never set); with a doppler the frequency over 1 + clamp(place . change x doppler / (3430 x
    // distance), -0.3, 0.3), cut, clamped to [minimum, 48000], given when it changed.
    int place(uint32_t i, const float relative[3], const float change[3], float distance, float doppler);
    // +0x34 / +0x38: the position (bytes) kept and given.
    void setPosition(uint32_t i, uint32_t position);
    // +0x88 (0x005C22D0, 3D; the 2D set's 0x005285D0 does nothing): the voice's occlusion (+0x1C) a step of 0.25
    // toward v x [0x00725858] (0.5, or 0.1 once the device is seen without the voices' own EAX), as the EAX room of
    // the voice (EAXSOURCE_ROOM, device flag 0x10) or else of FX slot 0's reverb through the probe's property set;
    // nothing without the voice's property set (+0x28).
    int setOcclusion(uint32_t i, float v);

    bool is3d() const { return m_type == 3; }
    bool isStream() const { return m_type == 1 || m_type == 2; }

private:
    void refill(uint32_t i);  // 0x005C3310
    SoundSet& m_set;
    const SampleEntry& m_entry;
    int m_type = 0;
    std::vector<HardwareVoice> m_voices;
    Stream* m_stream = nullptr;
    float m_streamStart = 0.0f;  // +0x18 of the stream set (0x005C3F10)
};

// A sound set (0x88 bytes, vtable 0x006FEDB8, made by the bank's +0x00 0x005BDDF0): [BeginSS] .. [EndSS] of the .cfg.
class SoundSet {
public:
    // vtable +0x9C (0x005BE5D0): 31 characters, upper case.
    void setName(const std::string& name);
    const std::string& name() const { return m_name; }                 // +0x18: +0x40
    const std::string& displayName() const { return m_displayName; }   // +0xA4: +0x60
    void setDisplayName(const std::string& name) { m_displayName = name; }
    int sample() const { return m_sample; }               // +0x74: +0x78
    void setSample(int sample) { m_sample = sample; }     // +0x24
    int type() const { return m_type; }                   // +0xB4: +0x80 (0 2D, 1 and 2 streams, 3 3D)
    void setType(int type) { m_type = type; }             // +0xB0
    uint32_t instances() const { return static_cast<uint32_t>(m_owner.size()); }  // +0x14: +0x84
    // +0xA8 and +0xAC (0x005BE680): the instances and their arrays (taken from the bank's, 0x005BA900 ..).
    void setInstances(uint32_t count);
    Bank* bank() const { return m_bank; }                 // +0x70
    void setBank(Bank* bank) { m_bank = bank; }           // +0x04

    VoiceSet* voices() const { return m_voices.get(); }   // +0x2C: +0x7C
    void setVoices(std::unique_ptr<VoiceSet> voices) { m_voices = std::move(voices); }

    // The sample's values (0x005C41D0): +0x08 its bytes, +0x04 and +0x10 its rate, +0x0C its milliseconds, +0x14 the
    // bytes of a second over the rate.
    float size = 0.0f;
    uint32_t rate = 48000;
    uint32_t milliseconds = 0;
    uint32_t rate2 = 0;
    float bytesPerFrame = 0.0f;
    int priority = 0;  // +0x18 (read by the slot strategy 0x005BF630)
    uint32_t users = 0;  // +0x24: the virtual voices playing it this frame (0x005BE490 .. 0x005BE4C0)

    bool allBusy() const { return m_busyCount == instances(); }  // +0x30 (0x005BE3E0)
    bool busy(uint32_t i) const { return (m_busy >> i) & 1; }   // +0x34
    void acquire(uint32_t i);                                     // +0x38 (0x005BE420): +0x1C, +0x20, +0x28
    void release(uint32_t i);                                     // +0x3C (0x005BE450)
    uint32_t owner(uint32_t i) const { return m_owner[i]; }      // +0x54: the handle playing the voice
    void setOwner(uint32_t i, uint32_t handle) { m_owner[i] = handle; }  // +0x58
    void markPlaying(uint32_t i) { m_playing |= 1u << i; }       // +0x5C
    void clearPlaying(uint32_t i) { m_playing &= ~(1u << i); }   // +0x60
    bool playing(uint32_t i) const { return (m_playing >> i) & 1; }  // +0x64
    void setStart(uint32_t i, uint32_t seconds) { m_start[i] = seconds; }  // +0x30 array
    void setVolume(uint32_t i, float volume) { m_volume[i] = volume; }     // +0x34 array

    // +0xB8 (0x005C1B10): the first voice not busy, taken (0); 4 when all are.
    int findFree(uint32_t& voice);
    // +0xBC (0x005C1B60): the quietest voice (the lowest of +0x34, scanned from the last) unless all are as quiet as
    // their mean (1); else 4.
    int stealQuietest(uint32_t& voice) const;
    // +0xC0 (0x005C1C90): the voice started first (the lowest nonzero start, +0x30) (2); else 4.
    int stealOldest(uint32_t& voice) const;

private:
    std::string m_name;
    std::string m_displayName;
    int m_sample = 0;
    int m_type = 0;
    Bank* m_bank = nullptr;
    std::unique_ptr<VoiceSet> m_voices;
    uint32_t m_busyCount = 0;  // +0x1C
    uint32_t m_busy = 0;       // +0x20
    uint32_t m_playing = 0;    // +0x28
    std::vector<uint32_t> m_owner;  // +0x2C
    std::vector<uint32_t> m_start;  // +0x30
    std::vector<float> m_volume;    // +0x34
};

class Bank {
public:
    Bank(int type, std::filesystem::path folder);
    ~Bank();

    const std::string& name() const { return m_name; }  // +0x20 method: +0x70
    int type() const { return m_type; }                 // +0x14
    uint32_t count() const { return static_cast<uint32_t>(m_sets.size()); }  // +0x0C
    SoundSet* set(uint32_t i) const { return i < m_sets.size() ? m_sets[i].get() : nullptr; }  // +0x4C
    // +0x50 (0x005BAAF0): the set of the name (case ignored), -1 when none.
    int findSet(const char* name) const;
    // +0x54 (0x005BAA90): the declared sample of the name (case ignored), -1 when none.
    int findSample(const char* name) const;
    const std::vector<SampleEntry>& samples() const { return m_entries; }
    Stream* stream() const { return m_stream.get(); }

    // Read by the .cfg's keywords (Bank::load).
    static std::unique_ptr<Bank> load(const GameFiles& files, const std::filesystem::path& folder,
                                      const std::string& name, uint32_t* channels);

private:
    void makeVoices(const GameFiles& files);  // [EndBank] (0x005BE000 / 0x005BE7A0)
    int m_type = 0;
    std::string m_name;  // +0x70: bank_name (+0x14 method), what banks are found by
    std::string m_file;  // +0x50: the .cfg's name (+0x18 method), of <file>.dat and <file>.dsb
    std::filesystem::path m_folder;
    std::vector<std::unique_ptr<SoundSet>> m_sets;
    std::vector<std::string> m_declared;    // +0xA0: the names declared, in order
    std::vector<std::byte> m_dat;           // <name>.dat
    std::vector<SampleEntry> m_entries;     // the .dat's entries (or the .wav's, one a set)
    std::vector<std::vector<std::byte>> m_wavs;
    std::unique_ptr<Stream> m_stream;
    friend class SoundSet;
};

}  // namespace oj::audio
